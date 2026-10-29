// Theme definitions: every per-theme colour, font and style choice lives here.
#include <algorithm>

#include "config/tuning.hpp"
#include "ui/theme.hpp"

namespace audrop::ui {
namespace {

constexpr std::string_view kKeys[] = {"drop", "milky", "frutiger", "retro"};
static_assert(std::size(kKeys) == static_cast<std::size_t>(ThemeId::Count));

// Dark neon glass over the full-colour visualizer.
Theme drop() {
    Theme t;  // the struct defaults are the Drop look
    t.pieces = {{
        {70, 225, 255, 255},   // I
        {255, 222, 70, 255},   // O
        {190, 95, 255, 255},   // T
        {95, 240, 120, 255},   // S
        {255, 80, 110, 255},   // Z
        {75, 120, 255, 255},   // J
        {255, 150, 60, 255},   // L
    }};
    t.block_glow = 1.0f;
    t.shatter_clears = true;
    for (std::size_t i = 0; i < t.tiers.size(); ++i) t.tiers[i] = rgba(tuning::kSyncTiers[i].rgba);
    return t;
}

// Light mode: milky glass panels and a washed-out pastel visualizer, dark text.
Theme milky() {
    Theme t;
    t.id = ThemeId::Milky;
    t.name = "MILKY";
    t.description = "Light mode: milky glass and soft pastels";
    t.regular_file = "Nunito-SemiBold.ttf";
    t.bold_file = "Nunito-ExtraBold.ttf";
    t.font_scale = 0.86f;

    t.text = {38, 42, 60, 255};
    t.text_dim = {92, 98, 122, 255};
    t.text_faint = {118, 124, 150, 255};
    t.text_shadow = {255, 255, 255, 70};
    t.accent = {98, 122, 230, 255};
    t.accent2 = {232, 110, 165, 255};
    t.panel = {246, 244, 241, 255};
    t.line = {60, 68, 100, 255};

    t.viz_saturation = 0.5f;
    t.viz_wash = {247, 244, 240, 150};
    t.viz_grade = VizGrade::Pastel;
    t.frost = {251, 249, 247, 255};
    t.vignette = 0.12f;

    t.pieces = {{
        {92, 196, 228, 255},   // I
        {240, 196, 80, 255},   // O
        {168, 118, 222, 255},  // T
        {102, 198, 136, 255},  // S
        {236, 112, 130, 255},  // Z
        {98, 132, 228, 255},   // J
        {242, 152, 92, 255},   // L
    }};
    t.blocks = BlockStyle::Soft;
    t.jelly = true;
    t.sfx = audio::SfxStyle::Soft;
    t.additive_glow = false;
    t.glow_gain = 0.45f;
    t.flash = {150, 168, 240, 255};
    t.tiers = {{
        {60, 140, 210, 255},   // OK
        {40, 168, 110, 255},   // GOOD
        {214, 150, 20, 255},   // GREAT
        {230, 100, 40, 255},   // AMAZING
        {220, 60, 170, 255},   // PERFECT
    }};
    t.off_beat = {130, 135, 160, 255};
    t.popup_clear = {60, 68, 100, 255};
    t.popup_big = {214, 140, 20, 255};
    t.popup_all_clear = {200, 128, 30, 255};
    t.popup_tspin = {150, 80, 210, 255};
    t.popup_level = {60, 120, 210, 255};
    t.popup_complete = {40, 160, 110, 255};
    t.popup_game_over = {214, 70, 100, 255};
    return t;
}

// Frutiger Aero: glossy gel pieces and tinted glass panels over a bright sky with bubbles.
Theme frutiger() {
    Theme t;
    t.id = ThemeId::Frutiger;
    t.name = "FRUTIGER";
    t.description = "Glossy aero gel, blue skies and bubbles";
    t.regular_file = "Selawik-Semibold.ttf";
    t.bold_file = "Selawik-Bold.ttf";
    t.font_scale = 0.88f;

    t.text = {250, 253, 255, 255};
    t.text_dim = {222, 240, 252, 255};
    t.text_faint = {196, 226, 248, 255};
    t.text_shadow = {0, 36, 88, 215};
    t.accent = {120, 230, 255, 255};
    t.accent2 = {160, 230, 70, 255};
    t.panel = {14, 60, 115, 255};
    t.line = {215, 242, 255, 255};

    t.viz_grade = VizGrade::Sky;
    t.viz_saturation = 0.8f;
    t.viz_wash = {150, 245, 230, 130};  // aqua highlights
    t.sky_top = {22, 96, 186, 255};
    t.sky_bottom = {98, 190, 230, 255};
    t.bubbles = true;
    t.frost = {16, 72, 140, 255};
    t.panel_gloss = 0.9f;
    t.vignette = 0.22f;

    t.pieces = {{
        {40, 205, 245, 255},   // I
        {255, 208, 40, 255},   // O
        {176, 92, 236, 255},   // T
        {96, 214, 54, 255},    // S
        {244, 66, 92, 255},    // Z
        {36, 112, 236, 255},   // J
        {255, 140, 28, 255},   // L
    }};
    t.blocks = BlockStyle::Gel;
    t.jelly = true;
    t.sfx = audio::SfxStyle::Aero;
    // Same-hue ghosts vanish against the blue sky, so they get a mostly white fill.
    t.ghost_whiten = 0.75f;
    t.ghost_alpha = 0.65f;
    t.glow_gain = 0.8f;
    for (std::size_t i = 0; i < t.tiers.size(); ++i) t.tiers[i] = rgba(tuning::kSyncTiers[i].rgba);
    t.popup_level = {170, 240, 90, 255};
    return t;
}

// 8-bit console look: pixel font and blocks, a dithered limited-palette visualizer, boxed
// panels, scanlines and piece colours that change with the level.
Theme retro() {
    Theme t;
    t.id = ThemeId::Retro;
    t.name = "RETRO";
    t.description = "8-bit pixel art and chiptune sounds";
    t.regular_file = "PressStart2P-Regular.ttf";
    t.bold_file = "PressStart2P-Regular.ttf";
    t.font_scale = 0.6f;
    t.spacing_scale = 0.0f;
    t.pixel_art = true;

    t.text = {252, 252, 252, 255};
    t.text_dim = {188, 188, 188, 255};
    t.text_faint = {124, 124, 124, 255};
    t.text_shadow = {0, 0, 0, 255};
    t.accent = {60, 188, 252, 255};
    t.accent2 = {248, 120, 88, 255};
    t.panel = {0, 0, 0, 255};
    t.line = {252, 252, 252, 255};

    t.viz_grade = VizGrade::Nes;
    t.frost = {0, 0, 16, 255};
    t.boxed_panels = true;
    t.box_light = {252, 252, 252, 255};
    t.box_dark = {0, 0, 0, 255};
    t.vignette = 0.0f;
    t.scanlines = 0.2f;

    t.blocks = BlockStyle::Pixel;
    t.sfx = audio::SfxStyle::Chip;
    t.grid_alpha = 0.0f;
    t.ghost_alpha = 0.0f;
    t.glow_gain = 0.0f;
    t.level_palettes = true;
    // Own pairs picked from the NES master palette (not any game's level colours).
    t.level_colors = {{
        {{{248, 56, 0, 255}, {248, 184, 0, 255}}},
        {{{0, 136, 136, 255}, {88, 248, 152, 255}}},
        {{{104, 68, 252, 255}, {60, 188, 252, 255}}},
        {{{168, 16, 0, 255}, {248, 120, 248, 255}}},
        {{{0, 168, 68, 255}, {248, 216, 120, 255}}},
        {{{0, 120, 248, 255}, {252, 160, 68, 255}}},
        {{{216, 0, 204, 255}, {0, 232, 216, 255}}},
        {{{172, 124, 0, 255}, {184, 248, 24, 255}}},
        {{{0, 0, 188, 255}, {248, 120, 88, 255}}},
        {{{152, 120, 248, 255}, {248, 184, 248, 255}}},
    }};
    t.piece_tone = {0, 0, 0, 1, 2, 1, 2};  // I O T hollow; S J colour 1; Z L colour 2
    t.set_level(1);
    t.flash = {252, 252, 252, 255};
    t.tiers = {{
        {60, 188, 252, 255},   // OK
        {88, 216, 84, 255},    // GOOD
        {248, 184, 0, 255},    // GREAT
        {248, 120, 88, 255},   // AMAZING
        {248, 120, 248, 255},  // PERFECT
    }};
    t.off_beat = {124, 124, 124, 255};
    t.popup_big = {248, 184, 0, 255};
    t.popup_all_clear = {248, 216, 120, 255};
    t.popup_tspin = {248, 120, 248, 255};
    t.popup_level = {60, 188, 252, 255};
    t.popup_complete = {88, 216, 84, 255};
    t.popup_game_over = {248, 56, 0, 255};
    return t;
}

}  // namespace

Theme theme_spec(ThemeId id) {
    switch (id) {
        case ThemeId::Milky: return milky();
        case ThemeId::Frutiger: return frutiger();
        case ThemeId::Retro: return retro();
        case ThemeId::Drop:
        case ThemeId::Count: break;
    }
    return drop();
}

ThemeId theme_from_key(std::string_view key) {
    const auto it = std::ranges::find(kKeys, key);
    return it == std::end(kKeys) ? ThemeId::Retro : static_cast<ThemeId>(it - std::begin(kKeys));
}

std::string_view theme_key(ThemeId id) { return kKeys[static_cast<std::size_t>(id)]; }

}  // namespace audrop::ui
