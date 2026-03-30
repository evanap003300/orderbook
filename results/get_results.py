import plot
import statistics

def main():
    # Get statistics
    statistics.get_stats()

    # Make plots
    plot.plot_latencies_over_time()
    plot.plot_latency_histogram()
    plot.plot_latency_cdf()