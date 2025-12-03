#pragma once

#include <string>
#include <vector>
#include <thread>
#include <functional>

class AlphaEngine;
class CandleAggregator;
struct MarketTick;

class PolygonFeed {
public:
	PolygonFeed(const std::vector<std::string>& symbols,
				const std::string& apiKey,
				AlphaEngine& engine,
				CandleAggregator& aggregator);

	void start();

	void stop();

	void setTickCallback(std::function<void(const MarketTick&)> callback);

private:
	void pollLoop();
	void fetchSymbol(const std::string& symbol);

	std::vector<std::string> symbols_;
	std::string apiKey_;
	AlphaEngine& engine_;
	CandleAggregator& aggregator_;
	bool running_;

	std::thread pollThread_;

	std::function<void(const MarketTick&)> tickCallback_;
};