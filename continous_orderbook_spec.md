# Continous Orderbook Impl.

## Goal: Make the orderbook continous so it can work with real-time data.

## Overview
* Need to make a loop which continously takes in orders and then proceses them
* Needs to be able to do the following:
    * Read in the itch from the binary and turn it into an order
    * Process the current order
    * Remove any orders which have now been fufilled
    * Add the current order with it's correct shares amount if non-zero

## To-dos
- [ ] Update data structures to handle fast deletion
- [ ] Handle time-based priority
- [ ] Add removing fufiled orders 
- [ ] Add adding current order

## Updating Data Structures
* Use a deque instead of a vector to have efficent FIFO operations, also allows for canceling of orders from back efficently
* Instead of the map holding vectors with all the orders, change this to split into bid and asks order deques
    * This would be like a map which maps tickers to vectors which store a bid and ask map which maps thier price to a deque of Orders

## Matching engine:
* The orderbook will handle the matching and updating the book based on new orders
* The matching engine will handling passing the bids and asks for a ticker so it will contain a map of tickers to Orderbooks