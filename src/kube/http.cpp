#include "kube/http.hpp"

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <array>
#include <cstdio>
#include <memory>
#include <mutex>

namespace k10s {

namespace {

// curl_global_* must run once, before any other curl call, and pick the TLS backend.
// macOS's libcurl defaults to SecureTransport, which can't load PEM keys from memory;
// its LibreSSL ("openssl") backend can.
void globalInit() {
    static std::once_flag once;
    std::call_once(once, [] {
        curl_global_sslset(CURLSSLBACKEND_OPENSSL, nullptr, nullptr);
        curl_global_init(CURL_GLOBAL_DEFAULT);
    });
}

struct CurlDeleter {
    void operator()(CURL* c) const { curl_easy_cleanup(c); }
};
struct SlistDeleter {
    void operator()(curl_slist* l) const { curl_slist_free_all(l); }
};

struct Transfer {
    const std::stop_token* st;
    const std::function<void(std::string_view)>* onLine; // null: buffer the whole body
    std::string buffer;
};

std::size_t onWrite(char* data, std::size_t size, std::size_t n, void* user) {
    auto* t = static_cast<Transfer*>(user);
    const std::size_t len = size * n;
    t->buffer.append(data, len);
    if (t->onLine) {
        // Hand out complete lines; keep a partial trailing line for the next chunk.
        std::size_t start = 0;
        for (std::size_t nl; (nl = t->buffer.find('\n', start)) != std::string::npos; start = nl + 1) {
            if (nl > start) (*t->onLine)(std::string_view{t->buffer}.substr(start, nl - start));
            if (t->st->stop_requested()) return 0; // abort the transfer
        }
        t->buffer.erase(0, start);
    }
    return len;
}

// Called by curl at least once a second, even when idle: our cancellation hook.
int onProgress(void* user, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
    return static_cast<Transfer*>(user)->st->stop_requested() ? 1 : 0;
}

std::string shellQuote(const std::string& s) {
    std::string out = "'";
    for (char c : s) out += (c == '\'') ? std::string{"'\\''"} : std::string(1, c);
    return out + "'";
}

// Runs a kubeconfig exec plugin and extracts status.token from its ExecCredential JSON.
std::string runExecPlugin(const ExecConfig& ex, std::string& error) {
    std::string cmd =
        "KUBERNETES_EXEC_INFO=" +
        shellQuote(R"({"apiVersion":"client.authentication.k8s.io/v1beta1","kind":"ExecCredential","spec":{"interactive":false}})");
    for (const auto& [k, v] : ex.env) cmd += " " + k + "=" + shellQuote(v);
    cmd += " " + shellQuote(ex.command);
    for (const auto& a : ex.args) cmd += " " + shellQuote(a);
    cmd += " 2>/dev/null";

    std::unique_ptr<FILE, int (*)(FILE*)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        error = "failed to run exec plugin: " + ex.command;
        return {};
    }
    std::string out;
    std::array<char, 4096> buf{};
    while (std::size_t n = std::fread(buf.data(), 1, buf.size(), pipe.get())) out.append(buf.data(), n);

    try {
        const auto j = nlohmann::json::parse(out);
        return j.at("status").at("token").get<std::string>();
    } catch (const std::exception& e) {
        error = "exec plugin " + ex.command + " returned no token";
        return {};
    }
}

} // namespace

KubeHttp::KubeHttp(KubeContext ctx) : ctx_(std::move(ctx)) { globalInit(); }

std::string KubeHttp::bearerToken() {
    if (!ctx_.token.empty()) return ctx_.token;
    if (ctx_.exec && cachedExecToken_.empty()) {
        std::string err;
        cachedExecToken_ = runExecPlugin(*ctx_.exec, err);
    }
    return cachedExecToken_;
}

HttpResult KubeHttp::get(const std::string& path, const std::stop_token& st) {
    return perform(path, st, nullptr);
}

HttpResult KubeHttp::stream(const std::string& path, const std::stop_token& st,
                            const std::function<void(std::string_view)>& onLine) {
    return perform(path, st, &onLine);
}

HttpResult KubeHttp::perform(const std::string& path, const std::stop_token& st,
                             const std::function<void(std::string_view)>* onLine) {
    HttpResult result;
    std::unique_ptr<CURL, CurlDeleter> curl{curl_easy_init()};
    if (!curl) {
        result.error = "curl_easy_init failed";
        return result;
    }
    CURL* c = curl.get();
    Transfer transfer{&st, onLine, {}};
    const std::string url = ctx_.server + path;

    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, onWrite);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &transfer);
    curl_easy_setopt(c, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(c, CURLOPT_XFERINFOFUNCTION, onProgress);
    curl_easy_setopt(c, CURLOPT_XFERINFODATA, &transfer);
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L); // required for multithreaded use
    curl_easy_setopt(c, CURLOPT_ACCEPT_ENCODING, ""); // allow gzip
    if (!onLine) curl_easy_setopt(c, CURLOPT_TIMEOUT, 60L); // watches stay open, lists don't

    // --- TLS: trust the cluster CA, present our client certificate ---
    curl_blob ca{}, cert{}, key{};
    if (ctx_.insecureSkipTlsVerify) {
        curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 0L);
    } else if (!ctx_.caPem.empty()) {
        ca = {ctx_.caPem.data(), ctx_.caPem.size(), CURL_BLOB_NOCOPY};
        curl_easy_setopt(c, CURLOPT_CAINFO_BLOB, &ca);
    }
    if (!ctx_.clientCertPem.empty() && !ctx_.clientKeyPem.empty()) {
        cert = {ctx_.clientCertPem.data(), ctx_.clientCertPem.size(), CURL_BLOB_NOCOPY};
        key = {ctx_.clientKeyPem.data(), ctx_.clientKeyPem.size(), CURL_BLOB_NOCOPY};
        curl_easy_setopt(c, CURLOPT_SSLCERT_BLOB, &cert);
        curl_easy_setopt(c, CURLOPT_SSLCERTTYPE, "PEM");
        curl_easy_setopt(c, CURLOPT_SSLKEY_BLOB, &key);
        curl_easy_setopt(c, CURLOPT_SSLKEYTYPE, "PEM");
    }

    // --- Auth header ---
    std::unique_ptr<curl_slist, SlistDeleter> headers{curl_slist_append(nullptr, "Accept: application/json")};
    if (const std::string token = bearerToken(); !token.empty()) {
        const std::string auth = "Authorization: Bearer " + token;
        headers.reset(curl_slist_append(headers.release(), auth.c_str()));
    }
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, headers.get());

    char errbuf[CURL_ERROR_SIZE] = {};
    curl_easy_setopt(c, CURLOPT_ERRORBUFFER, errbuf);

    const CURLcode rc = curl_easy_perform(c);
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &result.status);
    result.body = std::move(transfer.buffer);

    if (rc != CURLE_OK) {
        if (st.stop_requested()) result.error = "cancelled";
        else result.error = errbuf[0] ? errbuf : curl_easy_strerror(rc);
    }
    if (result.status == 401 && ctx_.exec) cachedExecToken_.clear(); // token expired: refetch next time
    return result;
}

} // namespace k10s
