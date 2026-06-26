#pragma once

// ============================================================================
// MarketMakerBot.h — Automated market-making agent
// ============================================================================
//
// Strategy:
//   Every tick, cancel all existing resting orders and re-quote:
//     Bid = mid - halfSpread  (adjusted for inventory skew)
//     Ask = mid + halfSpread  (adjusted for inventory skew)
//
//   Inventory skew: if inventory > 0 (long), shift both quotes DOWN
//   to encourage selling and discourage buying.  Vice versa for short.
//   Skew magnitude = (inventory / inventoryLimit) * halfSpread.
//
// Risk management:
//   - Does NOT quote if abs(inventory) >= inventoryLimit
//   - Does NOT quote if mid <= 0 (empty book)
//
// Real-world analogue: designated market makers (DMMs) on NYSE,
// HFT firms providing passive liquidity on electronic venues.
// ============================================================================

#include <vector>
#include <cmath>
#include "Bot.h"

class MarketMakerBot : public Bot {
    double halfSpread;
    int    quoteSize;
    int    inventoryLimit;

    // Track order IDs of our current resting quotes
    std::vector<int> activeOrderIds;

public:
    // -----------------------------------------------------------------------
    // Constructor
    //
    // id           — unique trader ID
    // cash         — starting cash
    // halfSpr      — half the quoted spread (default 0.5)
    // quoteSz      — quantity per side (default 10)
    // invLimit     — maximum absolute inventory (default 100)
    // -----------------------------------------------------------------------
    MarketMakerBot(int id, double cash,
                   double halfSpr = 0.5, int quoteSz = 10,
                   int invLimit = 100)
        : Bot(id, cash),
          halfSpread(halfSpr),
          quoteSize(quoteSz),
          inventoryLimit(invLimit) {}

    BotAction act(const LOBState& state, long long /*time*/) override {
        BotAction action;

        // Step 1: Cancel all existing resting orders
        for (int oid : activeOrderIds) {
            action.cancels.push_back({oid});
        }
        activeOrderIds.clear();

        // Step 2: Check if we should quote
        if (state.mid <= 0) return action;
        if (std::abs(inventory) >= inventoryLimit) return action;

        // Step 3: Compute inventory skew
        // Positive inventory → shift quotes down to encourage selling
        // Negative inventory → shift quotes up to encourage buying
        double skew = 0.0;
        if (inventoryLimit > 0) {
            skew = (static_cast<double>(inventory) / inventoryLimit) * halfSpread;
        }

        // Step 4: Compute quote prices
        double bidPrice = state.mid - halfSpread - skew;
        double askPrice = state.mid + halfSpread - skew;

        // Sanity: don't post negative prices
        if (bidPrice <= 0) bidPrice = 0.01;

        // Step 5: Post quotes
        action.orders.push_back({true,  bidPrice, quoteSize});   // bid
        action.orders.push_back({false, askPrice, quoteSize});   // ask

        return action;
    }

    // -----------------------------------------------------------------------
    // onFill — update tracking when our resting orders are filled
    // -----------------------------------------------------------------------
    void onFill(const TradeEvent& evt, bool wasMaker) override {
        Bot::onFill(evt, wasMaker);

        // Remove filled order from our active tracking
        if (wasMaker) {
            auto it = std::find(activeOrderIds.begin(), activeOrderIds.end(),
                                evt.makerOrderId);
            if (it != activeOrderIds.end()) activeOrderIds.erase(it);
        }
    }

    // -----------------------------------------------------------------------
    // registerOrderId — called by Simulation after submitting our orders
    // so we can track them for cancellation next tick
    // -----------------------------------------------------------------------
    void registerOrderId(int orderId) {
        activeOrderIds.push_back(orderId);
    }
};
