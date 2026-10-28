# audrop

A falling-block puzzle game that plays your own music, puts a projectM (Milkdrop) visualizer
behind a frosted glass board, and rewards line clears that land on the beat.

## Building

Requires CMake 3.25+, a C++23 compiler (GCC 14+, Clang 17+, Apple Clang 16+) and an
OpenGL 3.3 capable GPU. raylib, libprojectM, nativefiledialog-extended, nlohmann/json,
doctest and the projectM preset/texture packs are downloaded by CMake on first configure.

**Fedora**

```sh
sudo dnf install cmake ninja-build gcc-c++ libX11-devel libXrandr-devel libXinerama-devel \
    libXcursor-devel libXi-devel mesa-libGL-devel dbus-devel
```

**Debian / Ubuntu**

```sh
sudo apt install cmake ninja-build g++ libx11-dev libxrandr-dev libxinerama-dev \
    libxcursor-dev libxi-dev libgl-dev libdbus-1-dev
```

**macOS**: Xcode command line tools and `brew install cmake ninja`.

```sh
cmake -S . -B build -G Ninja
cmake --build build
./build/audrop
ctest --test-dir build   # unit tests
```

Build options:

| Option | Default | |
|---|---|---|
| `AUDROP_WITH_PROJECTM` | ON | projectM visualizer (falls back to built-in shaders when OFF) |
| `AUDROP_WITH_NFD` | ON | native file dialogs; on Linux this needs dbus headers, otherwise zenity/kdialog are used |
| `AUDROP_FETCH_PRESETS` | ON | download the cream-of-the-crop presets (~23 MB) and textures |
| `AUDROP_BUILD_TESTS` | ON | doctest unit tests |

## Playing

Drag music files or folders onto the window, or use **Playlist → Add files / Add folder**.
mp3, flac, wav, ogg and qoa are supported. The playlist loops (sequential or shuffled) and keeps
playing across games.

| Key | Action |
|---|---|
| ← → | move |
| ↓ | soft drop |
| ↑ / Space | hard drop |
| X | rotate clockwise |
| Z / Ctrl | rotate counter-clockwise |
| A | rotate 180° (enable in settings) |
| Shift / C | hold |
| Esc / P / F1 | pause |
| `[` `]` | previous / next track (menus) |
| Tab | next visual |
| F8 | ban the current visual forever |
| F11 | fullscreen |

Gamepads work too (d-pad, A/B rotate, Y 180°, bumpers hold, Start pause).

Modes: **Marathon** (150 lines, 15 levels), **Sprint** (40 lines), **Ultra** (2 minutes),
**Endless**. Rules follow the modern guideline: SRS with wall kicks, 7-bag, hold, 5-piece
preview, 0.5 s lock delay with 15 move resets, T-spin/mini detection, back-to-back, combos and
all clears.

### Beat sync

Each track is beat-tracked in the background the first time it plays (results are cached).
A clear is judged by how far its lock was from the nearest beat:

| Tier | Window (± of a beat) | Floor | Score × |
|---|---|---|---|
| OK | 1/8 | 40 ms | 1.10 |
| GOOD | 1/16 | 25 ms | 1.25 |
| GREAT | 1/24 | 18 ms | 1.50 |
| AMAZING | 1/32 | 14 ms | 1.75 |
| PERFECT | 1/48 | 10 ms | 2.00 |

The tiers, multipliers and effect intensities live in `src/config/tuning.hpp`. Closer hits
trigger grander effects: particle bursts, then shockwaves and screen shake, then chromatic
aberration, full-screen flashes and (for PERFECT tetrises / T-spins / all clears) a hard cut to
a new visual.

- **Settings → Calibrate audio offset** measures your audio/display latency by tapping along to
  a click track. The results screen tells you if you tend to clear early or late.
- **Pause → Beat offset / Tempo** fixes a track whose grid is slightly off or detected at
  half/double speed. Fixes are saved per track.
- The lane under the board shows upcoming beats scrolling into the centre line, with the tier
  windows shaded.

## Files

Everything is stored per user:

- Linux: `$XDG_DATA_HOME/audrop` (default `~/.local/share/audrop`)
- macOS: `~/Library/Application Support/audrop`
- Windows: `%APPDATA%\audrop`

`settings.json`, `scores.json` (top 10 per mode with full stat sheets, plus lifetime totals),
`playlist.json`, `preset_blocklist.txt`, and `beats/` (beat-grid cache).
Put your own `.milk` presets in `presets/` there to use them instead of the bundled pack.

## Environment variables

- `AUDROP_VISUALIZER=builtin`: skip projectM and use the built-in shader scenes.
- Developer hooks: `AUDROP_DEBUG_MODE=marathon|sprint|ultra|endless`, `AUDROP_DEBUG_BOT=1|drop`,
  `AUDROP_DEBUG_SCREEN=playlist|settings|scores|modes|calibration`,
  `AUDROP_DEBUG_SHOT=file.png` with `AUDROP_DEBUG_SHOT_AT=<seconds>` or
  `AUDROP_DEBUG_SHOT_CLEAR=<n>` (see `src/app/debug_hooks.cpp`).

## Licenses

audrop links libprojectM (LGPL-2.1) as a shared library. raylib and nativefiledialog-extended are
zlib, nlohmann/json is MIT, the Rajdhani, Nunito, Selawik and Press Start 2P fonts are SIL OFL 1.1 (`assets/fonts/*-OFL.txt`), and the
projectM preset packs carry their authors' licenses.
