#include <iostream>
#include <string>
#include <map>
#include <unordered_map>
#include <limits>
#include <algorithm>

using namespace std;
/*TOFIX : When market orders are placed, they are placed by just placing an order with price = max or min long, and then trying to match it. 
This causes extra messages to be printed about the market order being partially filled at its price, which is confusing. In a real implementation, market orders would likely be handled as a special case within the matching logic to avoid this.
Adress this when implimenting the bots.

- Add Text promts for ammends
*/
// Forward declaration so Order knows Limit exists
class Limit;

class Order {
public:
    int orderId;
    bool isBuy;
    string orderType;
    double price;
    int quantity;
    long long timestamp;

    // DLL pointers
    Order* nextOrder = nullptr;
    Order* prevOrder = nullptr;
    Limit* parentLimit = nullptr;

    Order(int id, bool buy, string type, double p, int q, long long ts)
    {
        orderId = id;
        isBuy = buy;
        orderType = type;
        price = p;
        quantity = q;
        timestamp = ts;
    }

    string getReadableTime() const {
        long long hours = timestamp / 3600;
        long long minutes = (timestamp % 3600) / 60;
        long long seconds = timestamp % 60;
        char buffer[9];
        snprintf(buffer, sizeof(buffer), "%02lld:%02lld:%02lld", hours, minutes, seconds);
        return string(buffer);
    }
};


class Limit {
public:
    double price;
    double totalQuantity;
    int orderCount;

    Order* head;
    Order* tail;

    Limit(double p) : price(p), totalQuantity(0), orderCount(0), head(nullptr), tail(nullptr) {}

    bool isEmpty() const { return head == nullptr; }

    // O(1) add to back of queue (time priority)
    void addOrder(Order* order) {
        order->parentLimit = this;
        order->nextOrder = nullptr;

        if (head == nullptr) {
            head = order;
            tail = order;
            order->prevOrder = nullptr;
        } else {
            order->prevOrder = tail;
            tail->nextOrder = order;
            tail = order;
        }

        orderCount++;
        totalQuantity += order->quantity;
    }

    // O(1) remove — caller already has the pointer
    void removeOrder(Order* order) {
        if (order->prevOrder != nullptr) {
            order->prevOrder->nextOrder = order->nextOrder;
        } else {
            head = order->nextOrder;
        }

        if (order->nextOrder != nullptr) {
            order->nextOrder->prevOrder = order->prevOrder;
        } else {
            tail = order->prevOrder;
        }

        order->prevOrder = nullptr;
        order->nextOrder = nullptr;
        order->parentLimit = nullptr;

        orderCount--;
        totalQuantity -= order->quantity;
    }

    void printOrderRecords() const {
        Order* current = head;
        cout << "Limit Price: " << price << " | Total Vol: " << totalQuantity << " -> Queue: [ ";
        while (current != nullptr) {
            cout << current->orderId << "(Q:" << current->quantity << ") ";
            current = current->nextOrder;
        }
        cout << "]\n";
    }
};


class OrderBook {
public:
    // Best bid at front (highest first), best ask at front (lowest first)
    map<double, Limit*, greater<double>> bidLimits;
    map<double, Limit*, less<double>>    askLimits;

    // O(1) lookup by orderId for amend/cancel
    unordered_map<int, Order*> orderIdMap;

    int matchedQuantity = 0;

    // -------------------------------------------------------
    // Public interface
    // -------------------------------------------------------

    void addOrder(Order* order) {
        if (order->isBuy) {
            // Try to match against existing asks before resting
            tryMatch(order, askLimits);
        } else {
            tryMatch(order, bidLimits);
        }

        // If fully filled, nothing left to rest
        if (order->quantity == 0) {
            return;
        }

        // Rest the remaining quantity at its price level
        if(order->isBuy) {
            restOrder(order, bidLimits);
        } else {    
            restOrder(order, askLimits);
        }
    }

    // Amend = cancel + re-add (loses time priority, same as reference impl)
    void amendOrder(int orderId, double newPrice, int newQuantity) {
        if (!orderIdMap.count(orderId)) {
            cerr << "amendOrder: order id " << orderId << " not found\n";
            return;
        }
        Order* order = orderIdMap[orderId];
        order->price    = newPrice;
        order->quantity = newQuantity;

        removeOrder(orderId);
        addOrder(order);
    }

    void removeOrder(int orderId) {
        if (!orderIdMap.count(orderId)) {
            cerr << "removeOrder: order id " << orderId << " not found\n";
            return;
        }

        Order* order = orderIdMap[orderId];
        Limit* limit = order->parentLimit;

        limit->removeOrder(order);
        orderIdMap.erase(orderId);

        // Clean up empty price levels (mirrors reference impl)
        if (limit->isEmpty()) {
            if (order->isBuy) {
                bidLimits.erase(limit->price);
            } else {
                askLimits.erase(limit->price);
            }
            delete limit;
        }
    }

    // -------------------------------------------------------
    // Market orders
    // -------------------------------------------------------

    void placeMarketBuyOrder(int quantity) {
        if (askLimits.empty()) {
            cout << "No asks available to fill market buy order\n";
            return;
        }
        // Price max so it crosses every ask
        Order* marketOrder = new Order{-1, true, "market",
                                       numeric_limits<double>::max(), quantity, 0};
        tryMatch(marketOrder, askLimits);
        if (marketOrder->quantity > 0) {
            cout << "Market buy partially filled, " << marketOrder->quantity << " units remaining\n";
        } else {
            cout << "Market buy completely filled\n";
        }
        delete marketOrder;
    }

    void placeMarketSellOrder(int quantity) {
        if (bidLimits.empty()) {
            cout << "No bids available to fill market sell order\n";
            return;
        }
        Order* marketOrder = new Order{-1, false, "market", // doing this causes addition confusing output message as order it -1;
                                       numeric_limits<double>::lowest(), quantity, 0};
        tryMatch(marketOrder, bidLimits);
        if (marketOrder->quantity > 0) {
            cout << "Market sell partially filled, " << marketOrder->quantity << " units remaining\n";
        } else {
            cout << "Market sell completely filled\n";
        }
        delete marketOrder;
    }

    // -------------------------------------------------------
    // Helpers / display
    // -------------------------------------------------------

    double getBestBidPrice() const {
        return bidLimits.empty() ? -1.0 : bidLimits.begin()->first;
    }

    double getBestAskPrice() const {
        return askLimits.empty() ? -1.0 : askLimits.begin()->first;
    }

    double getSpread() const {
        if (bidLimits.empty() || askLimits.empty()) return -1.0;
        return getBestAskPrice() - getBestBidPrice();
    }

    void printBook() const {
        cout << "\n=== ORDER BOOK ===\n";
        cout << "-- ASKS (lowest first) --\n";
        for (auto it = askLimits.begin(); it != askLimits.end(); ++it) {
            it->second->printOrderRecords();
        }
        cout << "-- BIDS (highest first) --\n";
        for (auto& [price, limit] : bidLimits) {
            limit->printOrderRecords();
        }
        cout << "Spread: " << getSpread() << "\n";
        cout << "Total matched qty so far: " << matchedQuantity << "\n\n";
    }

private:
    // -------------------------------------------------------
    // Internal helpers
    // -------------------------------------------------------

    // Place a resting order into the correct limit level
    template<typename LimitMap>
    void restOrder(Order* order, LimitMap& limitSide) {
        auto it = limitSide.find(order->price);
        Limit* limit;
        if (it != limitSide.end()) {
            limit = it->second;
        } else {
            limit = new Limit(order->price);
            limitSide[order->price] = limit;
        }
        limit->addOrder(order);
        orderIdMap[order->orderId] = order;
    }

    // Match incoming order against the opposing side
    // Mirrors OrderBook<T>::TryMatch from the reference implementation
    template<typename LimitMap>
    void tryMatch(Order* incoming, LimitMap& opposingLimits) {
        const bool isBuy = incoming->isBuy;
        auto opposingIter = opposingLimits.begin();

        while (opposingIter != opposingLimits.end() && incoming->quantity > 0) {
            double opposingPrice = opposingIter->first;

            // Price crossing check — same logic as the reference impl
            if ((isBuy  && incoming->price < opposingPrice) ||
                (!isBuy && incoming->price > opposingPrice)) {
                break;
            }

            Limit* limit = opposingIter->second;
            Order* restingPtr = limit->head;

            while (restingPtr != nullptr && incoming->quantity > 0) {
                int matchedQty = min(restingPtr->quantity, incoming->quantity);

                restingPtr->quantity -= matchedQty;
                limit->totalQuantity -= matchedQty;
                incoming->quantity   -= matchedQty;
                matchedQuantity      += matchedQty;

                cout << (isBuy ? "BUY" : "SELL") << " order " << incoming->orderId
                     << (matchedQty < incoming->quantity + matchedQty ? " partially" : "")
                     << " filled @ " << opposingPrice << "\n";
                cout << (isBuy ? "SELL" : "BUY") << " order " << restingPtr->orderId
                     << (restingPtr->quantity == 0 ? "" : " partially")
                     << " filled @ " << opposingPrice << "\n";

                if (restingPtr->quantity == 0) {
                    Order* next = restingPtr->nextOrder;
                    int filledId = restingPtr->orderId;

                    limit->removeOrder(restingPtr);
                    orderIdMap.erase(filledId);

                    if (limit->isEmpty()) {
                        opposingIter = opposingLimits.erase(opposingIter);
                        delete limit;
                        limit = nullptr;
                        restingPtr = next; // next is now dangling from erased limit, but we break below
                        break;             // limit gone — move to next price level
                    }

                    restingPtr = next;
                } else {
                    break; // resting order partially filled — stays at front
                }
            }

            if (limit != nullptr) {
                ++opposingIter; // limit still exists, advance normally
            }
            // if limit was erased, opposingIter already points to the next element
        }
    }
};


// -------------------------------------------------------
// Quick smoke test
// -------------------------------------------------------
/*
int main() {
    OrderBook book;

    // Resting asks
    book.addOrder(new Order{1, false, "limit", 102.0, 10, 1000});
    book.addOrder(new Order{2, false, "limit", 103.0, 5,  1001});
    book.addOrder(new Order{3, false, "limit", 101.0, 8,  1002});

    // Resting bids
    book.addOrder(new Order{4, true, "limit", 99.0,  6, 1003});
    book.addOrder(new Order{5, true, "limit", 98.0,  4, 1004});

    book.printBook();

    // Aggressive buy that crosses ask at 101
    cout << "-- Adding aggressive buy @ 101.5 qty 5 --\n";
    book.addOrder(new Order{6, true, "limit", 101.5, 5, 1005});
    book.printBook();

    // Market buy
    cout << "-- Market buy qty 6 --\n";
    book.placeMarketBuyOrder(6);                
    book.printBook();

    // Amend order 4 (bid) to a higher price
    cout << "-- Amending order 4 to price 100 qty 10 --\n"; // ratther than changing that same order create a new order; No this would cause problem in cancelling
    book.amendOrder(4, 100.0, 10);
    book.printBook();

    // Cancel order 5
    cout << "-- Cancelling order 5 --\n";
    book.removeOrder(5);
    book.printBook();

    return 0;
}
*/
// -------------------------------------------------------
// EDGE CASE TESTS
// Replace the main() in OrderBook.cpp with this
// -------------------------------------------------------


//Test 2 (testing edge cases)
/*
int main() {

    // -------------------------------------------------------
    // TEST 1: Partial fill label bug probe
    // Incoming buy qty 10, resting ask qty 4 then qty 4
    // After first match: matchedQty=4, incoming->quantity=6
    // condition: 4 < 6+4=10 → true → prints "partially" ✓ (correct)
    // After second match: matchedQty=4, incoming->quantity=2
    // condition: 4 < 2+4=6 → true → prints "partially" ✓ (correct)
    // Remaining qty 2 rests. No bug here.
    // But try: incoming qty 4 vs resting qty 4 (exact fill)
    // matchedQty=4, incoming->quantity=0
    // condition: 4 < 0+4=4 → FALSE → prints "filled" ✓ (correct)
    // -------------------------------------------------------
    cout << "=== TEST 1: Exact fill — should print 'filled' not 'partially filled' ===\n";
    {
        OrderBook book;
        book.addOrder(new Order{1, false, "limit", 100.0, 4, 1000}); // ask qty 4
        book.addOrder(new Order{2, true,  "limit", 100.0, 4, 1001}); // buy qty 4 — exact match
        book.printBook(); // should be empty book, matched qty = 4
    }

    // -------------------------------------------------------
    // TEST 2: Incoming order sweeps MULTIPLE levels fully
    // Tests iterator management when limits are erased mid-loop
    // -------------------------------------------------------
    cout << "=== TEST 2: Sweep multiple full levels ===\n";
    {
        OrderBook book;
        book.addOrder(new Order{1, false, "limit", 100.0, 3, 1000});
        book.addOrder(new Order{2, false, "limit", 101.0, 3, 1001});
        book.addOrder(new Order{3, false, "limit", 102.0, 3, 1002});
        // Buy that sweeps all 3 levels exactly
        book.addOrder(new Order{4, true, "limit", 102.0, 9, 1003});
        book.printBook(); // should be completely empty, matched = 9
    }
    // -------------------------------------------------------
    // TEST 3: Incoming order sweeps multiple levels with remainder
    // Tests that leftover quantity rests correctly after sweep
    // -------------------------------------------------------
    cout << "=== TEST 3: Sweep + rest remainder ===\n";
    {
        OrderBook book;
        book.addOrder(new Order{1, false, "limit", 100.0, 3, 1000});
        book.addOrder(new Order{2, false, "limit", 101.0, 3, 1001});
        // Buy qty 10 sweeps both asks (total 6), rests 4 at 102
        book.addOrder(new Order{3, true, "limit", 102.0, 10, 1002});
        book.printBook(); // asks empty, bid @ 102 qty 4
    }

    // -------------------------------------------------------
    // TEST 4: Multiple orders at the same price level
    // Tests time priority — first in should be first matched
    // -------------------------------------------------------
    cout << "=== TEST 4: Time priority within same level ===\n";
    {
        OrderBook book;
        book.addOrder(new Order{1, false, "limit", 100.0, 3, 1000}); // first in queue
        book.addOrder(new Order{2, false, "limit", 100.0, 3, 1001}); // second
        book.addOrder(new Order{3, false, "limit", 100.0, 3, 1002}); // third
        book.printBook();
        // Buy qty 5 — should fill order 1 fully (3), order 2 partially (2)
        book.addOrder(new Order{4, true, "limit", 100.0, 5, 1003});
        book.printBook(); // order 1 gone, order 2 qty 1 remaining, order 3 intact
    }

    // -------------------------------------------------------
    // TEST 5: Amend into a crossing price (should trigger match)
    // Order 1 is a bid @ 98. Amend it to 102 which crosses ask @ 100
    // -------------------------------------------------------
    cout << "=== TEST 5: Amend triggers match ===\n";
    {
        OrderBook book;
        book.addOrder(new Order{1, false, "limit", 100.0, 5, 1000}); // ask @ 100
        book.addOrder(new Order{2, true,  "limit",  98.0, 5, 1001}); // bid @ 98, no match
        book.printBook();
        // Amend bid to 102 — should now cross the ask @ 100
        book.amendOrder(2, 102.0, 5);
        book.printBook(); // both orders should be gone, matched = 5
    }

    // -------------------------------------------------------
    // TEST 6: Market buy against empty book
    // -------------------------------------------------------
    cout << "=== TEST 6: Market buy on empty ask side ===\n";
    {
        OrderBook book;
        book.addOrder(new Order{1, true, "limit", 99.0, 5, 1000}); // only bids, no asks
        book.placeMarketBuyOrder(5); // should print "No asks available"
        book.printBook();
    }

    // -------------------------------------------------------
    // TEST 7: Market buy partially fills (not enough liquidity)
    // -------------------------------------------------------
    cout << "=== TEST 7: Market buy exceeds available liquidity ===\n";
    {
        OrderBook book;
        book.addOrder(new Order{1, false, "limit", 100.0, 3, 1000}); // only 3 available
        book.placeMarketBuyOrder(10); // should partially fill 3, report 7 remaining
        book.printBook(); // ask side empty
    }

    // -------------------------------------------------------
    // TEST 8: Cancel the only order at a level — level should be cleaned up
    // -------------------------------------------------------
    cout << "=== TEST 8: Cancel cleans up empty limit level ===\n";
    {
        OrderBook book;
        book.addOrder(new Order{1, false, "limit", 100.0, 5, 1000});
        book.addOrder(new Order{2, false, "limit", 101.0, 5, 1001});
        book.removeOrder(1);
        book.printBook(); // only 101 level should remain
    }

    // -------------------------------------------------------
    // TEST 9: Amend non-existent order — should not crash
    // -------------------------------------------------------
    cout << "=== TEST 9: Amend non-existent order ===\n";
    {
        OrderBook book;
        book.amendOrder(999, 100.0, 5); // should print error, not crash
    }

    // -------------------------------------------------------
    // TEST 10: Two orders same price, cancel first, second should still match
    // Tests DLL pointer integrity after removal from middle/head
    // -------------------------------------------------------
    cout << "=== TEST 10: Cancel head order, remaining order still matchable ===\n";
    {
        OrderBook book;
        book.addOrder(new Order{1, false, "limit", 100.0, 5, 1000}); // head
        book.addOrder(new Order{2, false, "limit", 100.0, 5, 1001}); // tail
        book.removeOrder(1); // remove head
        book.printBook(); // only order 2 remains at 100
        book.addOrder(new Order{3, true, "limit", 100.0, 5, 1002}); // should match order 2
        book.printBook(); // should be empty
    }

    return 0;
}
    */

