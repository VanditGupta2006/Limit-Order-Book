#pragma once
#include <vector>
#include <unordered_map>
#include "OrderBook.h"   // gives us LOBState and TradeEvent

// Returned by act() — describes an order the bot wants to submit
struct OrderRequest {
    bool   isBuy;
    double price;      // -1.0 = market order
    int    quantity;
};

class Bot {
public:
    int    traderId;
    double cash;
    int    inventory = 0;   // positive = long, negative = short

    // orderId -> price: tracks every resting order this bot owns
    std::unordered_map<int, double> activeOrders;

    Bot(int id, double startCash) : traderId(id), cash(startCash) {}
    virtual ~Bot() = default;

    // Called every tick — return orders to submit this tick
    virtual std::vector<OrderRequest> act(const LOBState& state,
                                          long long time) = 0;

    // Called every tick — return orderIds to cancel this tick
    // Default: never cancel anything
    virtual std::vector<int> ordersToCancel(const LOBState& state,
                                             long long time) {
        return {};
    }

    // Called by Simulation when one of this bot's orders fills.
    // wasMaker=true  → this bot's resting order was hit
    // wasMaker=false → this bot sent the aggressive order
    virtual void onFill(const TradeEvent& evt, bool wasMaker) {
        bool bought = ( wasMaker && !evt.buyerIsMaker) ||
                      (!wasMaker &&  evt.buyerIsMaker);
        if (bought) {
            inventory += evt.quantity;
            cash      -= evt.price * evt.quantity;
        } else {
            inventory -= evt.quantity;
            cash      += evt.price * evt.quantity;
        }
        // Remove from active tracking if fully filled
        if (wasMaker) activeOrders.erase(evt.makerOrderId);
        else          activeOrders.erase(evt.takerOrderId);
    }
};