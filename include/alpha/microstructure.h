#pragma once
#include "util/market_types.h"
#include <deque>
#include <vector>

enum class TradeSide {
    BUY,
    SELL,
    UNKNOWN
};

struct TradeClassification {
    TradeSide side;
    double signedVolume;  // positive = buy, negative = sell
};

struct VPINMetrics {
    double vpin;              // [0,1] - probability of informed trading
    double toxicity;          // flow toxicity score
    double buyVolume;         // aggregate buy volume
    double sellVolume;        // aggregate sell volume
    double imbalance;         // |buy - sell| / (buy + sell)
};

struct HasbrouckMetrics {
    double lambda;            // Kyle's lambda (price impact per unit volume)
    double permanentImpact;   // long-term price impact
    double transientImpact;   // temporary price impact
    double adverseSelection;  // adverse selection component
};

class MicrostructureAnalyzer {
public:
    explicit MicrostructureAnalyzer(
        size_t bucketSize = 50,
        size_t vpinWindow = 50,
        size_t impactWindow = 100
    );

    // Process new tick and update all metrics
    void onTick(const MarketTick& tick);

    // Get current VPIN metrics (flow toxicity)
    VPINMetrics getVPIN() const;

    // Get Hasbrouck price impact metrics
    HasbrouckMetrics getHasbrouckMetrics() const;

    // Classify trade as buy/sell using Lee-Ready algorithm
    TradeClassification classifyTrade(
        double price,
        double volume,
        double bidPrice = 0.0,
        double askPrice = 0.0
    ) const;

    // Get order flow imbalance over last N ticks
    double getOrderFlowImbalance(size_t window = 20) const;

    // Get volume-weighted average spread
    double getEffectiveSpread() const;

    // Reset all state
    void reset();

private:
    size_t bucketSize_;
    size_t vpinWindow_;
    size_t impactWindow_;

    // Trade history
    std::deque<MarketTick> tradeHistory_;
    std::deque<TradeClassification> classifiedTrades_;

    // VPIN calculation state
    std::deque<double> volumeBuckets_;  // alternating buy/sell buckets
    double currentBucketVolume_;
    double currentBucketBuyVolume_;
    bool isCurrentBucketBuy_;

    // Price impact state
    std::deque<double> priceChanges_;
    std::deque<double> signedVolumes_;

    // Running statistics
    double lastPrice_;
    double lastMidPrice_;
    double cumulativeVolume_;
    double cumulativeBuyVolume_;
    double cumulativeSellVolume_;

    // Helper methods
    void updateVPINBuckets(const TradeClassification& trade);
    void updatePriceImpact(double priceChange, double signedVolume);
    double computeVPIN() const;
    HasbrouckMetrics estimatePriceImpact() const;
    TradeSide inferTradeSide(double price) const;
};

namespace microstructure {
    // Calculate Volume-Weighted Average Price
    double computeVWAP(const std::vector<MarketTick>& ticks);

    // Calculate realized volatility (standard deviation of returns)
    double computeRealizedVolatility(const std::vector<double>& prices);

    // Roll measure of effective spread (Roll 1984)
    double computeRollSpread(const std::vector<double>& priceChanges);
}