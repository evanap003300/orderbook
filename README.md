# Matching Engine — Built for Speed

## Status
* Just finished writing the intial matching engine with synthetic data testing 
* Currently operating at ~2 million orders per second
* Using pretty optimal data structures and algorithims
* Need to enhance the infra and optimize at the hardware level

## To-dos:
- [ ] Process one order at a time 
- [ ] Track and store timing metrics with production tracking
- [ ] Add fast logging
- [ ] Add support for more order types 
- [ ] Plot results using python
- [ ] Make it faster (custom memory management, cache allignment, core pinning, and more)
- [ ] Add resilence with a redundent engine
- [ ] Add a risk gateway to remove outliers
