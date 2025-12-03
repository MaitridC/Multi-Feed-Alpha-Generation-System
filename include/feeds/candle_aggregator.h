#pragma once
#include "util/market_types.h"
#include <functional>
#include <chrono>

class CandleAggregator {
public:
	using CandleCallback = std::function<void(const Candle&)>;

	explicit CandleAggregator(int intervalSeconds);

	// Called for every market tick
	void onTick(double price,
				double volume,
				std::chrono::system_clock::time_point timestamp);

	// Register callback when a candle completes
	void setOnCandleClosed(CandleCallback cb);

private:
	int intervalSeconds;
	bool hasCandle;
	std::chrono::system_clock::time_point candleStart;
	Candle currentCandle;
	CandleCallback onCandleClosed;
};
