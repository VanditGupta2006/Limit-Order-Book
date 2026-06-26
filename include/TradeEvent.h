#pragma once

// ============================================================================
// TradeEvent.h — Immutable record of a single trade execution
// ============================================================================
//
// Created every time two orders match in the OrderBook engine.
// Stored in the trade log, forwarded to DataLogger, and dispatched
// to the participating bots via their onFill() callbacks.
//
// Fields:
//   timestamp     — simulation tick at which the trade occurred
//   price         — execution price in integer ticks (use fromTicks() to display)
//   quantity      — number of units traded in this fill
//   buyerIsMaker  — true if the resting (maker) order was a bid
//   takerTraderId — bot ID of the aggressive (incoming) order
//   takerOrderId  — order ID of the aggressive order (-1 for market orders)
//   makerTraderId — bot ID of the resting (passive) order
//   makerOrderId  — order ID of the resting order
// ============================================================================

#include "Types.h"

struct TradeEvent {
    long long timestamp    = 0;
    Price     price        = 0;      // integer ticks — use fromTicks() to display
    int       quantity     = 0;
    bool      buyerIsMaker = false;   // true = resting order was a bid

    int       takerTraderId = -1;
    int       takerOrderId  = -1;
    int       makerTraderId = -1;
    int       makerOrderId  = -1;
};
