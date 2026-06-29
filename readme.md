# 📈 Limit Order Book Simulation

A high-fidelity C++ simulation of a limit order book (LOB) with heterogeneous trading agents, designed for **market microstructure research** and **ML training data generation**.

## Project Overview

This project implements a price-time priority limit order book matching engine populated by four types of trading bots — each modelling a distinct real-world market participant. The simulation produces synthetic market data (trades and LOB snapshots) in CSV format, suitable for:

- Studying **stylised facts** of financial markets (fat tails, mean-reverting spreads, volume clustering)
- Training **ML models** for mid-price prediction, optimal execution, and market making
- Backtesting **trading strategies** in a controlled environment
- Understanding **market microstructure** from first principles

---


## Architecture

```
┌─────────────────────────────────────────────────────┐
│                    Simulation.h                      │
│  (orchestrates ticks, dispatches bot actions)        │
│                                                      │
│  ┌──────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │  ZIBot   │  │ MomentumBot  │  │ MeanRevBot   │  │
│  └────┬─────┘  └──────┬───────┘  └──────┬───────┘  │
│       │               │                 │           │
│  ┌────┴───────────────┴─────────────────┴───────┐   │
│  │              Bot.h (base class)               │   │
│  │        act() → BotAction {orders, cancels}    │   │
│  └──────────────────┬───────────────────────────┘   │
│                     │                                │
│  ┌──────────────────▼───────────────────────────┐   │
│  │             OrderBook.h                       │   │
│  │   Bid map (sorted desc) ←→ Ask map (sorted)  │   │
│  │   Limit → Order → Order → ... (FIFO)         │   │
│  │   tryMatch() / restOrder()                    │   │
│  └──────────────────┬───────────────────────────┘   │
│                     │ onTrade callback               │
│  ┌──────────────────▼───────────────────────────┐   │
│  │            DataLogger.h                       │   │
│  │   trades.csv  |  lob_snapshots.csv            │   │
│  └──────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────┘
```

## Order Matching Architecture
-  Order Matching follows FIFO (First-In-First-Out) based matching (on timestamp);
-  Orders on the same timestamp are processed in order of Bots added in main.cpp;
-  Traded Price is the Resting Order Price in log book.

### Project Structure

```
LOB/
├── include/                    # Header-only library
│   ├── OrderBook.h             #   Core matching engine
│   ├── TradeEvent.h            #   Trade execution record
│   ├── Bot.h                   #   Abstract bot base class
│   ├── ZIBot.h                 #   Zero Intelligence bot
│   ├── MomentumBot.h           #   Trend-following bot
│   ├── MeanReversionBot.h      #   Mean-reversion bot
│   ├── MarketMakerBot.h        #   Market maker bot
│   ├── DataLogger.h            #   CSV trade/snapshot logger
│   └── Simulation.h            #   Simulation engine
├── scripts/                    # Python tooling
│   └── visualise.py            #   LOB depth + price plotting
├── docs/                       # Reference papers (PDFs)
├── data/                       # Generated CSVs (gitignored)
├── main.cpp                    # Entry point — 3 sim configs
├── CMakeLists.txt              # CMake build (C++17)
├── .gitignore
└── README.md
```

---

## Trading Bots

| Bot | Strategy Type | Real-world Equivalent | Key Parameters |
|-----|--------------|----------------------|----------------|
| **ZIBot** | Random | Noise / retail flow | `minPrice`, `maxPrice`, `actProb` |
| **MomentumBot** | Trend-following | CTA / trend funds | `window` (20), `threshold` (0.5%), `maxInventory` (50) |
| **MeanReversionBot** | Mean-reversion | Stat arb desks | `window` (30), `reversionStrength` (1.5) |
| **MarketMakerBot** | Market-making | DMM / HFT liquidity | `halfSpread` (0.5), `quoteSize` (10), `inventoryLimit` (100) |

---

## How to Build

### Option 1: CMake (recommended)

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

### Option 2: One-liner with g++

```bash
g++ -std=c++17 -Wall -Wextra -O2 -I include -o lob_sim main.cpp
```

### Option 3: MSVC (Visual Studio Developer Prompt)

```cmd
cl /std:c++17 /EHsc /O2 /I include /Fe:lob_sim.exe main.cpp
```

---

## Running the Components

Once the project is successfully built, you can run each of the components as follows:

### 1. Run the Market Simulation
Executes the trading bot simulation configs and generates trade and snapshot logs inside `data/`:
```bash
# On Linux/macOS:
./build/lob_sim

# On Windows:
.\build\lob_sim.exe
```

### 2. Run the Python Visualiser
Parses the generated CSV files to create performance plots and save them:
```bash
# Install dependencies first:
pip install pandas numpy matplotlib scipy

# Visualise Config 1 (Noise Only):
python scripts/visualise.py --dir data --trades config1_trades.csv --snapshots config1_snapshots.csv --show
```

### 3. Run the Unit Tests
Runs the GoogleTest suite (28 test cases verifying matching engine logic, self-trade prevention, and latencies):
```bash
# On Linux/macOS:
./build/orderbook_test

# On Windows:
.\build\orderbook_test.exe

# Or run via CTest:
cd build && ctest --output-on-failure
```

### 4. Run the Performance Benchmarks
Runs the Google Benchmark suite to measure microsecond/nanosecond latencies of the order book operations:
```bash
# On Linux/macOS:
./build/orderbook_benchmark

# On Windows:
.\build\orderbook_benchmark.exe
```

---

## Running Unit Tests

The project uses [GoogleTest](https://github.com/google/googletest) (fetched automatically by CMake on first build).

### Build & run tests

```bash
# Build everything (simulation + tests)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Run all tests
cd build && ctest --output-on-failure

# Or run the test binary directly for detailed output
./build/orderbook_test
```

### Run specific tests

```bash
# Run only STP tests
./build/orderbook_test --gtest_filter="SelfTradePrevention.*"

# Run only matching engine tests
./build/orderbook_test --gtest_filter="OrderBook.*"

# Run a single test
./build/orderbook_test --gtest_filter="OrderBook.PartialFillResting"
```

### Test coverage (28 tests)

| Suite | Tests | What it covers |
|-------|-------|----------------|
| `PriceTicks` | 3 | Integer tick conversion, 0.1+0.2 edge case |
| `OrderBook` | 20 | Placement, matching, partial fills, market orders, cancel, amend, multi-level walk, stress test |
| `SelfTradePrevention` | 4 | STP for limit orders, market buy/sell, cross-trader matching |
| `Simulation` | 1 | Bot tick-gap enforcement |

---

## Performance Benchmarking

The project integrates [Google Benchmark](https://github.com/google/benchmark) to measure matching engine latencies. Benchmarks are built in **Release mode** to capture true execution speed.

### Build & run benchmarks

```bash
# Configure in Release mode (disable TLS verify if there are SSL issues during download)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target orderbook_benchmark

# Run the benchmark binary
./build/orderbook_benchmark
```

### Benchmark Results (Release Mode)

Running on a 6-core Intel/AMD CPU, the engine achieves **sub-microsecond latencies** and can match up to **8.2 Million orders per second**:

| Operation | Latency (ns) | Throughput / sec | Description |
|-----------|--------------|-------------------|-------------|
| `BM_GetBestBid` | **0.93 ns** | ~1.07 Billion/s | Query top-of-book bid price (O(1) cached lookup) |
| `BM_GetBestAsk` | **0.94 ns** | ~1.06 Billion/s | Query top-of-book ask price (O(1) cached lookup) |
| `BM_GetState` | **252 ns** | ~3.97 Million/s | Generating full `LOBState` (mid, spread, depths) |
| `BM_PlaceLimitOrder_NewLevel` | **708 ns** | ~1.41 Million/s | Place order on a new price level (requires map insertion) |
| `BM_PlaceLimitOrder_ExistingLevel` | **889 ns** | ~1.12 Million/s | Place order on an existing price level (FIFO queue push) |
| `BM_PlaceMarketOrder_Cross1` | **1,084 ns** | ~922,000/s | Place market order matching 1 resting price level |
| `BM_PlaceMarketOrder_Cross3` | **1,798 ns** | ~556,000/s | Place market order matching 3 resting price levels |
| `BM_RemoveOrder` | **914 ns** | ~1.09 Million/s | Cancel a resting order by ID (removal from DLL + map lookup) |
| `BM_FullCycle_LimitMatch` | **639 ns** | ~1.56 Million/s | Place limit sell + place limit buy that instantly fills it |
| `BM_SimulationTick` | **12,817 ns** | ~78,000/s | Execute 1 simulation tick with 20 active ZI trading bots |
| `BM_Throughput/10000` | **243 ns / order** | **8.22 Million/s** | Sequential order match throughput |

---

## How to Run the Simulation

```bash
# Run the compiled simulator
./build/lob_sim          # or .\build\lob_sim.exe on Windows
```

This runs **three simulation configurations** (10,000 ticks each):

| Config | Name | Bot Mix | Output |
|--------|------|---------|--------|
| 1 | Noise Only | 20 ZI bots | `data/config1_*.csv` |
| 2 | Trending Market | 10 ZI + 8 Momentum + 2 MM | `data/config2_*.csv` |
| 3 | Mean Reverting | 10 ZI + 8 MeanRev + 2 MM | `data/config3_*.csv` |

Each config prints a summary (total trades, final mid, avg spread) and saves:
- `configN_trades.csv` — every trade execution
- `configN_snapshots.csv` — LOB state at every tick

---

## Visualisation

The Python visualiser reads the generated CSVs and produces analysis plots, saving all output back to the same directory.

### Prerequisites

```bash
pip install pandas numpy matplotlib scipy
```

### CLI Interface

```
python scripts/visualise.py --dir <DIR> --trades <FILE> --snapshots <FILE> [--show] [--animate]
```

| Argument | Required | Default | Description |
|----------|----------|---------|-------------|
| `--dir` | ✅ | — | Directory containing the CSV files (outputs are saved here too) |
| `--trades` | — | `trades.csv` | Trades CSV filename |
| `--snapshots` | — | `snapshots.csv` | Snapshots CSV filename |
| `--show` | — | off | Open an interactive matplotlib window after saving |
| `--animate` | — | off | Generate an animated GIF of the price path (slower) |

### Usage Examples

```bash
# Visualise each simulation config
python scripts/visualise.py --dir data --trades config1_trades.csv --snapshots config1_snapshots.csv
python scripts/visualise.py --dir data --trades config2_trades.csv --snapshots config2_snapshots.csv
python scripts/visualise.py --dir data --trades config3_trades.csv --snapshots config3_snapshots.csv

# Open interactive window + generate animation GIF
python scripts/visualise.py --dir data --trades config1_trades.csv --snapshots config1_snapshots.csv --show --animate
```

### Output Files

The output prefix is auto-derived from the trades filename (e.g. `config1_trades.csv` → `config1`):

| File | Content |
|------|---------|
| `{prefix}_price_spread.png` | Mid price path with bid-ask band + spread time series |
| `{prefix}_returns.png` | Log-return histogram vs Normal fit + QQ plot (fat tails check) |
| `{prefix}_volume_depth.png` | Trade volume per tick + bid/ask book depth |
| `{prefix}_summary.png` | 4-panel dashboard: mid price, spread, return autocorrelation, key stats table |
| `{prefix}_animation.gif` | *(only with `--animate`)* Animated price path with bid/ask lines |

---

## Simulation Validation

The generated data should exhibit these **stylised facts** of real financial markets:

1. **Fat tails** — return distribution has excess kurtosis (heavier tails than Gaussian)
2. **Spread mean reversion** — the bid-ask spread oscillates around a stable mean
3. **Volume clustering** — trades arrive in bursts, not uniformly
4. **Order flow imbalance (OFI) predicts price** — net buying pressure forecasts short-term price moves
5. **Concave price impact** — large orders move price less than proportionally (√n scaling)

These can be verified using the visualiser plots above — in particular `_returns.png` for fat tails and `_price_spread.png` for spread mean reversion.

---

## Data Format

### trades.csv

| Column | Type | Description |
|--------|------|-------------|
| `timestamp` | int | Simulation tick |
| `price` | float | Execution price |
| `quantity` | int | Units traded |
| `buyerIsMaker` | bool | 1 if resting order was a bid |
| `takerTraderId` | int | Aggressive bot's ID |
| `makerTraderId` | int | Passive bot's ID |

### lob_snapshots.csv

| Column | Type | Description |
|--------|------|-------------|
| `timestamp` | int | Simulation tick |
| `bestBid` | float | Top-of-book bid (-1 if empty) |
| `bestAsk` | float | Top-of-book ask (-1 if empty) |
| `mid` | float | Midpoint price (-1 if one side empty) |
| `spread` | float | Best ask - best bid (-1 if invalid) |
| `bidDepth` | float | Total resting bid quantity |
| `askDepth` | float | Total resting ask quantity |

---

## Future Work

### ML Pipeline

The generated CSV data feeds directly into a machine learning pipeline:

1. **Feature Engineering** — construct features from LOB snapshots:
   - Order flow imbalance (OFI)
   - Weighted mid-price
   - Rolling volatility, volume, and spread statistics
   - Book imbalance at multiple depth levels

2. **Models**:
   - **XGBoost** — mid-price direction classification (tick-by-tick)
   - **LSTM** — sequence-to-sequence mid-price regression
   - **Transformer** — attention over LOB state sequences

3. **Validation** — walk-forward (expanding window) to prevent look-ahead bias:
   - Train on ticks [0, T], predict [T+1, T+k]
   - Slide window forward, retrain, repeat
   - Report Sharpe ratio of a simple strategy following model predictions

---

## References

- Gode & Sunder (1993) — *Allocative Efficiency of Markets with Zero-Intelligence Traders*
- Cont, Stoikov & Talreja (2010) — *A Stochastic Model for Order Book Dynamics*
- Avellaneda & Stoikov (2008) — *High-frequency Trading in a Limit Order Book*

---

## License

This project is for educational and research purposes.
