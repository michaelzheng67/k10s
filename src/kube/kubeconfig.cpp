#include "kube/kubeconfig.hpp"

#include <yaml-cpp/yaml.h>

#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>

#include "util/base64.hpp"

namespace k10s {

namespace {

namespace fs = std::filesystem;

struct Located {
    YAML::Node node;
    fs::path baseDir;
};

std::string str(const YAML::Node& n, const char* key) {
    const auto v = n[key];
    return v && v.IsScalar() ? v.as<std::string>() : std::string{};
}

std::optional<std::string> readFile(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in) return std::nullopt;
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// Kubeconfig fields come in pairs: `foo` (a path) or `foo-data` (inline base64).
std::string resolvePem(const YAML::Node& n, const char* pathKey, const char* dataKey,
                       const fs::path& baseDir, const std::string& owner,
                       std::vector<std::string>& warnings) {
    if (const auto data = str(n, dataKey); !data.empty()) {
        if (auto decoded = base64Decode(data)) return *decoded;
        warnings.push_back(owner + ": invalid base64 in " + dataKey);
        return {};
    }
    if (const auto path = str(n, pathKey); !path.empty()) {
        fs::path p{path};
        if (p.is_relative()) p = baseDir / p;
        if (auto contents = readFile(p)) return *contents;
        warnings.push_back(owner + ": cannot read " + p.string());
    }
    return {};
}

// Name-indexed sections across all files; the first file to define a name wins.
void collect(const YAML::Node& doc, const char* section, const char* inner, const fs::path& baseDir,
             std::map<std::string, Located>& out) {
    const auto list = doc[section];
    if (!list || !list.IsSequence()) return;
    for (const auto& item : list) {
        const auto name = str(item, "name");
        if (name.empty() || out.contains(name)) continue;
        out.emplace(name, Located{item[inner], baseDir});
    }
}

} // namespace

const KubeContext* Kubeconfig::find(const std::string& name) const {
    for (const auto& c : contexts)
        if (c.name == name) return &c;
    return nullptr;
}

Kubeconfig parseKubeconfigs(const std::vector<std::string>& docs,
                            const std::vector<fs::path>& baseDirs) {
    Kubeconfig cfg;
    std::map<std::string, Located> clusters, users, contexts;
    std::vector<std::string> contextOrder;

    for (std::size_t i = 0; i < docs.size(); ++i) {
        const fs::path baseDir = i < baseDirs.size() ? baseDirs[i] : fs::path{};
        YAML::Node doc;
        try {
            doc = YAML::Load(docs[i]);
        } catch (const YAML::Exception& e) {
            cfg.warnings.push_back(std::string{"kubeconfig parse error: "} + e.what());
            continue;
        }
        if (!doc.IsMap()) continue;

        if (cfg.currentContext.empty()) cfg.currentContext = str(doc, "current-context");
        collect(doc, "clusters", "cluster", baseDir, clusters);
        collect(doc, "users", "user", baseDir, users);
        if (const auto list = doc["contexts"]; list && list.IsSequence()) {
            for (const auto& item : list) {
                const auto name = str(item, "name");
                if (name.empty() || contexts.contains(name)) continue;
                contexts.emplace(name, Located{item["context"], baseDir});
                contextOrder.push_back(name);
            }
        }
    }

    for (const auto& name : contextOrder) {
        const auto& ctx = contexts.at(name);
        KubeContext kc;
        kc.name = name;
        kc.defaultNamespace = str(ctx.node, "namespace");

        if (auto it = clusters.find(str(ctx.node, "cluster")); it != clusters.end()) {
            const auto& c = it->second;
            kc.server = str(c.node, "server");
            kc.insecureSkipTlsVerify = str(c.node, "insecure-skip-tls-verify") == "true";
            kc.caPem = resolvePem(c.node, "certificate-authority", "certificate-authority-data",
                                  c.baseDir, "cluster " + it->first, cfg.warnings);
        } else {
            cfg.warnings.push_back("context " + name + ": unknown cluster");
        }

        if (auto it = users.find(str(ctx.node, "user")); it != users.end()) {
            const auto& u = it->second;
            const std::string owner = "user " + it->first;
            kc.clientCertPem = resolvePem(u.node, "client-certificate", "client-certificate-data",
                                          u.baseDir, owner, cfg.warnings);
            kc.clientKeyPem = resolvePem(u.node, "client-key", "client-key-data", u.baseDir, owner,
                                         cfg.warnings);
            kc.token = str(u.node, "token");
            if (kc.token.empty()) {
                if (const auto tf = str(u.node, "tokenFile"); !tf.empty()) {
                    fs::path p{tf};
                    if (p.is_relative()) p = u.baseDir / p;
                    if (auto t = readFile(p)) kc.token = *t;
                }
            }
            if (const auto e = u.node["exec"]; e && e.IsMap()) {
                ExecConfig ex;
                ex.command = str(e, "command");
                if (const auto args = e["args"]; args && args.IsSequence())
                    for (const auto& a : args) ex.args.push_back(a.as<std::string>());
                if (const auto env = e["env"]; env && env.IsSequence())
                    for (const auto& kv : env) ex.env.emplace_back(str(kv, "name"), str(kv, "value"));
                kc.exec = std::move(ex);
            }
        }
        cfg.contexts.push_back(std::move(kc));
    }
    return cfg;
}

Kubeconfig loadKubeconfig() {
    std::vector<fs::path> paths;
    if (const char* env = std::getenv("KUBECONFIG"); env && *env) {
        std::stringstream ss{env};
        for (std::string part; std::getline(ss, part, ':');)
            if (!part.empty()) paths.emplace_back(part);
    } else if (const char* home = std::getenv("HOME")) {
        paths.emplace_back(fs::path{home} / ".kube" / "config");
    }

    std::vector<std::string> docs;
    std::vector<fs::path> dirs;
    std::vector<std::string> missing;
    for (const auto& p : paths) {
        if (auto contents = readFile(p)) {
            docs.push_back(std::move(*contents));
            dirs.push_back(p.parent_path());
        } else {
            missing.push_back("kubeconfig not found: " + p.string());
        }
    }
    Kubeconfig cfg = parseKubeconfigs(docs, dirs);
    cfg.warnings.insert(cfg.warnings.end(), missing.begin(), missing.end());
    return cfg;
}

} // namespace k10s
