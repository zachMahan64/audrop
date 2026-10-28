#pragma once

#include <raylib.h>

#include <cstdint>
#include <string>
#include <string_view>

#include "game/piece.hpp"

namespace audrop::ui {

enum class FontWeight { Regular, Bold };

struct Theme {
    Font regular{};
    Font bold{};
    Texture2D glow{};  // soft radial sprite for additive glows/particles

    Color text{236, 240, 255, 255};
    Color text_dim{170, 176, 205, 255};
    Color text_faint{120, 126, 160, 255};
    Color accent{120, 220, 255, 255};
    Color accent2{255, 110, 220, 255};
    Color panel{8, 6, 20, 255};
    Color line{210, 220, 255, 255};
};

void load_theme(Theme& theme);
void unload_theme(Theme& theme);

Color piece_color(game::PieceType t);
Color rgba(std::uint32_t hex);
Color with_alpha(Color c, float a);
Color lerp(Color a, Color b, float t);

// Text helpers. `spacing` is extra letter spacing in px.
Vector2 measure(const Theme& th, std::string_view s, float size, FontWeight w = FontWeight::Regular, float spacing = 0);
// Cuts `s` short with a trailing ellipsis so it fits in `max_width`; returns it unchanged if it already fits.
std::string ellipsize(const Theme& th, std::string_view s, float max_width, float size, FontWeight w = FontWeight::Regular,
                      float spacing = 0);
void text(const Theme& th, std::string_view s, Vector2 pos, float size, Color c, FontWeight w = FontWeight::Regular,
          float spacing = 0);
void text_centered(const Theme& th, std::string_view s, Vector2 center, float size, Color c,
                   FontWeight w = FontWeight::Regular, float spacing = 0);
void text_right(const Theme& th, std::string_view s, Vector2 top_right, float size, Color c,
                FontWeight w = FontWeight::Regular, float spacing = 0);
// Small uppercase label with wide tracking.
void label(const Theme& th, std::string_view s, Vector2 pos, float size, Color c);
// Text with a soft additive glow behind it.
void glow_text_centered(const Theme& th, std::string_view s, Vector2 center, float size, Color c, float glow,
                        FontWeight w = FontWeight::Bold, float spacing = 0);
void draw_glow(const Theme& th, Vector2 center, float radius, Color c);

std::string format_time(double seconds, bool millis = true);
std::string format_int(std::int64_t v);  // 1,234,567

}  // namespace audrop::ui
