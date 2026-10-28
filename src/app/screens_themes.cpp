// Theme picker: moving the cursor onto a theme applies it straight away, so the whole screen is
// the preview.
#include <raylib.h>

#include <algorithm>

#include "app/app.hpp"

namespace audrop {

using namespace ui;

namespace {

constexpr int kThemeCount = static_cast<int>(ThemeId::Count);
constexpr int kPixelSnapRow = kThemeCount;
constexpr int kBackRow = kThemeCount + 1;

}  // namespace

void App::open_themes() {
    themes_return_ = game_ && paused_ ? Screen::Playing : Screen::Title;
    go(Screen::Themes);
    menu_.select(static_cast<int>(theme_.id));
}

std::vector<MenuItem> App::theme_items() const {
    std::vector<MenuItem> items;
    for (int i = 0; i < kThemeCount; ++i) {
        const Theme spec = theme_spec(static_cast<ThemeId>(i));
        MenuItem it{spec.name};
        it.detail = spec.description;
        items.push_back(std::move(it));
    }
    items.push_back({"PIXEL SNAP", settings_.pixel_snap ? "ON" : "OFF", true, theme_.pixel_art,
                     "Sizes the board to whole pixels for crisp pixel art"});
    items.push_back({"BACK"});
    return items;
}

void App::tick_themes() {
    const auto items = theme_items();
    const float rh = row_height() * 1.6f;
    const auto r = menu_.update(items, menu_area(0.4f, 0.24f, static_cast<int>(items.size()), rh), rh);
    const int sel = menu_.selected();
    if (sel < kThemeCount && static_cast<ThemeId>(sel) != theme_.id) {
        settings_.theme = std::string(theme_key(static_cast<ThemeId>(sel)));
        apply_theme(static_cast<ThemeId>(sel));
        persist::save_settings(settings_);
    }
    switch (r.action) {
        case MenuAction::Back: go(themes_return_); break;
        case MenuAction::Left:
        case MenuAction::Right:
            if (r.index == kPixelSnapRow) {
                settings_.pixel_snap = !settings_.pixel_snap;
                update_layout();
                persist::save_settings(settings_);
            }
            break;
        case MenuAction::Activate:
            if (r.index < kThemeCount || r.index == kBackRow) go(themes_return_);
            break;
        case MenuAction::None: break;
    }
}

void App::draw_themes() {
    draw_header("THEMES");
    const auto items = theme_items();
    const float rh = row_height() * 1.6f;
    const Rectangle area = menu_area(0.4f, 0.24f, static_cast<int>(items.size()), rh);
    draw_panel({area.x, area.y, area.width, area.height + rh * 0.6f}, rh * 0.3f);
    menu_.draw(theme_, items, area, rh);
    draw_now_playing();
}

}  // namespace audrop
