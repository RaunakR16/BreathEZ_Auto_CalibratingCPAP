import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
from scipy.signal import find_peaks
from datetime import datetime, timedelta

# ── CONFIG ─────────────────────────────────────────────────────────────────────
CSV_FILE  = r"breathingPatternAnalysis\DATA\sensor_log2.csv"  # change to your CSV path (use r"..." on Windows)
DISTANCE  = 10                  # min samples between peaks  (~1 s at 10 Hz)
PROMINENCE = 20                 # min prominence in ADC units (raise to be stricter)
# ───────────────────────────────────────────────────────────────────────────────


# ── TIMESTAMP PARSER ───────────────────────────────────────────────────────────
# Tries multiple formats so the script works across all your CSV files.
_TS_FORMATS = [
    "%H:%M:%S.%f",   # 13:21:49.926318  (most files)
    "%H:%M:%S",      # 13:21:49
    "%M:%S.%f",      # 01:32.1
    "%M:%S",         # 01:32
]

def _parse_ts(ts: str) -> datetime:
    ts = ts.strip()
    for fmt in _TS_FORMATS:
        try:
            return datetime.strptime(ts, fmt)
        except ValueError:
            continue
    raise ValueError(
        f"Timestamp '{ts}' did not match any known format.\n"
        f"Tried: {_TS_FORMATS}"
    )


def load_data(path: str) -> pd.DataFrame:
    df = pd.read_csv(path, names=["Timestamp", "Raw_Value", "Filtered_Value"], skiprows=1,)
    df["Raw_Value"]      = pd.to_numeric(df["Raw_Value"].astype(str).str.strip())
    df["Filtered_Value"] = pd.to_numeric(df["Filtered_Value"].astype(str).str.strip())
    df["_dt"] = df["Timestamp"].apply(_parse_ts)

    # Elapsed seconds from first sample (works regardless of what date/hour was chosen)
    t0 = df["_dt"].iloc[0]
    df["Elapsed_s"] = df["_dt"].apply(lambda t: (t - t0).total_seconds())

    return df


# ── STATS ──────────────────────────────────────────────────────────────────────
def compute_stats(df: pd.DataFrame, peaks: np.ndarray) -> dict:
    n         = len(peaks)
    total_sec = df["Elapsed_s"].iloc[-1] - df["Elapsed_s"].iloc[0]

    if n > 1:
        peak_times   = df["Elapsed_s"].values[peaks]
        intervals    = np.diff(peak_times)       
        avg_interval = float(np.mean(intervals))
        std_interval = float(np.std(intervals))
        min_interval = float(np.min(intervals))
        max_interval = float(np.max(intervals))
        # BPM = breaths per minute, using interval between first and last peak
        bpm = (n - 1) / ((peak_times[-1] - peak_times[0]) / 60)
    else:
        avg_interval = std_interval = min_interval = max_interval = float("nan")
        bpm = float("nan")

    return {
        "exhale_count":   n,
        "bpm":            bpm,
        "avg_interval_s": avg_interval,
        "std_interval_s": std_interval,
        "min_interval_s": min_interval,
        "max_interval_s": max_interval
    }


# ── PLOT ───────────────────────────────────────────────────────────────────────
def plot(df: pd.DataFrame, peaks: np.ndarray, stats: dict) -> None:
    fig, ax = plt.subplots(figsize=(16, 6))
    fig.patch.set_facecolor("#ffffff")
    ax.set_facecolor("#f9f9f9")

    t = df["_dt"]

    # Signals
    ax.plot(t, df["Raw_Value"],      color="#0080FF", lw=0.9, alpha=0.75, label="Raw ADC")
    ax.plot(t, df["Filtered_Value"], color="#FFA200", lw=2.0, label="Filtered EMA")

    # Exhale markers
    ax.scatter(t.iloc[peaks], df["Filtered_Value"].iloc[peaks], color="#E24B4A", s=90, zorder=5, linewidths=1.5, label="Exhale detected")

    # Peak index labels
    for k, i in enumerate(peaks, start=1):
        ax.annotate(
            f"#{k}",
            xy=(t.iloc[i], df["Filtered_Value"].iloc[i]),
            xytext=(0, 10), textcoords="offset points",
            ha="center", fontsize=8, color="#A32D2D", fontweight="bold",
        )

    # Stats box
    stats_lines = [
        f"Exhales detected : {stats['exhale_count']}",
        f"Breathing rate   : {stats['bpm']:.1f} BPM",
        f"Avg interval     : {stats['avg_interval_s']:.2f} s",
        f"Std deviation    : {stats['std_interval_s']:.2f} s",
        f"Min interval     : {stats['min_interval_s']:.2f} s",
        f"Max interval     : {stats['max_interval_s']:.2f} s"
    ]
    ax.text(
        0.01, 0.97, "\n".join(stats_lines),
        transform=ax.transAxes, fontsize=9, verticalalignment="top",
        family="monospace",
        bbox=dict(boxstyle="round,pad=0.55", facecolor="white",
                  edgecolor="#cccccc", alpha=0.92),
    )

    # Axes & formatting
    ax.xaxis.set_major_formatter(mdates.DateFormatter("%H:%M:%S"))
    fig.autofmt_xdate()
    ax.set_xlabel("Time", fontsize=16)
    ax.set_ylabel("Sensor ADC Value", fontsize=16)
    ax.grid(True, color="#cccccc", linewidth=0.5, linestyle="--", alpha=0.7)
    ax.legend(loc="lower right", fontsize=9, framealpha=0.9)

    plt.tight_layout()
    plt.savefig("breathing_analysis.png", dpi=150, bbox_inches="tight")
    print("Plot saved -> breathing_analysis.png")
    plt.show()


# ── MAIN ───────────────────────────────────────────────────────────────────────
def main() -> None:
    df = load_data(CSV_FILE)

    # scipy find_peaks on the filtered (EMA) signal — same as your working code
    peaks, properties = find_peaks(
        df["Filtered_Value"],
        distance=DISTANCE,
        prominence=PROMINENCE,
    )

    stats = compute_stats(df, peaks)

    # Print summary
    print("=" * 46)
    print(f"  Exhales detected  : {stats['exhale_count']}")
    print(f"  Breathing rate    : {stats['bpm']:.2f} BPM")
    print(f"  Avg interval      : {stats['avg_interval_s']:.2f} s")
    print(f"  Std deviation     : {stats['std_interval_s']:.2f} s")
    print(f"  Min interval      : {stats['min_interval_s']:.2f} s")
    print(f"  Max interval      : {stats['max_interval_s']:.2f} s")
    print("=" * 46)

    print("\nExhale timestamps:")
    for k, i in enumerate(peaks, start=1):
        ts  = df["Timestamp"].iloc[i].strip().split(".")[0]
        val = df["Filtered_Value"].iloc[i]
        prom = properties["prominences"][k - 1]
        print(f"  #{k:2d}  {ts}   EMA = {val:.2f}   prominence = {prom:.1f}")

    plot(df, peaks, stats)


if __name__ == "__main__":
    main()