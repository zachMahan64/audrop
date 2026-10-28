#include <doctest/doctest.h>

#include <cstdint>
#include <sstream>
#include <string>

#include "audio/tags.hpp"

using namespace audrop::audio;

namespace {

std::string be32(std::uint32_t v) {
    return {static_cast<char>(v >> 24), static_cast<char>(v >> 16), static_cast<char>(v >> 8), static_cast<char>(v)};
}
std::string le32(std::uint32_t v) {
    return {static_cast<char>(v), static_cast<char>(v >> 8), static_cast<char>(v >> 16), static_cast<char>(v >> 24)};
}
std::string syncsafe(std::uint32_t v) {
    return {static_cast<char>((v >> 21) & 0x7F), static_cast<char>((v >> 14) & 0x7F), static_cast<char>((v >> 7) & 0x7F),
            static_cast<char>(v & 0x7F)};
}

std::string id3_frame(int ver, const std::string& id, const std::string& payload) {
    return id + (ver == 4 ? syncsafe(static_cast<std::uint32_t>(payload.size())) : be32(static_cast<std::uint32_t>(payload.size()))) +
           std::string(2, '\0') + payload;
}

std::string id3_tag(int ver, const std::string& frames) {
    return "ID3" + std::string{static_cast<char>(ver), 0, 0} + syncsafe(static_cast<std::uint32_t>(frames.size() + 16)) +
           frames + std::string(16, '\0');
}

TrackTags parse(const std::string& bytes) {
    std::istringstream in(bytes, std::ios::binary);
    return read_tags(in);
}

std::string vorbis_comment(std::initializer_list<std::string> entries) {
    std::string out = le32(4) + "test" + le32(static_cast<std::uint32_t>(entries.size()));
    for (const auto& e : entries) out += le32(static_cast<std::uint32_t>(e.size())) + e;
    return out;
}

}  // namespace

TEST_CASE("id3v2.3 with latin-1 and utf-16 frames") {
    const std::string utf16_title = std::string("\x01\xFF\xFE", 3) + std::string("H\0i\0", 4);
    const auto t = parse(id3_tag(3, id3_frame(3, "TIT2", utf16_title) + id3_frame(3, "TPE1", std::string("\0Caf\xE9", 5)) +
                                        id3_frame(3, "TALB", std::string("\x03" "Album\0", 7))) +
                         "audio data");
    CHECK(t.title == "Hi");
    CHECK(t.artist == "Caf\xC3\xA9");
    CHECK(t.album == "Album");
}

TEST_CASE("id3v2.4 uses syncsafe frame sizes and falls back to album artist") {
    const auto t = parse(id3_tag(4, id3_frame(4, "TIT2", std::string("\x03Song", 5)) +
                                        id3_frame(4, "TPE2", std::string("\x03" "Band", 5))));
    CHECK(t.title == "Song");
    CHECK(t.artist == "Band");
    CHECK(t.album.empty());
}

TEST_CASE("id3v1 fills fields id3v2 lacks") {
    auto field = [](std::string s) { s.resize(30, '\0'); return s; };
    const std::string v1 = "TAG" + field("Old Title") + field("Old Artist") + field("Old Album") + std::string(35, '\0');
    const auto t = parse(id3_tag(3, id3_frame(3, "TIT2", std::string("\0New Title", 10))) + std::string(64, 'x') + v1);
    CHECK(t.title == "New Title");
    CHECK(t.artist == "Old Artist");
    CHECK(t.album == "Old Album");
}

TEST_CASE("flac vorbis comment, keys are case-insensitive") {
    const std::string streaminfo(34, '\0');
    const auto vc = vorbis_comment({"title=Track", "ARTIST=Someone", "Album=Record"});
    const std::string flac = "fLaC" + std::string{0x00, 0, 0, 34} + streaminfo + std::string{static_cast<char>(0x84)} +
                             be32(static_cast<std::uint32_t>(vc.size())).substr(1) + vc;
    const auto t = parse(flac);
    CHECK(t.title == "Track");
    CHECK(t.artist == "Someone");
    CHECK(t.album == "Record");
}

TEST_CASE("ogg vorbis comment packet") {
    auto page = [](const std::string& packet) {
        return "OggS" + std::string(10, '\0') + le32(7) + std::string(8, '\0') +
               std::string{1, static_cast<char>(packet.size())} + packet;
    };
    const auto t = parse(page("\x01vorbis-id") + page("\x03vorbis" + vorbis_comment({"TITLE=Ogg Song", "ARTIST=Ogg Artist"})));
    CHECK(t.title == "Ogg Song");
    CHECK(t.artist == "Ogg Artist");
}

TEST_CASE("wav LIST INFO chunk") {
    const std::string info = "INFO" + std::string("INAM") + le32(5) + std::string("Wave\0", 5) + std::string(1, '\0') + "IART" + le32(4) +
                             std::string("Me\0\0", 4);
    const std::string riff = "RIFF" + le32(0) + "WAVE" + "fmt " + le32(2) + "ab" + "LIST" + le32(static_cast<std::uint32_t>(info.size())) + info;
    const auto t = parse(riff);
    CHECK(t.title == "Wave");
    CHECK(t.artist == "Me");
}

TEST_CASE("untagged and malformed input yields empty tags") {
    CHECK(parse("qoaf garbage").title.empty());
    CHECK(parse("ID3\x03").title.empty());
    CHECK(parse(std::string("ID3\x03\0\0\x7F\x7F\x7F\x7F", 10) + "TIT2").title.empty());
}
