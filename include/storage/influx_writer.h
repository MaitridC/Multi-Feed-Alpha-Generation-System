#pragma once
#include <string>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>

class InfluxWriter {
public:
	InfluxWriter(const std::string& org,
				 const std::string& bucket,
				 const std::string& token,
				 const std::string& url = "http://localhost:8086");

	~InfluxWriter();

	void writeAlphaSignal(const std::string& symbol,
						 double momentum,
						 double meanRevZ,
						 double rsi,
						 double vbr,
						 const std::string& signalType) const;

	void writeMicrostructureSignal(const std::string& symbol,
								   double vpin,
								   double toxicity,
								   double lambda,
								   double spread,
								   long timestamp) const;

	void writeOrderFlowSignal(const std::string& symbol,
							 double ofi,
							 double bidPressure,
							 double askPressure,
							 double volumeDelta,
							 long timestamp) const;

	void writeRegimeSignal(const std::string& symbol,
						  const std::string& regime,
						  double hurstExponent,
						  double volatility,
						  double trendStrength,
						  long timestamp) const;

	void writeVWAP(const std::string& symbol,
			   double vwap,
			   double deviation,
			   long timestamp) const;

	void writeCandle(const std::string& symbol,
					 double open,
					 double high,
					 double low,
					 double close,
					 double volume,
					 long timestamp) const;

	void writePriceTick(const std::string& symbol,
						double price,
						double volume,
						long timestamp) const;

	void writeAsync(const std::string& lineProtocol);

	void flush();

private:
	std::string org_;
	std::string bucket_;
	std::string token_;
	std::string url_;

	mutable std::queue<std::string> writeQueue_;
	mutable std::mutex queueMutex_;
	std::thread writerThread_;
	std::atomic<bool> running_;

	void writerLoop();
	void writeToInflux(const std::string& data) const;
};