/*
 * scene_compat_qnx65 — force-included (-include) before every C++ scene source.
 *
 * WHY THIS EXISTS
 * Luka builds the scene engine with QNX GCC 8.5.  Our toolchain is the one this
 * project already uses for everything else: GCC 4.9.4 against the QNX 6.5 SP1
 * (Dinkum) headers.  That pairing does not expose the C99 float math the scene
 * sources call.  Measured in the container, after #include <cmath>:
 *
 *     std::fminf          FAIL        std::fabs            OK
 *     std::fmaxf          FAIL        __builtin_isfinite   OK
 *     std::fpclassify     FAIL
 *     std::isfinite       FAIL        ::fminf / ::fmaxf    FAIL
 *
 * Including <math.h> before <cmath> does not help either — it breaks <cmath>'s
 * own std::abs overloads.
 *
 * This header supplies exactly the four spellings the scene sources use, and
 * nothing else, so that NOT ONE LINE of Luka's scene sources has to change.  It
 * is pulled in from build_overlay.sh with -include, which puts it ahead of the
 * sources' own includes.
 *
 * WHY fminf/fmaxf AND NOT std::fmin/std::fmax
 * This is Luka's landmine, and it still applies: on QNX 6.5 the xtgmath float
 * templates behind std::fmin/std::fmax recurse into a `b .` self-branch and the
 * renderer hangs with "progress watchdog TIMEOUT phase=lane-update"
 * (docs/LANDMINES.md:85).  His sources therefore call ::fminf / ::fmaxf, the
 * real libm entry points.  We declare those entry points rather than provide
 * our own inline versions, so the call still lands in libm exactly as it does
 * on his unit.
 *
 * ADDING TO namespace std
 * Formally reserved, pragmatically the only way to give an old libstdc++ a
 * C++11 name without editing the callers.  Scoped to float and double, which
 * is all the scene code passes.
 */

#ifndef K3346_SCENE_COMPAT_QNX65_HPP
#define K3346_SCENE_COMPAT_QNX65_HPP

#include <cmath>

/* The libm symbols QNX's math.h declares but keeps out of reach of this
 * libstdc++ pairing.  Real functions, not inlines — same code path as Luka's.
 * Exactly the set the scene sources call, nothing speculative:
 *   fmaxf 23x  fminf 19x  fabsf 13x  sinf 8x  cosf 8x  sqrtf 3x  hypotf 1x  expf 1x */
extern "C" {
float fminf(float, float);
float fmaxf(float, float);
float hypotf(float, float);
}
/* Deliberately NOT declared here: fabsf, sqrtf and expf already resolve, and
 * sinf/cosf are MACROS in QNX's math.h (they expand to _FSin/_FCos with a
 * quadrant argument).  Declaring those two as functions makes the macro eat
 * the declaration and the build dies inside <cmath>.  The scene sources call
 * them unqualified, so the macro serves them correctly. */

namespace std {

inline bool isfinite(float v)  { return __builtin_isfinite(v) != 0; }
inline bool isfinite(double v) { return __builtin_isfinite(v) != 0; }

}  /* namespace std */

/* Unqualified isfinite() as well: the shared headers (route_progress.h,
 * route_path.h) are plain C and call it the C way.  When C compiles them the
 * <math.h> macro answers; when the scene engine pulls them into C++ that macro
 * has been #undef'd by <cmath>, so the overloads below stand in for it. */
inline bool isfinite(float v)  { return __builtin_isfinite(v) != 0; }
inline bool isfinite(double v) { return __builtin_isfinite(v) != 0; }

#endif /* K3346_SCENE_COMPAT_QNX65_HPP */
