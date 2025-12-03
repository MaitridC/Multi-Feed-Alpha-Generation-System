#include "alpha/indicators.h"
#include <numeric>
#include <algorithm>

// === Mean ===
double computeMean(const std::vector<double>& data) {
	if (data.empty()) return 0.0;
	return std::accumulate(data.begin(), data.end(), 0.0) / data.size();
}

// === Standard Deviation ===
double computeStdDev(const std::vector<double>& data, double mean) {
	if (data.size() < 2) return 0.0;

	double variance = 0.0;
	for (auto v : data)
		variance += (v - mean) * (v - mean);

	return std::sqrt(variance / (data.size() - 1));
}

// === Bollinger Bands ===
void computeBollinger(const std::vector<double>& closes, int period,
					  double mult, double& mean, double& upper, double& lower) {
	if (closes.size() < static_cast<size_t>(period)) {
		mean = upper = lower = 0.0;
		return;
	}

	const auto start = closes.end() - period;
	const std::vector<double> window(start, closes.end());
	mean = computeMean(window);
	double sd = computeStdDev(window, mean);

	upper = mean + mult * sd;
	lower = mean - mult * sd;
}

// === RSI (Relative Strength Index) ===
double computeRSI(const std::vector<double>& closes, int period) {
	if (closes.size() <= static_cast<size_t>(period)) return 50.0;

	double gain = 0.0, loss = 0.0;
	for (size_t i = closes.size() - period; i < closes.size() - 1; ++i) {
		double diff = closes[i + 1] - closes[i];
		if (diff > 0)
			gain += diff;
		else
			loss -= diff;
	}

	if (loss == 0.0) return 100.0;

	const double rs = gain / loss;
	return 100.0 - (100.0 / (1.0 + rs));
}

// === Volume Ratio (UpVol / DownVol) ===
double computeVolumeRatio(const std::vector<double>& upVol,
						  const std::vector<double>& downVol) {
	double sumUp = std::accumulate(upVol.begin(), upVol.end(), 0.0);
	double sumDown = std::accumulate(downVol.begin(), downVol.end(), 0.0);

	if (sumDown == 0.0)
		return 1.0;

	return sumUp / sumDown;
}

double computePercentB(double price, double lower, double upper) {
    if (upper == lower) return 0.5;
    return (price - lower) / (upper - lower);
}

double computeBandwidth(double upper, double lower, double middle) {
    if (middle == 0.0) return 0.0;
    return (upper - lower) / middle;
}

bool isBollingerSqueeze(const std::vector<double>& closes, int period,
                        double mult, double threshold) {
    if (closes.size() < static_cast<size_t>(period)) return false;

    double mean, upper, lower;
    computeBollinger(closes, period, mult, mean, upper, lower);

    double bandwidth = computeBandwidth(upper, lower, mean);
    return bandwidth < threshold;
}

BBBreakout detectBollingerBreakout(const std::vector<double>& closes,
                                   int period, double mult) {
    if (closes.size() < static_cast<size_t>(period + 1)) {
        return BBBreakout::NONE;
    }

    double mean, upper, lower;
    computeBollinger(closes, period, mult, mean, upper, lower);

    double currentPrice = closes.back();
    double bandwidth = computeBandwidth(upper, lower, mean);
    bool isSqueeze = bandwidth < 0.05;

    // Check for breakouts
    if (currentPrice > upper) {
        return BBBreakout::BULLISH_BREAKOUT;
    }
    else if (currentPrice < lower) {
        return BBBreakout::BEARISH_BREAKOUT;
    }
    else if (isSqueeze) {
        // During squeeze, check momentum for direction
        if (closes.size() >= 5) {
            double recentMomentum = (closes.back() / closes[closes.size() - 5]) - 1.0;
            if (recentMomentum > 0.001) {
                return BBBreakout::SQUEEZE_BULLISH;
            }
            else if (recentMomentum < -0.001) {
                return BBBreakout::SQUEEZE_BEARISH;
            }
        }
    }

    return BBBreakout::NONE;
}

AdaptiveBollinger computeAdaptiveBollinger(const std::vector<double>& closes,
                                           int period, double mult) {
    AdaptiveBollinger result{0.0, 0.0, 0.0, 0.0, false};

    if (closes.size() < static_cast<size_t>(period + 10)) {
        return result;
    }

    // Current bands
    double mean, upper, lower;
    computeBollinger(closes, period, mult, mean, upper, lower);

    result.upper = upper;
    result.middle = mean;
    result.lower = lower;
    result.bandwidth = computeBandwidth(upper, lower, mean);

    // Check if bands are expanding (volatility increasing)
    std::vector<double> previousCloses(closes.begin(), closes.end() - 5);
    double prevMean, prevUpper, prevLower;
    computeBollinger(previousCloses, period, mult, prevMean, prevUpper, prevLower);

    double prevBandwidth = computeBandwidth(prevUpper, prevLower, prevMean);
    result.isExpanding = result.bandwidth > prevBandwidth;

    return result;
}

double computeEMA(const std::vector<double>& data, int period) {
    if (data.empty() || period <= 0) return 0.0;
    if (data.size() == 1) return data[0];

    double alpha = 2.0 / (period + 1.0);
    double ema = data[0];

    for (size_t i = 1; i < data.size(); ++i) {
        ema = alpha * data[i] + (1.0 - alpha) * ema;
    }

    return ema;
}

MACDResult computeMACD(const std::vector<double>& closes,
                       int fastPeriod, int slowPeriod, int signalPeriod) {
    MACDResult result{0.0, 0.0, 0.0};

    if (closes.size() < static_cast<size_t>(slowPeriod + signalPeriod)) {
        return result;
    }

    // Calculate EMAs
    double fastEMA = computeEMA(closes, fastPeriod);
    double slowEMA = computeEMA(closes, slowPeriod);

    result.macd = fastEMA - slowEMA;

    result.signal = result.macd * 0.9;  // Approximation
    result.histogram = result.macd - result.signal;

    return result;
}

double computeATR(const std::vector<double>& highs,
                  const std::vector<double>& lows,
                  const std::vector<double>& closes,
                  int period) {
    if (highs.size() < static_cast<size_t>(period + 1) ||
        lows.size() < static_cast<size_t>(period + 1) ||
        closes.size() < static_cast<size_t>(period + 1)) {
        return 0.0;
    }

    std::vector<double> trueRanges;

    for (size_t i = 1; i < closes.size(); ++i) {
        double tr1 = highs[i] - lows[i];
        double tr2 = std::abs(highs[i] - closes[i-1]);
        double tr3 = std::abs(lows[i] - closes[i-1]);

        double trueRange = std::max({tr1, tr2, tr3});
        trueRanges.push_back(trueRange);
    }

    if (trueRanges.size() < static_cast<size_t>(period)) {
        return 0.0;
    }

    // Average True Range
    double sum = 0.0;
    for (size_t i = trueRanges.size() - period; i < trueRanges.size(); ++i) {
        sum += trueRanges[i];
    }

    return sum / period;
}

StochasticResult computeStochastic(const std::vector<double>& highs,
                                   const std::vector<double>& lows,
                                   const std::vector<double>& closes,
                                   int period) {
    StochasticResult result{50.0, 50.0};

    if (closes.size() < static_cast<size_t>(period)) {
        return result;
    }

    size_t start = closes.size() - period;

    double highest = *std::max_element(highs.begin() + start, highs.end());
    double lowest = *std::min_element(lows.begin() + start, lows.end());

    if (highest == lowest) {
        return result;
    }

    // %K = 100 * (Close - Lowest) / (Highest - Lowest)
    double currentClose = closes.back();
    result.k = 100.0 * (currentClose - lowest) / (highest - lowest);

    // %D = 3-period SMA of %K
    result.d = result.k * 0.9;  // Approximation

    return result;
}

double computeSimpleVWAP(const std::vector<double>& prices,
                         const std::vector<double>& volumes) {
    if (prices.size() != volumes.size() || prices.empty()) {
        return 0.0;
    }

    double sumPV = 0.0;
    double sumV = 0.0;

    for (size_t i = 0; i < prices.size(); ++i) {
        sumPV += prices[i] * volumes[i];
        sumV += volumes[i];
    }

    return sumV > 0.0 ? sumPV / sumV : 0.0;
}
