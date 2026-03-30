def get_stats():
    latencies = []

    # Read latencies from file
    with open('latencies.txt', 'r') as f:
        for line in f:
            latencies.append(float(line.strip()))

    # Compute statistics
    mean_latency = sum(latencies) / len(latencies)
    median_latency = sorted(latencies)[len(latencies) // 2]
    p99_latency = sorted(latencies)[int(len(latencies) * 0.99)]
    p999_latency = sorted(latencies)[int(len(latencies) * 0.999)]

    print(f'Mean Latency: {mean_latency:.2f} nanoseconds')
    print(f'Median Latency: {median_latency:.2f} nanoseconds')
    print(f'99th Percentile Latency: {p99_latency:.2f} nanoseconds')
    print(f'99.9th Percentile Latency: {p999_latency:.2f} nanoseconds')