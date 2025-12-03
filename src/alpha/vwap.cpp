#include "alpha/vwap.h"

VWAPCalculator::VWAPCalculator(double bandMultiplier, size_t rollingWindow)
    : bandMultiplier_(bandMultiplier),
      rollingWindow_(rollingWindow),
      vwap_(0.0),
      cumulativePV_(0.0),
      cumulativeVolume_(0.0),
      cumulativePV2_(0.0),
      isAnchored_(false) {
    anchorTime_ = std::chrono::system_clock::now();
}

void VWAPCalculator::onTick(const MarketTick& tick) {
    if (rollingWindow_ > 0) {
        tickWindow_.push_back(tick);
        if (tickWindow_.size() > rollingWindow_) {
            tickWindow_.pop_front();
        }
        updateRollingVWAP();
    } else {
        updateSessionVWAP(tick);
    }

    // Track recent prices for mean reversion
    recentPrices_.push_back(tick.price);
    if (recentPrices_.size() > 10) {
        recentPrices_.pop_front();
    }
}

void VWAPCalculator::reset() {
    vwap_ = 0.0;
    cumulativePV_ = 0.0;
    cumulativeVolume_ = 0.0;
    cumulativePV2_ = 0.0;
    tickWindow_.clear();
    recentPrices_.clear();
    isAnchored_ = false;
    anchorTime_ = std::chrono::system_clock::now();
}

void VWAPCalculator::anchor() {
    anchorTime_ = std::chrono::system_clock::now();
    isAnchored_ = true;
    cumulativePV_ = 0.0;
    cumulativeVolume_ = 0.0;
    cumulativePV2_ = 0.0;
}

VWAPMetrics VWAPCalculator::getMetrics() const {
    VWAPMetrics metrics;
    metrics.vwap = vwap_;

    double stdDev = computeStdDev();
    metrics.upperBand = vwap_ + bandMultiplier_ * stdDev;
    metrics.lowerBand = vwap_ - bandMultiplier_ * stdDev;

    double currentPrice = recentPrices_.empty() ? 0.0 : recentPrices_.back();
    metrics.deviation = getDeviationPercent(currentPrice);
    metrics.volumeAtVWAP = cumulativeVolume_;
    metrics.priceToVWAPRatio = vwap_ > 0 ? currentPrice / vwap_ : 1.0;
    metrics.priceAboveVWAP = currentPrice > vwap_;

    return metrics;
}

std::pair<double, double> VWAPCalculator::getBands() const {
    double stdDev = computeStdDev();
    return {vwap_ - bandMultiplier_ * stdDev, vwap_ + bandMultiplier_ * stdDev};
}

VWAPSignal VWAPCalculator::getSignal(double currentPrice) const {
    if (vwap_ <= 0) return VWAPSignal::NEUTRAL;

    double devPct = getDeviationPercent(currentPrice);

    if (devPct > 2.0) return VWAPSignal::STRONG_ABOVE;
    if (devPct > 0.5) return VWAPSignal::ABOVE;
    if (devPct < -2.0) return VWAPSignal::STRONG_BELOW;
    if (devPct < -0.5) return VWAPSignal::BELOW;

    return VWAPSignal::NEUTRAL;
}

double VWAPCalculator::getDeviationPercent(double currentPrice) const {
    if (vwap_ <= 0) return 0.0;
    return ((currentPrice - vwap_) / vwap_) * 100.0;
}

bool VWAPCalculator::isMeanReverting() const {
    if (recentPrices_.size() < 5) return false;

    // Check if price is moving back toward VWAP
    double firstDev = std::abs(recentPrices_[0] - vwap_);
    double lastDev = std::abs(recentPrices_.back() - vwap_);

    return lastDev < firstDev * 0.8;  // 20% reduction in deviation
}

void VWAPCalculator::updateRollingVWAP() {
    if (tickWindow_.empty()) {
        vwap_ = 0.0;
        return;
    }

    double sumPV = 0.0;
    double sumV = 0.0;
    double sumPV2 = 0.0;

    for (const auto& tick : tickWindow_) {
        sumPV += tick.price * tick.volume;
        sumV += tick.volume;
        sumPV2 += tick.price * tick.price * tick.volume;
    }

    vwap_ = sumV > 0 ? sumPV / sumV : 0.0;
    cumulativeVolume_ = sumV;
    cumulativePV_ = sumPV;
    cumulativePV2_ = sumPV2;
}

void VWAPCalculator::updateSessionVWAP(const MarketTick& tick) {
    cumulativePV_ += tick.price * tick.volume;
    cumulativeVolume_ += tick.volume;
    cumulativePV2_ += tick.price * tick.price * tick.volume;

    vwap_ = cumulativeVolume_ > 0 ? cumulativePV_ / cumulativeVolume_ : 0.0;
}

double VWAPCalculator::computeStdDev() const {
    if (cumulativeVolume_ <= 0) return 0.0;

    // Volume-weighted standard deviation
    double meanPriceSquared = cumulativePV2_ / cumulativeVolume_;
    double variance = meanPriceSquared - vwap_ * vwap_;

    if (variance < 0) variance = 0.0;

    return std::sqrt(variance);
}

namespace vwap {

double computeVWAP(const std::vector<MarketTick>& ticks) {
    if (ticks.empty()) return 0.0;

    double sumPV = 0.0;
    double sumV = 0.0;

    for (const auto& tick : ticks) {
        sumPV += tick.price * tick.volume;
        sumV += tick.volume;
    }

    return sumV > 0 ? sumPV / sumV : 0.0;
}

double computeVWAPInPeriod(
    const std::vector<MarketTick>& ticks,
    std::chrono::system_clock::time_point start,
    std::chrono::system_clock::time_point end
) {
    double sumPV = 0.0;
    double sumV = 0.0;

    long startMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        start.time_since_epoch()).count();
    long endMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        end.time_since_epoch()).count();

    for (const auto& tick : ticks) {
        if (tick.timestamp >= startMs && tick.timestamp <= endMs) {
            sumPV += tick.price * tick.volume;
            sumV += tick.volume;
        }
    }

    return sumV > 0 ? sumPV / sumV : 0.0;
}

VolumeProfile getVolumeProfile(const std::vector<MarketTick>& ticks, double vwap) {
    VolumeProfile profile{0.0, 0.0, 0.0};

    if (vwap <= 0) return profile;

    double vwapTolerance = vwap * 0.001;  // 0.1% tolerance

    for (const auto& tick : ticks) {
        if (tick.price > vwap + vwapTolerance) {
            profile.volumeAboveVWAP += tick.volume;
        } else if (tick.price < vwap - vwapTolerance) {
            profile.volumeBelowVWAP += tick.volume;
        } else {
            profile.volumeAtVWAP += tick.volume;
        }
    }

    return profile;
}

}



