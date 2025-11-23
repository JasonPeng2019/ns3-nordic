#!/usr/bin/env python3
"""
Phase 3 discovery visualizer.

Features:
- Static plots (topology, timeline, stats, TTL decay) matching the phase2 visualizer.
- Interactive animation with two sliders:
    * Time slider scrubs through the trace (loops automatically).
    * Speed slider controls playback speed.
- Play/Pause button (looping by default).
- Message colors: origin SEND (self) / forwarded SEND / RECV differ.
- Nodes change color based on election-cycle phase (time-based) and activity.

Usage:
  source ../../../../ns3-venv/bin/activate    # if you use the project venv
  python phase3_visualizer.py --trace ../traces/phase3_trace.csv
"""

import argparse
import atexit
import itertools
import signal
import sys
from pathlib import Path
from typing import Dict, List, Tuple

import matplotlib
# Set backend early to avoid macOS backend crashes - must be before importing pyplot
# Try multiple backends in order of preference, avoiding the unstable macOS native backend
_backend_set = False
for _backend in ['Qt5Agg', 'TkAgg', 'WXAgg', 'Agg']:
    try:
        # Test if the backend module is actually available before setting it
        if _backend == 'Qt5Agg':
            import PyQt5  # noqa
        elif _backend == 'TkAgg':
            import tkinter  # noqa
        elif _backend == 'WXAgg':
            import wx  # noqa
        # If we get here, the backend dependencies are available
        matplotlib.use(_backend)
        _backend_set = True
        print(f"Using matplotlib backend: {_backend}", file=sys.stderr)
        break
    except (ImportError, ModuleNotFoundError):
        continue

if not _backend_set:
    # Fall back to Agg (non-interactive but stable)
    matplotlib.use('Agg')
    print("Warning: No GUI backend available. Using 'Agg' (non-interactive).", file=sys.stderr)
    print("Windows will not be interactive. Install PyQt5 or tkinter for interactive mode.", file=sys.stderr)

import matplotlib.colors as mcolors
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import networkx as nx
import pandas as pd
from matplotlib.animation import FuncAnimation
from matplotlib.collections import LineCollection
from matplotlib.widgets import Button, Slider

# Configuration
ENABLE_FADE = False  # Set to True to enable message fade effect

# Message colors by type
MESSAGE_COLORS = {
    "DISCOVERY": "#42a5f5",
    "ELECTION": "#ec407a",
    "RENOUNCE": "#ef6c00",
    "UNKNOWN": "#9e9e9e",
    # Backward-compat keys used elsewhere in the script
    "send_origin": "#42a5f5",
    "send_fwd": "#ec407a",
    "recv": "#66bb6a",
}

# Base node colors for election-cycle phases (time-based approximation)
PHASE_COLORS = {
    "noise": "#90a4ae",        # early noise sampling
    "neighbor": "#64b5f6",     # neighbor discovery
    "candidate": "#ffb74d",    # candidacy / election prep
    "election": "#f06292",     # election / clusterhead flooding
}

# Activity highlight overrides
ACTIVITY_COLORS = {
    "send": "#fdd835",
    "recv": "#43a047",
}


def load_trace(path: Path) -> pd.DataFrame:
    """Load the CSV trace (phase2/phase3 schema)."""
    df = pd.read_csv(path)
    numeric_cols = [
        "time_ms",
        "sender_id",
        "receiver_id",
        "originator_id",
        "ttl",
        "path_length",
        "rssi",
        "is_renouncement",
        "class_id",
        "direct_connections",
        "score",
        "hash",
        "pdsf",
        "last_pi",
    ]
    for col in numeric_cols:
        if col in df.columns:
            df[col] = pd.to_numeric(df[col], errors="coerce")
    if "message_type" in df.columns:
        df["message_type"] = df["message_type"].fillna("DISCOVERY")
    else:
        df["message_type"] = "DISCOVERY"
    return df


def extract_topology(df: pd.DataFrame) -> nx.Graph:
    """Build a graph from TOPOLOGY events."""
    G = nx.Graph()
    topo = df[df["event"] == "TOPOLOGY"]
    for _, row in topo.iterrows():
        a = int(row["sender_id"])
        b = int(row["receiver_id"])
        G.add_edge(a, b)

    # Add any node mentioned elsewhere (defensive)
    for col in ("sender_id", "receiver_id", "originator_id"):
        if col in df:
            for val in df[col].dropna().astype(int).unique():
                G.add_node(int(val))
    return G


def extract_stats(df: pd.DataFrame) -> pd.DataFrame:
    """Extract STATS events into a stats table."""
    stats_df = df[df["event"] == "STATS"].copy()
    if stats_df.empty:
        return pd.DataFrame()
    stats_df = stats_df.rename(
        columns={
            "sender_id": "node_id",
            "receiver_id": "messages_sent",
            "originator_id": "messages_received",
            "ttl": "messages_forwarded",
            "path_length": "messages_dropped",
        }
    )
    return stats_df[["node_id", "messages_sent", "messages_received", "messages_forwarded", "messages_dropped"]]


def plot_topology(G: nx.Graph, ax: plt.Axes, title: str = "Mesh Topology") -> Dict[int, Tuple[float, float]]:
    pos = nx.spring_layout(G, seed=42, k=2)
    nx.draw_networkx_edges(G, pos, ax=ax, edge_color="gray", width=2, alpha=0.6)
    nx.draw_networkx_nodes(G, pos, ax=ax, node_color="#90caf9", node_size=700, edgecolors="black", linewidths=2)
    nx.draw_networkx_labels(G, pos, ax=ax, font_size=11, font_weight="bold")
    ax.set_title(title)
    ax.axis("off")
    return pos


def plot_packet_timeline(df: pd.DataFrame, ax: plt.Axes):
    send_df = df[df["event"] == "SEND"].copy()
    recv_df = df[df["event"] == "RECV"].copy()

    # Color by originator
    originators = sorted(
        set(send_df["originator_id"].dropna().astype(int).unique())
        | set(recv_df["originator_id"].dropna().astype(int).unique())
    )
    cmap = plt.cm.tab10
    orig_colors = {orig: cmap(i % 10) for i, orig in enumerate(originators)}

    for _, row in send_df.iterrows():
        orig = int(row["originator_id"]) if pd.notna(row["originator_id"]) else int(row["sender_id"])
        marker = ">" if pd.isna(row["path_length"]) or int(row["path_length"]) == 0 else "s"
        size = 110 if marker == ">" else 70
        msg_type = row.get("message_type", "DISCOVERY")
        color = MESSAGE_COLORS.get(str(msg_type).upper(), MESSAGE_COLORS["UNKNOWN"])
        ax.scatter(
            row["time_ms"],
            row["sender_id"],
            c=[color],
            marker=marker,
            s=size,
            edgecolors="black",
            linewidths=0.6,
            alpha=0.9,
        )

    for _, row in recv_df.iterrows():
        msg_type = row.get("message_type", "DISCOVERY")
        color = MESSAGE_COLORS.get(str(msg_type).upper(), MESSAGE_COLORS["UNKNOWN"])
        ax.scatter(
            row["time_ms"],
            row["receiver_id"],
            c=[color],
            marker="o",
            s=55,
            alpha=0.6,
            edgecolors="black",
            linewidths=0.5,
        )

    legend_elements = [
        plt.Line2D([0], [0], marker=">", color="w", markerfacecolor=MESSAGE_COLORS["DISCOVERY"],
                   markersize=10, markeredgecolor="black", label="Discovery SEND"),
        plt.Line2D([0], [0], marker="s", color="w", markerfacecolor=MESSAGE_COLORS["ELECTION"],
                   markersize=8, markeredgecolor="black", label="Election SEND"),
        plt.Line2D([0], [0], marker="s", color="w", markerfacecolor=MESSAGE_COLORS["RENOUNCE"],
                   markersize=8, markeredgecolor="black", label="Renouncement SEND"),
        plt.Line2D([0], [0], marker="o", color="w", markerfacecolor="#66bb6a",
                   markersize=8, markeredgecolor="black", label="RECV"),
    ]
    ax.legend(handles=legend_elements, loc="upper right", fontsize=8)
    ax.set_xlabel("Time (ms)")
    ax.set_ylabel("Node ID")
    ax.set_title("Packet Timeline")
    ax.grid(True, alpha=0.3)


def plot_statistics(stats_df: pd.DataFrame, ax: plt.Axes):
    if stats_df.empty:
        ax.text(0.5, 0.5, "No statistics available", ha="center", va="center")
        ax.set_title("Node Statistics")
        return
    stats_df = stats_df.copy()
    x = stats_df["node_id"].astype(int)
    width = 0.2
    ax.bar(x - 1.5 * width, stats_df["messages_sent"], width, label="Sent", color="steelblue")
    ax.bar(x - 0.5 * width, stats_df["messages_received"], width, label="Received", color="forestgreen")
    ax.bar(x + 0.5 * width, stats_df["messages_forwarded"], width, label="Forwarded", color="orange")
    ax.bar(x + 1.5 * width, stats_df["messages_dropped"], width, label="Dropped", color="crimson")
    ax.set_xlabel("Node ID")
    ax.set_ylabel("Message Count")
    ax.set_title("Per-Node Message Statistics")
    ax.set_xticks(x)
    ax.legend()
    ax.grid(True, alpha=0.3, axis="y")


def plot_ttl_decay(df: pd.DataFrame, ax: plt.Axes):
    send_df = df[df["event"] == "SEND"].copy()
    if send_df.empty:
        ax.text(0.5, 0.5, "No send events", ha="center", va="center")
        return
    originators = sorted(send_df["originator_id"].dropna().unique())
    cmap = plt.cm.tab10
    for i, orig in enumerate(originators):
        subset = send_df[send_df["originator_id"] == orig].sort_values("time_ms")
        ax.scatter(subset["path_length"].fillna(0), subset["ttl"], c=[cmap(i % 10)],
                   label=f"Originator {int(orig)}", alpha=0.7, s=55)
    ax.set_xlabel("Path Length (hops)")
    ax.set_ylabel("TTL (remaining)")
    ax.set_title("TTL Decay vs Path Length")
    ax.legend(fontsize=8)
    ax.grid(True, alpha=0.3)


def create_static_figure(df: pd.DataFrame):
    """Create a 2x2 static dashboard."""
    fig, axes = plt.subplots(2, 2, figsize=(14, 12))
    fig.suptitle("Phase 3 Discovery: Static Views", fontsize=14, fontweight="bold")
    G = extract_topology(df)
    stats_df = extract_stats(df)
    plot_topology(G, axes[0, 0], "Mesh Topology")
    plot_packet_timeline(df, axes[0, 1])
    plot_statistics(stats_df, axes[1, 0])
    plot_ttl_decay(df, axes[1, 1])
    plt.tight_layout()
    return fig


def phase_color(time_ms: float, max_time: float) -> str:
    """Approximate election-cycle phase by relative time."""
    if max_time <= 0:
        return PHASE_COLORS["noise"]
    frac = time_ms / max_time
    if frac < 0.25:
        return PHASE_COLORS["noise"]
    if frac < 0.5:
        return PHASE_COLORS["neighbor"]
    if frac < 0.75:
        return PHASE_COLORS["candidate"]
    return PHASE_COLORS["election"]


def build_animation(df: pd.DataFrame, G: nx.Graph):
    """Interactive animation with time/speed sliders and play/pause."""
    events = df[df["event"].isin(["SEND", "RECV"])].copy()
    if events.empty or G.number_of_nodes() == 0:
        fig = plt.figure(figsize=(9, 9))
        ax = fig.add_axes([0.1, 0.2, 0.8, 0.7])
        ax.text(0.5, 0.5, "No SEND/RECV events found", ha="center", va="center")
        ax.axis("off")
        return fig

    events = events.sort_values("time_ms")
    timestamps: List[float] = sorted(events["time_ms"].unique())
    min_time = min(timestamps)
    max_time = max(timestamps)
    # Base time step (ms) for continuous playback; similar speed as before but fine-grained.
    span = max_time - min_time if max_time > min_time else 1.0
    base_dt = max(1.0, span / 300.0)  # ~300 frames per loop, min 1 ms

    fig = plt.figure(figsize=(10, 9))
    # Store cleanup references
    fig._cleanup_handlers = []

    ax_anim = fig.add_axes([0.08, 0.25, 0.84, 0.7])
    ax_time = fig.add_axes([0.08, 0.15, 0.7, 0.03])
    ax_speed = fig.add_axes([0.08, 0.1, 0.7, 0.03])
    ax_btn = fig.add_axes([0.82, 0.1, 0.1, 0.08])

    pos = nx.spring_layout(G, seed=42, k=2)
    node_ids = sorted(G.nodes())

    base_color = phase_color(0, max_time)
    scatter = ax_anim.scatter(
        [pos[n][0] for n in node_ids],
        [pos[n][1] for n in node_ids],
        c=[base_color] * len(node_ids),
        s=600,
        edgecolors="black",
        linewidths=1.6,
        zorder=3,
    )
    nx.draw_networkx_edges(G, pos, ax=ax_anim, edge_color="lightgray", width=2.2, alpha=0.6)
    nx.draw_networkx_labels(G, pos, ax=ax_anim, font_size=11, font_weight="bold")
    highlight_coll = LineCollection([], colors=[], linewidths=2.8, alpha=0.9, zorder=2.5)
    ax_anim.add_collection(highlight_coll)
    time_text = ax_anim.text(0.02, 0.95, "", transform=ax_anim.transAxes, fontsize=10)

    # Legend for node phase colors + activity overlays
    phase_handles = [
        mpatches.Patch(color=PHASE_COLORS["noise"], label="Noise phase"),
        mpatches.Patch(color=PHASE_COLORS["neighbor"], label="Neighbor phase"),
        mpatches.Patch(color=PHASE_COLORS["candidate"], label="Candidate phase"),
        mpatches.Patch(color=PHASE_COLORS["election"], label="Election phase"),
    ]
    activity_handles = [
        mpatches.Patch(color=ACTIVITY_COLORS["send"], label="Send activity"),
        mpatches.Patch(color=ACTIVITY_COLORS["recv"], label="Receive activity"),
    ]
    ax_anim.legend(handles=phase_handles + activity_handles,
                   loc="upper right",
                   fontsize=8,
                   framealpha=0.85,
                   title="Node color guide",
                   title_fontsize=9)

    ax_anim.set_axis_off()
    ax_anim.set_title("Phase 3 Message Flow (looping)")

    # Sliders
    time_slider = Slider(ax_time, "Time (ms)", min_time, max_time, valinit=min_time, valstep=base_dt)
    speed_slider = Slider(ax_speed, "Speed (x)", 0.05, 1.5, valinit=1.0, valstep=0.05)

    # Play/pause button
    btn = Button(ax_btn, "Pause")

    # Calculate fade duration based on base_dt (fade over ~1 time step in simulation time)
    base_fade_duration = base_dt * 1.0 if ENABLE_FADE else 0.0

    state = {
        "running": True,
        "slider_edit": False,
        "current": min_time,
        "last": min_time,
        "message_history": [] if ENABLE_FADE else None,  # Track recent messages with timestamps (only if fade enabled)
    }

    # Store widget references for cleanup
    fig._cleanup_handlers.extend([time_slider, speed_slider, btn])

    def update_frame(current_time: float, last_time: float):
        """Render a frame at a specific simulation time."""
        # Capture events that occurred since last_time (handles wrap)
        if current_time >= last_time:
            frame_events = events[(events["time_ms"] > last_time) & (events["time_ms"] <= current_time + 1e-9)]
        else:
            # Wrapped around: include tail and head
            frame_events = events[(events["time_ms"] > last_time) | (events["time_ms"] <= current_time + 1e-9)]
            # Clear history on wrap
            if ENABLE_FADE:
                state["message_history"] = []

        if ENABLE_FADE:
            # Add new events to message history
            for _, row in frame_events.iterrows():
                state["message_history"].append({
                    "time": row["time_ms"],
                    "sender_id": row["sender_id"],
                    "receiver_id": row["receiver_id"],
                    "event": row["event"],
                    "path_length": row["path_length"],
                    "message_type": row.get("message_type", "DISCOVERY"),
                })

            # Remove messages older than fade duration (in simulation time, not real-time)
            state["message_history"] = [
                msg for msg in state["message_history"]
                if current_time - msg["time"] <= base_fade_duration
            ]

            # Sort message history by time (oldest first) so newer messages draw on top
            state["message_history"].sort(key=lambda m: m["time"])

            # Node colors: base phase + activity override with fade
            base = phase_color(current_time, max_time)
            node_activity = {}  # node_id -> (activity_type, age_ms)

            for msg in state["message_history"]:
                age = current_time - msg["time"]
                if msg["event"] == "SEND":
                    sender = int(msg["sender_id"]) if pd.notna(msg["sender_id"]) else None
                    if sender and (sender not in node_activity or age < node_activity[sender][1]):
                        node_activity[sender] = ("send", age)
                elif msg["event"] == "RECV":
                    receiver = int(msg["receiver_id"]) if pd.notna(msg["receiver_id"]) else None
                    if receiver and (receiver not in node_activity or age < node_activity[receiver][1]):
                        node_activity[receiver] = ("recv", age)

            colors = []
            for n in node_ids:
                if n in node_activity:
                    colors.append(ACTIVITY_COLORS[node_activity[n][0]])
                else:
                    colors.append(base)
            scatter.set_color(colors)

            # Edge highlights with fade effect
            segments = []
            edge_colors = []
            alphas = []

            for msg in state["message_history"]:
                src = int(msg["sender_id"]) if pd.notna(msg["sender_id"]) else None
                dst = int(msg["receiver_id"]) if pd.notna(msg["receiver_id"]) else None
                msg_type = str(msg.get("message_type", "DISCOVERY")).upper()
                if src is None or dst is None or src not in pos or dst not in pos:
                    continue

                # Calculate fade (1.0 = just happened, 0.0 = fully faded)
                age = current_time - msg["time"]
                fade = max(0.0, 1.0 - (age / base_fade_duration))

                segments.append([(pos[src][0], pos[src][1]), (pos[dst][0], pos[dst][1])])

                if msg["event"] == "SEND":
                    base_color = MESSAGE_COLORS.get(msg_type, MESSAGE_COLORS["UNKNOWN"])
                else:
                    base_color = MESSAGE_COLORS["recv"]

                # Convert hex to RGB and apply fade
                rgb = mcolors.to_rgb(base_color)
                edge_colors.append(rgb)
                alphas.append(fade * 0.9)  # Max alpha of 0.9

            # Update line collection with faded colors
            highlight_coll.set_segments(segments)
            if edge_colors:
                highlight_coll.set_color(edge_colors)
                highlight_coll.set_alpha(None)  # Clear global alpha
                # Set individual alphas via linewidths hack or use multiple collections
                # For simplicity, we'll create colors with alpha built-in
                colors_with_alpha = [(*rgb, alpha) for rgb, alpha in zip(edge_colors, alphas)]
                highlight_coll.set_color(colors_with_alpha)
            else:
                highlight_coll.set_segments([])
        else:
            # No fade: show only current frame events
            base = phase_color(current_time, max_time)
            senders = set(frame_events[frame_events["event"] == "SEND"]["sender_id"].dropna().astype(int))
            receivers = set(frame_events[frame_events["event"] == "RECV"]["receiver_id"].dropna().astype(int))
            colors = []
            for n in node_ids:
                if n in senders:
                    colors.append(ACTIVITY_COLORS["send"])
                elif n in receivers:
                    colors.append(ACTIVITY_COLORS["recv"])
                else:
                    colors.append(base)
            scatter.set_color(colors)

            # Edge highlights for current events only
            segments = []
            colors = []
            for _, row in frame_events.iterrows():
                src = int(row["sender_id"]) if pd.notna(row["sender_id"]) else None
                dst = int(row["receiver_id"]) if pd.notna(row["receiver_id"]) else None
                msg_type = str(row.get("message_type", "DISCOVERY")).upper()
                if src is None or dst is None or src not in pos or dst not in pos:
                    continue
                segments.append([(pos[src][0], pos[src][1]), (pos[dst][0], pos[dst][1])])
                colors.append(MESSAGE_COLORS.get(msg_type, MESSAGE_COLORS["UNKNOWN"])
                              if row["event"] == "SEND"
                              else MESSAGE_COLORS["recv"])
            highlight_coll.set_segments(segments)
            highlight_coll.set_colors(colors if colors else ["none"])

        time_text.set_text(f"t = {current_time:.1f} ms")
        fig.canvas.draw_idle()

    # Helper to get next event timestamp after a given time (wraps)
    def next_timestamp(after_time: float) -> float:
        for t in timestamps:
            if t > after_time:
                return t
        return min_time  # wrap

    # Animation loop driven by FuncAnimation; we control the event_source start/stop
    def advance_frame(_):
        if not state["running"]:
            return
        dt = base_dt * max(0.1, float(speed_slider.val))
        desired_time = state["current"] + dt
        # Clamp step so we never skip over the next logged timestamp (so 1 ms gaps are shown)
        nxt = next_timestamp(state["current"])
        if state["current"] < nxt <= desired_time:
            new_time = nxt
        else:
            new_time = desired_time
        wrapped = False
        if new_time > max_time:
            new_time = min_time + (new_time - max_time)
            wrapped = True
        last_time = state["current"] if not wrapped else max_time
        state["last"] = last_time
        state["current"] = new_time
        state["slider_edit"] = True
        time_slider.set_val(new_time)  # valstep snaps to base_dt
        state["slider_edit"] = False
        update_frame(new_time, last_time)

    anim = FuncAnimation(
        fig,
        advance_frame,
        frames=itertools.count(),
        interval=100,
        blit=False,
        cache_frame_data=False,
        save_count=len(events) + 5,
    )
    fig._phase3_anim = anim  # keep reference
    # Ensure event source is running initially
    if hasattr(anim, "event_source"):
        anim.event_source.start()

    def on_time_release(val):
        # Only allow scrubbing while paused and when slider is active
        if state["slider_edit"] or state["running"] or not time_slider.active:
            return
        snapped = min_time + round((float(val) - min_time) / base_dt) * base_dt
        snapped = max(min_time, min(max_time, snapped))

        if ENABLE_FADE:
            # When scrubbing, rebuild message history from scratch for the current time window
            state["message_history"] = []
            current_time = snapped

            # Get all events within fade duration of current time
            history_events = events[
                (events["time_ms"] <= current_time) &
                (events["time_ms"] > current_time - base_fade_duration)
            ]

            for _, row in history_events.iterrows():
                state["message_history"].append({
                    "time": row["time_ms"],
                    "sender_id": row["sender_id"],
                    "receiver_id": row["receiver_id"],
                    "event": row["event"],
                    "path_length": row["path_length"],
                })

        state["last"] = state["current"]
        state["current"] = snapped
        update_frame(state["current"], state["current"])

    # Use on_changed for visual feedback but store the callback ID
    # The actual update happens on mouse release via the event handler below
    def on_time_drag(val):
        # Update the time text during drag for visual feedback only
        if not state["running"] and time_slider.active:
            snapped = min_time + round((float(val) - min_time) / base_dt) * base_dt
            snapped = max(min_time, min(max_time, snapped))
            time_text.set_text(f"t = {snapped:.1f} ms")
            fig.canvas.draw_idle()

    # Connect to changed event for visual feedback during drag
    time_drag_cid = time_slider.on_changed(on_time_drag)
    fig._cleanup_handlers.append(time_drag_cid)

    # Connect mouse release event to actually update the visualization
    def on_mouse_release(event):
        if event.inaxes == ax_time and not state["running"] and time_slider.active:
            on_time_release(time_slider.val)

    mouse_release_cid = fig.canvas.mpl_connect('button_release_event', on_mouse_release)
    fig._cleanup_handlers.append(mouse_release_cid)

    def on_speed_change(val):
        # Speed is applied as a multiplier on dt in advance_frame; also adjust interval for responsiveness
        speed = max(0.1, float(val))
        src = getattr(anim, "event_source", None)
        if src is not None:
            src.interval = max(10, 200 / speed)
            if state["running"]:
                src.stop()
                src.start()

    speed_cid = speed_slider.on_changed(on_speed_change)
    fig._cleanup_handlers.append(speed_cid)

    def toggle(_event):
        state["running"] = not state["running"]
        btn.label.set_text("Pause" if state["running"] else "Play")
        time_slider.set_active(not state["running"])  # disable scrubbing while playing
        src = getattr(anim, "event_source", None)
        if src is not None:
            if state["running"]:
                src.start()
            else:
                src.stop()

    btn_cid = btn.on_clicked(toggle)
    fig._cleanup_handlers.append(btn_cid)

    # Start with scrubbing disabled while running
    time_slider.set_active(False)

    # Legend text
    legend_ax = fig.add_axes([0.08, 0.02, 0.84, 0.05])
    legend_ax.axis("off")
    legend_ax.text(
        0.0,
        0.65,
        "Messages: origin SEND=orange, forward SEND=dark orange, RECV=green; "
        "Nodes: send=yellow, recv=green; otherwise phase-color (noise/neighbor/candidate/election).",
        fontsize=9,
    )

    # Initial frame
    update_frame(min_time, min_time)

    # Add cleanup handler for when figure is closed
    def on_close(event):
        """Clean up resources when figure is closed."""
        try:
            # Stop animation first
            if hasattr(fig, "_phase3_anim"):
                anim_obj = fig._phase3_anim
                if hasattr(anim_obj, "event_source") and anim_obj.event_source is not None:
                    try:
                        anim_obj.event_source.stop()
                    except Exception:
                        pass
            # Disconnect all callbacks
            for handler in getattr(fig, "_cleanup_handlers", []):
                if isinstance(handler, int):
                    # It's a connection ID
                    try:
                        fig.canvas.mpl_disconnect(handler)
                    except Exception:
                        pass
                elif hasattr(handler, "disconnect"):
                    try:
                        handler.disconnect()
                    except Exception:
                        pass
            # Clear references
            fig._cleanup_handlers = []
            fig._phase3_anim = None
        except Exception:
            pass  # Ignore errors during cleanup

    close_cid = fig.canvas.mpl_connect("close_event", on_close)
    fig._cleanup_handlers.append(close_cid)

    return fig


def cleanup_all():
    """Clean up all matplotlib resources."""
    try:
        plt.close("all")
    except Exception:
        pass


def signal_handler(sig, frame):
    """Handle interrupt signals gracefully."""
    print("\nInterrupt received, cleaning up...", file=sys.stderr)
    cleanup_all()
    sys.exit(0)


def main():
    # Register cleanup handlers
    atexit.register(cleanup_all)
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    parser = argparse.ArgumentParser(description="Phase 3 discovery trace visualizer with interactive controls.")
    parser.add_argument("--trace", required=True, help="Path to phase3_trace.csv")
    parser.add_argument("--no-static", action="store_true", help="Disable static plots and show only the animation")
    parser.add_argument("--no-anim", action="store_true", help="Disable animation and show only static plots")
    parser.add_argument("--backend", default=None, help="Matplotlib backend to use (e.g., 'TkAgg', 'Qt5Agg')")
    args = parser.parse_args()

    # Set backend if specified (overrides default TkAgg)
    if args.backend:
        try:
            matplotlib.use(args.backend, force=True)
            print(f"Using matplotlib backend: {args.backend}", file=sys.stderr)
        except Exception as e:
            print(f"Warning: Could not set backend to {args.backend}: {e}", file=sys.stderr)
            print(f"Falling back to default backend: TkAgg", file=sys.stderr)

    trace_path = Path(args.trace)
    if not trace_path.exists():
        raise SystemExit(f"Trace not found: {trace_path}")

    df = load_trace(trace_path)
    if df.empty:
        raise SystemExit("Trace file is empty or could not be parsed.")

    # Static dashboard
    if not args.no_static:
        create_static_figure(df)
    # Interactive animation
    if not args.no_anim:
        G = extract_topology(df)
        build_animation(df, G)

    try:
        plt.show()
    except KeyboardInterrupt:
        print("\nClosing visualizer...", file=sys.stderr)
    except Exception as e:
        print(f"Error during visualization: {e}", file=sys.stderr)
    finally:
        # Ensure clean shutdown
        cleanup_all()


if __name__ == "__main__":
    main()
