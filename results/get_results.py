import plot
import statistics

def main():
    # Load latencies from file
    latencies = statistics.load_latencies('latencies.txt')

    # Get statistics
    print("Calculating Latency Statistics...")
    statistics.get_stats(latencies)

    # Make plots
    print("Plotting Latencies Over Time...")
    plot.plot_latencies_over_time(latencies)
    
    print("Plotting Latency Histogram...")
    plot.plot_latency_histogram(latencies)

    print("Plotting Latency CDF...")
    plot.plot_latency_cdf(latencies)

if __name__ == "__main__":
    main()