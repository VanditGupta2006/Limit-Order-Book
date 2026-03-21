#pragma once
#include <vector>
#include <memory>
#include <fstream>
#include <iostream>
#include "OrderBook.h"
#include "Bot.h"

// One row per price level per tick — for LOB depth animation
struct LOBSnapshot {
    long long time;
    double    price;
    double    quantity;
    bool      isBid;
    int       level;      // 1 = best, 2 = second best, etc.
};

class Simulation {
    OrderBook                         book;
    std::vector<std::unique_ptr<Bot>> bots;
    std::vector<TradeEvent>           tradeLog;
    std::vector<LOBSnapshot>          snapshots;
    long long simTime       = 0;
    int       nextOrdId     = 1000;
    int       snapshotEvery = 1;   // take a snapshot every N ticks

public:
    void addBot(std::unique_ptr<Bot> bot) {
        bots.push_back(std::move(bot));
    }

    void setSnapshotFrequency(int every) { snapshotEvery = every; }

    void init() {
        book.onTrade = [this](const TradeEvent& evt) {
            tradeLog.push_back(evt);

            for (auto& b : bots)
                if (b->traderId == evt.makerTraderId)
                    b->onFill(evt, true);

            if (evt.takerTraderId >= 0)
                for (auto& b : bots)
                    if (b->traderId == evt.takerTraderId)
                        b->onFill(evt, false);
        };
    }

    void runTick() {
        simTime++;
        book.simTime = simTime;
        LOBState state = book.getState();

        for (auto& bot : bots) {
            for (int cancelId : bot->ordersToCancel(state, simTime)) {
                book.removeOrder(cancelId);
                bot->activeOrders.erase(cancelId);
            }
            for (const auto& req : bot->act(state, simTime)) {
                int oid = nextOrdId++;
                if (req.price < 0.0) {
                    if (req.isBuy) book.placeMarketBuyOrder(bot->traderId, req.quantity);
                    else           book.placeMarketSellOrder(bot->traderId, req.quantity);
                } else {
                    Order* o = new Order{oid, bot->traderId, req.isBuy,
                                         "limit", req.price, req.quantity, simTime};
                    bot->activeOrders[oid] = req.price;
                    book.addOrder(o);
                }
            }
        }

        // Take LOB snapshot every N ticks
        if (simTime % snapshotEvery == 0)
            takeSnapshot();
    }

    void run(int numTicks) {
        init();
        for (int i = 0; i < numTicks; i++)
            runTick();
    }

    // -------------------------------------------------------
    // Snapshot — captures up to `depth` levels each side
    // -------------------------------------------------------
    void takeSnapshot(int depth = 5) {
        int level = 1;
        for (auto& [price, lim] : book.bidLimits) {
            if (level > depth) break;
            snapshots.push_back({simTime, price, lim->totalQuantity, true, level++});
        }
        level = 1;
        for (auto& [price, lim] : book.askLimits) {
            if (level > depth) break;
            snapshots.push_back({simTime, price, lim->totalQuantity, false, level++});
        }
    }

    // -------------------------------------------------------
    // Export
    // -------------------------------------------------------
    void exportTrades(const std::string& filename) const {
        std::ofstream f(filename);
        f << "timestamp,price,quantity,makerTrader,takerTrader,buyerIsMaker\n";
        for (const auto& t : tradeLog)
            f << t.timestamp     << ","
              << t.price         << ","
              << t.quantity      << ","
              << t.makerTraderId << ","
              << t.takerTraderId << ","
              << t.buyerIsMaker  << "\n";
        std::cout << "Exported " << tradeLog.size() << " trades to " << filename << "\n";
    }

    void exportSnapshots(const std::string& filename) const {
        std::ofstream f(filename);
        f << "timestamp,price,quantity,side,level\n";
        for (const auto& s : snapshots)
            f << s.time     << ","
              << s.price    << ","
              << s.quantity << ","
              << (s.isBid ? "bid" : "ask") << ","
              << s.level    << "\n";
        std::cout << "Exported " << snapshots.size() << " snapshot rows to " << filename << "\n";
    }

    void printSummary() const {
        std::cout << "\n=== Simulation summary ===\n";
        std::cout << "Ticks run  : " << simTime   << "\n";
        std::cout << "Trades     : " << tradeLog.size() << "\n";
        for (const auto& b : bots)
            std::cout << "Bot " << b->traderId
                      << " | cash=" << b->cash
                      << " | inventory=" << b->inventory
                      << " | active orders=" << b->activeOrders.size() << "\n";
    }

    const std::vector<TradeEvent>& getTrades()    const { return tradeLog; }
    const OrderBook&               getBook()      const { return book; }
};