#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace audrop::persist {

// Per-user writable directory (created on first use):
//   Linux:   $XDG_DATA_HOME/audrop or ~/.local/share/audrop
//   macOS:   ~/Library/Application Support/audrop
//   Windows: %APPDATA%\audrop
std::filesystem::path data_dir();

// Read-only bundled assets (fonts).
std::filesystem::path asset_dir();

// projectM presets/textures. User presets in <data_dir>/presets are used if present.
std::optional<std::filesystem::path> preset_dir();
std::optional<std::filesystem::path> preset_texture_dir();

std::string path_to_utf8(const std::filesystem::path& p);
std::filesystem::path utf8_to_path(const std::string& s);

// Atomically replaces `path` with `contents` (write to temp + rename).
bool write_file_atomic(const std::filesystem::path& path, const std::string& contents);
std::optional<std::string> read_file(const std::filesystem::path& path);

}  // namespace audrop::persist
