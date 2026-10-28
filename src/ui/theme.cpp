#include "ui/theme.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "persist/paths.hpp"

namespace audrop::ui {
namespace {

Font load_font(const char* file, int size) {
    // ASCII + Latin-1 + common typographic punctuation so most track titles render.
    std::vector<int> codepoints;
    for (int c = 32; c < 127; ++c) codepoints.push_back(c);
    for (int c = 160; c < 256; ++c) codepoints.push_back(c);
    for (int c : {0x2013, 0x2014, 0x2018, 0x2019, 0x201A, 0x201C, 0x201D, 0x201E, 0x2022, 0x2026}) codepoints.push_back(c);
    const std::string path = persist::path_to_utf8(persist::asset_dir() / "fonts" / file);
    Font f = LoadFontEx(path.c_str(), size, codepoints.data(), static_cast<int>(codepoints.size()));
    if (f.texture.id == 0 || f.glyphCount == 0) {
        TraceLog(LOG_WARNING, "audrop: missing font %s, using default", path.c_str());
        return GetFontDefault();
    }
    GenTextureMipmaps(&f.texture);
    SetTextureFilter(f.texture, TEXTURE_FILTER_TRILINEAR);
    return f;
}

const Font& pick(const Theme& th, FontWeight w) { return w == FontWeight::Bold ? th.bold : th.regular; }

}  // namespace

void load_theme(Theme& th) {
    th.regular = load_font("Rajdhani-SemiBold.ttf", 72);
    th.bold = load_font("Rajdhani-Bold.ttf", 128);
    Image img = GenImageGradientRadial(128, 128, 0.0f, WHITE, BLANK);
    th.glow = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(th.glow, TEXTURE_FILTER_BILINEAR);
}

void unload_theme(Theme& th) {
    if (th.regular.texture.id != GetFontDefault().texture.id) UnloadFont(th.regular);
    if (th.bold.texture.id != GetFontDefault().texture.id) UnloadFont(th.bold);
    UnloadTexture(th.glow);
}

Color piece_color(game::PieceType t) {
    switch (t) {
        case game::PieceType::I: return {70, 225, 255, 255};
        case game::PieceType::O: return {255, 222, 70, 255};
        case game::PieceType::T: return {190, 95, 255, 255};
        case game::PieceType::S: return {95, 240, 120, 255};
        case game::PieceType::Z: return {255, 80, 110, 255};
        case game::PieceType::J: return {75, 120, 255, 255};
        case game::PieceType::L: return {255, 150, 60, 255};
    }
    return WHITE;
}

Color rgba(std::uint32_t hex) {
    return {static_cast<unsigned char>(hex >> 24), static_cast<unsigned char>(hex >> 16),
            static_cast<unsigned char>(hex >> 8), static_cast<unsigned char>(hex)};
}

Color with_alpha(Color c, float a) {
    c.a = static_cast<unsigned char>(std::clamp(a, 0.0f, 1.0f) * static_cast<float>(c.a));
    return c;
}

Color lerp(Color a, Color b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    auto mix = [t](unsigned char x, unsigned char y) {
        return static_cast<unsigned char>(static_cast<float>(x) + (static_cast<float>(y) - static_cast<float>(x)) * t);
    };
    return {mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b), mix(a.a, b.a)};
}

Vector2 measure(const Theme& th, std::string_view s, float size, FontWeight w, float spacing) {
    const std::string str(s);
    return MeasureTextEx(pick(th, w), str.c_str(), size, spacing);
}

std::string ellipsize(const Theme& th, std::string_view s, float max_width, float size, FontWeight w, float spacing) {
    if (measure(th, s, size, w, spacing).x <= max_width) return std::string(s);
    // Binary search over UTF-8 codepoint boundaries for the longest prefix that fits.
    std::vector<std::size_t> cuts;
    for (std::size_t i = 1; i < s.size(); ++i)
        if ((static_cast<unsigned char>(s[i]) & 0xC0) != 0x80) cuts.push_back(i);
    auto with_dots = [&](std::size_t n) {
        std::string out(s.substr(0, n));
        while (!out.empty() && out.back() == ' ') out.pop_back();
        return out + "\u2026";
    };
    std::size_t lo = 0, hi = cuts.size();  // answer is cuts[lo - 1], or nothing when lo == 0
    while (lo < hi) {
        const std::size_t mid = (lo + hi + 1) / 2;
        if (measure(th, with_dots(cuts[mid - 1]), size, w, spacing).x <= max_width) lo = mid;
        else hi = mid - 1;
    }
    return lo == 0 ? "\u2026" : with_dots(cuts[lo - 1]);
}

void text(const Theme& th, std::string_view s, Vector2 pos, float size, Color c, FontWeight w, float spacing) {
    const std::string str(s);
    // Soft drop shadow keeps text readable over bright visualizer presets.
    const float off = std::max(1.0f, std::round(size * 0.05f));
    const Color shadow{0, 0, 0, static_cast<unsigned char>(static_cast<float>(c.a) * 0.6f)};
    DrawTextEx(pick(th, w), str.c_str(), {std::round(pos.x) + off, std::round(pos.y) + off}, size, spacing, shadow);
    DrawTextEx(pick(th, w), str.c_str(), {std::round(pos.x), std::round(pos.y)}, size, spacing, c);
}

void text_centered(const Theme& th, std::string_view s, Vector2 center, float size, Color c, FontWeight w,
                   float spacing) {
    const Vector2 m = measure(th, s, size, w, spacing);
    text(th, s, {center.x - m.x / 2, center.y - m.y / 2}, size, c, w, spacing);
}

void text_right(const Theme& th, std::string_view s, Vector2 top_right, float size, Color c, FontWeight w,
                float spacing) {
    const Vector2 m = measure(th, s, size, w, spacing);
    text(th, s, {top_right.x - m.x, top_right.y}, size, c, w, spacing);
}

void label(const Theme& th, std::string_view s, Vector2 pos, float size, Color c) {
    text(th, s, pos, size, c, FontWeight::Regular, size * 0.22f);
}

void draw_glow(const Theme& th, Vector2 center, float radius, Color c) {
    DrawTexturePro(th.glow, {0, 0, static_cast<float>(th.glow.width), static_cast<float>(th.glow.height)},
                   {center.x - radius, center.y - radius, radius * 2, radius * 2}, {0, 0}, 0, c);
}

void glow_text_centered(const Theme& th, std::string_view s, Vector2 center, float size, Color c, float glow,
                        FontWeight w, float spacing) {
    if (glow > 0) {
        const Vector2 m = measure(th, s, size, w, spacing);
        BeginBlendMode(BLEND_ADDITIVE);
        const float r = std::max(m.x, m.y) * 0.6f;
        DrawTexturePro(th.glow, {0, 0, static_cast<float>(th.glow.width), static_cast<float>(th.glow.height)},
                       {center.x - r, center.y - m.y * 0.9f, r * 2, m.y * 1.8f}, {0, 0}, 0, with_alpha(c, 0.35f * glow));
        EndBlendMode();
    }
    text_centered(th, s, center, size, c, w, spacing);
}

std::string format_time(double seconds, bool millis) {
    seconds = std::max(0.0, seconds);
    const int total_ms = static_cast<int>(std::floor(seconds * 1000.0));
    const int m = total_ms / 60000, s = (total_ms / 1000) % 60, ms = total_ms % 1000;
    char buf[32];
    if (millis) std::snprintf(buf, sizeof buf, "%d:%02d.%03d", m, s, ms);
    else std::snprintf(buf, sizeof buf, "%d:%02d", m, s);
    return buf;
}

std::string format_int(std::int64_t v) {
    std::string digits = std::to_string(v < 0 ? -v : v);
    std::string out;
    int n = 0;
    for (auto it = digits.rbegin(); it != digits.rend(); ++it) {
        if (n && n % 3 == 0) out.push_back(',');
        out.push_back(*it);
        ++n;
    }
    if (v < 0) out.push_back('-');
    return {out.rbegin(), out.rend()};
}

}  // namespace audrop::ui
