#include "audio/tags.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <exception>
#include <fstream>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace audrop::audio {
namespace {

using Bytes = std::vector<unsigned char>;
using View = std::span<const unsigned char>;

// Album art can make tags large; anything beyond this is skipped rather than read.
constexpr std::size_t kMaxTagBytes = 16u << 20;

std::uint32_t be24(const unsigned char* p) { return (p[0] << 16) | (p[1] << 8) | p[2]; }
std::uint32_t be32(const unsigned char* p) {
    return (std::uint32_t{p[0]} << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}
std::uint32_t le32(const unsigned char* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (std::uint32_t{p[3]} << 24);
}
std::uint32_t syncsafe(const unsigned char* p) {
    return ((p[0] & 0x7Fu) << 21) | ((p[1] & 0x7Fu) << 14) | ((p[2] & 0x7Fu) << 7) | (p[3] & 0x7Fu);
}

std::optional<Bytes> read_exact(std::istream& in, std::size_t n) {
    if (n > kMaxTagBytes) return std::nullopt;
    Bytes b(n);
    if (n > 0 && !in.read(reinterpret_cast<char*>(b.data()), static_cast<std::streamsize>(n))) return std::nullopt;
    return b;
}

bool starts_with(View d, std::string_view magic) {
    return d.size() >= magic.size() && std::memcmp(d.data(), magic.data(), magic.size()) == 0;
}

// --- text decoding --------------------------------------------------------

void append_utf8(std::string& out, char32_t c) {
    if (c < 0x80) {
        out += static_cast<char>(c);
    } else if (c < 0x800) {
        out += static_cast<char>(0xC0 | (c >> 6));
        out += static_cast<char>(0x80 | (c & 0x3F));
    } else if (c < 0x10000) {
        out += static_cast<char>(0xE0 | (c >> 12));
        out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (c & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (c >> 18));
        out += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (c & 0x3F));
    }
}

bool valid_utf8(std::string_view s) {
    for (std::size_t i = 0; i < s.size();) {
        const auto c = static_cast<unsigned char>(s[i]);
        const std::size_t n = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 0;
        if (n == 0 || i + n > s.size()) return false;
        for (std::size_t k = 1; k < n; ++k)
            if ((static_cast<unsigned char>(s[i + k]) & 0xC0) != 0x80) return false;
        i += n;
    }
    return true;
}

// Cuts at the first NUL (ID3v2.4 multi-value separator / C-string padding) and trims whitespace.
std::string clean(std::string s) {
    if (const auto nul = s.find('\0'); nul != std::string::npos) s.resize(nul);
    const auto blank = [](unsigned char c) { return std::isspace(c) != 0; };
    while (!s.empty() && blank(static_cast<unsigned char>(s.back()))) s.pop_back();
    const auto first = std::ranges::find_if_not(s, blank);
    return std::string(first, s.end());
}

// Nominally Latin-1 text (ID3v1, RIFF INFO, ID3v2 encoding 0) is often UTF-8 in practice.
std::string narrow_to_utf8(View d) {
    std::string raw(d.begin(), d.end());
    raw = clean(std::move(raw));
    if (valid_utf8(raw)) return raw;
    std::string out;
    for (const unsigned char c : raw) append_utf8(out, c);
    return out;
}

std::string utf16_to_utf8(View d, bool big_endian) {
    std::string out;
    for (std::size_t i = 0; i + 1 < d.size(); i += 2) {
        char32_t u = big_endian ? (d[i] << 8) | d[i + 1] : d[i] | (d[i + 1] << 8);
        if (u == 0) break;
        if (u >= 0xD800 && u < 0xDC00 && i + 3 < d.size()) {
            const char32_t lo = big_endian ? (d[i + 2] << 8) | d[i + 3] : d[i + 2] | (d[i + 3] << 8);
            if (lo >= 0xDC00 && lo < 0xE000) {
                u = 0x10000 + ((u - 0xD800) << 10) + (lo - 0xDC00);
                i += 2;
            }
        }
        append_utf8(out, u);
    }
    return clean(std::move(out));
}

void fill_missing(TrackTags& dst, const TrackTags& src) {
    if (dst.title.empty()) dst.title = src.title;
    if (dst.artist.empty()) dst.artist = src.artist;
    if (dst.album.empty()) dst.album = src.album;
}

bool complete(const TrackTags& t) { return !t.title.empty() && !t.artist.empty() && !t.album.empty(); }

// --- ID3 ------------------------------------------------------------------

Bytes remove_unsync(View d) {
    Bytes out;
    out.reserve(d.size());
    for (std::size_t i = 0; i < d.size(); ++i) {
        out.push_back(d[i]);
        if (d[i] == 0xFF && i + 1 < d.size() && d[i + 1] == 0x00) ++i;
    }
    return out;
}

std::string id3_text(View d) {
    if (d.empty()) return {};
    const unsigned char enc = d[0];
    d = d.subspan(1);
    switch (enc) {
        case 0: return narrow_to_utf8(d);
        case 1: {
            bool be = false;  // BOM is mandatory; little-endian is the common default when it's missing
            if (d.size() >= 2 && ((d[0] == 0xFE && d[1] == 0xFF) || (d[0] == 0xFF && d[1] == 0xFE))) {
                be = d[0] == 0xFE;
                d = d.subspan(2);
            }
            return utf16_to_utf8(d, be);
        }
        case 2: return utf16_to_utf8(d, true);
        default: return clean(std::string(d.begin(), d.end()));
    }
}

// `tag` starts at the "ID3" header; it may be truncated.
void parse_id3v2(View tag, TrackTags& out) {
    if (tag.size() < 10 || !starts_with(tag, "ID3")) return;
    const int ver = tag[3];
    const unsigned char flags = tag[5];
    if (ver < 2 || ver > 4) return;
    View body_view = tag.subspan(10, std::min<std::size_t>(syncsafe(&tag[6]), tag.size() - 10));
    Bytes unsynced;
    if ((flags & 0x80) && ver < 4) {
        unsynced = remove_unsync(body_view);
        body_view = unsynced;
    }
    const View body = body_view;

    std::size_t pos = 0;
    if ((flags & 0x40) && ver >= 3) {
        if (body.size() < 4) return;
        pos = ver == 4 ? syncsafe(body.data()) : be32(body.data()) + 4;
    }
    const std::size_t id_len = ver == 2 ? 3 : 4, header_len = ver == 2 ? 6 : 10;
    std::string album_artist;
    while (pos + header_len <= body.size()) {
        const unsigned char* h = &body[pos];
        if (h[0] == 0) break;  // padding
        const std::size_t size = ver == 2 ? be24(h + 3) : ver == 4 ? syncsafe(h + 4) : be32(h + 4);
        const unsigned frame_flags = ver == 2 ? 0 : (h[8] << 8) | h[9];
        const std::string_view id(reinterpret_cast<const char*>(h), id_len);
        pos += header_len;
        if (size > body.size() - pos) break;
        View data = body.subspan(pos, size);
        pos += size;

        std::string* dst = id == "TIT2" || id == "TT2"   ? &out.title
                           : id == "TPE1" || id == "TP1" ? &out.artist
                           : id == "TALB" || id == "TAL" ? &out.album
                           : id == "TPE2" || id == "TP2" ? &album_artist
                                                         : nullptr;
        if (!dst || !dst->empty()) continue;

        Bytes frame_unsynced;
        if (ver == 3) {
            if (frame_flags & 0x00C0) continue;  // compressed / encrypted
            if (frame_flags & 0x0020) data = data.subspan(std::min<std::size_t>(1, data.size()));
        } else if (ver == 4) {
            if (frame_flags & 0x000C) continue;  // compressed / encrypted
            if (frame_flags & 0x0040) data = data.subspan(std::min<std::size_t>(1, data.size()));
            if (frame_flags & 0x0001) data = data.subspan(std::min<std::size_t>(4, data.size()));
            if ((frame_flags & 0x0002) || (flags & 0x80)) {
                frame_unsynced = remove_unsync(data);
                data = frame_unsynced;
            }
        }
        *dst = id3_text(data);
    }
    if (out.artist.empty()) out.artist = album_artist;
}

void parse_id3v1(std::istream& in, TrackTags& out) {
    in.clear();
    if (!in.seekg(-128, std::ios::end)) return;
    const auto d = read_exact(in, 128);
    if (!d || !starts_with(*d, "TAG")) return;
    const View v = *d;
    TrackTags t{narrow_to_utf8(v.subspan(3, 30)), narrow_to_utf8(v.subspan(33, 30)), narrow_to_utf8(v.subspan(63, 30))};
    fill_missing(out, t);
}

// --- Vorbis comments (FLAC, Ogg) -------------------------------------------

void parse_vorbis_comment(View d, TrackTags& out) {
    std::size_t pos = 0;
    auto u32 = [&]() -> std::optional<std::uint32_t> {
        if (pos + 4 > d.size()) return std::nullopt;
        const auto v = le32(&d[pos]);
        pos += 4;
        return v;
    };
    const auto vendor = u32();
    if (!vendor || *vendor > d.size() - pos) return;
    pos += *vendor;
    const auto count = u32();
    if (!count) return;
    std::string album_artist;
    for (std::uint32_t i = 0; i < *count; ++i) {
        const auto len = u32();
        if (!len || *len > d.size() - pos) return;
        const std::string_view entry(reinterpret_cast<const char*>(&d[pos]), *len);
        pos += *len;
        const auto eq = entry.find('=');
        if (eq == std::string_view::npos) continue;
        std::string key(entry.substr(0, eq));
        for (auto& c : key) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        std::string* dst = key == "TITLE"         ? &out.title
                           : key == "ARTIST"      ? &out.artist
                           : key == "ALBUM"       ? &out.album
                           : key == "ALBUMARTIST" ? &album_artist
                                                  : nullptr;
        if (dst && dst->empty()) *dst = clean(std::string(entry.substr(eq + 1)));
    }
    if (out.artist.empty()) out.artist = album_artist;
}

void parse_flac(std::istream& in, std::streamoff start, TrackTags& out) {
    in.clear();
    if (!in.seekg(start + 4)) return;
    for (int i = 0; i < 128; ++i) {
        unsigned char h[4];
        if (!in.read(reinterpret_cast<char*>(h), 4)) return;
        const bool last = (h[0] & 0x80) != 0;
        const std::size_t len = be24(h + 1);
        if ((h[0] & 0x7F) == 4) {
            if (const auto d = read_exact(in, len)) parse_vorbis_comment(*d, out);
            return;
        }
        if (last || !in.seekg(static_cast<std::streamoff>(len), std::ios::cur)) return;
    }
}

void parse_ogg(std::istream& in, TrackTags& out) {
    in.clear();
    in.seekg(0);
    // The comment header is the second packet of the first logical stream.
    Bytes packet;
    int packet_no = 0;
    std::optional<std::uint32_t> serial;
    for (int page = 0; page < 256; ++page) {
        unsigned char h[27];
        if (!in.read(reinterpret_cast<char*>(h), 27) || std::memcmp(h, "OggS", 4) != 0) return;
        const auto segments = read_exact(in, h[26]);
        if (!segments) return;
        std::size_t total = 0;
        for (const auto s : *segments) total += s;
        const auto body = read_exact(in, total);
        if (!body) return;
        if (!serial) serial = le32(h + 14);
        if (le32(h + 14) != *serial) continue;
        std::size_t off = 0;
        for (const auto s : *segments) {
            if (packet_no == 1) packet.insert(packet.end(), body->begin() + off, body->begin() + off + s);
            off += s;
            if (s == 255) continue;
            if (packet_no == 1) {
                const View p = packet;
                if (starts_with(p, "\x03vorbis")) parse_vorbis_comment(p.subspan(7), out);
                else if (starts_with(p, "OpusTags")) parse_vorbis_comment(p.subspan(8), out);
                return;
            }
            ++packet_no;
        }
        if (packet.size() > kMaxTagBytes) return;
    }
}

// --- RIFF / WAV -----------------------------------------------------------

void parse_riff_info(View d, TrackTags& out) {
    std::size_t pos = 4;  // skip "INFO"
    while (pos + 8 <= d.size()) {
        const std::string_view id(reinterpret_cast<const char*>(&d[pos]), 4);
        const std::size_t len = le32(&d[pos + 4]);
        pos += 8;
        if (len > d.size() - pos) return;
        std::string* dst = id == "INAM" ? &out.title : id == "IART" ? &out.artist : id == "IPRD" ? &out.album : nullptr;
        if (dst && dst->empty()) *dst = narrow_to_utf8(d.subspan(pos, len));
        pos += len + (len & 1);
    }
}

void parse_riff(std::istream& in, TrackTags& out) {
    in.clear();
    if (!in.seekg(12)) return;
    for (int i = 0; i < 256; ++i) {
        unsigned char h[8];
        if (!in.read(reinterpret_cast<char*>(h), 8)) return;
        const std::string_view id(reinterpret_cast<const char*>(h), 4);
        const std::size_t len = le32(h + 4);
        const bool info = id == "LIST", id3 = id == "id3 " || id == "ID3 ";
        if ((info || id3) && len <= kMaxTagBytes) {
            const auto d = read_exact(in, len);
            if (!d) return;
            if (info && starts_with(*d, "INFO")) parse_riff_info(*d, out);
            if (id3) parse_id3v2(*d, out);
            if ((len & 1) && !in.seekg(1, std::ios::cur)) return;
        } else if (!in.seekg(static_cast<std::streamoff>(len + (len & 1)), std::ios::cur)) {
            return;
        }
    }
}

}  // namespace

TrackTags read_tags(std::istream& in) {
    TrackTags tags;
    try {
        unsigned char head[12] = {};
        in.read(reinterpret_cast<char*>(head), sizeof head);
        const View h(head, static_cast<std::size_t>(in.gcount()));
        if (starts_with(h, "ID3") && h.size() >= 10) {
            std::size_t size = std::size_t{syncsafe(head + 6)} + 10;
            if (head[5] & 0x10) size += 10;  // footer
            in.clear();
            in.seekg(0);
            Bytes tag(std::min(size, kMaxTagBytes));
            in.read(reinterpret_cast<char*>(tag.data()), static_cast<std::streamsize>(tag.size()));
            tag.resize(static_cast<std::size_t>(in.gcount()));
            parse_id3v2(tag, tags);
            // Some FLAC files carry a leading ID3v2 tag.
            if (!complete(tags) && tag.size() == size) {
                unsigned char magic[4] = {};
                in.clear();
                in.seekg(static_cast<std::streamoff>(size));
                if (in.read(reinterpret_cast<char*>(magic), 4) && std::memcmp(magic, "fLaC", 4) == 0) {
                    TrackTags flac;
                    parse_flac(in, static_cast<std::streamoff>(size), flac);
                    fill_missing(tags, flac);
                }
            }
        } else if (starts_with(h, "fLaC")) {
            parse_flac(in, 0, tags);
        } else if (starts_with(h, "OggS")) {
            parse_ogg(in, tags);
        } else if (starts_with(h, "RIFF") && h.size() >= 12 && std::memcmp(head + 8, "WAVE", 4) == 0) {
            parse_riff(in, tags);
        }
        if (!complete(tags)) parse_id3v1(in, tags);
    } catch (const std::exception&) {
    }
    return tags;
}

TrackTags read_tags(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in) return {};
    return read_tags(in);
}

}  // namespace audrop::audio
