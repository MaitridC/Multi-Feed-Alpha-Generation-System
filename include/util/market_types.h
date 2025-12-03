#pragma once
#include <string>
#include <chrono>

struct MarketTick {
    std::string symbol;
    double price;
    double volume;
    long timestamp;  // milliseconds since epoch
};

struct Candle {
    double open;
    double high;
    double low;
    double close;
    double volume;
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;
};

struct Signal {
    std::string symbol;
    double momentum;
    double meanReversionZ;
    double rsi;
    double vbr;
    std::string type;  // "BUY", "SELL", "NONE"
};

struct AlphaSignal {
    std::string symbol;
    long timestamp;
    double momentum;
    double meanRevZ;
    double rsi;
    double vbr;
    std::string type;  // "BUY_1m", "SELL_5m", "NONE", etc.

    double vpin = 0.0;
    double ofi = 0.0;
    double toxicity = 0.0;

    std::string regime = "UNKNOWN";
};

struct OrderBookLevel {
    double price;
    double volume;
};

struct OrderBookSnapshot {
    std::string symbol;
    std::vector<OrderBookLevel> bids;
    std::vector<OrderBookLevel> asks;
    long timestamp;
};

enum class Side {
    BUY,
    SELL
};

enum class OrderType {
    MARKET,
    LIMIT,
    STOP,
    STOP_LIMIT
};

struct Order {
    std::string orderId;
    std::string symbol;
    Side side;
    OrderType type;
    double quantity;
    double price;  // For limit orders
    double stopPrice;  // For stop orders
    long timestamp;
    std::string status;  // "NEW", "FILLED", "CANCELLED", "REJECTED"
};

struct Fill {
    std::string orderId;
    std::string symbol;
    Side side;
    double quantity;
    double price;
    double commission;
    long timestamp;
};

struct Position;

inline long getCurrentTimestampMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

inline std::string sideToString(Side side) {
    return (side == Side::BUY) ? "BUY" : "SELL";
}

inline Side stringToSide(const std::string& s) {
    return (s == "BUY") ? Side::BUY : Side::SELL;
}