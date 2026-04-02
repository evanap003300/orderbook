import numpy as np

def load_latencies(filename):
    return np.loadtxt(filename)

def get_stats(latencies):
    print(f'Mean Latency: {np.mean(latencies):.2f} nanoseconds')
    print(f'Median Latency: {np.median(latencies):.2f} nanoseconds')
    print(f'Std Dev: {np.std(latencies):.2f} nanoseconds')
    print(f'99th Percentile Latency: {np.percentile(latencies, 99):.2f} nanoseconds')
    print(f'99.9th Percentile Latency: {np.percentile(latencies, 99.9):.2f} nanoseconds')
