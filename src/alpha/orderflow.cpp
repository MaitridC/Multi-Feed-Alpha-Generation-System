#include "alpha/orderflow.h"
#include <algorithm>
#include <numeric>

OrderFlowImbalance::OrderFlowImbalance(size_t window) : window_(window) {}

void OrderFlowImbalance::onTrade(double price, double volume, bool isBuy, long timestamp) {
    if (isBuy) {
        buyVolumes_.push_back(volume);
        buyPrices_.push_back(price);
    } else {
        sellVolumes_.push_back(volume);
        sellPrices_.push_back(price);
    }
    timestamps_.push_back(timestamp);

    // Maintain window
    while (buyVolumes_.size() + sellVolumes_.size() > window_) {
        if (!buyVolumes_.empty()) {
            buyVolumes_.pop_front();
            buyPrices_.pop_front();
        }
        if (!sellVolumes_.empty()) {
            sellVolumes_.pop_front();
            sellPrices_.pop_front();
        }
        if (!timestamps_.empty()) {
            timestamps_.pop_front();
        }
    }
}

double OrderFlowImbalance::computeImbalance() const {
    double buyVol = std::accumulate(buyVolumes_.begin(), buyVolumes_.end(), 0.0);
    double sellVol = std::accumulate(sellVolumes_.begin(), sellVolumes_.end(), 0.0);
    double totalVol = buyVol + sellVol;

    if (totalVol < 1e-10) return 0.0;

    // Normalized imbalance: (Buy - Sell) / Total
    return (buyVol - sellVol) / totalVol;
}

double OrderFlowImbalance::computeAggression() const {
    std::vector<double> allVolumes;
    allVolumes.insert(allVolumes.end(), buyVolumes_.begin(), buyVolumes_.end());
    allVolumes.insert(allVolumes.end(), sellVolumes_.begin(), sellVolumes_.end());

    if (allVolumes.empty()) return 0.0;

    std::sort(allVolumes.begin(), allVolumes.end());
    double median = allVolumes[allVolumes.size() / 2];
    double threshold = median * 1.5;

    size_t largeCount = std::count_if(allVolumes.begin(), allVolumes.end(),
                                     [threshold](double v) { return v > threshold; });

    return static_cast<double>(largeCount) / allVolumes.size();
}

double OrderFlowImbalance::computeMomentum() const {
    if (timestamps_.size() < 2) return 0.0;

    size_t halfWindow = window_ / 2;

    double recentBuy = 0.0, recentSell = 0.0;
    double oldBuy = 0.0, oldSell = 0.0;

    size_t buyIdx = 0, sellIdx = 0;
    for (size_t i = 0; i < timestamps_.size(); ++i) {
        bool isRecent = (i >= halfWindow);

        if (buyIdx < buyVolumes_.size()) {
            if (isRecent) recentBuy += buyVolumes_[buyIdx];
            else oldBuy += buyVolumes_[buyIdx];
            buyIdx++;
        }
        if (sellIdx < sellVolumes_.size()) {
            if (isRecent) recentSell += sellVolumes_[sellIdx];
            else oldSell += sellVolumes_[sellIdx];
            sellIdx++;
        }
    }

    double recentImb = (recentBuy + recentSell > 0) ?
                       (recentBuy - recentSell) / (recentBuy + recentSell) : 0.0;
    double oldImb = (oldBuy + oldSell > 0) ?
                    (oldBuy - oldSell) / (oldBuy + oldSell) : 0.0;

    return recentImb - oldImb;
}

std::optional<OFIResult> OrderFlowImbalance::getOFI() const {
    if (buyVolumes_.empty() && sellVolumes_.empty()) {
        return std::nullopt;
    }

    double imbalance = computeImbalance();
    double aggression = computeAggression();
    double momentum = computeMomentum();

    double buyVol = std::accumulate(buyVolumes_.begin(), buyVolumes_.end(), 0.0);
    double sellVol = std::accumulate(sellVolumes_.begin(), sellVolumes_.end(), 0.0);
    double totalVol = buyVol + sellVol;

    double bidPressure = (totalVol > 0) ? buyVol / totalVol : 0.5;
    double askPressure = (totalVol > 0) ? sellVol / totalVol : 0.5;

    long timestamp = timestamps_.empty() ? 0 : timestamps_.back();

    return OFIResult{imbalance, bidPressure, askPressure, aggression, momentum, timestamp};
}

bool OrderFlowImbalance::isExtremeImbalance(double threshold) const {
    double imb = computeImbalance();
    return std::abs(imb) > threshold;
}

BidAskPressure::BidAskPressure(size_t window) : window_(window) {}

void BidAskPressure::onTrade(bool isBuy, double volume) {
    if (isBuy) {
        bidVolumes_.push_back(volume);
    } else {
        askVolumes_.push_back(volume);
    }

    while (bidVolumes_.size() > window_) bidVolumes_.pop_front();
    while (askVolumes_.size() > window_) askVolumes_.pop_front();
}

PressureResult BidAskPressure::getPressure() const {
    double bidVol = std::accumulate(bidVolumes_.begin(), bidVolumes_.end(), 0.0);
    double askVol = std::accumulate(askVolumes_.begin(), askVolumes_.end(), 0.0);
    double total = bidVol + askVol;

    double ratio = (total > 0) ? (bidVol - askVol) / total : 0.0;
    double dominant = (ratio > 0.1) ? 1.0 : (ratio < -0.1) ? -1.0 : 0.0;

    return PressureResult{bidVol, askVol, ratio, dominant};
}

TradeAggression::TradeAggression(size_t window) : window_(window) {}

void TradeAggression::onTrade(double volume, double avgVolume, bool isBuy) {
    // Aggression score: how much larger than average
    double score = (avgVolume > 0) ? (volume / avgVolume) - 1.0 : 0.0;

    // Sign: positive for buys, negative for sells
    score = isBuy ? score : -score;

    aggressionScores_.push_back(score);

    if (aggressionScores_.size() > window_) {
        aggressionScores_.pop_front();
    }
}

double TradeAggression::getAggression() const {
    if (aggressionScores_.empty()) return 0.0;

    double sum = std::accumulate(aggressionScores_.begin(), aggressionScores_.end(), 0.0);
    return sum / aggressionScores_.size();
}

VolumeDelta::VolumeDelta() : cumulativeDelta_(0.0) {}

void VolumeDelta::onTrade(double volume, bool isBuy) {
    double delta = isBuy ? volume : -volume;
    cumulativeDelta_ += delta;
    recentDeltas_.push_back(delta);

    if (recentDeltas_.size() > RECENT_WINDOW) {
        recentDeltas_.pop_front();
    }
}

double VolumeDelta::getRecentDelta() const {
    return std::accumulate(recentDeltas_.begin(), recentDeltas_.end(), 0.0);
}

void VolumeDelta::reset() {
    cumulativeDelta_ = 0.0;
    recentDeltas_.clear();
}

FlowToxicity::FlowToxicity(double toxicityThreshold)
    : threshold_(toxicityThreshold),
      toxicity_(0.0),
      ofiWeight_(0.4),
      pressureWeight_(0.3),
      aggressionWeight_(0.3) {}

void FlowToxicity::update(double ofi, double pressure, double aggression) {
    // Normalize components to [0, 1]
    double ofiNorm = (std::abs(ofi) + 1.0) / 2.0;
    double pressureNorm = (std::abs(pressure) + 1.0) / 2.0;
    double aggressionNorm = std::min(1.0, std::abs(aggression));

    // Weighted combination
    toxicity_ = ofiWeight_ * ofiNorm +
                pressureWeight_ * pressureNorm +
                aggressionWeight_ * aggressionNorm;
}

ToxicityScore FlowToxicity::getScore() const {
    return ToxicityScore{
        toxicity_,
        ofiWeight_ * toxicity_,
        pressureWeight_ * toxicity_,
        aggressionWeight_ * toxicity_,
        toxicity_ > threshold_
    };
}

OrderFlowEngine::OrderFlowEngine()
    : avgVolume_(0.0), tickCount_(0) {}

std::optional<OrderFlowSignal> OrderFlowEngine::onTick(const MarketTick& tick, bool isBuy) {
    tickCount_++;

    // Update running average volume
    avgVolume_ = ((tickCount_ - 1) * avgVolume_ + tick.volume) / tickCount_;

    // Update all components
    ofi_.onTrade(tick.price, tick.volume, isBuy, tick.timestamp);
    pressure_.onTrade(isBuy, tick.volume);
    aggression_.onTrade(tick.volume, avgVolume_, isBuy);
    volumeDelta_.onTrade(tick.volume, isBuy);

    auto ofiResult = ofi_.getOFI();
    if (!ofiResult) {
        return std::nullopt;
    }

    auto pressureResult = pressure_.getPressure();
    double aggrScore = aggression_.getAggression();

    // Update toxicity
    toxicity_.update(ofiResult->imbalance, pressureResult.imbalanceRatio, aggrScore);
    auto toxScore = toxicity_.getScore();

    std::string flowDir = determineFlowDirection(ofiResult->imbalance, pressureResult.imbalanceRatio);

    return OrderFlowSignal{
        ofiResult->imbalance,
        ofiResult->bidPressure,
        ofiResult->askPressure,
        aggrScore,
        volumeDelta_.getCumulativeDelta(),
        toxScore.toxicity,
        toxScore.isToxic,
        flowDir,
        tick.timestamp
    };
}

std::string OrderFlowEngine::determineFlowDirection(double ofi, double pressure) const {
    double combined = (ofi + pressure) / 2.0;

    if (combined > 0.2) return "BUY_DOMINANT";
    if (combined < -0.2) return "SELL_DOMINANT";
    return "NEUTRAL";
}

void OrderFlowEngine::reset() {
    volumeDelta_.reset();
    avgVolume_ = 0.0;
    tickCount_ = 0;
}