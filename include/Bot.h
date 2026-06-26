#pragma once

// ============================================================================
// Bot.h — Abstract base class for all trading agents
// ============================================================================
//
// Every bot must implement act(), which returns a BotAction containing:
//   - orders:  limit/market orders to submit
//   - cancels: order IDs to cancel
//
// The Simulation loop calls act() on every bot each tick, then processes
// the returned BotAction.  Fill notifications arrive via onFill().
//
// Latency simulation:
//   - minTickGap controls how frequently a bot can act (in ticks)
//   - nextAllowedTick is managed by the Simulation to enforce the gap
//   - Higher minTickGap = slower/more latent trader
//
// Inventory tracking:
//   - cash decreases when buying, increases when selling
//   - inventory increases when buying, decreases when selling
//   - activeOrders maps orderId → price for every resting order this bot owns
// ============================================================================

#include <vector>
#include <unordered_map>
#include "OrderBook.h"   // gives us LOBState, TradeEvent, Price, fromTicks

// ---------------------------------------------------------------------------
// OrderRequest — describes a limit or market order to submit
// ---------------------------------------------------------------------------
struct OrderRequest {
    bool   isBuy;
    double price;       // negative value (e.g. -1.0) signals a market order
    int    quantity;
};

// ---------------------------------------------------------------------------
// CancelRequest — describes an order to cancel
// ---------------------------------------------------------------------------
struct CancelRequest {
    int orderId;
};

// ---------------------------------------------------------------------------
// BotAction — the complete set of actions a bot wants to take this tick
// ---------------------------------------------------------------------------
struct BotAction {
    std::vector<OrderRequest>  orders;
    std::vector<CancelRequest> cancels;
};

// ---------------------------------------------------------------------------
// Bot — abstract base class
// ---------------------------------------------------------------------------
class Bot {
public:
    int    traderId;
    double cash;
    int    inventory = 0;   // positive = long, negative = short

    // Latency simulation: bot can only act every minTickGap ticks
    int    minTickGap      = 1;   // default: act every tick
    long long nextAllowedTick = 0;   // managed by Simulation

    // orderId → price: tracks every resting order this bot owns
    std::unordered_map<int, double> activeOrders;

    Bot(int id, double startCash, int tickGap = 1)
        : traderId(id), cash(startCash), minTickGap(tickGap) {}
    virtual ~Bot() = default;

    // -----------------------------------------------------------------------
    // act() — called every tick.  Return orders to submit and cancels.
    // -----------------------------------------------------------------------
    virtual BotAction act(const LOBState& state, long long time) = 0;

    // -----------------------------------------------------------------------
    // onFill() — called by Simulation when one of this bot's orders fills.
    //
    // wasMaker=true  → this bot's resting order was hit
    // wasMaker=false → this bot sent the aggressive order
    //
    // Note: TradeEvent.price is in integer ticks — use fromTicks() for cash.
    // -----------------------------------------------------------------------
    virtual void onFill(const TradeEvent& evt, bool wasMaker) {
        // Determine if this bot bought or sold
        bool bought = ( wasMaker &&  evt.buyerIsMaker) ||
                      (!wasMaker && !evt.buyerIsMaker);

        double fillPrice = fromTicks(evt.price);

        if (bought) {
            inventory += evt.quantity;
            cash      -= fillPrice * evt.quantity;
        } else {
            inventory -= evt.quantity;
            cash      += fillPrice * evt.quantity;
        }

        // Remove from active tracking if fully filled
        if (wasMaker) activeOrders.erase(evt.makerOrderId);
        else          activeOrders.erase(evt.takerOrderId);
    }
};
