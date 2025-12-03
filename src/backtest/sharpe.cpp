#include "backtest/sharpe.h"
#include <algorithm>
#include <numeric>

static double mean(const std::vector<double>& data) {
    if (data.empty()) return 0.0;
    return std::accumulate(data.begin(), data.end(), 0.0) / data.size();
}

static double stddev(const std::vector<double>& data) {
    if (data.size() < 2) return 0.0;
    double m = mean(data);
    double variance = 0.0;
    for (auto v : data) {
        variance += (v - m) * (v - m);
    }
    return std::sqrt(variance / (data.size() - 1));
}

static double downsideDeviation(const std::vector<double>& returns) {
    if (returns.empty()) return 0.0;
    double sum = 0.0;
    int count = 0;
    for (auto r : returns) {
        if (r < 0.0) {
            sum += r * r;
            count++;
        }
    }
    return (count > 0) ? std::sqrt(sum / count) : 0.0;
}

double computeSharpeRatio(const std::vector<double>& returns,
                         double riskFreeRate,
                         double periodsPerYear) {
    if (returns.size() < 2) return 0.0;

    double meanReturn = mean(returns);
    double sd = stddev(returns);

    if (sd < 1e-10) return 0.0;

    double dailyRiskFree = riskFreeRate / periodsPerYear;
    double excessReturn = meanReturn - dailyRiskFree;

    return (excessReturn / sd) * std::sqrt(periodsPerYear);
}

double computeSortinoRatio(const std::vector<double>& returns,
                          double riskFreeRate,
                          double periodsPerYear) {
    if (returns.size() < 2) return 0.0;

    double meanReturn = mean(returns);
    double downsideDev = downsideDeviation(returns);

    if (downsideDev < 1e-10) return 0.0;

    double dailyRiskFree = riskFreeRate / periodsPerYear;
    double excessReturn = meanReturn - dailyRiskFree;

    return (excessReturn / downsideDev) * std::sqrt(periodsPerYear);
}

double computeCalmarRatio(const std::vector<double>& returns, double maxDrawdown) {
    if (returns.empty() || maxDrawdown < 1e-10) return 0.0;

    double totalReturn = std::accumulate(returns.begin(), returns.end(), 0.0);
    double annualizedReturn = (totalReturn / returns.size()) * 252.0;

    return annualizedReturn / maxDrawdown;
}

double computeMaxDrawdown(const std::vector<double>& equityCurve) {
    if (equityCurve.empty()) return 0.0;

    double peak = equityCurve[0];
    double maxDD = 0.0;

    for (auto equity : equityCurve) {
        peak = std::max(peak, equity);
        double drawdown = peak - equity;
        maxDD = std::max(maxDD, drawdown);
    }

    return maxDD;
}

double computeMaxDrawdownPercent(const std::vector<double>& equityCurve) {
    if (equityCurve.empty()) return 0.0;

    double peak = equityCurve[0];
    double maxDDPct = 0.0;

    for (auto equity : equityCurve) {
        peak = std::max(peak, equity);
        if (peak > 0.0) {
            double ddPct = (peak - equity) / peak;
            maxDDPct = std::max(maxDDPct, ddPct);
        }
    }

    return maxDDPct * 100.0;
}

double computeVaR(const std::vector<double>& returns, double confidenceLevel) {
    if (returns.empty()) return 0.0;

    std::vector<double> sortedReturns = returns;
    std::sort(sortedReturns.begin(), sortedReturns.end());

    // Find the (1 - confidence) percentile
    size_t idx = static_cast<size_t>((1.0 - confidenceLevel) * sortedReturns.size());
    idx = std::min(idx, sortedReturns.size() - 1);

    return -sortedReturns[idx];  // VaR is positive, so negate
}

double computeCVaR(const std::vector<double>& returns, double confidenceLevel) {
    if (returns.empty()) return 0.0;

    std::vector<double> sortedReturns = returns;
    std::sort(sortedReturns.begin(), sortedReturns.end());

    // Average of all returns below VaR threshold
    size_t idx = static_cast<size_t>((1.0 - confidenceLevel) * sortedReturns.size());
    idx = std::min(idx, sortedReturns.size() - 1);

    double sum = 0.0;
    for (size_t i = 0; i <= idx; ++i) {
        sum += sortedReturns[i];
    }

    return -(sum / (idx + 1));  // CVaR is positive, so negate
}

double computeInformationRatio(const std::vector<double>& portfolioReturns,
                               const std::vector<double>& benchmarkReturns) {
    if (portfolioReturns.size() != benchmarkReturns.size() ||
        portfolioReturns.size() < 2) {
        return 0.0;
    }

    // Compute tracking error (excess returns)
    std::vector<double> excessReturns;
    for (size_t i = 0; i < portfolioReturns.size(); ++i) {
        excessReturns.push_back(portfolioReturns[i] - benchmarkReturns[i]);
    }

    double meanExcess = mean(excessReturns);
    double trackingError = stddev(excessReturns);

    if (trackingError < 1e-10) return 0.0;

    return meanExcess / trackingError;
}

double computeWinRate(const std::vector<double>& returns) {
    if (returns.empty()) return 0.0;

    int wins = std::count_if(returns.begin(), returns.end(),
                             [](double r) { return r > 0; });

    return static_cast<double>(wins) / returns.size();
}

double computeProfitFactor(const std::vector<double>& returns) {
    if (returns.empty()) return 0.0;

    double sumWins = 0.0;
    double sumLosses = 0.0;

    for (auto r : returns) {
        if (r > 0) sumWins += r;
        else sumLosses += std::abs(r);
    }

    return (sumLosses > 0) ? sumWins / sumLosses : 0.0;
}

PerformanceMetrics computeAllMetrics(const std::vector<double>& returns,
                                    const std::vector<double>& equityCurve,
                                    double riskFreeRate) {
    PerformanceMetrics metrics{};

    if (returns.empty()) return metrics;

    metrics.sharpeRatio = computeSharpeRatio(returns, riskFreeRate);
    metrics.sortinoRatio = computeSortinoRatio(returns, riskFreeRate);
    metrics.maxDrawdown = computeMaxDrawdown(equityCurve);
    metrics.maxDrawdownPercent = computeMaxDrawdownPercent(equityCurve);
    metrics.calmarRatio = computeCalmarRatio(returns, metrics.maxDrawdown);
    metrics.var95 = computeVaR(returns, 0.95);
    metrics.cvar95 = computeCVaR(returns, 0.95);
    metrics.volatility = stddev(returns) * std::sqrt(252.0);
    metrics.averageReturn = mean(returns);
    metrics.totalReturn = std::accumulate(returns.begin(), returns.end(), 0.0);
    metrics.winRate = computeWinRate(returns);
    metrics.profitFactor = computeProfitFactor(returns);

    return metrics;
}

std::vector<double> computeRollingSharpe(const std::vector<double>& returns,
                                        size_t window,
                                        double riskFreeRate) {
    std::vector<double> rollingSharpe;

    if (returns.size() < window) return rollingSharpe;

    for (size_t i = window; i <= returns.size(); ++i) {
        std::vector<double> windowReturns(returns.begin() + i - window,
                                         returns.begin() + i);
        double sharpe = computeSharpeRatio(windowReturns, riskFreeRate);
        rollingSharpe.push_back(sharpe);
    }

    return rollingSharpe;
}

std::vector<double> computeDrawdownSeries(const std::vector<double>& equityCurve) {
    std::vector<double> drawdowns;

    if (equityCurve.empty()) return drawdowns;

    double peak = equityCurve[0];

    for (auto equity : equityCurve) {
        peak = std::max(peak, equity);
        double drawdown = (peak > 0) ? (peak - equity) / peak : 0.0;
        drawdowns.push_back(drawdown);
    }

    return drawdowns;
}