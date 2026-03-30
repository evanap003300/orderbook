import matplotlib.pyplot as plt
import numpy as np

def plot_latencies_over_time(latencies):
    latencies = np.array(latencies)
    window = 10000
    rolling_avg = np.convolve(latencies, np.ones(window) / window, mode='valid')

    plt.figure(figsize=(10, 6))
    plt.plot(rolling_avg, label=f'Rolling Average (window={window})', linewidth=0.5)
    plt.xlabel('Message Index')
    plt.ylabel('Latency (ns)')
    plt.title('Latency of Matching Engine Over Time')
    plt.legend()
    plt.grid()
    plt.savefig('plots/latencies_timeseries.png')
    plt.show()

def plot_latency_histogram(latencies):
    latencies = np.array(latencies)
    p99 = np.percentile(latencies, 99)
    clipped = latencies[latencies <= p99]

    plt.figure(figsize=(10, 6))
    plt.hist(clipped, bins=100, edgecolor='black')
    plt.xlabel('Latency (ns)')
    plt.ylabel('Frequency')
    plt.title('Distribution of Latencies (clipped to p99)')
    plt.grid()
    plt.savefig('plots/latency_histogram.png')
    plt.show()

def plot_latency_cdf(latencies):
    latencies = np.sort(latencies)
    cdf = np.arange(len(latencies)) / len(latencies)

    plt.figure(figsize=(10, 6))
    plt.plot(latencies, cdf, label='CDF')
    plt.xlabel('Latency (ns)')
    plt.ylabel('Cumulative Probability')
    plt.title('CDF of Latencies')
    plt.legend()
    plt.grid()
    plt.savefig('plots/latency_cdf.png')
    plt.show()
