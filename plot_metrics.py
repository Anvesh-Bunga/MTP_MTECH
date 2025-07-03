import pandas as pd
import matplotlib.pyplot as plt

# Load the CSVs
lca_df = pd.read_csv("lca_metrics.csv")
baseline_df = pd.read_csv("random_baseline.csv")

# Reward comparison
plt.figure()
plt.plot(lca_df['Window'], lca_df['Reward'], label='LCA Reward')
plt.axhline(y=baseline_df['Reward'].mean(), color='red', linestyle='--', label='Baseline Avg Reward')
plt.xlabel("Window")
plt.ylabel("Reward")
plt.title("Reward: LCA vs Random Baseline")
plt.legend()
plt.grid(True)
plt.show()

# Avg Delay
plt.figure()
plt.plot(lca_df['Window'], lca_df['AvgDelay'], label='Avg Delay')
plt.xlabel("Window")
plt.ylabel("Avg Delay (ms)")
plt.title("Average Delay per Window")
plt.grid(True)
plt.show()

# Throughput
plt.figure()
plt.plot(lca_df['Window'], lca_df['Throughput'], label='Throughput')
plt.xlabel("Window")
plt.ylabel("Throughput (bps)")
plt.title("Throughput per Window")
plt.grid(True)
plt.show()
