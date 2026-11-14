#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

#include "util/config.hpp"

using namespace k10s;
namespace fs = std::filesystem;

TEST_CASE("user config round-trips the theme") {
    const fs::path dir = fs::temp_directory_path() / "k10s_config_test";
    fs::remove_all(dir);
    const fs::path file = dir / "nested" / "config.yaml"; // parent dirs get created

    CHECK(loadUserConfig(file).theme.empty()); // missing file -> defaults
    REQUIRE(saveUserConfig(UserConfig{"beach"}, file));
    CHECK(loadUserConfig(file).theme == "beach");
    fs::remove_all(dir);
}

TEST_CASE("malformed config falls back to defaults") {
    const fs::path file = fs::temp_directory_path() / "k10s_bad_config.yaml";
    std::ofstream(file) << "theme: [unclosed";
    CHECK(loadUserConfig(file).theme.empty());
    fs::remove(file);
}
