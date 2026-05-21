#ifndef MULLVAD_SOCKS5_PROBE_RELAY_PROBE_H
#define MULLVAD_SOCKS5_PROBE_RELAY_PROBE_H

#include <mutex>
#include <string>
#include <vector>

/// A Mullvad SOCKS5 relay endpoint, as advertised by the Mullvad relay API.
struct Relay
{
  std::string host;
  int port;
};

/// Outcome of probing a single relay's SOCKS5 connectivity.
///
/// `isWorking` is true only when the probe reached the target service
/// through the relay. On failure, `details` carries the reason (transport
/// error, HTTP status, or parse error). On success, `details` holds the
/// exit IP reported by the target.
struct ProbeResult
{
  std::string host;
  int port;
  bool isWorking;
  std::string details;
};

/// Orchestrates SOCKS5 health probing against the Mullvad relay set.
///
/// The class owns the libcurl global state (initialized in the constructor,
/// torn down in the destructor) and is therefore non-copyable. A single
/// instance per process is sufficient.
class RelayProbe
{
public:
  /// Initializes the libcurl global state.
  RelayProbe();

  /// Tears down the libcurl global state.
  ~RelayProbe();

  RelayProbe(const RelayProbe &) = delete;
  RelayProbe &operator=(const RelayProbe &) = delete;

  /// Returns true iff the current outbound connection is routed through
  /// the Mullvad VPN, as reported by `am.i.mullvad.net/json`. Logs a
  /// confirmation line to stdout on success.
  bool isMullvadActive();

  /// Fetches the relay list from Mullvad and probes SOCKS5 connectivity on
  /// each entry concurrently. Returns the working endpoints as `host:port`
  /// strings. `workers` is clamped to `[1, relay_count]`.
  std::vector<std::string> bulkProbe(unsigned int workers);

  /// Writes one `host:port` entry per line to `filename`. On open failure,
  /// logs an error to stderr and returns without throwing.
  void saveWorkingRelays(const std::vector<std::string> &relays, const std::string &filename);

private:
  std::mutex resultsMutex;

  std::vector<Relay> fetchRelays();
  ProbeResult probeOne(const Relay &relay);
  std::vector<ProbeResult> probeAll(const std::vector<Relay> &relays, unsigned int workers);
  void reportResults(const std::vector<ProbeResult> &results) const;
};

#endif // MULLVAD_SOCKS5_PROBE_RELAY_PROBE_H
