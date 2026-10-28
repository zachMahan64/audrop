#include "viz/visualizer.hpp"

#ifdef AUDROP_HAS_PROJECTM

#include <projectM-4/playlist.h>
#include <projectM-4/projectM.h>
#include <rlgl.h>

#include <algorithm>
#include <fstream>
#include <set>
#include <vector>

#include "persist/paths.hpp"
#include "platform/gl_state.hpp"

namespace audrop::viz {
namespace {

namespace fs = std::filesystem;

// Runs projectM calls that may touch GL (preset loads compile shaders) without disturbing raylib.
template <class F>
void with_gl(F&& f) {
    rlDrawRenderBatchActive();
    f();
    gl::restore_raylib_state(GetRenderWidth(), GetRenderHeight());
}

fs::path blocklist_path() { return persist::data_dir() / "preset_blocklist.txt"; }

std::set<std::string> load_blocklist() {
    std::set<std::string> out;
    std::ifstream in(blocklist_path());
    std::string line;
    while (std::getline(in, line))
        if (!line.empty()) out.insert(line);
    return out;
}

class ProjectMVisualizer final : public Visualizer {
public:
    ProjectMVisualizer(projectm_handle pm, fs::path preset_root) : pm_(pm), root_(std::move(preset_root)) {
        playlist_ = projectm_playlist_create(pm_);
        projectm_playlist_set_shuffle(playlist_, true);

        const auto blocked = load_blocklist();
        std::vector<std::string> files;
        std::error_code ec;
        for (auto it = fs::recursive_directory_iterator(root_, fs::directory_options::skip_permission_denied, ec);
             it != fs::recursive_directory_iterator(); it.increment(ec)) {
            if (ec) break;
            // Folders starting with '!' (e.g. "! Transition") hold short transition presets.
            if (it->is_directory(ec) && it->path().filename().string().starts_with("!")) {
                it.disable_recursion_pending();
                continue;
            }
            if (!it->is_regular_file(ec) || it->path().extension() != ".milk") continue;
            const std::string rel = persist::path_to_utf8(fs::relative(it->path(), root_, ec));
            if (blocked.contains(rel)) {
                banned_.push_back(persist::path_to_utf8(it->path()));
                continue;
            }
            files.push_back(persist::path_to_utf8(it->path()));
        }
        std::vector<const char*> ptrs;
        ptrs.reserve(files.size());
        for (const auto& f : files) ptrs.push_back(f.c_str());
        projectm_playlist_add_presets(playlist_, ptrs.data(), static_cast<uint32_t>(ptrs.size()), false);
        TraceLog(LOG_INFO, "audrop: projectM loaded %zu presets (%zu blocked)", files.size(), blocked.size());
        if (!files.empty()) with_gl([&] { projectm_playlist_play_next(playlist_, true); });
    }

    ~ProjectMVisualizer() override {
        if (playlist_) projectm_playlist_destroy(playlist_);
        projectm_destroy(pm_);
        if (target_.id) UnloadRenderTexture(target_);
    }

    std::string name() const override { return "projectM"; }
    bool supports_presets() const override { return true; }

    void resize(int width, int height) override {
        width = std::max(16, width);
        height = std::max(16, height);
        if (target_.id && target_.texture.width == width && target_.texture.height == height) return;
        if (target_.id) UnloadRenderTexture(target_);
        target_ = LoadRenderTexture(width, height);
        SetTextureFilter(target_.texture, TEXTURE_FILTER_BILINEAR);
        projectm_set_window_size(pm_, static_cast<size_t>(width), static_cast<size_t>(height));
    }

    void add_pcm(std::span<const float> stereo) override {
        const std::size_t max_frames = projectm_pcm_get_max_samples();
        std::size_t frames = stereo.size() / 2;
        const float* data = stereo.data();
        // projectM only keeps the most recent window, so skip anything older.
        if (frames > max_frames * 4) {
            data += (frames - max_frames * 4) * 2;
            frames = max_frames * 4;
        }
        while (frames > 0) {
            const std::size_t n = std::min(frames, max_frames);
            projectm_pcm_add_float(pm_, data, static_cast<unsigned int>(n), PROJECTM_STEREO);
            data += n * 2;
            frames -= n;
        }
    }

    void render(const audio::Spectrum&, double) override {
        if (!target_.id) return;
        // projectM 4.1 always presents to the default framebuffer, so render there first and
        // copy the result into our texture. Everything on screen is redrawn afterwards anyway.
        rlDrawRenderBatchActive();
        gl::prepare_external(GetRenderWidth(), GetRenderHeight());
        projectm_opengl_render_frame(pm_);
        gl::blit_default_to(target_.id, target_.texture.width, target_.texture.height);
        gl::restore_raylib_state(GetRenderWidth(), GetRenderHeight());
    }

    const Texture2D& texture() const override { return target_.texture; }

    void next_preset(bool hard_cut) override {
        if (projectm_playlist_size(playlist_) > 0) with_gl([&] { projectm_playlist_play_next(playlist_, hard_cut); });
    }

    void ban_current() override {
        const uint32_t pos = projectm_playlist_get_position(playlist_);
        char* item = projectm_playlist_item(playlist_, pos);
        if (!item) return;
        std::error_code ec;
        const std::string rel = persist::path_to_utf8(fs::relative(persist::utf8_to_path(item), root_, ec));
        banned_.emplace_back(item);
        projectm_playlist_free_string(item);
        {
            std::ofstream out(blocklist_path(), std::ios::app);
            out << rel << '\n';
        }
        TraceLog(LOG_INFO, "audrop: banned preset %s", rel.c_str());
        projectm_playlist_remove_preset(playlist_, pos);
        if (projectm_playlist_size(playlist_) > 0)
            with_gl([&] { projectm_playlist_set_position(playlist_, std::min(pos, projectm_playlist_size(playlist_) - 1), true); });
    }

    int unban_all() override {
        std::error_code ec;
        fs::remove(blocklist_path(), ec);
        if (banned_.empty()) return 0;
        std::vector<const char*> ptrs;
        for (const auto& f : banned_) ptrs.push_back(f.c_str());
        const bool was_empty = projectm_playlist_size(playlist_) == 0;
        projectm_playlist_add_presets(playlist_, ptrs.data(), static_cast<uint32_t>(ptrs.size()), false);
        if (was_empty) with_gl([&] { projectm_playlist_play_next(playlist_, true); });
        const int n = static_cast<int>(banned_.size());
        TraceLog(LOG_INFO, "audrop: unbanned %d presets", n);
        banned_.clear();
        return n;
    }

    int banned_count() const override { return static_cast<int>(banned_.size()); }

    std::string current_preset() const override {
        if (projectm_playlist_size(playlist_) == 0) return "(no presets found)";
        char* item = projectm_playlist_item(playlist_, projectm_playlist_get_position(playlist_));
        if (!item) return {};
        std::string name = persist::path_to_utf8(persist::utf8_to_path(item).stem());
        projectm_playlist_free_string(item);
        return name;
    }

    void set_preset_duration(double seconds) override { projectm_set_preset_duration(pm_, seconds); }

private:
    projectm_handle pm_;
    projectm_playlist_handle playlist_ = nullptr;
    fs::path root_;
    std::vector<std::string> banned_;  // absolute paths, so unban_all can add them back
    RenderTexture2D target_{};
};

}  // namespace

std::unique_ptr<Visualizer> make_projectm_visualizer(int width, int height, double preset_seconds) {
    auto presets = persist::preset_dir();
    if (!presets) {
        TraceLog(LOG_WARNING, "audrop: no projectM preset directory found");
        return nullptr;
    }
    if (!gl::load()) return nullptr;

    projectm_handle pm = nullptr;
    with_gl([&] { pm = projectm_create(); });
    if (!pm) {
        TraceLog(LOG_WARNING, "audrop: projectM failed to initialise (needs OpenGL 3.3 core)");
        return nullptr;
    }

    std::string tex_dir;
    if (auto t = persist::preset_texture_dir()) {
        tex_dir = persist::path_to_utf8(*t);
        const char* paths[] = {tex_dir.c_str()};
        projectm_set_texture_search_paths(pm, paths, 1);
    }
    projectm_set_fps(pm, 60);
    projectm_set_aspect_correction(pm, true);
    projectm_set_preset_duration(pm, preset_seconds);
    projectm_set_soft_cut_duration(pm, 3.0);
    // We trigger our own hard cuts on big on-beat clears; projectM's beat-driven ones are too flashy.
    projectm_set_hard_cut_enabled(pm, false);
    projectm_set_beat_sensitivity(pm, 1.0f);

    auto viz = std::make_unique<ProjectMVisualizer>(pm, *presets);
    viz->resize(width, height);
    return viz;
}

}  // namespace audrop::viz

#else

namespace audrop::viz {
std::unique_ptr<Visualizer> make_projectm_visualizer(int, int, double) { return nullptr; }
}  // namespace audrop::viz

#endif
