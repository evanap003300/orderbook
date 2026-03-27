#include <chrono>
#include <iostream>
#include <vector>

#include "matching_engine.hpp"

int main() {
  std::cout << "Running matching engine...\n";
  MatchingEngine engine;

  auto start = std::chrono::high_resolution_clock::now();
  engine.run();
  auto end = std::chrono::high_resolution_clock::now();

  auto elapsed = end - start;
  std::cout << "Matching engine completed in " << elapsed.count()
            << " nanoseconds.\n";

  return 0;
}