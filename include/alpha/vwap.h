#pragma once
#include "util/market_types.h"
#include <deque>
#include <chrono>


struct VWAPMetrics {
    double vwap;                  // Current VWAP
    double upperBand;             // VWAP + N * std dev
    double lowerBand;             // VWAP - N * std dev
    double deviation;             // Current price deviation from VWAP (%)
    double volumeAtVWAP;          // Cumulative volume
    double priceToVWAPRatio;      // Price / VWAP
    bool priceAboveVWAP;          // Is current price > VWAP?
};

enum class VWAPSignal {
    STRONG_ABOVE,     // Price significantly above VWAP (bullish)
    ABOVE,            // Price moderately above VWAP
    NEUTRAL,          // Price near VWAP
    BELOW,            // Price moderately below VWAP
    STRONG_BELOW      // Price significantly below VWAP (bearish)
};

class VWAPCalculator {
public:
    explicit VWAPCalculator(
        double bandMultiplier = 2.0,
        size_t rollingWindow = 0
    );

    // Process new tick
    void onTick(const MarketTick& tick);

    // Reset VWAP (e.g., at market open)
    void reset();

    // Anchor VWAP from current point
    void anchor();

    // Get current VWAP metrics
    VWAPMetrics getMetrics() const;

    // Get current VWAP value
    double getVWAP() const { return vwap_; }

    // Get VWAP bands
    std::pair<double, double> getBands() const;

    // Get VWAP signal based on price position
    VWAPSignal getSignal(double currentPrice) const;

    // Get deviation from VWAP in percentage
    double getDeviationPercent(double currentPrice) const;

    // Check if price is mean-reverting to VWAP
    bool isMeanReverting() const;

private:
    double bandMultiplier_;
    size_t rollingWindow_;

    // VWAP calculation state
    double vwap_;
    double cumulativePV_;     // Sum of (price * volume)
    double cumulativeVolume_;

    // For standard deviation bands
    double cumulativePV2_;    // Sum of (price^2 * volume)

    // Rolling window
    std::deque<MarketTick> tickWindow_;

    // Anchor point
    std::chrono::system_clock::time_point anchorTime_;
    bool isAnchored_;

    // Price history for mean reversion detection
    std::deque<double> recentPrices_;

    void updateRollingVWAP();
    void updateSessionVWAP(const MarketTick& tick);
    double computeStdDev() const;
};

namespace vwap {
    // Compute VWAP over a vector of ticks
    double computeVWAP(const std::vector<MarketTick>& ticks);

    // Compute VWAP for specific time period
    double computeVWAPInPeriod(
        const std::vector<MarketTick>& ticks,
        std::chrono::system_clock::time_point start,
        std::chrono::system_clock::time_point end
    );

    // Get volume profile around VWAP
    struct VolumeProfile {
        double volumeAboveVWAP;
        double volumeBelowVWAP;
        double volumeAtVWAP;  // Within 0.1% of VWAP
    };

    VolumeProfile getVolumeProfile(const std::vector<MarketTick>& ticks, double vwap);
}