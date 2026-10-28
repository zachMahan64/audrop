#include "audio/beat_tracker.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <tuple>
#include <vector>

#include "audio/fft.hpp"

namespace audrop::audio {
namespace {

constexpr double kMinBpm = 50.0;
constexpr double kMaxBpm = 220.0;
constexpr double kPriorBpm = 120.0;
constexpr double kPriorOctaves = 1.0;  // std-dev of the log-normal tempo prior
constexpr double kTightness = 100.0;   // DP penalty for deviating from the period
constexpr int kFluxLag = 2;            // frames
constexpr int kRefineRadius = 3;       // frames searched around each DP beat
constexpr int kRegularizeRadius = 8;           // beats on each side for the local tempo fit
constexpr double kRegularizeOutlier = 0.025;    // seconds; beats further off the fit are ignored
constexpr double kRegularizeAgreement = 0.6;    // fraction of the window that must fit
// The flux peak lands slightly before the perceived attack; measured on synthetic percussive onsets.
constexpr double kOnsetLatency = 0.0111;  // seconds
// If off-beat midpoints are nearly as strong as the beats, the tracker locked onto half tempo.
constexpr double kDoubleTempoRatio = 0.75;
constexpr std::size_t kBands = 40;
constexpr double kBandLowHz = 30.0;
constexpr double kBandHighHz = 11000.0;

bool cancelled(const std::atomic<bool>* c) { return c && c->load(std::memory_order_relaxed); }

std::size_t next_pow2(std::size_t v) {
    std::size_t p = 1;
    while (p < v) p <<= 1;
    return p;
}

double stddev(const std::vector<double>& v) {
    if (v.size() < 2) return 1.0;
    const double mean = std::accumulate(v.begin(), v.end(), 0.0) / static_cast<double>(v.size());
    double acc = 0;
    for (double x : v) acc += (x - mean) * (x - mean);
    const double sd = std::sqrt(acc / static_cast<double>(v.size() - 1));
    return sd > 1e-12 ? sd : 1.0;
}

// Fractional offset of a peak from three samples around it.
double parabolic(double a, double b, double c) {
    const double denom = a - 2 * b + c;
    if (std::abs(denom) < 1e-12) return 0;
    return std::clamp(0.5 * (a - c) / denom, -0.5, 0.5);
}

}  // namespace

sync::BeatGrid track_beats(std::span<const float> mono, int sample_rate, const std::atomic<bool>* cancel) {
    sync::BeatGrid grid;
    const double sr = sample_rate;
    if (sample_rate <= 0 || mono.size() < static_cast<std::size_t>(sr * 4)) return grid;

    // --- 1. onset strength envelope (log-magnitude spectral flux) -------------
    const std::size_t win = next_pow2(static_cast<std::size_t>(sr * 0.046));
    const std::size_t hop = std::max<std::size_t>(1, static_cast<std::size_t>(std::lround(sr * 0.0058)));
    const double frame_rate = sr / static_cast<double>(hop);
    const std::size_t bins = win / 2 + 1;
    const std::size_t max_bin = std::min(bins, static_cast<std::size_t>(11000.0 / sr * static_cast<double>(win)));
    const std::size_t frames = (mono.size() - win) / hop + 1;

    // Log-spaced bands so every octave weighs the same; per-bin flux would let hi-hats (which
    // span hundreds of bins) drown out kicks and snares and lock onto double tempo.
    std::vector<std::size_t> band_edges{1};
    for (std::size_t b = 1; b <= kBands; ++b) {
        const double hz = kBandLowHz * std::pow(kBandHighHz / kBandLowHz, static_cast<double>(b) / kBands);
        const auto edge = std::min(max_bin, static_cast<std::size_t>(hz / sr * static_cast<double>(win)));
        if (edge > band_edges.back()) band_edges.push_back(edge);
    }
    const std::size_t bands = band_edges.size() - 1;

    RealFft fft(win);
    std::vector<float> mag(bins);
    std::vector<std::vector<float>> history(kFluxLag + 1, std::vector<float>(bands, 0.0f));
    std::vector<double> onset(frames, 0.0);
    const float norm = 2.0f / static_cast<float>(win);

    for (std::size_t f = 0; f < frames; ++f) {
        if ((f & 1023) == 0 && cancelled(cancel)) return {};
        fft.magnitudes(mono.subspan(f * hop, win), mag);
        auto& cur = history[f % history.size()];
        const auto& prev = history[(f + history.size() - kFluxLag) % history.size()];
        double flux = 0;
        for (std::size_t b = 0; b < bands; ++b) {
            float acc = 0;
            for (std::size_t k = band_edges[b]; k < band_edges[b + 1]; ++k) acc += mag[k];
            cur[b] = std::log1p(100.0f * acc * norm / static_cast<float>(band_edges[b + 1] - band_edges[b]));
            if (f >= static_cast<std::size_t>(kFluxLag)) flux += std::max(0.0f, cur[b] - prev[b]);
        }
        onset[f] = flux;
    }

    // Remove slow trends (crescendos etc.) with a one-second moving average, then normalise.
    {
        const auto radius = static_cast<std::ptrdiff_t>(frame_rate * 0.5);
        std::vector<double> prefix(frames + 1, 0.0);
        for (std::size_t i = 0; i < frames; ++i) prefix[i + 1] = prefix[i] + onset[i];
        std::vector<double> hp(frames);
        for (std::size_t i = 0; i < frames; ++i) {
            const auto lo = static_cast<std::size_t>(std::max<std::ptrdiff_t>(0, static_cast<std::ptrdiff_t>(i) - radius));
            const auto hi = std::min(frames, i + static_cast<std::size_t>(radius) + 1);
            const double mean = (prefix[hi] - prefix[lo]) / static_cast<double>(hi - lo);
            hp[i] = std::max(0.0, onset[i] - mean);
        }
        const double sd = stddev(hp);
        for (std::size_t i = 0; i < frames; ++i) onset[i] = hp[i] / sd;
    }
    if (cancelled(cancel)) return {};

    // --- 2. tempo: weighted autocorrelation -----------------------------------
    const auto lag_min = static_cast<std::size_t>(std::floor(60.0 / kMaxBpm * frame_rate));
    const auto lag_max = std::min(frames / 2, static_cast<std::size_t>(std::ceil(60.0 / kMinBpm * frame_rate)));
    if (lag_max <= lag_min + 2) return grid;

    const double onset_mean = std::accumulate(onset.begin(), onset.end(), 0.0) / static_cast<double>(frames);
    std::vector<double> centered(frames);
    for (std::size_t i = 0; i < frames; ++i) centered[i] = onset[i] - onset_mean;

    std::vector<double> ac(lag_max + 2, 0.0);
    double ac0 = 0;
    for (std::size_t i = 0; i < frames; ++i) ac0 += centered[i] * centered[i];
    ac0 /= static_cast<double>(frames);
    for (std::size_t lag = lag_min - 1; lag <= lag_max + 1 && lag < frames; ++lag) {
        double acc = 0;
        for (std::size_t i = 0; i + lag < frames; ++i) acc += centered[i] * centered[i + lag];
        ac[lag] = acc / static_cast<double>(frames - lag);
    }
    if (cancelled(cancel)) return {};

    std::size_t best_lag = 0;
    double best_score = -1e300;
    for (std::size_t lag = lag_min; lag <= lag_max; ++lag) {
        const double bpm = 60.0 * frame_rate / static_cast<double>(lag);
        const double z = std::log2(bpm / kPriorBpm) / kPriorOctaves;
        const double score = ac[lag] * std::exp(-0.5 * z * z);
        if (score > best_score) {
            best_score = score;
            best_lag = lag;
        }
    }
    if (best_lag == 0 || ac[best_lag] <= 0) return grid;
    const double period = static_cast<double>(best_lag) + parabolic(ac[best_lag - 1], ac[best_lag], ac[best_lag + 1]);

    // --- 3. dynamic-programming beat placement --------------------------------
    std::vector<double> local(frames, 0.0);
    {
        const auto half = static_cast<std::ptrdiff_t>(std::ceil(period));
        std::vector<double> kernel(static_cast<std::size_t>(2 * half + 1));
        for (std::ptrdiff_t k = -half; k <= half; ++k) {
            const double x = static_cast<double>(k) * 32.0 / period;
            kernel[static_cast<std::size_t>(k + half)] = std::exp(-0.5 * x * x);
        }
        for (std::size_t i = 0; i < frames; ++i) {
            double acc = 0;
            for (std::ptrdiff_t k = -half; k <= half; ++k) {
                const auto j = static_cast<std::ptrdiff_t>(i) + k;
                if (j >= 0 && j < static_cast<std::ptrdiff_t>(frames))
                    acc += onset[static_cast<std::size_t>(j)] * kernel[static_cast<std::size_t>(k + half)];
            }
            local[i] = acc;
        }
    }

    const auto back_min = static_cast<std::ptrdiff_t>(std::lround(period / 2));
    const auto back_max = static_cast<std::ptrdiff_t>(std::lround(period * 2));
    std::vector<double> txcost(static_cast<std::size_t>(back_max + 1), 0.0);
    for (std::ptrdiff_t d = back_min; d <= back_max; ++d) {
        const double l = std::log(static_cast<double>(d) / period);
        txcost[static_cast<std::size_t>(d)] = -kTightness * l * l;
    }

    std::vector<double> cum(frames, 0.0);
    std::vector<std::ptrdiff_t> backlink(frames, -1);
    const double local_max = *std::max_element(local.begin(), local.end());
    bool first_beat = true;
    for (std::size_t i = 0; i < frames; ++i) {
        if ((i & 4095) == 0 && cancelled(cancel)) return {};
        double best = -1e300;
        std::ptrdiff_t arg = -1;
        for (std::ptrdiff_t d = back_min; d <= back_max; ++d) {
            const auto j = static_cast<std::ptrdiff_t>(i) - d;
            if (j < 0) break;
            const double s = cum[static_cast<std::size_t>(j)] + txcost[static_cast<std::size_t>(d)];
            if (s > best) {
                best = s;
                arg = j;
            }
        }
        // Don't link the very first beats into silence.
        if (first_beat && local[i] < 0.01 * local_max) arg = -1;
        else first_beat = false;
        cum[i] = local[i] + (arg >= 0 ? best : 0.0);
        backlink[i] = arg;
    }

    // Last beat: the last local maximum of the cumulative score that is reasonably strong.
    std::vector<std::size_t> peaks;
    for (std::size_t i = 1; i + 1 < frames; ++i)
        if (cum[i] > cum[i - 1] && cum[i] >= cum[i + 1]) peaks.push_back(i);
    if (peaks.empty()) return grid;
    std::vector<double> peak_vals;
    for (auto p : peaks) peak_vals.push_back(cum[p]);
    std::nth_element(peak_vals.begin(), peak_vals.begin() + static_cast<std::ptrdiff_t>(peak_vals.size() / 2), peak_vals.end());
    const double median = peak_vals[peak_vals.size() / 2];
    std::size_t last = peaks.back();
    for (auto it = peaks.rbegin(); it != peaks.rend(); ++it) {
        if (cum[*it] >= 0.5 * median) {
            last = *it;
            break;
        }
    }

    std::vector<std::size_t> beat_frames;
    for (std::ptrdiff_t b = static_cast<std::ptrdiff_t>(last); b >= 0; b = backlink[static_cast<std::size_t>(b)])
        beat_frames.push_back(static_cast<std::size_t>(b));
    std::ranges::reverse(beat_frames);

    // Trim weak beats at both ends (silence / fades).
    if (beat_frames.size() > 4) {
        double rms = 0;
        for (auto f : beat_frames) rms += local[f] * local[f];
        rms = std::sqrt(rms / static_cast<double>(beat_frames.size()));
        const double thresh = 0.5 * rms;
        std::size_t lo = 0, hi = beat_frames.size();
        while (lo < hi && local[beat_frames[lo]] < thresh) ++lo;
        while (hi > lo && local[beat_frames[hi - 1]] < thresh) --hi;
        beat_frames = {beat_frames.begin() + static_cast<std::ptrdiff_t>(lo), beat_frames.begin() + static_cast<std::ptrdiff_t>(hi)};
    }
    if (beat_frames.size() < 4) return grid;

    // --- 4. refinement ---------------------------------------------------------
    // Snap each beat to the nearest onset peak with sub-frame precision.
    std::vector<double> times;
    times.reserve(beat_frames.size());
    const double center = static_cast<double>(win) / 2.0;
    for (auto f : beat_frames) {
        std::size_t best = f;
        for (std::ptrdiff_t k = -kRefineRadius; k <= kRefineRadius; ++k) {
            const auto j = static_cast<std::ptrdiff_t>(f) + k;
            if (j > 0 && j + 1 < static_cast<std::ptrdiff_t>(frames) && onset[static_cast<std::size_t>(j)] > onset[best])
                best = static_cast<std::size_t>(j);
        }
        double frac = static_cast<double>(best);
        if (best > 0 && best + 1 < frames) frac += parabolic(onset[best - 1], onset[best], onset[best + 1]);
        // The flux at frame f compares against frame f - lag, so the onset sits between the two.
        const double sample = (frac - kFluxLag / 2.0) * static_cast<double>(hop) + center;
        times.push_back(sample / sr + kOnsetLatency);
    }

    // Robust local linear fit over neighbouring beats: removes frame jitter and pulls single
    // beats that the DP dragged onto a syncopated hit back onto the grid, while still
    // following genuine tempo drift. Outliers are rejected iteratively; if too few beats
    // agree (a real tempo change or a break), the detected time is kept.
    std::vector<double> regular(times.size());
    std::vector<std::size_t> inliers;
    for (std::size_t i = 0; i < times.size(); ++i) {
        const std::size_t lo = i >= static_cast<std::size_t>(kRegularizeRadius) ? i - kRegularizeRadius : 0;
        const std::size_t hi = std::min(times.size() - 1, i + kRegularizeRadius);
        inliers.clear();
        for (std::size_t k = lo; k <= hi; ++k) inliers.push_back(k);

        double slope = 0, icpt = 0;
        bool ok = false;
        for (int iter = 0; iter < 3; ++iter) {
            if (inliers.size() < 4) break;
            const double n = static_cast<double>(inliers.size());
            double sx = 0, sy = 0, sxx = 0, sxy = 0;
            for (auto k : inliers) {
                const double x = static_cast<double>(k);
                sx += x;
                sy += times[k];
                sxx += x * x;
                sxy += x * times[k];
            }
            const double denom = n * sxx - sx * sx;
            if (denom <= 0) break;
            slope = (n * sxy - sx * sy) / denom;
            icpt = (sy - slope * sx) / n;
            ok = true;
            inliers.clear();
            for (std::size_t k = lo; k <= hi; ++k)
                if (std::abs(times[k] - (icpt + slope * static_cast<double>(k))) < kRegularizeOutlier) inliers.push_back(k);
        }
        const auto window = static_cast<double>(hi - lo + 1);
        const bool agreed = ok && static_cast<double>(inliers.size()) >= kRegularizeAgreement * window;
        regular[i] = agreed ? icpt + slope * static_cast<double>(i) : times[i];
    }

    // --- 5. confidence ---------------------------------------------------------
    // Pulse clarity (autocorrelation peak relative to signal energy) combined with how much
    // stronger the onsets are on the beats than halfway between them.
    auto on_off = [&](const std::vector<std::size_t>& bf) {
        double on = 0, off = 0;
        for (std::size_t i = 0; i < bf.size(); ++i) {
            on += onset[bf[i]];
            if (i + 1 < bf.size()) off += onset[(bf[i] + bf[i + 1]) / 2];
        }
        return std::pair{on / static_cast<double>(bf.size()), off / static_cast<double>(bf.size() - 1)};
    };
    auto [on, off] = on_off(beat_frames);

    if (off > kDoubleTempoRatio * on) {
        std::vector<double> doubled;
        std::vector<std::size_t> doubled_frames;
        for (std::size_t i = 0; i < regular.size(); ++i) {
            doubled.push_back(regular[i]);
            doubled_frames.push_back(beat_frames[i]);
            if (i + 1 < regular.size()) {
                doubled.push_back((regular[i] + regular[i + 1]) / 2.0);
                doubled_frames.push_back((beat_frames[i] + beat_frames[i + 1]) / 2);
            }
        }
        regular = std::move(doubled);
        beat_frames = std::move(doubled_frames);
        std::tie(on, off) = on_off(beat_frames);
    }

    const double clarity = ac0 > 0 ? std::clamp(2.0 * ac[best_lag] / ac0, 0.0, 1.0) : 0.0;
    const double contrast = std::clamp((on - off) / (on + off + 1e-9), 0.0, 1.0);

    std::vector<double> diffs;
    for (std::size_t i = 1; i < regular.size(); ++i) diffs.push_back(regular[i] - regular[i - 1]);
    std::nth_element(diffs.begin(), diffs.begin() + static_cast<std::ptrdiff_t>(diffs.size() / 2), diffs.end());

    grid.beats = std::move(regular);
    grid.bpm = 60.0 / diffs[diffs.size() / 2];
    grid.confidence = static_cast<float>(std::min(clarity, contrast));
    return grid;
}

}  // namespace audrop::audio
