# Tests Writing Spec

## Simple Functionality
* Placing a bid order works
* Placing a ask order works
* Reading in orders works

## Cases to cover
* Price should have precedence
* Time should have precedence if there is a tie
* Current order should match with multiple orders until there is no more stock or no more orders
* Current order should be added if there is no stock to match with
* No match should occur if buying price is slower than ask
* No match should occur if selling price is higher than bid