#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

#include "kube/kubeconfig.hpp"
#include "util/base64.hpp"

using namespace k10s;

TEST_CASE("base64 decode") {
    CHECK(base64Decode("aGVsbG8=") == "hello");
    CHECK(base64Decode("aGVsbG8gd29ybGQ=") == "hello world");
    CHECK(base64Decode("aGVs\nbG8=") == "hello"); // whitespace ignored
    CHECK(base64Decode("") == "");
    CHECK_FALSE(base64Decode("not*base64").has_value());
}

namespace {

// "Q0EtUEVN" = "CA-PEM", "Q0VSVA==" = "CERT", "S0VZ" = "KEY"
constexpr const char* kSample = R"(
apiVersion: v1
kind: Config
current-context: kind-dev
clusters:
- name: kind-dev
  cluster:
    server: https://127.0.0.1:6443
    certificate-authority-data: Q0EtUEVN
- name: eks-prod
  cluster:
    server: https://ABC.eks.amazonaws.com
    insecure-skip-tls-verify: true
contexts:
- name: kind-dev
  context:
    cluster: kind-dev
    user: kind-dev
    namespace: village
- name: prod
  context:
    cluster: eks-prod
    user: eks-user
- name: broken
  context:
    cluster: nope
    user: nope
users:
- name: kind-dev
  user:
    client-certificate-data: Q0VSVA==
    client-key-data: S0VZ
- name: eks-user
  user:
    exec:
      command: aws
      args: [eks, get-token, --cluster-name, prod]
      env:
      - name: AWS_PROFILE
        value: work
)";

} // namespace

TEST_CASE("parses contexts with inline cert data") {
    const auto cfg = parseKubeconfigs({kSample}, {{}});
    CHECK(cfg.currentContext == "kind-dev");
    REQUIRE(cfg.contexts.size() == 3);

    const auto* dev = cfg.find("kind-dev");
    REQUIRE(dev != nullptr);
    CHECK(dev->server == "https://127.0.0.1:6443");
    CHECK(dev->defaultNamespace == "village");
    CHECK(dev->caPem == "CA-PEM");
    CHECK(dev->clientCertPem == "CERT");
    CHECK(dev->clientKeyPem == "KEY");
    CHECK_FALSE(dev->exec.has_value());
}

TEST_CASE("parses exec plugins and insecure flag") {
    const auto cfg = parseKubeconfigs({kSample}, {{}});
    const auto* prod = cfg.find("prod");
    REQUIRE(prod != nullptr);
    CHECK(prod->insecureSkipTlsVerify);
    REQUIRE(prod->exec.has_value());
    CHECK(prod->exec->command == "aws");
    CHECK(prod->exec->args.size() == 4);
    REQUIRE(prod->exec->env.size() == 1);
    CHECK(prod->exec->env[0].first == "AWS_PROFILE");
}

TEST_CASE("unknown cluster produces a warning, not a crash") {
    const auto cfg = parseKubeconfigs({kSample}, {{}});
    CHECK(cfg.find("broken") != nullptr);
    CHECK_FALSE(cfg.warnings.empty());
}

TEST_CASE("cert paths are resolved relative to the kubeconfig's directory") {
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "k10s_kubeconfig_test";
    fs::create_directories(dir);
    std::ofstream(dir / "ca.crt") << "FILE-CA";

    const char* doc = R"(
current-context: c
clusters: [{name: c, cluster: {server: "https://x", certificate-authority: ca.crt}}]
users: [{name: u, user: {token: abc123}}]
contexts: [{name: c, context: {cluster: c, user: u}}]
)";
    const auto cfg = parseKubeconfigs({doc}, {dir});
    REQUIRE(cfg.find("c") != nullptr);
    CHECK(cfg.find("c")->caPem == "FILE-CA");
    CHECK(cfg.find("c")->token == "abc123");
    fs::remove_all(dir);
}

TEST_CASE("multiple files: first definition wins, current-context from first file") {
    const char* a = R"(
current-context: one
clusters: [{name: shared, cluster: {server: "https://from-a"}}]
contexts: [{name: one, context: {cluster: shared, user: x}}]
)";
    const char* b = R"(
current-context: two
clusters: [{name: shared, cluster: {server: "https://from-b"}}]
contexts: [{name: two, context: {cluster: shared, user: x}}]
)";
    const auto cfg = parseKubeconfigs({a, b}, {{}, {}});
    CHECK(cfg.currentContext == "one");
    CHECK(cfg.contexts.size() == 2);
    CHECK(cfg.find("two")->server == "https://from-a");
}

TEST_CASE("garbage input yields a warning") {
    const auto cfg = parseKubeconfigs({"{ this is: [not valid"}, {{}});
    CHECK(cfg.contexts.empty());
    CHECK_FALSE(cfg.warnings.empty());
}
