#pragma once
#include <random>
#include "Bot.h"

// Zero-Intelligence bot: submits random limit orders.
// No strategy — exists to test the engine and generate base noise.
class ZIBot : public Bot {
    std::mt19937                     rng;
    std::uniform_real_distribution<> priceDist;
    std::uniform_int_distribution<>  qtyDist{1, 5};
    std::bernoulli_distribution      sideDist{0.5};   // 50% buy, 50% sell
    std::bernoulli_distribution      actDist;          // probability of acting

public:
    ZIBot(int id, double cash, double minPrice, double maxPrice,
          double actProb = 0.4)
        : Bot(id, cash),
          rng(id),         // seeded with traderId → reproducible per-bot
          priceDist(minPrice, maxPrice),
          actDist(actProb) {}

    std::vector<OrderRequest> act(const LOBState& state,
                                   long long time) override {
        if (!actDist(rng)) return {};   // sit out this tick

        return {{ sideDist(rng), priceDist(rng), qtyDist(rng) }};
    }

    // ZI never cancels — orders rest until filled or simulation ends
};