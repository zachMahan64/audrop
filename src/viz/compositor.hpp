#pragma once

#include <raylib.h>

#include <array>

namespace audrop::viz {

struct Shockwave {
    Vector2 center;   // screen px
    float radius;     // px
    float strength;   // displacement in px at the ring
};

struct PostParams {
    float chroma = 0;     // chromatic aberration in px at the screen edge
    Color flash = BLANK;  // additive full-screen flash (alpha = amount)
    std::array<Shockwave, 4> waves{};
    int wave_count = 0;
    float wave_width = 40;
    float vignette = 0.45f;
};

// Owns the off-screen targets: the whole scene is drawn into one texture so post effects
// (shockwaves, chromatic aberration, flashes) can be applied when presenting.
class Compositor {
public:
    Compositor();
    ~Compositor();
    Compositor(const Compositor&) = delete;
    Compositor& operator=(const Compositor&) = delete;

    void resize(int width, int height);
    void build_blur(const Texture2D& viz);
    void begin_scene();
    void end_scene();
    void draw_viz(const Texture2D& viz, float brightness, float saturation);
    // Blurred visualizer clipped to `r`, darkened by `darkness` (0..1).
    void draw_frosted(Rectangle r, float darkness);
    void present(const PostParams& p);

    int width() const { return width_; }
    int height() const { return height_; }

private:
    int width_ = 0, height_ = 0;
    RenderTexture2D scene_{}, blur_a_{}, blur_b_{};
    Shader blur_{}, post_{}, grade_{};
    int blur_dir_loc_ = -1;
    int post_res_loc_ = -1, post_chroma_loc_ = -1, post_flash_loc_ = -1, post_waves_loc_ = -1, post_wave_count_loc_ = -1,
        post_wave_width_loc_ = -1, post_vignette_loc_ = -1;
    int grade_brightness_loc_ = -1, grade_saturation_loc_ = -1;
};

}  // namespace audrop::viz
