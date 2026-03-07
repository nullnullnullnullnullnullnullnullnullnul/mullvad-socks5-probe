#include "MullvadRelayChecker.h"
#include <chrono>
#include <fmt/base.h>
#include <fmt/format.h>

int main()
{
  MullvadRelayChecker checker;
  if (!checker.isMullvadActive()) {
    fmt::print("[!] ERROR: Mullvad proxies only work while connected to Mullvad VPN.\n");
    fmt::print("[*] Quitting...\n");
    return 1;
  }
  auto startTime = std::chrono::high_resolution_clock::now();
  std::vector<std::string> workingProxies = checker.bulkTestProxies(50);
  if (!workingProxies.empty()) {
    checker.saveWorkingProxies(workingProxies);
  } else {
    fmt::print("[*] No working proxies found to save.\n");
  }
  auto endTime = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed = endTime - startTime;
  fmt::print("[*] Total execution time: {:.2f} seconds.\n", elapsed.count());
  return 0;
}