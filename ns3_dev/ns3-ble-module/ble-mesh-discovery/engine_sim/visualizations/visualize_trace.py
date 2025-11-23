

import argparse
import sys
from pathlib import Path

import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from matplotlib.animation import FuncAnimation
import networkx as nx


def load_trace(filepath: str) -> pd.DataFrame:
    """
    Load and parse the CSV trace file from the NS-3 simulation.

    The CSV has columns: time_ms, event, sender_id, receiver_id, originator_id, ttl, path_length, rssi

    Note: Some columns may be empty depending on event type:
    - SEND: receiver_id and rssi are empty (broadcast)
    - RECV: sender_id is empty (inferred from context)
    - TOPOLOGY: originator_id, ttl, path_length, rssi are empty
    - STATS: columns are repurposed (see docstring in C++ code)
    """
    df = pd.read_csv(filepath)

    numeric_cols = ['time_ms', 'sender_id', 'receiver_id', 'originator_id', 'ttl', 'path_length', 'rssi']
    for col in numeric_cols:
        if col in df.columns:
            df[col] = pd.to_numeric(df[col], errors='coerce')

    return df


def extract_topology(df: pd.DataFrame) -> nx.Graph:
    """
    Build a NetworkX graph from TOPOLOGY events.

    TOPOLOGY events are logged at t=0 and define which nodes can communicate.
    Each row represents a bidirectional link: sender_id <-> receiver_id
    """
    G = nx.Graph()

    topology_df = df[df['event'] == 'TOPOLOGY']

    for _, row in topology_df.iterrows():
        node_a = int(row['sender_id'])
        node_b = int(row['receiver_id'])
        G.add_edge(node_a, node_b)

    for _, row in df[df['event'] == 'SEND'].iterrows():
        if pd.notna(row['sender_id']):
            G.add_node(int(row['sender_id']))

    return G


def extract_stats(df: pd.DataFrame) -> pd.DataFrame:
    """
    Extract final statistics from STATS events.

    STATS events repurpose columns:
    - sender_id -> node_id
    - receiver_id -> messages_sent
    - originator_id -> messages_received
    - ttl -> messages_forwarded
    - path_length -> messages_dropped
    """
    stats_df = df[df['event'] == 'STATS'].copy()

    if stats_df.empty:
        return pd.DataFrame()

    stats_df = stats_df.rename(columns={
        'sender_id': 'node_id',
        'receiver_id': 'messages_sent',
        'originator_id': 'messages_received',
        'ttl': 'messages_forwarded',
        'path_length': 'messages_dropped'
    })

    return stats_df[['node_id', 'messages_sent', 'messages_received',
                     'messages_forwarded', 'messages_dropped']]


def plot_topology(G: nx.Graph, ax: plt.Axes, title: str = "Mesh Topology") -> dict:
    """
    Draw the network topology graph.

    Returns the position dictionary for consistent node placement across plots.

    The layout algorithm (spring_layout) positions nodes so that:
    - Connected nodes are closer together
    - Nodes repel each other to avoid overlap
    """
    pos = nx.spring_layout(G, seed=42, k=2)

    nx.draw_networkx_edges(G, pos, ax=ax, edge_color='gray', width=2, alpha=0.6)

    nx.draw_networkx_nodes(G, pos, ax=ax, node_color='lightblue',
                           node_size=700, edgecolors='black', linewidths=2)

    nx.draw_networkx_labels(G, pos, ax=ax, font_size=12, font_weight='bold')

    ax.set_title(title)
    ax.axis('off')

    return pos


def plot_packet_timeline(df: pd.DataFrame, ax: plt.Axes):
    """
    Create a timeline showing when packets are sent and received.

    This visualization helps understand:
    - The temporal pattern of discovery (which nodes start first)
    - Propagation delays (1ms between SEND and RECV)
    - Forwarding behavior (SEND events with path_length > 0)

    Y-axis: Node ID
    X-axis: Time in milliseconds
    Markers:
    - Triangles pointing right (▷): SEND events
    - Circles (●): RECV events
    Colors indicate the originator of the packet (who created the discovery)
    """
    send_df = df[df['event'] == 'SEND'].copy()
    recv_df = df[df['event'] == 'RECV'].copy()

    all_originators = set()
    all_originators.update(send_df['originator_id'].dropna().astype(int).unique())
    all_originators.update(recv_df['originator_id'].dropna().astype(int).unique())

    cmap = plt.cm.tab10
    originator_colors = {orig: cmap(i % 10) for i, orig in enumerate(sorted(all_originators))}

    for _, row in send_df.iterrows():
        originator = int(row['originator_id']) if pd.notna(row['originator_id']) else int(row['sender_id'])
        color = originator_colors.get(originator, 'gray')

        marker = '>' if row['path_length'] == 0 or pd.isna(row['path_length']) else 's'
        size = 100 if marker == '>' else 60

        ax.scatter(row['time_ms'], row['sender_id'],
                   c=[color], marker=marker, s=size, edgecolors='black', linewidths=0.5)

    for _, row in recv_df.iterrows():
        originator = int(row['originator_id']) if pd.notna(row['originator_id']) else 0
        color = originator_colors.get(originator, 'gray')
        ax.scatter(row['time_ms'], row['receiver_id'],
                   c=[color], marker='o', s=40, alpha=0.6, edgecolors='black', linewidths=0.5)

    legend_elements = []
    for orig, color in sorted(originator_colors.items()):
        legend_elements.append(mpatches.Patch(color=color, label=f'Originator {int(orig)}'))
    legend_elements.append(plt.Line2D([0], [0], marker='>', color='w', markerfacecolor='gray',
                                       markersize=10, label='Self-advertisement'))
    legend_elements.append(plt.Line2D([0], [0], marker='s', color='w', markerfacecolor='gray',
                                       markersize=8, label='Forward'))
    legend_elements.append(plt.Line2D([0], [0], marker='o', color='w', markerfacecolor='gray',
                                       markersize=8, label='Receive', alpha=0.6))

    ax.legend(handles=legend_elements, loc='upper right', fontsize=8)
    ax.set_xlabel('Time (ms)')
    ax.set_ylabel('Node ID')
    ax.set_title('Packet Timeline: Discovery Propagation')
    ax.grid(True, alpha=0.3)


def plot_statistics(stats_df: pd.DataFrame, ax: plt.Axes):
    """
    Bar chart showing per-node statistics.

    This helps identify:
    - Which nodes are most active (high messages_sent)
    - Hub nodes (high messages_forwarded)
    - Potential issues (high messages_dropped)
    """
    if stats_df.empty:
        ax.text(0.5, 0.5, 'No statistics available', ha='center', va='center')
        ax.set_title('Node Statistics')
        return

    x = stats_df['node_id'].astype(int)
    width = 0.2

    ax.bar(x - 1.5*width, stats_df['messages_sent'], width, label='Sent', color='steelblue')
    ax.bar(x - 0.5*width, stats_df['messages_received'], width, label='Received', color='forestgreen')
    ax.bar(x + 0.5*width, stats_df['messages_forwarded'], width, label='Forwarded', color='orange')
    ax.bar(x + 1.5*width, stats_df['messages_dropped'], width, label='Dropped', color='crimson')

    ax.set_xlabel('Node ID')
    ax.set_ylabel('Message Count')
    ax.set_title('Per-Node Message Statistics')
    ax.set_xticks(x)
    ax.legend()
    ax.grid(True, alpha=0.3, axis='y')


def plot_propagation_heatmap(df: pd.DataFrame, G: nx.Graph, ax: plt.Axes):
    """
    Heatmap showing packet flow between node pairs.

    This matrix visualization shows:
    - Rows: Sending nodes
    - Columns: Receiving nodes
    - Color intensity: Number of packets sent from row to column

    Helps identify:
    - Heavily used links
    - Asymmetric communication patterns
    - Potential bottlenecks
    """
    nodes = sorted(G.nodes())
    n = len(nodes)

    if n == 0:
        ax.text(0.5, 0.5, 'No nodes found', ha='center', va='center')
        return

    flow_matrix = pd.DataFrame(0, index=nodes, columns=nodes)

    send_df = df[df['event'] == 'SEND']
    recv_df = df[df['event'] == 'RECV']

    for _, send_row in send_df.iterrows():
        sender = int(send_row['sender_id'])
        send_time = send_row['time_ms']
        originator = send_row['originator_id']
        ttl = send_row['ttl']

        matching_recvs = recv_df[
            (recv_df['time_ms'] == send_time + 1) &
            (recv_df['originator_id'] == originator) &
            (recv_df['ttl'] == ttl)
        ]

        for _, recv_row in matching_recvs.iterrows():
            receiver = int(recv_row['receiver_id'])
            if sender in flow_matrix.index and receiver in flow_matrix.columns:
                flow_matrix.loc[sender, receiver] += 1

    im = ax.imshow(flow_matrix.values, cmap='YlOrRd', aspect='equal')

    ax.set_xticks(range(n))
    ax.set_yticks(range(n))
    ax.set_xticklabels(nodes)
    ax.set_yticklabels(nodes)
    ax.set_xlabel('Receiver Node')
    ax.set_ylabel('Sender Node')
    ax.set_title('Packet Flow Matrix (Sender → Receiver)')

    plt.colorbar(im, ax=ax, label='Packet Count')

    for i in range(n):
        for j in range(n):
            val = flow_matrix.iloc[i, j]
            if val > 0:
                ax.text(j, i, str(int(val)), ha='center', va='center', fontsize=8)


def plot_ttl_decay(df: pd.DataFrame, ax: plt.Axes):
    """
    Show how TTL decreases as packets propagate through the network.

    This visualization demonstrates:
    - Multi-hop forwarding (TTL decreases with each hop)
    - Packet reach (how far discovery messages travel)
    - The relationship between path_length and TTL

    Each line represents packets from a single originator.
    """
    send_df = df[df['event'] == 'SEND'].copy()

    if send_df.empty:
        ax.text(0.5, 0.5, 'No send events', ha='center', va='center')
        return

    originators = send_df['originator_id'].dropna().unique()

    cmap = plt.cm.tab10

    for i, orig in enumerate(sorted(originators)):
        orig_packets = send_df[send_df['originator_id'] == orig].copy()
        orig_packets = orig_packets.sort_values('time_ms')

        path_lengths = orig_packets['path_length'].fillna(0)
        ttls = orig_packets['ttl']

        ax.scatter(path_lengths, ttls, c=[cmap(i % 10)],
                   label=f'Originator {int(orig)}', alpha=0.7, s=50)

    ax.set_xlabel('Path Length (hops traveled)')
    ax.set_ylabel('TTL (remaining hops)')
    ax.set_title('TTL Decay vs Path Length')
    ax.legend()
    ax.grid(True, alpha=0.3)


def create_summary_figure(df: pd.DataFrame, output_path: str = None):
    """
    Create a comprehensive 2x2 figure with all visualizations.
    """
    fig, axes = plt.subplots(2, 2, figsize=(14, 12))
    fig.suptitle('BLE Mesh Discovery Simulation Analysis', fontsize=14, fontweight='bold')

    G = extract_topology(df)
    stats_df = extract_stats(df)

    plot_topology(G, axes[0, 0], "Mesh Topology")

    plot_packet_timeline(df, axes[0, 1])

    plot_statistics(stats_df, axes[1, 0])

    plot_ttl_decay(df, axes[1, 1])

    plt.tight_layout()

    if output_path:
        plt.savefig(output_path, dpi=150, bbox_inches='tight')
        print(f"Saved figure to: {output_path}")

    return fig


def create_animation(df: pd.DataFrame, G: nx.Graph, output_path: str = None):
    """
    Create an animated visualization showing packet propagation over time.

    This animation shows:
    - Nodes lighting up when they send packets
    - Edges highlighting when packets travel across them
    - Time progression in the title
    """
    fig, ax = plt.subplots(figsize=(10, 8))

    pos = nx.spring_layout(G, seed=42, k=2)

    event_df = df[df['event'].isin(['SEND', 'RECV'])]
    timestamps = sorted(event_df['time_ms'].unique())

    if len(timestamps) == 0:
        print("No events to animate")
        return None

    def update(frame):
        ax.clear()
        current_time = timestamps[frame]

        nx.draw_networkx_edges(G, pos, ax=ax, edge_color='lightgray', width=2, alpha=0.5)

        current_events = event_df[event_df['time_ms'] == current_time]

        node_colors = []
        for node in G.nodes():
            if node in current_events[current_events['event'] == 'SEND']['sender_id'].values:
                node_colors.append('red')
            elif node in current_events[current_events['event'] == 'RECV']['receiver_id'].values:
                node_colors.append('green')
            else:
                node_colors.append('lightblue')

        nx.draw_networkx_nodes(G, pos, ax=ax, node_color=node_colors,
                               node_size=700, edgecolors='black', linewidths=2)
        nx.draw_networkx_labels(G, pos, ax=ax, font_size=12, font_weight='bold')

        ax.set_title(f'Time: {current_time} ms')
        ax.axis('off')

        legend_elements = [
            mpatches.Patch(color='red', label='Sending'),
            mpatches.Patch(color='green', label='Receiving'),
            mpatches.Patch(color='lightblue', label='Idle'),
        ]
        ax.legend(handles=legend_elements, loc='upper left')

    n_frames = min(len(timestamps), 100)
    anim = FuncAnimation(fig, update, frames=n_frames, interval=100, repeat=True)

    if output_path:
        anim.save(output_path, writer='pillow', fps=10)
        print(f"Saved animation to: {output_path}")

    return anim


def print_summary(df: pd.DataFrame):
    """Print a text summary of the simulation trace."""
    print("\n" + "="*60)
    print("SIMULATION TRACE SUMMARY")
    print("="*60)

    event_counts = df['event'].value_counts()
    print("\nEvent counts:")
    for event, count in event_counts.items():
        print(f"  {event}: {count}")

    event_df = df[df['event'].isin(['SEND', 'RECV'])]
    if not event_df.empty:
        print(f"\nTime range: {event_df['time_ms'].min()} ms - {event_df['time_ms'].max()} ms")

    stats_df = extract_stats(df)
    if not stats_df.empty:
        print("\nPer-node statistics:")
        print(stats_df.to_string(index=False))

    G = extract_topology(df)
    print(f"\nTopology: {G.number_of_nodes()} nodes, {G.number_of_edges()} edges")
    print(f"Edges: {list(G.edges())}")

    print("="*60 + "\n")


def main():
    parser = argparse.ArgumentParser(
        description='Visualize BLE Mesh Discovery simulation traces',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    parser.add_argument('trace_file', help='Path to the CSV trace file')
    parser.add_argument('--output', '-o', default='.', help='Output directory for figures')
    parser.add_argument('--animate', '-a', action='store_true', help='Create animation (requires more time)')
    parser.add_argument('--no-show', action='store_true', help='Do not display plots interactively')

    args = parser.parse_args()

    if not Path(args.trace_file).exists():
        print(f"Error: Trace file not found: {args.trace_file}")
        sys.exit(1)

    output_dir = Path(args.output)
    output_dir.mkdir(parents=True, exist_ok=True)

    print(f"Loading trace from: {args.trace_file}")
    df = load_trace(args.trace_file)

    print_summary(df)

    summary_path = output_dir / 'simulation_summary.png'
    fig = create_summary_figure(df, str(summary_path))

    G = extract_topology(df)
    if G.number_of_nodes() > 0:
        fig2, ax2 = plt.subplots(figsize=(8, 6))
        plot_propagation_heatmap(df, G, ax2)
        heatmap_path = output_dir / 'flow_heatmap.png'
        plt.savefig(str(heatmap_path), dpi=150, bbox_inches='tight')
        print(f"Saved heatmap to: {heatmap_path}")

    if args.animate:
        anim_path = output_dir / 'simulation_animation.gif'
        create_animation(df, G, str(anim_path))

    if not args.no_show:
        plt.show()

    print("\nVisualization complete!")


if __name__ == '__main__':
    main()
