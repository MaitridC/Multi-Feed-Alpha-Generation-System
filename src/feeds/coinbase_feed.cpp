#include "feeds/coinbase_feed.h"
#include "alpha/alpha_engine.h"
#include "feeds/candle_aggregator.h"
#include "util/market_types.h"

#include <nlohmann/json.hpp>
#include <iostream>
#include <chrono>

using json = nlohmann::json;

CoinbaseAdvancedFeed::CoinbaseAdvancedFeed(
    const std::vector<std::string>& productIds,
    AlphaEngine& engine,
    CandleAggregator& aggregator
)
    : productIds_(productIds),
      engine_(engine),
      aggregator_(aggregator),
      running_(false)
{}

void CoinbaseAdvancedFeed::setTickCallback(std::function<void(const MarketTick&)> callback) {
    tickCallback_ = std::move(callback);
}

void CoinbaseAdvancedFeed::start() {
    running_ = true;
    wsThread_ = std::thread(&CoinbaseAdvancedFeed::connectWebSocket, this);
}

void CoinbaseAdvancedFeed::stop() {
    running_ = false;
    if (ws_) {
        ws_->stop();
    }
    if (wsThread_.joinable()) {
        wsThread_.join();
    }
}

void CoinbaseAdvancedFeed::connectWebSocket() {
    // Coinbase Advanced Trade WebSocket
    std::string url = "wss://advanced-trade-ws.coinbase.com";

    std::cout << "[Coinbase WS] Connecting to: " << url << std::endl;

    ws_ = std::make_unique<ix::WebSocket>();
    ws_->setUrl(url);

    ws_->setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            handleMessage(msg->str);
        }
        else if (msg->type == ix::WebSocketMessageType::Open) {
            std::cout << "[Coinbase WS] Connected! Subscribing to channels..." << std::endl;
            subscribe();
        }
        else if (msg->type == ix::WebSocketMessageType::Error) {
            std::cerr << "[Coinbase WS] Error: " << msg->errorInfo.reason << std::endl;
        }
        else if (msg->type == ix::WebSocketMessageType::Close) {
            std::cout << "[Coinbase WS] Connection closed" << std::endl;
        }
    });

    ws_->start();

    // Keep thread alive
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void CoinbaseAdvancedFeed::subscribe() {
    // Coinbase Advanced Trade format
    json subscribeMsg = {
        {"type", "subscribe"},
        {"product_ids", productIds_},
        {"channels", json::array({
            "ticker",
            "matches"
        })}
    };

    std::string msgStr = subscribeMsg.dump();
    std::cout << "[Coinbase WS] Subscribing: " << msgStr << std::endl;

    ws_->send(msgStr);
}

void CoinbaseAdvancedFeed::handleMessage(const std::string& message) {
    try {
        auto j = json::parse(message);

        std::string type = j.value("type", "");

        if (type == "subscriptions") {
            std::cout << "[Coinbase WS] Subscribed successfully" << std::endl;
            return;
        }

        // Handle ticker updates
        if (type == "ticker") {
            std::string productId = j.value("product_id", "");

            std::string priceStr = j.value("price", "0");
            double price = std::stod(priceStr);

            std::string volumeStr = j.value("best_bid_size", "0");
            double volume = std::stod(volumeStr);

            std::string timeStr = j.value("time", "");
            long long timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();

            if (productId.empty() || price == 0.0) return;

            auto tickTime = std::chrono::system_clock::time_point{
                std::chrono::milliseconds(timestamp)
            };

            aggregator_.onTick(price, volume, tickTime);

            MarketTick tick{
                productId,
                price,
                volume,
                timestamp
            };

            if (tickCallback_) {
                tickCallback_(tick);
            }

            //Alpha generation
            auto sigOpt = engine_.onTick(tick);
            if (sigOpt) {
                const auto& sig = *sigOpt;
                std::cout << "[Coinbase Alpha] "
                          << sig.symbol << " | $" << price
                          << " | Mom: " << sig.momentum
                          << " | MRZ: " << sig.meanRevZ << std::endl;
            }
        }

        else if (type == "match" || type == "last_match") {
            std::string productId = j.value("product_id", "");

            std::string priceStr = j.value("price", "0");
            double price = std::stod(priceStr);

            std::string sizeStr = j.value("size", "0");
            double size = std::stod(sizeStr);

            long long timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();

            if (productId.empty() || price == 0.0) return;

            auto tickTime = std::chrono::system_clock::time_point{
                std::chrono::milliseconds(timestamp)
            };

            aggregator_.onTick(price, size, tickTime);

            MarketTick tick{
                productId,
                price,
                size,
                timestamp
            };

            if (tickCallback_) {
                tickCallback_(tick);
            }

            auto sigOpt = engine_.onTick(tick);
            if (sigOpt) {
                const auto& sig = *sigOpt;
                std::cout << "[Coinbase Trade] "
                          << sig.symbol << " | $" << price
                          << " | Size: " << size << std::endl;
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "[Coinbase WS] Parse error: " << e.what() << std::endl;
        std::cerr << "[Coinbase WS] Message: " << message.substr(0, 200) << std::endl;
    }
}