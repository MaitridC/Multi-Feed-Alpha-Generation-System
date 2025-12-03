#pragma once
#include "util/market_types.h"
#include <deque>
#include <vector>
#include <string>


enum class MarketRegime {
    TRENDING_HIGH_VOL,      // Strong trend + high volatility
    TRENDING_LOW_VOL,       // Strong trend + low volatility
    MEAN_REVERTING_HIGH_VOL,// Range-bound + high volatility
    MEAN_REVERTING_LOW_VOL, // Range-bound + low volatility
    TRANSITIONING,          // Regime change in progress
    UNKNOWN                 // Not enough data
};

struct RegimeMetrics {
    MarketRegime regime;
    double hurstExponent;      // [0, 1]: <0.5 = mean-rev, >0.5 = trending
    double autocorrelation;    // [-1, 1]: persistence measure
    double volatility;         // Realized volatility
    double volRegime;          // Normalized volatility [0, 1]
    double trendStrength;      // [0, 1]: strength of trend
    double confidence;         // [0, 1]: regime classification confidence
};

struct RegimeSignalWeights {
    double momentumWeight;     // Weight for momentum signals
    double meanRevWeight;      // Weight for mean-reversion signals
    double breakoutWeight;     // Weight for breakout signals
    double volatilityAdjust;   // Volatility scaling factor
};

class RegimeDetector {
public:
    explicit RegimeDetector(
        size_t window = 100,
        size_t hurstLag = 20,
        size_t volWindow = 50
    );

    // Process new price data
    void onTick(const MarketTick& tick);
    void onCandle(const Candle& candle);

    // Get current regime
    MarketRegime getCurrentRegime() const { return currentRegime_; }

    // Get detailed regime metrics
    RegimeMetrics getMetrics() const;

    // Get adaptive signal weights for current regime
    RegimeSignalWeights getSignalWeights() const;

    // Check if regime changed recently
    bool hasRegimeChanged(size_t lookback = 5) const;

    // Get regime transition probability
    double getTransitionProbability() const;

    void reset();

private:
    size_t window_;
    size_t hurstLag_;
    size_t volWindow_;

    // Price history
    std::deque<double> prices_;
    std::deque<double> returns_;
    std::deque<double> volumes_;

    // Current state
    MarketRegime currentRegime_;
    std::deque<MarketRegime> regimeHistory_;

    // Cached metrics
    double hurstExponent_;
    double autocorrelation_;
    double volatility_;
    double trendStrength_;

    void updateMetrics();
    MarketRegime classifyRegime() const;
    double computeHurstExponent() const;
    double computeAutocorrelation(size_t lag = 1) const;
    double computeRealizedVolatility() const;
    double computeTrendStrength() const;
    double computeVolatilityRegime() const;
    RegimeSignalWeights computeSignalWeights(MarketRegime regime) const;
};

namespace regime {
    // Compute Hurst exponent using R/S analysis
    double hurstExponent(const std::vector<double>& prices, size_t maxLag = 20);

    // Compute autocorrelation at given lag
    double autocorrelation(const std::vector<double>& returns, size_t lag = 1);

    // Detect regime change using CUSUM
    bool detectRegimeChange(const std::vector<double>& returns, double threshold = 3.0);

    // Classify regime based on Hurst and volatility
    std::string regimeToString(MarketRegime regime);
}