// ============================================================================
// main.cpp — LOB Simulation Entry Point
// ============================================================================
//
// Runs three simulation configurations to generate synthetic market data:
//
//   Config 1: "Noise Only"          — 20 ZI bots
//   Config 2: "Trending Market"     — 10 ZI + 8 Momentum + 2 Market Makers
//   Config 3: "Mean Reverting"      — 10 ZI + 8 MeanReversion + 2 Market Makers
//
// Each config outputs trades and LOB snapshots to CSV files in data/.
//
// Bot latency (minTickGap):
//   Market Makers act every tick (gap=1) — fastest participants
//   ZI bots act every 2 ticks (gap=2)   — moderate latency
//   Momentum/MeanRev act every 3 ticks  — slower signal-based traders
// ============================================================================

#include <iostream>
#include <memory>
#include <filesystem>

#include "include/Simulation.h"
#include "include/ZIBot.h"
#include "include/MomentumBot.h"
#include "include/MeanReversionBot.h"
#include "include/MarketMakerBot.h"
#include "include/DataLogger.h"

// ============================================================================
// Simulation parameters
// ============================================================================
static constexpr int    NUM_TICKS     = 10000;
static constexpr double MIN_PRICE     = 95.0;
static constexpr double MAX_PRICE     = 105.0;
static constexpr double START_CASH    = 100000.0;

// Bot latency settings (ticks between actions)
static constexpr int    ZI_GAP        = 2;   // noise traders: moderate latency
static constexpr int    SIGNAL_GAP    = 3;   // signal-based: slower
static constexpr int    MM_GAP        = 1;   // market makers: fastest

// ============================================================================
// Config 1: Noise Only — pure ZI bots
// ============================================================================
void runConfig1(const std::string& dataDir) {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "  CONFIG 1: Noise Only (20 ZI Bots)\n";
    std::cout << std::string(60, '=') << "\n";

    DataLogger logger(dataDir + "/config1_trades.csv",
                      dataDir + "/config1_snapshots.csv");
    Simulation sim(&logger);

    // Add 20 ZI bots (gap=2)
    for (int i = 0; i < 20; i++) {
        sim.addBot(std::make_unique<ZIBot>(i, START_CASH, MIN_PRICE, MAX_PRICE,
                                            0.7, ZI_GAP));
    }

    sim.run(NUM_TICKS);
    sim.printSummary();
}

// ============================================================================
// Config 2: Trending Market — ZI + Momentum + Market Makers
// ============================================================================
void runConfig2(const std::string& dataDir) {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "  CONFIG 2: Trending Market (10 ZI + 8 Momentum + 2 MM)\n";
    std::cout << std::string(60, '=') << "\n";

    DataLogger logger(dataDir + "/config2_trades.csv",
                      dataDir + "/config2_snapshots.csv");
    Simulation sim(&logger);

    int id = 0;

    // 10 ZI bots (gap=2)
    for (int i = 0; i < 10; i++) {
        sim.addBot(std::make_unique<ZIBot>(id++, START_CASH, MIN_PRICE, MAX_PRICE,
                                            0.7, ZI_GAP));
    }

    // 8 Momentum bots (window=20, threshold=0.5%, actProb=0.6, maxInv=50, gap=3)
    for (int i = 0; i < 8; i++) {
        sim.addBot(std::make_unique<MomentumBot>(id++, START_CASH,
                                                  20, 0.005, 0.6, 50, SIGNAL_GAP));
    }

    // 2 Market Maker bots (halfSpread=0.5, quoteSize=10, invLimit=100, gap=1)
    for (int i = 0; i < 2; i++) {
        sim.addBot(std::make_unique<MarketMakerBot>(id++, START_CASH,
                                                     0.5, 10, 100, MM_GAP));
    }

    sim.run(NUM_TICKS);
    sim.printSummary();
}

// ============================================================================
// Config 3: Mean Reverting Market — ZI + MeanReversion + Market Makers
// ============================================================================
void runConfig3(const std::string& dataDir) {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "  CONFIG 3: Mean Reverting (10 ZI + 8 MeanRev + 2 MM)\n";
    std::cout << std::string(60, '=') << "\n";

    DataLogger logger(dataDir + "/config3_trades.csv",
                      dataDir + "/config3_snapshots.csv");
    Simulation sim(&logger);

    int id = 0;

    // 10 ZI bots (gap=2)
    for (int i = 0; i < 10; i++) {
        sim.addBot(std::make_unique<ZIBot>(id++, START_CASH, MIN_PRICE, MAX_PRICE,
                                            0.7, ZI_GAP));
    }

    // 8 Mean Reversion bots (window=30, revStrength=1.5, actProb=0.5, gap=3)
    for (int i = 0; i < 8; i++) {
        sim.addBot(std::make_unique<MeanReversionBot>(id++, START_CASH,
                                                       30, 1.5, 0.5, SIGNAL_GAP));
    }

    // 2 Market Maker bots (gap=1)
    for (int i = 0; i < 2; i++) {
        sim.addBot(std::make_unique<MarketMakerBot>(id++, START_CASH,
                                                     0.5, 10, 100, MM_GAP));
    }

    sim.run(NUM_TICKS);
    sim.printSummary();
}

// ============================================================================
// Main
// ============================================================================
int main() {
    // Ensure data/ directory exists
    const std::string dataDir = "data";
    std::filesystem::create_directories(dataDir);

    std::cout << "LOB Simulation — Market Microstructure Data Generator\n";
    std::cout << "=====================================================\n";
    std::cout << "Ticks per config : " << NUM_TICKS << "\n";
    std::cout << "Price range      : [" << MIN_PRICE << ", " << MAX_PRICE << "]\n";
    std::cout << "Starting cash    : " << START_CASH << "\n";
    std::cout << "Tick size        : " << (1.0 / PRICE_SCALE) << "\n";
    std::cout << "Bot latency      : ZI=" << ZI_GAP << " Signal=" << SIGNAL_GAP
              << " MM=" << MM_GAP << " ticks\n";

    runConfig1(dataDir);
    runConfig2(dataDir);
    runConfig3(dataDir);

    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "  All simulations complete. CSVs written to data/\n";
    std::cout << std::string(60, '=') << "\n";

    return 0;
}