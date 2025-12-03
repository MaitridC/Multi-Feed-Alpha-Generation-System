#pragma once
#include <vector>

struct PerformanceMetrics {
    double sharpeRatio;
    double sortinoRatio;
    double calmarRatio;
    double maxDrawdown;
    double maxDrawdownPercent;
    double var95;              // Value at Risk (95% confidence)
    double cvar95;             // Conditional VaR (95%)
    double volatility;
    double averageReturn;
    double totalReturn;
    double winRate;
    double profitFactor;
};

// Sharpe Ratio: (mean_return - risk_free_rate) / std_dev(returns)
double computeSharpeRatio(
    const std::vector<double>& returns,
    double riskFreeRate = 0.0,
    double periodsPerYear = 252.0
);

// Sortino Ratio: (mean_return - risk_free_rate) / downside_deviation
double computeSortinoRatio(
    const std::vector<double>& returns,
    double riskFreeRate = 0.0,
    double periodsPerYear = 252.0
);

// Calmar Ratio: annual_return / max_drawdown
double computeCalmarRatio(
    const std::vector<double>& returns,
    double maxDrawdown
);

// Maximum Drawdown: largest peak-to-trough decline
double computeMaxDrawdown(const std::vector<double>& equityCurve);

// Maximum Drawdown in percentage terms
double computeMaxDrawdownPercent(const std::vector<double>& equityCurve);

// Value at Risk (VaR) at given confidence level
double computeVaR(
    const std::vector<double>& returns,
    double confidenceLevel = 0.95
);

// Conditional Value at Risk (CVaR/Expected Shortfall)
double computeCVaR(
    const std::vector<double>& returns,
    double confidenceLevel = 0.95
);

// Information Ratio: (portfolio_return - benchmark_return) / tracking_error
double computeInformationRatio(
    const std::vector<double>& portfolioReturns,
    const std::vector<double>& benchmarkReturns
);

// Win rate: % of positive returns
double computeWinRate(const std::vector<double>& returns);

// Profit factor: sum(wins) / abs(sum(losses))
double computeProfitFactor(const std::vector<double>& returns);

// Compute all metrics at once
PerformanceMetrics computeAllMetrics(
    const std::vector<double>& returns,
    const std::vector<double>& equityCurve,
    double riskFreeRate = 0.0
);

// Rolling Sharpe Ratio
std::vector<double> computeRollingSharpe(
    const std::vector<double>& returns,
    size_t window = 20,
    double riskFreeRate = 0.0
);

// Drawdown series (at each point in time)
std::vector<double> computeDrawdownSeries(const std::vector<double>& equityCurve);