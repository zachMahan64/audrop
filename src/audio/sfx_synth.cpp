#include "audio/sfx_synth.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <random>

#include "config/tuning.hpp"

namespace audrop::audio {
namespace {

constexpr double kRate = kSfxSampleRate;
constexpr double kTau = 2 * std::numbers::pi;

// Small deterministic white noise source in [-1, 1].
class Noise {
public:
    explicit Noise(std::uint64_t seed) : state_(0x9E3779B97F4A7C15ull ^ (seed * 0xBF58476D1CE4E5B9ull)) {}
    double next() {
        state_ ^= state_ << 13;
        state_ ^= state_ >> 7;
        state_ ^= state_ << 17;
        return static_cast<double>(state_ >> 11) / static_cast<double>(1ull << 52) - 1.0;
    }

private:
    std::uint64_t state_;
};

std::size_t frames(double seconds) { return static_cast<std::size_t>(seconds * kRate); }

double one_pole(double hz) { return 1.0 - std::exp(-kTau * std::min(hz, kRate * 0.45) / kRate); }

// Sine with an exponential downward pitch glide, plus a short noise click on the attack.
void add_kick(std::vector<double>& out, const tuning::KickParams& p, double level, Noise& noise) {
    out.resize(std::max(out.size(), frames(p.length_s)), 0.0);
    double phase = 0, click_lp = 0;
    const double click_a = one_pole(4000);
    const double norm = std::tanh(p.drive);
    for (std::size_t i = 0; i < frames(p.length_s); ++i) {
        const double t = static_cast<double>(i) / kRate;
        const double hz = p.end_hz + (p.start_hz - p.end_hz) * std::exp(-t / p.glide_s);
        phase += kTau * hz / kRate;
        const double attack = 1.0 - std::exp(-t / 0.0015);
        double body = std::sin(phase) * attack * std::exp(-t / p.decay_s);
        body = std::tanh(p.drive * body) / norm;
        click_lp += click_a * (noise.next() - click_lp);
        out[i] += level * (body + p.click * click_lp * std::exp(-t / 0.004));
    }
}

void add_whoosh(std::vector<double>& out, const tuning::WhooshParams& p, double level, Noise& noise) {
    const std::size_t n = std::min(out.size(), frames(p.decay_s * 6));
    double lp1 = 0, lp2 = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / kRate;
        const double a = one_pole(p.end_hz + (p.start_hz - p.end_hz) * std::exp(-t / p.decay_s));
        const double x = noise.next();
        lp1 += a * (x - lp1);
        lp2 += a * (lp1 - lp2);
        const double filtered = p.highpass ? x - lp2 : lp2;
        const double env = (1.0 - std::exp(-t / 0.008)) * std::exp(-t / p.decay_s);
        out[i] += level * p.level * filtered * env;
    }
}

// Randomizes a take's parameters; take 0 keeps the tuned values.
class Variation {
public:
    Variation(SfxId id, int take) : rng_(static_cast<std::uint32_t>(static_cast<int>(id) * 1000 + take)), on_(take > 0) {}

    tuning::KickParams kick(tuning::KickParams p) {
        p.start_hz *= vary(tuning::kSfxVaryPitch);
        p.end_hz *= vary(tuning::kSfxVaryPitch);
        p.glide_s *= vary(tuning::kSfxVaryTime);
        p.decay_s *= vary(tuning::kSfxVaryTime);
        p.click *= vary(tuning::kSfxVaryClick);
        return p;
    }
    tuning::WhooshParams whoosh(tuning::WhooshParams p) {
        p.start_hz *= vary(tuning::kSfxVaryFilter);
        p.end_hz *= vary(tuning::kSfxVaryFilter);
        p.decay_s *= vary(tuning::kSfxVaryTime);
        return p;
    }
    double pitch() { return vary(tuning::kSfxVaryPitch); }
    double time() { return vary(tuning::kSfxVaryTime); }

private:
    double vary(double amount) { return on_ ? std::uniform_real_distribution(1.0 - amount, 1.0 + amount)(rng_) : 1.0; }

    std::mt19937 rng_;
    bool on_;
};

// --- chip voices -------------------------------------------------------------

// Volume stepped to 16 levels, like a 4-bit envelope.
double quantize(double v) { return std::round(std::clamp(v, 0.0, 1.0) * 15.0) / 15.0; }

double pulse(double phase, double duty) { return phase - std::floor(phase) < duty ? 1.0 : -1.0; }

// 32-step triangle (16 levels up, 16 down).
double triangle(double phase) {
    const double step = std::floor((phase - std::floor(phase)) * 32.0);
    const double level = step < 16 ? step : 31 - step;
    return level / 7.5 - 1.0;
}

// Pitch glides exponentially from start_hz to end_hz over the sound's length.
double glide(double t, double length, double start_hz, double end_hz) {
    return start_hz * std::pow(end_hz / start_hz, std::min(1.0, t / length));
}

void ensure(std::vector<double>& out, std::size_t n) { out.resize(std::max(out.size(), n), 0.0); }

void add_chip_lock(std::vector<double>& out, double hz) {
    const std::size_t n = frames(tuning::kChipLockLength);
    ensure(out, n);
    double phase = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / kRate;
        phase += (t < 0.012 ? hz : hz * 0.75) / kRate;  // two quick steps: bip-bup
        out[i] += pulse(phase, 0.5) * quantize(1.0 - t / tuning::kChipLockLength);
    }
}

void add_chip_drop(std::vector<double>& out, double start_hz, double end_hz, double length, Noise& noise) {
    const std::size_t n = frames(length);
    ensure(out, n);
    double phase = 0, hold = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / kRate;
        phase += glide(t, length * 0.5, start_hz, end_hz) / kRate;
        if (i % 24 == 0) hold = noise.next();  // low-rate noise reads as a crunchy hit
        out[i] += triangle(phase) * (t < length * 0.75 ? 1.0 : (length - t) / (length * 0.25)) +
                  0.5 * hold * quantize(1.0 - t / 0.04);
    }
}

void add_chip_clear(std::vector<double>& out, double start_hz, double end_hz, double length, Noise& noise) {
    const std::size_t n = frames(length);
    ensure(out, n);
    double phase = 0, hold = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / kRate;
        // Pitch moves in small steps, like a hardware sweep unit.
        const double stepped_t = std::floor(t / 0.016) * 0.016;
        phase += glide(stepped_t, length * 0.6, start_hz, end_hz) / kRate;
        if (i % 6 == 0) hold = noise.next();
        out[i] += 0.7 * pulse(phase, 0.25) * quantize(1.0 - t / length) + 0.6 * hold * quantize(1.0 - t / (length * 0.5));
    }
}

void add_chip_arpeggio(std::vector<double>& out, double transpose, double tempo, Noise& noise) {
    double total = 0;
    for (const auto& note : tuning::kChipArpeggio) total += note.length_s * tempo;
    const std::size_t n = frames(total);
    ensure(out, n);
    double phase = 0, bass = 0, hold = 0, start = 0;
    std::size_t k = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / kRate;
        while (k + 1 < std::size(tuning::kChipArpeggio) && t >= start + tuning::kChipArpeggio[k].length_s * tempo)
            start += tuning::kChipArpeggio[k++].length_s * tempo;
        const double note_t = t - start, note_len = tuning::kChipArpeggio[k].length_s * tempo;
        phase += tuning::kChipArpeggio[k].hz * transpose / kRate;
        bass += tuning::kChipArpeggioBassHz * transpose / kRate;
        if (i % 10 == 0) hold = noise.next();
        const double lead = pulse(phase, 0.125) * quantize(1.0 - 0.5 * note_t / note_len) * quantize(1.0 - 0.6 * t / total);
        out[i] += 0.6 * lead + 0.5 * triangle(bass) * quantize(1.0 - t / total) + 0.4 * hold * quantize(1.0 - t / 0.12);
    }
}

void add_chip_beat(std::vector<double>& out, double transpose) {
    // Second note rings three times as long as the first.
    const double len = tuning::kChipBeatNoteLength;
    const std::size_t n = frames(len * 4);
    ensure(out, n);
    double phase = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / kRate;
        phase += tuning::kChipBeatHz[t < len ? 0 : 1] * transpose / kRate;
        out[i] += pulse(phase, 0.5) * quantize(t < len ? 1.0 : 1.0 - (t - len) / (len * 3));
    }
}

std::vector<float> finish(std::vector<double>& s, float peak) {
    // Short fade so every sound ends exactly at zero.
    const std::size_t fade = std::min(s.size(), frames(0.01));
    for (std::size_t i = 0; i < fade; ++i) s[s.size() - 1 - i] *= static_cast<double>(i) / static_cast<double>(fade);
    double max = 0;
    for (const double v : s) max = std::max(max, std::abs(v));
    const double gain = max > 0 ? peak / max : 0;
    std::vector<float> out(s.size());
    std::ranges::transform(s, out.begin(), [&](double v) { return static_cast<float>(v * gain); });
    return out;
}

std::vector<float> render_chip(SfxId id, int take) {
    std::vector<double> s;
    Noise noise(static_cast<std::uint64_t>(take) + 100);
    Variation v(id, take);
    switch (id) {
        case SfxId::Lock:
            add_chip_lock(s, tuning::kChipLockHz * v.pitch());
            return finish(s, tuning::kChipLockPeak);
        case SfxId::HardDrop:
            add_chip_drop(s, tuning::kChipDropStartHz * v.pitch(), tuning::kChipDropEndHz * v.pitch(),
                          tuning::kChipDropLength * v.time(), noise);
            return finish(s, tuning::kChipHardDropPeak);
        case SfxId::Clear:
            add_chip_clear(s, tuning::kChipClearStartHz * v.pitch(), tuning::kChipClearEndHz * v.pitch(),
                           tuning::kChipClearLength * v.time(), noise);
            return finish(s, tuning::kChipClearPeak);
        case SfxId::ClearBig:
            add_chip_arpeggio(s, v.pitch(), v.time(), noise);
            return finish(s, tuning::kChipClearBigPeak);
        case SfxId::OnBeat:
            add_chip_beat(s, v.pitch());
            return finish(s, tuning::kChipOnBeatPeak);
        case SfxId::Count: break;
    }
    return {};
}

}  // namespace

std::vector<float> render_sfx(SfxId id, int take, SfxStyle style) {
    if (style == SfxStyle::Chip) return render_chip(id, take);
    std::vector<double> s;
    Noise noise(static_cast<std::uint64_t>(take));
    Variation v(id, take);
    switch (id) {
        case SfxId::Lock:
            add_kick(s, v.kick(tuning::kSfxLock), 1.0, noise);
            return finish(s, tuning::kSfxLockPeak);
        case SfxId::HardDrop:
            add_kick(s, v.kick(tuning::kSfxHardDrop), 1.0, noise);
            return finish(s, tuning::kSfxHardDropPeak);
        case SfxId::Clear:
            add_kick(s, v.kick(tuning::kSfxClear), 1.0, noise);
            add_whoosh(s, v.whoosh(tuning::kSfxClearWhoosh), 1.0, noise);
            return finish(s, tuning::kSfxClearPeak);
        case SfxId::ClearBig:
            add_kick(s, v.kick(tuning::kSfxClearBig), 1.0, noise);
            add_kick(s, v.kick(tuning::kSfxClearBigSub), tuning::kSfxClearBigSubLevel, noise);
            add_whoosh(s, v.whoosh(tuning::kSfxClearBigWhoosh), 1.0, noise);
            return finish(s, tuning::kSfxClearBigPeak);
        case SfxId::OnBeat: {
            const auto p = v.whoosh(tuning::kSfxOnBeat);
            s.resize(frames(p.decay_s * 6), 0.0);
            add_whoosh(s, p, 1.0, noise);
            return finish(s, tuning::kSfxOnBeatPeak);
        }
        case SfxId::Count: break;
    }
    return {};
}

}  // namespace audrop::audio
