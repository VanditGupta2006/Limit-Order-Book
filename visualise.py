"""
LOB Visualiser
--------------
Reads trades.csv and snapshots.csv produced by the C++ simulation
and renders three views:

  1. Animated LOB depth — bid/ask bars updating each tick (like the image)
  2. Price time-series  — mid price + last trade price over time
  3. Static summary     — final state of all three panels side by side

Run:
    python visualise.py                  # saves animation + static PNG
    python visualise.py --show           # opens interactive window too
"""

import sys
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import matplotlib.gridspec as gridspec
from matplotlib.patches import FancyBboxPatch
import numpy as np

# -------------------------------------------------------
# Load data
# -------------------------------------------------------
trades    = pd.read_csv("./Data/trades.csv")
snapshots = pd.read_csv("./Data/snapshots.csv")

ticks = sorted(snapshots["timestamp"].unique())
MAX_DEPTH = 5

# -------------------------------------------------------
# Compute mid price per tick from snapshots
# -------------------------------------------------------
def mid_at(t):
    s = snapshots[snapshots["timestamp"] == t]
    bids = s[s["side"] == "bid"]["price"]
    asks = s[s["side"] == "ask"]["price"]
    if bids.empty or asks.empty:
        return np.nan
    return (bids.max() + asks.min()) / 2.0

mids = pd.Series({t: mid_at(t) for t in ticks}).ffill()

# Last trade price up to each tick
def last_trade_at(t):
    past = trades[trades["timestamp"] <= t]
    return past["price"].iloc[-1] if not past.empty else np.nan

last_trade = pd.Series({t: last_trade_at(t) for t in ticks})

# -------------------------------------------------------
# Colour palette — matches the image style
# -------------------------------------------------------
BID_COLOR  = "#26a69a"   # teal green
ASK_COLOR  = "#ef5350"   # red
MID_COLOR  = "#1565c0"   # dark blue
TRADE_COLOR = "#ff6f00"  # amber
BG_COLOR   = "#f5f5f5"

# -------------------------------------------------------
# Build figure — 3 panels
# -------------------------------------------------------
fig = plt.figure(figsize=(14, 8), facecolor=BG_COLOR)
gs  = gridspec.GridSpec(2, 2, figure=fig,
                        left=0.06, right=0.97,
                        top=0.93,  bottom=0.08,
                        hspace=0.35, wspace=0.3)

ax_depth  = fig.add_subplot(gs[:, 0])   # left: full height LOB depth
ax_price  = fig.add_subplot(gs[0, 1])   # top right: price series
ax_table  = fig.add_subplot(gs[1, 1])   # bottom right: level table
ax_table.axis("off")

fig.suptitle("LOB Simulation — real-time view", fontsize=13, color="#222")

# -------------------------------------------------------
# Helper: draw the LOB depth bar chart for tick t
# -------------------------------------------------------
def draw_depth(ax, t):
    ax.cla()
    ax.set_facecolor(BG_COLOR)
    ax.set_title(f"Order book depth  (t={t})", fontsize=10, pad=4)
    ax.set_xlabel("Quantity", fontsize=9)
    ax.set_ylabel("Price", fontsize=9)

    s    = snapshots[snapshots["timestamp"] == t]
    bids = s[s["side"] == "bid"].sort_values("price", ascending=False).head(MAX_DEPTH)
    asks = s[s["side"] == "ask"].sort_values("price").head(MAX_DEPTH)

    if not bids.empty:
        ax.barh(bids["price"], bids["quantity"],
                color=BID_COLOR, alpha=0.8, height=0.06, label="Bid")
    if not asks.empty:
        ax.barh(asks["price"], asks["quantity"],
                color=ASK_COLOR, alpha=0.8, height=0.06, label="Ask")

    # Mid price line
    m = mids.get(t, np.nan)
    if not np.isnan(m):
        ax.axhline(m, color=MID_COLOR, linewidth=1.2,
                   linestyle="--", alpha=0.7, label=f"Mid {m:.3f}")

    # Last trade marker
    lt = last_trade.get(t, np.nan)
    if not np.isnan(lt):
        ax.axhline(lt, color=TRADE_COLOR, linewidth=1,
                   linestyle=":", alpha=0.9, label=f"Last {lt:.3f}")

    ax.legend(fontsize=7, loc="lower right")
    ax.tick_params(labelsize=8)
    ax.spines[["top","right"]].set_visible(False)

# -------------------------------------------------------
# Helper: draw the price time-series up to tick t
# -------------------------------------------------------
def draw_price(ax, t):
    ax.cla()
    ax.set_facecolor(BG_COLOR)
    ax.set_title("Mid price & last trade", fontsize=10, pad=4)
    ax.set_xlabel("Tick", fontsize=9)
    ax.set_ylabel("Price", fontsize=9)

    past_ticks = [x for x in ticks if x <= t]
    mid_vals   = [mids.get(x, np.nan)        for x in past_ticks]
    trade_vals = [last_trade.get(x, np.nan)  for x in past_ticks]

    ax.plot(past_ticks, mid_vals,   color=MID_COLOR,   linewidth=1.2,
            label="Mid", zorder=3)
    ax.plot(past_ticks, trade_vals, color=TRADE_COLOR, linewidth=0.8,
            linestyle="--", alpha=0.8, label="Last trade", zorder=2)

    # Mark trades on the chart
    tick_trades = trades[trades["timestamp"] <= t]
    if not tick_trades.empty:
        ax.scatter(tick_trades["timestamp"], tick_trades["price"],
                   s=10, color=TRADE_COLOR, zorder=4, alpha=0.6)

    ax.legend(fontsize=7)
    ax.tick_params(labelsize=8)
    ax.spines[["top","right"]].set_visible(False)

# -------------------------------------------------------
# Helper: draw the level table (mimics the image)
# -------------------------------------------------------
def draw_table(ax, t):
    ax.cla()
    ax.axis("off")
    ax.set_title("Book levels", fontsize=10, pad=4)

    s    = snapshots[snapshots["timestamp"] == t]
    bids = s[s["side"] == "bid"].sort_values("price", ascending=False).head(MAX_DEPTH).reset_index()
    asks = s[s["side"] == "ask"].sort_values("price").head(MAX_DEPTH).reset_index()

    rows = []
    for lvl in range(1, MAX_DEPTH + 1):
        b = bids[bids["level"] == lvl]
        a = asks[asks["level"] == lvl]
        bid_p = f'{b["price"].values[0]:.3f}'   if not b.empty else "—"
        bid_q = f'{int(b["quantity"].values[0])}'if not b.empty else "—"
        ask_p = f'{a["price"].values[0]:.3f}'   if not a.empty else "—"
        ask_q = f'{int(a["quantity"].values[0])}'if not a.empty else "—"
        rows.append([str(lvl), bid_q, bid_p, ask_p, ask_q, str(lvl)])

    cols = ["Lvl", "Bid Qty", "Bid $", "Ask $", "Ask Qty", "Lvl"]
    tbl = ax.table(cellText=rows, colLabels=cols,
                   loc="center", cellLoc="center")
    tbl.auto_set_font_size(False)
    tbl.set_fontsize(8)
    tbl.scale(1, 1.4)

    # Colour header and bid/ask columns
    for (r, c), cell in tbl.get_celld().items():
        cell.set_edgecolor("#cccccc")
        if r == 0:
            cell.set_facecolor("#37474f")
            cell.set_text_props(color="white", fontweight="bold")
        elif c in (1, 2):   # bid columns
            cell.set_facecolor("#e0f2f1")
        elif c in (3, 4):   # ask columns
            cell.set_facecolor("#fce4ec")
        else:
            cell.set_facecolor("#f5f5f5")

# -------------------------------------------------------
# Draw static summary at final tick → save as PNG
# -------------------------------------------------------
final_t = ticks[-1]
draw_depth(ax_depth, final_t)
draw_price(ax_price, final_t)
draw_table(ax_table, final_t)

plt.savefig("./Data/lob_summary.png", dpi=150, bbox_inches="tight")
print("Saved lob_summary.png")

# -------------------------------------------------------
# Animation — step through every tick
# -------------------------------------------------------
# Use every Nth tick so the animation isn't too slow
STEP = max(1, len(ticks) // 120)   # ~120 frames target
frame_ticks = ticks[::STEP]

def animate(i):
    t = frame_ticks[i]
    draw_depth(ax_depth, t)
    draw_price(ax_price, t)
    draw_table(ax_table, t)
    return []

ani = animation.FuncAnimation(
    fig, animate,
    frames=len(frame_ticks),
    interval=80,       # ms between frames
    blit=False
)

ani.save("./Data/lob_animation.gif", writer="pillow", fps=12, dpi=100)
print("Saved lob_animation.gif")

if "--show" in sys.argv:
    plt.show()