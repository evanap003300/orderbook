#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "matching_engine.hpp"

int main() {
  std::cout << "Running matching engine...\n";
  MatchingEngine engine;

  engine.run();

  std::cout << "Saving latencies...\n";

  std::ofstream latencyFile("latencies.txt");
  for (const auto& latency : engine.latencies) {
    latencyFile << latency << "\n";
  }
  latencyFile.close();
  return 0;
}