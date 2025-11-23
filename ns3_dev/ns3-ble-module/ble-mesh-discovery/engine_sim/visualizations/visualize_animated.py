#!/usr/bin/env python3
"""
NetAnim-Style Animated BLE Mesh Discovery Visualizer

This script provides a NetAnim-like visualization experience for the
phase2-discovery-sim trace output. It shows:
- Nodes positioned on a 2D canvas
- Animated packets moving between nodes
- Color-coded node states (idle, sending, receiving)
- Event labels explaining what's happening
- Playback controls (play/pause, speed, timeline scrubbing)

=============================================================================
WHAT YOU'LL SEE
=============================================================================

Node Colors:
- Light blue: Idle
- Red: Currently sending a packet
- Green: Currently receiving a packet
- Orange: Both sending and receiving

Event Labels:
- When events occur, labels appear next to nodes explaining what happened
- "Sending discovery" - Node broadcasting its own discovery
- "Forwarding from Node X" - Node re-broadcasting another node's packet
- "Received from Node X" - Node received a packet

Packet Animation:
- Small colored circles move from sender to receiver
- Color indicates the originator of the discovery packet
- Animation pauses briefly when events occur

Controls:
- Space: Play/Pause
- Left/Right arrows: Step backward/forward
- Up/Down arrows: Speed up/slow down
- Click on timeline: Jump to time
- 'R': Reset to beginning
- 'Q': Quit

=============================================================================
USAGE
=============================================================================

    python3 visualize_animated.py simulation_trace.csv

Dependencies: pip install pandas matplotlib networkx

"""

import argparse
import sys
from pathlib import Path
from dataclasses import dataclass
from typing import List, Dict, Tuple, Set
from collections import defaultdict

import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from matplotlib.animation import FuncAnimation
from matplotlib.widgets import Slider, Button
import networkx as nx
import numpy as np


@dataclass
class PacketAnimation:
    """Represents a packet in flight between two nodes."""
    sender_id: int
    receiver_id: int
    originator_id: int
    start_time: float  # ms
    end_time: float    # ms
    ttl: int
    path_length: int


@dataclass
class NodeEvent:
    """Represents a node state change event."""
    time_ms: float
    node_id: int
    event_type: str  # 'SEND' or 'RECV'
    originator_id: int
    is_self_discovery: bool  # True if sending own discovery (path_length == 1)


class AnimatedVisualizer:
    """
    NetAnim-style animated visualization of BLE mesh discovery.

    This class manages:
    - Network topology graph
    - Node positions and rendering
    - Packet animations
    - Event labels and pausing
    - Playback state and controls
    """

    def __init__(self, trace_file: str):
        """Load trace data and initialize visualization."""
        self.df = self._load_trace(trace_file)
        self.G = self._build_topology()
        self.pos = self._compute_layout()

        # Extract events and animations
        self.node_events = self._extract_node_events()
        self.packet_animations = self._extract_packet_animations()
        self.event_times = self._get_event_times()

        # Create sorted list of unique event times for stepping
        self.sorted_event_times = sorted(self.event_times)
        self.current_event_index = 0

        # Time range
        event_times = self.df[self.df['event'].isin(['SEND', 'RECV'])]['time_ms']
        self.min_time = event_times.min() if len(event_times) > 0 else 0
        self.max_time = event_times.max() if len(event_times) > 0 else 1000

        # Playback state - EVENT-DRIVEN stepping
        self.current_time = self.min_time
        self.playing = False
        self.auto_advance = True  # Automatically advance through events
        self.pause_duration = 60  # Frames to pause at each event (~3 seconds at 20 FPS)
        self.frames_at_current_event = 0

        # Event pause state
        self.pause_until_frame = 0
        self.current_frame = 0

        # Color map for originators
        self.originator_colors = self._create_color_map()

        # Setup figure
        self._setup_figure()

    def _load_trace(self, filepath: str) -> pd.DataFrame:
        """Load and parse the CSV trace file."""
        df = pd.read_csv(filepath)
        numeric_cols = ['time_ms', 'sender_id', 'receiver_id', 'originator_id',
                        'ttl', 'path_length', 'rssi']
        for col in numeric_cols:
            if col in df.columns:
                df[col] = pd.to_numeric(df[col], errors='coerce')
        return df

    def _build_topology(self) -> nx.Graph:
        """Build network graph from TOPOLOGY events."""
        G = nx.Graph()
        topology_df = self.df[self.df['event'] == 'TOPOLOGY']

        for _, row in topology_df.iterrows():
            node_a = int(row['sender_id'])
            node_b = int(row['receiver_id'])
            G.add_edge(node_a, node_b)

        # Also add nodes from SEND events
        for _, row in self.df[self.df['event'] == 'SEND'].iterrows():
            if pd.notna(row['sender_id']):
                G.add_node(int(row['sender_id']))

        return G

    def _compute_layout(self) -> Dict[int, Tuple[float, float]]:
        """Compute node positions using spring layout, keeping nodes away from edges."""
        if len(self.G.nodes()) == 0:
            return {}

        # Use spring layout with more spacing
        pos = nx.spring_layout(self.G, seed=42, k=3, iterations=100)

        # Scale positions to use central area (leave room for labels and legend)
        for node in pos:
            x, y = pos[node]
            # Scale to [-0.6, 0.6] range to leave margins
            pos[node] = (x * 0.6, y * 0.6)

        return pos

    def _extract_node_events(self) -> List[NodeEvent]:
        """Extract all SEND and RECV events for node state tracking."""
        events = []

        for _, row in self.df[self.df['event'] == 'SEND'].iterrows():
            sender_id = int(row['sender_id'])
            originator = int(row['originator_id']) if pd.notna(row['originator_id']) else sender_id
            path_length = int(row['path_length']) if pd.notna(row['path_length']) else 1

            events.append(NodeEvent(
                time_ms=row['time_ms'],
                node_id=sender_id,
                event_type='SEND',
                originator_id=originator,
                is_self_discovery=(path_length == 1 and sender_id == originator)
            ))

        for _, row in self.df[self.df['event'] == 'RECV'].iterrows():
            events.append(NodeEvent(
                time_ms=row['time_ms'],
                node_id=int(row['receiver_id']),
                event_type='RECV',
                originator_id=int(row['originator_id']) if pd.notna(row['originator_id']) else 0,
                is_self_discovery=False
            ))

        return sorted(events, key=lambda e: e.time_ms)

    def _extract_packet_animations(self) -> List[PacketAnimation]:
        """
        Extract packet animations by matching SEND events with their RECV events.
        """
        animations = []
        send_df = self.df[self.df['event'] == 'SEND']
        recv_df = self.df[self.df['event'] == 'RECV']

        for _, send_row in send_df.iterrows():
            sender = int(send_row['sender_id'])
            send_time = send_row['time_ms']
            originator = int(send_row['originator_id']) if pd.notna(send_row['originator_id']) else sender
            ttl = int(send_row['ttl']) if pd.notna(send_row['ttl']) else 6
            path_len = int(send_row['path_length']) if pd.notna(send_row['path_length']) else 1

            # Find matching RECV events
            matching = recv_df[
                (recv_df['time_ms'] == send_time + 1) &
                (recv_df['originator_id'] == originator) &
                (recv_df['ttl'] == ttl)
            ]

            for _, recv_row in matching.iterrows():
                receiver = int(recv_row['receiver_id'])
                if self.G.has_edge(sender, receiver):
                    animations.append(PacketAnimation(
                        sender_id=sender,
                        receiver_id=receiver,
                        originator_id=originator,
                        start_time=send_time,
                        end_time=send_time + 1,
                        ttl=ttl,
                        path_length=path_len
                    ))

        return animations

    def _get_event_times(self) -> Set[float]:
        """Get all unique event times for pausing."""
        times = set()
        for event in self.node_events:
            times.add(event.time_ms)
        return times

    def _create_color_map(self) -> Dict[int, str]:
        """Create a color map for packet originators."""
        originators = set()
        for anim in self.packet_animations:
            originators.add(anim.originator_id)
        for event in self.node_events:
            originators.add(event.originator_id)

        # Use distinct colors
        colors = ['#e41a1c', '#377eb8', '#4daf4a', '#984ea3', '#ff7f00',
                  '#ffff33', '#a65628', '#f781bf', '#999999', '#66c2a5']
        return {orig: colors[i % len(colors)] for i, orig in enumerate(sorted(originators))}

    def _setup_figure(self):
        """Setup matplotlib figure with controls."""
        self.fig = plt.figure(figsize=(16, 11))

        # Main visualization area - give more vertical space
        self.ax_main = self.fig.add_axes([0.02, 0.22, 0.70, 0.75])
        self.ax_main.set_title('BLE Mesh Discovery Animation', fontsize=14, fontweight='bold')
        self.ax_main.axis('off')
        self.ax_main.set_xlim(-1.2, 1.2)
        self.ax_main.set_ylim(-1.2, 1.2)

        # Legend area on the right side
        self.ax_legend = self.fig.add_axes([0.74, 0.22, 0.24, 0.75])
        self.ax_legend.axis('off')
        self._draw_legend()

        # Timeline slider
        self.ax_slider = self.fig.add_axes([0.15, 0.10, 0.7, 0.03])
        self.slider = Slider(
            self.ax_slider, 'Time (ms)',
            self.min_time, self.max_time,
            valinit=self.min_time, valstep=1
        )
        self.slider.on_changed(self._on_slider_change)

        # Play/Pause button
        self.ax_play = self.fig.add_axes([0.15, 0.03, 0.1, 0.04])
        self.btn_play = Button(self.ax_play, 'Play')
        self.btn_play.on_clicked(self._toggle_play)

        # Speed buttons
        self.ax_slower = self.fig.add_axes([0.28, 0.03, 0.08, 0.04])
        self.btn_slower = Button(self.ax_slower, 'Slower')
        self.btn_slower.on_clicked(lambda e: self._change_speed(0.5))

        self.ax_faster = self.fig.add_axes([0.38, 0.03, 0.08, 0.04])
        self.btn_faster = Button(self.ax_faster, 'Faster')
        self.btn_faster.on_clicked(lambda e: self._change_speed(2.0))

        # Reset button
        self.ax_reset = self.fig.add_axes([0.50, 0.03, 0.08, 0.04])
        self.btn_reset = Button(self.ax_reset, 'Reset')
        self.btn_reset.on_clicked(self._reset)

        # Speed/pause display
        self.ax_speed = self.fig.add_axes([0.62, 0.03, 0.12, 0.04])
        self.ax_speed.axis('off')
        pause_sec = self.pause_duration / 20.0
        self.speed_text = self.ax_speed.text(0.5, 0.5, f'Pause: {pause_sec:.1f}s',
                                              ha='center', va='center', fontsize=10)

        # Status text
        self.ax_status = self.fig.add_axes([0.02, 0.16, 0.96, 0.04])
        self.ax_status.axis('off')
        self.status_text = self.ax_status.text(
            0.5, 0.5, 'SPACE: play/pause | LEFT/RIGHT: step through events | UP/DOWN: adjust pause time | R: reset | Q: quit',
            ha='center', va='center', fontsize=9, color='gray'
        )

        # Connect keyboard events
        self.fig.canvas.mpl_connect('key_press_event', self._on_key_press)

    def _draw_legend(self):
        """Draw the color legend in the side panel."""
        self.ax_legend.clear()
        self.ax_legend.axis('off')
        self.ax_legend.set_xlim(0, 1)
        self.ax_legend.set_ylim(0, 1)
        self.ax_legend.set_aspect('equal')  # Ensure circles render as circles, not ovals

        self.ax_legend.text(0.5, 0.98, 'Legend', ha='center', va='top',
                           fontsize=12, fontweight='bold')

        y = 0.92
        spacing = 0.08  # Slightly more spacing for cleaner look

        # Node states
        self.ax_legend.text(0.08, y, 'Node States:', fontsize=10, fontweight='bold')
        y -= spacing

        states = [
            ('lightblue', 'Idle'),
            ('red', 'Sending'),
            ('green', 'Receiving'),
            ('orange', 'Send + Receive'),
        ]

        for color, label in states:
            circle = plt.Circle((0.12, y), 0.025, color=color, ec='black', linewidth=1.5)
            self.ax_legend.add_patch(circle)
            self.ax_legend.text(0.20, y, label, fontsize=9, va='center')
            y -= spacing

        y -= spacing * 0.5

        # Packet colors
        self.ax_legend.text(0.08, y, 'Packet Origins:', fontsize=10, fontweight='bold')
        y -= spacing

        for orig, color in sorted(self.originator_colors.items()):
            circle = plt.Circle((0.12, y), 0.02, color=color, ec='black', linewidth=1.5)
            self.ax_legend.add_patch(circle)
            self.ax_legend.text(0.20, y, f'From Node {orig}', fontsize=9, va='center')
            y -= spacing
            if y < 0.1:  # Don't overflow
                break

    def _get_node_states(self, time_ms: float) -> Dict[int, str]:
        """Get the state of each node at a given time."""
        states = {node: 'idle' for node in self.G.nodes()}
        window = 5  # Window to show state after event occurred

        for event in self.node_events:
            # Only show state for events that have happened (not future events)
            # and are within the display window
            if event.time_ms <= time_ms <= event.time_ms + window:
                node = event.node_id
                if event.event_type == 'SEND':
                    if states[node] == 'recv':
                        states[node] = 'both'
                    elif states[node] == 'idle':
                        states[node] = 'send'
                elif event.event_type == 'RECV':
                    if states[node] == 'send':
                        states[node] = 'both'
                    elif states[node] == 'idle':
                        states[node] = 'recv'

        return states

    def _get_current_events(self, time_ms: float) -> List[NodeEvent]:
        """Get events happening at the current time (within display window)."""
        window = 5  # Same window as node states for consistency
        # Only show labels for events that have happened (not future events)
        return [e for e in self.node_events
                if e.time_ms <= time_ms <= e.time_ms + window]

    def _get_event_labels(self, time_ms: float) -> Dict[int, List[str]]:
        """Get descriptive labels for each node's current events."""
        labels = defaultdict(list)
        events = self._get_current_events(time_ms)

        for event in events:
            node = event.node_id

            if event.event_type == 'SEND':
                if event.is_self_discovery:
                    labels[node].append("SEND: Broadcasting\nmy own discovery")
                else:
                    labels[node].append(f"FORWARD: Re-broadcasting\nNode {event.originator_id}'s discovery")

            elif event.event_type == 'RECV':
                labels[node].append(f"RECV: Got packet\noriginated by Node {event.originator_id}")

        return labels

    def _get_event_summary(self, time_ms: float) -> str:
        """Get a summary of what's happening at this time for the status bar."""
        events = self._get_current_events(time_ms)
        if not events:
            return "No events at this time"

        send_events = [e for e in events if e.event_type == 'SEND']
        recv_events = [e for e in events if e.event_type == 'RECV']

        parts = []
        if send_events:
            self_disc = [e for e in send_events if e.is_self_discovery]
            forwards = [e for e in send_events if not e.is_self_discovery]

            if self_disc:
                nodes = [e.node_id for e in self_disc]
                parts.append(f"Nodes {nodes} broadcast their own discovery")
            if forwards:
                for e in forwards[:3]:  # Limit to avoid overflow
                    parts.append(f"Node {e.node_id} forwards Node {e.originator_id}'s packet")

        if recv_events:
            receivers = list(set(e.node_id for e in recv_events))
            parts.append(f"Nodes {receivers} receive packets")

        return " | ".join(parts[:2]) if parts else "No events"

    def _get_active_packets(self, time_ms: float) -> List[Tuple[PacketAnimation, float]]:
        """Get packets that are in flight at the given time."""
        active = []
        for anim in self.packet_animations:
            if anim.start_time <= time_ms <= anim.end_time:
                duration = anim.end_time - anim.start_time
                if duration > 0:
                    progress = (time_ms - anim.start_time) / duration
                else:
                    progress = 1.0
                active.append((anim, progress))
        return active

    def _draw_frame(self, time_ms: float):
        """Draw a single frame of the animation."""
        self.ax_main.clear()

        # Get event summary for title
        event_summary = self._get_event_summary(time_ms)
        event_idx = self.current_event_index + 1
        total_events = len(self.sorted_event_times)

        self.ax_main.set_title(
            f'BLE Mesh Discovery - Time: {time_ms:.0f} ms  (Event {event_idx}/{total_events})\n{event_summary}',
            fontsize=12, fontweight='bold', pad=10)
        self.ax_main.axis('off')
        self.ax_main.set_xlim(-1.2, 1.2)
        self.ax_main.set_ylim(-1.0, 1.0)

        if len(self.G.nodes()) == 0:
            self.ax_main.text(0, 0, 'No topology data', ha='center', va='center', fontsize=14)
            return

        # Draw edges first (so they're behind everything)
        nx.draw_networkx_edges(self.G, self.pos, ax=self.ax_main,
                               edge_color='lightgray', width=3, alpha=0.7)

        # Get node states and colors
        states = self._get_node_states(time_ms)
        state_colors = {
            'idle': 'lightblue',
            'send': 'red',
            'recv': 'green',
            'both': 'orange'
        }
        node_colors = [state_colors[states[node]] for node in self.G.nodes()]

        # Draw nodes
        nx.draw_networkx_nodes(self.G, self.pos, ax=self.ax_main,
                               node_color=node_colors, node_size=1200,
                               edgecolors='black', linewidths=2)

        # Draw node ID labels
        nx.draw_networkx_labels(self.G, self.pos, ax=self.ax_main,
                                font_size=14, font_weight='bold')

        # Draw event labels beside nodes (to the left or right)
        event_labels = self._get_event_labels(time_ms)
        for node, labels in event_labels.items():
            if node in self.pos:
                x, y = self.pos[node]
                label_text = '\n'.join(labels[:2])  # Max 2 labels

                # Position label to the left or right of node depending on x position
                if x > 0:
                    label_x = x - 0.35
                    ha = 'right'
                else:
                    label_x = x + 0.35
                    ha = 'left'

                # Draw label with background box beside the node
                self.ax_main.annotate(
                    label_text,
                    xy=(x, y),
                    xytext=(label_x, y),
                    fontsize=8,
                    ha=ha,
                    va='center',
                    bbox=dict(boxstyle='round,pad=0.3', facecolor='yellow',
                              alpha=0.9, edgecolor='orange', linewidth=1),
                    arrowprops=dict(arrowstyle='->', color='orange', lw=1.5)
                )

        # Draw packets in flight
        active_packets = self._get_active_packets(time_ms)
        for anim, progress in active_packets:
            if anim.sender_id in self.pos and anim.receiver_id in self.pos:
                start_pos = np.array(self.pos[anim.sender_id])
                end_pos = np.array(self.pos[anim.receiver_id])

                # Interpolate position
                packet_pos = start_pos + progress * (end_pos - start_pos)

                # Draw packet as a larger, more visible circle
                color = self.originator_colors.get(anim.originator_id, 'gray')
                circle = plt.Circle(packet_pos, 0.06, color=color,
                                   ec='black', linewidth=2, zorder=15)
                self.ax_main.add_patch(circle)

                # Add small label showing originator
                self.ax_main.text(packet_pos[0], packet_pos[1] + 0.1,
                                 f'{anim.originator_id}',
                                 fontsize=7, ha='center', va='bottom',
                                 fontweight='bold', color='black')

    def _is_at_event_time(self, time_ms: float) -> bool:
        """Check if we're at an event time (should pause)."""
        for event_time in self.event_times:
            if abs(time_ms - event_time) < 1:
                return True
        return False

    def _on_slider_change(self, val):
        """Handle slider value change - snap to nearest event."""
        # Find nearest event time
        if self.sorted_event_times:
            nearest_idx = min(range(len(self.sorted_event_times)),
                            key=lambda i: abs(self.sorted_event_times[i] - val))
            self.current_event_index = nearest_idx
            self.current_time = self.sorted_event_times[nearest_idx]
        else:
            self.current_time = val
        self.frames_at_current_event = 0
        self._draw_frame(self.current_time)
        self.fig.canvas.draw_idle()

    def _toggle_play(self, event=None):
        """Toggle play/pause state."""
        self.playing = not self.playing
        self.btn_play.label.set_text('Pause' if self.playing else 'Play')

    def _change_speed(self, factor: float):
        """Change pause duration by a factor (inverse of speed)."""
        self.pause_duration = int(self.pause_duration / factor)
        self.pause_duration = max(10, min(120, self.pause_duration))
        self._update_speed_display()

    def _reset(self, event=None):
        """Reset to beginning."""
        self.current_event_index = 0
        self.current_time = self.sorted_event_times[0] if self.sorted_event_times else self.min_time
        self.frames_at_current_event = 0
        self.slider.set_val(self.current_time)
        self.playing = False
        self.btn_play.label.set_text('Play')
        self._draw_frame(self.current_time)
        self.fig.canvas.draw_idle()

    def _on_key_press(self, event):
        """Handle keyboard events."""
        if event.key == ' ':
            self._toggle_play()
        elif event.key == 'right':
            # Step to next event
            self._next_event()
            self.frames_at_current_event = 0
            self.slider.set_val(self.current_time)
            self._draw_frame(self.current_time)
            self.fig.canvas.draw_idle()
        elif event.key == 'left':
            # Step to previous event
            self._prev_event()
            self.frames_at_current_event = 0
            self.slider.set_val(self.current_time)
            self._draw_frame(self.current_time)
            self.fig.canvas.draw_idle()
        elif event.key == 'up':
            # Decrease pause duration (faster)
            self.pause_duration = max(10, self.pause_duration - 10)
            self._update_speed_display()
        elif event.key == 'down':
            # Increase pause duration (slower)
            self.pause_duration = min(120, self.pause_duration + 10)
            self._update_speed_display()
        elif event.key == 'r':
            self._reset()
        elif event.key == 'q':
            plt.close(self.fig)

    def _update_speed_display(self):
        """Update the speed display text."""
        pause_sec = self.pause_duration / 20.0  # Assuming 20 FPS
        self.speed_text.set_text(f'Pause: {pause_sec:.1f}s')
        self.fig.canvas.draw_idle()

    def _animate(self, frame):
        """Animation update function - EVENT-DRIVEN stepping."""
        self.current_frame = frame

        if self.playing and self.auto_advance:
            # Increment frames at current event
            self.frames_at_current_event += 1

            # Check if we've paused long enough at this event
            if self.frames_at_current_event >= self.pause_duration:
                # Move to next event
                self.frames_at_current_event = 0
                self._next_event()

            self.slider.set_val(self.current_time)

        self._draw_frame(self.current_time)
        return []

    def _next_event(self):
        """Advance to the next event in the timeline."""
        if self.current_event_index < len(self.sorted_event_times) - 1:
            self.current_event_index += 1
            self.current_time = self.sorted_event_times[self.current_event_index]
        else:
            # Loop back to start
            self.current_event_index = 0
            self.current_time = self.sorted_event_times[0] if self.sorted_event_times else self.min_time

    def _prev_event(self):
        """Go back to the previous event in the timeline."""
        if self.current_event_index > 0:
            self.current_event_index -= 1
            self.current_time = self.sorted_event_times[self.current_event_index]
        else:
            # Loop to end
            self.current_event_index = len(self.sorted_event_times) - 1
            self.current_time = self.sorted_event_times[self.current_event_index] if self.sorted_event_times else self.max_time

    def run(self):
        """Start the animated visualization."""
        # Initialize to first event
        if self.sorted_event_times:
            self.current_event_index = 0
            self.current_time = self.sorted_event_times[0]

        # Initial draw
        self._draw_frame(self.current_time)

        # Create animation - runs at 20 FPS, event-driven stepping
        self.anim = FuncAnimation(
            self.fig, self._animate,
            interval=50,  # 20 FPS
            blit=False,
            cache_frame_data=False
        )

        plt.show()


def main():
    parser = argparse.ArgumentParser(
        description='NetAnim-style animated visualization of BLE mesh discovery',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    parser.add_argument('trace_file', help='Path to the CSV trace file')

    args = parser.parse_args()

    if not Path(args.trace_file).exists():
        print(f"Error: Trace file not found: {args.trace_file}")
        sys.exit(1)

    print(f"Loading trace from: {args.trace_file}")
    print("\n" + "="*70)
    print("EVENT-DRIVEN VISUALIZATION")
    print("="*70)
    print("This visualization steps through each event in the simulation,")
    print("pausing to clearly show what's happening at each step.")
    print("")
    print("CONTROLS:")
    print("  SPACE       : Play/Pause auto-advance through events")
    print("  LEFT/RIGHT  : Manually step to previous/next event")
    print("  UP/DOWN     : Adjust pause duration (faster/slower)")
    print("  R           : Reset to first event")
    print("  Q           : Quit")
    print("")
    print("WHAT YOU'LL SEE:")
    print("  - Title shows current time, event number, and summary")
    print("  - Nodes change color: Blue=Idle, Red=Sending, Green=Receiving")
    print("  - Yellow labels explain exactly what each node is doing")
    print("  - Colored circles show packets moving between nodes")
    print("="*70)
    print("\nStarting visualization...")

    viz = AnimatedVisualizer(args.trace_file)
    viz.run()


if __name__ == '__main__':
    main()

