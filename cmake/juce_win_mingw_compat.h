//
//  juce_win_mingw_compat.h
//
//  Compatibility layer that lets JUCE 9 compile for Windows x86_64 with
//  MinGW-w64 GCC.
//
//  JUCE upstream deliberately rejects MinGW in juce_TargetPlatform.h and only
//  enables the 64-bit code path for MSVC, which leaves pointer_sized_int at 32
//  bits and produces pointer-truncation errors throughout the framework.
//
//  This header is injected into every JUCE translation unit as a forced include
//  (-include) and is therefore intentionally limited to preprocessor directives.
//

#ifndef VOXFORMA_MINGW_COMPAT_H
#define VOXFORMA_MINGW_COMPAT_H

/* JUCE_TargetPlatform.h rejects MinGW unless this macro is set. The define has
   to be visible before that header is parsed, which is why it lives in this
   forced-include file. */
#define JUCE_MINGW 1

/* Make sure the 64-bit branch of JUCE_TargetPlatform.h applies. */
#ifndef _WIN64
 #define _WIN64 1
#endif

/* Enable the C99 limit/format macros and math constants when the CRT headers are
   pulled in later on. */
#ifndef __STDC_LIMIT_MACROS
 #define __STDC_LIMIT_MACROS 1
#endif
#ifndef __STDC_FORMAT_MACROS
 #define __STDC_FORMAT_MACROS 1
#endif
#ifndef _USE_MATH_DEFINES
 #define _USE_MATH_DEFINES 1
#endif
#ifndef NOMINMAX
 #define NOMINMAX 1
#endif

/* Turn off the Windows backends whose headers in MinGW-w64 are incomplete.
   JUCE falls back to GDI rendering, which is pixel-identical for this
   plugin's custom-drawn UI, and to the legacy accessibility layer. */
#ifndef JUCE_USE_DIRECT2D
 #define JUCE_USE_DIRECT2D 0
#endif

/* Provide the handful of constants that the MinGW-w64 headers omit but that
   JUCE's Direct2D and UI Automation code references. The values match the
   Windows SDK definitions. */
#define D2D1_SATURATION_PROP_SATURATION 0

#ifndef VOXFORMA_MINGW_UIA_CARET_POSITION
 #define VOXFORMA_MINGW_UIA_CARET_POSITION 1
 #ifdef __cplusplus
 enum UIA_CaretPosition_Compat
 {
     CaretPosition_BeginningOfLine = 0,
     CaretPosition_EndOfLine       = 1,
     CaretPosition_Unknown         = 2
 };
 #else
  #define CaretPosition_BeginningOfLine 0
  #define CaretPosition_EndOfLine       1
  #define CaretPosition_Unknown         2
 #endif
#endif

/* The JUCE audio-device and audio-format modules reference Windows types that
   are not needed by a hosted plugin and that the MinGW headers spell
   differently in places. Disable them. */
#ifndef JUCE_USE_WINRT_MIDI
 #define JUCE_USE_WINRT_MIDI 0
#endif

/* The MinGW CRT does not pull the <cstring>/<cstdlib> declarations into every
   translation unit that JUCE relies on. Include them up front so that
   juce_Memory.h, juce_CharacterFunctions.h and friends find memset, memcpy,
   strlen, strcmp, stricmp and calloc. */
#ifdef __cplusplus
 #include <cstring>
 #include <cstdlib>
 #include <cstdint>
#else
 #include <string.h>
 #include <stdlib.h>
 #include <stdint.h>
#endif

/* MinGW spells the MSVC SAL callback annotation SAL__callback; JUCE uses the
   short MSVC form in juce_Midi_windows.cpp. Map one to the other. */
#ifndef __callback
 #ifdef SAL__callback
  #define __callback SAL__callback
 #else
  #define __callback
 #endif
#endif

/* nserror.h defines NS_E_NO_MORE_SAMPLES but juce_WindowsMediaAudioFormat.cpp
   does not include it. The literal value comes from the Windows SDK. */
#ifndef NS_E_NO_MORE_SAMPLES
 #define NS_E_NO_MORE_SAMPLES ((HRESULT) 0xC00D0BCFL)
#endif

/* GCC on x86_64-w64-mingw32 declares __cpuid/__cpuidex via the toolchain's
   own <cpuid.h>, which juce_SystemStats_windows.cpp pulls in on demand. No
   shim is needed here. */

#endif /* VOXFORMA_MINGW_COMPAT_H */
