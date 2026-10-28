#pragma once

#include <array>
#include <climits>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "audio/analysis_worker.hpp"
#include "audio/music_player.hpp"
#include "audio/sfx.hpp"
#include "audio/spectrum.hpp"
#include "fx/bubbles.hpp"
#include "fx/effects.hpp"
#include "game/game.hpp"
#include "persist/scores.hpp"
#include "persist/settings.hpp"
#include "sync/beat_grid.hpp"
#include "ui/board_view.hpp"
#include "ui/theme.hpp"
#include "ui/widgets.hpp"
#include "viz/compositor.hpp"
#include "viz/visualizer.hpp"

namespace audrop {

enum class Screen { Title, ModeSelect, Playlist, Playing, Results, HighScores, Settings, Calibration };

class App {
public:
    App();
    ~App();
    int run();

private:
    // --- lifecycle / loop (app.cpp) ---
    void init();
    void shutdown();
    void tick(double dt);
    void render(double frame_dt);
    void handle_resize();
    void handle_drops();
    void handle_global_keys();
    void update_track_state();
    void refresh_beat_grid();
    void apply_settings();
    void apply_theme(ui::ThemeId id);
    void update_layout();  // depends on the window size, theme and pixel snap setting
    void save_playlist() const;
    void load_playlist();
    void add_paths(const std::vector<std::filesystem::path>& paths);
    void go(Screen s);
    void toast(std::string msg);
    double sync_time() const;  // calibrated track time
    std::string beat_status() const;
    const sync::BeatGrid* usable_grid() const;

    // shared drawing (app.cpp)
    void draw_header(const char* title, const char* subtitle = nullptr);
    // Frosted backing for menu content so it stays readable over bright presets.
    void draw_panel(Rectangle r, float pad);
    void draw_now_playing();
    // Bottom-left title, artist / album / BPM line and progress bar. Returns the title's top y.
    float draw_track_info(float size, float max_width);
    void draw_toast();
    Rectangle menu_area(float width_frac, float top_frac, int rows, float row_h) const;
    float row_height() const;

    // --- screens ---
    void tick_title();
    void draw_title();
    void tick_mode_select();
    void draw_mode_select();
    std::vector<ui::MenuItem> mode_items() const;
    void tick_playlist();
    void draw_playlist();
    std::vector<ui::MenuItem> playlist_items() const;
    void start_game(game::Mode mode);
    void tick_game(double dt);
    void draw_game();
    void play_event_sfx(const std::vector<game::GameEvent>& events);
    std::vector<ui::MenuItem> pause_items() const;
    void handle_pause_menu();
    void set_paused(bool paused);
    void release_all_buttons();
    void finish_game();
    void tick_results();
    void draw_results();
    void tick_high_scores();
    void draw_high_scores();
    void tick_settings();
    void draw_settings();
    std::vector<ui::MenuItem> settings_items() const;
    void adjust_setting(int index, int dir);
    void start_calibration();
    void tick_calibration();
    void draw_calibration();
    void end_calibration(bool save);

    // Developer hooks driven by environment variables (see README).
    void init_debug_hooks();
    void debug_after_present();
    void debug_bot_step();

    // --- state ---
    persist::Settings settings_;
    persist::ScoreBook scores_;
    std::unique_ptr<audio::MusicPlayer> player_;
    std::unique_ptr<audio::AnalysisWorker> analysis_;
    std::unique_ptr<audio::Sfx> sfx_;
    audio::Spectrum spectrum_;
    std::unique_ptr<viz::Visualizer> viz_;
    std::unique_ptr<viz::Compositor> comp_;
    ui::Theme theme_;
    fx::Effects effects_;
    fx::Bubbles bubbles_;
    ui::BoardLayout layout_;
    std::vector<float> pcm_scratch_;

    Screen screen_ = Screen::Title;
    ui::Menu menu_;
    int screen_w_ = 0, screen_h_ = 0;
    float viz_scale_applied_ = 0;
    bool quit_ = false;
    double wall_ = 0;
    double sync_now_ = 0;

    // Current track + beat grid.
    std::uint64_t track_generation_ = ~0ull;
    bool track_override_ = false;
    std::filesystem::path track_path_;
    std::string track_title_;
    std::string track_artist_;
    std::string track_album_;
    double track_banner_at_ = -100;
    sync::BeatGrid raw_grid_;
    sync::BeatAdjust adjust_;
    std::optional<sync::BeatGrid> grid_;
    audio::AnalysisWorker::State beat_state_ = audio::AnalysisWorker::State::Missing;
    std::uint64_t analysis_revision_ = ~0ull;
    long last_beat_index_ = LONG_MIN;

    // Game.
    std::unique_ptr<game::Game> game_;
    game::Mode mode_ = game::Mode::Marathon;
    bool paused_ = false;
    ui::Menu pause_menu_;
    std::array<bool, static_cast<std::size_t>(game::Button::Count)> held_{};
    std::vector<std::string> run_tracks_;
    double countdown_ = 0;
    double end_timer_ = 0;
    bool ending_ = false;

    // Results / high scores.
    persist::ScoreEntry last_entry_;
    int last_rank_ = -1;
    int hs_mode_ = 0;

    // Calibration.
    std::vector<double> cal_offsets_;
    sync::BeatGrid cal_grid_;
    Screen cal_return_ = Screen::Settings;

    std::string toast_;
    double toast_at_ = -100;

    std::string debug_shot_;
    double debug_shot_at_ = 0;
    std::optional<game::Mode> debug_mode_;
    bool debug_bot_ = false;
    bool debug_bot_drop_only_ = false;
    int debug_shot_clear_ = 0;  // screenshot shortly after this many line clears
    double debug_pause_at_ = 0;
    int debug_clears_ = 0;
    void debug_on_clear();
    int debug_bot_piece_ = -1;
    int debug_bot_rot_ = 0;
    int debug_bot_x_ = 0;
    double debug_bot_next_ = 0;
};

}  // namespace audrop
