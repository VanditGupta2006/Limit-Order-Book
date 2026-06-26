"""
LOB Visualiser
==============
Reads trades and snapshots CSVs produced by the C++ LOB simulation
and generates analysis plots saved to the same directory.

Usage:
    python visualise.py --dir data --trades config1_trades.csv --snapshots config1_snapshots.csv
    python visualise.py --dir data --trades config2_trades.csv --snapshots config2_snapshots.csv --show
    python visualise.py --dir data --trades config1_trades.csv --snapshots config1_snapshots.csv --animate

Arguments:
    --dir         Directory containing the CSV files (outputs saved here too)
    --trades      Trades CSV filename         (default: trades.csv)
    --snapshots   Snapshots CSV filename      (default: snapshots.csv)
    --show        Open interactive matplotlib window after saving
    --animate     Also generate an animated GIF of the LOB (slower)

Expected CSV schemas:
    trades:     timestamp, price, quantity, buyerIsMaker, takerTraderId, makerTraderId
    snapshots:  timestamp, bestBid, bestAsk, mid, spread, bidDepth, askDepth
"""

import sys
import os
import argparse
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
from scipy import stats


# =============================================================================
# Argument parsing
# =============================================================================
def parse_args():
    parser = argparse.ArgumentParser(
        description="LOB Simulation Visualiser — reads CSVs, outputs analysis plots"
    )
    parser.add_argument(
        "--dir", type=str, required=True,
        help="Directory containing the input CSV files (outputs are saved here too)"
    )
    parser.add_argument(
        "--trades", type=str, default="trades.csv",
        help="Trades CSV filename (default: trades.csv)"
    )
    parser.add_argument(
        "--snapshots", type=str, default="snapshots.csv",
        help="Snapshots CSV filename (default: snapshots.csv)"
    )
    parser.add_argument(
        "--show", action="store_true",
        help="Open interactive matplotlib window after saving"
    )
    parser.add_argument(
        "--animate", action="store_true",
        help="Generate animated GIF of price evolution (slower)"
    )
    return parser.parse_args()


# =============================================================================
# Colour palette
# =============================================================================
BID_COLOR   = "#26a69a"   # teal
ASK_COLOR   = "#ef5350"   # red
MID_COLOR   = "#1565c0"   # dark blue
TRADE_COLOR = "#ff6f00"   # amber
SPREAD_COLOR = "#7e57c2"  # purple
VOL_COLOR   = "#43a047"   # green
BG_COLOR    = "#fafafa"
GRID_COLOR  = "#e0e0e0"


# =============================================================================
# Data loading
# =============================================================================
def load_data(data_dir, trades_file, snapshots_file):
    trades_path = os.path.join(data_dir, trades_file)
    snaps_path  = os.path.join(data_dir, snapshots_file)

    if not os.path.isfile(trades_path):
        print(f"Error: trades file not found: {trades_path}")
        sys.exit(1)
    if not os.path.isfile(snaps_path):
        print(f"Error: snapshots file not found: {snaps_path}")
        sys.exit(1)

    trades = pd.read_csv(trades_path)
    snaps  = pd.read_csv(snaps_path)

    print(f"Loaded {len(trades):,} trades from {trades_path}")
    print(f"Loaded {len(snaps):,} snapshots from {snaps_path}")
    return trades, snaps


# =============================================================================
# Derived features
# =============================================================================
def compute_features(trades, snaps):
    """Compute returns, rolling stats, and trade aggregates."""
    features = {}

    # --- Mid-price returns (tick-to-tick log returns) ---
    valid_mid = snaps[snaps["mid"] > 0]["mid"].values
    if len(valid_mid) > 1:
        log_returns = np.diff(np.log(valid_mid))
        features["log_returns"] = log_returns
    else:
        features["log_returns"] = np.array([])

    # --- Rolling spread ---
    features["spread"] = snaps["spread"].values

    # --- Trade volume per tick ---
    vol_per_tick = trades.groupby("timestamp")["quantity"].sum()
    features["vol_ticks"]  = vol_per_tick.index.values
    features["vol_values"] = vol_per_tick.values

    # --- Cumulative volume ---
    features["cum_volume"] = trades["quantity"].cumsum().values

    # --- Trade prices ---
    features["trade_times"]  = trades["timestamp"].values
    features["trade_prices"] = trades["price"].values

    # --- Bid/ask depth ---
    features["bid_depth"] = snaps["bidDepth"].values
    features["ask_depth"] = snaps["askDepth"].values

    return features


# =============================================================================
# Plot 1: Price Path + Spread  (2 subplots stacked)
# =============================================================================
def plot_price_and_spread(snaps, trades, features, output_dir, prefix):
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(14, 8), facecolor=BG_COLOR,
                                    gridspec_kw={"height_ratios": [3, 1]},
                                    sharex=True)
    fig.suptitle("Price Path & Spread", fontsize=14, fontweight="bold", y=0.95)

    time = snaps["timestamp"].values

    # --- Top: mid price + trade scatter ---
    ax1.set_facecolor(BG_COLOR)
    valid = snaps["mid"] > 0
    ax1.plot(time[valid], snaps["mid"].values[valid],
             color=MID_COLOR, linewidth=0.8, label="Mid price", zorder=3)

    # Subsample trades for scatter (too many = visual noise)
    n_trades = len(trades)
    sample_n = min(n_trades, 2000)
    if n_trades > 0:
        idx = np.linspace(0, n_trades - 1, sample_n, dtype=int)
        ax1.scatter(trades["timestamp"].values[idx], trades["price"].values[idx],
                    s=3, color=TRADE_COLOR, alpha=0.3, zorder=2, label="Trades")

    # Best bid/ask bands
    bid_valid = snaps["bestBid"] > 0
    ask_valid = snaps["bestAsk"] > 0
    ax1.fill_between(time[bid_valid & ask_valid],
                     snaps["bestBid"].values[bid_valid & ask_valid],
                     snaps["bestAsk"].values[bid_valid & ask_valid],
                     alpha=0.12, color=MID_COLOR, label="Bid-Ask band")

    ax1.set_ylabel("Price", fontsize=10)
    ax1.legend(fontsize=8, loc="upper left")
    ax1.grid(True, alpha=0.3, color=GRID_COLOR)
    ax1.spines[["top", "right"]].set_visible(False)

    # --- Bottom: spread ---
    ax2.set_facecolor(BG_COLOR)
    spread_valid = snaps["spread"] > 0
    ax2.fill_between(time[spread_valid], 0,
                     snaps["spread"].values[spread_valid],
                     alpha=0.5, color=SPREAD_COLOR)
    ax2.plot(time[spread_valid], snaps["spread"].values[spread_valid],
             color=SPREAD_COLOR, linewidth=0.5)

    mean_spread = snaps.loc[spread_valid, "spread"].mean()
    ax2.axhline(mean_spread, color=SPREAD_COLOR, linestyle="--",
                linewidth=1, alpha=0.7, label=f"Mean: {mean_spread:.4f}")

    ax2.set_ylabel("Spread", fontsize=10)
    ax2.set_xlabel("Tick", fontsize=10)
    ax2.legend(fontsize=8)
    ax2.grid(True, alpha=0.3, color=GRID_COLOR)
    ax2.spines[["top", "right"]].set_visible(False)

    plt.tight_layout()
    out_path = os.path.join(output_dir, f"{prefix}_price_spread.png")
    plt.savefig(out_path, dpi=150, bbox_inches="tight")
    print(f"  Saved: {out_path}")
    return fig


# =============================================================================
# Plot 2: Return Distribution + QQ plot  (stylised facts)
# =============================================================================
def plot_return_distribution(features, output_dir, prefix):
    log_ret = features["log_returns"]
    if len(log_ret) < 10:
        print("  Skipping return distribution (not enough data)")
        return None

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(13, 5), facecolor=BG_COLOR)
    fig.suptitle("Return Distribution — Fat Tails Check",
                 fontsize=14, fontweight="bold", y=0.98)

    # --- Left: histogram vs normal ---
    ax1.set_facecolor(BG_COLOR)
    ax1.hist(log_ret, bins=80, density=True, alpha=0.7,
             color=MID_COLOR, edgecolor="white", linewidth=0.3, label="Empirical")

    # Overlay fitted normal
    mu, sigma = np.mean(log_ret), np.std(log_ret)
    x = np.linspace(mu - 5 * sigma, mu + 5 * sigma, 300)
    ax1.plot(x, stats.norm.pdf(x, mu, sigma),
             color=ASK_COLOR, linewidth=2, label="Normal fit")

    kurt = stats.kurtosis(log_ret)
    skew = stats.skew(log_ret)
    ax1.set_title(f"Kurtosis: {kurt:.2f}  |  Skew: {skew:.3f}", fontsize=10)
    ax1.set_xlabel("Log return", fontsize=10)
    ax1.set_ylabel("Density", fontsize=10)
    ax1.legend(fontsize=8)
    ax1.spines[["top", "right"]].set_visible(False)

    # --- Right: QQ plot ---
    ax2.set_facecolor(BG_COLOR)
    stats.probplot(log_ret, dist="norm", plot=ax2)
    ax2.set_title("QQ Plot (vs Normal)", fontsize=10)
    ax2.get_lines()[0].set(markersize=2, color=MID_COLOR, alpha=0.5)
    ax2.get_lines()[1].set(color=ASK_COLOR, linewidth=1.5)
    ax2.spines[["top", "right"]].set_visible(False)

    plt.tight_layout()
    out_path = os.path.join(output_dir, f"{prefix}_returns.png")
    plt.savefig(out_path, dpi=150, bbox_inches="tight")
    print(f"  Saved: {out_path}")
    return fig


# =============================================================================
# Plot 3: Volume & Depth  (trade volume per tick + book depth)
# =============================================================================
def plot_volume_and_depth(snaps, features, output_dir, prefix):
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(14, 7), facecolor=BG_COLOR,
                                    sharex=True)
    fig.suptitle("Volume & Book Depth", fontsize=14, fontweight="bold", y=0.95)

    time = snaps["timestamp"].values

    # --- Top: trade volume per tick (bar chart) ---
    ax1.set_facecolor(BG_COLOR)
    if len(features["vol_ticks"]) > 0:
        # Bin into wider bars if too many ticks
        n_bins = min(500, len(features["vol_ticks"]))
        if len(features["vol_ticks"]) > n_bins:
            bins = np.array_split(features["vol_ticks"], n_bins)
            vals = np.array_split(features["vol_values"], n_bins)
            bar_x = [b.mean() for b in bins]
            bar_h = [v.sum() for v in vals]
        else:
            bar_x = features["vol_ticks"]
            bar_h = features["vol_values"]

        ax1.bar(bar_x, bar_h, width=max(1, time[-1] / len(bar_x) * 0.8),
                color=VOL_COLOR, alpha=0.7, edgecolor="none")

    ax1.set_ylabel("Trade volume", fontsize=10)
    ax1.grid(True, alpha=0.3, color=GRID_COLOR)
    ax1.spines[["top", "right"]].set_visible(False)

    # --- Bottom: bid/ask depth ---
    ax2.set_facecolor(BG_COLOR)
    ax2.fill_between(time, 0, features["bid_depth"],
                     alpha=0.5, color=BID_COLOR, label="Bid depth")
    ax2.fill_between(time, 0, -features["ask_depth"].astype(float),
                     alpha=0.5, color=ASK_COLOR, label="Ask depth")
    ax2.axhline(0, color="#666", linewidth=0.5)

    ax2.set_ylabel("Depth (bid +, ask −)", fontsize=10)
    ax2.set_xlabel("Tick", fontsize=10)
    ax2.legend(fontsize=8)
    ax2.grid(True, alpha=0.3, color=GRID_COLOR)
    ax2.spines[["top", "right"]].set_visible(False)

    plt.tight_layout()
    out_path = os.path.join(output_dir, f"{prefix}_volume_depth.png")
    plt.savefig(out_path, dpi=150, bbox_inches="tight")
    print(f"  Saved: {out_path}")
    return fig


# =============================================================================
# Plot 4: Summary statistics panel
# =============================================================================
def plot_summary(snaps, trades, features, output_dir, prefix):
    fig, axes = plt.subplots(2, 2, figsize=(13, 8), facecolor=BG_COLOR)
    fig.suptitle("Simulation Summary", fontsize=14, fontweight="bold", y=0.98)

    time = snaps["timestamp"].values
    valid = snaps["mid"] > 0

    # --- (0,0) Mid price ---
    ax = axes[0, 0]
    ax.set_facecolor(BG_COLOR)
    ax.plot(time[valid], snaps["mid"].values[valid],
            color=MID_COLOR, linewidth=0.6)
    ax.set_title("Mid Price", fontsize=10)
    ax.set_ylabel("Price")
    ax.grid(True, alpha=0.3, color=GRID_COLOR)
    ax.spines[["top", "right"]].set_visible(False)

    # --- (0,1) Spread ---
    ax = axes[0, 1]
    ax.set_facecolor(BG_COLOR)
    spread_valid = snaps["spread"] > 0
    ax.plot(time[spread_valid], snaps["spread"].values[spread_valid],
            color=SPREAD_COLOR, linewidth=0.5, alpha=0.7)
    # Rolling mean
    window = min(100, len(snaps) // 10)
    if window > 1:
        rolling = snaps["spread"].rolling(window, min_periods=1).mean()
        ax.plot(time, rolling.values, color=SPREAD_COLOR, linewidth=1.5,
                label=f"Rolling {window}-tick mean")
        ax.legend(fontsize=7)
    ax.set_title("Spread", fontsize=10)
    ax.set_ylabel("Spread")
    ax.grid(True, alpha=0.3, color=GRID_COLOR)
    ax.spines[["top", "right"]].set_visible(False)

    # --- (1,0) Return autocorrelation ---
    ax = axes[1, 0]
    ax.set_facecolor(BG_COLOR)
    log_ret = features["log_returns"]
    if len(log_ret) > 50:
        max_lag = min(50, len(log_ret) // 5)
        acf = [np.corrcoef(log_ret[:-lag], log_ret[lag:])[0, 1]
               for lag in range(1, max_lag + 1)]
        ax.bar(range(1, max_lag + 1), acf, color=MID_COLOR, alpha=0.7)
        # Confidence band (approximate)
        conf = 1.96 / np.sqrt(len(log_ret))
        ax.axhline(conf, color=ASK_COLOR, linestyle="--", linewidth=0.8, alpha=0.6)
        ax.axhline(-conf, color=ASK_COLOR, linestyle="--", linewidth=0.8, alpha=0.6)
    ax.set_title("Return Autocorrelation", fontsize=10)
    ax.set_xlabel("Lag")
    ax.set_ylabel("ACF")
    ax.grid(True, alpha=0.3, color=GRID_COLOR)
    ax.spines[["top", "right"]].set_visible(False)

    # --- (1,1) Stats table ---
    ax = axes[1, 1]
    ax.set_facecolor(BG_COLOR)
    ax.axis("off")
    ax.set_title("Key Statistics", fontsize=10)

    total_ticks   = len(snaps)
    total_trades  = len(trades)
    total_vol     = trades["quantity"].sum() if len(trades) > 0 else 0
    final_mid     = snaps.loc[valid, "mid"].iloc[-1] if valid.any() else "N/A"
    avg_spread    = snaps.loc[snaps["spread"] > 0, "spread"].mean()
    median_spread = snaps.loc[snaps["spread"] > 0, "spread"].median()
    kurt_val      = stats.kurtosis(log_ret) if len(log_ret) > 10 else "N/A"

    rows = [
        ["Total ticks",      f"{total_ticks:,}"],
        ["Total trades",     f"{total_trades:,}"],
        ["Total volume",     f"{total_vol:,}"],
        ["Final mid price",  f"{final_mid:.4f}" if isinstance(final_mid, float) else final_mid],
        ["Avg spread",       f"{avg_spread:.4f}" if not np.isnan(avg_spread) else "N/A"],
        ["Median spread",    f"{median_spread:.4f}" if not np.isnan(median_spread) else "N/A"],
        ["Return kurtosis",  f"{kurt_val:.2f}" if isinstance(kurt_val, float) else kurt_val],
    ]

    tbl = ax.table(cellText=rows, colLabels=["Metric", "Value"],
                   loc="center", cellLoc="center")
    tbl.auto_set_font_size(False)
    tbl.set_fontsize(9)
    tbl.scale(1, 1.6)

    for (r, c), cell in tbl.get_celld().items():
        cell.set_edgecolor("#cccccc")
        if r == 0:
            cell.set_facecolor("#37474f")
            cell.set_text_props(color="white", fontweight="bold")
        elif r % 2 == 0:
            cell.set_facecolor("#f5f5f5")
        else:
            cell.set_facecolor("white")

    plt.tight_layout()
    out_path = os.path.join(output_dir, f"{prefix}_summary.png")
    plt.savefig(out_path, dpi=150, bbox_inches="tight")
    print(f"  Saved: {out_path}")
    return fig


# =============================================================================
# Optional: animated price path GIF
# =============================================================================
def generate_animation(snaps, trades, output_dir, prefix):
    import matplotlib.animation as anim

    fig, ax = plt.subplots(figsize=(12, 5), facecolor=BG_COLOR)
    ax.set_facecolor(BG_COLOR)
    ax.set_xlabel("Tick", fontsize=10)
    ax.set_ylabel("Price", fontsize=10)
    ax.set_title("LOB — Live Price Path", fontsize=12, fontweight="bold")
    ax.spines[["top", "right"]].set_visible(False)
    ax.grid(True, alpha=0.3, color=GRID_COLOR)

    valid = snaps[snaps["mid"] > 0]
    time_vals = valid["timestamp"].values
    mid_vals  = valid["mid"].values
    bid_vals  = valid["bestBid"].values
    ask_vals  = valid["bestAsk"].values

    # Target ~150 frames
    step = max(1, len(time_vals) // 150)
    frame_idx = list(range(0, len(time_vals), step))

    line_mid, = ax.plot([], [], color=MID_COLOR, linewidth=1.2, label="Mid")
    line_bid, = ax.plot([], [], color=BID_COLOR, linewidth=0.6, alpha=0.6, label="Best Bid")
    line_ask, = ax.plot([], [], color=ASK_COLOR, linewidth=0.6, alpha=0.6, label="Best Ask")
    ax.legend(fontsize=8, loc="upper left")

    ax.set_xlim(time_vals[0], time_vals[-1])
    price_min = min(mid_vals.min(), bid_vals[bid_vals > 0].min()) * 0.998
    price_max = max(mid_vals.max(), ask_vals[ask_vals > 0].max()) * 1.002
    ax.set_ylim(price_min, price_max)

    def animate(i):
        end = frame_idx[i] + 1
        line_mid.set_data(time_vals[:end], mid_vals[:end])
        line_bid.set_data(time_vals[:end], bid_vals[:end])
        line_ask.set_data(time_vals[:end], ask_vals[:end])
        return [line_mid, line_bid, line_ask]

    ani = anim.FuncAnimation(fig, animate, frames=len(frame_idx),
                             interval=60, blit=True)

    out_path = os.path.join(output_dir, f"{prefix}_animation.gif")
    print(f"  Generating animation ({len(frame_idx)} frames)...")
    ani.save(out_path, writer="pillow", fps=15, dpi=100)
    print(f"  Saved: {out_path}")
    return fig


# =============================================================================
# Main
# =============================================================================
def main():
    args = parse_args()

    data_dir       = args.dir
    trades_file    = args.trades
    snapshots_file = args.snapshots

    # Derive output prefix from trades filename (e.g. "config1_trades.csv" → "config1")
    base = os.path.splitext(trades_file)[0]
    if base.endswith("_trades"):
        prefix = base[:-7]    # strip "_trades"
    else:
        prefix = base

    print(f"\n{'=' * 60}")
    print(f"  LOB Visualiser")
    print(f"  Directory : {os.path.abspath(data_dir)}")
    print(f"  Trades    : {trades_file}")
    print(f"  Snapshots : {snapshots_file}")
    print(f"  Prefix    : {prefix}")
    print(f"{'=' * 60}\n")

    # Load
    trades, snaps = load_data(data_dir, trades_file, snapshots_file)
    features = compute_features(trades, snaps)

    # Generate plots
    figs = []
    print("\nGenerating plots...")

    figs.append(plot_price_and_spread(snaps, trades, features, data_dir, prefix))
    figs.append(plot_return_distribution(features, data_dir, prefix))
    figs.append(plot_volume_and_depth(snaps, features, data_dir, prefix))
    figs.append(plot_summary(snaps, trades, features, data_dir, prefix))

    if args.animate:
        figs.append(generate_animation(snaps, trades, data_dir, prefix))

    print(f"\nAll outputs saved to: {os.path.abspath(data_dir)}/")

    if args.show:
        plt.show()
    else:
        plt.close("all")


if __name__ == "__main__":
    main()