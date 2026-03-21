#include "matching_engine.hpp"
#include <iostream>

int main() {    
    std::cout << "Generating synthetic ITCH data...\n";
    const uint32_t numOrders = 10000;
    ItchParser parser;
    parser.generateItch(numOrders);

    std::cout << "Running matching engine...\n";
    MatchingEngine engine;

    auto start = std::chrono::high_resolution_clock::now();
    engine.run();
    auto end = std::chrono::high_resolution_clock::now();

    auto elapsed = end - start;
    std::cout << "Matching engine completed in " << elapsed.count() << " milliseconds.\n";
    
    return 0;
}