#pragma once
#include <iostream>
#include <string>
#include <map>
#include <unordered_map>
#include <functional>
#include <algorithm>
#include "TradeEvent.h"

using std::map;
using std::unordered_map;
using std::string;
using std::cout;
using std::cerr;
using std::greater;
using std::less;
using std::min;

// -------------------------------------------------------
// Forward declaration
// -------------------------------------------------------
class Limit;

// -------------------------------------------------------
// LOBState — snapshot passed to bots each tick
// -------------------------------------------------------
struct LOBState {
    double bestBid  = -1.0;
    double bestAsk  = -1.0;
    double mid      = -1.0;
    double spread   = -1.0;
    double bidDepth =  0.0;   // total qty resting on bid side
    double askDepth =  0.0;   // total qty resting on ask side
    long long time  =  0;
};

// -------------------------------------------------------
// Order
// -------------------------------------------------------
class Order {
public:
    int       orderId;
    int       traderId  = -1;   // -1 = no owner (manual test orders)
    bool      isBuy;
    string    orderType;
    double    price;
    int       quantity;
    long long timestamp;

    Order* nextOrder  = nullptr;
    Order* prevOrder  = nullptr;
    Limit* parentLimit = nullptr;

    Order(int id, int trader, bool buy, string type,
          double p, int q, long long ts)
        : orderId(id), traderId(trader), isBuy(buy),
          orderType(type), price(p), quantity(q), timestamp(ts) {}
};

// -------------------------------------------------------
// Limit — a price level holding a FIFO queue of orders
// -------------------------------------------------------
class Limit {
public:
    double price;
    double totalQuantity;
    int    orderCount;
    Order* head;
    Order* tail;

    Limit(double p)
        : price(p), totalQuantity(0), orderCount(0),
          head(nullptr), tail(nullptr) {}

    bool isEmpty() const { return head == nullptr; }

    void addOrder(Order* order) {
        order->parentLimit = this;
        order->nextOrder   = nullptr;
        if (head == nullptr) {
            head = tail = order;
            order->prevOrder = nullptr;
        } else {
            order->prevOrder = tail;
            tail->nextOrder  = order;
            tail = order;
        }
        orderCount++;
        totalQuantity += order->quantity;
    }

    void removeOrder(Order* order) {
        if (order->prevOrder) order->prevOrder->nextOrder = order->nextOrder;
        else                  head = order->nextOrder;

        if (order->nextOrder) order->nextOrder->prevOrder = order->prevOrder;
        else                  tail = order->prevOrder;

        order->prevOrder  = nullptr;
        order->nextOrder  = nullptr;
        order->parentLimit = nullptr;
        orderCount--;
        totalQuantity -= order->quantity;
    }

    void print() const {
        cout << "  $" << price << " | vol=" << totalQuantity << " | [ ";
        for (Order* o = head; o; o = o->nextOrder)
            cout << o->orderId << "(q:" << o->quantity << ") ";
        cout << "]\n";
    }
};

// -------------------------------------------------------
// OrderBook
// -------------------------------------------------------
class OrderBook {
public:
    // Callback — Simulation registers this once at init
    std::function<void(const TradeEvent&)> onTrade;

    // Simulation sets this every tick so events are timestamped
    long long simTime = 0;

    map<double, Limit*, greater<double>> bidLimits;  // best bid first
    map<double, Limit*, less<double>>    askLimits;  // best ask first
    unordered_map<int, Order*>           orderIdMap;
    int matchedQuantity = 0;

    // -------------------------------------------------------
    // Core public interface
    // -------------------------------------------------------

    // Takes ownership of the Order pointer
    void addOrder(Order* order) {
        if (order->isBuy) tryMatch(order, askLimits);
        else              tryMatch(order, bidLimits);

        if (order->quantity == 0) {
            // Fully matched — engine owns and deletes it
            delete order;
            return;
        }
        // Rest the remainder
        if (order->isBuy) restOrder(order, bidLimits);
        else              restOrder(order, askLimits);
    }

    // Amend: update price/qty and re-submit (loses time priority)
    // Safe: we pull the pointer out of the map before removeOrder touches it
    void amendOrder(int orderId, double newPrice, int newQty) {
        auto it = orderIdMap.find(orderId);
        if (it == orderIdMap.end()) {
            cerr << "amendOrder: id " << orderId << " not found\n";
            return;
        }
        Order* order = it->second;

        // Remove from book structures without deleting the pointer
        removeFromBook(order);

        // Update and re-submit
        order->price    = newPrice;
        order->quantity = newQty;
        addOrder(order);   // addOrder takes ownership again
    }

    // Cancel: remove from book and free memory
    void removeOrder(int orderId) {
        auto it = orderIdMap.find(orderId);
        if (it == orderIdMap.end()) {
            cerr << "removeOrder: id " << orderId << " not found\n";
            return;
        }
        Order* order = it->second;
        removeFromBook(order);
        delete order;   // free memory — engine owns all Order*
    }

    // -------------------------------------------------------
    // Market orders
    // -------------------------------------------------------

    void placeMarketBuyOrder(int traderId, int quantity) {
        if (askLimits.empty()) { cout << "No asks available\n"; return; }

        while (quantity > 0 && !askLimits.empty()) {
            auto  it      = askLimits.begin();
            Limit* limit  = it->second;
            Order* resting = limit->head;

            while (resting && quantity > 0) {
                int    matchQty   = min(resting->quantity, quantity);
                double tradePrice = resting->price;

                resting->quantity    -= matchQty;
                limit->totalQuantity -= matchQty;
                quantity             -= matchQty;
                matchedQuantity      += matchQty;

                if (onTrade) {
                    TradeEvent evt;
                    evt.timestamp    = simTime;
                    evt.price        = tradePrice;
                    evt.quantity     = matchQty;
                    evt.buyerIsMaker = false;       // buyer is the taker here
                    evt.takerTraderId = traderId;
                    evt.takerOrderId  = -1;         // market orders have no resting id
                    evt.makerTraderId = resting->traderId;
                    evt.makerOrderId  = resting->orderId;
                    onTrade(evt);
                }

                Order* next = resting->nextOrder;
                if (resting->quantity == 0) {
                    int filledId = resting->orderId;
                    limit->removeOrder(resting);
                    orderIdMap.erase(filledId);
                    delete resting;
                    resting = next;
                } else break;
            }
            if (limit->isEmpty()) {
                askLimits.erase(it);
                delete limit;
            }
        }
    }

    void placeMarketSellOrder(int traderId, int quantity) {
        if (bidLimits.empty()) { cout << "No bids available\n"; return; }

        while (quantity > 0 && !bidLimits.empty()) {
            auto  it      = bidLimits.begin();
            Limit* limit  = it->second;
            Order* resting = limit->head;

            while (resting && quantity > 0) {
                int    matchQty   = min(resting->quantity, quantity);
                double tradePrice = resting->price;

                resting->quantity    -= matchQty;
                limit->totalQuantity -= matchQty;
                quantity             -= matchQty;
                matchedQuantity      += matchQty;

                if (onTrade) {
                    TradeEvent evt;
                    evt.timestamp    = simTime;
                    evt.price        = tradePrice;
                    evt.quantity     = matchQty;
                    evt.buyerIsMaker = true;        // buyer is the resting maker
                    evt.takerTraderId = traderId;
                    evt.takerOrderId  = -1;
                    evt.makerTraderId = resting->traderId;
                    evt.makerOrderId  = resting->orderId;
                    onTrade(evt);
                }

                Order* next = resting->nextOrder;
                if (resting->quantity == 0) {
                    int filledId = resting->orderId;
                    limit->removeOrder(resting);
                    orderIdMap.erase(filledId);
                    delete resting;
                    resting = next;
                } else break;
            }
            if (limit->isEmpty()) {
                bidLimits.erase(it);
                delete limit;
            }
        }
    }

    // -------------------------------------------------------
    // Queries
    // -------------------------------------------------------

    double getBestBid() const {
        return bidLimits.empty() ? -1.0 : bidLimits.begin()->first;
    }
    double getBestAsk() const {
        return askLimits.empty() ? -1.0 : askLimits.begin()->first;
    }
    double getSpread() const {
        if (bidLimits.empty() || askLimits.empty()) return -1.0;
        return getBestAsk() - getBestBid();
    }

    LOBState getState() const {
        LOBState s;
        s.bestBid = getBestBid();
        s.bestAsk = getBestAsk();
        s.spread  = getSpread();
        if (s.bestBid > 0 && s.bestAsk > 0)
            s.mid = (s.bestBid + s.bestAsk) / 2.0;

        for (auto& [p, lim] : bidLimits) s.bidDepth += lim->totalQuantity;
        for (auto& [p, lim] : askLimits) s.askDepth += lim->totalQuantity;
        s.time = simTime;
        return s;
    }

    void printBook() const {
        cout << "\n=== ORDER BOOK (t=" << simTime << ") ===\n";
        cout << "-- ASKS --\n";
        for (auto& [p, lim] : askLimits) lim->print();
        cout << "-- BIDS --\n";
        for (auto& [p, lim] : bidLimits) lim->print();
        cout << "Spread: " << getSpread()
             << "  Matched so far: " << matchedQuantity << "\n";
    }

private:
    // -------------------------------------------------------
    // Internal helpers
    // -------------------------------------------------------

    // Remove order from book structures only — does NOT delete the pointer.
    // Used by amendOrder so it can reuse the pointer.
    void removeFromBook(Order* order) {
        Limit* limit = order->parentLimit;
        limit->removeOrder(order);
        orderIdMap.erase(order->orderId);

        if (limit->isEmpty()) {
            if (order->isBuy) bidLimits.erase(limit->price);
            else              askLimits.erase(limit->price);
            delete limit;
        }
    }

    template<typename LimitMap>
    void restOrder(Order* order, LimitMap& side) {
        auto it = side.find(order->price);
        Limit* limit;
        if (it != side.end()) {
            limit = it->second;
        } else {
            limit = new Limit(order->price);
            side[order->price] = limit;
        }
        limit->addOrder(order);
        orderIdMap[order->orderId] = order;
    }

    template<typename LimitMap>
    void tryMatch(Order* incoming, LimitMap& opposing) {
        const bool isBuy = incoming->isBuy;
        auto it = opposing.begin();

        while (it != opposing.end() && incoming->quantity > 0) {
            double  opPrice = it->first;

            // Stop if price no longer crosses
            if (( isBuy && incoming->price < opPrice) ||
                (!isBuy && incoming->price > opPrice)) break;

            Limit* limit      = it->second;
            Order* restingPtr = limit->head;

            while (restingPtr && incoming->quantity > 0) {
                int matchQty = min(restingPtr->quantity, incoming->quantity);

                restingPtr->quantity    -= matchQty;
                limit->totalQuantity    -= matchQty;
                incoming->quantity      -= matchQty;
                matchedQuantity         += matchQty;

                // Fire callback
                if (onTrade) {
                    TradeEvent evt;
                    evt.timestamp     = simTime;
                    evt.price         = opPrice;
                    evt.quantity      = matchQty;
                    evt.buyerIsMaker  = !isBuy;
                    evt.makerTraderId = restingPtr->traderId;
                    evt.makerOrderId  = restingPtr->orderId;
                    evt.takerTraderId = incoming->traderId;
                    evt.takerOrderId  = incoming->orderId;
                    onTrade(evt);
                }

                Order* next = restingPtr->nextOrder;

                if (restingPtr->quantity == 0) {
                    int filledId = restingPtr->orderId;
                    limit->removeOrder(restingPtr);
                    orderIdMap.erase(filledId);
                    delete restingPtr;
                    restingPtr = next;

                    if (limit->isEmpty()) {
                        it = opposing.erase(it);
                        delete limit;
                        limit = nullptr;
                        break;
                    }
                } else {
                    break; // partial fill — resting order stays
                }
            }

            if (limit != nullptr) ++it;
            // if limit was erased, it already points to next element
        }
    }
};