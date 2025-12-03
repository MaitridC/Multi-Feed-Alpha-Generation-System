#include "alpha/alpha_engine.h"
#include "storage/influx_writer.h"
#include <iostream>

static InfluxWriter influx("alpha_org", "market_data", "alpha_token");

AlphaEngine::AlphaEngine(size_t windowSize, const std::string& timeframe)
    : windowSize_(windowSize),
      timeframe_(timeframe),
      sumPrices_(0.0),
      sumSquares_(0.0) {}

std::optional<AlphaSignal> AlphaEngine::onTick(const MarketTick& tick) {
    window_.push_back(tick);
    sumPrices_ += tick.price;
    sumSquares_ += tick.price * tick.price;

    if (window_.size() > windowSize_) {
        const auto& old = window_.front();
        sumPrices_ -= old.price;
        sumSquares_ -= old.price * old.price;
        window_.pop_front();
    }

    if (window_.size() < windowSize_) return std::nullopt;

    double n = static_cast<double>(window_.size());
    double sma = sumPrices_ / n;
    double meanSq = sumSquares_ / n;
    double variance = meanSq - sma * sma;
    if (variance < 0.0) variance = 0.0;
    double vol = std::sqrt(variance);

    const auto& oldest = window_.front();
    double momentum = (tick.price / oldest.price) - 1.0;
    double meanRevZ = (vol > 1e-8) ? (tick.price - sma) / vol : 0.0;

    influx.writeAlphaSignal(
        tick.symbol,
        momentum,
        meanRevZ,
        0.0,
        0.0,
        "TICK_" + timeframe_
    );

    return AlphaSignal{ tick.symbol, tick.timestamp, momentum, meanRevZ, 0.0, 0.0, "TICK_" + timeframe_ };
}

void AlphaEngine::onCandle(const Candle& c) {
    closes_.push_back(c.close);
    highs_.push_back(c.high);
    lows_.push_back(c.low);
    volumes_.push_back(c.volume);

    if (closes_.size() < windowSize_)
        return;

    double mean = 0.0, upper = 0.0, lower = 0.0;
    computeBollinger(closes_, 20, 2.0, mean, upper, lower);
    double rsi = computeRSI(closes_, 14);

    std::vector<double> upVol, downVol;
    for (size_t i = 1; i < closes_.size(); ++i) {
        if (closes_[i] > closes_[i - 1]) upVol.push_back(volumes_[i]);
        else downVol.push_back(volumes_[i]);
    }
    double vbr = computeVolumeRatio(upVol, downVol);
    double price = closes_.back();

    std::string signalType = "NONE_" + timeframe_;
    if (price < lower && rsi < 30 && vbr < 0.7) {
        signalType = "BUY_" + timeframe_;
        std::cout << "[BUY] " << timeframe_
                  << " | Price: " << price
                  << " | RSI: " << rsi
                  << " | VBR: " << vbr
                  << " | LowerBand: " << lower << std::endl;
    } else if (price > upper && rsi > 70 && vbr > 1.3) {
        signalType = "SELL_" + timeframe_;
        std::cout << "[SELL] " << timeframe_
                  << " | Price: " << price
                  << " | RSI: " << rsi
                  << " | VBR: " << vbr
                  << " | UpperBand: " << upper << std::endl;
    }

    influx.writeAlphaSignal("BTCUSDT", 0.0, 0.0, rsi, vbr, signalType);
}
