#pragma once

struct TradeEvent {
    long long timestamp    = 0;
    int    makerTraderId   = -1;
    int    makerOrderId    = -1;
    int    takerTraderId   = -1;
    int    takerOrderId    = -1;
    double price           = 0.0;
    int    quantity        = 0;
    bool   buyerIsMaker    = false; // true = resting order was a bid
};