#!/usr/bin/env python3
"""
Simple Mesh Network Topology Visualizer

Generates a single graph showing the mesh network topology from a simulation trace CSV.
Shows node positions and connections based on successful packet receptions.

Usage:
    python3 visualize_topology.py <trace_file.csv> [output_filename.png]

Example:
    python3 visualize_topology.py physical_simulation_trace_random_toggle.csv
    python3 visualize_topology.py trace.csv my_topology.png
"""

import sys
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from collections import defaultdict
import subprocess
import platform

def load_trace(trace_file):
    """Load and parse the simulation trace CSV file."""
    print(f"Loading trace file: {trace_file}")

    # Read CSV with proper column names
    df = pd.read_csv(trace_file)
    df.columns = [col.strip() for col in df.columns]

    # Convert numeric columns
    numeric_cols = ['time_ms', 'sender_id', 'receiver_id', 'originator_id',
                    'ttl', 'path_length', 'rssi', 'latitude', 'longitude']
    for col in numeric_cols:
        if col in df.columns:
            df[col] = pd.to_numeric(df[col], errors='coerce')

    print(f"Loaded {len(df)} events")
    return df

def extract_topology(df):
    """Extract node positions and connections from trace."""

    # Get node positions from TOPOLOGY events
    topology_df = df[df['event'] == 'TOPOLOGY'].copy()
    if topology_df.empty:
        print("No TOPOLOGY rows found in trace")
        return {}, defaultdict(set), {}

    if 'latitude' not in topology_df.columns or 'longitude' not in topology_df.columns:
        print("Missing latitude/longitude columns in trace")
        return {}, defaultdict(set), {}

    if topology_df['longitude'].isna().all() and 'rssi' in topology_df.columns:
        rssi_vals = pd.to_numeric(topology_df['rssi'], errors='coerce')
        lat_vals = pd.to_numeric(topology_df['latitude'], errors='coerce')
        if rssi_vals.notna().any() and lat_vals.notna().any():
            topology_df['latitude'] = rssi_vals
            topology_df['longitude'] = lat_vals
            print("Detected shifted TOPOLOGY columns; using rssi/latitude as X/Y meters")

    topology_df = topology_df.dropna(subset=['latitude', 'longitude'])
    if topology_df.empty:
        print("TOPOLOGY rows missing latitude/longitude values")
        return {}, defaultdict(set), {}

    node_positions = {}

    for _, row in topology_df.iterrows():
        node_id = int(row['sender_id'])
        # Use latitude/longitude columns as X/Y meters
        x = row['latitude']
        y = row['longitude']
        node_positions[node_id] = (x, y)

    print(f"Found {len(node_positions)} nodes")
    print("Interpreting TOPOLOGY coordinates as meters")

    # Extract connections from RECV events
    recv_df = df[df['event'] == 'RECV'].copy()

    connections = defaultdict(set)
    rssi_values = defaultdict(list)

    for _, row in recv_df.iterrows():
        sender = int(row['originator_id']) if pd.notna(row['originator_id']) else None
        receiver = int(row['receiver_id']) if pd.notna(row['receiver_id']) else None
        rssi = row['rssi'] if pd.notna(row['rssi']) else None

        if sender and receiver and sender != receiver:
            # Create bidirectional connection
            edge = tuple(sorted([sender, receiver]))
            connections[edge].add((sender, receiver))
            if rssi:
                rssi_values[edge].append(rssi)

    # Calculate average RSSI for each connection
    avg_rssi = {}
    for edge, rssi_list in rssi_values.items():
        avg_rssi[edge] = np.mean(rssi_list)

    print(f"Found {len(connections)} unique connections")

    return node_positions, connections, avg_rssi

def plot_topology(node_positions, connections, avg_rssi, output_file='mesh_topology.png'):
    """Plot the mesh network topology."""

    fig, ax = plt.subplots(figsize=(14, 10))

    # Extract node coordinates
    node_ids = sorted(node_positions.keys())
    x_coords = [node_positions[nid][0] for nid in node_ids]
    y_coords = [node_positions[nid][1] for nid in node_ids]

    # Plot connections (edges)
    for edge, avg_rssi_value in avg_rssi.items():
        node1, node2 = edge
        if node1 in node_positions and node2 in node_positions:
            x1, y1 = node_positions[node1]
            x2, y2 = node_positions[node2]

            # Color based on RSSI (stronger = darker)
            # RSSI typically ranges from -90 to -40 dBm
            rssi_normalized = (avg_rssi_value + 90) / 50.0  # Normalize to 0-1
            rssi_normalized = max(0, min(1, rssi_normalized))  # Clamp

            # Use green for strong signals, yellow for medium, red for weak
            if rssi_normalized > 0.7:
                color = 'green'
                alpha = 0.7
                linewidth = 2.0
            elif rssi_normalized > 0.4:
                color = 'orange'
                alpha = 0.5
                linewidth = 1.5
            else:
                color = 'red'
                alpha = 0.3
                linewidth = 1.0

            ax.plot([x1, x2], [y1, y2], color=color, alpha=alpha,
                   linewidth=linewidth, zorder=1)

    # Plot nodes
    scatter = ax.scatter(x_coords, y_coords, c='blue', s=300,
                        alpha=0.8, edgecolors='black', linewidths=2, zorder=2)

    # Add node labels
    for node_id in node_ids:
        x, y = node_positions[node_id]
        ax.annotate(str(node_id), (x, y), fontsize=10, fontweight='bold',
                   ha='center', va='center', color='white', zorder=3)

    # Calculate network statistics
    num_nodes = len(node_positions)
    num_connections = len(connections)
    avg_degree = (2 * num_connections) / num_nodes if num_nodes > 0 else 0

    # Add title and labels
    ax.set_title(f'Mesh Network Topology\n'
                f'{num_nodes} Nodes, {num_connections} Connections, '
                f'Avg Degree: {avg_degree:.1f}',
                fontsize=16, fontweight='bold', pad=20)
    ax.set_xlabel('X Position (meters)', fontsize=12)
    ax.set_ylabel('Y Position (meters)', fontsize=12)
    ax.grid(True, alpha=0.3, linestyle='--')

    # Add legend for connection quality
    from matplotlib.lines import Line2D
    legend_elements = [
        Line2D([0], [0], color='green', linewidth=2, label='Strong (RSSI > -58 dBm)'),
        Line2D([0], [0], color='orange', linewidth=1.5, label='Medium (RSSI -58 to -72 dBm)'),
        Line2D([0], [0], color='red', linewidth=1, label='Weak (RSSI < -72 dBm)')
    ]
    ax.legend(handles=legend_elements, loc='upper right', fontsize=10)

    # Equal aspect ratio for proper spatial representation
    ax.set_aspect('equal', adjustable='box')

    plt.tight_layout()
    plt.savefig(output_file, dpi=150, bbox_inches='tight')
    print(f"\nTopology visualization saved to: {output_file}")

    # Print statistics
    print(f"\n=== Network Statistics ===")
    print(f"Total Nodes: {num_nodes}")
    print(f"Total Connections: {num_connections}")
    print(f"Average Node Degree: {avg_degree:.2f}")
    density = 0.0
    if num_nodes > 1:
        density = (2 * num_connections) / (num_nodes * (num_nodes - 1))
    print(f"Network Density: {density:.3f}")

    if avg_rssi:
        all_rssi = list(avg_rssi.values())
        print(f"\n=== RSSI Statistics ===")
        print(f"Average RSSI: {np.mean(all_rssi):.1f} dBm")
        print(f"Min RSSI: {np.min(all_rssi):.1f} dBm")
        print(f"Max RSSI: {np.max(all_rssi):.1f} dBm")

    plt.close()

def open_image(filename):
    """Open the generated image using the default system viewer."""
    try:
        system = platform.system()
        if system == 'Darwin':  # macOS
            subprocess.run(['open', filename], check=True)
        elif system == 'Linux':
            subprocess.run(['xdg-open', filename], check=True)
        elif system == 'Windows':
            subprocess.run(['start', filename], shell=True, check=True)
        else:
            print(f"Cannot auto-open on {system}. Please open {filename} manually.")
    except Exception as e:
        print(f"Could not open image automatically: {e}")
        print(f"Please open {filename} manually.")

def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    trace_file = sys.argv[1]
    output_file = sys.argv[2] if len(sys.argv) > 2 else 'mesh_topology.png'

    try:
        # Load trace
        df = load_trace(trace_file)

        # Extract topology
        node_positions, connections, avg_rssi = extract_topology(df)

        # Plot
        plot_topology(node_positions, connections, avg_rssi, output_file)

        # Open the image
        open_image(output_file)

        print("\nDone!")

    except FileNotFoundError:
        print(f"Error: Trace file '{trace_file}' not found")
        sys.exit(1)
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)

if __name__ == '__main__':
    main()
