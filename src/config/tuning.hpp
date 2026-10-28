#pragma once
// Every gameplay, timing and effects constant lives here so they can be tuned in one place.

#include <array>
#include <cstdint>
#include <string_view>

namespace audrop::tuning {

// ---------------------------------------------------------------------------
// Beat sync tiers
//
// A line clear is judged by its distance to the nearest beat. A tier is hit when
//     |offset| <= max(beat_fraction * beat_length, min_ms)
// Tiers are checked from tightest (last) to loosest (first).
// `multiplier` scales the line-clear score; `fx_intensity` (0..1) drives how grand
// the clear effects are. `rgba` is the judgment colour.
// ---------------------------------------------------------------------------
struct SyncTier {
    std::string_view name;
    double beat_fraction;  // ± fraction of one beat
    double min_ms;         // floor so fast tempos stay fair
    double multiplier;
    float fx_intensity;
    std::uint32_t rgba;
};

inline constexpr std::array kSyncTiers{
    SyncTier{"OK",      1.0 / 8.0,  40.0, 1.10, 0.20f, 0x8fd3ffff},
    SyncTier{"GOOD",    1.0 / 16.0, 25.0, 1.25, 0.40f, 0x7dffb2ff},
    SyncTier{"GREAT",   1.0 / 24.0, 18.0, 1.50, 0.60f, 0xffe66bff},
    SyncTier{"AMAZING", 1.0 / 32.0, 14.0, 1.75, 0.80f, 0xff9a4dff},
    SyncTier{"PERFECT", 1.0 / 48.0, 10.0, 2.00, 1.00f, 0xff5ce1ff},
};
inline constexpr int kSyncTierCount = static_cast<int>(kSyncTiers.size());

// Below this tracker confidence the beat grid is considered unreliable and sync judging is off.
inline constexpr float kMinBeatConfidence = 0.15f;

// ---------------------------------------------------------------------------
// Board / rules
// ---------------------------------------------------------------------------
inline constexpr int kBoardWidth = 10;
inline constexpr int kBoardHeight = 40;  // total rows including the hidden buffer
inline constexpr int kVisibleRows = 20;
inline constexpr int kNextPreviewCount = 5;

inline constexpr double kLockDelay = 0.5;        // seconds
inline constexpr int kMaxLockResets = 15;        // move/rotate resets while grounded
inline constexpr int kLinesPerLevel = 10;

// Handling defaults (user-adjustable in settings).
inline constexpr double kDefaultDasMs = 150.0;
inline constexpr double kDefaultArrMs = 33.0;
inline constexpr double kDefaultSoftDropFactor = 20.0;  // multiple of gravity; >= 40 is "instant"
inline constexpr double kInstantSoftDropFactor = 40.0;

// Mode parameters.
inline constexpr int kMarathonLines = 150;
inline constexpr int kMarathonMaxLevel = 15;
inline constexpr int kSprintLines = 40;
inline constexpr double kUltraSeconds = 120.0;
inline constexpr int kEndlessMaxLevel = 20;

// ---------------------------------------------------------------------------
// Scoring (guideline values, multiplied by level unless noted)
// ---------------------------------------------------------------------------
inline constexpr std::array<int, 5> kLineClearScore{0, 100, 300, 500, 800};
inline constexpr std::array<int, 4> kTSpinScore{400, 800, 1200, 1600};
inline constexpr std::array<int, 3> kTSpinMiniScore{100, 200, 400};
inline constexpr std::array<int, 5> kAllClearScore{0, 800, 1200, 1800, 2000};
inline constexpr int kB2BTetrisAllClearScore = 3200;
inline constexpr double kBackToBackMultiplier = 1.5;
inline constexpr int kComboScore = 50;
inline constexpr int kSoftDropPerCell = 1;  // not level-multiplied
inline constexpr int kHardDropPerCell = 2;  // not level-multiplied

// ---------------------------------------------------------------------------
// Effects
// ---------------------------------------------------------------------------
// Clear-size factor that multiplies a tier's fx_intensity.
inline constexpr std::array<float, 5> kClearSizeFx{0.0f, 0.55f, 0.7f, 0.85f, 1.0f};
inline constexpr float kTSpinFxBonus = 0.15f;
inline constexpr float kAllClearFxBonus = 0.3f;
inline constexpr float kOffBeatFx = 0.08f;  // effect intensity for clears that miss every tier

// Thresholds on the final effect intensity (0..1.3) at which grander effects switch on.
inline constexpr float kFxRowFlash = 0.0f;
inline constexpr float kFxBoardPulse = 0.15f;
inline constexpr float kFxShockwave = 0.45f;
inline constexpr float kFxScreenShake = 0.45f;
inline constexpr float kFxChromatic = 0.65f;
inline constexpr float kFxSpill = 0.65f;  // particles escape the board
inline constexpr float kFxFullFlash = 0.85f;

inline constexpr int kMaxParticles = 12000;
inline constexpr int kParticlesPerCellBase = 6;
inline constexpr int kParticlesPerCellMax = 40;

// ---------------------------------------------------------------------------
// Sound effects (synthesized at startup, see audio/sfx_synth.cpp)
//
// Every sound is normalized to its `*Peak` level (0..1) before the SFX volume setting
// applies, so the peaks set the relative loudness of the sounds.
// ---------------------------------------------------------------------------
struct KickParams {
    double start_hz;  // pitch at the attack...
    double end_hz;    // ...gliding down to this
    double glide_s;   // pitch glide time constant
    double decay_s;   // amplitude decay time constant
    double length_s;
    double click;     // noise transient level relative to the body
    double drive;     // tanh saturation (1 = clean, higher = grittier)
};
// Filtered noise layered on clears; the filter cutoff sweeps from start_hz to end_hz.
struct WhooshParams {
    double start_hz;
    double end_hz;
    double decay_s;
    double level;     // relative to the kick body
    bool highpass;    // false = low-passed rumble, true = bright hiss
};

inline constexpr KickParams kSfxLock{120, 62, 0.018, 0.045, 0.16, 0.12, 1.0};
inline constexpr KickParams kSfxHardDrop{165, 46, 0.028, 0.11, 0.38, 0.35, 1.6};
inline constexpr KickParams kSfxClear{150, 42, 0.045, 0.30, 0.95, 0.30, 1.8};
inline constexpr WhooshParams kSfxClearWhoosh{3200, 250, 0.22, 0.35, false};
// Tetris, T-spin and all clear: heavier kick plus a slower sub layer underneath.
inline constexpr KickParams kSfxClearBig{190, 36, 0.07, 0.55, 1.7, 0.40, 2.6};
inline constexpr KickParams kSfxClearBigSub{80, 30, 0.12, 0.8, 1.7, 0.0, 1.0};
inline constexpr double kSfxClearBigSubLevel = 0.6;
inline constexpr WhooshParams kSfxClearBigWhoosh{5500, 180, 0.45, 0.45, false};
// Layered on any clear that lands in a sync tier, scaled by the tier's fx_intensity.
inline constexpr WhooshParams kSfxOnBeat{9000, 2500, 0.16, 1.0, true};

// Chip style (Retro theme): 8-bit console voices. Pulse duty is the high fraction of each
// cycle; envelopes and the triangle are quantized to 16 levels like the original hardware.
struct ChipNote {
    double hz;
    double length_s;
};
inline constexpr double kChipLockHz = 880;          // short square blip
inline constexpr double kChipLockLength = 0.07;
inline constexpr double kChipDropStartHz = 330;     // triangle thump gliding down...
inline constexpr double kChipDropEndHz = 55;        // ...to here
inline constexpr double kChipDropLength = 0.2;
inline constexpr double kChipClearStartHz = 300;    // pulse sweep up over the noise burst
inline constexpr double kChipClearEndHz = 1500;
inline constexpr double kChipClearLength = 0.4;
inline constexpr ChipNote kChipArpeggio[] = {       // big clear: rising major arpeggio
    {523.25, 0.055}, {659.26, 0.055}, {783.99, 0.055}, {1046.5, 0.055}, {1318.5, 0.055}, {1568.0, 0.3},
};
inline constexpr double kChipArpeggioBassHz = 130.81;
inline constexpr double kChipBeatHz[] = {1567.98, 2093.0};  // on-beat two-note chime
inline constexpr double kChipBeatNoteLength = 0.045;
// Pulse waves in the 1-2 kHz range sound far louder than kicks at the same peak, so the chip
// voices have their own (lower) peaks.
inline constexpr float kChipLockPeak = 0.12f;
inline constexpr float kChipHardDropPeak = 0.6f;
inline constexpr float kChipClearPeak = 0.42f;
inline constexpr float kChipClearBigPeak = 0.5f;
inline constexpr float kChipOnBeatPeak = 0.09f;

inline constexpr float kSfxLockPeak = 0.35f;
inline constexpr float kSfxHardDropPeak = 0.8f;
inline constexpr float kSfxClearPeak = 0.85f;
inline constexpr float kSfxClearBigPeak = 0.95f;
inline constexpr float kSfxOnBeatPeak = 0.25f;

inline constexpr float kSfxHardDropMinGain = 0.75f;  // 0-row drop; rises to 1 at kSfxHardDropFullRows
inline constexpr int kSfxHardDropFullRows = 20;
inline constexpr float kSfxOnBeatMinGain = 0.5f;     // on-beat gain = min + (1 - min) * tier fx_intensity
inline constexpr int kSfxVoices = 4;                 // overlapping copies of each take

// Variety: each sound is rendered as several takes with randomized parameters (take 0 is
// exactly the values above), and every playback adds a little pitch and gain jitter.
// Amounts are ± fractions.
inline constexpr int kSfxTakes = 6;
inline constexpr double kSfxVaryPitch = 0.1;    // kick start / end pitch
inline constexpr double kSfxVaryTime = 0.2;     // glide and decay times
inline constexpr double kSfxVaryClick = 0.45;   // attack click level
inline constexpr double kSfxVaryFilter = 0.3;   // whoosh filter sweep
inline constexpr float kSfxPlayPitch = 0.045f;  // playback rate per play
inline constexpr float kSfxPlayGain = 0.18f;    // volume per play

// ---------------------------------------------------------------------------
// Loop timing
// ---------------------------------------------------------------------------
inline constexpr double kLogicHz = 1000.0;  // input polling / game stepping rate

}  // namespace audrop::tuning
