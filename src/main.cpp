#include "matching_engine.hpp"
#include <iostream>

int main() {    
    std::cout << "Generating synthetic ITCH data...\n";
    ItchParser parser;
    parser.generateItch(10000);

    std::cout << "Running matching engine...\n";
    MatchingEngine engine;
    engine.run();
    
    return 0;
}