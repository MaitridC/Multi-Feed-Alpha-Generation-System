#pragma once
#include "../util/market_types.h"
#include <optional>
#include <vector>
#include <deque>

class AlphaEngine {
public:
	explicit AlphaEngine(size_t windowSize = 20, const std::string& timeframe = "1m");

	// Tick-level alpha (momentum + mean-reversion)
	std::optional<AlphaSignal> onTick(const MarketTick& tick);

	// Candle-level alpha (technical indicators)
	void onCandle(const Candle& c);

private:
	size_t windowSize_;
	std::string timeframe_;

	// tick rolling window
	std::deque<MarketTick> window_;
	double sumPrices_;
	double sumSquares_;

	// candle history
	std::vector<double> closes_;
	std::vector<double> highs_;
	std::vector<double> lows_;
	std::vector<double> volumes_;
};