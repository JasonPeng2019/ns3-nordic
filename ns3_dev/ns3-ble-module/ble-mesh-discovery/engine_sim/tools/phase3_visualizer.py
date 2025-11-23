#!/usr/bin/env python3
"""
Matplotlib-based visualizer for phase3-discovery-engine-sim traces.

Generate a trace with:
  ./waf --run "phase3-discovery-engine-sim --trace=phase3_trace.csv"

Then view it with:
  ./engine_sim/tools/phase3_visualizer.py --trace phase3_trace.csv
"""

import argparse
import csv
from pathlib import Path

import matplotlib.pyplot as plt
from matplotlib import animation

# BLE mesh node state constants (must match ble_mesh_node_state_t)
STATE_NAMES = {
    0: "INIT",
    1: "DISCOVERY",
    2: "EDGE",
    3: "CANDIDATE",
    4: "CLUSTERHEAD",
    5: "MEMBER",
}

STATE_COLORS = {
    "INIT": "#9e9e9e",
    "DISCOVERY": "#90caf9",
    "EDGE": "#4db6ac",
    "CANDIDATE": "#ffb74d",
    "CLUSTERHEAD": "#ef5350",
    "MEMBER": "#9575cd",
}


def load_trace(path):
    positions = {}
    events = []
    final_states = {}

    with open(path, newline="") as f:
        reader = csv.reader(f)
        for row in reader:
            if not row or row[0].startswith("#"):
                continue
            if row[0] == "P":
                _, nid, x, y = row
                positions[int(nid)] = (float(x), float(y))
            elif row[0] == "T":
                (
                    _,
                    t_ms,
                    src,
                    dst,
                    rssi,
                    ttl,
                    path_len,
                    is_election,
                    is_cluster,
                ) = row
                events.append(
                    {
                        "time": float(t_ms) / 1000.0,
                        "src": int(src),
                        "dst": int(dst),
                        "rssi": int(rssi),
                        "ttl": int(ttl),
                        "path": int(path_len),
                        "election": is_election == "1",
                        "cluster": is_cluster == "1",
                    }
                )
            elif row[0] == "S":
                _, nid, state, clusterhead = row
                final_states[int(nid)] = {
                    "state": int(state),
                    "clusterhead": int(clusterhead),
                }

    events.sort(key=lambda e: e["time"])
    return positions, events, final_states


def build_animation(positions, events, final_states, interval_ms):
    node_ids = sorted(positions.keys())
    xs = [positions[n][0] for n in node_ids]
    ys = [positions[n][1] for n in node_ids]

    def base_colors():
        colors = []
        for n in node_ids:
            state = STATE_NAMES.get(final_states.get(n, {}).get("state", 1), "DISCOVERY")
            colors.append(STATE_COLORS.get(state, "#90caf9"))
        return colors

    colors = base_colors()

    fig, ax = plt.subplots(figsize=(8, 8))
    scatter = ax.scatter(xs, ys, c=colors, s=80, zorder=3)
    text_labels = []
    for x, y, nid in zip(xs, ys, node_ids):
        text_labels.append(ax.text(x, y + 3, str(nid), ha="center", fontsize=8))

    line, = ax.plot([], [], "r-", lw=1.5, alpha=0.7, zorder=2)
    ax.set_title("Phase 3 BLE Mesh Discovery")
    ax.set_xlim(min(xs) - 10, max(xs) + 10)
    ax.set_ylim(min(ys) - 10, max(ys) + 10)
    ax.set_aspect("equal", adjustable="box")
    time_text = ax.text(0.02, 0.95, "", transform=ax.transAxes)

    def init():
        line.set_data([], [])
        time_text.set_text("")
        scatter.set_color(base_colors())
        return scatter, line, time_text

    def update(frame_idx):
        if not events:
            return scatter, line, time_text

        e = events[frame_idx % len(events)]
        src = e["src"]
        dst = e["dst"]
        sx, sy = positions[src]
        dx, dy = positions[dst]

        frame_colors = base_colors()
        for idx, nid in enumerate(node_ids):
            if nid == src:
                frame_colors[idx] = "#ffeb3b"
            elif nid == dst:
                frame_colors[idx] = "#81d4fa"
        scatter.set_color(frame_colors)

        line.set_data([sx, dx], [sy, dy])
        time_text.set_text(f"t={e['time']:.3f}s  src={src} dst={dst} ttl={e['ttl']}")
        return scatter, line, time_text

    frames = max(len(events), 1)
    anim = animation.FuncAnimation(
        fig,
        update,
        init_func=init,
        frames=frames,
        interval=interval_ms,
        blit=True,
        repeat=len(events) > 0,
    )
    return anim


def main():
    parser = argparse.ArgumentParser(description="Visualize Phase 3 discovery trace")
    parser.add_argument("--trace", required=True, help="Trace CSV from phase3-discovery-engine-sim")
    parser.add_argument("--interval-ms", type=int, default=80, help="Frame interval for animation")
    args = parser.parse_args()

    trace_path = Path(args.trace)
    if not trace_path.exists():
        raise SystemExit(f"Trace file not found: {trace_path}")

    positions, events, final_states = load_trace(trace_path)
    anim = build_animation(positions, events, final_states, args.interval_ms)
    plt.show()


if __name__ == "__main__":
    main()
