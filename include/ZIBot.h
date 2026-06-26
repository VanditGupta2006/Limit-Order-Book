#pragma once

// ============================================================================
// ZIBot.h — Zero Intelligence bot (unconstrained)
// ============================================================================
//
// Based on Gode & Sunder (1993).  Places random limit orders uniformly
// distributed over [minPrice, maxPrice] with random side (50/50 buy/sell)
// and random quantity (1–5).
//
// Acts with probability actProb each tick (introduces noise in activity).
// Uses mt19937 seeded with traderId for full reproducibility.
//
// Key insight from the paper: even zero-intelligence traders generate
// near-efficient market outcomes when constrained by the budget mechanism
// (here we use the unconstrained variant for simplicity).
// ============================================================================

#include <random>
#include "Bot.h"

class ZIBot : public Bot {
    std::mt19937                          rng;
    std::uniform_real_distribution<>      priceDist;
    std::uniform_int_distribution<>       qtyDist;
    std::bernoulli_distribution           sideDist;    // 50% buy, 50% sell
    std::bernoulli_distribution           actDist;

public:
    // -----------------------------------------------------------------------
    // Constructor
    //
    // id       — unique trader ID (also used as RNG seed)
    // cash     — starting cash balance
    // minPrice — lower bound of the price range
    // maxPrice — upper bound of the price range
    // actProb  — probability of acting on any given tick (default 0.7)
    // -----------------------------------------------------------------------
    ZIBot(int id, double cash,
          double minPrice, double maxPrice,
          double actProb = 0.7)
        : Bot(id, cash),
          rng(static_cast<unsigned>(id)),
          priceDist(minPrice, maxPrice),
          qtyDist(1, 5),
          sideDist(0.5),
          actDist(actProb) {}

    BotAction act(const LOBState& /*state*/, long long /*time*/) override {
        BotAction action;

        if (!actDist(rng)) return action;

        bool   isBuy = sideDist(rng);
        double price = priceDist(rng);
        int    qty   = qtyDist(rng);

        action.orders.push_back({isBuy, price, qty});
        return action;
    }
};
