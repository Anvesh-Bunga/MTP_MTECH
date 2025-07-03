import numpy as np
from collections import defaultdict, deque
import random
import matplotlib.pyplot as plt

# ======================
# OPTIMIZED PARAMETERS
# ======================
NUM_UES = 24                 # Reduced from 48 to decrease congestion
NUM_BWPS = 3                 
MAX_UES_PER_SLOT = 8         # Reduced from 16 for better resource allocation
SLOTS_PER_WINDOW = 500       
ALPHA = 0.3                  # Reduced delay weight (from 1.0)
BETA = 1.0                   # Balanced throughput weight (from 2.0)
GAMMA = 0.99                 # Increased future reward importance
EPSILON = 1.0
EPSILON_MIN = 0.05
EPSILON_DECAY = 0.995        # Faster exploration decay (from 0.9995)
LEARNING_RATE = 0.01         # Reduced learning rate (from 0.1)
EPISODES = 2000              # Increased training episodes (from 500)
WINDOWS_PER_EPISODE = 20     # More samples per episode (from 10)
MAX_QUEUE_SIZE = 200
WIFI_INTERFERENCE = [0.1, 0.2, 0.3]  # Reduced interference (from [0.2,0.4,0.6])
BWP_RBS = [50, 70, 100]
SLOTS_PER_SEC = 2000
SUBCARRIERS_PER_RB = 12
SYMBOLS_PER_SLOT = 14
BITS_PER_SYMBOL = 8
MAX_DELAY_THRESHOLD = 100    # New parameter for reward normalization

# Precompute T_max for each BWP (in Mbps)
T_MAX_LIST = [
    n_rb * SUBCARRIERS_PER_RB * SYMBOLS_PER_SLOT * BITS_PER_SYMBOL * SLOTS_PER_SEC / 1e6
    for n_rb in BWP_RBS
]

def ema_curve(data, alpha=0.1):
    ema = []
    prev = data[0]
    for x in data:
        prev = alpha * x + (1 - alpha) * prev
        ema.append(prev)
    return ema

class NRUUE:
    def __init__(self, ue_id):
        self.ue_id = ue_id
        self.queue = deque()
        self.hol_delay = 0
        self.avg_delay = 0
        self.mean_pkt_size = np.random.uniform(8, 20)  # Reduced packet sizes
        self.arrival_rate = np.random.uniform(0.1, 0.3)  # Reduced arrival rates
        self.current_C = 0
        self.cqi = np.random.uniform(0.7, 1.5)  # Higher minimum CQI
        self.avg_throughput = 1.0
        self.q_max = MAX_QUEUE_SIZE

    def generate_traffic(self, slot):
        arrivals = np.random.poisson(self.arrival_rate)
        for _ in range(arrivals):
            pkt_size = np.random.poisson(self.mean_pkt_size)
            pkt_size = max(0, min(30, pkt_size))  # Reduced max packet size
            if len(self.queue) < self.q_max:
                self.queue.append({'size': pkt_size, 'arrival_slot': slot, 'age': 0})

    def update_C(self):
        # Smoother capacity variations
        self.current_C = max(5, min(30, self.current_C + np.random.normal(0, 3)))
        
    def update_queue(self, served, slot, allocated_rbs, channel_quality):
        if self.queue:
            self.hol_delay = slot - self.queue[0]['arrival_slot']
        else:
            self.hol_delay = 0
            
        if served and self.queue:
            pkt = self.queue[0]
            capacity = self.current_C * allocated_rbs * channel_quality
            transmittable = min(pkt['size'], capacity)
            throughput = transmittable
            
            if pkt['size'] <= transmittable:
                self.queue.popleft()
                self.avg_delay = 0.95 * self.avg_delay + 0.05 * (slot - pkt['arrival_slot'])
            else:
                pkt['size'] -= transmittable
                self.avg_delay = 0.95 * self.avg_delay + 0.05 * self.hol_delay
                
            self.avg_throughput = 0.95 * self.avg_throughput + 0.05 * throughput
            return throughput
        else:
            self.avg_delay = 0.95 * self.avg_delay + 0.05 * self.hol_delay
            return 0

class NRUChannel:
    def __init__(self):
        self.channel_quality = {bwp: 1.0 for bwp in range(NUM_BWPS)}
        self.wifi_interference = WIFI_INTERFERENCE

    def update(self):
        for bwp in range(NUM_BWPS):
            # Smoother channel variations
            fading = 0.9 * self.channel_quality[bwp] + 0.1 * np.random.rayleigh() * (1 - self.wifi_interference[bwp])
            self.channel_quality[bwp] = max(0.3, min(1.5, fading))

def cat4_lbt(bwp_id, wifi_interference):
    if random.random() < wifi_interference[bwp_id]:
        cw = 4                       # Reduced initial CW
        cw_max = 64                  # Reduced max CW
        max_attempts = 10            # Increased attempts
        attempts = 0
        while attempts < max_attempts:
            backoff = random.randint(0, cw)
            for _ in range(backoff):
                if random.random() < wifi_interference[bwp_id]:
                    cw = min(cw_max, cw * 2)
                    attempts += 1
                    break
            else:
                return True
        return False
    else:
        return True

class PythonNRUEnv:
    def __init__(self):
        self.num_ues = NUM_UES
        self.num_bwps = NUM_BWPS
        self.max_ues_per_slot = MAX_UES_PER_SLOT
        self.slots_per_window = SLOTS_PER_WINDOW
        self.ues = [NRUUE(ue_id) for ue_id in range(self.num_ues)]
        self.channel = NRUChannel()
        self.bwp_assignments = [0] * self.num_ues  # Initialize with correct size

    def reset(self):
        self.ues = [NRUUE(ue_id) for ue_id in range(self.num_ues)]
        self.channel = NRUChannel()
        self.bwp_assignments = [random.randint(0, self.num_bwps - 1) for _ in range(self.num_ues)]
        return self.get_state()

    def get_state(self):
        # Cluster UEs by BWP for more efficient state representation
        state = []
        for bwp in range(NUM_BWPS):
            bwp_ues = [ue for ue in self.ues if self.bwp_assignments[ue.ue_id] == bwp]
            if bwp_ues:
                avg_delay = np.mean([ue.hol_delay for ue in bwp_ues])
                avg_cqi = np.mean([ue.cqi for ue in bwp_ues])
                total_queue = sum(len(ue.queue) for ue in bwp_ues)
                state.extend([
                    min(10, int(avg_delay / 5)),  # Discretized delay
                    min(10, int(avg_cqi * 2)),    # Discretized CQI
                    min(10, int(total_queue / 5)) # Discretized queue
                ])
            else:
                state.extend([0, 0, 0])
        return tuple(state)

    def step(self, action):
        # Ensure action length matches number of UEs
        if len(action) != self.num_ues:
            raise ValueError(f"Action length {len(action)} doesn't match number of UEs {self.num_ues}")
            
        self.bwp_assignments = action
        total_throughput = 0
        total_delay = 0
        successful_transmissions = 0

        for slot in range(self.slots_per_window):
            # Generate traffic and update channel conditions
            for ue in self.ues:
                ue.generate_traffic(slot)
                ue.update_C()
            self.channel.update()

            # Process each BWP
            for bwp in range(NUM_BWPS):
                bwp_ues = [ue for ue in self.ues if self.bwp_assignments[ue.ue_id] == bwp]
                if not bwp_ues:
                    continue
                    
                # LBT Procedure
                lbt_success = cat4_lbt(bwp, self.channel.wifi_interference)
                if not lbt_success:
                    for ue in bwp_ues:
                        ue.update_queue(served=False, slot=slot, allocated_rbs=0, channel_quality=0)
                    total_delay += sum(ue.hol_delay for ue in bwp_ues)
                    continue

                successful_transmissions += 1
                channel_quality = self.channel.channel_quality[bwp]
                n_rb = BWP_RBS[bwp]
                
                # Proportional Fair Scheduling
                pf_metrics = [
                    (ue, (channel_quality * ue.cqi) / (ue.avg_throughput + 1e-5))
                    for ue in bwp_ues
                ]
                scheduled_ues = sorted(pf_metrics, key=lambda x: x[1], reverse=True)[:self.max_ues_per_slot]
                scheduled_ues = [ue for ue, _ in scheduled_ues]
                
                # Resource Allocation
                allocated_rbs = n_rb // max(1, len(scheduled_ues))
                
                # Process transmissions
                for ue in scheduled_ues:
                    throughput = ue.update_queue(True, slot, allocated_rbs, channel_quality)
                    total_throughput += throughput
                    total_delay += ue.hol_delay
                    
                # Update unscheduled UEs
                for ue in [u for u in bwp_ues if u not in scheduled_ues]:
                    ue.update_queue(False, slot, 0, 0)
                    total_delay += ue.hol_delay

        # Calculate metrics
        avg_hol_delay = total_delay / (self.num_ues * self.slots_per_window)
        throughput = total_throughput / self.slots_per_window
        lbt_success_rate = successful_transmissions / self.slots_per_window
        
        # Improved Reward Function
        norm_throughput = throughput / np.mean(T_MAX_LIST)  # Normalize against average BWP capacity
        norm_delay = avg_hol_delay / MAX_DELAY_THRESHOLD
        reward = norm_throughput - ALPHA * norm_delay
        
        next_state = self.get_state()
        return next_state, reward, avg_hol_delay, throughput, lbt_success_rate

class QLearningAgent:
    def __init__(self, num_ues, num_bwps):
        self.num_ues = num_ues
        self.num_bwps = num_bwps
        # Using a single Q-table for clustered state representation
        self.q_table = defaultdict(lambda: np.zeros(num_bwps))
        self.lr = LEARNING_RATE
        self.gamma = GAMMA
        self.epsilon = EPSILON
        self.min_epsilon = EPSILON_MIN
        self.decay = EPSILON_DECAY

    def choose_action(self, state):
        if random.random() < self.epsilon:
            return [random.randint(0, self.num_bwps - 1) for _ in range(self.num_ues)]
        else:
            best_bwp = np.argmax(self.q_table[state])
            return [best_bwp for _ in range(self.num_ues)]

    def update(self, state, action, reward, next_state):
        # All UEs get the same action (simplified for clustered approach)
        current_q = self.q_table[state][action[0]]
        next_max_q = np.max(self.q_table[next_state])
        new_q = (1 - self.lr) * current_q + self.lr * (reward + self.gamma * next_max_q)
        self.q_table[state][action[0]] = new_q
        self.epsilon = max(self.min_epsilon, self.epsilon * self.decay)

def plot_results(rewards, delays, throughputs, lbt_rates):
    plt.figure(figsize=(20, 5))
    
    # Reward Plot
    plt.subplot(1, 4, 1)
    plt.plot(rewards, 'b-', alpha=0.3)
    plt.plot(ema_curve(rewards), 'c-', linewidth=2)
    plt.title("Reward Convergence")
    plt.xlabel("Episodes")
    plt.ylabel("Reward")
    plt.grid(True)
    
    # Delay Plot
    plt.subplot(1, 4, 2)
    plt.plot(delays, 'r-', alpha=0.3)
    plt.plot(ema_curve(delays), 'm-', linewidth=2)
    plt.title("Head-of-Line Delay")
    plt.xlabel("Episodes")
    plt.ylabel("Delay (slots)")
    plt.grid(True)
    
    # Throughput Plot
    plt.subplot(1, 4, 3)
    plt.plot(throughputs, 'g-', alpha=0.3)
    plt.plot(ema_curve(throughputs), 'k-', linewidth=2)
    plt.title("System Throughput")
    plt.xlabel("Episodes")
    plt.ylabel("Throughput (Mbps)")
    plt.grid(True)
    
    # LBT Success Rate
    plt.subplot(1, 4, 4)
    plt.plot(lbt_rates, 'orange', alpha=0.3)
    plt.plot(ema_curve(lbt_rates), 'brown', linewidth=2)
    plt.title("LBT Success Rate")
    plt.xlabel("Episodes")
    plt.ylabel("Success Rate")
    plt.grid(True)
    
    plt.tight_layout()
    plt.savefig("optimized_nru_results.png", dpi=300)
    plt.show()

def train_q_learning():
    env = PythonNRUEnv()
    agent = QLearningAgent(NUM_UES, NUM_BWPS)
    
    # Tracking metrics
    rewards = []
    delays = []
    throughputs = []
    lbt_rates = []
    
    print("Starting training with optimized parameters...")
    for episode in range(EPISODES):
        state = env.reset()
        episode_rewards = 0
        episode_delays = 0
        episode_throughputs = 0
        episode_lbt_rates = 0
        
        for _ in range(WINDOWS_PER_EPISODE):
            action = agent.choose_action(state)
            next_state, reward, delay, throughput, lbt_rate = env.step(action)
            agent.update(state, action, reward, next_state)
            state = next_state
            
            episode_rewards += reward
            episode_delays += delay
            episode_throughputs += throughput
            episode_lbt_rates += lbt_rate
            
        # Store episode averages
        rewards.append(episode_rewards / WINDOWS_PER_EPISODE)
        delays.append(episode_delays / WINDOWS_PER_EPISODE)
        throughputs.append(episode_throughputs / WINDOWS_PER_EPISODE)
        lbt_rates.append(episode_lbt_rates / WINDOWS_PER_EPISODE)
        
        # Progress reporting
        if (episode + 1) % 50 == 0:
            avg_reward = np.mean(rewards[-50:])
            avg_delay = np.mean(delays[-50:])
            avg_throughput = np.mean(throughputs[-50:])
            avg_lbt = np.mean(lbt_rates[-50:])
            print(f"Episode {episode+1}: Reward={avg_reward:.2f}, Delay={avg_delay:.1f}, "
                  f"Throughput={avg_throughput:.1f} Mbps, LBT={avg_lbt:.3f}")

    # Final evaluation
    final_reward = np.mean(rewards[-100:])
    final_delay = np.mean(delays[-100:])
    final_throughput = np.mean(throughputs[-100:])
    final_lbt = np.mean(lbt_rates[-100:])
    
    print("\nTraining completed!")
    print(f"Final 100-episode averages:")
    print(f"Reward: {final_reward:.2f}")
    print(f"Delay: {final_delay:.1f} slots")
    print(f"Throughput: {final_throughput:.1f} Mbps")
    print(f"LBT Success Rate: {final_lbt:.3f}")
    
    plot_results(rewards, delays, throughputs, lbt_rates)
    return rewards, delays, throughputs, lbt_rates

if __name__ == "__main__":
    np.random.seed(42)
    random.seed(42)
    train_q_learning()
