import pandas as pd
import matplotlib.pyplot as plt

# Load data with correct filename
df = pd.read_csv('bwp_lca_metrics.csv')  # FIX 1

# Convert delay to milliseconds and rename columns
df['Delay(ms)'] = df['Avg_HoL_Delay_s'] * 1000  # FIX 2
df['Throughput(Mbps)'] = df['Throughput_Mbps']  # FIX 3

# Use only existing columns
windows = df['Window']
throughput = df['Throughput(Mbps)']  # Use new column
delay = df['Delay(ms)']              # Use new column
dropped = df['Dropped']

# Create figure
fig, ax1 = plt.subplots(figsize=(12, 7))
fig.subplots_adjust(right=0.78)

# Throughput (left y-axis)
color1 = 'tab:blue'
p1, = ax1.plot(windows, throughput, color=color1, label='Throughput (Mbps)')
ax1.set_xlabel('Window')
ax1.set_ylabel('Throughput (Mbps)', color=color1)
ax1.tick_params(axis='y', labelcolor=color1)

# Delay (right y-axis)
ax2 = ax1.twinx()
color2 = 'tab:red'
p2, = ax2.plot(windows, delay, color=color2, label='Delay (ms)')
ax2.set_ylabel('Delay (ms)', color=color2)
ax2.tick_params(axis='y', labelcolor=color2)

# Dropped packets (third y-axis)
ax3 = ax1.twinx()
color3 = 'tab:green'
ax3.spines.right.set_position(("axes", 1.12))
p3, = ax3.plot(windows, dropped, color=color3, label='Dropped Packets')
ax3.set_ylabel('Dropped Packets', color=color3)
ax3.tick_params(axis='y', labelcolor=color3)

# REMOVED: Utilization, BWP, Achievement plots (columns don't exist)

# Combine legends
lines = [p1, p2, p3]
labels = [line.get_label() for line in lines]
ax1.legend(lines, labels, loc='upper left', bbox_to_anchor=(1.05, 1))

plt.title('BWP Performance Metrics Over Windows')
plt.tight_layout()
plt.show()
