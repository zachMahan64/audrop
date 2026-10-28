#include "app/app.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <nlohmann/json.hpp>

#include "app/loop_clock.hpp"
#include "persist/paths.hpp"
#include "platform/file_dialog.hpp"
#include "platform/gl_state.hpp"
#include "sync/judge.hpp"

namespace audrop {

using namespace ui;

App::App() = default;
App::~App() = default;

void App::init() {
    settings_ = persist::load_settings();
    scores_.load();

    SetTraceLogLevel(LOG_INFO);
    unsigned flags = FLAG_WINDOW_RESIZABLE;
    if (settings_.vsync) flags |= FLAG_VSYNC_HINT;
    SetConfigFlags(flags);
    InitWindow(1600, 900, "audrop");
    SetWindowMinSize(960, 600);
    SetExitKey(KEY_NULL);
    if (settings_.fullscreen) ToggleBorderlessWindowed();
    gl::load();

    InitAudioDevice();
    platform::init_file_dialogs();

    load_theme(theme_);
    comp_ = std::make_unique<viz::Compositor>();
    screen_w_ = GetScreenWidth();
    screen_h_ = GetScreenHeight();
    comp_->resize(screen_w_, screen_h_);
    viz_scale_applied_ = settings_.viz_resolution;
    viz_ = viz::make_visualizer(static_cast<int>(static_cast<float>(screen_w_) * settings_.viz_resolution),
                                static_cast<int>(static_cast<float>(screen_h_) * settings_.viz_resolution),
                                settings_.preset_seconds);
    layout_ = compute_layout(static_cast<float>(screen_w_), static_cast<float>(screen_h_));

    analysis_ = std::make_unique<audio::AnalysisWorker>();
    player_ = std::make_unique<audio::MusicPlayer>();
    apply_settings();
    load_playlist();
    init_debug_hooks();
}

void App::shutdown() {
    save_playlist();
    persist::save_settings(settings_);
    game_.reset();
    player_.reset();
    analysis_.reset();
    viz_.reset();
    comp_.reset();
    unload_theme(theme_);
    platform::shutdown_file_dialogs();
    CloseAudioDevice();
    CloseWindow();
}

int App::run() {
    init();
    const double logic_step = 1.0 / tuning::kLogicHz;
    double last = now_seconds();
    double next_frame = last;
    double last_frame = last;

    while (!quit_) {
        PollInputEvents();
        if (WindowShouldClose()) break;

        const double now = now_seconds();
        const double dt = std::min(0.1, now - last);
        if (debug_mode_ && wall_ > 1.5) {
            start_game(*debug_mode_);
            debug_mode_.reset();
        }
        last = now;
        wall_ = now;
        player_->update_clock(now);
        sync_now_ = sync_time();
        tick(dt);

        int hz = GetMonitorRefreshRate(GetCurrentMonitor());
        if (hz <= 0) hz = 60;
        const double period = 1.0 / std::clamp(hz, 30, 360);
        if (settings_.vsync || now >= next_frame) {
            render(std::min(0.1, now - last_frame));
            last_frame = now;
            SwapScreenBuffer();
            next_frame += period;
            if (next_frame < now) next_frame = now + period;
        } else {
            sleep_seconds(std::min(logic_step, next_frame - now));
        }
    }
    shutdown();
    return 0;
}

double App::sync_time() const { return player_->time() - settings_.calibration_ms / 1000.0; }

const sync::BeatGrid* App::usable_grid() const { return grid_ && grid_->usable() ? &*grid_ : nullptr; }

std::string App::beat_status() const {
    if (!player_->now_playing().valid) return "NO MUSIC  -  ADD TRACKS IN THE PLAYLIST";
    switch (beat_state_) {
        case audio::AnalysisWorker::State::Missing:
        case audio::AnalysisWorker::State::Pending: return "ANALYZING BEAT...";
        case audio::AnalysisWorker::State::Failed: return "BEAT ANALYSIS FAILED";
        case audio::AnalysisWorker::State::Ready: return "NO STEADY BEAT DETECTED";
    }
    return {};
}

void App::apply_settings() {
    player_->set_volume(settings_.music_volume);
    viz_->set_preset_duration(settings_.preset_seconds);
    effects_.set_options({settings_.reduce_flashing, settings_.screen_shake});
    if (settings_.vsync) SetWindowState(FLAG_VSYNC_HINT);
    else ClearWindowState(FLAG_VSYNC_HINT);
}

void App::go(Screen s) {
    screen_ = s;
    menu_.select(0);
    menu_.set_scroll_rows(0);
}

void App::toast(std::string msg) {
    toast_ = std::move(msg);
    toast_at_ = wall_;
}

// --- input / logic ----------------------------------------------------------

void App::tick(double dt) {
    handle_resize();
    handle_drops();
    update_track_state();
    refresh_beat_grid();

    // Beat pulses for the HUD, driven by the calibrated clock.
    if (const auto* g = usable_grid()) {
        const auto p = sync::beat_phase(*g, sync_now_);
        if (p.valid && p.index != last_beat_index_) {
            if (last_beat_index_ != LONG_MIN && !player_->paused()) effects_.on_beat(0.8f);
            last_beat_index_ = p.index;
        }
    }

    if (screen_ != Screen::Calibration) handle_global_keys();

    switch (screen_) {
        case Screen::Title: tick_title(); break;
        case Screen::ModeSelect: tick_mode_select(); break;
        case Screen::Playlist: tick_playlist(); break;
        case Screen::Playing: tick_game(dt); break;
        case Screen::Results: tick_results(); break;
        case Screen::HighScores: tick_high_scores(); break;
        case Screen::Settings: tick_settings(); break;
        case Screen::Calibration: tick_calibration(); break;
    }
}

void App::handle_resize() {
    const int w = GetScreenWidth(), h = GetScreenHeight();
    if (w <= 0 || h <= 0) return;
    if (w != screen_w_ || h != screen_h_ || viz_scale_applied_ != settings_.viz_resolution) {
        screen_w_ = w;
        screen_h_ = h;
        viz_scale_applied_ = settings_.viz_resolution;
        comp_->resize(w, h);
        viz_->resize(static_cast<int>(static_cast<float>(w) * settings_.viz_resolution),
                     static_cast<int>(static_cast<float>(h) * settings_.viz_resolution));
        layout_ = compute_layout(static_cast<float>(w), static_cast<float>(h));
    }
}

void App::handle_drops() {
    if (!IsFileDropped()) return;
    FilePathList list = LoadDroppedFiles();
    std::vector<std::filesystem::path> paths;
    for (unsigned i = 0; i < list.count; ++i) paths.push_back(persist::utf8_to_path(list.paths[i]));
    UnloadDroppedFiles(list);
    add_paths(paths);
}

void App::add_paths(const std::vector<std::filesystem::path>& paths) {
    const auto files = audio::collect_audio(paths);
    if (files.empty()) {
        if (!paths.empty()) toast("No supported audio found (mp3, flac, wav, ogg)");
        return;
    }
    std::vector<audio::Track> tracks;
    for (const auto& f : files) tracks.push_back(audio::make_track(f));
    const auto before = player_->size();
    player_->add_tracks(tracks);
    const auto added = player_->size() - before;
    toast("Added " + std::to_string(added) + (added == 1 ? " track" : " tracks"));
    save_playlist();
}

void App::handle_global_keys() {
    if (IsKeyPressed(KEY_F11)) {
        ToggleBorderlessWindowed();
        settings_.fullscreen = !settings_.fullscreen;
        persist::save_settings(settings_);
    }
    if (IsKeyPressed(KEY_TAB) && viz_->supports_presets()) {
        viz_->next_preset(false);
        toast("Visual: " + viz_->current_preset());
    }
    if (IsKeyPressed(KEY_F8) && viz_->supports_presets()) {
        const std::string name = viz_->current_preset();
        viz_->ban_current();
        toast("Banned preset " + name);
    }
    const bool in_game = screen_ == Screen::Playing && !paused_;
    if (!in_game) {
        if (IsKeyPressed(KEY_RIGHT_BRACKET)) player_->next();
        if (IsKeyPressed(KEY_LEFT_BRACKET)) player_->previous();
    }
}

void App::update_track_state() {
    const std::uint64_t gen = player_->generation();
    if (gen == track_generation_) return;
    track_generation_ = gen;
    const auto np = player_->now_playing();
    track_override_ = np.override;
    track_title_ = np.valid ? np.track.title : "";
    track_path_ = np.valid && !np.override ? np.track.path : std::filesystem::path{};
    last_beat_index_ = LONG_MIN;
    analysis_revision_ = ~0ull;
    grid_.reset();
    beat_state_ = audio::AnalysisWorker::State::Missing;
    if (!np.valid || np.override) return;

    track_banner_at_ = wall_;
    analysis_->request(track_path_, true);
    if (auto next = player_->upcoming()) analysis_->request(next->path, false);
    viz_->next_preset(false);
    if (game_ && !game_->over()) {
        if (run_tracks_.empty() || run_tracks_.back() != track_title_) run_tracks_.push_back(track_title_);
    }
    save_playlist();
}

void App::refresh_beat_grid() {
    if (track_override_) {
        grid_ = cal_grid_;
        beat_state_ = audio::AnalysisWorker::State::Ready;
        return;
    }
    if (track_path_.empty()) return;
    const auto rev = analysis_->revision();
    if (rev == analysis_revision_) return;
    analysis_revision_ = rev;
    const auto r = analysis_->get(track_path_);
    beat_state_ = r.state;
    if (r.state == audio::AnalysisWorker::State::Ready) {
        raw_grid_ = r.raw;
        adjust_ = r.adjust;
        grid_ = sync::apply_adjust(raw_grid_, adjust_);
    } else {
        grid_.reset();
    }
}

// --- playlist persistence -------------------------------------------------

void App::save_playlist() const {
    nlohmann::json tracks = nlohmann::json::array();
    for (const auto& t : player_->tracks()) tracks.push_back(persist::path_to_utf8(t.path));
    const auto np = player_->now_playing();
    const nlohmann::json j{
        {"tracks", tracks},
        {"shuffle", player_->shuffle()},
        {"repeat", player_->repeat()},
        {"index", np.valid && !np.override ? static_cast<int>(np.index) : -1},
    };
    persist::write_file_atomic(persist::data_dir() / "playlist.json", j.dump(2));
}

void App::load_playlist() {
    auto text = persist::read_file(persist::data_dir() / "playlist.json");
    if (!text) return;
    const auto j = nlohmann::json::parse(*text, nullptr, false);
    if (j.is_discarded()) return;
    try {
        player_->set_shuffle(j.value("shuffle", false));
        player_->set_repeat(j.value("repeat", true));
        std::vector<audio::Track> tracks;
        std::error_code ec;
        for (const auto& p : j.value("tracks", nlohmann::json::array())) {
            auto path = persist::utf8_to_path(p.get<std::string>());
            if (std::filesystem::is_regular_file(path, ec)) tracks.push_back(audio::make_track(path));
        }
        player_->add_tracks(tracks);
        const int index = j.value("index", -1);
        if (index > 0 && static_cast<std::size_t>(index) < tracks.size()) player_->play_index(static_cast<std::size_t>(index));
    } catch (const nlohmann::json::exception&) {
    }
}

// --- rendering ------------------------------------------------------------

void App::render(double frame_dt) {
    pcm_scratch_.clear();
    player_->drain_pcm(pcm_scratch_);
    viz_->add_pcm(pcm_scratch_);
    spectrum_.push(pcm_scratch_);
    spectrum_.update(frame_dt);
    effects_.update(static_cast<float>(frame_dt));

    viz_->render(spectrum_, wall_);
    comp_->build_blur(viz_->texture());

    comp_->begin_scene();
    const bool menu_screen = screen_ != Screen::Playing;
    comp_->draw_viz(viz_->texture(), settings_.viz_brightness * (menu_screen ? 0.75f : 1.0f), 1.0f);
    switch (screen_) {
        case Screen::Title: draw_title(); break;
        case Screen::ModeSelect: draw_mode_select(); break;
        case Screen::Playlist: draw_playlist(); break;
        case Screen::Playing: draw_game(); break;
        case Screen::Results: draw_results(); break;
        case Screen::HighScores: draw_high_scores(); break;
        case Screen::Settings: draw_settings(); break;
        case Screen::Calibration: draw_calibration(); break;
    }
    draw_toast();
    comp_->end_scene();

    BeginDrawing();
    ClearBackground(BLACK);
    viz::PostParams post = screen_ == Screen::Playing ? effects_.post() : viz::PostParams{};
    comp_->present(post);
    EndDrawing();
    debug_after_present();
}

float App::row_height() const { return std::clamp(static_cast<float>(screen_h_) * 0.052f, 30.0f, 64.0f); }

Rectangle App::menu_area(float width_frac, float top_frac, int rows, float row_h) const {
    const float w = static_cast<float>(screen_w_) * width_frac;
    return {(static_cast<float>(screen_w_) - w) / 2, static_cast<float>(screen_h_) * top_frac, w, row_h * static_cast<float>(rows)};
}

void App::draw_header(const char* title, const char* subtitle) {
    const float h = static_cast<float>(screen_h_);
    const float size = h * 0.075f;
    glow_text_centered(theme_, title, {static_cast<float>(screen_w_) / 2, h * 0.12f}, size, theme_.text, 0.6f,
                       FontWeight::Bold, size * 0.12f);
    if (subtitle)
        text_centered(theme_, subtitle, {static_cast<float>(screen_w_) / 2, h * 0.12f + size * 0.75f}, size * 0.32f,
                      theme_.text_dim, FontWeight::Regular, 2);
}

void App::draw_panel(Rectangle r, float pad) {
    const Rectangle p{r.x - pad, r.y - pad, r.width + pad * 2, r.height + pad * 2};
    comp_->draw_frosted(p, 0.55f);
    DrawRectangleLinesEx(p, 1, with_alpha(theme_.line, 0.08f));
}

void App::draw_now_playing() {
    const float h = static_cast<float>(screen_h_), w = static_cast<float>(screen_w_);
    const float size = std::clamp(h * 0.024f, 14.0f, 26.0f);
    const float y = h - size * 2.4f;
    const auto np = player_->now_playing();
    if (np.valid) {
        label(theme_, "NOW PLAYING", {size, y - size * 1.05f}, size * 0.7f, theme_.text_faint);
        std::string line = np.track.title;
        if (const auto* g = usable_grid()) line += "   " + std::to_string(static_cast<int>(std::lround(g->bpm))) + " BPM";
        text(theme_, line, {size, y}, size * 1.15f, theme_.text, FontWeight::Regular, 1);
        const float progress = np.length > 0 ? static_cast<float>(std::clamp(player_->time() / np.length, 0.0, 1.0)) : 0;
        DrawRectangleRec({size, y + size * 1.5f, w * 0.25f, 2}, with_alpha(theme_.line, 0.15f));
        DrawRectangleRec({size, y + size * 1.5f, w * 0.25f * progress, 2}, with_alpha(theme_.accent, 0.9f));
    } else {
        text(theme_, "Drop music files or folders onto the window to start", {size, y}, size, theme_.text_dim);
    }
    std::string keys = "[ ] TRACK    TAB VISUAL    F8 BAN VISUAL    F11 FULLSCREEN";
    text_right(theme_, keys, {w - size, y + size * 0.3f}, size * 0.7f, theme_.text_faint, FontWeight::Regular, 1.5f);
    if (viz_->supports_presets())
        text_right(theme_, viz_->current_preset(), {w - size, y - size * 1.0f}, size * 0.7f, with_alpha(theme_.text_faint, 0.7f));
}

void App::draw_toast() {
    const double age = wall_ - toast_at_;
    if (age > 2.5 || toast_.empty()) return;
    const float a = static_cast<float>(std::min(1.0, (2.5 - age) / 0.4));
    const float size = std::clamp(static_cast<float>(screen_h_) * 0.026f, 14.0f, 28.0f);
    const Vector2 m = measure(theme_, toast_, size);
    const Rectangle r{(static_cast<float>(screen_w_) - m.x) / 2 - size, size * 0.8f, m.x + size * 2, m.y + size * 0.6f};
    DrawRectangleRounded(r, 0.5f, 8, with_alpha(theme_.panel, 0.8f * a));
    text_centered(theme_, toast_, {r.x + r.width / 2, r.y + r.height / 2}, size, with_alpha(theme_.text, a));
}

}  // namespace audrop
