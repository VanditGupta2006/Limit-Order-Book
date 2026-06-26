#pragma once

// ============================================================================
// DataLogger.h — CSV logging for trades and LOB snapshots
// ============================================================================
//
// Writes two CSV files in real-time during simulation:
//   1. Trades CSV    — one row per trade execution
//   2. Snapshots CSV — one row per tick with top-of-book state
//
// Usage:
//   DataLogger logger("data/trades.csv", "data/snapshots.csv");
//   // ... pass &logger to Simulation constructor
//
// The constructor opens both files and writes headers.
// The destructor flushes and closes both files.
//
// Note: TradeEvent.price is in integer ticks — we convert to double for CSV.
// ============================================================================

#include <fstream>
#include <string>
#include <iostream>
#include "TradeEvent.h"
#include "OrderBook.h"   // for LOBState

class DataLogger {
    std::ofstream tradesFile;
    std::ofstream snapshotsFile;

public:
    // -----------------------------------------------------------------------
    // Constructor — opens files and writes CSV headers
    // -----------------------------------------------------------------------
    DataLogger(const std::string& tradesPath, const std::string& snapshotsPath) {
        tradesFile.open(tradesPath);
        if (!tradesFile.is_open()) {
            std::cerr << "DataLogger: failed to open " << tradesPath << "\n";
        }
        tradesFile << "timestamp,price,quantity,buyerIsMaker,takerTraderId,makerTraderId\n";

        snapshotsFile.open(snapshotsPath);
        if (!snapshotsFile.is_open()) {
            std::cerr << "DataLogger: failed to open " << snapshotsPath << "\n";
        }
        snapshotsFile << "timestamp,bestBid,bestAsk,mid,spread,bidDepth,askDepth\n";
    }

    // Destructor — flush and close
    ~DataLogger() {
        if (tradesFile.is_open())    tradesFile.close();
        if (snapshotsFile.is_open()) snapshotsFile.close();
    }

    // Non-copyable, non-movable
    DataLogger(const DataLogger&) = delete;
    DataLogger& operator=(const DataLogger&) = delete;

    // -----------------------------------------------------------------------
    // logTrade — write one trade row (converts tick price to double)
    // -----------------------------------------------------------------------
    void logTrade(const TradeEvent& e) {
        if (!tradesFile.is_open()) return;
        tradesFile << e.timestamp      << ","
                   << fromTicks(e.price) << ","
                   << e.quantity       << ","
                   << e.buyerIsMaker   << ","
                   << e.takerTraderId  << ","
                   << e.makerTraderId  << "\n";
    }

    // -----------------------------------------------------------------------
    // logSnapshot — write one LOB state row (already in doubles from getState)
    // -----------------------------------------------------------------------
    void logSnapshot(const LOBState& s) {
        if (!snapshotsFile.is_open()) return;
        snapshotsFile << s.time     << ","
                      << s.bestBid  << ","
                      << s.bestAsk  << ","
                      << s.mid      << ","
                      << s.spread   << ","
                      << s.bidDepth << ","
                      << s.askDepth << "\n";
    }
};
