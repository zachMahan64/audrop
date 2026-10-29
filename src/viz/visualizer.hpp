#pragma once

#include <raylib.h>

#include <memory>
#include <span>
#include <string>

#include "audio/spectrum.hpp"

namespace audrop::viz {

// A background visualizer that renders into its own texture each frame.
class Visualizer {
public:
    virtual ~Visualizer() = default;

    virtual std::string name() const = 0;
    virtual void resize(int width, int height) = 0;
    virtual void add_pcm(std::span<const float> stereo) = 0;
    // Called before BeginDrawing(); must leave raylib's GL state intact.
    virtual void render(const audio::Spectrum& spectrum, double time) = 0;
    // Render-texture convention (draw with a negative source height).
    virtual const Texture2D& texture() const = 0;

    virtual void next_preset(bool /*hard_cut*/) {}
    virtual void ban_current() {}
    // Restores every banned preset (and clears the saved blocklist); returns how many came back.
    virtual int unban_all() { return 0; }
    virtual int banned_count() const { return 0; }
    virtual std::string current_preset() const { return {}; }
    virtual void set_preset_duration(double /*seconds*/) {}
    // Stops the visualizer's own timed preset changes; next_preset() still switches.
    virtual void set_locked(bool /*locked*/) {}
    virtual bool supports_presets() const { return false; }
};

// projectM when compiled in and it initialises; otherwise the built-in shader visualizer.
std::unique_ptr<Visualizer> make_visualizer(int width, int height, double preset_seconds);
std::unique_ptr<Visualizer> make_shader_visualizer(int width, int height);
std::unique_ptr<Visualizer> make_projectm_visualizer(int width, int height, double preset_seconds);

}  // namespace audrop::viz
