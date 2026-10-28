#include "persist/settings.hpp"

#include <nlohmann/json.hpp>

#include "persist/paths.hpp"

namespace audrop::persist {
namespace {

using nlohmann::json;

std::filesystem::path settings_path() { return data_dir() / "settings.json"; }

template <class T>
void get(const json& j, const char* key, T& out) {
    if (auto it = j.find(key); it != j.end()) {
        try {
            out = it->get<T>();
        } catch (const json::exception&) {
        }
    }
}

}  // namespace

Settings load_settings() {
    Settings s;
    auto text = read_file(settings_path());
    if (!text) return s;
    const json j = json::parse(*text, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return s;
    get(j, "das_ms", s.das_ms);
    get(j, "arr_ms", s.arr_ms);
    get(j, "soft_drop_factor", s.soft_drop_factor);
    get(j, "allow_180", s.allow_180);
    get(j, "music_volume", s.music_volume);
    get(j, "sfx_enabled", s.sfx_enabled);
    get(j, "sfx_volume", s.sfx_volume);
    get(j, "calibration_ms", s.calibration_ms);
    get(j, "subdivide_beats", s.subdivide_beats);
    get(j, "theme", s.theme);
    get(j, "viz_brightness", s.viz_brightness);
    get(j, "viz_resolution", s.viz_resolution);
    get(j, "board_opacity", s.board_opacity);
    get(j, "preset_seconds", s.preset_seconds);
    get(j, "reduce_flashing", s.reduce_flashing);
    get(j, "screen_shake", s.screen_shake);
    get(j, "show_beat_lane", s.show_beat_lane);
    get(j, "vsync", s.vsync);
    get(j, "fullscreen", s.fullscreen);
    return s;
}

void save_settings(const Settings& s) {
    const json j{
        {"das_ms", s.das_ms},
        {"arr_ms", s.arr_ms},
        {"soft_drop_factor", s.soft_drop_factor},
        {"allow_180", s.allow_180},
        {"music_volume", s.music_volume},
        {"sfx_enabled", s.sfx_enabled},
        {"sfx_volume", s.sfx_volume},
        {"calibration_ms", s.calibration_ms},
        {"subdivide_beats", s.subdivide_beats},
        {"theme", s.theme},
        {"viz_brightness", s.viz_brightness},
        {"viz_resolution", s.viz_resolution},
        {"board_opacity", s.board_opacity},
        {"preset_seconds", s.preset_seconds},
        {"reduce_flashing", s.reduce_flashing},
        {"screen_shake", s.screen_shake},
        {"show_beat_lane", s.show_beat_lane},
        {"vsync", s.vsync},
        {"fullscreen", s.fullscreen},
    };
    write_file_atomic(settings_path(), j.dump(2));
}

}  // namespace audrop::persist
