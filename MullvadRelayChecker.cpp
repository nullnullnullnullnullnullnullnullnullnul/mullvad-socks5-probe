#include "MullvadRelayChecker.h"
#include <fmt/base.h>
#include <fmt/format.h>
#include <fstream>
#include <iostream>

MullvadRelayChecker::MullvadRelayChecker()
{
  curl_global_init(CURL_GLOBAL_DEFAULT);
}

MullvadRelayChecker::~MullvadRelayChecker()
{
  curl_global_cleanup();
}

bool MullvadRelayChecker::isMullvadActive()
{
  CURL *curl;
  CURLcode res;
  std::string readBuffer;
  bool isActive = false;
  std::string checkUrl = "https://am.i.mullvad.net/json";
  curl = curl_easy_init();
  if (curl) {
    curl_easy_setopt(curl, CURLOPT_URL, checkUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    if (res == CURLE_OK && httpCode == 200) {
      try {
        json data = json::parse(readBuffer);
        if (data.contains("mullvad_exit_ip") &&
            data["mullvad_exit_ip"].is_boolean()) {
          isActive = data["mullvad_exit_ip"].get<bool>();
          if (isActive) {
            fmt::print("[*] Mullvad VPN is active!\n");
          }
        }
      } catch (json::parse_error &e) {
        fmt::print(stderr,
                   "[!] JSON parse error while checking Mullvad status: {}.\n",
                   e.what());
      }
    } else {
      fmt::print(stderr, "[!] Failed to check Mullvad status: {}\n",
                 (res != CURLE_OK) ? curl_easy_strerror(res)
                                   : ("HTTP " + std::to_string(httpCode)));
    }
    curl_easy_cleanup(curl);
  } else {
    fmt::print(stderr,
               "[!] Error initializing curl for Mullvad status check.\n");
  }
  return isActive;
}

size_t MullvadRelayChecker::WriteCallback(void *contents, size_t size,
                                         size_t nmemb, void *userp)
{
  ((std::string *)userp)->append((char *)contents, size * nmemb);
  return size * nmemb;
}

std::vector<ProxyInfo> MullvadRelayChecker::fetchProxies()
{
  std::vector<ProxyInfo> proxies;
  CURL *curl;
  CURLcode res;
  std::string readBuffer;
  std::string url = "https://api.mullvad.net/www/relays/wireguard/";
  curl = curl_easy_init();
  if (curl) {
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    if (res != CURLE_OK) {
      fmt::print(
          stderr,
          "[!] curl_easy_perform() failed while fetching proxy list: {}\n",
          curl_easy_strerror(res));
    } else if (httpCode != 200) {
      fmt::print(stderr,
                 "[!] Failed to fetch proxy list, HTTP status code: {}\n",
                 httpCode);
    } else {
      try {
        json data = json::parse(readBuffer);
        if (data.is_array()) {
          for (const auto &item : data) {
            if (item.contains("socks_name") && item["socks_name"].is_string() &&
                item.contains("socks_port") &&
                item["socks_port"].is_number_integer()) {
              proxies.push_back({item["socks_name"].get<std::string>(),
                                 item["socks_port"].get<int>()});
            } else {
              fmt::print(stderr, "[!] Warning: Skipping relay item due to "
                                 "missing or invalid SOCKS info.\n");
            }
          }
        } else {
          fmt::print(stderr,
                     "[!] Expected JSON array but got different structure.\n");
        }
      } catch (json::parse_error &e) {
        fmt::print(stderr,
                   "[!] JSON parse error while fetching proxy list: {}.\n",
                   e.what());
      }
    }
    curl_easy_cleanup(curl);
  } else {
    fmt::print(stderr,
               "[!] Error initializing curl for fetching proxy list.\n");
  }
  return proxies;
}

TestResult MullvadRelayChecker::testSocks5Proxy(const ProxyInfo &proxy)
{
  CURL *curl;
  CURLcode res;
  std::string readBuffer;
  std::string testUrl = "http://httpbin.org/ip";
  std::string proxyUrl =
      "socks5h://" + proxy.host + ":" + std::to_string(proxy.port);
  TestResult result = {proxy.host, proxy.port, false, ""};
  curl = curl_easy_init();
  if (curl) {
    curl_easy_setopt(curl, CURLOPT_URL, testUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_PROXY, proxyUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    if (res != CURLE_OK) {
      result.details = curl_easy_strerror(res);
    } else if (httpCode == 200) {
      try {
        json data = json::parse(readBuffer);
        if (data.contains("origin") && data["origin"].is_string()) {
          result.isWorking = true;
          result.details = data["origin"].get<std::string>();
        } else {
          result.details = "Error: 'origin' key not found in JSON response.";
        }
      } catch (json::parse_error &e) {
        result.details = std::string("JSON parse error: ") + e.what();
      }
    } else {
      result.details = "HTTP " + std::to_string(httpCode);
    }
    curl_easy_cleanup(curl);
  } else {
    result.details = "Error initializing curl for testing.";
  }
  return result;
}

std::vector<std::string>
MullvadRelayChecker::bulkTestProxies(unsigned int maxWorkers)
{
  fmt::print("[*] Fetching proxy list...\n");
  std::vector<ProxyInfo> proxies = fetchProxies();
  if (proxies.empty()) {
    fmt::print(stderr, "[!] No proxies fetched, aborting test.\n");
    return {};
  }
  fmt::print("[*] Fetched {} proxies. Starting tests...\n", proxies.size());
  std::vector<std::string> workingProxies;
  std::vector<TestResult> results;
  std::vector<std::thread> threads;
  unsigned int totalProxies = proxies.size();
  if (maxWorkers > totalProxies) {
    maxWorkers = totalProxies;
  }
  if (maxWorkers == 0 && totalProxies > 0) {
    maxWorkers = 1;
  }
  auto workerTask = [&](const std::vector<ProxyInfo> &proxiesSubset) {
    for (const auto &proxy : proxiesSubset) {
      TestResult singleResult = testSocks5Proxy(proxy);
      std::lock_guard<std::mutex> lock(resultsMutex);
      results.push_back(singleResult);
    }
  };
  size_t proxiesPerWorker = totalProxies / maxWorkers;
  size_t extraProxies = totalProxies % maxWorkers;
  auto proxyIt = proxies.begin();
  for (unsigned int i = 0; i < maxWorkers; ++i) {
    size_t currentBatchSize = proxiesPerWorker + (i < extraProxies ? 1 : 0);
    if (currentBatchSize == 0)
      continue;
    std::vector<ProxyInfo> subset;
    subset.reserve(currentBatchSize);
    for (size_t j = 0; j < currentBatchSize && proxyIt != proxies.end(); ++j) {
      subset.push_back(*proxyIt);
      ++proxyIt;
    }
    if (!subset.empty()) {
      threads.emplace_back(workerTask, std::move(subset));
    }
  }
  for (auto &th : threads) {
    if (th.joinable()) {
      th.join();
    }
  }
  int workingCount = 0;
  for (const auto &result : results) {
    if (result.isWorking) {
      fmt::print("[+] {}:{} | IP: {}\n", result.host, result.port,
                 result.details);
      workingProxies.push_back(result.host + ":" + std::to_string(result.port));
      workingCount++;
    } else {
      fmt::print("[-] {}:{} | Error: {}\n", result.host, result.port,
                 result.details);
    }
  }
  fmt::print("\n[*] Working: {}/{}\n", workingCount, totalProxies);
  return workingProxies;
}

void MullvadRelayChecker::saveWorkingProxies(
    const std::vector<std::string> &proxies, const std::string &filename)
{
  std::ofstream outFile(filename);
  if (!outFile.is_open()) {
    fmt::print(stderr, "\n[!] Error opening file {} for writing.\n", filename);
    return;
  }
  for (const auto &proxy : proxies) {
    outFile << proxy << std::endl;
  }
  outFile.close();
  fmt::print("\n[*] Saved working proxies to {}\n", filename);
}
