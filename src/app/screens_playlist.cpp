// Playlist management screen.
#include <raylib.h>

#include <algorithm>
#include <cmath>

#include "app/app.hpp"
#include "platform/file_dialog.hpp"

namespace audrop {

using namespace ui;

namespace {
constexpr int kActionCount = 6;  // rows before the track list
}

std::vector<MenuItem> App::playlist_items() const {
    std::vector<MenuItem> items;
    const bool dialogs = platform::file_dialogs_available();
    items.push_back({"ADD FILES...", "", false, dialogs, dialogs ? "" : "No file dialog available - drag files onto the window"});
    items.push_back({"ADD FOLDER...", "", false, dialogs, "Adds every supported file inside, recursively"});
    items.push_back({"SHUFFLE", player_->shuffle() ? "ON" : "OFF", true});
    items.push_back({"REPEAT", player_->repeat() ? "ON" : "OFF", true});
    items.push_back({"CLEAR ALL", "", false, player_->size() > 0});
    items.push_back({"BACK"});

    const auto np = player_->now_playing();
    const auto tracks = player_->tracks();
    for (std::size_t i = 0; i < tracks.size(); ++i) {
        MenuItem it;
        const bool current = np.valid && !np.override && np.index == i;
        it.label = (current ? "> " : "   ") + tracks[i].title;
        const auto r = analysis_->get(tracks[i].path);
        switch (r.state) {
            case audio::AnalysisWorker::State::Ready: {
                auto g = sync::apply_adjust(r.raw, r.adjust);
                it.value = g.usable() ? std::to_string(static_cast<int>(std::lround(g.bpm))) + " BPM" : "no beat";
                break;
            }
            case audio::AnalysisWorker::State::Pending: it.value = "analyzing"; break;
            case audio::AnalysisWorker::State::Failed: it.value = "unreadable"; break;
            case audio::AnalysisWorker::State::Missing: break;
        }
        it.hint = "ENTER play    DEL remove    CTRL+UP/DOWN reorder";
        items.push_back(std::move(it));
    }
    return items;
}

void App::tick_playlist() {
    const auto items = playlist_items();
    const float rh = row_height() * 0.85f;
    const int rows = std::max(4, static_cast<int>(static_cast<float>(screen_h_) * 0.58f / rh));
    menu_.set_scroll_rows(rows);
    const Rectangle area = menu_area(0.6f, 0.22f, std::min(rows, static_cast<int>(items.size())), rh);

    // Track-specific keys.
    const int sel = menu_.selected();
    if (sel >= kActionCount) {
        const auto index = static_cast<std::size_t>(sel - kActionCount);
        if (IsKeyPressed(KEY_DELETE)) {
            player_->remove_track(index);
            save_playlist();
            return;
        }
        const bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        if (ctrl && (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_DOWN))) {
            const int d = IsKeyPressed(KEY_UP) ? -1 : 1;
            player_->move_track(index, d);
            menu_.select(std::clamp(sel + d, kActionCount, static_cast<int>(items.size()) - 1));
            save_playlist();
            return;
        }
    }

    const auto r = menu_.update(items, area, rh);
    switch (r.action) {
        case MenuAction::Back: go(Screen::Title); return;
        case MenuAction::Left:
        case MenuAction::Right:
            if (r.index == 2) player_->set_shuffle(!player_->shuffle());
            if (r.index == 3) player_->set_repeat(!player_->repeat());
            save_playlist();
            return;
        case MenuAction::Activate: break;
        case MenuAction::None: return;
    }
    switch (r.index) {
        case 0: add_paths(platform::pick_audio_files()); break;
        case 1: add_paths(platform::pick_folders()); break;
        case 4:
            player_->clear();
            save_playlist();
            toast("Playlist cleared");
            break;
        case 5: go(Screen::Title); break;
        default:
            if (r.index >= kActionCount) player_->play_index(static_cast<std::size_t>(r.index - kActionCount));
            break;
    }
}

void App::draw_playlist() {
    const auto n = player_->size();
    const std::string sub = std::to_string(n) + (n == 1 ? " TRACK" : " TRACKS") + "  -  DRAG FILES OR FOLDERS ONTO THE WINDOW";
    draw_header("PLAYLIST", sub.c_str());
    const auto items = playlist_items();
    const float rh = row_height() * 0.85f;
    const int rows = std::max(4, static_cast<int>(static_cast<float>(screen_h_) * 0.58f / rh));
    const Rectangle area = menu_area(0.6f, 0.22f, std::min(rows, static_cast<int>(items.size())), rh);
    draw_panel({area.x, area.y, area.width, area.height + rh}, rh * 0.4f);
    menu_.draw(theme_, items, area, rh);
    draw_now_playing();
}

}  // namespace audrop
