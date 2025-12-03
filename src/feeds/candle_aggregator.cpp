#include "feeds/candle_aggregator.h"
#include <iostream>
#include <algorithm>
#include <chrono>

CandleAggregator::CandleAggregator(int intervalSeconds)
	: intervalSeconds(intervalSeconds), hasCandle(false) {}

void CandleAggregator::setOnCandleClosed(CandleCallback cb) {
	onCandleClosed = std::move(cb);
}

void CandleAggregator::onTick(
	double price,
	double volume,
	std::chrono::system_clock::time_point timestamp
) {
	if (!hasCandle) {
		hasCandle = true;
		candleStart = timestamp;

		currentCandle = {
			price,   // open
			price,   // high
			price,   // low
			price,   // close
			volume,  // volume
			candleStart,
			candleStart
		};
		return;
	}

	// seconds elapsed since candle start
	auto elapsed =
		std::chrono::duration_cast<std::chrono::seconds>(timestamp - candleStart)
			.count();

	// update current candle with latest tick
	currentCandle.high   = std::max(currentCandle.high, price);
	currentCandle.low    = std::min(currentCandle.low, price);
	currentCandle.close  = price;
	currentCandle.volume += volume;
	currentCandle.endTime = timestamp;

	// if candle duration exceeded, emit & reset
	if (elapsed >= intervalSeconds) {
		if (onCandleClosed) {
			std::cout << "[Candle Closed] "
					  << "O:" << currentCandle.open
					  << " H:" << currentCandle.high
					  << " L:" << currentCandle.low
					  << " C:" << currentCandle.close
					  << " V:" << currentCandle.volume
					  << std::endl;

			onCandleClosed(currentCandle);
		}

		candleStart = timestamp;
		currentCandle = {
			price,   // open
			price,   // high
			price,   // low
			price,   // close
			0.0,     // volume reset
			candleStart,
			candleStart
		};
	}
}
