#pragma once

// ============================================================================
// MeanReversionBot.h — Mean-reverting trading agent
// ============================================================================
//
// Strategy:
//   Maintains a rolling mean of the last N mid-prices.
//   When the current mid deviates from the rolling mean by more than
//   (spread * reversionStrength), it places a limit order to fade the move:
//     - Mid above mean → SELL at bestAsk  (expects price to revert down)
//     - Mid below mean → BUY  at bestBid  (expects price to revert up)
//
// This is the natural counterpart to MomentumBot — in real markets,
// mean-reversion and momentum coexist at different timescales.
//
// Real-world analogue: statistical arbitrage / pairs trading desks that
// trade deviations from fair value.
// ============================================================================

#include <random>
#include <vector>
#include <cmath>
#include "Bot.h"

class MeanReversionBot : public Bot {
    std::mt19937                     rng;
    std::uniform_int_distribution<>  qtyDist;
    std::bernoulli_distribution      actDist;

    int                              windowSize;
    double                           reversionStrength;

    std::vector<double>              priceBuffer;
    int                              bufferIdx   = 0;
    int                              bufferCount = 0;

public:
    // -----------------------------------------------------------------------
    // Constructor
    //
    // id              — unique trader ID (also RNG seed)
    // cash            — starting cash
    // window          — lookback window for rolling mean (default 30)
    // revStrength     — multiplier on spread for trigger threshold (default 1.5)
    // actProb         — probability of acting when signal fires (default 0.5)
    // tickGap         — minimum ticks between actions (default 1)
    // -----------------------------------------------------------------------
    MeanReversionBot(int id, double cash,
                     int window = 30, double revStrength = 1.5,
                     double actProb = 0.5, int tickGap = 1)
        : Bot(id, cash, tickGap),
          rng(static_cast<unsigned>(id)),
          qtyDist(1, 3),
          actDist(actProb),
          windowSize(window),
          reversionStrength(revStrength),
          priceBuffer(window, 0.0) {}

    BotAction act(const LOBState& state, long long /*time*/) override {
        BotAction action;

        // Skip if book is empty or spread/mid is invalid
        if (state.mid <= 0 || state.spread <= 0) return action;

        // Update circular buffer
        priceBuffer[bufferIdx] = state.mid;
        bufferIdx = (bufferIdx + 1) % windowSize;
        if (bufferCount < windowSize) bufferCount++;

        // Need a full window before generating signals
        if (bufferCount < windowSize) return action;

        // Compute rolling mean
        double sum = 0.0;
        for (int i = 0; i < windowSize; i++) sum += priceBuffer[i];
        double rollingMean = sum / windowSize;

        // Threshold = spread * reversionStrength
        double threshold = state.spread * reversionStrength;

        // Probabilistic execution
        if (!actDist(rng)) return action;

        int qty = qtyDist(rng);

        if (state.mid > rollingMean + threshold && state.bestAsk > 0) {
            // Price above mean — sell at best ask (limit order)
            action.orders.push_back({false, state.bestAsk, qty});
        } else if (state.mid < rollingMean - threshold && state.bestBid > 0) {
            // Price below mean — buy at best bid (limit order)
            action.orders.push_back({true, state.bestBid, qty});
        }

        return action;
    }
};
