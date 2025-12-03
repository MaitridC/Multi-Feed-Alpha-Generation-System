#pragma once

#include <string>
#include <vector>
#include <thread>
#include <functional>
#include <memory>
#include <ixwebsocket/IXWebSocket.h>

struct MarketTick;
class AlphaEngine;
class CandleAggregator;

class CoinbaseAdvancedFeed {
public:
	CoinbaseAdvancedFeed(
		const std::vector<std::string>& productIds,
		AlphaEngine& engine,
		CandleAggregator& aggregator
	);

	void start();
	void stop();

	void setTickCallback(std::function<void(const MarketTick&)> callback);

private:
	void connectWebSocket();
	void handleMessage(const std::string& message);
	void subscribe();

	std::vector<std::string> productIds_;
	AlphaEngine& engine_;
	CandleAggregator& aggregator_;

	std::unique_ptr<ix::WebSocket> ws_;
	std::function<void(const MarketTick&)> tickCallback_;

	bool running_;
	std::thread wsThread_;
};
