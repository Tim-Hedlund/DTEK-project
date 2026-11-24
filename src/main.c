#include "hardware.h"
#include "fixmath.h"
#include "dtekv-lib.h"

void handle_interrupt(unsigned _irq) {}

#define BENCH_ITERS 1000
#define PI_F        3.14159265f

// ============================================================
// Minimal Terminal Printing Helpers (no stdio/printf needed)
// ============================================================
static void print_uint(uint32_t v) {
    char buf[12], out[12];
    int i = 0;
    if (v == 0) {
        print("0");
        return;
    }
    while (v > 0) {
        buf[i++] = (char)('0' + (v % 10));
        v /= 10;
    }
    for (int j = 0; j < i; j++) out[j] = buf[i - 1 - j];
    out[i] = '\0';
    print(out);
}

// Prints a non-negative float with 5 decimal places (e.g. 0.00056)
static void print_float5(float v) {
    if (v < 0.0f) {
        print("-");
        v = -v;
    }
    uint32_t whole = (uint32_t)v;
    uint32_t frac  = (uint32_t)((v - (float)whole) * 100000.0f + 0.5f);
    if (frac >= 100000) {
        whole++;
        frac -= 100000;
    }

    print_uint(whole);
    print(".");
    char dec[6];
    for (int i = 4; i >= 0; i--) {
        dec[i] = (char)('0' + (frac % 10));
        frac /= 10;
    }
    dec[5] = '\0';
    print(dec);
}

static void print_bench_row(const char *name, uint32_t cyc_fix, uint32_t cyc_flt) {
    print(name);
    print(" | fix: ");
    print_uint(cyc_fix / BENCH_ITERS);
    print(" cyc/op | float: ");
    print_uint(cyc_flt / BENCH_ITERS);
    print(" cyc/op | speedup: ");
    print_float5((float)cyc_flt / (float)(cyc_fix ? cyc_fix : 1));
    print("x\n");
}

// ============================================================
// Float Reference Helpers (avoids needing -lm)
// ============================================================
static float ref_fabsf(float x) {
    return (x < 0.0f) ? -x : x;
}

static float ref_sinf(int angle) {
    int a = angle & ANGLE_MASK;
    int neg = 0;
    if (a >= ANGLE_180) { a -= ANGLE_180; neg = 1; }
    if (a > ANGLE_90)   { a = ANGLE_180 - a; }

    float r  = (float)a * (PI_F / 512.0f);
    float r2 = r * r;
    // 9th-order Taylor polynomial over [0, pi/2], max error < 4e-6
    float s  = r * (1.0f - r2 * (1.0f / 6.0f
                 - r2 * (1.0f / 120.0f
                 - r2 * (1.0f / 5040.0f
                 - r2 * (1.0f / 362880.0f)))));
    return neg ? -s : s;
}

static float ref_cosf(int angle) {
    return ref_sinf(angle + ANGLE_90);
}

static inline float fix_to_f(fix_t x) {
    return (float)x / 65536.0f;
}

static inline fix_t f_to_fix(float x) {
    return (fix_t)(x * 65536.0f);
}

// ============================================================
// 1. Correctness Tests
// ============================================================
static int run_correctness_tests(void) {
    int passed = 1;
    print("\n=== CORRECTNESS TESTS (vs float) ===\n");

    // Test values covering typical raycaster ranges
    static const float vals[] = {
        -12.75f, -3.5f, -0.625f, -0.125f, 0.125f, 0.5f, 1.333f, 7.25f, 19.5f
    };
    const int n = (int)(sizeof(vals) / sizeof(vals[0]));

    float max_mul_err = 0.0f;
    float max_div_err = 0.0f;
    float max_inv_err = 0.0f;

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            float a = vals[i], b = vals[j];
            fix_t fa = f_to_fix(a), fb = f_to_fix(b);

            float err_m = ref_fabsf(fix_to_f(fix_mul(fa, fb)) - (a * b));
            if (err_m > max_mul_err) max_mul_err = err_m;

            float err_d = ref_fabsf(fix_to_f(fix_div(fa, fb)) - (a / b));
            if (err_d > max_div_err) max_div_err = err_d;
        }
        float a = vals[i];
        float err_i = ref_fabsf(fix_to_f(fix_inv_abs(f_to_fix(a))) - (1.0f / ref_fabsf(a)));
        if (err_i > max_inv_err) max_inv_err = err_i;
    }

    // Sweep all 1024 angles for sin and cos
    float max_sin_err = 0.0f;
    float max_cos_err = 0.0f;
    for (int a = 0; a < 1024; a++) {
        float es = ref_fabsf(fix_to_f(fix_sin(a)) - ref_sinf(a));
        float ec = ref_fabsf(fix_to_f(fix_cos(a)) - ref_cosf(a));
        if (es > max_sin_err) max_sin_err = es;
        if (ec > max_cos_err) max_cos_err = ec;
    }

    int cardinals_ok =
        (fix_sin(0) == 0)         && (fix_cos(0) == FIX_ONE) &&
        (fix_sin(ANGLE_90) == FIX_ONE)  && (fix_cos(ANGLE_90) == 0) &&
        (fix_sin(ANGLE_180) == 0)       && (fix_cos(ANGLE_180) == -FIX_ONE) &&
        (fix_sin(ANGLE_270) == -FIX_ONE)&& (fix_cos(ANGLE_270) == 0);

    print("fix_mul     max err: "); print_float5(max_mul_err);
    print((max_mul_err < 0.002f) ? " [PASS]\n" : " [FAIL]\n");

    print("fix_div     max err: "); print_float5(max_div_err);
    print((max_div_err < 0.005f) ? " [PASS]\n" : " [FAIL]\n");

    print("fix_inv_abs max err: "); print_float5(max_inv_err);
    print((max_inv_err < 0.005f) ? " [PASS]\n" : " [FAIL]\n");

    print("fix_sin     max err: "); print_float5(max_sin_err);
    print((max_sin_err < 0.001f) ? " [PASS]\n" : " [FAIL]\n");

    print("fix_cos     max err: "); print_float5(max_cos_err);
    print((max_cos_err < 0.001f) ? " [PASS]\n" : " [FAIL]\n");

    print("Cardinals (0/90/180/270 exact): ");
    print(cardinals_ok ? "[PASS]\n" : "[FAIL]\n");

    if (max_mul_err >= 0.002f || max_div_err >= 0.005f ||
        max_inv_err >= 0.005f || max_sin_err >= 0.001f ||
        max_cos_err >= 0.001f || !cardinals_ok) {
        passed = 0;
    }
    return passed;
}

// ============================================================
// 2. Speed Benchmarks (volatile prevents -O3 from folding loops)
// ============================================================
static void run_speed_benchmarks(void) {
    print("\n=== SPEED BENCHMARKS (1000 ops each, mcycle) ===\n");

    volatile fix_t va_fix = f_to_fix(3.75f);
    volatile fix_t vb_fix = f_to_fix(1.375f);
    volatile float va_flt = 3.75f;
    volatile float vb_flt = 1.375f;
    volatile fix_t sink_fix = 0;
    volatile float sink_flt = 0.0f;
    uint32_t t0, t1, cyc_fix, cyc_flt;

    // --- Multiply ---
    t0 = hardware_get_cycles();
    for (int i = 0; i < BENCH_ITERS; i++) {
        sink_fix = fix_mul(va_fix + i, vb_fix);
    }
    t1 = hardware_get_cycles();
    cyc_fix = t1 - t0;

    t0 = hardware_get_cycles();
    for (int i = 0; i < BENCH_ITERS; i++) {
        sink_flt = (va_flt + (float)i) * vb_flt;
    }
    t1 = hardware_get_cycles();
    cyc_flt = t1 - t0;
    print_bench_row("mul    ", cyc_fix, cyc_flt);

    // --- Divide ---
    t0 = hardware_get_cycles();
    for (int i = 0; i < BENCH_ITERS; i++) {
        sink_fix = fix_div(va_fix + i, vb_fix);
    }
    t1 = hardware_get_cycles();
    cyc_fix = t1 - t0;

    t0 = hardware_get_cycles();
    for (int i = 0; i < BENCH_ITERS; i++) {
        sink_flt = (va_flt + (float)i) / vb_flt;
    }
    t1 = hardware_get_cycles();
    cyc_flt = t1 - t0;
    print_bench_row("div    ", cyc_fix, cyc_flt);

    // --- Old 64-bit Divide ---
    t0 = hardware_get_cycles();
    for (int i = 0; i < BENCH_ITERS; i++) {
        sink_fix = (fix_t)(((int64_t)(va_fix + i) << FIX_SHIFT) / vb_fix);
    }
    t1 = hardware_get_cycles();
    print_bench_row("div64  ", t1 - t0, cyc_flt);

    // --- Inv Abs (|1 / x|, raycaster deltaDist) ---
    t0 = hardware_get_cycles();
    for (int i = 0; i < BENCH_ITERS; i++) {
        sink_fix = fix_inv_abs(vb_fix + (i << 4));
    }
    t1 = hardware_get_cycles();
    cyc_fix = t1 - t0;

    t0 = hardware_get_cycles();
    for (int i = 0; i < BENCH_ITERS; i++) {
        sink_flt = 1.0f / ref_fabsf(vb_flt + (float)i * 0.01f);
    }
    t1 = hardware_get_cycles();
    cyc_flt = t1 - t0;
    print_bench_row("inv_abs", cyc_fix, cyc_flt);

    // --- Sin ---
    t0 = hardware_get_cycles();
    for (int i = 0; i < BENCH_ITERS; i++) {
        sink_fix = fix_sin(i);
    }
    t1 = hardware_get_cycles();
    cyc_fix = t1 - t0;

    t0 = hardware_get_cycles();
    for (int i = 0; i < BENCH_ITERS; i++) {
        sink_flt = ref_sinf(i);
    }
    t1 = hardware_get_cycles();
    cyc_flt = t1 - t0;
    print_bench_row("sin    ", cyc_fix, cyc_flt);

    // Show fix_inv_abs cycles/op on the 7-segment display as a quick readout
    (void)sink_fix;
    (void)sink_flt;
}

int main(void) {
    hardware_init();

    int ok = run_correctness_tests();
    run_speed_benchmarks();

    // Light all 10 LEDs if all correctness checks passed, or just LED 0 if failed
    hardware_set_leds(ok ? 0x3FF : 0x001);
    print("\nDone. All LEDs ON = PASS.\n");

    while (1) {}
}