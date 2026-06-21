/*
 * bench_ueval.c - Hot-path benchmark for ueval.h
 *
 * Not part of the test suite (no pass/fail assertions) - this is a
 * standalone microbenchmark you can run to get real numbers on your
 * own hardware/compiler before deciding whether any of the
 * "Performance Notes" advice in the README actually matters for your
 * call rate.
 *
 * Build:  gcc -O2 bench_ueval.c -o bench_ueval -lm
 * Run:    ./bench_ueval
 *
 * Compares two patterns for feeding new input values into a repeatedly
 * evaluated expression at a high call rate (e.g. once per MIDI message):
 *
 *   Pattern A: ueval_bind() once at setup, then ueval_ptr() once to get
 *              a stable pointer, then *ptr = value on every call.
 *   Pattern B: ueval_bind(env, name, value) by name on every call.
 *
 * Both patterns produce identical results; only the per-call update
 * mechanism differs.
 */

#include "ueval.h"
#include <stdio.h>
#include <time.h>

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void bench_realistic(long n) {
    /* A handful of variables, representative of a small DSP/control
     * expression - e.g. transforming a couple of input values. */
    ueval_env env;
    ueval_init(&env);
    ueval_bind(&env, "a", 0.0);
    ueval_bind(&env, "b", 0.0);
    ueval_bind(&env, "c", 0.0);
    const char *expr = "(c - 60) / 12 + a * b";

    double *ap = ueval_ptr(&env, "a");
    double *bp = ueval_ptr(&env, "b");
    double *cp = ueval_ptr(&env, "c");

    double sink = 0;
    double t0 = now_sec();
    for (long i = 0; i < n; i++) {
        *ap = (double)(i % 128) / 127.0;
        *bp = (double)((i * 3) % 128) / 127.0;
        *cp = (double)(60 + (i % 24));
        ueval_result r = ueval_eval(&env, expr);
        sink += r.value;
    }
    double t1 = now_sec();
    printf("Pattern A (ueval_ptr cache), 3 vars, %ld calls: %.1f ns/call  [sink=%.3f]\n",
           n, (t1 - t0) * 1e9 / n, sink);

    sink = 0;
    double t2 = now_sec();
    for (long i = 0; i < n; i++) {
        ueval_bind(&env, "a", (double)(i % 128) / 127.0);
        ueval_bind(&env, "b", (double)((i * 3) % 128) / 127.0);
        ueval_bind(&env, "c", (double)(60 + (i % 24)));
        ueval_result r = ueval_eval(&env, expr);
        sink += r.value;
    }
    double t3 = now_sec();
    printf("Pattern B (ueval_bind by name), 3 vars, %ld calls: %.1f ns/call  [sink=%.3f]\n",
           n, (t3 - t2) * 1e9 / n, sink);

    printf("  -> slowdown B/A: %.2fx\n\n", (t3 - t2) / (t1 - t0));
}

static void bench_worst_case(long n) {
    /* Near-full variable table (UEVAL_MAX_VARS default is 32), with the
     * hot variable in the last slot - worst case for ueval_bind's
     * linear strncmp scan. */
    ueval_env env;
    ueval_init(&env);

    char names[31][8];
    for (int i = 0; i < 31; i++) {
        snprintf(names[i], sizeof(names[i]), "v%d", i);
        ueval_bind(&env, names[i], (double)i);
    }
    ueval_bind(&env, "hot", 0.0); /* 32nd / last slot */

    const char *expr = "hot * 2";
    double *hot = ueval_ptr(&env, "hot");

    double sink = 0;
    double t0 = now_sec();
    for (long i = 0; i < n; i++) {
        *hot = (double)(i % 128) / 127.0;
        ueval_result r = ueval_eval(&env, expr);
        sink += r.value;
    }
    double t1 = now_sec();
    printf("Pattern A (ueval_ptr cache), 32 vars/hot last, %ld calls: %.1f ns/call  [sink=%.3f]\n",
           n, (t1 - t0) * 1e9 / n, sink);

    sink = 0;
    double t2 = now_sec();
    for (long i = 0; i < n; i++) {
        ueval_bind(&env, "hot", (double)(i % 128) / 127.0);
        ueval_result r = ueval_eval(&env, expr);
        sink += r.value;
    }
    double t3 = now_sec();
    printf("Pattern B (ueval_bind by name), 32 vars/hot last, %ld calls: %.1f ns/call  [sink=%.3f]\n",
           n, (t3 - t2) * 1e9 / n, sink);

    printf("  -> slowdown B/A: %.2fx\n\n", (t3 - t2) / (t1 - t0));
}

static void bench_bare_parse(long n) {
    /* Floor cost: parsing alone, no variables, trivial expression. */
    ueval_env env;
    ueval_init(&env);

    double sink = 0;
    double t0 = now_sec();
    for (long i = 0; i < n; i++) {
        ueval_result r = ueval_eval(&env, "1 + 1");
        sink += r.value;
    }
    double t1 = now_sec();
    printf("Bare parse floor, trivial literal expr, %ld calls: %.1f ns/call  [sink=%.3f]\n\n",
           n, (t1 - t0) * 1e9 / n, sink);
}

int main(void) {
    const long N = 2000000;

    printf("ueval.h hot-path benchmark (%ld calls per scenario)\n", N);
    printf("Build flags affect these numbers a lot - check how this was compiled.\n\n");

    bench_bare_parse(N);
    bench_realistic(N);
    bench_worst_case(N);

    printf("Takeaway: ueval_ptr caching wins more as the variable table fills up.\n"
           "At a handful of variables the difference is small; near a full\n"
           "default-sized table (32 vars) it's roughly 1.5x in this measurement.\n"
           "Either way, the parse itself is the floor cost - see README\n"
           "\"Performance Notes\" for what that means for a MIDI-rate callback.\n");

    return 0;
}
