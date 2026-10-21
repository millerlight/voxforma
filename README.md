# voxforma

A JUCE-based VST3 formant synthesizer plugin.

`voxforma` is a vocal-formant synthesizer: it takes an oscillator-based source signal and
routes it through a bank of bandpass filters tuned to the formants of the human voice.
The vowel character can be steered manually via F1/F2/F3, modulated by an LFO, or picked
from a set of four custom vowel banks.

## Features

- Formant synthesis with three parallel bandpass stages (F1, F2, F3)
- Four independent custom vowel banks, each holding its own formant set
- LFO modulation of the vowel coordinates
- Master section with a Dry/Wet blend, a gain stage and a soft limiter
- True bypass (unity-gain passthrough)

## Requirements

| Tool | Version | Needed for |
|------|---------|------------|
| CMake | 3.22 or newer | all builds |
| A C++17 compiler | — | all builds |
| MinGW-w64 GCC | `x86_64-w64-mingw32-gcc-posix` | Linux → Windows cross-build |
| Visual Studio 2022 | — | native Windows build |

JUCE is vendored as a git submodule and is pinned to a specific upstream commit.

## Getting started

```bash
git clone --recurse-submodules <repo-url>
cd voxforma

# Apply the MinGW compatibility patches to the JUCE submodule. Required for the
# Linux -> Windows cross-build; harmless on other platforms.
./patches/apply-juce-patches.sh

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

The plugin bundle is written to
`build/voxforma_artefacts/Release/VST3/voxforma.vst3`.

## Building on Windows

```bat
build-windows.bat
```

This configures a Visual Studio 2022 build in `build-win/` and produces
`build-win\voxforma_artefacts\Release\VST3\voxforma.vst3`.

## Cross-compiling for Windows from Linux

```bash
cmake -S . -B build-win \
      -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw.cmake \
      -DCMAKE_BUILD_TYPE=Release
cmake --build build-win --parallel
```

JUCE upstream only supports MSVC on Windows, so a small set of compatibility patches is
applied to the submodule. They live in `patches/juce-mingw/` and are applied by
`patches/apply-juce-patches.sh`:

| Patch | Purpose |
|-------|---------|
| `0001-target-platform-allow-mingw.patch` | Lifts the hard `#error` on MinGW and enables the 64-bit code path |
| `0002-system-stats-cpuid-mingw.patch` | Uses the GCC builtin for CPUID instead of the MSVC intrinsic |
| `0003-directx-uuidof-mingw.patch` | Uses the explicit interface UUID, since `__uuidof(type)` is MSVC-only |
| `0004-direct2d-rendering-params-mingw.patch` | Skips the DirectWrite rendering-params call, whose overloads differ under MinGW |
| `0005-windowing-renderer-mingw.patch` | Fixes the renderer flag, which relied on a Wine-specific MSVC intrinsic |

The script also supports `--check` (report state) and `--reverse` (undo the patches).

## VST3 manifest

JUCE normally generates the VST3 `moduleinfo.json` by running a helper binary that is
compiled for the *target* platform. That binary cannot be executed while cross-compiling,
so JUCE writes an empty `{}` placeholder, which is not a valid module description.

`cmake/vst3_moduleinfo.cmake` replaces it. It reproduces the helper's output exactly —
including the platform-dependent byte order of the VST3 class IDs — and is invoked as a
`POST_BUILD` step on the `voxforma_VST3` target.

## Tests

The `tests/` directory contains four smoke tests that are built alongside the plugin:

| Test | Checks |
|------|--------|
| `engine_smoke` | Signal finiteness, dry/wet energy routing, master section, bypass |
| `editor_smoke` | Editor instantiation and lifetime |
| `vowels_smoke` | Custom vowel bank handling and pending-set semantics |
| `vst3_smoke` | Plugin loading through the VST3 host wrapper, parameter count |

```bash
./build/engine_smoke
./build/editor_smoke
./build/vowels_smoke
./build/vst3_smoke
```

All four print `PASS` and exit with status 0.

## Layout

```
CMakeLists.txt              Plugin target, manifest generation, test targets
build-windows.bat           Native Windows build
cmake/
  toolchain-mingw.cmake     MinGW-w64 cross-compilation toolchain
  vst3_moduleinfo.cmake     VST3 manifest generator
  juce_win_mingw_compat.h   Forced-include compatibility header for MinGW
patches/
  apply-juce-patches.sh     Applies the JUCE MinGW patches
  juce-mingw/*.patch        The patches themselves
Source/
  dsp/                      Formant engine, LFO, vowel bank
  ui/                       Visualizer, LFO section, look and feel
  Params.h                  Parameter IDs and ranges
  PluginProcessor.*         Audio processing
  PluginEditor.*            User interface
tests/                      Smoke tests
JUCE/                       Git submodule (upstream JUCE)
```

## License

See [LICENSE](LICENSE).
