#include "platform/file_dialog.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>

#ifdef AUDROP_HAS_NFD
#include <nfd.h>
#endif

namespace audrop::platform {
namespace {

#ifdef AUDROP_HAS_NFD
bool nfd_ready = false;

std::vector<std::filesystem::path> collect(const nfdpathset_t* set) {
    std::vector<std::filesystem::path> out;
    nfdpathsetsize_t count = 0;
    if (NFD_PathSet_GetCount(set, &count) != NFD_OKAY) return out;
    for (nfdpathsetsize_t i = 0; i < count; ++i) {
        nfdu8char_t* p = nullptr;
        if (NFD_PathSet_GetPathU8(set, i, &p) == NFD_OKAY && p) {
            out.emplace_back(std::u8string(reinterpret_cast<const char8_t*>(p)));
            NFD_PathSet_FreePathU8(p);
        }
    }
    return out;
}
#endif

#if defined(__linux__) || defined(__FreeBSD__)
bool have_command(const char* name) {
    const std::string cmd = std::string("command -v ") + name + " >/dev/null 2>&1";
    return std::system(cmd.c_str()) == 0;
}

std::vector<std::filesystem::path> run_lines(const std::string& cmd) {
    std::vector<std::filesystem::path> out;
    FILE* p = popen(cmd.c_str(), "r");
    if (!p) return out;
    std::string line;
    char buf[4096];
    while (std::fgets(buf, sizeof buf, p)) {
        line = buf;
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
        if (!line.empty()) out.emplace_back(line);
    }
    pclose(p);
    return out;
}

std::vector<std::filesystem::path> fallback_pick(bool folders) {
    static const bool zenity = have_command("zenity");
    static const bool kdialog = !zenity && have_command("kdialog");
    if (zenity) {
        std::string cmd = "zenity --file-selection --multiple --separator='\n' 2>/dev/null";
        if (folders)
            cmd = "zenity --file-selection --directory --multiple --separator='\n' --title='Add music folders' 2>/dev/null";
        else
            cmd = "zenity --file-selection --multiple --separator='\n' --title='Add music' "
                  "--file-filter='Audio | *.mp3 *.flac *.wav *.ogg *.qoa *.MP3 *.FLAC *.WAV *.OGG' 2>/dev/null";
        return run_lines(cmd);
    }
    if (kdialog) {
        if (folders) return run_lines("kdialog --getexistingdirectory . 2>/dev/null");
        return run_lines("kdialog --getopenfilename . 'Audio (*.mp3 *.flac *.wav *.ogg *.qoa)' --multiple --separate-output 2>/dev/null");
    }
    return {};
}
#endif

}  // namespace

void init_file_dialogs() {
#ifdef AUDROP_HAS_NFD
    nfd_ready = NFD_Init() == NFD_OKAY;
#endif
}

void shutdown_file_dialogs() {
#ifdef AUDROP_HAS_NFD
    if (nfd_ready) NFD_Quit();
    nfd_ready = false;
#endif
}

bool file_dialogs_available() {
#ifdef AUDROP_HAS_NFD
    if (nfd_ready) return true;
#endif
#if defined(__linux__) || defined(__FreeBSD__)
    return have_command("zenity") || have_command("kdialog");
#else
    return false;
#endif
}

std::vector<std::filesystem::path> pick_audio_files() {
#ifdef AUDROP_HAS_NFD
    if (nfd_ready) {
        nfdu8filteritem_t filters[] = {{"Audio", "mp3,flac,wav,ogg,qoa"}};
        const nfdpathset_t* set = nullptr;
        if (NFD_OpenDialogMultipleU8(&set, filters, 1, nullptr) != NFD_OKAY || !set) return {};
        auto out = collect(set);
        NFD_PathSet_Free(set);
        return out;
    }
#endif
#if defined(__linux__) || defined(__FreeBSD__)
    return fallback_pick(false);
#else
    return {};
#endif
}

std::vector<std::filesystem::path> pick_folders() {
#ifdef AUDROP_HAS_NFD
    if (nfd_ready) {
        const nfdpathset_t* set = nullptr;
        if (NFD_PickFolderMultipleU8(&set, nullptr) != NFD_OKAY || !set) return {};
        auto out = collect(set);
        NFD_PathSet_Free(set);
        return out;
    }
#endif
#if defined(__linux__) || defined(__FreeBSD__)
    return fallback_pick(true);
#else
    return {};
#endif
}

}  // namespace audrop::platform
