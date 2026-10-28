// Theme definitions: every per-theme colour, font and style choice lives here.
#include <algorithm>

#include "config/tuning.hpp"
#include "ui/theme.hpp"

namespace audrop::ui {
namespace {

constexpr std::string_view kKeys[] = {"drop", "milky", "frutiger"};
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
    for (std::size_t i = 0; i < t.tiers.size(); ++i) t.tiers[i] = rgba(tuning::kSyncTiers[i].rgba);
    return t;
}

// Light mode: milky glass panels and a washed-out pastel visualizer, dark text.
Theme milky() {
    Theme t;
    t.id = ThemeId::Milky;
    t.name = "MILKY";
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
    // Same-hue ghosts vanish against the blue sky, so they get a mostly white fill.
    t.ghost_whiten = 0.75f;
    t.ghost_alpha = 0.65f;
    t.glow_gain = 0.8f;
    for (std::size_t i = 0; i < t.tiers.size(); ++i) t.tiers[i] = rgba(tuning::kSyncTiers[i].rgba);
    t.popup_level = {170, 240, 90, 255};
    return t;
}

}  // namespace

Theme theme_spec(ThemeId id) {
    switch (id) {
        case ThemeId::Milky: return milky();
        case ThemeId::Frutiger: return frutiger();
        case ThemeId::Drop:
        case ThemeId::Count: break;
    }
    return drop();
}

ThemeId theme_from_key(std::string_view key) {
    const auto it = std::ranges::find(kKeys, key);
    return it == std::end(kKeys) ? ThemeId::Drop : static_cast<ThemeId>(it - std::begin(kKeys));
}

std::string_view theme_key(ThemeId id) { return kKeys[static_cast<std::size_t>(id)]; }

}  // namespace audrop::ui
