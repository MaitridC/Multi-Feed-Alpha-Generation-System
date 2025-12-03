#pragma once
#include "util/market_types.h"
#include "pnl.h"
#include <vector>
#include <functional>
#include <string>


struct BacktestConfig {
    double initialCapital = 10000.0;
    double commissionRate = 0.001;     // 0.1% per trade
    double slippageBps = 2.0;          // 2 basis points
    double latencyMs = 10.0;           // 10ms execution delay
    double maxPositionSize = 0.5;      // 50% of capital per position
    bool enableShortSelling = true;
    bool enableMarginTrading = false;
    double marginRequirement = 0.5;    // 50% margin
};

struct Trade {
    std::string symbol;
    long timestamp;
    double entryPrice;
    double exitPrice;
    double quantity;
    bool isLong;
    double pnl;
    double commission;
    double slippage;
    std::string entryReason;
    std::string exitReason;
};

struct BacktestResult {
    std::vector<Trade> trades;
    double totalPnL;
    double totalReturn;
    double sharpeRatio;
    double maxDrawdown;
    double winRate;
    int numTrades;
    int numWinningTrades;
    int numLosingTrades;
    double avgWin;
    double avgLoss;
    double profitFactor;        // avg win / avg loss
    double expectancy;          // avg trade PnL

    // Equity curve
    std::vector<double> equityCurve;
    std::vector<long> timestamps;
};

// Signal generator callback
using SignalGenerator = std::function<int(const MarketTick&)>;  // Returns: 1=buy, -1=sell, 0=hold

class Backtester {
public:
    explicit Backtester(const BacktestConfig& config = BacktestConfig());

    // Run backtest on historical data
    BacktestResult run(
        const std::vector<MarketTick>& historicalData,
        SignalGenerator signalFunc
    );

    // Run walk-forward analysis
    std::vector<BacktestResult> walkForward(
        const std::vector<MarketTick>& historicalData,
        SignalGenerator signalFunc,
        size_t trainPeriod,
        size_t testPeriod
    );

    // Run Monte Carlo simulation
    std::vector<BacktestResult> monteCarlo(
        const std::vector<MarketTick>& historicalData,
        SignalGenerator signalFunc,
        int numSimulations = 1000
    );

    // Set custom execution model
    void setExecutionModel(
        std::function<double(double price, double quantity, bool isBuy)> model
    );

private:
    BacktestConfig config_;
    PnLTracker pnlTracker_;

    // Current state
    double currentPosition_;
    double avgEntryPrice_;
    double currentCash_;

    // Execution modeling
    double applySlippage(double price, double quantity, bool isBuy) const;
    double calculateCommission(double notional) const;

    // Position management
    bool canEnterPosition(double price, double quantity) const;
    void enterPosition(const MarketTick& tick, double quantity, bool isLong, const std::string& reason);
    void exitPosition(const MarketTick& tick, const std::string& reason);

    // Results calculation
    BacktestResult computeResults(const std::vector<Trade>& trades) const;
};
