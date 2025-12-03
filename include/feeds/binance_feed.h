#pragma once

#include <string>
#include <vector>
#include <thread>
#include <functional>
#include <ixwebsocket/IXWebSocket.h>

struct MarketTick;
class AlphaEngine;
class CandleAggregator;

class BinancePublicFeed {
public:
	BinancePublicFeed(
		const std::vector<std::string>& symbols,
		AlphaEngine& engine,
		CandleAggregator& aggregator
	);

	void start();
	void stop();

	// Set callback
	void setTickCallback(std::function<void(const MarketTick&)> callback);

private:
	void connectWebSocket();
	void handleMessage(const std::string& message);

	std::vector<std::string> symbols_;
	AlphaEngine& engine_;
	CandleAggregator& aggregator_;

	std::unique_ptr<ix::WebSocket> ws_;
	std::function<void(const MarketTick&)> tickCallback_;

	bool running_;
	std::thread wsThread_;
};
