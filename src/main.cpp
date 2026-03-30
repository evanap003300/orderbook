#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "matching_engine.hpp"

int main() {
  std::cout << "Running matching engine...\n";
  MatchingEngine engine;

  auto start = std::chrono::high_resolution_clock::now();
  engine.run();
  auto end = std::chrono::high_resolution_clock::now();

  std::cout << "Stored: " << engine.latencies.size() << " latencies.";

  std::ofstream latencyFile("latencies.txt");
  for (const auto& latency : engine.latencies) {
    latencyFile << latency << "\n";
  }
  latencyFile.close();
  return 0;
}