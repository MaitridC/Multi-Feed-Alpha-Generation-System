#include "backtest/pnl.h"
#include <chrono>

PnLTracker::PnLTracker(double initialCash, CostMethod method)
    : method_(method),
      initialCash_(initialCash),
      cash_(initialCash) {}

void PnLTracker::addPosition(const std::string& symbol, double quantity, double price) {
    auto it = positions_.find(symbol);

    if (it == positions_.end()) {
        // New position
        Position pos;
        pos.symbol = symbol;
        pos.quantity = quantity;
        pos.avgEntryPrice = price;
        pos.currentPrice = price;
        pos.totalCost = std::abs(quantity) * price;
        pos.unrealizedPnL = 0.0;
        pos.realizedPnL = 0.0;

        positions_[symbol] = pos;
    } else {
        // Add to existing position
        Position& pos = it->second;

        if ((pos.quantity > 0 && quantity > 0) || (pos.quantity < 0 && quantity < 0)) {
            // Adding to same side
            updatePositionCost(pos, quantity, price);
        } else {
            // Closing or flipping position
            double closeQuantity = std::min(std::abs(quantity), std::abs(pos.quantity));
            double pnl = (price - pos.avgEntryPrice) * closeQuantity *
                        (pos.quantity > 0 ? 1.0 : -1.0);

            realizedPnL_[symbol] += pnl;
            pos.realizedPnL += pnl;

            pos.quantity += quantity;
            if (std::abs(pos.quantity) < 1e-8) {
                positions_.erase(symbol);
            } else {
                pos.avgEntryPrice = price;
                pos.totalCost = std::abs(pos.quantity) * price;
            }
        }
    }

    // Update cash
    cash_ -= quantity * price;

    // Record transaction
    Transaction txn;
    txn.symbol = symbol;
    txn.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    txn.quantity = quantity;
    txn.price = price;
    txn.type = quantity > 0 ? "BUY" : "SELL";
    transactions_.push_back(txn);
}

void PnLTracker::closePosition(const std::string& symbol, double price) {
    auto it = positions_.find(symbol);
    if (it == positions_.end()) return;

    Position& pos = it->second;
    double pnl = (price - pos.avgEntryPrice) * pos.quantity;

    realizedPnL_[symbol] += pnl;
    pos.realizedPnL += pnl;

    cash_ += pos.quantity * price;

    // Record transaction
    Transaction txn;
    txn.symbol = symbol;
    txn.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    txn.quantity = -pos.quantity;
    txn.price = price;
    txn.type = "CLOSE";
    transactions_.push_back(txn);

    positions_.erase(symbol);
}

void PnLTracker::closePartialPosition(const std::string& symbol, double quantity, double price) {
    auto it = positions_.find(symbol);
    if (it == positions_.end()) return;

    Position& pos = it->second;
    double closeQty = std::min(std::abs(quantity), std::abs(pos.quantity));

    if ((pos.quantity > 0 && quantity < 0) || (pos.quantity < 0 && quantity > 0)) {
        double pnl = (price - pos.avgEntryPrice) * closeQty * (pos.quantity > 0 ? 1.0 : -1.0);
        realizedPnL_[symbol] += pnl;
        pos.realizedPnL += pnl;

        pos.quantity += quantity;
        cash_ += closeQty * price * (quantity < 0 ? -1.0 : 1.0);

        if (std::abs(pos.quantity) < 1e-8) {
            positions_.erase(symbol);
        }

        // Record transaction
        Transaction txn;
        txn.symbol = symbol;
        txn.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        txn.quantity = quantity;
        txn.price = price;
        txn.type = "PARTIAL_CLOSE";
        transactions_.push_back(txn);
    }
}

void PnLTracker::updatePrice(const std::string& symbol, double price) {
    auto it = positions_.find(symbol);
    if (it != positions_.end()) {
        Position& pos = it->second;
        pos.currentPrice = price;
        pos.unrealizedPnL = (price - pos.avgEntryPrice) * pos.quantity;
    }
}

Position PnLTracker::getPosition(const std::string& symbol) const {
    auto it = positions_.find(symbol);
    if (it != positions_.end()) {
        return it->second;
    }

    Position empty;
    empty.symbol = symbol;
    empty.quantity = 0.0;
    empty.avgEntryPrice = 0.0;
    empty.currentPrice = 0.0;
    empty.unrealizedPnL = 0.0;
    empty.realizedPnL = 0.0;
    empty.totalCost = 0.0;
    return empty;
}

std::vector<Position> PnLTracker::getAllPositions() const {
    std::vector<Position> result;
    for (const auto& pair : positions_) {
        result.push_back(pair.second);
    }
    return result;
}

bool PnLTracker::hasPosition(const std::string& symbol) const {
    return positions_.find(symbol) != positions_.end();
}

double PnLTracker::getUnrealizedPnL(const std::string& symbol) const {
    auto it = positions_.find(symbol);
    return it != positions_.end() ? it->second.unrealizedPnL : 0.0;
}

double PnLTracker::getRealizedPnL(const std::string& symbol) const {
    auto it = realizedPnL_.find(symbol);
    return it != realizedPnL_.end() ? it->second : 0.0;
}

double PnLTracker::getTotalPnL(const std::string& symbol) const {
    return getRealizedPnL(symbol) + getUnrealizedPnL(symbol);
}

PortfolioMetrics PnLTracker::getPortfolioMetrics() const {
    PortfolioMetrics metrics;
    metrics.cash = cash_;
    metrics.realizedPnL = 0.0;
    metrics.unrealizedPnL = 0.0;
    metrics.exposure = 0.0;
    metrics.numPositions = positions_.size();

    double positionsValue = 0.0;

    for (const auto& pair : positions_) {
        const Position& pos = pair.second;
        positionsValue += pos.quantity * pos.currentPrice;
        metrics.unrealizedPnL += pos.unrealizedPnL;
        metrics.exposure += std::abs(pos.quantity * pos.currentPrice);
    }

    for (const auto& pair : realizedPnL_) {
        metrics.realizedPnL += pair.second;
    }

    metrics.totalValue = cash_ + positionsValue;
    metrics.totalPnL = metrics.realizedPnL + metrics.unrealizedPnL;
    metrics.leverage = metrics.totalValue > 0 ? metrics.exposure / metrics.totalValue : 0.0;

    return metrics;
}

double PnLTracker::getTotalPortfolioPnL() const {
    PortfolioMetrics metrics = getPortfolioMetrics();
    return metrics.totalPnL;
}

void PnLTracker::reset() {
    positions_.clear();
    realizedPnL_.clear();
    transactions_.clear();
    cash_ = initialCash_;
}

std::vector<PnLTracker::Transaction> PnLTracker::getTransactionHistory() const {
    return transactions_;
}

void PnLTracker::updatePositionCost(Position& pos, double quantity, double price) {
    if (method_ == CostMethod::AVERAGE) {
        double totalQuantity = pos.quantity + quantity;
        pos.avgEntryPrice = ((pos.avgEntryPrice * std::abs(pos.quantity)) +
                            (price * std::abs(quantity))) / std::abs(totalQuantity);
        pos.quantity = totalQuantity;
        pos.totalCost = std::abs(pos.quantity) * pos.avgEntryPrice;
    } else if (method_ == CostMethod::FIFO || method_ == CostMethod::LIFO) {
        double totalQuantity = pos.quantity + quantity;
        pos.avgEntryPrice = ((pos.avgEntryPrice * std::abs(pos.quantity)) +
                            (price * std::abs(quantity))) / std::abs(totalQuantity);
        pos.quantity = totalQuantity;
        pos.totalCost = std::abs(pos.quantity) * pos.avgEntryPrice;
    }
}