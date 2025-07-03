import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import os

# --- File mapping: filename to arrival rate
file_map = {
    'bwp_metrics.csv': 0.0025,
    'bwp_metrics1.csv': 0.005,
    'bwp_metrics2.csv': 0.02,
    'bwp_metrics3.csv': 0.1,
    'bwp_metrics4.csv': 0.2,
    'bwp_metrics6.csv': 0.5,
    # Add more files here if you have them, e.g.:
    # 'bwp_metrics6.csv': 0.5,
}

data_dir = 'data'
output_dir = 'plots'
os.makedirs(output_dir, exist_ok=True)

# --- Load and combine data
dfs = []
for filename, rate in file_map.items():
    path = os.path.join(data_dir, filename)
    df = pd.read_csv(path)
    df['Arrival Rate'] = rate
    # Handle missing columns
    if 'Dropped' not in df.columns:
        df['Dropped'] = 0
    dfs.append(df)

combined_df = pd.concat(dfs, ignore_index=True)

# --- Compute summary statistics per arrival rate
stats = combined_df.groupby('Arrival Rate').agg({
    'Throughput (bits)': 'mean',
    'Avg HoL Delay (s)': 'mean',
    'Dropped': 'mean'
}).reset_index()

# --- Plot 1: Throughput over time for all arrival rates
plt.figure(figsize=(12, 6))
for rate in sorted(combined_df['Arrival Rate'].unique()):
    df = combined_df[combined_df['Arrival Rate'] == rate]
    plt.plot(df['Window'], df['Throughput (bits)'], label=f'λ={rate}')
plt.xlabel('Window')
plt.ylabel('Throughput (bits)')
plt.title('Throughput Over Time for Different Arrival Rates')
plt.legend()
plt.tight_layout()
plt.savefig(os.path.join(output_dir, 'throughput_over_time.png'), dpi=300)
plt.close()

# --- Plot 2: Avg HOL Delay over time for all arrival rates
plt.figure(figsize=(12, 6))
for rate in sorted(combined_df['Arrival Rate'].unique()):
    df = combined_df[combined_df['Arrival Rate'] == rate]
    plt.plot(df['Window'], df['Avg HoL Delay (s)'], label=f'λ={rate}')
plt.xlabel('Window')
plt.ylabel('Avg HoL Delay (s)')
plt.title('Head-of-Line Delay Over Time for Different Arrival Rates')
plt.legend()
plt.tight_layout()
plt.savefig(os.path.join(output_dir, 'hol_delay_over_time.png'), dpi=300)
plt.close()

# --- Plot 3: Dropped packets over time for all arrival rates
plt.figure(figsize=(12, 6))
for rate in sorted(combined_df['Arrival Rate'].unique()):
    df = combined_df[combined_df['Arrival Rate'] == rate]
    plt.plot(df['Window'], df['Dropped'], label=f'λ={rate}')
plt.xlabel('Window')
plt.ylabel('Dropped Packets')
plt.title('Packet Drops Over Time for Different Arrival Rates')
plt.legend()
plt.tight_layout()
plt.savefig(os.path.join(output_dir, 'dropped_over_time.png'), dpi=300)
plt.close()

# --- Plot 4: Summary comparison bar plots
fig, axs = plt.subplots(1,3 , figsize=(18, 5))
sns.barplot(data=stats, x='Arrival Rate', y='Throughput (bits)', ax=axs[0])
axs[0].set_title('Mean Throughput')
sns.barplot(data=stats, x='Arrival Rate', y='Avg HoL Delay (s)', ax=axs[1])
axs[1].set_title('Mean HOL Delay')
sns.barplot(data=stats, x='Arrival Rate', y='Dropped', ax=axs[2])
axs[2].set_title('Mean Dropped Packets')
for ax in axs:
    ax.set_xlabel('Arrival Rate')
plt.suptitle('Comparison of Metrics Across Arrival Rates')
plt.tight_layout(rect=[0, 0, 1, 0.96])
plt.savefig(os.path.join(output_dir, 'metrics_comparison.png'), dpi=300)
plt.close()

# --- Save summary statistics
stats.to_csv(os.path.join(output_dir, 'metrics_summary.csv'), index=False)

print("Analysis complete. Plots and summary saved in the 'plots' directory.")

