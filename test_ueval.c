/*
 * test_ueval.c - Test suite for ueval.h
 *
 * Build:  gcc test_ueval.c -o test_ueval -lm
 * Run:    ./test_ueval          (verbose: every assertion prints PASS/FAIL)
 *         ./test_ueval -q       (quiet: only section headers + failures + summary)
 *
 * No test framework dependency - plain C, exits non-zero on any failure
 * so it works as a CI gate (see Makefile `test` target).
 *
 * Output is grouped into named sections (one per test_*() function below).
 * Every individual assertion is labeled with what it's actually checking,
 * and prints PASS or FAIL for that specific check - nothing is a silent
 * pass. Each section prints its own subtotal, and a grand total closes
 * the run.
 */

#include "ueval.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int g_pass = 0;
static int g_fail = 0;
static int g_quiet = 0;       /* set via -q */
static int g_section_pass = 0;
static int g_section_fail = 0;

#define EPS 1e-9

/* ---------- section / reporting plumbing ---------------------------------- */

static void begin_section(const char *name) {
    g_section_pass = 0;
    g_section_fail = 0;
    printf("\n== %s ==\n", name);
}

static void end_section(void) {
    printf("   (%d passed, %d failed)\n", g_section_pass, g_section_fail);
}

static void report(int ok, const char *label, const char *detail) {
    if (ok) {
        g_pass++;
        g_section_pass++;
        if (!g_quiet) printf("  PASS  %-26s %s\n", label, detail ? detail : "");
    } else {
        g_fail++;
        g_section_fail++;
        /* failures always print, even in quiet mode */
        printf("  FAIL  %-26s %s\n", label, detail ? detail : "");
    }
}

/* ---------- typed assertions ------------------------------------------------
 * Every assertion takes a short human-readable label describing what's
 * being checked (shown in PASS/FAIL output) in addition to the inputs
 * needed to actually perform the check.
 * ------------------------------------------------------------------------ */

/* Evaluate `expr`, assert it succeeds and equals `expect` within EPS. */
static void check_d(const char *label, const char *expr, double expect, ueval_env *env) {
    char detail[160];
    ueval_result r = ueval_eval(env, expr);
    if (r.status != UEVAL_OK) {
        snprintf(detail, sizeof(detail), "\"%s\" -> error: %s (expected %g)",
                 expr, r.error_msg, expect);
        report(0, label, detail);
        return;
    }
    if (fabs(r.value - expect) > EPS) {
        snprintf(detail, sizeof(detail), "\"%s\" -> %.10g (expected %.10g)",
                 expr, r.value, expect);
        report(0, label, detail);
        return;
    }
    snprintf(detail, sizeof(detail), "\"%s\" -> %.10g", expr, r.value);
    report(1, label, detail);
}

/* Evaluate `expr`, assert it fails with status `expect`. */
static void check_err(const char *label, const char *expr, ueval_status expect, ueval_env *env) {
    char detail[160];
    ueval_result r = ueval_eval(env, expr);
    if (r.status != expect) {
        snprintf(detail, sizeof(detail), "\"%s\" -> status %d (expected %d), msg=\"%s\"",
                  expr, r.status, expect, r.error_msg);
        report(0, label, detail);
        return;
    }
    snprintf(detail, sizeof(detail), "\"%s\" -> status %d (%s)", expr, r.status, r.error_msg);
    report(1, label, detail);
}

/* Generic boolean assertion for checks that aren't a single ueval_eval call
 * (pointer identity, side-effect counters, table-full conditions, etc.). */
static void check_true(const char *label, int condition, const char *detail) {
    report(condition, label, detail);
}

/* ---------- helper functions for binding tests --------------------------- */

static double sq(double x) { return x * x; }
static double clampf(double v, double hi) { return v > hi ? hi : v; }

/* ===========================================================================
 * Literals
 * ======================================================================== */

static void test_literals(void) {
    begin_section("Literals");
    ueval_env env; ueval_init(&env);

    check_d("integer",         "0", 0.0, &env);
    check_d("integer",         "42", 42.0, &env);
    check_d("decimal",         "3.14", 3.14, &env);
    check_d("leading-dot decimal", ".5", 0.5, &env);
    check_d("scientific notation", "1e3", 1000.0, &env);
    check_d("scientific w/ decimal", "1.5e2", 150.0, &env);
    check_d("hex literal",     "0xFF", 255.0, &env);
    check_d("hex literal",     "0x10", 16.0, &env);
    check_d("hex, uppercase X", "0X1A", 26.0, &env);
    end_section();
}

/* ===========================================================================
 * Arithmetic
 * ======================================================================== */

static void test_arithmetic(void) {
    begin_section("Arithmetic");
    ueval_env env; ueval_init(&env);

    check_d("addition",            "2 + 3", 5.0, &env);
    check_d("subtraction",         "10 - 4", 6.0, &env);
    check_d("multiplication",      "3 * 4", 12.0, &env);
    check_d("division",            "10 / 4", 2.5, &env);
    check_d("modulo",               "10 % 3", 1.0, &env);
    check_d("* before + precedence", "2 + 3 * 4", 14.0, &env);
    check_d("parens override precedence", "(2 + 3) * 4", 20.0, &env);
    check_d("left-assoc subtraction", "2 - 3 - 4", -5.0, &env);
    check_d("left-assoc division",  "100 / 10 / 2", 5.0, &env);
    end_section();
}

static void test_power(void) {
    begin_section("Power operator");
    ueval_env env; ueval_init(&env);

    check_d("basic power",          "2**10", 1024.0, &env);
    check_d("fractional exponent",  "2**0.5", sqrt(2.0), &env);
    check_d("right-associative",    "2**2**3", 256.0, &env);   /* 2**(2**3) */
    check_d("explicit left grouping differs", "(2**2)**3", 64.0, &env);
    end_section();
}

/* ===========================================================================
 * Bitwise
 * ======================================================================== */

static void test_bitwise(void) {
    begin_section("Bitwise operators");
    ueval_env env; ueval_init(&env);

    check_d("bitwise AND",   "0xFF & 0x0F", 15.0, &env);
    check_d("bitwise OR",    "0x0F | 0xF0", 255.0, &env);
    check_d("bitwise XOR",   "0x0F ^ 0xFF", 240.0, &env);
    check_d("bitwise NOT",   "~0", -1.0, &env);
    check_d("NOT + AND combo", "~0xF0 & 0xFF", 0x0F, &env);
    check_d("left shift",    "1 << 4", 16.0, &env);
    check_d("right shift",   "256 >> 4", 16.0, &env);
    check_d("& not confused with &&", "4 & 5", 4.0, &env);
    check_d("| not confused with ||", "0 | 2", 2.0, &env);
    end_section();
}

/* ===========================================================================
 * Logical / comparison
 * ======================================================================== */

static void test_logical(void) {
    begin_section("Logical and comparison operators");
    ueval_env env; ueval_init(&env);

    check_d("logical AND, true",  "1 && 1", 1.0, &env);
    check_d("logical AND, false", "1 && 0", 0.0, &env);
    check_d("logical OR, false",  "0 || 0", 0.0, &env);
    check_d("logical OR, any nonzero -> 1", "0 || 5", 1.0, &env);
    check_d("logical NOT of 0",   "!0", 1.0, &env);
    check_d("logical NOT of 1",   "!1", 0.0, &env);
    check_d("logical NOT, only 0 is falsy", "!5", 0.0, &env);

    /* QUIRK: consecutive unary '!' are XOR-toggled as a group, not applied
     * value-by-value, so an even count cancels out entirely instead of
     * idiomatic-C "boolify". See README "Known Quirks". */
    check_d("QUIRK: !! cancels out (not 1)", "!!5", 5.0, &env);
    check_d("QUIRK: !!! same as single !",   "!!!5", 0.0, &env);

    check_d("less than, true",     "3 < 5", 1.0, &env);
    check_d("less than, false",    "5 < 3", 0.0, &env);
    check_d("greater than, true",  "5 > 3", 1.0, &env);

    /* FIXED: '<=' and '>=' previously collided with the shift dispatch
     * (len==2 alone was used to mean "it's a shift"), so "5 <= 5" used to
     * silently evaluate as "5 << 5" (160). The operator's second character
     * is now captured before the recursive right-hand-side parse and used
     * to disambiguate << from <=, and >> from >=. */
    check_d("<= equal case",   "5 <= 5", 1.0, &env);
    check_d("<= false case",   "5 <= 4", 0.0, &env);
    check_d("<= true case",    "4 <= 5", 1.0, &env);
    check_d(">= equal case",   "5 >= 5", 1.0, &env);
    check_d(">= false case",   "3 >= 5", 0.0, &env);
    check_d(">= true case",    "5 >= 3", 1.0, &env);

    /* shift operators still work correctly alongside the <=/>= fix */
    check_d("<< unaffected by <= fix", "1 << 4", 16.0, &env);
    check_d(">> unaffected by >= fix", "256 >> 4", 16.0, &env);

    check_d("equality, true",     "5 == 5", 1.0, &env);
    check_d("inequality, false",  "5 != 5", 0.0, &env);
    check_d("inequality, true",   "5 != 4", 1.0, &env);

    /* comparisons sit below shifts in precedence */
    check_d("shift binds tighter than comparison", "1 << 2 < 8", 1.0, &env); /* (1<<2)<8 */

    /* comparisons are left-associative and do NOT chain like Python:
     * "1 < 2 < 3" parses as "(1 < 2) < 3" -> "1 < 3" -> 1.0. */
    check_d("comparisons left-assoc, not chained (case 1)", "1 < 2 < 3", 1.0, &env);
    check_d("idiomatic chained form via &&", "(1 < 2) && (2 < 3)", 1.0, &env);
    check_d("QUIRK: non-chaining pitfall (looks true, isn't a real chain)",
            "5 < 2 < 100", 1.0, &env); /* (5<2)<100 -> 0<100 -> 1 */

    /* == on doubles is exact, no epsilon - documents standard float behavior */
    check_d("float equality has no epsilon (matches C)", "0.1 + 0.2 == 0.3", 0.0, &env);

    end_section();
}

/* ===========================================================================
 * Ternary (short-circuit)
 * ======================================================================== */

static double g_side_effect_calls = 0;
static double side_effect_marker(double x) { g_side_effect_calls++; return x; }

static void test_ternary(void) {
    begin_section("Ternary operator (short-circuit)");
    ueval_env env; ueval_init(&env);

    check_d("ternary, true branch",  "1 ? 10 : 20", 10.0, &env);
    check_d("ternary, false branch", "0 ? 10 : 20", 20.0, &env);
    check_d("ternary w/ comparison condition (true)", "(1 == 1) ? 1 : 2", 1.0, &env);
    check_d("ternary w/ comparison condition (false)", "(2 > 5) ? 1 : 2", 2.0, &env);

    check_d("nested ternary, true/true", "1 ? (1 ? 100 : 200) : 300", 100.0, &env);
    check_d("nested ternary, false/false", "0 ? 1 : (0 ? 2 : 3)", 3.0, &env);

    /* short-circuit: division by zero in untaken branch must not error */
    ueval_bind(&env, "x", 0.0);
    check_d("short-circuit avoids div-by-zero in false branch", "x != 0 ? 1/x : -1", -1.0, &env);
    ueval_bind(&env, "x", 2.0);
    check_d("true branch evaluated normally", "x != 0 ? 1/x : -1", 0.5, &env);

    /* short-circuit: untaken branch's function calls are not evaluated */
    ueval_init(&env);
    ueval_bind_f1(&env, "se", side_effect_marker);
    g_side_effect_calls = 0;
    check_d("false-branch function call not reached", "1 ? 42 : se(99)", 42.0, &env);
    check_true("false branch fn truly not called", g_side_effect_calls == 0,
               g_side_effect_calls == 0 ? "call count = 0, as expected"
                                         : "call count > 0, side effect leaked");

    g_side_effect_calls = 0;
    check_d("true-branch function call not reached", "0 ? se(99) : 7", 7.0, &env);
    check_true("true branch fn truly not called", g_side_effect_calls == 0,
               g_side_effect_calls == 0 ? "call count = 0, as expected"
                                         : "call count > 0, side effect leaked");

    /* ternary used as a function argument: false branch must skip to the
     * matching comma/paren, not the first one it sees */
    ueval_init(&env);
    ueval_bind_f2(&env, "clamp", clampf);
    ueval_bind(&env, "v", 5.0);
    check_d("ternary inside function arg skips to matching ')'", "clamp(v, 1 ? 10 : 20)", 5.0, &env);

    end_section();
}

/* ===========================================================================
 * Unary operators
 * ======================================================================== */

static void test_unary(void) {
    begin_section("Unary operators");
    ueval_env env; ueval_init(&env);

    check_d("unary minus",        "-5", -5.0, &env);
    check_d("unary plus",         "+5", 5.0, &env);
    check_d("double unary minus", "--5", 5.0, &env);
    check_d("unary minus on group", "-(2+3)", -5.0, &env);

    ueval_bind(&env, "flag", 0.0);
    check_d("logical NOT on variable", "!flag", 1.0, &env);
    check_d("bitwise NOT + AND combo", "~0xF0 & 0xFFFF", 0xFF0F, &env);

    /* Unary minus binds to the primary BEFORE ** is applied (unlike Python,
     * where unary binds looser than **). So -2**2 parses as (-2)**2 = 4,
     * not -(2**2) = -4. Use explicit parens if you mean the latter. */
    check_d("QUIRK: unary minus binds tighter than **", "-2**2", 4.0, &env);
    check_d("explicit parens give the Python-style result", "-(2**2)", -4.0, &env);

    end_section();
}

/* ===========================================================================
 * Variables
 * ======================================================================== */

static void test_variables(void) {
    begin_section("Variable binding");
    ueval_env env; ueval_init(&env);

    ueval_bind(&env, "x", 2.0);
    check_d("read bound variable",  "x", 2.0, &env);
    check_d("variable in expression", "x * 3", 6.0, &env);

    ueval_bind(&env, "x", 100.0);
    check_d("re-bind updates in place", "x", 100.0, &env);

    ueval_bind(&env, "y", 5.0);
    check_d("multiple variables together", "x + y", 105.0, &env);

    /* ueval_ptr gives a stable, writable pointer */
    double *xp = ueval_ptr(&env, "x");
    check_true("ueval_ptr returns non-NULL for bound var", xp != NULL,
               xp != NULL ? "pointer obtained" : "got NULL unexpectedly");
    if (xp) {
        *xp = 7.0;
        check_d("write through ueval_ptr is visible to eval", "x", 7.0, &env);
    }

    double *missing = ueval_ptr(&env, "nope");
    check_true("ueval_ptr returns NULL for unbound var", missing == NULL,
               missing == NULL ? "got NULL, as expected" : "got non-NULL unexpectedly");

    check_err("unbound variable is an error", "undefined_var", UEVAL_ERR_SYMBOL_NOT_FOUND, &env);

    end_section();
}

/* ===========================================================================
 * Functions
 * ======================================================================== */

static void test_functions(void) {
    begin_section("Function binding");
    ueval_env env; ueval_init(&env);

    ueval_bind_f1(&env, "sqrt", sqrt);
    ueval_bind_f1(&env, "sq", sq);
    ueval_bind_f2(&env, "clamp", clampf);

    check_d("1-arg function call",     "sqrt(16)", 4.0, &env);
    check_d("1-arg custom function",   "sq(5)", 25.0, &env);
    check_d("nested function calls",   "sqrt(sq(3))", 3.0, &env);
    check_d("2-arg function, clamps",  "clamp(5, 3)", 3.0, &env);
    check_d("2-arg function, passthrough", "clamp(2, 3)", 2.0, &env);
    check_d("function calls combined with +", "sqrt(4) + sq(2)", 6.0, &env);

    ueval_bind_f1(&env, "sq", sqrt);
    check_d("re-binding a function replaces it", "sq(16)", 4.0, &env);

    check_err("calling an unknown function", "nope(1)", UEVAL_ERR_SYMBOL_NOT_FOUND, &env);
    check_err("too few args IS caught", "clamp(5)", UEVAL_ERR_INVALID_ARGS, &env);

    /* QUIRK: extra arguments are NOT caught for either arity. The parser
     * reads the arguments it expects, then if a 1-arg call sees a ','
     * instead of ')' it simply does not advance, and on a 2-arg call any
     * tokens after the second argument up to the next ')' are unconsumed.
     * Either way, no UEVAL_ERR_INVALID_ARGS is raised and the leftover
     * text is silently dropped (see "trailing garbage" test below). */
    check_d("QUIRK: extra arg to 1-arg fn silently ignored", "sqrt(16,99)", 4.0, &env);
    check_d("QUIRK: extra arg to 2-arg fn silently ignored", "clamp(5,3,9)", 3.0, &env);

    end_section();
}

/* ===========================================================================
 * QUIRK: trailing garbage is not detected
 *
 * ueval_eval() never checks that parsing consumed the entire input string.
 * As soon as the precedence-climbing loop can't match a known operator, it
 * simply stops and returns whatever value it has so far - any leftover
 * characters (typos, stray tokens, unterminated hex literals) are silently
 * dropped rather than reported as an error. Validate expressions yourself
 * if this matters for your use case (e.g. a live-coding text input).
 * ======================================================================== */

static void test_trailing_garbage_not_detected(void) {
    begin_section("QUIRK: trailing garbage is silently dropped");
    ueval_env env; ueval_init(&env);

    check_d("junk after valid expr is dropped, no error", "1 + 1 @", 2.0, &env);
    check_d("\"0x\" with no hex digits -> 0, no error",   "0x", 0.0, &env);
    check_d("non-hex chars after 0x are dropped",         "0xZZ", 0.0, &env);
    check_d("second literal never reached, no error",     "5 5", 5.0, &env);

    end_section();
}

/* ===========================================================================
 * Dollar-variable mode
 * ======================================================================== */

static void test_dollar_mode(void) {
    begin_section("Dollar-variable mode");
    ueval_env env; ueval_init(&env);
    env.dollar_vars = 1;

    ueval_bind(&env, "x", 10.0);
    ueval_bind(&env, "y", 3.0);
    ueval_bind_f1(&env, "sqrt", sqrt);

    check_d("$-prefixed variable read",      "$x", 10.0, &env);
    check_d("$-prefixed variables combined", "$x + $y", 13.0, &env);
    check_d("$-vars mixed with function call", "$x * $y + sqrt($x)", 33.16227766, &env);
    check_d("function calls stay bare (no $)", "sqrt($x + 6)", 4.0, &env);

    check_err("bare identifier rejected in $ mode", "x", UEVAL_ERR_SYMBOL_NOT_FOUND, &env);
    check_err("bare identifier in expr rejected in $ mode", "x * 2", UEVAL_ERR_SYMBOL_NOT_FOUND, &env);

    env.dollar_vars = 0;
    check_d("disabling $ mode restores bare names", "x * 2", 20.0, &env);

    end_section();
}

/* ===========================================================================
 * Error handling
 * ======================================================================== */

static void test_errors(void) {
    begin_section("Error handling");
    ueval_env env; ueval_init(&env);

    check_err("division by zero",        "1 / 0", UEVAL_ERR_DIVISION_BY_ZERO, &env);
    check_err("modulo by zero",          "1 % 0", UEVAL_ERR_DIVISION_BY_ZERO, &env);
    check_err("unclosed paren",          "(1 + 2", UEVAL_ERR_EXPECTED_CLOSE_PAREN, &env);
    check_err("ternary missing ':'",     "1 ? 2", UEVAL_ERR_EXPECTED_COLON, &env);
    check_err("unrecognized character",  "@", UEVAL_ERR_UNEXPECTED_CHAR, &env);
    check_err("unknown symbol",          "unknown_symbol", UEVAL_ERR_SYMBOL_NOT_FOUND, &env);

    ueval_result r = ueval_eval(&env, "1 / 0");
    check_true("error_msg is populated, not empty", strlen(r.error_msg) > 0, r.error_msg);

    check_d("env reusable after an error", "2 + 2", 4.0, &env);

    /* empty / whitespace-only input evaluates to 0 with no error, rather
     * than e.g. UEVAL_ERR_UNEXPECTED_CHAR - there's no "empty expression"
     * status. Worth knowing if you evaluate user-typed text live. */
    check_d("QUIRK: empty string evaluates to 0, no error", "", 0.0, &env);
    check_d("QUIRK: whitespace-only evaluates to 0, no error", "   ", 0.0, &env);

    end_section();
}

static void test_recursion_guard(void) {
    begin_section("Recursion guard");
    ueval_env env; ueval_init(&env);

    /* deeply nested parens should hit the depth guard, not crash */
    char expr[4096];
    int n = 0;
    for (int i = 0; i < 600 && n < (int)sizeof(expr) - 10; i++) expr[n++] = '(';
    expr[n++] = '1';
    for (int i = 0; i < 600 && n < (int)sizeof(expr) - 2; i++) expr[n++] = ')';
    expr[n] = '\0';

    ueval_result r = ueval_eval(&env, expr);
    char detail[64];
    snprintf(detail, sizeof(detail), "600 nested parens -> status %d", r.status);
    check_true("deep nesting hits UEVAL_ERR_STACK_OVERFLOW, not a crash",
               r.status == UEVAL_ERR_STACK_OVERFLOW, detail);

    end_section();
}

/* ===========================================================================
 * Env reuse / thread-safety shape (single-threaded sanity check)
 * ======================================================================== */

static void test_multiple_envs_independent(void) {
    begin_section("Multiple independent envs");
    ueval_env a, b;
    ueval_init(&a);
    ueval_init(&b);

    ueval_bind(&a, "x", 1.0);
    ueval_bind(&b, "x", 2.0);

    check_d("env A keeps its own binding", "x", 1.0, &a);
    check_d("env B keeps its own binding, unaffected by A", "x", 2.0, &b);

    end_section();
}

/* ===========================================================================
 * Configuration overrides (UEVAL_MAX_VARS / FUNCS / DEPTH)
 * NOTE: this file does not redefine the macros (ueval.h is shared with other
 * tests in this binary), so this just exercises the default table-full path.
 * ======================================================================== */

static void test_table_full(void) {
    begin_section("Variable table capacity");
    ueval_env env; ueval_init(&env);
    char name[8];
    int rc = 0;
    for (int i = 0; i < UEVAL_MAX_VARS; i++) {
        snprintf(name, sizeof(name), "v%d", i);
        rc = ueval_bind(&env, name, (double)i);
        if (rc != 0) break;
    }
    rc = ueval_bind(&env, "overflow", 1.0);
    char detail[64];
    snprintf(detail, sizeof(detail), "UEVAL_MAX_VARS=%d, bind past limit returned %d", UEVAL_MAX_VARS, rc);
    check_true("ueval_bind returns -1 once table is full", rc == -1, detail);

    end_section();
}

/* ===========================================================================
 * Version macros
 *
 * Sanity-checks that UEVAL_VERSION (string) and the three numeric
 * UEVAL_VERSION_{MAJOR,MINOR,PATCH} macros agree with each other, so a
 * future hand-edit to one without the others doesn't silently drift.
 * See README "Versioning" and CHANGELOG.md for what these mean.
 * ======================================================================== */

static void test_version(void) {
    begin_section("Version macros");

    char expected[32];
    snprintf(expected, sizeof(expected), "%d.%d.%d",
              UEVAL_VERSION_MAJOR, UEVAL_VERSION_MINOR, UEVAL_VERSION_PATCH);

    char detail[96];
    int matches = (strcmp(UEVAL_VERSION, expected) == 0);
    snprintf(detail, sizeof(detail), "UEVAL_VERSION=\"%s\" vs %d.%d.%d composed from numeric macros",
              UEVAL_VERSION, UEVAL_VERSION_MAJOR, UEVAL_VERSION_MINOR, UEVAL_VERSION_PATCH);
    check_true("UEVAL_VERSION string matches numeric macros", matches, detail);

    check_true("MAJOR is non-negative", UEVAL_VERSION_MAJOR >= 0, NULL);
    check_true("MINOR is non-negative", UEVAL_VERSION_MINOR >= 0, NULL);
    check_true("PATCH is non-negative", UEVAL_VERSION_PATCH >= 0, NULL);

    end_section();
}

/* ===========================================================================
 * main
 * ======================================================================== */

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0) g_quiet = 1;
    }

    printf("ueval.h test suite (testing version %s)%s\n", UEVAL_VERSION, g_quiet ? " (quiet mode)" : "");

    test_version();
    test_literals();
    test_arithmetic();
    test_power();
    test_bitwise();
    test_logical();
    test_ternary();
    test_unary();
    test_variables();
    test_functions();
    test_dollar_mode();
    test_errors();
    test_trailing_garbage_not_detected();
    test_recursion_guard();
    test_multiple_envs_independent();
    test_table_full();

    printf("\n========================================\n");
    printf("TOTAL: %d passed, %d failed\n", g_pass, g_fail);
    printf("========================================\n");
    return g_fail == 0 ? 0 : 1;
}
