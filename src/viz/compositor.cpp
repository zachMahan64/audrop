#include "viz/compositor.hpp"

#include <algorithm>

namespace audrop::viz {
namespace {

constexpr const char* kBlurFs = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;
uniform vec2 direction;
const float w[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
void main() {
    vec3 c = texture(texture0, fragTexCoord).rgb * w[0];
    for (int i = 1; i < 5; i++) {
        vec2 o = direction * float(i) * 1.6;
        c += texture(texture0, fragTexCoord + o).rgb * w[i];
        c += texture(texture0, fragTexCoord - o).rgb * w[i];
    }
    finalColor = vec4(c, 1.0);
}
)";

constexpr const char* kGradeFs = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;
uniform float brightness;
uniform float saturation;
uniform vec4 wash;
uniform int mode;
uniform vec3 sky_top;
uniform vec3 sky_bottom;
void main() {
    vec3 c = texture(texture0, fragTexCoord).rgb;
    float l = dot(c, vec3(0.299, 0.587, 0.114));
    vec3 g = mix(vec3(l), c, saturation) * brightness;
    vec3 hue = g / max(max(g.r, max(g.g, g.b)), 0.001);
    float lg = clamp(dot(g, vec3(0.299, 0.587, 0.114)) * 1.6, 0.0, 1.0);
    vec3 col;
    if (mode == 1) {
        col = wash.rgb * mix(vec3(1.0), hue, wash.a * lg);
    } else if (mode == 2) {
        // Render textures are stored bottom-up, so texture y = 1 is the top of the screen.
        vec3 base = mix(sky_bottom, sky_top, smoothstep(0.0, 1.0, fragTexCoord.y));
        vec3 glow = mix(wash.rgb, hue, 0.35) * lg * wash.a;
        col = 1.0 - (1.0 - base) * (1.0 - glow);
    } else {
        col = mix(g, wash.rgb, wash.a);
    }
    finalColor = vec4(col, 1.0);
}
)";

constexpr const char* kPostFs = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;
uniform vec2 resolution;
uniform float chroma;
uniform vec4 flash;
uniform vec4 waves[4];
uniform int wave_count;
uniform float wave_width;
uniform float vignette;
void main() {
    vec2 px = vec2(fragTexCoord.x, 1.0 - fragTexCoord.y) * resolution;
    vec2 disp = vec2(0.0);
    for (int i = 0; i < 4; i++) {
        if (i >= wave_count) break;
        vec2 d = px - waves[i].xy;
        float dist = length(d) + 1e-4;
        float x = (dist - waves[i].z) / wave_width;
        disp += (d / dist) * exp(-x * x) * waves[i].w;
    }
    px -= disp;
    vec2 uv = vec2(px.x / resolution.x, 1.0 - px.y / resolution.y);
    vec2 dir = uv - 0.5;
    vec2 ca = dir * 2.0 * chroma / resolution;
    vec3 col = vec3(texture(texture0, uv + ca).r, texture(texture0, uv).g, texture(texture0, uv - ca).b);
    float v = smoothstep(1.0, 0.25, length(dir * vec2(1.0, resolution.y / resolution.x)) * 1.35);
    col *= mix(1.0, v, vignette);
    col += flash.rgb * flash.a;
    finalColor = vec4(col, 1.0);
}
)";

// Source rectangle for drawing a render texture (stored bottom-up) right side up.
Rectangle flipped(const Texture2D& t) {
    return {0, 0, static_cast<float>(t.width), -static_cast<float>(t.height)};
}

}  // namespace

Compositor::Compositor() {
    blur_ = LoadShaderFromMemory(nullptr, kBlurFs);
    blur_dir_loc_ = GetShaderLocation(blur_, "direction");
    grade_ = LoadShaderFromMemory(nullptr, kGradeFs);
    grade_brightness_loc_ = GetShaderLocation(grade_, "brightness");
    grade_saturation_loc_ = GetShaderLocation(grade_, "saturation");
    grade_wash_loc_ = GetShaderLocation(grade_, "wash");
    grade_mode_loc_ = GetShaderLocation(grade_, "mode");
    grade_sky_top_loc_ = GetShaderLocation(grade_, "sky_top");
    grade_sky_bottom_loc_ = GetShaderLocation(grade_, "sky_bottom");
    post_ = LoadShaderFromMemory(nullptr, kPostFs);
    post_res_loc_ = GetShaderLocation(post_, "resolution");
    post_chroma_loc_ = GetShaderLocation(post_, "chroma");
    post_flash_loc_ = GetShaderLocation(post_, "flash");
    post_waves_loc_ = GetShaderLocation(post_, "waves");
    post_wave_count_loc_ = GetShaderLocation(post_, "wave_count");
    post_wave_width_loc_ = GetShaderLocation(post_, "wave_width");
    post_vignette_loc_ = GetShaderLocation(post_, "vignette");
}

Compositor::~Compositor() {
    UnloadShader(blur_);
    UnloadShader(grade_);
    UnloadShader(post_);
    if (scene_.id) UnloadRenderTexture(scene_);
    if (blur_a_.id) UnloadRenderTexture(blur_a_);
    if (blur_b_.id) UnloadRenderTexture(blur_b_);
}

void Compositor::resize(int width, int height) {
    width = std::max(16, width);
    height = std::max(16, height);
    if (width == width_ && height == height_) return;
    width_ = width;
    height_ = height;
    if (scene_.id) UnloadRenderTexture(scene_);
    if (blur_a_.id) UnloadRenderTexture(blur_a_);
    if (blur_b_.id) UnloadRenderTexture(blur_b_);
    scene_ = LoadRenderTexture(width, height);
    SetTextureFilter(scene_.texture, TEXTURE_FILTER_BILINEAR);
    blur_a_ = LoadRenderTexture(std::max(4, width / 4), std::max(4, height / 4));
    blur_b_ = LoadRenderTexture(std::max(4, width / 4), std::max(4, height / 4));
    SetTextureFilter(blur_a_.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(blur_b_.texture, TEXTURE_FILTER_BILINEAR);
}

void Compositor::build_blur(const Texture2D& viz) {
    const float bw = static_cast<float>(blur_a_.texture.width), bh = static_cast<float>(blur_a_.texture.height);
    BeginTextureMode(blur_a_);
    DrawTexturePro(viz, flipped(viz), {0, 0, bw, bh}, {0, 0}, 0, WHITE);
    EndTextureMode();
    for (int pass = 0; pass < 2; ++pass) {
        const float h[2] = {1.0f / bw, 0.0f};
        const float v[2] = {0.0f, 1.0f / bh};
        BeginTextureMode(blur_b_);
        BeginShaderMode(blur_);
        SetShaderValue(blur_, blur_dir_loc_, h, SHADER_UNIFORM_VEC2);
        DrawTexturePro(blur_a_.texture, flipped(blur_a_.texture), {0, 0, bw, bh}, {0, 0}, 0, WHITE);
        EndShaderMode();
        EndTextureMode();
        BeginTextureMode(blur_a_);
        BeginShaderMode(blur_);
        SetShaderValue(blur_, blur_dir_loc_, v, SHADER_UNIFORM_VEC2);
        DrawTexturePro(blur_b_.texture, flipped(blur_b_.texture), {0, 0, bw, bh}, {0, 0}, 0, WHITE);
        EndShaderMode();
        EndTextureMode();
    }
}

void Compositor::begin_scene() {
    BeginTextureMode(scene_);
    ClearBackground(BLACK);
}

void Compositor::end_scene() { EndTextureMode(); }

void Compositor::begin_grade(float brightness) {
    const Grade& g = grade_params_;
    const float wash[4] = {g.wash.r / 255.0f, g.wash.g / 255.0f, g.wash.b / 255.0f, g.wash.a / 255.0f};
    const float top[3] = {g.sky_top.r / 255.0f, g.sky_top.g / 255.0f, g.sky_top.b / 255.0f};
    const float bottom[3] = {g.sky_bottom.r / 255.0f, g.sky_bottom.g / 255.0f, g.sky_bottom.b / 255.0f};
    BeginShaderMode(grade_);
    SetShaderValue(grade_, grade_brightness_loc_, &brightness, SHADER_UNIFORM_FLOAT);
    SetShaderValue(grade_, grade_saturation_loc_, &g.saturation, SHADER_UNIFORM_FLOAT);
    SetShaderValue(grade_, grade_wash_loc_, wash, SHADER_UNIFORM_VEC4);
    SetShaderValue(grade_, grade_mode_loc_, &g.mode, SHADER_UNIFORM_INT);
    SetShaderValue(grade_, grade_sky_top_loc_, top, SHADER_UNIFORM_VEC3);
    SetShaderValue(grade_, grade_sky_bottom_loc_, bottom, SHADER_UNIFORM_VEC3);
}

void Compositor::draw_viz(const Texture2D& viz, float brightness) {
    begin_grade(brightness);
    DrawTexturePro(viz, flipped(viz), {0, 0, static_cast<float>(width_), static_cast<float>(height_)}, {0, 0}, 0, WHITE);
    EndShaderMode();
}

void Compositor::draw_frosted(Rectangle r, Color tint, float gloss) {
    const Texture2D& t = blur_a_.texture;
    const float sx = static_cast<float>(t.width) / static_cast<float>(width_);
    const float sy = static_cast<float>(t.height) / static_cast<float>(height_);
    // Render textures are stored bottom-up: screen row y maps to texture row (H - y).
    const Rectangle src{r.x * sx, static_cast<float>(t.height) - (r.y + r.height) * sy, r.width * sx, -r.height * sy};
    begin_grade(1.0f);
    DrawTexturePro(t, src, r, {0, 0}, 0, WHITE);
    EndShaderMode();
    DrawRectangleRec(r, tint);
    if (gloss > 0) {
        const unsigned char a = static_cast<unsigned char>(std::clamp(gloss, 0.0f, 1.0f) * 255.0f);
        auto white = [a](float k) { return Color{255, 255, 255, static_cast<unsigned char>(a * k)}; };
        DrawRectangleGradientV(static_cast<int>(r.x), static_cast<int>(r.y), static_cast<int>(r.width),
                               static_cast<int>(r.height * 0.45f), white(0.22f), white(0.03f));
        DrawRectangleGradientV(static_cast<int>(r.x), static_cast<int>(r.y + r.height * 0.8f), static_cast<int>(r.width),
                               static_cast<int>(r.height * 0.2f), white(0.0f), white(0.08f));
        DrawRectangleLinesEx(r, 1, white(0.55f));
        DrawLineEx({r.x + 1, r.y + 1.5f}, {r.x + r.width - 1, r.y + 1.5f}, 1, white(0.8f));
    }
}

void Compositor::present(const PostParams& p) {
    const float res[2] = {static_cast<float>(width_), static_cast<float>(height_)};
    const float flash[4] = {p.flash.r / 255.0f, p.flash.g / 255.0f, p.flash.b / 255.0f, p.flash.a / 255.0f};
    float waves[16] = {};
    for (int i = 0; i < p.wave_count && i < 4; ++i) {
        const auto& w = p.waves[static_cast<std::size_t>(i)];
        waves[i * 4 + 0] = w.center.x;
        waves[i * 4 + 1] = w.center.y;
        waves[i * 4 + 2] = w.radius;
        waves[i * 4 + 3] = w.strength;
    }
    BeginShaderMode(post_);
    SetShaderValue(post_, post_res_loc_, res, SHADER_UNIFORM_VEC2);
    SetShaderValue(post_, post_chroma_loc_, &p.chroma, SHADER_UNIFORM_FLOAT);
    SetShaderValue(post_, post_flash_loc_, flash, SHADER_UNIFORM_VEC4);
    SetShaderValueV(post_, post_waves_loc_, waves, SHADER_UNIFORM_VEC4, 4);
    SetShaderValue(post_, post_wave_count_loc_, &p.wave_count, SHADER_UNIFORM_INT);
    SetShaderValue(post_, post_wave_width_loc_, &p.wave_width, SHADER_UNIFORM_FLOAT);
    SetShaderValue(post_, post_vignette_loc_, &p.vignette, SHADER_UNIFORM_FLOAT);
    DrawTexturePro(scene_.texture, flipped(scene_.texture), {0, 0, res[0], res[1]}, {0, 0}, 0, WHITE);
    EndShaderMode();
}

}  // namespace audrop::viz
