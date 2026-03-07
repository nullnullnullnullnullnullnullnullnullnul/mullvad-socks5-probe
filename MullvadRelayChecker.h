#ifndef MULLVAD_RELAY_CHECKER_H
#define MULLVAD_RELAY_CHECKER_H

#include <curl/curl.h>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <vector>

using json = nlohmann::json;

struct ProxyInfo
{
  std::string host;
  int port;
};

struct TestResult
{
  std::string host;
  int port;
  bool isWorking;
  std::string details;
};

class MullvadRelayChecker
{
public:
  MullvadRelayChecker();
  ~MullvadRelayChecker();
  bool isMullvadActive();
  std::vector<std::string> bulkTestProxies(unsigned int maxWorkers = 20);
  void saveWorkingProxies(const std::vector<std::string> &proxies,
                          const std::string &filename = "proxies.txt");

private:
  std::mutex resultsMutex;
  std::vector<ProxyInfo> fetchProxies();
  TestResult testSocks5Proxy(const ProxyInfo &proxy);
  static size_t WriteCallback(void *contents, size_t size, size_t nmemb,
                              void *userp);
};

#endif // PROXY_CHECKER_H
