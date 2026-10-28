#include "ui/widgets.hpp"

#include <algorithm>

namespace audrop::ui {
namespace {

bool pad(int button) { return IsGamepadAvailable(0) && IsGamepadButtonPressed(0, button); }
bool key_or_repeat(int key) { return IsKeyPressed(key) || IsKeyPressedRepeat(key); }

}  // namespace

bool ui_up() { return key_or_repeat(KEY_UP) || pad(GAMEPAD_BUTTON_LEFT_FACE_UP); }
bool ui_down() { return key_or_repeat(KEY_DOWN) || pad(GAMEPAD_BUTTON_LEFT_FACE_DOWN); }
bool ui_left() { return key_or_repeat(KEY_LEFT) || pad(GAMEPAD_BUTTON_LEFT_FACE_LEFT); }
bool ui_right() { return key_or_repeat(KEY_RIGHT) || pad(GAMEPAD_BUTTON_LEFT_FACE_RIGHT); }
bool ui_accept() {
    return IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || IsKeyPressed(KEY_SPACE) ||
           pad(GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
}
bool ui_back() { return IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE) || pad(GAMEPAD_BUTTON_RIGHT_FACE_RIGHT); }

int Menu::first_visible(int count) const {
    if (visible_rows_ <= 0 || count <= visible_rows_) return 0;
    return std::clamp(selected_ - visible_rows_ / 2, 0, count - visible_rows_);
}

MenuResult Menu::update(const std::vector<MenuItem>& items, Rectangle area, float row_height) {
    const int n = static_cast<int>(items.size());
    if (n == 0) return {ui_back() ? MenuAction::Back : MenuAction::None, -1};
    selected_ = std::clamp(selected_, 0, n - 1);

    auto step = [&](int dir) {
        for (int i = 0; i < n; ++i) {
            selected_ = (selected_ + dir + n) % n;
            if (items[static_cast<std::size_t>(selected_)].enabled) break;
        }
    };
    if (ui_up()) step(-1);
    if (ui_down()) step(1);

    const auto& cur = items[static_cast<std::size_t>(selected_)];
    if (ui_back()) return {MenuAction::Back, selected_};
    if (cur.enabled && cur.adjustable) {
        if (ui_left()) return {MenuAction::Left, selected_};
        if (ui_right()) return {MenuAction::Right, selected_};
    }
    if (cur.enabled && ui_accept()) return {cur.adjustable ? MenuAction::Right : MenuAction::Activate, selected_};

    // Mouse.
    const Vector2 m = GetMousePosition();
    const int first = first_visible(n);
    const int shown = visible_rows_ > 0 ? std::min(n, visible_rows_) : n;
    for (int i = 0; i < shown; ++i) {
        const int idx = first + i;
        const Rectangle r{area.x, area.y + static_cast<float>(i) * row_height, area.width, row_height};
        if (!CheckCollisionPointRec(m, r) || !items[static_cast<std::size_t>(idx)].enabled) continue;
        const Vector2 d = GetMouseDelta();
        if (d.x != 0 || d.y != 0) selected_ = idx;
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            selected_ = idx;
            if (items[static_cast<std::size_t>(idx)].adjustable)
                return {m.x < r.x + r.width * 0.75f ? MenuAction::Left : MenuAction::Right, idx};
            return {MenuAction::Activate, idx};
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && items[static_cast<std::size_t>(idx)].adjustable)
            return {MenuAction::Left, idx};
    }
    const float wheel = GetMouseWheelMove();
    if (visible_rows_ > 0 && wheel != 0) step(wheel > 0 ? -1 : 1);
    return {};
}

void Menu::draw(const Theme& th, const std::vector<MenuItem>& items, Rectangle area, float row_height) const {
    const int n = static_cast<int>(items.size());
    const int first = first_visible(n);
    const int shown = visible_rows_ > 0 ? std::min(n, visible_rows_) : n;
    const float size = row_height * 0.62f;
    for (int i = 0; i < shown; ++i) {
        const int idx = first + i;
        const auto& it = items[static_cast<std::size_t>(idx)];
        const Rectangle r{area.x, area.y + static_cast<float>(i) * row_height, area.width, row_height};
        const bool sel = idx == selected_;
        if (sel) {
            DrawRectangleGradientH(static_cast<int>(r.x), static_cast<int>(r.y + 2), static_cast<int>(r.width),
                                   static_cast<int>(r.height - 4), with_alpha(th.accent, 0.22f), with_alpha(th.accent, 0.0f));
            DrawRectangle(static_cast<int>(r.x), static_cast<int>(r.y + 4), 3, static_cast<int>(r.height - 8), th.accent);
        }
        const Color c = !it.enabled ? th.text_faint : (sel ? th.text : th.text_dim);
        text(th, it.label, {r.x + row_height * 0.45f, r.y + (row_height - size) / 2}, size, c, FontWeight::Regular, 1.5f);
        if (!it.value.empty()) {
            std::string v = it.adjustable ? "<  " + it.value + "  >" : it.value;
            text_right(th, v, {r.x + r.width - row_height * 0.3f, r.y + (row_height - size) / 2}, size,
                       sel ? th.accent : th.text_dim, FontWeight::Regular, 1.0f);
        }
    }
    if (visible_rows_ > 0 && n > visible_rows_) {
        const float track_h = row_height * static_cast<float>(shown);
        const float thumb_h = track_h * static_cast<float>(shown) / static_cast<float>(n);
        const float thumb_y = area.y + (track_h - thumb_h) * static_cast<float>(first) / static_cast<float>(n - shown);
        DrawRectangleRec({area.x + area.width + 6, area.y, 2, track_h}, with_alpha(th.line, 0.1f));
        DrawRectangleRec({area.x + area.width + 6, thumb_y, 2, thumb_h}, with_alpha(th.accent, 0.8f));
    }
    if (selected_ >= 0 && selected_ < n && !items[static_cast<std::size_t>(selected_)].hint.empty())
        text(th, items[static_cast<std::size_t>(selected_)].hint,
             {area.x + row_height * 0.45f, area.y + row_height * static_cast<float>(shown) + row_height * 0.4f},
             size * 0.75f, th.text_faint);
}

}  // namespace audrop::ui
