#!/usr/bin/env python3
"""
NR-U Simulation Log Parser and Analyzer

Parses ns-3 simulation output files and generates performance analysis
and visualizations matching the paper's results.
"""

import argparse
import pandas as pd
import matplotlib.pyplot as plt
import xml.etree.ElementTree as ET
from pathlib import Path
import numpy as np

# Constants matching paper parameters
PAPER_ALPHA = 1.0  # Delay weight in reward function
PAPER_BETA = 1.0   # Throughput weight in reward function
BASELINE_DELAY = 1.0  # Normalized baseline values
BASELINE_THROUGHPUT = 1.0

def parse_simulation_csv(csv_file):
    """Parse the main simulation stats CSV file."""
    df = pd.read_csv(csv_file)
   
    # Calculate moving averages for smoothing
    df['AvgHolDelaySmooth'] = df['AvgHolDelay'].rolling(window=10).mean()
    df['TotalThroughputSmooth'] = df['TotalThroughput'].rolling(window=10).mean()
    df['RewardSmooth'] = df['Reward'].rolling(window=10).mean()
   
    return df

def parse_flow_monitor(xml_file):
    """Parse the flow monitor XML file for detailed metrics."""
    tree = ET.parse(xml_file)
    root = tree.getroot()
   
    metrics = {
        'flow': [],
        'tx_packets': [],
        'rx_packets': [],
        'delay_sum': [],
        'throughput': []
    }
   
    for flow in root.findall(".//Flow"):
        metrics['flow'].append(flow.get('flowId'))
        metrics['tx_packets'].append(float(flow.get('txPackets')))
        metrics['rx_packets'].append(float(flow.get('rxPackets')))
        metrics['delay_sum'].append(float(flow.get('delaySum')[:-2]))  # Remove 'ns'
        metrics['throughput'].append(float(flow.get('throughput')[:-7]))  # Remove 'bit/s'
   
    return pd.DataFrame(metrics)

def generate_plots(stats_df, algorithm, output_dir):
    """Generate plots matching paper's figures."""
    output_dir = Path(output_dir)
    output_dir.mkdir(exist_ok=True)
   
    # Plot 1: Reward convergence (Fig 2a)
    plt.figure(figsize=(10, 6))
    plt.plot(stats_df['Time'], stats_df['RewardSmooth'], label='Smoothed Reward')
    plt.xlabel('Time (s)')
    plt.ylabel('Reward')
    plt.title(f'Reward Convergence ({algorithm} Algorithm)')
    plt.grid(True)
    plt.savefig(output_dir / f'reward_convergence_{algorithm}.png')
    plt.close()
   
    # Plot 2: Delay and Throughput (Fig 2b)
    fig, ax1 = plt.subplots(figsize=(10, 6))
   
    color = 'tab:red'
    ax1.set_xlabel('Time (s)')
    ax1.set_ylabel('Avg HoL Delay', color=color)
    ax1.plot(stats_df['Time'], stats_df['AvgHolDelaySmooth'], color=color)
    ax1.tick_params(axis='y', labelcolor=color)
   
    ax2 = ax1.twinx()
    color = 'tab:blue'
    ax2.set_ylabel('Total Throughput', color=color)
    ax2.plot(stats_df['Time'], stats_df['TotalThroughputSmooth'], color=color)
    ax2.tick_params(axis='y', labelcolor=color)
   
    plt.title(f'Delay and Throughput ({algorithm} Algorithm)')
    fig.tight_layout()
    plt.savefig(output_dir / f'delay_throughput_{algorithm}.png')
    plt.close()
   
    # Plot 3: CDF of per-flow delays
    if 'flow_delays' in stats_df.columns:
        plt.figure(figsize=(10, 6))
        stats_df['flow_delays'].hist(cumulative=True, density=True,
                                    bins=100, histtype='step')
        plt.xlabel('Delay (ms)')
        plt.ylabel('CDF')
        plt.title(f'Per-Flow Delay CDF ({algorithm} Algorithm)')
        plt.grid(True)
        plt.savefig(output_dir / f'delay_cdf_{algorithm}.png')
        plt.close()

def compare_algorithms(results_dict, output_dir):
    """Generate comparative plots between algorithms."""
    output_dir = Path(output_dir)
   
    # Comparative reward plot
    plt.figure(figsize=(10, 6))
    for algo, df in results_dict.items():
        plt.plot(df['Time'], df['RewardSmooth'], label=algo)
    plt.xlabel('Time (s)')
    plt.ylabel('Reward')
    plt.title('Algorithm Comparison: Reward Convergence')
    plt.legend()
    plt.grid(True)
    plt.savefig(output_dir / 'comparison_reward.png')
    plt.close()
   
    # Comparative delay-throughput
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 12))
   
    for algo, df in results_dict.items():
        ax1.plot(df['Time'], df['AvgHolDelaySmooth'], label=algo)
        ax2.plot(df['Time'], df['TotalThroughputSmooth'], label=algo)
   
    ax1.set_ylabel('Avg HoL Delay')
    ax1.set_title('Algorithm Comparison: Delay')
    ax1.legend()
    ax1.grid(True)
   
    ax2.set_xlabel('Time (s)')
    ax2.set_ylabel('Total Throughput')
    ax2.set_title('Algorithm Comparison: Throughput')
    ax2.legend()
    ax2.grid(True)
   
    fig.tight_layout()
    plt.savefig(output_dir / 'comparison_delay_throughput.png')
    plt.close()
   
    # Calculate and print improvement percentages
    baseline = results_dict.get('BASELINE', None)
    lca = results_dict.get('LCA', None)
    rla = results_dict.get('RLA', None)
   
    if baseline is not None and lca is not None and rla is not None:
        # Calculate average metrics over stable period (last 50% of simulation)
        def avg_last_half(df, col):
            return df[col].iloc[len(df)//2:].mean()
       
        baseline_delay = avg_last_half(baseline, 'AvgHolDelay')
        lca_delay = avg_last_half(lca, 'AvgHolDelay')
        rla_delay = avg_last_half(rla, 'AvgHolDelay')
       
        baseline_throughput = avg_last_half(baseline, 'TotalThroughput')
        lca_throughput = avg_last_half(lca, 'TotalThroughput')
        rla_throughput = avg_last_half(rla, 'TotalThroughput')
       
        # Calculate improvements
        lca_delay_improvement = (baseline_delay - lca_delay) / baseline_delay * 100
        lca_throughput_improvement = (lca_throughput - baseline_throughput) / baseline_throughput * 100
       
        rla_delay_improvement = (baseline_delay - rla_delay) / baseline_delay * 100
        rla_throughput_improvement = (rla_throughput - baseline_throughput) / baseline_throughput * 100
       
        # Generate summary table
        summary = pd.DataFrame({
            'Algorithm': ['Baseline', 'LCA', 'RLA'],
            'Avg HoL Delay': [baseline_delay, lca_delay, rla_delay],
            'Delay Improvement %': ['-',
                                  f"{lca_delay_improvement:.2f}%",
                                  f"{rla_delay_improvement:.2f}%"],
            'Total Throughput': [baseline_throughput, lca_throughput, rla_throughput],
            'Throughput Improvement %': ['-',
                                       f"{lca_throughput_improvement:.2f}%",
                                       f"{rla_throughput_improvement:.2f}%"]
        })
       
        summary.to_markdown(output_dir / 'performance_summary.md')
        summary.to_csv(output_dir / 'performance_summary.csv')
       
        print("\nPerformance Summary:")
        print(summary.to_markdown())

def main():
    parser = argparse.ArgumentParser(description='Parse and analyze NR-U simulation logs')
    parser.add_argument('--csv', required=True, help='Input CSV file from simulation')
    parser.add_argument('--xml', help='Flow monitor XML file')
    parser.add_argument('--algorithm', required=True,
                       choices=['LCA', 'RLA', 'BASELINE'],
                       help='Algorithm used in simulation')
    parser.add_argument('--output', default='results',
                       help='Output directory for plots and analysis')
   
    args = parser.parse_args()
   
    # Parse simulation data
    stats_df = parse_simulation_csv(args.csv)
   
    if args.xml:
        flow_df = parse_flow_monitor(args.xml)
        # Calculate per-flow delays in milliseconds
        flow_df['delay_ms'] = flow_df['delay_sum'] / (flow_df['rx_packets'] * 1e6)
        stats_df['flow_delays'] = flow_df['delay_ms'].mean()
   
    # Generate plots
    generate_plots(stats_df, args.algorithm, args.output)
   
    print(f"Analysis complete. Results saved to {args.output}")

if __name__ == "__main__":
    main()
