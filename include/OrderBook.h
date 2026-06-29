#pragma once

// ============================================================================
// OrderBook.h — Price-time priority limit order book (matching engine)
// ============================================================================
//
// Architecture:
//   Bid side: std::map<Price, Limit*, std::greater<Price>>  (best bid first)
//   Ask side: std::map<Price, Limit*, std::less<Price>>     (best ask first)
//
//   Prices are stored as integer ticks (see Types.h) to eliminate
//   floating-point comparison issues in map keys.
//
//   Each price level is a Limit object holding a FIFO doubly-linked list of
//   Order pointers.  The engine owns all Order* and Limit* memory.
//
// Matching rules:
//   - Incoming limit orders are matched against the opposing side using
//     price-time priority before any residual quantity is rested.
//   - Market orders walk the opposing book until filled or exhausted.
//   - Every fill fires the onTrade callback (set by Simulation).
//   - Self-Trade Prevention (STP): if the incoming order's traderId matches
//     the resting order's traderId, the resting order is skipped (no fill).
//
// Memory management:
//   - addOrder() takes ownership of the Order*.
//   - Fully matched orders are deleted immediately.
//   - removeOrder() deletes the cancelled order.
//   - The destructor cleans up all remaining Limit and Order objects.
// ============================================================================

#include <iostream>
#include <string>
#include <map>
#include <unordered_map>
#include <functional>
#include <algorithm>
#include "TradeEvent.h"

// Forward declaration
class Limit;

// ============================================================================
// LOBState — lightweight snapshot of the book, passed to bots each tick
// ============================================================================
struct LOBState {
    double    bestBid  = -1.0;
    double    bestAsk  = -1.0;
    double    mid      = -1.0;
    double    spread   = -1.0;
    double    bidDepth =  0.0;   // total qty resting on bid side
    double    askDepth =  0.0;   // total qty resting on ask side
    long long time     =  0;

    double bidPrices[5] = {0, 0, 0, 0, 0};
    double bidVols[5]   = {0, 0, 0, 0, 0};
    double askPrices[5] = {0, 0, 0, 0, 0};
    double askVols[5]   = {0, 0, 0, 0, 0};
};

// ============================================================================
// Order — a single order sitting in the book
// ============================================================================
class Order {
public:
    int       orderId;
    int       traderId   = -1;     // -1 = no owner (manual test orders)
    bool      isBuy;
    std::string orderType;
    Price     price;               // integer ticks (use fromTicks() to display)
    int       quantity;
    long long timestamp;

    // Doubly-linked list pointers within the parent Limit
    Order* nextOrder   = nullptr;
    Order* prevOrder   = nullptr;
    Limit* parentLimit = nullptr;

    Order(int id, int trader, bool buy, std::string type,
          Price p, int q, long long ts)
        : orderId(id), traderId(trader), isBuy(buy),
          orderType(std::move(type)), price(p), quantity(q), timestamp(ts) {}
};

// ============================================================================
// Limit — a single price level containing a FIFO queue of orders
// ============================================================================
class Limit {
public:
    Price  price;
    double totalQuantity;
    int    orderCount;
    Order* head;
    Order* tail;

    explicit Limit(Price p)
        : price(p), totalQuantity(0), orderCount(0),
          head(nullptr), tail(nullptr) {}

    bool isEmpty() const { return head == nullptr; }

    // Append order to the back of the queue (FIFO — time priority)
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

    // Unlink order from the queue (does NOT free memory)
    void removeOrder(Order* order) {
        if (order->prevOrder) order->prevOrder->nextOrder = order->nextOrder;
        else                  head = order->nextOrder;

        if (order->nextOrder) order->nextOrder->prevOrder = order->prevOrder;
        else                  tail = order->prevOrder;

        order->prevOrder   = nullptr;
        order->nextOrder   = nullptr;
        order->parentLimit = nullptr;
        orderCount--;
        totalQuantity -= order->quantity;
    }

    // Debug: print all orders at this price level
    void print() const {
        std::cout << "  $" << fromTicks(price) << " | vol=" << totalQuantity << " | [ ";
        for (Order* o = head; o; o = o->nextOrder)
            std::cout << o->orderId << "(q:" << o->quantity << ") ";
        std::cout << "]\n";
    }
};

// ============================================================================
// OrderBook — the core matching engine
// ============================================================================
class OrderBook {
public:
    // Callback — Simulation registers this once at init
    std::function<void(const TradeEvent&)> onTrade;

    // Simulation sets this every tick so events are timestamped
    long long simTime = 0;

    // Price-level maps — keyed by integer ticks for exact comparison
    std::map<Price, Limit*, std::greater<Price>> bidLimits;  // best bid first
    std::map<Price, Limit*, std::less<Price>>    askLimits;  // best ask first

    // Fast lookup: orderId → Order*
    std::unordered_map<int, Order*> orderIdMap;

    int matchedQuantity = 0;

    // Self-Trade Prevention: count of skipped self-trade matches
    int stpSkipCount = 0;

    // -------------------------------------------------------------------
    // Destructor — clean up all remaining Limit and Order objects
    // -------------------------------------------------------------------
    ~OrderBook() {
        auto cleanSide = [](auto& side) {
            for (auto& [price, limit] : side) {
                Order* curr = limit->head;
                while (curr) {
                    Order* next = curr->nextOrder;
                    delete curr;
                    curr = next;
                }
                delete limit;
            }
            side.clear();
        };
        cleanSide(bidLimits);
        cleanSide(askLimits);
        orderIdMap.clear();
    }

    // -------------------------------------------------------------------
    // addOrder — takes ownership of the Order pointer
    //
    // Attempts to match against the opposing side first.  Any residual
    // quantity is rested on the appropriate side of the book.
    // -------------------------------------------------------------------
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

    // -------------------------------------------------------------------
    // amendOrder — cancel + re-submit at new price/qty (loses time priority)
    // -------------------------------------------------------------------
    void amendOrder(int orderId, Price newPrice, int newQty) {
        auto it = orderIdMap.find(orderId);
        if (it == orderIdMap.end()) {
            std::cerr << "amendOrder: id " << orderId << " not found\n";
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

    // -------------------------------------------------------------------
    // removeOrder — cancel an order and free its memory
    // -------------------------------------------------------------------
    void removeOrder(int orderId) {
        auto it = orderIdMap.find(orderId);
        if (it == orderIdMap.end()) {
            // Silently ignore — order may have already been filled
            return;
        }
        Order* order = it->second;
        removeFromBook(order);
        delete order;
    }

    // -------------------------------------------------------------------
    // Market orders — walk the opposing book until filled or exhausted
    // Self-trade prevention: skip resting orders owned by the same trader
    // -------------------------------------------------------------------

    void placeMarketBuyOrder(int traderId, int quantity) {
        if (askLimits.empty()) return;

        auto it = askLimits.begin();
        while (quantity > 0 && it != askLimits.end()) {
            Limit* limit   = it->second;
            Order* resting = limit->head;

            while (resting && quantity > 0) {
                // STP: skip own resting orders
                if (resting->traderId == traderId) {
                    resting = resting->nextOrder;
                    stpSkipCount++;
                    continue;
                }

                int    matchQty   = std::min(resting->quantity, quantity);
                Price  tradePrice = resting->price;

                resting->quantity    -= matchQty;
                limit->totalQuantity -= matchQty;
                quantity             -= matchQty;
                matchedQuantity      += matchQty;

                if (onTrade) {
                    TradeEvent evt;
                    evt.timestamp     = simTime;
                    evt.price         = tradePrice;
                    evt.quantity      = matchQty;
                    evt.buyerIsMaker  = false;       // buyer is the taker
                    evt.takerTraderId = traderId;
                    evt.takerOrderId  = -1;           // market orders have no resting id
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
                } else {
                    break;
                }
            }
            if (limit->isEmpty()) {
                it = askLimits.erase(it);
                delete limit;
            } else {
                ++it;
            }
        }
    }

    void placeMarketSellOrder(int traderId, int quantity) {
        if (bidLimits.empty()) return;

        auto it = bidLimits.begin();
        while (quantity > 0 && it != bidLimits.end()) {
            Limit* limit   = it->second;
            Order* resting = limit->head;

            while (resting && quantity > 0) {
                // STP: skip own resting orders
                if (resting->traderId == traderId) {
                    resting = resting->nextOrder;
                    stpSkipCount++;
                    continue;
                }

                int    matchQty   = std::min(resting->quantity, quantity);
                Price  tradePrice = resting->price;

                resting->quantity    -= matchQty;
                limit->totalQuantity -= matchQty;
                quantity             -= matchQty;
                matchedQuantity      += matchQty;

                if (onTrade) {
                    TradeEvent evt;
                    evt.timestamp     = simTime;
                    evt.price         = tradePrice;
                    evt.quantity      = matchQty;
                    evt.buyerIsMaker  = true;          // buyer is the resting maker
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
                } else {
                    break;
                }
            }
            if (limit->isEmpty()) {
                it = bidLimits.erase(it);
                delete limit;
            } else {
                ++it;
            }
        }
    }

    // -------------------------------------------------------------------
    // Queries — return doubles for external consumers
    // -------------------------------------------------------------------

    double getBestBid() const {
        return bidLimits.empty() ? -1.0 : fromTicks(bidLimits.begin()->first);
    }

    double getBestAsk() const {
        return askLimits.empty() ? -1.0 : fromTicks(askLimits.begin()->first);
    }

    Price getBestBidTick() const {
        return bidLimits.empty() ? -1 : bidLimits.begin()->first;
    }

    Price getBestAskTick() const {
        return askLimits.empty() ? -1 : askLimits.begin()->first;
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

        int i = 0;
        for (auto it = bidLimits.begin(); it != bidLimits.end() && i < 5; ++it, ++i) {
            s.bidPrices[i] = fromTicks(it->first);
            s.bidVols[i]   = it->second->totalQuantity;
        }

        i = 0;
        for (auto it = askLimits.begin(); it != askLimits.end() && i < 5; ++it, ++i) {
            s.askPrices[i] = fromTicks(it->first);
            s.askVols[i]   = it->second->totalQuantity;
        }

        return s;
    }

    // Debug: print the full book state
    void printBook() const {
        std::cout << "\n=== ORDER BOOK (t=" << simTime << ") ===\n";
        std::cout << "-- ASKS --\n";
        for (auto& [p, lim] : askLimits) lim->print();
        std::cout << "-- BIDS --\n";
        for (auto& [p, lim] : bidLimits) lim->print();
        std::cout << "Spread: " << getSpread()
                  << "  Matched so far: " << matchedQuantity
                  << "  STP skips: " << stpSkipCount << "\n";
    }

private:
    // -------------------------------------------------------------------
    // removeFromBook — unlink from Limit + erase from orderIdMap
    //                  Does NOT delete the Order pointer (used by amend)
    // -------------------------------------------------------------------
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

    // -------------------------------------------------------------------
    // restOrder — place a limit order on the given side of the book
    // -------------------------------------------------------------------
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

    // -------------------------------------------------------------------
    // tryMatch — match incoming order against the opposing side
    // Self-trade prevention: skip resting orders from the same trader
    // -------------------------------------------------------------------
    template<typename LimitMap>
    void tryMatch(Order* incoming, LimitMap& opposing) {
        const bool isBuy = incoming->isBuy;
        auto it = opposing.begin();

        while (it != opposing.end() && incoming->quantity > 0) {
            Price opPrice = it->first;

            // Stop if price no longer crosses
            if (( isBuy && incoming->price < opPrice) ||
                (!isBuy && incoming->price > opPrice)) break;

            Limit* limit      = it->second;
            Order* restingPtr = limit->head;

            while (restingPtr && incoming->quantity > 0) {
                // STP: skip if same trader
                if (restingPtr->traderId == incoming->traderId) {
                    restingPtr = restingPtr->nextOrder;
                    stpSkipCount++;
                    continue;
                }

                int matchQty = std::min(restingPtr->quantity, incoming->quantity);

                restingPtr->quantity -= matchQty;
                limit->totalQuantity -= matchQty;
                incoming->quantity   -= matchQty;
                matchedQuantity      += matchQty;

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
