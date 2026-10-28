#pragma once

#include <filesystem>
#include <istream>
#include <string>

namespace audrop::audio {

// Track metadata as UTF-8. Fields are empty when the file has no tag for them.
struct TrackTags {
    std::string title;
    std::string artist;
    std::string album;
};

// Reads ID3v2/ID3v1 (mp3, wav "id3 " chunk), Vorbis comments (flac, ogg vorbis/opus) and
// RIFF INFO (wav). Never throws; unknown or malformed files yield empty tags.
TrackTags read_tags(const std::filesystem::path& p);
TrackTags read_tags(std::istream& in);

}  // namespace audrop::audio
