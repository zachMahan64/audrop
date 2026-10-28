#include "viz/visualizer.hpp"

#include <cstdlib>
#include <string_view>

namespace audrop::viz {

std::unique_ptr<Visualizer> make_visualizer(int width, int height, double preset_seconds) {
    // AUDROP_VISUALIZER=builtin skips projectM (handy on GPUs where it misbehaves).
    const char* choice = std::getenv("AUDROP_VISUALIZER");
    const bool builtin_only = choice && std::string_view(choice) == "builtin";
    if (!builtin_only)
        if (auto pm = make_projectm_visualizer(width, height, preset_seconds)) return pm;
    TraceLog(LOG_INFO, "audrop: using the built-in shader visualizer");
    return make_shader_visualizer(width, height);
}

}  // namespace audrop::viz
