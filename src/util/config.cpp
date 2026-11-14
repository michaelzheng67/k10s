#include "util/config.hpp"

#include <yaml-cpp/yaml.h>

#include <cstdlib>
#include <fstream>

namespace k10s {

std::filesystem::path userConfigPath() {
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg)
        return std::filesystem::path{xdg} / "k10s" / "config.yaml";
    const char* home = std::getenv("HOME");
    return std::filesystem::path{home ? home : "."} / ".config" / "k10s" / "config.yaml";
}

UserConfig loadUserConfig(const std::filesystem::path& path) {
    UserConfig cfg;
    try {
        const YAML::Node root = YAML::LoadFile(path.string());
        if (root["theme"] && root["theme"].IsScalar()) cfg.theme = root["theme"].as<std::string>();
    } catch (const std::exception&) {
        // missing or malformed: keep defaults
    }
    return cfg;
}

bool saveUserConfig(const UserConfig& cfg, const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    YAML::Emitter out;
    out << YAML::BeginMap << YAML::Key << "theme" << YAML::Value << cfg.theme << YAML::EndMap;
    std::ofstream file(path);
    if (!file) return false;
    file << "# k10s settings\n" << out.c_str() << "\n";
    return static_cast<bool>(file);
}

} // namespace k10s
