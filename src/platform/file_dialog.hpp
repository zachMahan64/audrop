#pragma once

#include <filesystem>
#include <vector>

namespace audrop::platform {

void init_file_dialogs();
void shutdown_file_dialogs();
bool file_dialogs_available();

// Both block until the dialog closes. Empty result = cancelled.
std::vector<std::filesystem::path> pick_audio_files();
std::vector<std::filesystem::path> pick_folders();

}  // namespace audrop::platform
