#pragma once
#include "util/market_types.h"
#include <deque>
#include <optional>

struct OFIResult {
    double imbalance;        // Net order flow imbalance
    double bidPressure;      // Buying pressure (normalized)
    double askPressure;      // Selling pressure (normalized)
    double aggression;       // Trade aggression score
    double momentum;         // Flow momentum
    long timestamp;
};

class OrderFlowImbalance {
public:
    explicit OrderFlowImbalance(size_t window = 100);

    // Update with new trade
    void onTrade(double price, double volume, bool isBuy, long timestamp);

    // Get current OFI metrics
    std::optional<OFIResult> getOFI() const;

    // Detect extreme imbalance (potential reversal or continuation)
    bool isExtremeImbalance(double threshold = 2.0) const;

private:
    size_t window_;
    std::deque<double> buyVolumes_;
    std::deque<double> sellVolumes_;
    std::deque<double> buyPrices_;
    std::deque<double> sellPrices_;
    std::deque<long> timestamps_;

    double computeImbalance() const;
    double computeAggression() const;
    double computeMomentum() const;
};

struct PressureResult {
    double bidVolume;
    double askVolume;
    double imbalanceRatio;   // (bid - ask) / (bid + ask)
    double dominantSide;     // +1 for bid dominant, -1 for ask dominant
};

class BidAskPressure {
public:
    explicit BidAskPressure(size_t window = 50);

    void onTrade(bool isBuy, double volume);
    PressureResult getPressure() const;

private:
    size_t window_;
    std::deque<double> bidVolumes_;
    std::deque<double> askVolumes_;
};

class TradeAggression {
public:
    explicit TradeAggression(size_t window = 30);

    // Update with trade size relative to average
    void onTrade(double volume, double avgVolume, bool isBuy);

    // Get aggression score (-1 to +1: negative=passive, positive=aggressive)
    double getAggression() const;

private:
    size_t window_;
    std::deque<double> aggressionScores_;
};

class VolumeDelta {
public:
    VolumeDelta();

    void onTrade(double volume, bool isBuy);

    double getCumulativeDelta() const { return cumulativeDelta_; }
    double getRecentDelta() const;  // Last N trades
    void reset();

private:
    double cumulativeDelta_;
    std::deque<double> recentDeltas_;
    static constexpr size_t RECENT_WINDOW = 50;
};

struct ToxicityScore {
    double toxicity;         // Overall toxicity [0, 1]
    double ofiComponent;     // Contribution from OFI
    double pressureComponent; // Contribution from pressure
    double aggressionComponent; // Contribution from aggression
    bool isToxic;            // Binary flag (toxicity > threshold)
};

class FlowToxicity {
public:
    explicit FlowToxicity(double toxicityThreshold = 0.7);

    // Update with all flow components
    void update(double ofi, double pressure, double aggression);

    ToxicityScore getScore() const;

private:
    double threshold_;
    double toxicity_;
    double ofiWeight_;
    double pressureWeight_;
    double aggressionWeight_;
};

struct OrderFlowSignal {
    double ofi;
    double bidPressure;
    double askPressure;
    double aggression;
    double volumeDelta;
    double toxicity;
    bool isToxicFlow;
    std::string flowDirection;  // "BUY_DOMINANT", "SELL_DOMINANT", "NEUTRAL"
    long timestamp;
};

class OrderFlowEngine {
public:
    OrderFlowEngine();

    // Process tick and compute all order flow metrics
    std::optional<OrderFlowSignal> onTick(const MarketTick& tick, bool isBuy);

    // Reset all accumulators
    void reset();

private:
    OrderFlowImbalance ofi_;
    BidAskPressure pressure_;
    TradeAggression aggression_;
    VolumeDelta volumeDelta_;
    FlowToxicity toxicity_;

    double avgVolume_;
    size_t tickCount_;

    std::string determineFlowDirection(double ofi, double pressure) const;
};