#include "relay_probe.h"

#include <curl/curl.h>
#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <optional>
#include <thread>

namespace
{

constexpr long kStatusTimeoutSec = 10;
constexpr long kFetchTimeoutSec = 30;
constexpr long kProbeTimeoutSec = 10;
constexpr const char *kUserAgent = "mullvad-socks5-probe/1.0";
constexpr const char *kVpnStatusUrl = "https://am.i.mullvad.net/json";
constexpr const char *kRelayApiUrl = "https://api.mullvad.net/www/relays/wireguard/";
constexpr const char *kProbeTargetUrl = "http://httpbin.org/ip";

using nlohmann::json;

struct HttpResponse
{
  long status = 0;
  std::string body;
  std::string error; // non-empty on transport-level failure
};

size_t writeCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
  static_cast<std::string *>(userp)->append(static_cast<char *>(contents), size * nmemb);
  return size * nmemb;
}

/// Performs a GET against `url` and returns the response.
///
/// `error` is non-empty on transport-level failure (curl returned non-OK),
/// and in that case `status` is unset. On transport success, `status` is
/// the HTTP status code and `body` is the payload. When `proxy` is given,
/// the request is routed through it (e.g. `socks5h://host:port`).
HttpResponse httpGet(const std::string &url, long timeoutSec,
                     const std::optional<std::string> &proxy = std::nullopt)
{
  HttpResponse resp;
  CURL *curl = curl_easy_init();
  if (!curl) {
    resp.error = "curl_easy_init failed";
    return resp;
  }
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp.body);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeoutSec);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, kUserAgent);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  if (proxy) {
    curl_easy_setopt(curl, CURLOPT_PROXY, proxy->c_str());
  }

  CURLcode rc = curl_easy_perform(curl);
  if (rc == CURLE_OK) {
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp.status);
  } else {
    resp.error = curl_easy_strerror(rc);
  }
  curl_easy_cleanup(curl);
  return resp;
}

} // namespace

RelayProbe::RelayProbe()
{
  curl_global_init(CURL_GLOBAL_DEFAULT);
}

RelayProbe::~RelayProbe()
{
  curl_global_cleanup();
}

bool RelayProbe::isMullvadActive()
{
  HttpResponse resp = httpGet(kVpnStatusUrl, kStatusTimeoutSec);
  if (!resp.error.empty()) {
    fmt::print(stderr, "[!] Failed to check Mullvad status: {}\n", resp.error);
    return false;
  }
  if (resp.status != 200) {
    fmt::print(stderr, "[!] Failed to check Mullvad status: HTTP {}\n", resp.status);
    return false;
  }
  try {
    json data = json::parse(resp.body);
    if (data.contains("mullvad_exit_ip") && data["mullvad_exit_ip"].is_boolean()) {
      bool active = data["mullvad_exit_ip"].get<bool>();
      if (active) {
        fmt::print("[*] Mullvad VPN is active.\n");
      }
      return active;
    }
  } catch (const json::parse_error &e) {
    fmt::print(stderr, "[!] JSON parse error while checking Mullvad status: {}\n", e.what());
  }
  return false;
}

std::vector<Relay> RelayProbe::fetchRelays()
{
  HttpResponse resp = httpGet(kRelayApiUrl, kFetchTimeoutSec);
  if (!resp.error.empty()) {
    fmt::print(stderr, "[!] Failed to fetch relay list: {}\n", resp.error);
    return {};
  }
  if (resp.status != 200) {
    fmt::print(stderr, "[!] Failed to fetch relay list: HTTP {}\n", resp.status);
    return {};
  }

  std::vector<Relay> relays;
  try {
    json data = json::parse(resp.body);
    if (!data.is_array()) {
      fmt::print(stderr, "[!] Expected JSON array but got a different structure.\n");
      return {};
    }
    for (const auto &item : data) {
      if (item.contains("socks_name") && item["socks_name"].is_string() &&
          item.contains("socks_port") && item["socks_port"].is_number_integer()) {
        relays.push_back({item["socks_name"].get<std::string>(), item["socks_port"].get<int>()});
      }
    }
  } catch (const json::parse_error &e) {
    fmt::print(stderr, "[!] JSON parse error while fetching relay list: {}\n", e.what());
  }
  return relays;
}

ProbeResult RelayProbe::probeOne(const Relay &relay)
{
  ProbeResult result{relay.host, relay.port, false, {}};
  std::string proxyUrl = fmt::format("socks5h://{}:{}", relay.host, relay.port);
  HttpResponse resp = httpGet(kProbeTargetUrl, kProbeTimeoutSec, proxyUrl);

  if (!resp.error.empty()) {
    result.details = resp.error;
    return result;
  }
  if (resp.status != 200) {
    result.details = fmt::format("HTTP {}", resp.status);
    return result;
  }
  try {
    json data = json::parse(resp.body);
    if (data.contains("origin") && data["origin"].is_string()) {
      result.isWorking = true;
      result.details = data["origin"].get<std::string>();
    } else {
      result.details = "missing 'origin' in response";
    }
  } catch (const json::parse_error &e) {
    result.details = fmt::format("JSON parse error: {}", e.what());
  }
  return result;
}

std::vector<ProbeResult> RelayProbe::probeAll(const std::vector<Relay> &relays,
                                              unsigned int workers)
{
  std::vector<ProbeResult> results;
  if (relays.empty()) {
    return results;
  }
  workers = std::max(1u, std::min(workers, static_cast<unsigned int>(relays.size())));

  std::vector<std::thread> threads;
  threads.reserve(workers);
  const size_t baseChunk = relays.size() / workers;
  const size_t remainder = relays.size() % workers;

  auto it = relays.cbegin();
  for (unsigned int i = 0; i < workers; ++i) {
    size_t chunk = baseChunk + (i < remainder ? 1 : 0);
    if (chunk == 0) {
      continue;
    }
    std::vector<Relay> subset(it, it + chunk);
    it += chunk;
    threads.emplace_back([this, subset = std::move(subset), &results] {
      for (const auto &relay : subset) {
        ProbeResult r = probeOne(relay);
        std::lock_guard<std::mutex> lock(resultsMutex);
        results.push_back(std::move(r));
      }
    });
  }
  for (auto &t : threads) {
    if (t.joinable()) {
      t.join();
    }
  }
  return results;
}

void RelayProbe::reportResults(const std::vector<ProbeResult> &results) const
{
  int working = 0;
  for (const auto &r : results) {
    if (r.isWorking) {
      fmt::print("[+] {}:{} | IP: {}\n", r.host, r.port, r.details);
      ++working;
    } else {
      fmt::print("[-] {}:{} | Error: {}\n", r.host, r.port, r.details);
    }
  }
  fmt::print("\n[*] Working: {}/{}\n", working, results.size());
}

std::vector<std::string> RelayProbe::bulkProbe(unsigned int workers)
{
  fmt::print("[*] Fetching relay list...\n");
  std::vector<Relay> relays = fetchRelays();
  if (relays.empty()) {
    fmt::print(stderr, "[!] No relays fetched, aborting.\n");
    return {};
  }
  fmt::print("[*] Fetched {} relays. Probing with {} workers...\n", relays.size(), workers);

  std::vector<ProbeResult> results = probeAll(relays, workers);
  reportResults(results);

  std::vector<std::string> working;
  working.reserve(results.size());
  for (const auto &r : results) {
    if (r.isWorking) {
      working.push_back(fmt::format("{}:{}", r.host, r.port));
    }
  }
  return working;
}

void RelayProbe::saveWorkingRelays(const std::vector<std::string> &relays,
                                   const std::string &filename)
{
  std::ofstream out(filename);
  if (!out) {
    fmt::print(stderr, "[!] Error opening file {} for writing.\n", filename);
    return;
  }
  for (const auto &r : relays) {
    out << r << '\n';
  }
  fmt::print("[*] Saved {} working relays to {}\n", relays.size(), filename);
}
