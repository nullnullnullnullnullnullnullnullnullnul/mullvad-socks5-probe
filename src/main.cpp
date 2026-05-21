#include "relay_probe.h"

#include <fmt/format.h>

#include <chrono>
#include <cstdlib>
#include <string>
#include <string_view>

namespace
{

constexpr unsigned int kDefaultWorkers = 20;
constexpr const char *kDefaultOutput = "relays.txt";

struct Options
{
  unsigned int workers = kDefaultWorkers;
  std::string output = kDefaultOutput;
  bool noVpnCheck = false;
};

void printUsage(std::string_view program)
{
  fmt::print("Usage: {} [options]\n"
             "\n"
             "Probe SOCKS5 connectivity on Mullvad VPN relays.\n"
             "\n"
             "Options:\n"
             "  -w, --workers N       number of concurrent probe workers (default: {})\n"
             "  -o, --output FILE     output file for working relays (default: {})\n"
             "      --no-vpn-check    skip the active-VPN precondition check\n"
             "  -h, --help            show this message and exit\n",
             program, kDefaultWorkers, kDefaultOutput);
}

bool parseUint(std::string_view s, unsigned int &out)
{
  if (s.empty()) {
    return false;
  }
  unsigned int value = 0;
  for (char c : s) {
    if (c < '0' || c > '9') {
      return false;
    }
    value = value * 10 + static_cast<unsigned int>(c - '0');
  }
  out = value;
  return true;
}

bool parseArgs(int argc, char **argv, Options &opts)
{
  for (int i = 1; i < argc; ++i) {
    std::string_view arg = argv[i];

    if (arg == "-h" || arg == "--help") {
      printUsage(argv[0]);
      std::exit(0);
    }
    if (arg == "--no-vpn-check") {
      opts.noVpnCheck = true;
      continue;
    }
    if (arg == "-w" || arg == "--workers") {
      if (i + 1 >= argc) {
        fmt::print(stderr, "[!] Missing value for {}\n", arg);
        return false;
      }
      std::string_view value = argv[++i];
      if (!parseUint(value, opts.workers) || opts.workers == 0) {
        fmt::print(stderr, "[!] Invalid value for --workers: {}\n", value);
        return false;
      }
      continue;
    }
    if (arg == "-o" || arg == "--output") {
      if (i + 1 >= argc) {
        fmt::print(stderr, "[!] Missing value for {}\n", arg);
        return false;
      }
      opts.output = argv[++i];
      continue;
    }
    fmt::print(stderr, "[!] Unknown argument: {}\n", arg);
    return false;
  }
  return true;
}

} // namespace

int main(int argc, char **argv)
{
  Options opts;
  if (!parseArgs(argc, argv, opts)) {
    printUsage(argv[0]);
    return 2;
  }

  RelayProbe probe;
  if (!opts.noVpnCheck && !probe.isMullvadActive()) {
    fmt::print(stderr, "[!] Mullvad VPN is not active. SOCKS5 relays are only reachable through "
                       "the Mullvad tunnel.\n"
                       "    Use --no-vpn-check to skip this check.\n");
    return 1;
  }

  auto start = std::chrono::high_resolution_clock::now();
  std::vector<std::string> working = probe.bulkProbe(opts.workers);
  if (!working.empty()) {
    probe.saveWorkingRelays(working, opts.output);
  } else {
    fmt::print("[*] No working relays to save.\n");
  }
  double elapsed =
      std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - start).count();
  fmt::print("[*] Total execution time: {:.2f} seconds.\n", elapsed);
  return 0;
}
