#include <iostream>
#include "Simulation.h"
#include "ZIBot.h"

int main() {
    Simulation sim;

    // Three ZI bots, each seeded differently via their traderId
    // Prices between 95 and 105, 40% chance to act each tick
    sim.addBot(std::make_unique<ZIBot>(0, 100000.0, 95.0, 105.0));
    sim.addBot(std::make_unique<ZIBot>(1, 100000.0, 95.0, 105.0));
    sim.addBot(std::make_unique<ZIBot>(2, 100000.0, 95.0, 105.0));
    sim.addBot(std::make_unique<ZIBot>(3, 100000.0, 95.0, 105.0));
    sim.addBot(std::make_unique<ZIBot>(4, 100000.0, 95.0, 105.0));
    sim.addBot(std::make_unique<ZIBot>(5, 100000.0, 95.0, 105.0));


    sim.run(5000);

    sim.printSummary();
    sim.exportTrades("trades.csv");
    sim.exportSnapshots("snapshots.csv");

    return 0;
}