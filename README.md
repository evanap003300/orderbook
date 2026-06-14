# Matching Engine — Built for Speed

## Status
Measured over 136.5M Add messages on a full NASDAQ ITCH 5.0 trading day:
- Mean Latency: 84.1 nanoseconds
- Median Latency: 42 nanoseconds
- 90th Percentile Latency: 167 nanoseconds
- 99th Percentile Latency: 709 nanoseconds
- 99.9th Percentile Latency: 2,042 nanoseconds
- 99.99th Percentile Latency: 9,750 nanoseconds

## To-dos:
- [ ] Make it faster (custom memory management, cache allignment, core pinning, branchless programming, and more)
- [ ] Do something with executed results
- [ ] Add fast logging
- [ ] Add resilence with a redundent engine
- [ ] Add support for more order types 
- [ ] Add a risk gateway to remove outliers
- [ ] Live data feed
