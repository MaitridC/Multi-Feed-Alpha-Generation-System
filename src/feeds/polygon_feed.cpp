#include "feeds/polygon_feed.h"
#include "alpha/alpha_engine.h"
#include "feeds/candle_aggregator.h"
#include "util/market_types.h"

#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <chrono>
#include <thread>

using json = nlohmann::json;

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
    auto* s = static_cast<std::string*>(userp);
    s->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

PolygonFeed::PolygonFeed(const std::vector<std::string>& symbols,
                         const std::string& apiKey,
                         AlphaEngine& engine,
                         CandleAggregator& aggregator)
    : symbols_(symbols),
      apiKey_(apiKey),
      engine_(engine),
      aggregator_(aggregator),
      running_(false)
{}

void PolygonFeed::setTickCallback(std::function<void(const MarketTick&)> callback) {
    tickCallback_ = std::move(callback);
}


void PolygonFeed::start()
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
    running_ = true;
    pollLoop();
    pollThread_ = std::thread(&PolygonFeed::pollLoop, this);
    curl_global_cleanup();
}

void PolygonFeed::stop()
{
    running_ = false;
    if (pollThread_.joinable())
        pollThread_.join();
}


void PolygonFeed::pollLoop()
{
    std::cout << "[Polygon REST] Polling started (30s)..." << std::endl;

    try
    {
        while (running_)
        {
            for (const auto& symbol : symbols_)
            {
                fetchSymbol(symbol);
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }

            std::this_thread::sleep_for(std::chrono::seconds(30));
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "[Polygon REST] Uncaught exception in pollLoop: "
                  << e.what() << std::endl;
    }
    catch (...)
    {
        std::cerr << "[Polygon REST] Unknown exception in pollLoop" << std::endl;
    }
}


void PolygonFeed::fetchSymbol(const std::string& symbol) {
    CURL* curl = curl_easy_init();
    if (!curl) return;

    long long to = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    long long from = to - (30LL * 24 * 60 * 60 * 1000);

    std::string url =
        "https://api.polygon.io/v2/aggs/ticker/" + symbol +
        "/range/1/day/" + std::to_string(from) + "/" + std::to_string(to) +
        "?adjusted=true&sort=desc&limit=5&apiKey=" + apiKey_;

    std::string response;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        std::cerr << "[Polygon REST] CURL error for " << symbol << std::endl;
        return;
    }

    try {
        auto j = json::parse(response);

        if (!j.contains("results") || j["results"].empty()) {
            std::cerr << "[Polygon REST] No results for " << symbol << std::endl;
            std::cerr << "[Polygon REST] Response: " << response.substr(0, 200) << std::endl;
            return;
        }

        std::cout << "[Polygon REST] Got " << j["results"].size()
                  << " bars for " << symbol << std::endl;

        for (const auto& c : j["results"]) {
            double open  = c.value("o", 0.0);
            double high  = c.value("h", 0.0);
            double low   = c.value("l", 0.0);
            double close = c.value("c", 0.0);
            double vol   = c.value("v", 0.0);
            long long ts = c.value("t", 0LL);

            auto tickTime =
                std::chrono::system_clock::time_point{
                    std::chrono::milliseconds(ts)
                };

            aggregator_.onTick(close, vol, tickTime);

            MarketTick tick{
                symbol,
                close,
                vol,
                ts
            };

            if (tickCallback_) {
                tickCallback_(tick);
            }

            // Basic alpha generation
            auto sigOpt = engine_.onTick(tick);
            if (sigOpt) {
                const auto& sig = *sigOpt;
                std::cout << "[Polygon REST Alpha] "
                          << sig.symbol << " | "
                          << "Price: $" << close << " | "
                          << "Momentum: " << sig.momentum << " | "
                          << "MeanRevZ: " << sig.meanRevZ << " | "
                          << "Signal: " << sig.type << std::endl;
            }

            std::cout << "[Polygon REST] " << symbol
                      << " | O:" << open << " H:" << high << " L:" << low
                      << " C:$" << close << " | Vol: " << vol << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "[Polygon REST] Parse error: " << e.what() << std::endl;
    }
}