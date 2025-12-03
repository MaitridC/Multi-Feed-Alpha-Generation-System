#include "alpha/regime.h"
#include <algorithm>
#include <numeric>

RegimeDetector::RegimeDetector(size_t window, size_t hurstLag, size_t volWindow)
    : window_(window),
      hurstLag_(hurstLag),
      volWindow_(volWindow),
      currentRegime_(MarketRegime::UNKNOWN),
      hurstExponent_(0.5),
      autocorrelation_(0.0),
      volatility_(0.0),
      trendStrength_(0.0) {}

void RegimeDetector::onTick(const MarketTick& tick) {
    prices_.push_back(tick.price);
    volumes_.push_back(tick.volume);

    if (prices_.size() > window_) {
        prices_.pop_front();
        volumes_.pop_front();
    }

    // Calculate returns
    if (prices_.size() >= 2) {
        double ret = std::log(prices_.back() / prices_[prices_.size() - 2]);
        returns_.push_back(ret);

        if (returns_.size() > window_) {
            returns_.pop_front();
        }
    }

    // Update metrics if we have enough data
    if (prices_.size() >= hurstLag_ * 2) {
        updateMetrics();

        MarketRegime newRegime = classifyRegime();
        if (newRegime != currentRegime_) {
            currentRegime_ = newRegime;
            regimeHistory_.push_back(newRegime);

            if (regimeHistory_.size() > 50) {
                regimeHistory_.pop_front();
            }
        }
    }
}

void RegimeDetector::onCandle(const Candle& candle) {
    prices_.push_back(candle.close);
    volumes_.push_back(candle.volume);

    if (prices_.size() > window_) {
        prices_.pop_front();
        volumes_.pop_front();
    }

    if (prices_.size() >= 2) {
        double ret = std::log(prices_.back() / prices_[prices_.size() - 2]);
        returns_.push_back(ret);

        if (returns_.size() > window_) {
            returns_.pop_front();
        }
    }

    if (prices_.size() >= hurstLag_ * 2) {
        updateMetrics();
        currentRegime_ = classifyRegime();
        regimeHistory_.push_back(currentRegime_);

        if (regimeHistory_.size() > 50) {
            regimeHistory_.pop_front();
        }
    }
}

RegimeMetrics RegimeDetector::getMetrics() const {
    RegimeMetrics metrics;
    metrics.regime = currentRegime_;
    metrics.hurstExponent = hurstExponent_;
    metrics.autocorrelation = autocorrelation_;
    metrics.volatility = volatility_;
    metrics.volRegime = computeVolatilityRegime();
    metrics.trendStrength = trendStrength_;

    // Confidence based on stability of regime
    if (regimeHistory_.size() < 5) {
        metrics.confidence = 0.3;
    } else {
        // Count how many of last 5 regimes match current
        size_t matches = 0;
        for (size_t i = regimeHistory_.size() - 5; i < regimeHistory_.size(); ++i) {
            if (regimeHistory_[i] == currentRegime_) ++matches;
        }
        metrics.confidence = matches / 5.0;
    }

    return metrics;
}

RegimeSignalWeights RegimeDetector::getSignalWeights() const {
    return computeSignalWeights(currentRegime_);
}

bool RegimeDetector::hasRegimeChanged(size_t lookback) const {
    if (regimeHistory_.size() < lookback + 1) return false;

    MarketRegime recent = regimeHistory_.back();
    MarketRegime older = regimeHistory_[regimeHistory_.size() - lookback - 1];

    return recent != older;
}

double RegimeDetector::getTransitionProbability() const {
    if (regimeHistory_.size() < 10) return 0.5;

    // Count regime changes in last 10 periods
    size_t changes = 0;
    for (size_t i = regimeHistory_.size() - 10; i < regimeHistory_.size() - 1; ++i) {
        if (regimeHistory_[i] != regimeHistory_[i + 1]) {
            ++changes;
        }
    }

    return changes / 9.0;  // probability of change per period
}

void RegimeDetector::reset() {
    prices_.clear();
    returns_.clear();
    volumes_.clear();
    regimeHistory_.clear();
    currentRegime_ = MarketRegime::UNKNOWN;
    hurstExponent_ = 0.5;
    autocorrelation_ = 0.0;
    volatility_ = 0.0;
    trendStrength_ = 0.0;
}


void RegimeDetector::updateMetrics() {
    hurstExponent_ = computeHurstExponent();
    autocorrelation_ = computeAutocorrelation(1);
    volatility_ = computeRealizedVolatility();
    trendStrength_ = computeTrendStrength();
}

MarketRegime RegimeDetector::classifyRegime() const {
    double volRegime = computeVolatilityRegime();
    bool highVol = volRegime > 0.6;
    bool trending = hurstExponent_ > 0.55 || trendStrength_ > 0.6;

    if (trending && highVol) {
        return MarketRegime::TRENDING_HIGH_VOL;
    } else if (trending && !highVol) {
        return MarketRegime::TRENDING_LOW_VOL;
    } else if (!trending && highVol) {
        return MarketRegime::MEAN_REVERTING_HIGH_VOL;
    } else {
        return MarketRegime::MEAN_REVERTING_LOW_VOL;
    }
}

double RegimeDetector::computeHurstExponent() const {
    if (prices_.size() < hurstLag_ * 2) return 0.5;

    std::vector<double> priceVec(prices_.begin(), prices_.end());
    return regime::hurstExponent(priceVec, hurstLag_);
}

double RegimeDetector::computeAutocorrelation(size_t lag) const {
    if (returns_.size() < lag + 10) return 0.0;

    std::vector<double> retVec(returns_.begin(), returns_.end());
    return regime::autocorrelation(retVec, lag);
}

double RegimeDetector::computeRealizedVolatility() const {
    if (returns_.size() < 10) return 0.0;

    // Use last volWindow_ returns
    size_t start = returns_.size() > volWindow_ ? returns_.size() - volWindow_ : 0;
    double sumSq = 0.0;
    size_t n = 0;

    for (size_t i = start; i < returns_.size(); ++i) {
        sumSq += returns_[i] * returns_[i];
        ++n;
    }

    // Annualized volatility
    double variance = n > 0 ? sumSq / n : 0.0;
    return std::sqrt(variance * 252.0);  // 252 trading days
}

double RegimeDetector::computeTrendStrength() const {
    if (prices_.size() < 20) return 0.0;

    // Linear regression slope
    size_t n = std::min(size_t(50), prices_.size());
    size_t start = prices_.size() - n;

    double sumX = 0.0, sumY = 0.0, sumXY = 0.0, sumX2 = 0.0;

    for (size_t i = 0; i < n; ++i) {
        double x = static_cast<double>(i);
        double y = prices_[start + i];
        sumX += x;
        sumY += y;
        sumXY += x * y;
        sumX2 += x * x;
    }

    double slope = (n * sumXY - sumX * sumY) / (n * sumX2 - sumX * sumX);
    double avgPrice = sumY / n;

    // Normalize slope by average price to get % trend strength
    double trendPct = avgPrice > 0 ? std::abs(slope / avgPrice) * 100.0 : 0.0;

    // Map to [0, 1]
    return std::min(trendPct / 5.0, 1.0);  // 5% trend = max strength
}

double RegimeDetector::computeVolatilityRegime() const {
    if (volatility_ <= 0.0) return 0.5;

    // Historical volatility percentile
    double normalizedVol = std::min(volatility_ / 1.0, 1.0);  // 100% annualized vol = high

    return normalizedVol;
}

RegimeSignalWeights RegimeDetector::computeSignalWeights(MarketRegime regime) const {
    RegimeSignalWeights weights;

    switch (regime) {
        case MarketRegime::TRENDING_HIGH_VOL:
            weights.momentumWeight = 0.7;
            weights.meanRevWeight = 0.2;
            weights.breakoutWeight = 0.5;
            weights.volatilityAdjust = 1.5;
            break;

        case MarketRegime::TRENDING_LOW_VOL:
            weights.momentumWeight = 0.8;
            weights.meanRevWeight = 0.1;
            weights.breakoutWeight = 0.6;
            weights.volatilityAdjust = 1.0;
            break;

        case MarketRegime::MEAN_REVERTING_HIGH_VOL:
            weights.momentumWeight = 0.2;
            weights.meanRevWeight = 0.7;
            weights.breakoutWeight = 0.3;
            weights.volatilityAdjust = 1.2;
            break;

        case MarketRegime::MEAN_REVERTING_LOW_VOL:
            weights.momentumWeight = 0.3;
            weights.meanRevWeight = 0.8;
            weights.breakoutWeight = 0.4;
            weights.volatilityAdjust = 0.8;
            break;

        case MarketRegime::TRANSITIONING:
        case MarketRegime::UNKNOWN:
        default:
            weights.momentumWeight = 0.5;
            weights.meanRevWeight = 0.5;
            weights.breakoutWeight = 0.5;
            weights.volatilityAdjust = 1.0;
            break;
    }

    return weights;
}

namespace regime {

double hurstExponent(const std::vector<double>& prices, size_t maxLag) {
    if (prices.size() < maxLag * 2) return 0.5;

    size_t numPrices = prices.size();
    std::vector<double> logReturns;
    logReturns.reserve(numPrices - 1);

    for (size_t i = 1; i < numPrices; ++i) {
        if (prices[i] > 0 && prices[i-1] > 0) {
            logReturns.push_back(std::log(prices[i] / prices[i-1]));
        }
    }

    if (logReturns.size() < maxLag) return 0.5;

    // R/S analysis: compute R/S for different lags
    std::vector<double> logLags, logRS;

    for (size_t lag = 2; lag <= maxLag && lag <= logReturns.size() / 2; ++lag) {
        size_t numSegments = logReturns.size() / lag;
        double avgRS = 0.0;

        for (size_t seg = 0; seg < numSegments; ++seg) {
            size_t start = seg * lag;
            std::vector<double> segment(logReturns.begin() + start,
                                       logReturns.begin() + start + lag);

            double mean = std::accumulate(segment.begin(), segment.end(), 0.0) / lag;

            // Cumulative deviation
            std::vector<double> cumDev(lag);
            double cumSum = 0.0;
            for (size_t i = 0; i < lag; ++i) {
                cumSum += segment[i] - mean;
                cumDev[i] = cumSum;
            }

            double R = *std::max_element(cumDev.begin(), cumDev.end()) -
                      *std::min_element(cumDev.begin(), cumDev.end());

            // Standard deviation
            double variance = 0.0;
            for (double x : segment) {
                variance += (x - mean) * (x - mean);
            }
            double S = std::sqrt(variance / lag);

            if (S > 1e-10) {
                avgRS += R / S;
            }
        }

        if (numSegments > 0) {
            avgRS /= numSegments;
            logLags.push_back(std::log(static_cast<double>(lag)));
            logRS.push_back(std::log(avgRS));
        }
    }

    if (logLags.size() < 3) return 0.5;

    // Linear regression: log(R/S) = H * log(n) + c
    double numLags = static_cast<double>(logLags.size());  
    double sumX = std::accumulate(logLags.begin(), logLags.end(), 0.0);
    double sumY = std::accumulate(logRS.begin(), logRS.end(), 0.0);
    double sumXY = 0.0, sumX2 = 0.0;

    for (size_t i = 0; i < logLags.size(); ++i) {
        sumXY += logLags[i] * logRS[i];
        sumX2 += logLags[i] * logLags[i];
    }

    double H = (numLags * sumXY - sumX * sumY) / (numLags * sumX2 - sumX * sumX);  

    // Clamp to [0, 1]
    return std::min(std::max(H, 0.0), 1.0);
}

double autocorrelation(const std::vector<double>& returns, size_t lag) {
    if (returns.size() < lag + 10) return 0.0;

    double mean = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();

    double numerator = 0.0;
    double denominator = 0.0;
    size_t n = returns.size() - lag;

    for (size_t i = 0; i < n; ++i) {
        double dev1 = returns[i] - mean;
        double dev2 = returns[i + lag] - mean;
        numerator += dev1 * dev2;
    }

    for (double r : returns) {
        denominator += (r - mean) * (r - mean);
    }

    return denominator > 1e-10 ? numerator / denominator : 0.0;
}

bool detectRegimeChange(const std::vector<double>& returns, double threshold) {
    if (returns.size() < 20) return false;

    // CUSUM test for mean shift
    double mean = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();

    double cusum = 0.0;
    double maxCusum = 0.0;

    for (double r : returns) {
        cusum += r - mean;
        maxCusum = std::max(maxCusum, std::abs(cusum));
    }

    // Normalize by std dev
    double variance = 0.0;
    for (double r : returns) {
        variance += (r - mean) * (r - mean);
    }
    double stddev = std::sqrt(variance / returns.size());

    return stddev > 1e-10 && (maxCusum / stddev) > threshold;
}

std::string regimeToString(MarketRegime regime) {
    switch (regime) {
        case MarketRegime::TRENDING_HIGH_VOL: return "TRENDING_HIGH_VOL";
        case MarketRegime::TRENDING_LOW_VOL: return "TRENDING_LOW_VOL";
        case MarketRegime::MEAN_REVERTING_HIGH_VOL: return "MEAN_REV_HIGH_VOL";
        case MarketRegime::MEAN_REVERTING_LOW_VOL: return "MEAN_REV_LOW_VOL";
        case MarketRegime::TRANSITIONING: return "TRANSITIONING";
        case MarketRegime::UNKNOWN: return "UNKNOWN";
        default: return "UNKNOWN";
    }
}

}
