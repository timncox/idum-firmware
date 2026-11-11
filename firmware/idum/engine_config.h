#pragma once

// Alternate-engine selection is deliberately compile-time.  The resulting
// firmware contains the stock runner plus at most one alternate runner; the
// physical CYCLE switch selects between those two at runtime.
#define IDUM_ALT_ENGINE_NONE 0
#define IDUM_ALT_ENGINE_SUPERLOOPER 1
#define IDUM_ALT_ENGINE_BEATGEN 2

#ifndef IDUM_ALT_ENGINE
#define IDUM_ALT_ENGINE IDUM_ALT_ENGINE_SUPERLOOPER
#endif

#if (IDUM_ALT_ENGINE != IDUM_ALT_ENGINE_NONE) && (IDUM_ALT_ENGINE != IDUM_ALT_ENGINE_SUPERLOOPER) \
    && (IDUM_ALT_ENGINE != IDUM_ALT_ENGINE_BEATGEN)
#error "Unknown IDUM_ALT_ENGINE selection"
#endif
