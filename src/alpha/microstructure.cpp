#include "alpha/microstructure.h"
#include <algorithm>
#include <numeric>

MicrostructureAnalyzer::MicrostructureAnalyzer(
    size_t bucketSize,
    size_t vpinWindow,
    size_t impactWindow
)
    : bucketSize_(bucketSize),
      vpinWindow_(vpinWindow),
      impactWindow_(impactWindow),
      currentBucketVolume_(0.0),
      currentBucketBuyVolume_(0.0),
      isCurrentBucketBuy_(true),
      lastPrice_(0.0),
      lastMidPrice_(0.0),
      cumulativeVolume_(0.0),
      cumulativeBuyVolume_(0.0),
      cumulativeSellVolume_(0.0) {}

void MicrostructureAnalyzer::onTick(const MarketTick& tick) {
    // Classify the trade
    auto classification = classifyTrade(tick.price, tick.volume);

    // Store trade history (keep last 1000 trades)
    tradeHistory_.push_back(tick);
    if (tradeHistory_.size() > 1000) {
        tradeHistory_.pop_front();
    }

    classifiedTrades_.push_back(classification);
    if (classifiedTrades_.size() > 1000) {
        classifiedTrades_.pop_front();
    }

    // Update cumulative volumes
    cumulativeVolume_ += tick.volume;
    if (classification.side == TradeSide::BUY) {
        cumulativeBuyVolume_ += tick.volume;
    } else if (classification.side == TradeSide::SELL) {
        cumulativeSellVolume_ += tick.volume;
    }

    // Update VPIN buckets
    updateVPINBuckets(classification);

    // Update price impact metrics
    if (lastPrice_ > 0.0) {
        double priceChange = tick.price - lastPrice_;
        updatePriceImpact(priceChange, classification.signedVolume);
    }

    lastPrice_ = tick.price;
}

VPINMetrics MicrostructureAnalyzer::getVPIN() const {
    VPINMetrics metrics;
    metrics.vpin = computeVPIN();

    // Calculate recent buy/sell volume
    double recentBuy = 0.0, recentSell = 0.0;
    size_t window = std::min(classifiedTrades_.size(), size_t(50));

    for (size_t i = classifiedTrades_.size() - window; i < classifiedTrades_.size(); ++i) {
        if (classifiedTrades_[i].side == TradeSide::BUY) {
            recentBuy += classifiedTrades_[i].signedVolume;
        } else if (classifiedTrades_[i].side == TradeSide::SELL) {
            recentSell += std::abs(classifiedTrades_[i].signedVolume);
        }
    }

    metrics.buyVolume = recentBuy;
    metrics.sellVolume = recentSell;

    double totalVol = recentBuy + recentSell;
    metrics.imbalance = totalVol > 0 ? std::abs(recentBuy - recentSell) / totalVol : 0.0;

    // Toxicity score (VPIN * imbalance)
    metrics.toxicity = metrics.vpin * metrics.imbalance;

    return metrics;
}

HasbrouckMetrics MicrostructureAnalyzer::getHasbrouckMetrics() const {
    return estimatePriceImpact();
}

TradeClassification MicrostructureAnalyzer::classifyTrade(
    double price,
    double volume,
    double bidPrice,
    double askPrice
) const {
    TradeClassification result;

    // If we have bid/ask quotes, use quote rule
    if (bidPrice > 0.0 && askPrice > 0.0) {
        double midPrice = (bidPrice + askPrice) / 2.0;

        if (price > midPrice) {
            result.side = TradeSide::BUY;
            result.signedVolume = volume;
        } else if (price < midPrice) {
            result.side = TradeSide::SELL;
            result.signedVolume = -volume;
        } else {
            // At midpoint - use tick rule
            result.side = inferTradeSide(price);
            result.signedVolume = (result.side == TradeSide::BUY) ? volume : -volume;
        }
    } else {
        // No quotes available - use tick rule (Lee-Ready fallback)
        result.side = inferTradeSide(price);
        result.signedVolume = (result.side == TradeSide::BUY) ? volume : -volume;
    }

    return result;
}

double MicrostructureAnalyzer::getOrderFlowImbalance(size_t window) const {
    if (classifiedTrades_.empty()) return 0.0;

    size_t n = std::min(window, classifiedTrades_.size());
    double buyVol = 0.0, sellVol = 0.0;

    for (size_t i = classifiedTrades_.size() - n; i < classifiedTrades_.size(); ++i) {
        if (classifiedTrades_[i].side == TradeSide::BUY) {
            buyVol += classifiedTrades_[i].signedVolume;
        } else if (classifiedTrades_[i].side == TradeSide::SELL) {
            sellVol += std::abs(classifiedTrades_[i].signedVolume);
        }
    }

    double total = buyVol + sellVol;
    return total > 0 ? (buyVol - sellVol) / total : 0.0;
}

double MicrostructureAnalyzer::getEffectiveSpread() const {
    //use Roll's measure
    if (priceChanges_.size() < 2) return 0.0;

    return microstructure::computeRollSpread(
        std::vector<double>(priceChanges_.begin(), priceChanges_.end())
    );
}

void MicrostructureAnalyzer::reset() {
    tradeHistory_.clear();
    classifiedTrades_.clear();
    volumeBuckets_.clear();
    priceChanges_.clear();
    signedVolumes_.clear();

    currentBucketVolume_ = 0.0;
    currentBucketBuyVolume_ = 0.0;
    isCurrentBucketBuy_ = true;
    lastPrice_ = 0.0;
    lastMidPrice_ = 0.0;
    cumulativeVolume_ = 0.0;
    cumulativeBuyVolume_ = 0.0;
    cumulativeSellVolume_ = 0.0;
}

void MicrostructureAnalyzer::updateVPINBuckets(const TradeClassification& trade) {
    double vol = std::abs(trade.signedVolume);
    currentBucketVolume_ += vol;

    if (trade.side == TradeSide::BUY) {
        currentBucketBuyVolume_ += vol;
    }

    // Check if bucket is full
    if (currentBucketVolume_ >= bucketSize_) {
        // Calculate imbalance for this bucket
        double bucketImbalance = std::abs(2.0 * currentBucketBuyVolume_ - currentBucketVolume_);
        volumeBuckets_.push_back(bucketImbalance);

        // Keep only vpinWindow_ buckets
        if (volumeBuckets_.size() > vpinWindow_) {
            volumeBuckets_.pop_front();
        }

        // Reset bucket
        currentBucketVolume_ = 0.0;
        currentBucketBuyVolume_ = 0.0;
    }
}

void MicrostructureAnalyzer::updatePriceImpact(double priceChange, double signedVolume) {
    priceChanges_.push_back(priceChange);
    signedVolumes_.push_back(signedVolume);

    if (priceChanges_.size() > impactWindow_) {
        priceChanges_.pop_front();
        signedVolumes_.pop_front();
    }
}

double MicrostructureAnalyzer::computeVPIN() const {
    if (volumeBuckets_.size() < 2) return 0.0;

    // VPIN = Average(|BuyVolume - SellVolume|) / TotalVolume per bucket
    double sumImbalance = std::accumulate(volumeBuckets_.begin(), volumeBuckets_.end(), 0.0);
    double avgImbalance = sumImbalance / volumeBuckets_.size();

    // Normalize by bucket size
    double vpin = avgImbalance / bucketSize_;

    // Clamp to [0, 1]
    return std::min(std::max(vpin, 0.0), 1.0);
}

HasbrouckMetrics MicrostructureAnalyzer::estimatePriceImpact() const {
    HasbrouckMetrics metrics{0.0, 0.0, 0.0, 0.0};

    if (priceChanges_.size() < 10 || signedVolumes_.size() < 10) {
        return metrics;
    }

    // Kyle's Lambda: λ = Cov(ΔP, SignedVolume) / Var(SignedVolume)
    // OLS: regress price changes on signed volumes

    size_t n = priceChanges_.size();
    double meanPriceChange = std::accumulate(priceChanges_.begin(), priceChanges_.end(), 0.0) / n;
    double meanSignedVol = std::accumulate(signedVolumes_.begin(), signedVolumes_.end(), 0.0) / n;

    double covariance = 0.0;
    double variance = 0.0;

    for (size_t i = 0; i < n; ++i) {
        double dpDev = priceChanges_[i] - meanPriceChange;
        double volDev = signedVolumes_[i] - meanSignedVol;
        covariance += dpDev * volDev;
        variance += volDev * volDev;
    }

    if (variance > 1e-10) {
        metrics.lambda = covariance / variance;
    }

    // Permanent impact (assume 80% of lambda is permanent, 20% transient)
    metrics.permanentImpact = 0.8 * metrics.lambda;
    metrics.transientImpact = 0.2 * metrics.lambda;

    // Adverse selection component (approximation)
    metrics.adverseSelection = std::abs(metrics.lambda);

    return metrics;
}

TradeSide MicrostructureAnalyzer::inferTradeSide(double price) const {
    // Tick rule: if price increased -> buy, if decreased -> sell
    if (lastPrice_ <= 0.0) return TradeSide::UNKNOWN;

    if (price > lastPrice_) {
        return TradeSide::BUY;
    } else if (price < lastPrice_) {
        return TradeSide::SELL;
    } else {
        // Price unchanged - look at previous direction (zero-tick rule)
        if (classifiedTrades_.empty()) return TradeSide::UNKNOWN;
        return classifiedTrades_.back().side;
    }
}

namespace microstructure {

double computeVWAP(const std::vector<MarketTick>& ticks) {
    if (ticks.empty()) return 0.0;

    double sumPV = 0.0;
    double sumV = 0.0;

    for (const auto& tick : ticks) {
        sumPV += tick.price * tick.volume;
        sumV += tick.volume;
    }

    return sumV > 0 ? sumPV / sumV : 0.0;
}

double computeRealizedVolatility(const std::vector<double>& prices) {
    if (prices.size() < 2) return 0.0;

    std::vector<double> returns;
    returns.reserve(prices.size() - 1);

    for (size_t i = 1; i < prices.size(); ++i) {
        if (prices[i-1] > 0) {
            returns.push_back(std::log(prices[i] / prices[i-1]));
        }
    }

    if (returns.empty()) return 0.0;

    double mean = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
    double variance = 0.0;

    for (double r : returns) {
        variance += (r - mean) * (r - mean);
    }

    return std::sqrt(variance / returns.size());
}

double computeRollSpread(const std::vector<double>& priceChanges) {
    // Roll (1984): Spread = 2 * sqrt(-Cov(ΔP_t, ΔP_{t-1}))
    if (priceChanges.size() < 2) return 0.0;

    double sumProduct = 0.0;
    size_t n = 0;

    for (size_t i = 1; i < priceChanges.size(); ++i) {
        sumProduct += priceChanges[i] * priceChanges[i-1];
        ++n;
    }

    double covariance = n > 0 ? sumProduct / n : 0.0;

    // Spread is positive, so we take negative covariance
    if (covariance < 0) {
        return 2.0 * std::sqrt(-covariance);
    }

    return 0.0;
}

}