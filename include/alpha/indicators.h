#pragma once
#include <vector>

// Moving average and statistics
double computeMean(const std::vector<double>& data);
double computeStdDev(const std::vector<double>& data, double mean);

// Bollinger Bands
void computeBollinger(const std::vector<double>& closes, int period,
					  double mult, double& mean, double& upper, double& lower);

// RSI (Relative Strength Index)
double computeRSI(const std::vector<double>& closes, int period = 14);

// Volume ratio (UpVol / DownVol)
double computeVolumeRatio(const std::vector<double>& upVol,
						  const std::vector<double>& downVol);

// Bollinger %B - Position within bands (0 = lower, 1 = upper)
double computePercentB(double price, double lower, double upper);

// Bollinger Bandwidth - Volatility measure
double computeBandwidth(double upper, double lower, double middle);

// Detect Bollinger Squeeze (low volatility = breakout setup)
bool isBollingerSqueeze(const std::vector<double>& closes, int period = 20,
                        double mult = 2.0, double threshold = 0.05);

// Bollinger Band Breakout Direction
enum class BBBreakout {
    NONE,
    BULLISH_BREAKOUT,    // Price breaks above upper band
    BEARISH_BREAKOUT,    // Price breaks below lower band
    SQUEEZE_BULLISH,     // Squeeze with upward bias
    SQUEEZE_BEARISH      // Squeeze with downward bias
};

BBBreakout detectBollingerBreakout(
    const std::vector<double>& closes,
    int period = 20,
    double mult = 2.0
);

// Walking Bollinger Bands
struct AdaptiveBollinger {
    double upper;
    double middle;
    double lower;
    double bandwidth;
    bool isExpanding;  // True if volatility increasing
};

AdaptiveBollinger computeAdaptiveBollinger(
    const std::vector<double>& closes,
    int period = 20,
    double mult = 2.0
);

// MACD (Moving Average Convergence Divergence)
struct MACDResult {
    double macd;
    double signal;
    double histogram;
};

MACDResult computeMACD(const std::vector<double>& closes,
                       int fastPeriod = 12,
                       int slowPeriod = 26,
                       int signalPeriod = 9);

// ATR (Average True Range) - Volatility
double computeATR(const std::vector<double>& highs,
                  const std::vector<double>& lows,
                  const std::vector<double>& closes,
                  int period = 14);

// Stochastic Oscillator
struct StochasticResult {
    double k;  // %K line
    double d;  // %D line (signal)
};

StochasticResult computeStochastic(const std::vector<double>& highs,
                                   const std::vector<double>& lows,
                                   const std::vector<double>& closes,
                                   int period = 14);

// EMA (Exponential Moving Average)
double computeEMA(const std::vector<double>& data, int period);

// Volume Weighted Average Price
double computeSimpleVWAP(const std::vector<double>& prices,
                         const std::vector<double>& volumes);