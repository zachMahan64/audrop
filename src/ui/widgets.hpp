#pragma once

#include <raylib.h>

#include <string>
#include <vector>

#include "ui/theme.hpp"

namespace audrop::ui {

struct MenuItem {
    std::string label;
    std::string value;     // shown right-aligned with < > arrows when `adjustable`
    bool adjustable = false;
    bool enabled = true;
    std::string hint;      // shown below the menu when selected
};

enum class MenuAction { None, Activate, Left, Right, Back };

struct MenuResult {
    MenuAction action = MenuAction::None;
    int index = -1;
};

// Keyboard / mouse / gamepad driven vertical menu.
class Menu {
public:
    // Handles input for this tick. `area` is where the rows are drawn (for mouse hit tests).
    MenuResult update(const std::vector<MenuItem>& items, Rectangle area, float row_height);
    void draw(const Theme& th, const std::vector<MenuItem>& items, Rectangle area, float row_height) const;

    int selected() const { return selected_; }
    void select(int i) { selected_ = i; }
    void set_scroll_rows(int visible) { visible_rows_ = visible; }

private:
    int first_visible(int count) const;

    int selected_ = 0;
    int visible_rows_ = 0;  // 0 = unlimited
    float anim_ = 0;
};

// Shared input helpers so menus also respond to gamepads.
bool ui_up();
bool ui_down();
bool ui_left();
bool ui_right();
bool ui_accept();
bool ui_back();

}  // namespace audrop::ui
