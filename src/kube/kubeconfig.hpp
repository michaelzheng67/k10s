#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace k10s {

// An exec credential plugin (e.g. `aws eks get-token`, `gke-gcloud-auth-plugin`).
struct ExecConfig {
    std::string command;
    std::vector<std::string> args;
    std::vector<std::pair<std::string, std::string>> env;
};

// Everything needed to talk to one cluster, fully resolved: file paths are read
// and base64 `*-data` fields are decoded, so PEM blobs are ready for TLS.
struct KubeContext {
    std::string name;
    std::string server;
    std::string defaultNamespace;

    std::string caPem;
    bool insecureSkipTlsVerify{false};

    std::string clientCertPem;
    std::string clientKeyPem;
    std::string token;
    std::optional<ExecConfig> exec;
};

struct Kubeconfig {
    std::vector<KubeContext> contexts;
    std::string currentContext;
    std::vector<std::string> warnings; // non-fatal problems (missing cert file, bad base64, ...)

    const KubeContext* find(const std::string& name) const;
};

// Loads $KUBECONFIG (colon-separated, first file wins on name clashes) or ~/.kube/config.
// Never throws: a missing or broken config yields no contexts plus a warning.
Kubeconfig loadKubeconfig();

// Parses one or more kubeconfig documents. `baseDirs[i]` resolves relative paths in `docs[i]`.
Kubeconfig parseKubeconfigs(const std::vector<std::string>& docs,
                            const std::vector<std::filesystem::path>& baseDirs);

} // namespace k10s
