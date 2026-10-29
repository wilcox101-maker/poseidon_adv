#pragma once

/*
 * version.h — single source of truth for the POSEIDON build tag.
 * POSEIDON_VERSION + POSEIDON_BUILD_DATE come from -D flags in
 * platformio.ini so a rebuild always reflects the current checkout.
 */

#ifndef POSEIDON_VERSION
#define POSEIDON_VERSION "0.8.1"
#endif

/* __DATE__ can't be passed through a -D flag as a literal, so bake it
 * here at the file that gets (re)compiled via -DPOSEIDON_VERSION
 * trigger. */
#undef POSEIDON_BUILD_DATE
#define POSEIDON_BUILD_DATE __DATE__

/* sys-027: static inline so each TU that includes this header gets
 * its own internal-linkage definition, matching Bruce's pattern.
 * Plain `inline` in C++ relies on the compiler picking exactly one
 * definition at link time — fine on this build but ODR-fragile if
 * the header is later included from a C TU or under different
 * inlining decisions. `static inline` is unconditional. */
static inline const char *poseidon_version(void)    { return POSEIDON_VERSION; }
static inline const char *poseidon_build_date(void) { return POSEIDON_BUILD_DATE; }
