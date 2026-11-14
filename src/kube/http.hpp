#pragma once

#include <functional>
#include <stop_token>
#include <string>
#include <string_view>

#include "kube/kubeconfig.hpp"

namespace k10s {

struct HttpResult {
    long status{0};    // HTTP status, 0 if the request never got a response
    std::string body;  // full body (get) or any unconsumed tail (stream)
    std::string error; // transport error (DNS, TLS, refused, cancelled, ...)

    bool ok() const { return error.empty() && status >= 200 && status < 300; }
};

// Minimal Kubernetes API client for one context: TLS + auth from the kubeconfig.
// Not thread-safe: use one per thread. Every call aborts within ~1s of `st` being stopped.
class KubeHttp {
  public:
    explicit KubeHttp(KubeContext ctx);

    // GET `path` (e.g. "/api/v1/pods") and return the whole body.
    HttpResult get(const std::string& path, const std::stop_token& st);

    // GET a streaming endpoint (a watch). Calls `onLine` for every complete line as it arrives;
    // returns when the server closes the stream, on error, or when `st` is stopped.
    HttpResult stream(const std::string& path, const std::stop_token& st,
                      const std::function<void(std::string_view line)>& onLine);

  private:
    HttpResult perform(const std::string& path, const std::stop_token& st,
                       const std::function<void(std::string_view)>* onLine);
    std::string bearerToken(); // static token, or fetched from the exec plugin

    KubeContext ctx_;
    std::string cachedExecToken_;
};

} // namespace k10s
