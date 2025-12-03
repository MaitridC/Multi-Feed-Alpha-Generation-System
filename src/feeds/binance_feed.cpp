#include "feeds/binance_feed.h"
#include "alpha/alpha_engine.h"
#include "feeds/candle_aggregator.h"
#include "util/market_types.h"

#include <nlohmann/json.hpp>
#include <iostream>
#include <chrono>

using json = nlohmann::json;

BinancePublicFeed::BinancePublicFeed(
    const std::vector<std::string>& symbols,
    AlphaEngine& engine,
    CandleAggregator& aggregator
)
    : symbols_(symbols),
      engine_(engine),
      aggregator_(aggregator),
      running_(false)
{}

void BinancePublicFeed::setTickCallback(std::function<void(const MarketTick&)> callback) {
    tickCallback_ = std::move(callback);
}

void BinancePublicFeed::start() {
    running_ = true;
    wsThread_ = std::thread(&BinancePublicFeed::connectWebSocket, this);
}

void BinancePublicFeed::stop() {
    running_ = false;
    if (ws_) {
        ws_->stop();
    }
    if (wsThread_.joinable()) {
        wsThread_.join();
    }
}

void BinancePublicFeed::connectWebSocket() {

    std::string streams;
    for (size_t i = 0; i < symbols_.size(); ++i) {
        std::string symbol = symbols_[i];
        // Convert to lowercase
        std::transform(symbol.begin(), symbol.end(), symbol.begin(), ::tolower);

        streams += symbol + "@trade";
        if (i < symbols_.size() - 1) {
            streams += "/";
        }
    }

    std::string url = "wss://stream.binance.us:9443/stream?streams=" + streams;

    std::cout << "[Binance WS] Connecting to: " << url << std::endl;

    ws_ = std::make_unique<ix::WebSocket>();
    ws_->setUrl(url);

    ws_->setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            handleMessage(msg->str);
        }
        else if (msg->type == ix::WebSocketMessageType::Open) {
            std::cout << "[Binance WS] Connected!" << std::endl;
        }
        else if (msg->type == ix::WebSocketMessageType::Error) {
            std::cerr << "[Binance WS] Error: " << msg->errorInfo.reason << std::endl;
        }
        else if (msg->type == ix::WebSocketMessageType::Close) {
            std::cout << "[Binance WS] Connection closed" << std::endl;
        }
    });

    ws_->start();

    // Keep thread alive
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void BinancePublicFeed::handleMessage(const std::string& message) {
    try {
        auto j = json::parse(message);

        // Binance format: {"stream":"btcusdt@trade","data":{...}}
        if (!j.contains("data")) return;

        auto& data = j["data"];

        std::string symbol = data.value("s", "");  // e.g., "BTCUSDT"
        double price = std::stod(data.value("p", "0"));
        double quantity = std::stod(data.value("q", "0"));
        long long timestamp = data.value("T", 0LL);

        if (symbol.empty() || price == 0.0) return;

        // Convert timestamp to system clock
        auto tickTime = std::chrono::system_clock::time_point{
            std::chrono::milliseconds(timestamp)
        };

        // Feed to candle aggregator
        aggregator_.onTick(price, quantity, tickTime);

        // Create market tick
        MarketTick tick{
            symbol,
            price,
            quantity,
            timestamp
        };

        if (tickCallback_) {
            tickCallback_(tick);
        }

        // Alpha generation
        auto sigOpt = engine_.onTick(tick);
        if (sigOpt) {
            const auto& sig = *sigOpt;
            std::cout << "[Binance Alpha] "
                      << sig.symbol << " | $" << price
                      << " | Mom: " << sig.momentum
                      << " | MRZ: " << sig.meanRevZ << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "[Binance WS] Parse error: " << e.what() << std::endl;
    }
}
