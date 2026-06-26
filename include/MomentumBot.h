#pragma once

// ============================================================================
// MomentumBot.h — Trend-following trading agent
// ============================================================================
//
// Strategy:
//   Maintains a circular buffer of the last N mid-prices.
//   If the current mid exceeds the buffer average by more than `threshold`,
//   submit a market BUY order (momentum chase).
//   If the current mid is below the average by more than `threshold`,
//   submit a market SELL order.
//
// Risk management:
//   - Inventory is capped at ±maxInventory to prevent unbounded exposure.
//   - Acts only with probability actProb even when a signal fires.
//
// Real-world analogue: CTA / trend-following hedge funds that use moving
// average crossover signals to enter positions.
// ============================================================================

#include <random>
#include <vector>
#include <numeric>
#include <cmath>
#include "Bot.h"

class MomentumBot : public Bot {
    std::mt19937                     rng;
    std::uniform_int_distribution<>  qtyDist;
    std::bernoulli_distribution      actDist;

    int                              windowSize;      // circular buffer capacity
    double                           threshold;       // % deviation to trigger signal
    int                              maxInventory;

    std::vector<double>              priceBuffer;     // circular buffer
    int                              bufferIdx = 0;   // write position
    int                              bufferCount = 0; // elements stored so far

public:
    // -----------------------------------------------------------------------
    // Constructor
    //
    // id            — unique trader ID (also RNG seed)
    // cash          — starting cash
    // window        — lookback window for moving average (default 20)
    // thresh        — trigger threshold as fraction (default 0.005 = 0.5%)
    // actProb       — probability of acting when signal fires (default 0.6)
    // maxInv        — maximum absolute inventory (default 50)
    // tickGap       — minimum ticks between actions (default 1)
    // -----------------------------------------------------------------------
    MomentumBot(int id, double cash,
                int window = 20, double thresh = 0.005,
                double actProb = 0.6, int maxInv = 50, int tickGap = 1)
        : Bot(id, cash, tickGap),
          rng(static_cast<unsigned>(id)),
          qtyDist(1, 5),
          actDist(actProb),
          windowSize(window),
          threshold(thresh),
          maxInventory(maxInv),
          priceBuffer(window, 0.0) {}

    BotAction act(const LOBState& state, long long /*time*/) override {
        BotAction action;

        // Skip if book is empty or mid is invalid
        if (state.mid <= 0) return action;

        // Update circular buffer with current mid price
        priceBuffer[bufferIdx] = state.mid;
        bufferIdx = (bufferIdx + 1) % windowSize;
        if (bufferCount < windowSize) bufferCount++;

        // Need at least a full window before generating signals
        if (bufferCount < windowSize) return action;

        // Compute moving average
        double sum = 0.0;
        for (int i = 0; i < windowSize; i++) sum += priceBuffer[i];
        double avg = sum / windowSize;

        if (avg <= 0) return action;

        // Calculate deviation from average
        double deviation = (state.mid - avg) / avg;

        // Check for momentum signal
        if (std::abs(deviation) <= threshold) return action;

        // Probabilistic execution
        if (!actDist(rng)) return action;

        int qty = qtyDist(rng);

        if (deviation > threshold) {
            // Bullish momentum — market buy
            if (inventory + qty > maxInventory) return action;
            action.orders.push_back({true, -1.0, qty});   // price < 0 = market order
        } else {
            // Bearish momentum — market sell
            if (inventory - qty < -maxInventory) return action;
            action.orders.push_back({false, -1.0, qty});
        }

        return action;
    }
};
