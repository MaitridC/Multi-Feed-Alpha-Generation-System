#pragma once
#include <string>
#include <map>
#include <vector>

enum class CostMethod {
    FIFO,       // First In First Out
    LIFO,       // Last In First Out
    AVERAGE     // Average cost
};

struct Position {
    std::string symbol;
    double quantity;
    double avgEntryPrice;
    double currentPrice;
    double unrealizedPnL;
    double realizedPnL;
    double totalCost;
};

struct PortfolioMetrics {
    double totalValue;          // Cash + positions value
    double totalPnL;            // Realized + unrealized
    double realizedPnL;
    double unrealizedPnL;
    double cash;
    double exposure;            // Sum of |position_value|
    double leverage;            // Exposure / totalValue
    int numPositions;
};

class PnLTracker {
public:
    explicit PnLTracker(
        double initialCash = 10000.0,
        CostMethod method = CostMethod::AVERAGE
    );

    // Position management
    void addPosition(const std::string& symbol, double quantity, double price);
    void closePosition(const std::string& symbol, double price);
    void closePartialPosition(const std::string& symbol, double quantity, double price);

    // Update current prices (for unrealized P&L)
    void updatePrice(const std::string& symbol, double price);

    // Get position info
    Position getPosition(const std::string& symbol) const;
    std::vector<Position> getAllPositions() const;
    bool hasPosition(const std::string& symbol) const;

    // Get P&L metrics
    double getUnrealizedPnL(const std::string& symbol) const;
    double getRealizedPnL(const std::string& symbol) const;
    double getTotalPnL(const std::string& symbol) const;

    // Portfolio-level metrics
    PortfolioMetrics getPortfolioMetrics() const;
    double getTotalPortfolioPnL() const;
    double getCash() const { return cash_; }

    // Reset tracker
    void reset();

    // Transaction history
    struct Transaction {
        std::string symbol;
        long timestamp;
        double quantity;
        double price;
        std::string type;  // "BUY", "SELL", "CLOSE"
    };

    std::vector<Transaction> getTransactionHistory() const;

private:
    CostMethod method_;
    double initialCash_;
    double cash_;

    std::map<std::string, Position> positions_;
    std::map<std::string, double> realizedPnL_;
    std::vector<Transaction> transactions_;

    void updatePositionCost(Position& pos, double quantity, double price);
};
