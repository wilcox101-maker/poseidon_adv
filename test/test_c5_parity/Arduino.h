/*
 * Host stub for <Arduino.h>, used ONLY by this native test suite.
 *
 * src/c5_cmd.h is a POSEIDON-side (S3/Arduino) header, so its first line is
 * `#include <Arduino.h>`. On the native host there is no Arduino, but the only
 * thing c5_cmd.h actually needs from it is the fixed-width integer types,
 * size_t/offsetof, and memcpy — everything else in the file is enums, packed
 * structs, static_asserts and function *declarations* (no bodies). Providing
 * those three standard headers here lets the native test include the real,
 * unmodified c5_cmd.h alongside the C5's proto.h and compare their wire enums.
 *
 * PlatformIO puts the current test suite's own directory on the include path,
 * so this file satisfies c5_cmd.h's `#include <Arduino.h>` for this suite only
 * and never shadows the real Arduino.h in any firmware build.
 */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
