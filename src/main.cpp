#include "alpha/alpha_engine.h"
#include "alpha/microstructure.h"
#include "alpha/orderflow.h"
#include "alpha/regime.h"
#include "alpha/vwap.h"
#include "alpha/indicators.h"
#include "feeds/binance_feed.h"
#include "feeds/polygon_feed.h"
#include "feeds/coinbase_feed.h"
#include "feeds/candle_aggregator.h"
#include "backtest/backtester.h"
#include <curl/curl.h>

#include "storage/influx_writer.h"

#include <memory>
#include <vector>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <iomanip>
#include <map>
#include <deque>
#include <optional>

// ==============================
//    BOLLINGER BANDS TRACKER
// ==============================

struct BollingerMetrics {
    double middleBand;
    double upperBand;
    double lowerBand;
    double bandwidth;      // (upper - lower) / middle
    double percentB;       // (price - lower) / (upper - lower)
    bool isSqueezing;      // bandwidth < 5%
    std::string signal;    // "BUY", "SELL", "NEUTRAL"
};

class BollingerTracker {
public:
    explicit BollingerTracker(int period = 10, double mult = 2.0)
        : period_(period), mult_(mult) {}

    std::optional<BollingerMetrics> onPrice(double price) {
        prices_.push_back(price);
        if (prices_.size() > static_cast<size_t>(period_)) {
            prices_.pop_front();
        }

        if (prices_.size() < static_cast<size_t>(period_)) {
            return std::nullopt;
        }

        // Calculate Bollinger Bands
        std::vector<double> priceVec(prices_.begin(), prices_.end());
        double mean, upper, lower;
        computeBollinger(priceVec, period_, mult_, mean, upper, lower);

        // Calculate metrics
        BollingerMetrics metrics;
        metrics.middleBand = mean;
        metrics.upperBand = upper;
        metrics.lowerBand = lower;
        metrics.bandwidth = (mean > 0) ? (upper - lower) / mean : 0.0;
        metrics.percentB = (upper != lower) ? (price - lower) / (upper - lower) : 0.5;
        metrics.isSqueezing = metrics.bandwidth < 0.05;  // 5% bandwidth

        // Generate signal
        if (price < lower && metrics.percentB < 0.1) {
            metrics.signal = "BUY";
        } else if (price > upper && metrics.percentB > 0.9) {
            metrics.signal = "SELL";
        } else if (metrics.isSqueezing && metrics.percentB > 0.5) {
            metrics.signal = "BREAKOUT_UP";
        } else if (metrics.isSqueezing && metrics.percentB < 0.5) {
            metrics.signal = "BREAKOUT_DOWN";
        } else {
            metrics.signal = "NEUTRAL";
        }

        return metrics;
    }

    void reset() {
        prices_.clear();
    }

private:
    int period_;
    double mult_;
    std::deque<double> prices_;
};

// ==========================
//     ALPHA ENGINE
// ==========================

class ProductionAlphaSystem {
public:
    ProductionAlphaSystem()
        : alphaEngine_(20, "1m"),
          microstructure_(50, 50, 100),
          regime_(100, 20, 50),
          vwap_(2.0, 0),
          bollinger_(20, 2.0),
          lastPrice_(0.0),
          tickCount_(0) {

        // InfluxDB wiring
        const char* org    = std::getenv("INFLUX_ORG");
        const char* bucket = std::getenv("INFLUX_BUCKET");
        const char* token  = std::getenv("INFLUX_TOKEN");
        const char* url    = std::getenv("INFLUX_URL");

        if (org && bucket && token && url) {
            influx_ = std::make_shared<InfluxWriter>(org, bucket, token, url);
            std::cout << "✅ InfluxDB writer attached for the engine\n";
        }
    }

    void processMarketTick(const MarketTick& tick) {
        // 1. Basic Alpha Signals
        auto basicSignal = alphaEngine_.onTick(tick);

        // 2. Microstructure Analysis (VPIN, Hasbrouck)
        microstructure_.onTick(tick);
        auto vpinMetrics = microstructure_.getVPIN();
        auto hasbrouckMetrics = microstructure_.getHasbrouckMetrics();

        // 3. Order Flow Analysis (OFI)
        bool isBuy = (tick.price > lastPrice_);
        auto flowSignal = orderflow_.onTick(tick, isBuy);

        // 4. Regime Detection
        regime_.onTick(tick);
        auto regimeMetrics = regime_.getMetrics();

        // 5. VWAP Calculation
        vwap_.onTick(tick);
        auto vwapMetrics = vwap_.getMetrics();

        // 6. ✅ BOLLINGER BANDS
        auto bollingerSignal = bollinger_.onPrice(tick.price);

        // ✅ InfluxDB writes
        if (influx_) {
            // Alpha / Bollinger summary
            influx_->writeAlphaSignal(
                tick.symbol,
                basicSignal ? basicSignal->momentum : 0.0,
                basicSignal ? basicSignal->meanRevZ : 0.0,
                bollingerSignal ? bollingerSignal->percentB : 0.0,
                0.0,  // reserved for combinedScore or extra signal later
                regime::regimeToString(regimeMetrics.regime)
            );

            // Microstructure (VPIN / Hasbrouck)
            influx_->writeMicrostructureSignal(
                tick.symbol,
                vpinMetrics.vpin,
                vpinMetrics.toxicity,
                hasbrouckMetrics.lambda,
                0.0,              // spread placeholder
                tick.timestamp
            );

            // Order flow
            influx_->writeOrderFlowSignal(
                tick.symbol,
                flowSignal ? flowSignal->ofi : 0.0,
                0.0,  // buy volume placeholder
                0.0,  // sell volume placeholder
                0.0,  // imbalance placeholder
                tick.timestamp
            );

            // Regime state
            influx_->writeRegimeSignal(
                tick.symbol,
                regime::regimeToString(regimeMetrics.regime),
                regimeMetrics.hurstExponent,
                regimeMetrics.volatility,
                regimeMetrics.trendStrength,
                tick.timestamp
            );
        }

        lastPrice_ = tick.price;
        tickCount_++;

        // Only print every 3rd tick to avoid spam
        if (tickCount_ % 3 != 0) return;

        // ===========================================
        //     DISPLAY COMPREHENSIVE ALPHA REPORT
        // ===========================================

        if (basicSignal && flowSignal) {
            std::cout << "\n╔══════════════════════════════════════════════════════════╗" << std::endl;
            std::cout << "║     ALPHA SIGNAL: " << std::setw(10) << tick.symbol
                      << " | Price: $" << std::fixed << std::setprecision(2) << tick.price
                      << std::setw(20) << " ║" << std::endl;
            std::cout << "╠══════════════════════════════════════════════════════════╣" << std::endl;

            // Basic Signals
            std::cout << "║    MOMENTUM:        "
                      << std::setw(8) << std::fixed << std::setprecision(4)
                      << basicSignal->momentum * 100 << "%" << std::setw(25) << "║" << std::endl;

            std::cout << "║    MEAN REV Z:      "
                      << std::setw(8) << std::fixed << std::setprecision(4)
                      << basicSignal->meanRevZ << std::setw(30) << "║" << std::endl;

            // BOLLINGER BANDS
            if (bollingerSignal) {
                std::cout << "║    BOLLINGER:                                        ║" << std::endl;
                std::cout << "║    Upper:  $" << std::setw(8) << std::setprecision(2)
                          << bollingerSignal->upperBand << std::setw(32) << "║" << std::endl;
                std::cout << "║    Middle: $" << std::setw(8) << bollingerSignal->middleBand
                          << std::setw(32) << "║" << std::endl;
                std::cout << "║    Lower:  $" << std::setw(8) << bollingerSignal->lowerBand
                          << std::setw(32) << "║" << std::endl;
                std::cout << "║    %B:      " << std::setw(8) << std::setprecision(3)
                          << bollingerSignal->percentB << std::setw(32) << "║" << std::endl;

                if (bollingerSignal->isSqueezing) {
                    std::cout << "║   SQUEEZE DETECTED - Breakout Imminent!         ║" << std::endl;
                }

                if (bollingerSignal->signal != "NEUTRAL") {
                    std::cout << "║    Signal: " << std::setw(15) << bollingerSignal->signal
                              << std::setw(24) << "║" << std::endl;
                }
            }

            // Microstructure
            if (vpinMetrics.vpin > 0.01) {
                std::cout << "║ VPIN (Toxicity): "
                          << std::setw(8) << std::fixed << std::setprecision(4)
                          << vpinMetrics.vpin
                          << (vpinMetrics.toxicity > 0.5 ? "   TOXIC!" : " ✅")
                          << std::setw(15) << "║" << std::endl;

                std::cout << "║ Price Impact:    "
                          << std::setw(8) << std::fixed << std::setprecision(6)
                          << hasbrouckMetrics.lambda << std::setw(26) << "║" << std::endl;
            }

            // Order Flow
            if (flowSignal->ofi != 0.0) {
                std::cout << "║   Order Flow OFI:  "
                          << std::setw(8) << std::fixed << std::setprecision(4)
                          << flowSignal->ofi
                          << " (" << flowSignal->flowDirection << ")"
                          << std::setw(10) << "║" << std::endl;
            }

            // Regime
            std::cout << "║    REGIME:          "
                      << regime::regimeToString(regimeMetrics.regime).substr(0, 20)
                      << std::setw(20) << "║" << std::endl;

            std::cout << "║    Hurst Exp:       "
                      << std::setw(8) << std::fixed << std::setprecision(4)
                      << regimeMetrics.hurstExponent
                      << (regimeMetrics.hurstExponent > 0.55 ? " (Trending)" : " (Mean-Rev)")
                      << std::setw(10) << "║" << std::endl;

            // VWAP
            if (vwapMetrics.vwap > 0.01) {
                std::cout << "║    VWAP:            $"
                          << std::setw(8) << std::fixed << std::setprecision(2)
                          << vwapMetrics.vwap
                          << " (Dev: " << std::setprecision(2) << vwapMetrics.deviation << "%)"
                          << std::setw(8) << "║" << std::endl;
            }

            // TRADING SIGNALS
            auto weights = regime_.getSignalWeights();
            double combinedScore = weights.momentumWeight * basicSignal->momentum +
                                   weights.meanRevWeight * basicSignal->meanRevZ;

            std::string signal = "NEUTRAL";
            if (bollingerSignal && bollingerSignal->signal == "BUY" &&
                combinedScore > 0.01 && vpinMetrics.toxicity < 0.5) {
                signal = " STRONG BUY (BB Confirm)";
            } else if (bollingerSignal && bollingerSignal->signal == "SELL" &&
                       combinedScore < -0.01 && vpinMetrics.toxicity < 0.5) {
                signal = " STRONG SELL (BB Confirm)";
            } else if (combinedScore > 0.01 && vpinMetrics.toxicity < 0.5) {
                signal = " BUY";
            } else if (combinedScore < -0.01 && vpinMetrics.toxicity < 0.5) {
                signal = " SELL";
            } else if (vpinMetrics.toxicity > 0.7) {
                signal = " WAIT (Toxic Flow)";
            } else if (bollingerSignal && bollingerSignal->isSqueezing) {
                signal = " WAIT (BB Squeeze)";
            }

            std::cout << "╠══════════════════════════════════════════════════════════╣" << std::endl;
            std::cout << "║   SIGNAL:          " << std::setw(30) << signal << std::setw(10) << "║" << std::endl;
            std::cout << "╚══════════════════════════════════════════════════════════╝\n" << std::endl;
        }
    }

private:
    AlphaEngine alphaEngine_;
    MicrostructureAnalyzer microstructure_;
    OrderFlowEngine orderflow_;
    RegimeDetector regime_;
    VWAPCalculator vwap_;
    BollingerTracker bollinger_;
    std::shared_ptr<InfluxWriter> influx_;
    double lastPrice_;
    int tickCount_;
};


void runEnhancedLiveTrading() {
    std::cout << " Starting ENHANCED ALPHA SYSTEM...\n" << std::endl;

    const char* key = std::getenv("POLYGON_API_KEY");
    if (!key) throw std::runtime_error(" POLYGON_API_KEY is not set");
    std::string polygonKey(key);

    // ONE ALPHA SYSTEM PER SYMBOL
    std::map<std::string, std::shared_ptr<ProductionAlphaSystem>> alphaSystems;

    std::vector<std::string> symbols = {"AAPL", "MSFT"};
    for (const auto& symbol : symbols) {
        alphaSystems[symbol] = std::make_shared<ProductionAlphaSystem>();
    }

    auto engine = std::make_shared<AlphaEngine>(20, "1m");
    auto aggregator = std::make_shared<CandleAggregator>(60);

    aggregator->setOnCandleClosed([engine](const Candle& c) {
        engine->onCandle(c);
    });

    PolygonFeed polygonFeed(symbols, polygonKey, *engine, *aggregator);

    // Route each symbol to its own alpha system
    polygonFeed.setTickCallback([&alphaSystems](const MarketTick& tick) {
        auto it = alphaSystems.find(tick.symbol);
        if (it != alphaSystems.end()) {
            it->second->processMarketTick(tick);
        }
    });

    std::thread polygonThread([&]() {
        std::cout << " Polygon feed started\n";
        polygonFeed.start();
    });

    std::cout << "✅ All systems operational\n" << std::endl;
    polygonThread.join();
}

void runCoinbaseLive() {
    std::cout << " Starting COINBASE CRYPTO FEED (24/7 Live!)...\n" << std::endl;

    // Create one alpha system per product
    std::map<std::string, std::shared_ptr<ProductionAlphaSystem>> alphaSystems;

    std::vector<std::string> products = {
        "ETH-USD",   // Ethereum
        "SOL-USD"    // Solana
    };

    for (const auto& product : products) {
        alphaSystems[product] = std::make_shared<ProductionAlphaSystem>();
    }

    auto engine = std::make_shared<AlphaEngine>(20, "1m");
    auto aggregator = std::make_shared<CandleAggregator>(60);

    aggregator->setOnCandleClosed([engine](const Candle& c) {
        engine->onCandle(c);
    });

    CoinbaseAdvancedFeed coinbaseFeed(products, *engine, *aggregator);

    // Route to per-symbol alpha systems
    coinbaseFeed.setTickCallback([&alphaSystems](const MarketTick& tick) {
        auto it = alphaSystems.find(tick.symbol);
        if (it != alphaSystems.end()) {
            it->second->processMarketTick(tick);
        }
    });

    coinbaseFeed.start();

    std::cout << " Coinbase feed running. Press Ctrl+C to stop.\n" << std::endl;

    // Keep running
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void runAllExchanges() {
    std::cout << " Starting ALL EXCHANGES (Binance + Coinbase + Polygon!)...\n" << std::endl;

    // Create alpha systems for all symbols across all exchanges
    std::map<std::string, std::shared_ptr<ProductionAlphaSystem>> alphaSystems;

    // Binance symbols
    std::vector<std::string> binanceSymbols = {"BTCUSDT", "BNBUSDT"};
    for (const auto& s : binanceSymbols) {
        alphaSystems[s] = std::make_shared<ProductionAlphaSystem>();
    }

    // Coinbase symbols
    std::vector<std::string> coinbaseProducts = {"ETH-USD", "SOL-USD"};
    for (const auto& s : coinbaseProducts) {
        alphaSystems[s] = std::make_shared<ProductionAlphaSystem>();
    }

    // Polygon symbols
    std::vector<std::string> polygonSymbols = {"AAPL", "MSFT", "GOOGL"};
    for (const auto& s : polygonSymbols) {
        alphaSystems[s] = std::make_shared<ProductionAlphaSystem>();
    }

    // Engines & aggregators per exchange
    auto binanceEngine = std::make_shared<AlphaEngine>(20, "1m");
    auto binanceAgg    = std::make_shared<CandleAggregator>(60);

    auto coinbaseEngine = std::make_shared<AlphaEngine>(20, "1m");
    auto coinbaseAgg    = std::make_shared<CandleAggregator>(60);

    auto polygonEngine  = std::make_shared<AlphaEngine>(20, "1m");
    auto polygonAgg     = std::make_shared<CandleAggregator>(60);

    // Feeds
    auto binanceFeed  = std::make_shared<BinancePublicFeed>(binanceSymbols, *binanceEngine, *binanceAgg);
    auto coinbaseFeed = std::make_shared<CoinbaseAdvancedFeed>(coinbaseProducts, *coinbaseEngine, *coinbaseAgg);

    const char* polygonKey = std::getenv("POLYGON_API_KEY");
    std::unique_ptr<PolygonFeed> polygonFeed;
    if (polygonKey) {
        polygonFeed = std::make_unique<PolygonFeed>(
            polygonSymbols, std::string(polygonKey), *polygonEngine, *polygonAgg
        );
    }

    // Shared callback
    auto callback = [&alphaSystems](const MarketTick& tick) {
        auto it = alphaSystems.find(tick.symbol);
        if (it != alphaSystems.end()) {
            it->second->processMarketTick(tick);
        }
    };

    binanceFeed->setTickCallback(callback);
    coinbaseFeed->setTickCallback(callback);
    if (polygonFeed) {
        polygonFeed->setTickCallback(callback);
    }

    // Threads
    std::thread binanceThread([binanceFeed]() { binanceFeed->start(); });
    std::thread coinbaseThread([coinbaseFeed]() { coinbaseFeed->start(); });
    std::thread polygonThread;

    if (polygonFeed) {
        PolygonFeed* rawPolygonFeed = polygonFeed.get();
        polygonThread = std::thread([rawPolygonFeed]() { rawPolygonFeed->start(); });
    }

    binanceThread.detach();
    coinbaseThread.detach();
    if (polygonThread.joinable()) {
        polygonThread.detach();
    }

    std::cout << " ALL EXCHANGES RUNNING!\n" << std::endl;
    std::cout << " Binance: "  << binanceSymbols.size()   << " symbols" << std::endl;
    std::cout << " Coinbase: " << coinbaseProducts.size() << " symbols" << std::endl;
    if (polygonFeed) {
        std::cout << " Polygon: " << polygonSymbols.size() << " symbols" << std::endl;
    }
    std::cout << "\nPress Ctrl+C to stop.\n" << std::endl;

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void runBacktestDemo() {
    std::cout << " Running Backtest with Bollinger Bands + Performance Metrics...\n" << std::endl;

    std::vector<MarketTick> historicalData;
    double price = 280.0;

    std::cout << " Generating 1000 synthetic ticks..." << std::endl;

    for (int i = 0; i < 1000; ++i) {
        double change = ((rand() % 200) - 95) / 10000.0;
        price *= (1.0 + change);

        historicalData.push_back(MarketTick{
            "AAPL",
            price,
            1000.0 + (rand() % 500),
            static_cast<long>(i * 1000)
        });
    }

    BacktestConfig config;
    config.initialCapital = 100000.0;
    config.commissionRate = 0.001;
    config.slippageBps    = 2.0;

    Backtester backtester(config);

    auto signalGen = [](const MarketTick& tick) -> int {
        static std::deque<double> prices;
        static int tickCount = 0;
        tickCount++;

        prices.push_back(tick.price);
        if (prices.size() > 20) prices.pop_front();

        if (prices.size() < 20) {
            return 0;  // HOLD
        }

        std::vector<double> priceVec(prices.begin(), prices.end());
        double mean, upper, lower;
        computeBollinger(priceVec, 20, 2.0, mean, upper, lower);

        double momentum = (prices.back() / prices.front()) - 1.0;
        double percentB = (upper != lower) ? (tick.price - lower) / (upper - lower) : 0.5;

        if (percentB < 0.2 && momentum > 0.005 && tickCount % 50 == 0) {
            return 1;   // BUY
        } else if (percentB > 0.8 && momentum < -0.005 && tickCount % 50 == 25) {
            return -1;  // SELL
        }

        return 0;  // HOLD
    };

    std::cout << " Running backtest with Bollinger Bands strategy..." << std::endl;
    auto result = backtester.run(historicalData, signalGen);

    std::cout << "\n Backtest complete!\n" << std::endl;

    std::cout << " Performance Summary:" << std::endl;
    std::cout << "   • Total Return: " << std::fixed << std::setprecision(2)
              << result.totalReturn << "%" << std::endl;
    std::cout << "   • Total Trades: " << result.numTrades << std::endl;
    std::cout << "   • Win Rate: " << std::setprecision(2)
              << result.winRate * 100 << "%" << std::endl;
    std::cout << "   • Sharpe Ratio: " << std::setprecision(3)
              << result.sharpeRatio << std::endl;
    std::cout << "   • Max Drawdown: " << std::setprecision(2)
              << result.maxDrawdown << std::endl;
    std::cout << "   • Profit Factor: " << std::setprecision(2)
              << result.profitFactor << "\n" << std::endl;
}

void runBinanceLive() {
    std::cout << " Starting BINANCE CRYPTO FEED (24/7 Live!)...\n" << std::endl;

    // Create one alpha system per symbol
    std::map<std::string, std::shared_ptr<ProductionAlphaSystem>> alphaSystems;

    std::vector<std::string> symbols = {
        "BTCUSDT",   // Bitcoin
        "ETHUSDT",   // Ethereum
        "BNBUSDT"    // Binance Coin
    };

    for (const auto& symbol : symbols) {
        alphaSystems[symbol] = std::make_shared<ProductionAlphaSystem>();
    }

    auto engine     = std::make_shared<AlphaEngine>(20, "1m");
    auto aggregator = std::make_shared<CandleAggregator>(60);

    aggregator->setOnCandleClosed([engine](const Candle& c) {
        engine->onCandle(c);
    });

    BinancePublicFeed binanceFeed(symbols, *engine, *aggregator);

    binanceFeed.setTickCallback([&alphaSystems](const MarketTick& tick) {
        auto it = alphaSystems.find(tick.symbol);
        if (it != alphaSystems.end()) {
            it->second->processMarketTick(tick);
        }
    });

    binanceFeed.start();

    std::cout << " Binance feed running. Press Ctrl+C to stop.\n" << std::endl;

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}


int main(int argc, char* argv[]) {
    curl_global_init(CURL_GLOBAL_ALL);

    std::string mode = "live";

    if (argc > 1) {
        mode = argv[1];
    }

    std::cout << R"(
╔═══════════════════════════════════════════════════════════╗
║                                                           ║
║     MULTI-EXCHANGE ALPHA GENERATION ENGINE                ║
║                                                           ║
║     Features: VPIN | Hasbrouck | OFI | Regime | VWAP      ║
║     Exchanges: Binance | Coinbase | Polygon               ║
║     Research-Backed | Low-Latency                         ║
║                                                           ║
╚═══════════════════════════════════════════════════════════╝
    )" << std::endl;

    int exit_code = 0;

    try {
        if (mode == "live" || mode == "all") {
            runAllExchanges();
        } else if (mode == "binance") {
            runBinanceLive();
        } else if (mode == "backtest") {
            runBacktestDemo();
        } else {
            std::cout << "   Usage:" << std::endl;
            std::cout << "  ./alpha_engine live      - Run live trading (all features)" << std::endl;
            std::cout << "  ./alpha_engine backtest  - Run backtest with Bollinger Bands" << std::endl;
            throw std::runtime_error("Unknown mode: " + mode);
        }
    }
    catch (const std::exception& e) {
        std::cerr << "\n ERROR: " << e.what() << std::endl;
        exit_code = 1;
    }

    curl_global_cleanup();
    return exit_code;
}
