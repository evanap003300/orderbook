import matplotlib.pyplot as plt

# Timeseries data for latencies
def plot_latencies_over_time():
    latencies = []

    # Read latencies from file
    with open('latencies.txt', 'r') as f:
        for line in f:
            latencies.append(float(line.strip()))
    
    # Plot latencies
    plt.figure(figsize=(10, 6))
    plt.plot(latencies, label='Latency (nanoseconds)')
    plt.xlabel('Message Index')
    plt.ylabel('Latency (cycles)')
    plt.title('Latency of Matching Engine')
    plt.legend()
    plt.grid()
    plt.savefig('plots/latencies_timeseries.png')
    plt.show()

def plot_latency_histogram():
    latencies = []

    # Read latencies from file
    with open('latencies.txt', 'r') as f:
        for line in f:
            latencies.append(float(line.strip()))
    
    # Plot histogram of latencies
    plt.figure(figsize=(10, 6))
    plt.hist(latencies, bins=50, edgecolor='black')
    plt.xlabel('Latency (nanoseconds)')
    plt.ylabel('Frequency')
    plt.title('Distribution of Latencies')
    plt.grid()
    plt.savefig('plots/latency_histogram.png')
    plt.show()

def plot_latency_cdf():
    latencies = []

    # Read latencies from file
    with open('latencies.txt', 'r') as f:
        for line in f:
            latencies.append(float(line.strip()))
    
    # Sort latencies and compute CDF
    latencies.sort()
    cdf = [i / len(latencies) for i in range(len(latencies))]

    # Plot CDF of latencies
    plt.figure(figsize=(10, 6))
    plt.plot(latencies, cdf, label='CDF')
    plt.xlabel('Latency (nanoseconds)')
    plt.ylabel('Cumulative Probability')
    plt.title('CDF of Latencies')
    plt.legend()
    plt.grid()
    plt.savefig('plots/latency_cdf.png')
    plt.show()