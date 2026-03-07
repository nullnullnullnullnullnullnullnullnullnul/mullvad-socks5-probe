# Mullvad Relay Checker

## Description
mullvad-relay-checker is a C++17 utility designed to scrape, test, and save working SOCKS5 proxies from the Mullvad WireGuard API. It is specifically built to operate while connected to the Mullvad VPN, ensuring that only active and functional proxies are collected. The tool uses multithreading to rapidly verify proxies against an external IP check service.

## Business Logic
1. VPN Verification: Before fetching proxies, the tool verifies that the user is actively connected to Mullvad VPN by checking the https://am.i.mullvad.net/json endpoint.
2. Proxy Fetching: Scrapes a list of available WireGuard relays from the official Mullvad API (https://api.mullvad.net/www/relays/wireguard/).
3. Multithreaded Testing: Spawns multiple threads (configurable, default 20, or up to 50 as seen in main.cpp) to concurrently test each proxy. It routes an HTTP request to http://httpbin.org/ip through the scraped SOCKS5 proxies.
4. Results Storage: Successfully tested proxies are collected and saved to a specified output file (default: proxies.txt).

## Public API
The MullvadRelayChecker class provides the following public interface:

- `MullvadRelayChecker()`: Constructor that initializes the global libcurl state.
- `~MullvadRelayChecker()`: Destructor that cleans up the global libcurl state.
- `bool isMullvadActive()`: Checks if the current network connection is routed through Mullvad VPN. Returns true if active.
- `std::vector<std::string> bulkTestProxies(unsigned int maxWorkers = 20)`: Fetches proxies from the Mullvad API and tests them using up to maxWorkers threads. Returns a list of working proxy addresses (format: host:port).
- `void saveWorkingProxies(const std::vector<std::string> &proxies, const std::string &filename = "proxies.txt")`: Saves a vector of working proxy strings to the specified text file.

## Build Instructions
To build the project, run the following commands:

```bash
mkdir build
cd build
cmake ..
make
```

## Prerequisites
- A C++ Compiler (GCC or Clang)
- CMake (version 3.10 or higher)
- Make
- fmt library
- libcurl
- nlohmann_json
