#include <algorithm>

#include "viz/visualizer.hpp"

namespace audrop::viz {
namespace {

constexpr const char* kFragment = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;

uniform vec2 resolution;
uniform float time;
uniform float bass;
uniform float mid;
uniform float level;
uniform float hit;
uniform float fade;
uniform int scene;
uniform float bands[32];

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float noise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), u.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), u.x), u.y);
}
float fbm(vec2 p) {
    float v = 0.0, a = 0.5;
    for (int i = 0; i < 5; i++) { v += a * noise(p); p = p * 2.03 + vec2(1.7, 9.2); a *= 0.5; }
    return v;
}
vec3 palette(float t) { return 0.5 + 0.5 * cos(6.28318 * (vec3(0.0, 0.33, 0.67) + t)); }

vec3 nebula(vec2 uv) {
    vec2 p = uv * vec2(resolution.x / resolution.y, 1.0) * 2.5;
    float t = time * 0.05;
    vec2 q = vec2(fbm(p + t), fbm(p + vec2(5.2, 1.3) - t));
    vec2 r = vec2(fbm(p + 4.0 * q + vec2(1.7, 9.2) + 0.15 * time), fbm(p + 4.0 * q + vec2(8.3, 2.8) - 0.12 * time));
    float f = fbm(p + 4.0 * r + bass * 0.6);
    vec3 col = palette(f * 0.8 + time * 0.02 + mid * 0.2) * (0.25 + 1.2 * f * f);
    col += vec3(0.6, 0.3, 1.0) * pow(f, 3.0) * (0.5 + 2.0 * hit);
    return col * (0.6 + 0.8 * level);
}

vec3 tunnel(vec2 uv) {
    vec2 p = (uv - 0.5) * vec2(resolution.x / resolution.y, 1.0);
    float r = length(p) + 1e-3;
    float a = atan(p.y, p.x);
    float z = 0.35 / r + time * 0.6 + bass * 0.3;
    float rings = 0.5 + 0.5 * sin(z * 12.0);
    float spokes = 0.5 + 0.5 * sin(a * 6.0 + time * 0.4 + mid * 3.0);
    int bi = int(clamp(abs(a) / 3.14159 * 31.0, 0.0, 31.0));
    vec3 col = palette(z * 0.05 + 0.15 * sin(a * 2.0)) * rings * (0.4 + 0.6 * spokes);
    col *= smoothstep(0.0, 0.6, r) * (0.5 + bands[bi]);
    col += vec3(1.0, 0.5, 0.9) * hit * exp(-r * 6.0);
    return col;
}

vec3 aurora(vec2 uv) {
    vec3 col = vec3(0.02, 0.0, 0.06);
    for (int i = 0; i < 6; i++) {
        float fi = float(i);
        float b = bands[i * 5];
        float y = 0.5 + 0.18 * sin(uv.x * (2.0 + fi * 0.7) + time * (0.3 + fi * 0.1) + fi) + 0.1 * b * sin(uv.x * 9.0 - time);
        float d = abs(uv.y - y);
        col += palette(fi * 0.13 + time * 0.03) * (0.004 + 0.02 * b) / (d + 0.01) * 0.15;
    }
    col += vec3(0.2, 0.1, 0.4) * fbm(uv * 4.0 + time * 0.1) * level;
    return col;
}

void main() {
    vec2 uv = gl_FragCoord.xy / resolution;
    vec3 col = scene == 0 ? nebula(uv) : (scene == 1 ? tunnel(uv) : aurora(uv));
    col = col / (1.0 + col);
    finalColor = vec4(col * fade, 1.0);
}
)";

constexpr int kSceneCount = 3;
constexpr const char* kSceneNames[kSceneCount] = {"Nebula", "Tunnel", "Aurora"};

class ShaderVisualizer final : public Visualizer {
public:
    ShaderVisualizer() {
        shader_ = LoadShaderFromMemory(nullptr, kFragment);
        loc_resolution_ = GetShaderLocation(shader_, "resolution");
        loc_time_ = GetShaderLocation(shader_, "time");
        loc_bass_ = GetShaderLocation(shader_, "bass");
        loc_mid_ = GetShaderLocation(shader_, "mid");
        loc_level_ = GetShaderLocation(shader_, "level");
        loc_hit_ = GetShaderLocation(shader_, "hit");
        loc_fade_ = GetShaderLocation(shader_, "fade");
        loc_scene_ = GetShaderLocation(shader_, "scene");
        loc_bands_ = GetShaderLocation(shader_, "bands");
    }
    ~ShaderVisualizer() override {
        UnloadShader(shader_);
        if (target_.id) UnloadRenderTexture(target_);
    }

    std::string name() const override { return "built-in"; }
    bool supports_presets() const override { return true; }

    void resize(int width, int height) override {
        width = std::max(16, width);
        height = std::max(16, height);
        if (target_.id && target_.texture.width == width && target_.texture.height == height) return;
        if (target_.id) UnloadRenderTexture(target_);
        target_ = LoadRenderTexture(width, height);
        SetTextureFilter(target_.texture, TEXTURE_FILTER_BILINEAR);
    }

    void add_pcm(std::span<const float>) override {}

    void render(const audio::Spectrum& s, double time) override {
        if (!target_.id) return;
        if (scene_start_ < 0) scene_start_ = time;
        if (time - scene_start_ > duration_) next_scene(time);

        const float res[2] = {static_cast<float>(target_.texture.width), static_cast<float>(target_.texture.height)};
        const float t = static_cast<float>(time);
        const float bass = s.bass(), mid = s.mid(), level = s.level(), hit = s.bass_hit();
        const float fade = static_cast<float>(std::clamp((time - scene_start_) / 0.8, 0.0, 1.0));
        SetShaderValue(shader_, loc_resolution_, res, SHADER_UNIFORM_VEC2);
        SetShaderValue(shader_, loc_time_, &t, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader_, loc_bass_, &bass, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader_, loc_mid_, &mid, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader_, loc_level_, &level, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader_, loc_hit_, &hit, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader_, loc_fade_, &fade, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader_, loc_scene_, &scene_, SHADER_UNIFORM_INT);
        SetShaderValueV(shader_, loc_bands_, s.bands().data(), SHADER_UNIFORM_FLOAT, static_cast<int>(s.bands().size()));

        BeginTextureMode(target_);
        BeginShaderMode(shader_);
        DrawRectangle(0, 0, target_.texture.width, target_.texture.height, WHITE);
        EndShaderMode();
        EndTextureMode();
        last_time_ = time;
    }

    const Texture2D& texture() const override { return target_.texture; }
    void next_preset(bool) override { next_scene(last_time_); }
    std::string current_preset() const override { return kSceneNames[scene_]; }
    void set_preset_duration(double seconds) override { duration_ = std::max(5.0, seconds); }

private:
    void next_scene(double time) {
        scene_ = (scene_ + 1) % kSceneCount;
        scene_start_ = time;
    }

    Shader shader_{};
    RenderTexture2D target_{};
    int loc_resolution_ = -1, loc_time_ = -1, loc_bass_ = -1, loc_mid_ = -1, loc_level_ = -1, loc_hit_ = -1,
        loc_fade_ = -1, loc_scene_ = -1, loc_bands_ = -1;
    int scene_ = 0;
    double scene_start_ = -1;
    double last_time_ = 0;
    double duration_ = 30;
};

}  // namespace

std::unique_ptr<Visualizer> make_shader_visualizer(int width, int height) {
    auto v = std::make_unique<ShaderVisualizer>();
    v->resize(width, height);
    return v;
}

}  // namespace audrop::viz
