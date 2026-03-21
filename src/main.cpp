#include "matching_engine.hpp"

int main() {    
    ItchParser parser;
    parser.generateItch(1000); // Generate 1000 synthetic orders for testing

    MatchingEngine engine;
    engine.run();
    
    return 0;
}