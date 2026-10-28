#include "persist/paths.hpp"

#include <raylib.h>

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <system_error>

namespace audrop::persist {
namespace fs = std::filesystem;

namespace {

fs::path env_path(const char* name) {
    const char* v = std::getenv(name);
    return (v && *v) ? fs::path(v) : fs::path{};
}

fs::path exe_dir() { return fs::path(GetApplicationDirectory()); }

std::optional<fs::path> first_existing(std::initializer_list<fs::path> candidates) {
    std::error_code ec;
    for (const auto& c : candidates)
        if (!c.empty() && fs::is_directory(c, ec)) return c;
    return std::nullopt;
}

}  // namespace

fs::path data_dir() {
    static const fs::path dir = [] {
        fs::path base;
#if defined(_WIN32)
        base = env_path("APPDATA");
        if (base.empty()) base = env_path("USERPROFILE");
#elif defined(__APPLE__)
        base = env_path("HOME") / "Library" / "Application Support";
#else
        base = env_path("XDG_DATA_HOME");
        if (base.empty()) base = env_path("HOME") / ".local" / "share";
#endif
        if (base.empty()) base = fs::current_path();
        fs::path d = base / "audrop";
        std::error_code ec;
        fs::create_directories(d, ec);
        return d;
    }();
    return dir;
}

fs::path asset_dir() {
    static const fs::path dir = [] {
        auto found = first_existing({
            exe_dir() / "assets",
            exe_dir() / ".." / "share" / "audrop" / "assets",
#ifdef AUDROP_SOURCE_ASSET_DIR
            fs::path(AUDROP_SOURCE_ASSET_DIR),
#endif
        });
        return found.value_or(exe_dir() / "assets");
    }();
    return dir;
}

std::optional<fs::path> preset_dir() {
    return first_existing({
        data_dir() / "presets",
        exe_dir() / "presets",
        exe_dir() / ".." / "share" / "audrop" / "presets",
#ifdef AUDROP_PRESET_DIR
        fs::path(AUDROP_PRESET_DIR),
#endif
    });
}

std::optional<fs::path> preset_texture_dir() {
    return first_existing({
        data_dir() / "textures",
        exe_dir() / "textures",
        exe_dir() / ".." / "share" / "audrop" / "textures",
#ifdef AUDROP_PRESET_TEXTURE_DIR
        fs::path(AUDROP_PRESET_TEXTURE_DIR),
#endif
    });
}

std::string path_to_utf8(const fs::path& p) {
    const auto u8 = p.u8string();
    return {reinterpret_cast<const char*>(u8.data()), u8.size()};
}

fs::path utf8_to_path(const std::string& s) {
    return fs::path(std::u8string(reinterpret_cast<const char8_t*>(s.data()), s.size()));
}

bool write_file_atomic(const fs::path& path, const std::string& contents) {
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    fs::path tmp = path;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out << contents;
        if (!out) return false;
    }
    fs::rename(tmp, path, ec);
    return !ec;
}

std::optional<std::string> read_file(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return std::nullopt;
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

}  // namespace audrop::persist
