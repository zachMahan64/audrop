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
// Loop timing
// ---------------------------------------------------------------------------
inline constexpr double kLogicHz = 1000.0;  // input polling / game stepping rate

}  // namespace audrop::tuning
