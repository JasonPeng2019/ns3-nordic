#!/usr/bin/env python3
"""
Animated packet flow visualization for BLE mesh discovery traces.

This utility replays the simulation events recorded in simulation_trace.csv,
showing:
  * Network topology with numbered nodes
  * Time-synchronized playback that pauses whenever packets are exchanged
  * Directional arrows between nodes for each transmission
  * Arrow colors keyed to the originator of every packet
  * Legend describing node states and packet origins

Run from the repository root:

    python3 ns3-nordic/ns3_dev/ns3-ble-module/ble-mesh-discovery/engine_sim/visualizations/visualize_packet_flow.py

By default the script looks for ns3-nordic/ns3_dev/ns-3-dev/simulation_trace.csv.
Provide a different file path if needed.
"""

from __future__ import annotations

import argparse
from collections import defaultdict, deque
from dataclasses import dataclass
from pathlib import Path
from typing import Deque, Dict, Iterable, List, Tuple

import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from matplotlib.patches import FancyArrowPatch
from matplotlib.animation import FuncAnimation
from matplotlib.lines import Line2D
import networkx as nx
import pandas as pd
from matplotlib.colors import to_hex


MAX_PROPAGATION_DELAY_MS = 5


@dataclass
class Transmission:
    """Represents a packet moving from one node to another."""

    send_time: float
    arrival_time: float
    sender_id: int
    receiver_id: int
    originator_id: int


class PacketFlowAnimator:
    """Event-driven animation controller."""

    def __init__(
        self,
        trace_path: Path,
        pause_frames: int = 20,
        frame_interval_ms: int = 250,
    ) -> None:
        self.trace_path = trace_path
        self.pause_frames = max(5, pause_frames)
        self.frame_interval_ms = frame_interval_ms

        self.df = self._load_trace(trace_path)
        self.graph = self._build_topology()
        self.nodelist = sorted(self.graph.nodes)
        self.positions = self._compute_layout()
        self.originator_colors = self._assign_origin_colors()
        self.transmissions_by_time = self._build_transmissions()
        self.events = self._group_events()
        self.min_time = (
            float(self.df["time_ms"].min()) if not self.df.empty else 0.0
        )

        # Matplotlib structures populated in _setup_plot
        self.fig = None
        self.ax = None
        self.node_collection = None
        self.timeline_text = None
        self.event_text = None
        self.arrow_artists: List[FancyArrowPatch] = []

        self._setup_plot()
        total_events = len(self.events) + 1  # include initial idle frame
        self.total_frames = max(1, total_events * self.pause_frames)

    def _load_trace(self, path: Path) -> pd.DataFrame:
        df = pd.read_csv(path)
        numeric_cols = [
            "time_ms",
            "sender_id",
            "receiver_id",
            "originator_id",
            "ttl",
            "path_length",
            "rssi",
        ]
        for col in numeric_cols:
            if col in df.columns:
                df[col] = pd.to_numeric(df[col], errors="coerce")
        return df

    def _build_topology(self) -> nx.Graph:
        graph = nx.Graph()
        topo = self.df[self.df["event"] == "TOPOLOGY"]
        for _, row in topo.iterrows():
            graph.add_edge(int(row["sender_id"]), int(row["receiver_id"]))

        send_rows = self.df[self.df["event"] == "SEND"]
        for _, row in send_rows.iterrows():
            if pd.notna(row["sender_id"]):
                graph.add_node(int(row["sender_id"]))
        return graph

    def _compute_layout(self) -> Dict[int, Tuple[float, float]]:
        if not self.graph.nodes:
            return {}
        pos = nx.spring_layout(self.graph, seed=7, k=2.5, iterations=200)
        for node, (x, y) in pos.items():
            pos[node] = (x * 0.7, y * 0.7)
        return pos

    def _assign_origin_colors(self) -> Dict[int, str]:
        send_df = self.df[self.df["event"] == "SEND"]
        origins = sorted(
            {int(row["originator_id"]) for _, row in send_df.iterrows() if pd.notna(row["originator_id"])}
        )
        if not origins:
            return {}
        cmap = plt.get_cmap("tab20", len(origins))
        return {origin: to_hex(cmap(idx)) for idx, origin in enumerate(origins)}

    def _build_transmissions(self) -> Dict[float, List[Transmission]]:
        transmissions: Dict[float, List[Transmission]] = defaultdict(list)
        send_df = self.df[self.df["event"] == "SEND"].sort_values("time_ms")
        recv_df = (
            self.df[self.df["event"] == "RECV"]
            .dropna(subset=["receiver_id", "originator_id", "time_ms"])
            .sort_values("time_ms")
        )

        recv_lookup: Dict[Tuple[int, int], Deque[pd.Series]] = defaultdict(deque)
        for _, row in recv_df.iterrows():
            key = (int(row["receiver_id"]), int(row["originator_id"]))
            recv_lookup[key].append(row)

        for _, send in send_df.iterrows():
            if pd.isna(send["sender_id"]):
                continue
            sender = int(send["sender_id"])
            origin = (
                int(send["originator_id"]) if pd.notna(send["originator_id"]) else sender
            )
            time_ms = float(send["time_ms"])
            for neighbor in self.graph.neighbors(sender):
                recv_row = self._match_recv_event(
                    recv_lookup, neighbor, origin, time_ms
                )
                if recv_row is None:
                    continue
                transmissions[time_ms].append(
                    Transmission(
                        send_time=time_ms,
                        arrival_time=float(recv_row["time_ms"]),
                        sender_id=sender,
                        receiver_id=neighbor,
                        originator_id=origin,
                    )
                )

        return transmissions

    def _match_recv_event(
        self,
        lookup: Dict[Tuple[int, int], Deque[pd.Series]],
        receiver: int,
        origin: int,
        send_time: float,
    ) -> pd.Series | None:
        key = (receiver, origin)
        queue = lookup.get(key)
        if not queue:
            return None

        while queue and queue[0]["time_ms"] <= send_time:
            queue.popleft()

        if queue and queue[0]["time_ms"] - send_time <= MAX_PROPAGATION_DELAY_MS:
            return queue.popleft()
        return None

    def _group_events(self) -> List[dict]:
        grouped_events: List[dict] = []
        send_df = self.df[self.df["event"] == "SEND"]
        recv_df = self.df[self.df["event"] == "RECV"]

        for time_ms, rows in send_df.groupby("time_ms"):
            senders = sorted(
                [int(val) for val in rows["sender_id"].dropna().astype(int).tolist()]
            )
            transmissions = self.transmissions_by_time.get(time_ms, [])
            grouped_events.append(
                {
                    "time_ms": float(time_ms),
                    "senders": senders,
                    "receivers": [],
                    "transmissions": transmissions,
                    "phase": "SEND",
                    "recv_details": [],
                }
            )

        for time_ms, rows in recv_df.groupby("time_ms"):
            receivers = sorted(
                [int(val) for val in rows["receiver_id"].dropna().astype(int).tolist()]
            )
            details = []
            for _, row in rows.iterrows():
                if pd.isna(row["receiver_id"]) or pd.isna(row["originator_id"]):
                    continue
                receiver = int(row["receiver_id"])
                origin = int(row["originator_id"])
                details.append(f"{receiver} ← origin {origin}")
            grouped_events.append(
                {
                    "time_ms": float(time_ms),
                    "senders": [],
                    "receivers": receivers,
                    "transmissions": [],
                    "phase": "RECV",
                    "recv_details": details,
                }
            )

        grouped_events.sort(key=lambda evt: evt["time_ms"])
        return grouped_events

    def _setup_plot(self) -> None:
        self.fig, self.ax = plt.subplots(figsize=(10, 8))
        self.ax.set_title("BLE Mesh Packet Flow Timeline")
        self.ax.axis("off")

        if not self.graph.nodes:
            return

        nx.draw_networkx_edges(
            self.graph, self.positions, ax=self.ax, edge_color="#9f9f9f", width=1.5
        )
        self.node_collection = nx.draw_networkx_nodes(
            self.graph,
            self.positions,
            nodelist=self.nodelist,
            ax=self.ax,
            node_color="#cfe2ff",
            node_size=750,
            edgecolors="#1f4b99",
            linewidths=1.5,
        )
        nx.draw_networkx_labels(
            self.graph,
            self.positions,
            ax=self.ax,
            font_size=12,
            font_weight="bold",
        )

        self.timeline_text = self.ax.text(
            0.02,
            0.96,
            "",
            transform=self.ax.transAxes,
            fontsize=12,
            fontweight="bold",
            bbox=dict(facecolor="white", alpha=0.8, edgecolor="#666666"),
        )
        self.event_text = self.ax.text(
            0.02,
            0.88,
            "",
            transform=self.ax.transAxes,
            fontsize=10,
            bbox=dict(facecolor="white", alpha=0.7, edgecolor="#666666"),
        )

        self._build_legend()

    def _build_legend(self) -> None:
        # Stack legends in the upper-left corner, moving downward to avoid overlapping nodes.
        anchor_x = 0.02
        current_anchor_y = 0.98

        # Node state legend (top entry of the stack)
        idle_patch = mpatches.Patch(color="#cfe2ff", label="Idle node")
        send_patch = mpatches.Patch(color="#f26b5c", label="Sending")
        recv_patch = mpatches.Patch(color="#6fcf79", label="Receiving")
        both_patch = mpatches.Patch(color="#ffb347", label="Send & Receive")
        state_legend = self.ax.legend(
            handles=[idle_patch, send_patch, recv_patch, both_patch],
            loc="upper left",
            bbox_to_anchor=(anchor_x, current_anchor_y),
            bbox_transform=self.ax.transAxes,
            frameon=True,
            fontsize=9,
            title="Node states",
        )
        self.ax.add_artist(state_legend)
        current_anchor_y -= 0.24  # shift down for the next legend

        # Transmission type legend (self broadcast vs forwarded)
        own_handle = Line2D(
            [0],
            [0],
            color="#4c4c4c",
            lw=2.2,
            label="Self broadcast",
            linestyle="-",
        )
        forward_handle = Line2D(
            [0],
            [0],
            color="#4c4c4c",
            lw=2.2,
            label="Forwarded packet",
            linestyle=(0, (5, 3)),
        )
        tx_type_legend = self.ax.legend(
            handles=[own_handle, forward_handle],
            loc="upper left",
            bbox_to_anchor=(anchor_x, max(0.74, current_anchor_y)),
            bbox_transform=self.ax.transAxes,
            frameon=True,
            fontsize=9,
            title="Transmission type",
        )
        self.ax.add_artist(tx_type_legend)
        current_anchor_y -= 0.18

        # Arrow origin legend (stacked below node states and transmission type)
        if self.originator_colors:
            arrow_handles: List[Line2D] = []
            for origin, color in self.originator_colors.items():
                arrow_handles.append(
                    Line2D(
                        [0],
                        [0],
                        color=color,
                        lw=3,
                        label=f"Origin {origin}",
                        marker=">",
                        markevery=(1, 1),
                    )
                )
            origin_legend = self.ax.legend(
                handles=arrow_handles,
                loc="upper left",
                bbox_to_anchor=(anchor_x, max(0.12, current_anchor_y)),
                bbox_transform=self.ax.transAxes,
                frameon=True,
                fontsize=9,
                title="Packet origin",
            )
            self.ax.add_artist(origin_legend)

    def animate(self) -> None:
        if not self.graph.nodes:
            print("No nodes found in trace – cannot animate.")
            return

        animation = FuncAnimation(
            self.fig,
            self._update_frame,
            frames=self.total_frames,
            interval=self.frame_interval_ms,
            repeat=False,
        )
        # keep reference to avoid GC
        self.animation = animation
        plt.show()

    def _update_frame(self, frame_index: int):
        if not self.events:
            self.timeline_text.set_text("No SEND events in trace.")
            self.event_text.set_text("Simulation idle.")
            return self.node_collection

        block = frame_index // self.pause_frames
        if block == 0:
            self._clear_arrows()
            self._set_node_states([], [])
            self.timeline_text.set_text(f"Simulation time: {self.min_time:.0f} ms")
            self.event_text.set_text("Network ready. Waiting for first event...")
            return self.node_collection

        event_idx = min(block - 1, len(self.events) - 1)
        event = self.events[event_idx]
        self._clear_arrows()
        self._set_node_states(event["senders"], event["receivers"])
        self._draw_transmissions(event["transmissions"])
        self._update_labels(event)
        return self.node_collection

    def _set_node_states(
        self, senders: Iterable[int], receivers: Iterable[int]
    ) -> None:
        sender_set = set(senders)
        receiver_set = set(receivers)
        colors = []
        for node in self.nodelist:
            if node in sender_set and node in receiver_set:
                colors.append("#ffb347")
            elif node in sender_set:
                colors.append("#f26b5c")
            elif node in receiver_set:
                colors.append("#6fcf79")
            else:
                colors.append("#cfe2ff")
        if self.node_collection is not None:
            self.node_collection.set_facecolor(colors)

    def _draw_transmissions(self, transmissions: List[Transmission]) -> None:
        for tx in transmissions:
            color = self.originator_colors.get(tx.originator_id, "#4c78a8")
            start = self.positions.get(tx.sender_id)
            end = self.positions.get(tx.receiver_id)
            if start is None or end is None:
                continue
            is_self = tx.sender_id == tx.originator_id
            linestyle = "-" if is_self else (0, (5, 3))
            arrow = FancyArrowPatch(
                start,
                end,
                arrowstyle="->",
                color=color,
                lw=2.5,
                linestyle=linestyle,
                mutation_scale=16,
                alpha=0.85,
            )
            self.ax.add_patch(arrow)
            self.arrow_artists.append(arrow)

    def _update_labels(self, event: dict) -> None:
        self.timeline_text.set_text(f"Simulation time: {event['time_ms']:.0f} ms")
        transmissions = event["transmissions"]
        phase = event.get("phase", "SEND")

        if transmissions:
            lines = []
            for tx in transmissions[:6]:
                lines.append(
                    f"{tx.sender_id} → {tx.receiver_id} (origin {tx.originator_id})"
                )
            if len(transmissions) > 6:
                lines.append("...")
            self.event_text.set_text("Packets in flight:\n" + "\n".join(lines))
            return

        if phase == "SEND":
            sender_str = ", ".join(str(s) for s in event["senders"]) or "—"
            self.event_text.set_text(
                f"Nodes transmitting: {sender_str}\n(Awaiting reception logs)"
            )
            return

        # RECV phase
        if event.get("recv_details"):
            preview = event["recv_details"][:6]
            if len(event["recv_details"]) > 6:
                preview.append("...")
            self.event_text.set_text("Nodes receiving:\n" + "\n".join(preview))
        else:
            receiver_str = ", ".join(str(r) for r in event["receivers"]) or "—"
            self.event_text.set_text(f"Nodes receiving: {receiver_str}")

    def _clear_arrows(self) -> None:
        for artist in self.arrow_artists:
            try:
                artist.remove()
            except ValueError:
                pass
        self.arrow_artists.clear()


def resolve_default_trace_path() -> Path:
    script_dir = Path(__file__).resolve()
    try:
        default_root = script_dir.parents[4]
    except IndexError:  # fallback if directory layout changes
        default_root = script_dir.parent
    candidate = default_root / "ns-3-dev" / "simulation_trace.csv"
    return candidate


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Visualize BLE mesh packet exchanges over time."
    )
    parser.add_argument(
        "trace",
        nargs="?",
        default=None,
        help="Path to simulation_trace.csv (defaults to ns-3-dev/simulation_trace.csv)",
    )
    parser.add_argument(
        "--pause-frames",
        type=int,
        default=20,
        help="How many frames to pause on each event (higher = slower).",
    )
    parser.add_argument(
        "--interval",
        type=int,
        default=250,
        help="Animation frame interval in milliseconds.",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    trace_path = Path(args.trace) if args.trace else resolve_default_trace_path()
    if not trace_path.exists():
        raise FileNotFoundError(
            f"Trace file '{trace_path}' not found. Provide the CSV path explicitly."
        )

    animator = PacketFlowAnimator(
        trace_path=trace_path,
        pause_frames=args.pause_frames,
        frame_interval_ms=args.interval,
    )
    animator.animate()


if __name__ == "__main__":
    main()
