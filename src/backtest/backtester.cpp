#include "backtest/backtester.h"
#include "backtest/sharpe.h"
#include <algorithm>
#include <random>
#include <iostream>

Backtester::Backtester(const BacktestConfig& config)
    : config_(config),
      pnlTracker_(config.initialCapital),
      currentPosition_(0.0),
      avgEntryPrice_(0.0),
      currentCash_(config.initialCapital) {}

BacktestResult Backtester::run(
    const std::vector<MarketTick>& historicalData,
    SignalGenerator signalFunc
) {
    // Reset state
    currentPosition_ = 0.0;
    avgEntryPrice_ = 0.0;
    currentCash_ = config_.initialCapital;
    pnlTracker_.reset();

    std::vector<Trade> trades;
    std::vector<double> equityCurve;
    std::vector<long> timestamps;

    for (const auto& tick : historicalData) {
        // Generate signal
        int signal = signalFunc(tick);

        // Execute trades based on signal
        if (signal == 1 && currentPosition_ <= 0) {
            // Buy signal
            double maxQuantity = (currentCash_ * config_.maxPositionSize) / tick.price;
            if (canEnterPosition(tick.price, maxQuantity)) {
                enterPosition(tick, maxQuantity, true, "SIGNAL_BUY");
            }
        } else if (signal == -1 && currentPosition_ >= 0) {
            // Sell signal
            if (currentPosition_ > 0) {
                exitPosition(tick, "SIGNAL_SELL");

                // Record trade
                Trade trade;
                trade.symbol = tick.symbol;
                trade.timestamp = tick.timestamp;
                trade.entryPrice = avgEntryPrice_;
                trade.exitPrice = tick.price;
                trade.quantity = currentPosition_;
                trade.isLong = true;
                trade.pnl = (tick.price - avgEntryPrice_) * currentPosition_;
                trade.commission = calculateCommission(tick.price * currentPosition_);
                trade.slippage = applySlippage(tick.price, currentPosition_, false) - tick.price;
                trade.entryReason = "SIGNAL_BUY";
                trade.exitReason = "SIGNAL_SELL";

                trades.push_back(trade);
            }

            // Short if enabled
            if (config_.enableShortSelling) {
                double maxQuantity = (currentCash_ * config_.maxPositionSize) / tick.price;
                if (canEnterPosition(tick.price, maxQuantity)) {
                    enterPosition(tick, -maxQuantity, false, "SIGNAL_SELL");
                }
            }
        }

        // Update equity curve
        double equity = currentCash_ + currentPosition_ * tick.price;
        equityCurve.push_back(equity);
        timestamps.push_back(tick.timestamp);

        pnlTracker_.updatePrice(tick.symbol, tick.price);
    }

    // Close any open positions at end
    if (currentPosition_ != 0.0 && !historicalData.empty()) {
        exitPosition(historicalData.back(), "END_OF_BACKTEST");
    }

    return computeResults(trades);
}

std::vector<BacktestResult> Backtester::walkForward(
    const std::vector<MarketTick>& historicalData,
    SignalGenerator signalFunc,
    size_t trainPeriod,
    size_t testPeriod
) {
    std::vector<BacktestResult> results;

    for (size_t i = 0; i + trainPeriod + testPeriod < historicalData.size();
         i += testPeriod) {

        // Extract test period data
        std::vector<MarketTick> testData(
            historicalData.begin() + i + trainPeriod,
            historicalData.begin() + i + trainPeriod + testPeriod
        );

        // Run backtest on test period
        BacktestResult result = run(testData, signalFunc);
        results.push_back(result);

        std::cout << "[Walk-Forward] Period " << (i / testPeriod + 1)
                  << " - PnL: " << result.totalPnL
                  << " | Sharpe: " << result.sharpeRatio
                  << " | Trades: " << result.numTrades
                  << std::endl;
    }

    return results;
}

std::vector<BacktestResult> Backtester::monteCarlo(
    const std::vector<MarketTick>& historicalData,
    SignalGenerator signalFunc,
    int numSimulations
) {
    std::vector<BacktestResult> results;
    std::random_device rd;
    std::mt19937 gen(rd());

    for (int sim = 0; sim < numSimulations; ++sim) {
        // Shuffle historical data (bootstrap resampling)
        std::vector<MarketTick> shuffledData = historicalData;
        std::shuffle(shuffledData.begin(), shuffledData.end(), gen);

        BacktestResult result = run(shuffledData, signalFunc);
        results.push_back(result);

        if ((sim + 1) % 100 == 0) {
            std::cout << "[Monte Carlo] Completed " << (sim + 1)
                      << "/" << numSimulations << " simulations" << std::endl;
        }
    }

    return results;
}

void Backtester::setExecutionModel(
    std::function<double(double, double, bool)> model
) {
    // For now, using default slippage model
}


double Backtester::applySlippage(double price, double quantity, bool isBuy) const {
    double slippageFactor = 1.0 + (isBuy ? 1.0 : -1.0) * (config_.slippageBps / 10000.0);
    return price * slippageFactor;
}

double Backtester::calculateCommission(double notional) const {
    return notional * config_.commissionRate;
}

bool Backtester::canEnterPosition(double price, double quantity) const {
    double notional = price * std::abs(quantity);
    double requiredCapital = notional * (config_.enableMarginTrading ? config_.marginRequirement : 1.0);
    return requiredCapital <= currentCash_ * config_.maxPositionSize;
}

void Backtester::enterPosition(
    const MarketTick& tick,
    double quantity,
    bool isLong,
    const std::string& reason
) {
    double executionPrice = applySlippage(tick.price, quantity, isLong);
    double notional = executionPrice * std::abs(quantity);
    double commission = calculateCommission(notional);

    currentPosition_ = quantity;
    avgEntryPrice_ = executionPrice;
    currentCash_ -= notional + commission;

    pnlTracker_.addPosition(tick.symbol, quantity, executionPrice);

    std::cout << "[Entry] " << (isLong ? "LONG" : "SHORT")
              << " | Price: " << executionPrice
              << " | Qty: " << quantity
              << " | Reason: " << reason
              << std::endl;
}

void Backtester::exitPosition(const MarketTick& tick, const std::string& reason) {
    if (currentPosition_ == 0.0) return;

    bool isLong = currentPosition_ > 0;
    double executionPrice = applySlippage(tick.price, std::abs(currentPosition_), !isLong);
    double notional = executionPrice * std::abs(currentPosition_);
    double commission = calculateCommission(notional);

    double pnl = isLong ?
        (executionPrice - avgEntryPrice_) * currentPosition_ :
        (avgEntryPrice_ - executionPrice) * std::abs(currentPosition_);

    currentCash_ += notional - commission;
    pnlTracker_.closePosition(tick.symbol, executionPrice);

    std::cout << "[Exit] " << (isLong ? "LONG" : "SHORT")
              << " | Price: " << executionPrice
              << " | PnL: " << pnl
              << " | Reason: " << reason
              << std::endl;

    currentPosition_ = 0.0;
    avgEntryPrice_ = 0.0;
}

BacktestResult Backtester::computeResults(const std::vector<Trade>& trades) const {
    BacktestResult result;
    result.trades = trades;
    result.numTrades = trades.size();

    if (trades.empty()) {
        result.totalPnL = 0.0;
        result.totalReturn = 0.0;
        result.sharpeRatio = 0.0;
        result.maxDrawdown = 0.0;
        result.winRate = 0.0;
        result.numWinningTrades = 0;
        result.numLosingTrades = 0;
        result.avgWin = 0.0;
        result.avgLoss = 0.0;
        result.profitFactor = 0.0;
        result.expectancy = 0.0;
        return result;
    }

    // Calculate basic metrics
    result.totalPnL = 0.0;
    double totalWin = 0.0, totalLoss = 0.0;
    result.numWinningTrades = 0;
    result.numLosingTrades = 0;

    for (const auto& trade : trades) {
        result.totalPnL += trade.pnl;

        if (trade.pnl > 0) {
            totalWin += trade.pnl;
            ++result.numWinningTrades;
        } else if (trade.pnl < 0) {
            totalLoss += std::abs(trade.pnl);
            ++result.numLosingTrades;
        }
    }

    result.totalReturn = (result.totalPnL / config_.initialCapital) * 100.0;
    result.winRate = static_cast<double>(result.numWinningTrades) / result.numTrades;
    result.avgWin = result.numWinningTrades > 0 ? totalWin / result.numWinningTrades : 0.0;
    result.avgLoss = result.numLosingTrades > 0 ? totalLoss / result.numLosingTrades : 0.0;
    result.profitFactor = result.avgLoss > 0 ? result.avgWin / result.avgLoss : 0.0;
    result.expectancy = result.totalPnL / result.numTrades;

    // Calculate Sharpe ratio and max drawdown
    std::vector<double> returns;
    for (const auto& trade : trades) {
        returns.push_back(trade.pnl / config_.initialCapital);
    }

    result.sharpeRatio = computeSharpeRatio(returns, 0.0);
    result.maxDrawdown = computeMaxDrawdown(result.equityCurve);

    return result;
}