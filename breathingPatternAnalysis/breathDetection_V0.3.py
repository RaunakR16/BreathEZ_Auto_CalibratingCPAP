import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
from scipy.signal import find_peaks, peak_prominences
from datetime import datetime

# ── CONFIG ─────────────────────────────────────────────────────────────────────
CSV_FILE = r"breathingPatternAnalysis\DATA\sensor_log1.csv"   # change to your file path (use r"..." on Windows)

# Baseline removal
# A large moving-average window (~10 s worth of samples) captures slow drift
# but ignores the fast breath-cycle oscillations we want to detect.
BASELINE_WINDOW = 100   # samples — increase if signal drifts very slowly

# Peak detection on the DETRENDED signal (values will be small, e.g. ±30 ADC)
DISTANCE   = 10    # min samples between two peaks  (~1 s at 10 Hz)
PROMINENCE =  5    # min ADC units above surroundings on the detrended signal
                   # much lower than before because drift is removed
                   # raise to ignore noise, lower to catch weak exhales
# ───────────────────────────────────────────────────────────────────────────────


# ── TIMESTAMP PARSER ───────────────────────────────────────────────────────────
_TS_FORMATS = [
    "%H:%M:%S.%f",   # 13:21:49.926318
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
        f"Timestamp '{ts}' did not match any known format.\nTried: {_TS_FORMATS}"
    )


def load_data(path: str) -> pd.DataFrame:
    df = pd.read_csv(path, names=["Timestamp", "Raw_Value", "Filtered_Value"], skiprows=1)
    df["Raw_Value"]      = pd.to_numeric(df["Raw_Value"].astype(str).str.strip())
    df["Filtered_Value"] = pd.to_numeric(df["Filtered_Value"].astype(str).str.strip())
    df["_dt"]       = df["Timestamp"].apply(_parse_ts)
    t0              = df["_dt"].iloc[0]
    df["Elapsed_s"] = df["_dt"].apply(lambda t: (t - t0).total_seconds())
    return df


# ── DETREND ────────────────────────────────────────────────────────────────────
def detrend(signal: np.ndarray, window: int) -> tuple[np.ndarray, np.ndarray]:
    """
    Returns (detrended, baseline).
    baseline  = slow moving average that captures long-term drift.
    detrended = signal - baseline  →  fast oscillations centred on 0.

    Edge handling: the rolling mean is back-filled at the start so no NaNs.
    """
    s = pd.Series(signal)
    baseline = s.rolling(window=window, min_periods=1, center=True).mean().values
    return (signal - baseline).astype(float), baseline


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
        bpm          = (n - 1) / ((peak_times[-1] - peak_times[0]) / 60)
    else:
        avg_interval = std_interval = min_interval = max_interval = float("nan")
        bpm          = float("nan")

    return {
        "exhale_count":   n,
        "bpm":            bpm,
        "avg_interval_s": avg_interval,
        "std_interval_s": std_interval,
        "min_interval_s": min_interval,
        "max_interval_s": max_interval,
        "total_sec":      total_sec,
    }


# ── PLOT ───────────────────────────────────────────────────────────────────────
# ── PLOT ───────────────────────────────────────────────────────────────────────
def plot(df: pd.DataFrame,
         peaks: np.ndarray,
         prominences: np.ndarray,
         detrended: np.ndarray,
         baseline: np.ndarray,
         stats: dict) -> None:

    fig, ax1 = plt.subplots(figsize=(16, 9))
    fig.patch.set_facecolor("#ffffff")

    t = df["_dt"]

    # ── Main plot ─────────────────────────────────────────────────────────────
    ax1.set_facecolor("#f9f9f9")

    ax1.plot(
        t,
        df["Raw_Value"],
        color="#378ADD",
        lw=0.9,
        alpha=0.75,
        label="Raw ADC"
    )

    ax1.plot(
        t,
        df["Filtered_Value"],
        color="#E9A020",
        lw=2.0,
        label="Filtered EMA"
    )

    ax1.plot(
        t,
        baseline,
        color="#888888",
        lw=1.2,
        linestyle="--",
        alpha=0.7,
        label="Baseline (drift)"
    )

    ax1.scatter(
        t.iloc[peaks],
        df["Filtered_Value"].iloc[peaks],
        color="#E24B4A",
        edgecolors="#A32D2D",
        s=90,
        zorder=5,
        linewidths=1.5,
        label="Exhale detected"
    )

    for k, i in enumerate(peaks, start=1):
        ax1.annotate(
            f"#{k}",
            xy=(t.iloc[i], df["Filtered_Value"].iloc[i]),
            xytext=(0, 10),
            textcoords="offset points",
            ha="center",
            fontsize=8,
            color="#A32D2D",
            fontweight="bold"
        )

    stats_lines = [
        f"Exhales detected : {stats['exhale_count']}",
        f"Breathing rate   : {stats['bpm']:.1f} BPM",
        f"Avg interval     : {stats['avg_interval_s']:.2f} s",
        f"Std deviation    : {stats['std_interval_s']:.2f} s",
        f"Min interval     : {stats['min_interval_s']:.2f} s",
        f"Max interval     : {stats['max_interval_s']:.2f} s"
    ]

    ax1.text(
        0.01,
        0.97,
        "\n".join(stats_lines),
        transform=ax1.transAxes,
        fontsize=9,
        verticalalignment="top",
        family="monospace",
        bbox=dict(
            boxstyle="round,pad=0.55",
            facecolor="white",
            edgecolor="#cccccc",
            alpha=0.92
        )
    )

    ax1.set_ylabel("Sensor ADC Value", fontsize=11)

    ax1.set_title(
        "Breathing Pattern - Exhale Detection & BPM",
        fontsize=13,
        fontweight="bold",
        pad=10
    )

    ax1.legend(loc="lower right", fontsize=9, framealpha=0.9)

    ax1.grid(
        True,
        color="#cccccc",
        linewidth=0.5,
        linestyle="--",
        alpha=0.7
    )

    ax1.xaxis.set_major_formatter(
        mdates.DateFormatter("%H:%M:%S")
    )

    fig.autofmt_xdate()

    plt.tight_layout()

    plt.savefig(
        "breathing_analysis.png",
        dpi=150,
        bbox_inches="tight"
    )

    print("Plot saved -> breathing_analysis.png")

    plt.show()


# ── MAIN ───────────────────────────────────────────────────────────────────────
def main() -> None:
    df     = load_data(CSV_FILE)
    signal = df["Filtered_Value"].values

    # Step 1 — remove slow baseline drift
    detrended, baseline = detrend(signal, BASELINE_WINDOW)

    # Step 2 — detect peaks on the flat detrended signal
    peaks, _ = find_peaks(detrended, distance=DISTANCE, prominence=PROMINENCE)

    # Per-peak prominences (on detrended, for display/debug)
    prominences, _, _ = peak_prominences(detrended, peaks)

    stats = compute_stats(df, peaks)

    print(f"\n[Detrend window={BASELINE_WINDOW} samples | "
          f"prominence={PROMINENCE} | distance={DISTANCE}]")
    print("=" * 46)
    print(f"  Exhales detected  : {stats['exhale_count']}")
    print(f"  Breathing rate    : {stats['bpm']:.2f} BPM")
    print(f"  Avg interval      : {stats['avg_interval_s']:.2f} s")
    print(f"  Std deviation     : {stats['std_interval_s']:.2f} s")
    print(f"  Min interval      : {stats['min_interval_s']:.2f} s")
    print(f"  Max interval      : {stats['max_interval_s']:.2f} s")
    print(f"  Recording length  : {stats['total_sec']:.1f} s")
    print("=" * 46)

    print("\nExhale timestamps:")
    for k, (i, prom) in enumerate(zip(peaks, prominences), start=1):
        ts  = df["Timestamp"].iloc[i].strip().split(".")[0]
        val = df["Filtered_Value"].iloc[i]
        print(f"  #{k:2d}  {ts}   EMA = {val:.2f}   detrended_prom = {prom:.2f}")

    plot(df, peaks, prominences, detrended, baseline, stats)


if __name__ == "__main__":
    main()