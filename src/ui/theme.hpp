#pragma once

#include <raylib.h>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "audio/sfx_synth.hpp"
#include "game/piece.hpp"

namespace audrop::ui {

enum class FontWeight { Regular, Bold };

// Selectable looks. Order is the order the THEME setting cycles through.
enum class ThemeId { Drop, Milky, Frutiger, Retro, Count };

enum class BlockStyle {
    Glass,  // translucent fill, top sheen, bright outline (Drop)
    Soft,   // opaque rounded tile with a soft shadow (Milky)
    Gel,    // glossy jelly tile: specular cap, bright bottom rim (Frutiger)
    Pixel,  // 8x8 fat-pixel tile, hollow or solid per piece (Retro)
};

// How the visualizer is coloured before anything is drawn over it.
enum class VizGrade {
    Dark,    // desaturate, then mix towards `viz_wash` by its alpha
    Pastel,  // pastel hue tints over a `viz_wash` base (light themes)
    Sky,     // `sky_top` -> `sky_bottom` gradient, visualizer screened on as glowing highlights
    Nes,     // pixelated to ~240 rows, dithered down to the NES hardware palette
};

// Everything that differs between themes. Fonts and the glow sprite are loaded by load_theme;
// the rest is plain data from theme_spec().
struct Theme {
    ThemeId id = ThemeId::Drop;
    const char* name = "DROP";

    // Fonts. Sizes and letter spacing passed to the text helpers are scaled by these.
    const char* description = "Neon glass over the full-colour visualizer";
    const char* regular_file = "Rajdhani-SemiBold.ttf";
    const char* bold_file = "Rajdhani-Bold.ttf";
    float font_scale = 1.0f;
    float spacing_scale = 1.0f;
    // Pixel-art theme: the font is an 8px bitmap-style face drawn only at whole multiples of 8px,
    // blocks are drawn in fat pixels (cell / 8), and the board can snap to whole fat pixels.
    bool pixel_art = false;
    float unit = 1.0f;       // one fat pixel on screen (board cell / 8); set from the layout
    int min_font_steps = 1;  // smallest pixel-font size, in multiples of 8px; set from the layout
    Font regular{};
    Font bold{};
    Texture2D glow{};  // soft radial sprite for glows/particles

    // Text and UI.
    Color text{236, 240, 255, 255};
    Color text_dim{170, 176, 205, 255};
    Color text_faint{120, 126, 160, 255};
    Color text_shadow{0, 0, 0, 153};  // alpha is relative to the text's alpha
    Color accent{120, 220, 255, 255};
    Color accent2{255, 110, 220, 255};
    Color panel{8, 6, 20, 255};       // solid overlays (pause dim, toasts)
    Color line{210, 220, 255, 255};   // grid, borders, well edges

    // Background (see VizGrade). For Sky, viz_wash's colour tints the highlights and its alpha
    // sets their strength.
    VizGrade viz_grade = VizGrade::Dark;
    float viz_saturation = 1.0f;
    Color viz_wash{0, 0, 0, 0};
    Color sky_top{0, 0, 0, 255};
    Color sky_bottom{0, 0, 0, 255};
    bool bubbles = false;             // ambient rising bubbles over the background
    Color frost{6, 4, 16, 255};       // tint over frosted panels; alpha comes from the panel setting
    float panel_gloss = 0.0f;         // glassy sheen and rim on frosted panels (0 = none)
    bool boxed_panels = false;        // chunky two-tone border around panels instead of a hairline
    Color box_light{188, 188, 188, 255};
    Color box_dark{80, 80, 80, 255};
    float vignette = 0.45f;
    float scanlines = 0.0f;           // darkening of every scanline's lower third (0 = off)

    // Pieces and effects.
    std::array<Color, 7> pieces{};   // indexed by game::PieceType
    BlockStyle blocks = BlockStyle::Glass;
    bool jelly = false;               // blocks squash and wobble on landing (see fx/jelly.hpp)
    audio::SfxStyle sfx = audio::SfxStyle::Modern;
    float grid_alpha = 0.05f;         // well grid lines
    float ghost_whiten = 0.0f;        // ghost fill: piece colour mixed this far towards white...
    float ghost_alpha = 0.10f;        // ...at this opacity (outline stays the piece colour)
    bool additive_glow = true;        // false = normal alpha blending (needed on light backgrounds)
    float glow_gain = 1.0f;           // scales every glow / particle alpha
    Color flash{255, 255, 255, 255};  // base for lock, row and screen flashes
    std::array<Color, 5> tiers{};     // judgment colours, indexed like tuning::kSyncTiers
    Color off_beat{150, 155, 185, 255};
    Color popup_clear{255, 255, 255, 255};
    Color popup_big{255, 230, 140, 255};  // tetris / t-spin
    Color popup_all_clear{255, 240, 160, 255};
    Color popup_tspin{220, 160, 255, 255};
    Color popup_level{140, 230, 255, 255};
    Color popup_complete{140, 255, 200, 255};
    Color popup_game_over{255, 120, 150, 255};

    // Retro: piece colours come from the current level's two-colour palette; `piece_tone` says
    // which colour each piece uses and whether its block is hollow.
    bool level_palettes = false;
    std::array<std::array<Color, 2>, 10> level_colors{};
    std::array<unsigned char, 7> piece_tone{};  // 0 = hollow in colour 1, 1 = solid colour 1, 2 = solid colour 2

    Color piece(game::PieceType t) const { return pieces[static_cast<std::size_t>(t)]; }
    bool hollow(game::PieceType t) const { return level_palettes && piece_tone[static_cast<std::size_t>(t)] == 0; }
    // Recolours `pieces` for `level` (1-based; palettes repeat). No-op without level palettes.
    void set_level(int level);
};

Theme theme_spec(ThemeId id);
ThemeId theme_from_key(std::string_view key);  // unknown keys give the default (Retro)
std::string_view theme_key(ThemeId id);        // stable id for settings.json

// Fills `theme` from theme_spec(id) and loads its fonts. Unloads anything previously loaded.
void load_theme(Theme& theme, ThemeId id);
void unload_theme(Theme& theme);

// Wrap glow / particle / flash drawing: additive or alpha blending depending on the theme.
void begin_glow(const Theme& th);
void end_glow();

Color rgba(std::uint32_t hex);
Color with_alpha(Color c, float a);
Color lerp(Color a, Color b, float t);

// Text helpers. `spacing` is extra letter spacing in px.
Vector2 measure(const Theme& th, std::string_view s, float size, FontWeight w = FontWeight::Regular, float spacing = 0);
// Cuts `s` short with a trailing ellipsis so it fits in `max_width`; returns it unchanged if it already fits.
std::string ellipsize(const Theme& th, std::string_view s, float max_width, float size, FontWeight w = FontWeight::Regular,
                      float spacing = 0);
// Packs the segments of `s` (separated by runs of three or more spaces) into lines no wider
// than `max_width`; segments keep their original separators within a line.
std::vector<std::string> wrap_segments(const Theme& th, std::string_view s, float max_width, float size,
                                       FontWeight w = FontWeight::Regular, float spacing = 0);
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
