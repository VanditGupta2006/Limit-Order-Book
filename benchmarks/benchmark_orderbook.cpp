// ============================================================================
// benchmark_orderbook.cpp — Micro-benchmarks for the LOB matching engine
// ============================================================================
//
// Uses Google Benchmark to measure per-operation latency of the OrderBook.
// Results are reported in nanoseconds per operation and implied rate/second.
//
// Benchmarks:
//   1. GetBestBid             — O(log n) map lookup → tells us query cost
//   2. GetState               — full LOB snapshot construction
//   3. PlaceLimitOrder_New    — limit order creating a new price level
//   4. PlaceLimitOrder_Exist  — limit order joining an existing price level
//   5. PlaceMarketOrder_1     — market order crossing 1 price level
//   6. PlaceMarketOrder_3     — market order crossing 3 price levels
//   7. RemoveOrder            — cancel a resting order by ID
//   8. FullCycle              — place limit + market match (round trip)
//   9. SimulationTick         — one full tick with 20 bots (end-to-end)
//
// Run with:
//   ./orderbook_benchmark
//   ./orderbook_benchmark --benchmark_format=json          # JSON output
//   ./orderbook_benchmark --benchmark_filter=PlaceLimit.*  # filter tests
// ============================================================================

#include <benchmark/benchmark.h>
#include <random>
#include <memory>
#include "Types.h"
#include "OrderBook.h"
#include "Simulation.h"
#include "ZIBot.h"
#include "MarketMakerBot.h"

// ============================================================================
// Helpers
// ============================================================================

// Build a pre-populated order book with N price levels on each side
static OrderBook buildBook(int levelsPerSide, int ordersPerLevel) {
    OrderBook book;
    int id = 1;
    // Bids: 9900, 9899, 9898, ... (ticks)
    for (int i = 0; i < levelsPerSide; i++) {
        Price p = 9900 - i;
        for (int j = 0; j < ordersPerLevel; j++) {
            Order* o = new Order(id++, j % 10, true, "limit", p, 10, 0);
            book.addOrder(o);
        }
    }
    // Asks: 10000, 10001, 10002, ... (ticks)
    for (int i = 0; i < levelsPerSide; i++) {
        Price p = 10000 + i;
        for (int j = 0; j < ordersPerLevel; j++) {
            Order* o = new Order(id++, j % 10, false, "limit", p, 10, 0);
            book.addOrder(o);
        }
    }
    return book;
}

// ============================================================================
// 1. GetBestBid — query best bid price from a populated book
// ============================================================================
static void BM_GetBestBid(benchmark::State& state) {
    OrderBook book = buildBook(50, 5);
    for (auto _ : state) {
        benchmark::DoNotOptimize(book.getBestBid());
    }
}
BENCHMARK(BM_GetBestBid);

// ============================================================================
// 2. GetBestAsk — query best ask price
// ============================================================================
static void BM_GetBestAsk(benchmark::State& state) {
    OrderBook book = buildBook(50, 5);
    for (auto _ : state) {
        benchmark::DoNotOptimize(book.getBestAsk());
    }
}
BENCHMARK(BM_GetBestAsk);

// ============================================================================
// 3. GetState — full LOB snapshot (bid/ask/mid/spread/depths)
// ============================================================================
static void BM_GetState(benchmark::State& state) {
    OrderBook book = buildBook(50, 5);
    for (auto _ : state) {
        benchmark::DoNotOptimize(book.getState());
    }
}
BENCHMARK(BM_GetState);

// ============================================================================
// 4. PlaceLimitOrder — new price level (worst case: map insert)
// ============================================================================
static void BM_PlaceLimitOrder_NewLevel(benchmark::State& state) {
    int id = 100000;
    for (auto _ : state) {
        state.PauseTiming();
        {
            OrderBook book = buildBook(50, 5);
            Order* o = new Order(id++, 99, true, "limit", 5000, 10, 0);
            state.ResumeTiming();

            book.addOrder(o);

            state.PauseTiming();
        }
        state.ResumeTiming();
    }
}
BENCHMARK(BM_PlaceLimitOrder_NewLevel);

// ============================================================================
// 5. PlaceLimitOrder — existing price level (best case: just append)
// ============================================================================
static void BM_PlaceLimitOrder_ExistingLevel(benchmark::State& state) {
    int id = 100000;
    for (auto _ : state) {
        state.PauseTiming();
        {
            OrderBook book = buildBook(50, 5);
            Order* o = new Order(id++, 99, true, "limit", 9900, 10, 0);
            state.ResumeTiming();

            book.addOrder(o);

            state.PauseTiming();
        }
        state.ResumeTiming();
    }
}
BENCHMARK(BM_PlaceLimitOrder_ExistingLevel);

// ============================================================================
// 6. PlaceMarketOrder — crossing 1 price level
// ============================================================================
static void BM_PlaceMarketOrder_Cross1(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        {
            OrderBook book = buildBook(50, 5);
            state.ResumeTiming();

            book.placeMarketBuyOrder(99, 10);

            state.PauseTiming();
        }
        state.ResumeTiming();
    }
}
BENCHMARK(BM_PlaceMarketOrder_Cross1);

// ============================================================================
// 7. PlaceMarketOrder — crossing 3 price levels
// ============================================================================
static void BM_PlaceMarketOrder_Cross3(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        {
            OrderBook book = buildBook(50, 5);
            state.ResumeTiming();

            book.placeMarketBuyOrder(99, 120);

            state.PauseTiming();
        }
        state.ResumeTiming();
    }
}
BENCHMARK(BM_PlaceMarketOrder_Cross3);

// ============================================================================
// 8. RemoveOrder — cancel a resting order by ID
// ============================================================================
static void BM_RemoveOrder(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        {
            OrderBook book = buildBook(50, 5);
            int targetId = 125;
            state.ResumeTiming();

            book.removeOrder(targetId);

            state.PauseTiming();
        }
        state.ResumeTiming();
    }
}
BENCHMARK(BM_RemoveOrder);

// ============================================================================
// 9. FullCycle — place a limit sell, then a limit buy that matches it
// ============================================================================
static void BM_FullCycle_LimitMatch(benchmark::State& state) {
    int id = 100000;
    for (auto _ : state) {
        state.PauseTiming();
        {
            OrderBook book;
            Order* sell = new Order(id++, 0, false, "limit", toTicks(100.0), 10, 0);
            Order* buy = new Order(id++, 1, true, "limit", toTicks(100.0), 10, 0);
            state.ResumeTiming();

            book.addOrder(sell);
            book.addOrder(buy);

            state.PauseTiming();
        }
        state.ResumeTiming();
    }
}
BENCHMARK(BM_FullCycle_LimitMatch);

// ============================================================================
// 10. AmendOrder — cancel + re-submit at new price
// ============================================================================
static void BM_AmendOrder(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        {
            OrderBook book = buildBook(50, 5);
            state.ResumeTiming();

            book.amendOrder(125, 8500, 20);

            state.PauseTiming();
        }
        state.ResumeTiming();
    }
}
BENCHMARK(BM_AmendOrder);

// ============================================================================
// 11. Simulation Tick — one full tick with 20 ZI bots (end-to-end)
// ============================================================================
static void BM_SimulationTick(benchmark::State& state) {
    Simulation sim;
    for (int i = 0; i < 20; i++) {
        sim.addBot(std::make_unique<ZIBot>(i, 100000.0, 95.0, 105.0, 0.7, 1));
    }
    sim.run(1);  // init + first tick to warm up

    for (auto _ : state) {
        sim.run(1);
    }
}
BENCHMARK(BM_SimulationTick);

// ============================================================================
// 12. Throughput test — N orders placed and matched in sequence
// ============================================================================
static void BM_Throughput(benchmark::State& state) {
    const int N = state.range(0);
    for (auto _ : state) {
        OrderBook book;
        int id = 1;
        for (int i = 0; i < N; i++) {
            // Alternate: sell at 100, buy at 100 → instant match
            Order* sell = new Order(id++, 0, false, "limit", 10000, 5, 0);
            book.addOrder(sell);
            Order* buy = new Order(id++, 1, true, "limit", 10000, 5, 0);
            book.addOrder(buy);
        }
        benchmark::DoNotOptimize(book.matchedQuantity);
    }
    // Report throughput: total orders = 2*N (sell+buy per iteration)
    state.SetItemsProcessed(state.iterations() * N * 2);
}
BENCHMARK(BM_Throughput)->Arg(100)->Arg(1000)->Arg(10000);
