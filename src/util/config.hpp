#pragma once

#include <filesystem>
#include <string>

namespace k10s {

// Persistent user preferences, stored as YAML (like k9s's config).
struct UserConfig {
    std::string theme;
};

// $XDG_CONFIG_HOME/k10s/config.yaml, else ~/.config/k10s/config.yaml.
std::filesystem::path userConfigPath();

// Missing or unreadable file -> defaults. Never throws.
UserConfig loadUserConfig(const std::filesystem::path& path = userConfigPath());

// Returns false if the file couldn't be written. Never throws.
bool saveUserConfig(const UserConfig& cfg, const std::filesystem::path& path = userConfigPath());

} // namespace k10s
