// Audio offset calibration: tap along to a generated click track.
#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <numbers>

#include "app/app.hpp"
#include "sync/judge.hpp"

namespace audrop {

using namespace ui;

namespace {

constexpr int kRate = 44100;
constexpr double kBpm = 120.0;
constexpr double kFirstBeat = 0.5;
constexpr int kBeats = 64;
constexpr std::size_t kMinTaps = 8;
constexpr std::size_t kWindow = 24;  // median over the most recent taps

void put_u32(std::vector<unsigned char>& v, std::uint32_t x) {
    for (int i = 0; i < 4; ++i) v.push_back(static_cast<unsigned char>(x >> (8 * i)));
}
void put_u16(std::vector<unsigned char>& v, std::uint16_t x) {
    v.push_back(static_cast<unsigned char>(x));
    v.push_back(static_cast<unsigned char>(x >> 8));
}

// 16-bit mono WAV with a click on every beat (accented every fourth).
std::vector<unsigned char> click_track_wav() {
    const double period = 60.0 / kBpm;
    const auto frames = static_cast<std::size_t>((kFirstBeat + kBeats * period) * kRate);
    std::vector<std::int16_t> pcm(frames, 0);
    for (int b = 0; b < kBeats; ++b) {
        const auto start = static_cast<std::size_t>((kFirstBeat + b * period) * kRate);
        const double freq = b % 4 == 0 ? 2200.0 : 1500.0;
        for (std::size_t i = 0; i < static_cast<std::size_t>(0.04 * kRate) && start + i < frames; ++i) {
            const double t = static_cast<double>(i) / kRate;
            const double v = std::sin(2 * std::numbers::pi * freq * t) * std::exp(-t / 0.012);
            pcm[start + i] = static_cast<std::int16_t>(v * 26000.0);
        }
    }
    std::vector<unsigned char> wav;
    const auto data_bytes = static_cast<std::uint32_t>(pcm.size() * 2);
    wav.insert(wav.end(), {'R', 'I', 'F', 'F'});
    put_u32(wav, 36 + data_bytes);
    wav.insert(wav.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
    put_u32(wav, 16);
    put_u16(wav, 1);  // PCM
    put_u16(wav, 1);  // mono
    put_u32(wav, kRate);
    put_u32(wav, kRate * 2);
    put_u16(wav, 2);
    put_u16(wav, 16);
    wav.insert(wav.end(), {'d', 'a', 't', 'a'});
    put_u32(wav, data_bytes);
    const auto* bytes = reinterpret_cast<const unsigned char*>(pcm.data());
    wav.insert(wav.end(), bytes, bytes + data_bytes);
    return wav;
}

double median_recent(const std::vector<double>& v) {
    if (v.empty()) return 0;
    std::vector<double> recent(v.end() - static_cast<std::ptrdiff_t>(std::min(v.size(), kWindow)), v.end());
    std::ranges::sort(recent);
    return recent[recent.size() / 2];
}

bool tapped() {
    return IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_X) || IsKeyPressed(KEY_Z) ||
           IsMouseButtonPressed(MOUSE_BUTTON_LEFT) ||
           (IsGamepadAvailable(0) && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN));
}

}  // namespace

void App::start_calibration() {
    cal_offsets_.clear();
    cal_grid_ = {};
    cal_grid_.bpm = kBpm;
    cal_grid_.confidence = 1.0f;
    for (int b = 0; b < kBeats; ++b) cal_grid_.beats.push_back(kFirstBeat + b * 60.0 / kBpm);
    cal_return_ = screen_;
    player_->play_override(click_track_wav(), ".wav", "Calibration click track");
    go(Screen::Calibration);
}

void App::end_calibration(bool save) {
    if (save && cal_offsets_.size() >= kMinTaps) {
        settings_.calibration_ms = std::round(median_recent(cal_offsets_));
        persist::save_settings(settings_);
        char msg[64];
        std::snprintf(msg, sizeof msg, "Audio offset set to %+.0f ms", settings_.calibration_ms);
        toast(msg);
    }
    player_->end_override();
    if (paused_) player_->set_paused(true);
    go(cal_return_);
}

void App::tick_calibration() {
    if (IsKeyPressed(KEY_ESCAPE)) {
        end_calibration(false);
        return;
    }
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        end_calibration(true);
        return;
    }
    if (IsKeyPressed(KEY_R)) cal_offsets_.clear();
    if (tapped()) {
        // Raw (uncalibrated) clock: the measured offset becomes the new calibration.
        const double t = player_->time();
        const auto p = sync::beat_phase(cal_grid_, t);
        if (p.valid) {
            const double frac = p.phase > 0.5 ? p.phase - 1.0 : p.phase;
            const double off_ms = frac * p.beat_length * 1000.0;
            if (std::abs(off_ms) < 200) cal_offsets_.push_back(off_ms);
        }
    }
}

void App::draw_calibration() {
    draw_header("AUDIO CALIBRATION", "TAP SPACE ON EVERY CLICK");
    const float w = static_cast<float>(screen_w_), h = static_cast<float>(screen_h_);
    const double current = median_recent(cal_offsets_);

    // Visual metronome using the offset measured so far, so you can check it lines up.
    const double t = player_->time() - (cal_offsets_.size() >= kMinTaps ? current : settings_.calibration_ms) / 1000.0;
    const auto p = sync::beat_phase(cal_grid_, t);
    const float pulse = p.valid ? static_cast<float>(std::exp(-p.phase * 8.0)) : 0.0f;
    const Vector2 c{w / 2, h * 0.45f};
    const float r = h * 0.09f;
    BeginBlendMode(BLEND_ADDITIVE);
    draw_glow(theme_, c, r * (1.6f + pulse), with_alpha(theme_.accent, 0.25f + 0.6f * pulse));
    EndBlendMode();
    DrawRing(c, r * 0.92f, r, 0, 360, 64, with_alpha(theme_.text, 0.5f + 0.5f * pulse));
    DrawCircleV(c, r * 0.85f * pulse, with_alpha(theme_.accent, 0.8f * pulse));

    // Recent taps as dots on an early/late axis.
    const float axis_w = w * 0.4f, axis_y = h * 0.64f;
    DrawLineEx({c.x - axis_w / 2, axis_y}, {c.x + axis_w / 2, axis_y}, 1, with_alpha(theme_.line, 0.25f));
    DrawLineEx({c.x, axis_y - 12}, {c.x, axis_y + 12}, 2, with_alpha(theme_.text, 0.6f));
    text_centered(theme_, "EARLY", {c.x - axis_w / 2, axis_y + 24}, h * 0.018f, theme_.text_faint);
    text_centered(theme_, "LATE", {c.x + axis_w / 2, axis_y + 24}, h * 0.018f, theme_.text_faint);
    const std::size_t n = std::min(cal_offsets_.size(), kWindow);
    for (std::size_t i = 0; i < n; ++i) {
        const double off = cal_offsets_[cal_offsets_.size() - n + i];
        const float x = c.x + static_cast<float>(std::clamp(off / 150.0, -1.0, 1.0)) * axis_w / 2;
        DrawCircleV({x, axis_y}, 5, with_alpha(theme_.accent2, 0.3f + 0.7f * static_cast<float>(i + 1) / static_cast<float>(n)));
    }

    char line[128];
    if (cal_offsets_.size() >= kMinTaps)
        std::snprintf(line, sizeof line, "Measured offset  %+.0f ms   (%zu taps)", current, cal_offsets_.size());
    else
        std::snprintf(line, sizeof line, "Keep tapping...  %zu / %zu", cal_offsets_.size(), kMinTaps);
    text_centered(theme_, line, {w / 2, h * 0.73f}, h * 0.032f, theme_.text, FontWeight::Bold, 2);
    std::snprintf(line, sizeof line, "Current setting  %+.0f ms", settings_.calibration_ms);
    text_centered(theme_, line, {w / 2, h * 0.78f}, h * 0.022f, theme_.text_dim);
    text_centered(theme_, "ENTER  SAVE        R  RESET        ESC  CANCEL", {w / 2, h * 0.9f}, h * 0.02f,
                  theme_.text_faint, FontWeight::Regular, 2);
}

}  // namespace audrop
