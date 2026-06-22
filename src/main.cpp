#include <fstream>
#include <iostream>
#include <string>

#include "matching_engine.hpp"

// Two modes:
//   ./order_matching                     # file mode (default; existing benchmark)
//   ./order_matching --udp [opts]        # UDP feed-handler mode
//
// UDP options:
//   --bind ADDR        (default "0.0.0.0")
//   --port PORT        (default 30001)
//   --multicast GROUP  (default none; if set, joins the group)
int main(int argc, char** argv) {
  bool udpMode = false;
  std::string bindAddr = "0.0.0.0";
  uint16_t port = 30001;
  std::string multicastGroup;
  int netCore = -1;
  bool useXdp = false;
  std::string iface;

  for (int i = 1; i < argc; ++i) {
    std::string k = argv[i];
    auto next = [&](const char* opt) -> std::string {
      if (i + 1 >= argc) { std::cerr << "missing arg for " << opt << "\n"; std::exit(1); }
      return argv[++i];
    };
    if (k == "--udp") udpMode = true;
    else if (k == "--bind") bindAddr = next("--bind");
    else if (k == "--port") port = static_cast<uint16_t>(std::stoi(next("--port")));
    else if (k == "--multicast") multicastGroup = next("--multicast");
    else if (k == "--net-core") netCore = std::stoi(next("--net-core"));
    else if (k == "--afxdp") useXdp = true;
    else if (k == "--iface") iface = next("--iface");
    else { std::cerr << "unknown arg: " << argv[i] << "\n"; return 1; }
  }

  MatchingEngine engine;

  if (udpMode) {
    std::cout << "Running matching engine in UDP mode...\n";
    if (useXdp && iface.empty()) {
      std::cerr << "--afxdp requires --iface IFNAME (e.g. --iface lo)\n";
      return 1;
    }
    engine.runUdp(bindAddr.c_str(), port,
                  multicastGroup.empty() ? nullptr : multicastGroup.c_str(),
                  netCore, useXdp, iface.empty() ? nullptr : iface.c_str());
  } else {
    std::cout << "Running matching engine (file mode)...\n";
    engine.run();
  }

  std::cout << "Saving latencies...\n";
  {
    std::ofstream f("latencies.txt");
    for (const auto& l : engine.latencies) f << l << "\n";
  }
  if (!engine.wireLatencies.empty()) {
    std::ofstream f("wire_latencies.txt");
    for (const auto& l : engine.wireLatencies) f << l << "\n";
    std::cout << "Saved wire_latencies.txt (" << engine.wireLatencies.size()
              << " samples)\n";
  }
  return 0;
}
