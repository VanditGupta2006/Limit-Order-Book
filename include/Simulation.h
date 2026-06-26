#pragma once

// ============================================================================
// Simulation.h — Discrete-time simulation engine
// ============================================================================
//
// Orchestrates the interaction between bots and the order book:
//
//   Each tick:
//     1. Increment simulation clock
//     2. Take LOB state snapshot
//     3. For each bot:
//        a. Check minTickGap — skip if bot isn't allowed to act yet
//        b. Process cancels from BotAction
//        c. Submit orders from BotAction (limit or market)
//     4. Log snapshot via DataLogger (if attached)
//
// Ownership:
//   - Bots are owned via unique_ptr
//   - DataLogger is NOT owned (external lifetime management)
//   - OrderBook is owned directly as a member
//
// Price conversion:
//   Bots produce OrderRequest with double prices.  Simulation converts
//   these to integer ticks (toTicks) before creating Order objects.
//
// MarketMakerBot integration:
//   After submitting orders for a MarketMakerBot, we call registerOrderId()
//   so the bot can track its resting orders for next-tick cancellation.
// ============================================================================

#include <vector>
#include <memory>
#include <iostream>
#include "OrderBook.h"
#include "Bot.h"
#include "DataLogger.h"
#include "MarketMakerBot.h"

class Simulation {
    OrderBook                             book;
    std::vector<std::unique_ptr<Bot>>     bots;
    std::vector<TradeEvent>               tradeLog;
    long long                             simTime    = 0;
    int                                   nextOrdId  = 1000;
    DataLogger*                           logger     = nullptr;

    // Track trade count and spread sum for summary statistics
    double spreadSum   = 0.0;
    int    spreadCount = 0;

public:
    // -----------------------------------------------------------------------
    // Constructor — optionally attach a DataLogger
    // -----------------------------------------------------------------------
    explicit Simulation(DataLogger* dataLogger = nullptr)
        : logger(dataLogger) {}

    // -----------------------------------------------------------------------
    // addBot — transfer ownership of a bot to the simulation
    // -----------------------------------------------------------------------
    void addBot(std::unique_ptr<Bot> bot) {
        bots.push_back(std::move(bot));
    }

    // -----------------------------------------------------------------------
    // init — wire up the onTrade callback
    // -----------------------------------------------------------------------
    void init() {
        book.onTrade = [this](const TradeEvent& evt) {
            tradeLog.push_back(evt);

            // Log trade to CSV
            if (logger) logger->logTrade(evt);

            // Notify maker bot
            for (auto& b : bots) {
                if (b->traderId == evt.makerTraderId) {
                    b->onFill(evt, true);
                    break;
                }
            }

            // Notify taker bot
            if (evt.takerTraderId >= 0) {
                for (auto& b : bots) {
                    if (b->traderId == evt.takerTraderId) {
                        b->onFill(evt, false);
                        break;
                    }
                }
            }
        };
    }

    // -----------------------------------------------------------------------
    // runTick — advance simulation by one tick
    // -----------------------------------------------------------------------
    void runTick() {
        simTime++;
        book.simTime = simTime;
        LOBState state = book.getState();

        for (auto& bot : bots) {
            // Latency simulation: skip if bot isn't allowed to act yet
            if (simTime < bot->nextAllowedTick) continue;

            BotAction action = bot->act(state, simTime);

            // Update next allowed tick
            bot->nextAllowedTick = simTime + bot->minTickGap;

            // Process cancels first
            for (const auto& cancel : action.cancels) {
                book.removeOrder(cancel.orderId);
                bot->activeOrders.erase(cancel.orderId);
            }

            // Process orders
            for (const auto& req : action.orders) {
                int oid = nextOrdId++;

                if (req.price < 0.0) {
                    // Market order
                    if (req.isBuy) book.placeMarketBuyOrder(bot->traderId, req.quantity);
                    else           book.placeMarketSellOrder(bot->traderId, req.quantity);
                } else {
                    // Limit order — convert double price to integer ticks
                    Price tickPrice = toTicks(req.price);
                    Order* o = new Order(oid, bot->traderId, req.isBuy,
                                         "limit", tickPrice, req.quantity, simTime);
                    bot->activeOrders[oid] = req.price;
                    book.addOrder(o);

                    // If this is a MarketMakerBot, register the order ID
                    MarketMakerBot* mm = dynamic_cast<MarketMakerBot*>(bot.get());
                    if (mm) mm->registerOrderId(oid);
                }
            }
        }

        // Log LOB snapshot after all bot actions
        if (logger) {
            LOBState postState = book.getState();
            logger->logSnapshot(postState);

            // Accumulate spread statistics
            if (postState.spread > 0) {
                spreadSum += postState.spread;
                spreadCount++;
            }
        }
    }

    // -----------------------------------------------------------------------
    // run — execute N ticks
    // -----------------------------------------------------------------------
    void run(int numTicks) {
        init();
        for (int i = 0; i < numTicks; i++)
            runTick();
    }

    // -----------------------------------------------------------------------
    // printSummary — print end-of-simulation statistics
    // -----------------------------------------------------------------------
    void printSummary() const {
        LOBState finalState = book.getState();

        std::cout << "\n=== Simulation Summary ===\n";
        std::cout << "  Ticks run     : " << simTime << "\n";
        std::cout << "  Total trades  : " << tradeLog.size() << "\n";
        std::cout << "  Final mid     : " << finalState.mid << "\n";
        std::cout << "  STP skips     : " << book.stpSkipCount << "\n";

        if (spreadCount > 0) {
            std::cout << "  Avg spread    : " << (spreadSum / spreadCount) << "\n";
        } else {
            std::cout << "  Avg spread    : N/A\n";
        }

        std::cout << "\n  Bot Positions:\n";
        for (const auto& b : bots) {
            std::cout << "    Bot " << b->traderId
                      << " | cash=" << b->cash
                      << " | inventory=" << b->inventory
                      << " | active=" << b->activeOrders.size()
                      << " | gap=" << b->minTickGap << "\n";
        }
    }

    // -----------------------------------------------------------------------
    // Accessors
    // -----------------------------------------------------------------------
    const std::vector<TradeEvent>& getTrades() const { return tradeLog; }
    const OrderBook&               getBook()   const { return book; }
};
