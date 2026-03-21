# Matching Engine Impl.

## Goal: Create a matching engine which handles matching orders for all tickers

## Overview
* Handles main loop logic for exchange
* Maps tickers to orderbooks 
* Based on the current order it passes it to the correct orderbook

## Implemenation
* Create a unordred_map that maps tickers to an orderbook
* Start a loop that repeats this process: 
    * Read in an order
    * Convert its ticker to a string
    * Send the order to the orderbook that the map maps the ticker to
    * Output the result to stdout (swtich to a fast logging lib. later)