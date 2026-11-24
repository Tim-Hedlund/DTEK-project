#pragma once
#include <stdint.h>

typedef int32_t fix_t;

#define FIX_SHIFT 16
#define FIX_ONE   (1 << FIX_SHIFT)       // 1.0  = 65536(int)
#define FIX_HALF  (1 << (FIX_SHIFT - 1)) // 0.5  = 32768(int)
#define FIX_MAX   0x7FFFFFFF

// Angle units: 1024 = full circle (360 deg), so wrapping is just (angle & 1023)
#define ANGLE_90   256
#define ANGLE_180  512
#define ANGLE_270  768
#define ANGLE_MASK 1023

// Conversions 
#define INT_TO_FIX(x) ((fix_t)(x) << FIX_SHIFT)
#define FIX_TO_INT(x) ((x) >> FIX_SHIFT)       // Floors toward -inf (exact for grid coords)
#define FIX_FRAC(x)   ((x) & (FIX_ONE - 1))    // Fractional part in [0, 1)

// Basic Math, addition and subtraction use int defaults
static inline fix_t fix_abs(fix_t x) {
    return (x < 0) ? -x : x;
}

static inline fix_t fix_mul(fix_t a, fix_t b) {
    return (fix_t)(((int64_t)a * b) >> FIX_SHIFT);
}

// Copied from libfixmath library
static inline fix_t fix_div(fix_t a, fix_t b) {
    if (b == 0) return (a >= 0) ? FIX_MAX : -FIX_MAX;

    int neg = (a < 0) ^ (b < 0);
    uint32_t rem = (a < 0) ? (uint32_t)(-a) : (uint32_t)a;
    uint32_t div = (b < 0) ? (uint32_t)(-b) : (uint32_t)b;
    uint32_t quot = 0;
    int bit = FIX_SHIFT;

    while (rem != 0 && bit >= 0) {
        while (bit > 0 && (rem & 0x80000000u) == 0) {
            rem <<= 1;
            bit--;
        }
        uint32_t q = rem / div;
        rem %= div;
        quot += q << bit;
        if (bit == 0) break;

        if (rem & 0x80000000u) {
            div >>= 1;
        } else {
            rem <<= 1;
        }
        bit--;
    }

    return neg ? -(fix_t)quot : (fix_t)quot;
}

static inline fix_t fix_inv_abs(fix_t x) {
    fix_t ax = fix_abs(x);
    if (ax < 128) return INT_TO_FIX(512); 
    return ((1 << 30) / ax) << 2;
}

// Trigonometry based on libfixmath library
static inline fix_t fix_sin(int angle) {
    int a = angle & ANGLE_MASK;
    int negate = 0;

    if (a >= ANGLE_180) {
        a -= ANGLE_180;
        negate = 1;
    }
    if (a > ANGLE_90) {
        a = ANGLE_180 - a;
    }

    fix_t x  = (fix_t)(a << 8);
    fix_t x2 = fix_mul(x, x);

    fix_t inner = 42334 - fix_mul(x2, 4926);
    fix_t res   = fix_mul(x, 102944 - fix_mul(x2, inner));

    return negate ? -res : res;
}

static inline fix_t fix_cos(int angle) {
    return fix_sin(angle + ANGLE_90);
}