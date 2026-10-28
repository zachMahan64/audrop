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
    float scan_strength = 0;  // darkens the lower third of every `scan_period` px row (0 = off)
    float scan_period = 3;
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
    // Colour grade applied to the visualizer and frosted panels (mirrors ui::VizGrade).
    struct Grade {
        int mode = 0;  // 0 dark: mix towards wash by its alpha; 1 pastel: hue tints over a wash base;
                       // 2 sky: top->bottom gradient with the visualizer screened on, tinted by wash;
                       // 3 nes: pixelated to ~240 rows and dithered to the NES palette
        float saturation = 1.0f;
        Color wash{0, 0, 0, 0};
        Color sky_top{0, 0, 0, 255};
        Color sky_bottom{0, 0, 0, 255};
    };
    void set_grade(const Grade& g) { grade_params_ = g; }
    // Renders the low-resolution pixel-art version of the visualizer (nes grade only). Call
    // before begin_scene().
    void build_pixel(const Texture2D& viz, float brightness);
    void draw_viz(const Texture2D& viz, float brightness);
    // Graded, blurred visualizer clipped to `r`, then tinted with `tint` (alpha = strength).
    // `gloss` (0..1) adds a glassy top sheen and a bright rim.
    void draw_frosted(Rectangle r, Color tint, float gloss = 0.0f);
    void present(const PostParams& p);

    int width() const { return width_; }
    int height() const { return height_; }

private:
    int width_ = 0, height_ = 0;
    RenderTexture2D scene_{}, blur_a_{}, blur_b_{}, pixel_{};
    int pixel_factor_ = 1;  // screen px per pixel-art px
    Shader blur_{}, post_{}, grade_{};
    int blur_dir_loc_ = -1;
    int post_res_loc_ = -1, post_chroma_loc_ = -1, post_flash_loc_ = -1, post_waves_loc_ = -1, post_wave_count_loc_ = -1,
        post_wave_width_loc_ = -1, post_vignette_loc_ = -1;
    int grade_brightness_loc_ = -1, grade_saturation_loc_ = -1, grade_wash_loc_ = -1, grade_mode_loc_ = -1,
        grade_sky_top_loc_ = -1, grade_sky_bottom_loc_ = -1, grade_palette_loc_ = -1;
    int post_scan_strength_loc_ = -1, post_scan_period_loc_ = -1;
    Grade grade_params_;

    void begin_grade(float brightness);
};

}  // namespace audrop::viz
