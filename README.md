# mullvad-socks5-probe

[![ci](https://github.com/nullnullnullnullnullnullnullnullnullnul/mullvad-socks5-probe/actions/workflows/ci.yml/badge.svg)](https://github.com/nullnullnullnullnullnullnullnullnullnul/mullvad-socks5-probe/actions/workflows/ci.yml)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

Concurrent SOCKS5 health probe for Mullvad VPN relays.

## Overview

Queries Mullvad's public WireGuard relay API, then verifies end-to-end SOCKS5 connectivity on each relay using a fixed-size worker pool. Intended for Mullvad subscribers to identify which SOCKS5 endpoints from the relay set are currently functional from their location.

The tool operates exclusively against the public endpoints published by Mullvad (`api.mullvad.net/www/relays/wireguard/`, `am.i.mullvad.net/json`) and requires an active Mullvad VPN connection: SOCKS5 relays are only routable through the Mullvad tunnel.

## Build

### Prerequisites

- CMake `>= 3.16`
- A C++17 compiler (GCC 7+, Clang 6+)
- Recommended: system packages
  - `libfmt-dev`
  - `libcurl4-openssl-dev`
  - `nlohmann-json3-dev`

Missing system libraries are pulled in via CMake `FetchContent` as a fallback.

### Build commands

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The resulting binary is `build/mullvad_socks5_probe`.

## Usage

```text
Usage: mullvad_socks5_probe [options]

Probe SOCKS5 connectivity on Mullvad VPN relays.

Options:
  -w, --workers N       number of concurrent probe workers (default: 20)
  -o, --output FILE     output file for working relays (default: relays.txt)
      --no-vpn-check    skip the active-VPN precondition check
  -h, --help            show this message and exit
```

### Example

```bash
./build/mullvad_socks5_probe --workers 30 --output socks5-up.txt
```

Sample output:

```text
[*] Mullvad VPN is active.
[*] Fetching relay list...
[*] Fetched 412 relays. Probing with 30 workers...
[+] se-sto-wg-001.relays.mullvad.net:1080 | IP: 185.213.155.74
[-] de-fra-wg-007.relays.mullvad.net:1080 | Error: Operation timeout
...

[*] Working: 387/412
[*] Saved 387 working relays to socks5-up.txt
[*] Total execution time: 14.32 seconds.
```

The output file contains one `host:port` entry per working relay, one per line.

## Quality

### Formatting

```bash
find src include -type f \( -name '*.cpp' -o -name '*.h' \) -print0 \
  | xargs -0 clang-format -i
```

CI enforces `clang-format` and fails on diffs.

### Static analysis

A `.clang-tidy` configuration is included. Run locally with:

```bash
cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build
find src -name '*.cpp' -print0 | xargs -0 clang-tidy -p build
```

`clang-tidy` is not wired into CI to keep the pipeline fast; contributors are expected to run it locally before opening PRs.

## Conventions

- **Commits**: [Conventional Commits](https://www.conventionalcommits.org/) (`feat`, `fix`, `refactor`, `docs`, `chore`, `ci`, ...).
- **Branching**: `main` is the only long-lived branch. Changes land on topic branches (`feat/<topic>`, `fix/<topic>`, `chore/<topic>`) via PR.
- **PRs**: CI must be green. Squash merge.

## Roadmap

- Unit tests for the HTTP helper and the JSON parsing paths (no test framework wired yet).
- Optional structured output (JSON / NDJSON) alongside the current `host:port` plain-text format.

## License

MIT. See [LICENSE](LICENSE).
