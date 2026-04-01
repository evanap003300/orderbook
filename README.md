# Matching Engine — Built for Speed

## Status
  - Median: 1.2 us per order                                                                                                                                                       
  - Mean: 5.7 us                                                                                                                                                                   
  - p99: 33 us                                                                                                                                                                     
  - p999: 474 us                                                                                                                                                                   
  - Throughput: ~56,000 orders/sec (full file run) 

## To-dos:
- [ ] Clean up code debt and rewrite tests (itch)
- [ ] Make it faster (custom memory management, cache allignment, core pinning, branchless programming, and more)
- [ ] Add fast logging
- [ ] Add resilence with a redundent engine
- [ ] Add support for more order types 
- [ ] Add a risk gateway to remove outliers
- [ ] Live data feed
