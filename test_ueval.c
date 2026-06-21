/*
 * test_ueval.c - Test suite for ueval.h
 *
 * Build:  gcc test_ueval.c -o test_ueval -lm
 * Run:    ./test_ueval
 *
 * No test framework dependency - plain C, exits non-zero on any failure
 * so it works as a CI gate (see Makefile `test` target).
 */

#include "ueval.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int g_pass = 0;
static int g_fail = 0;

#define EPS 1e-9

static void check_d(const char *expr, double expect, ueval_env *env) {
    ueval_result r = ueval_eval(env, expr);
    if (r.status != UEVAL_OK) {
        printf("FAIL: \"%s\" -> error: %s (expected %g)\n", expr, r.error_msg, expect);
        g_fail++;
        return;
    }
    if (fabs(r.value - expect) > EPS) {
        printf("FAIL: \"%s\" -> %.10g (expected %.10g)\n", expr, r.value, expect);
        g_fail++;
        return;
    }
    g_pass++;
}

static void check_err(const char *expr, ueval_status expect, ueval_env *env) {
    ueval_result r = ueval_eval(env, expr);
    if (r.status != expect) {
        printf("FAIL: \"%s\" -> status %d (expected %d), msg=\"%s\"\n",
               expr, r.status, expect, r.error_msg);
        g_fail++;
        return;
    }
    g_pass++;
}

/* ---------- helper functions for binding tests --------------------------- */

static double sq(double x) { return x * x; }
static double clampf(double v, double hi) { return v > hi ? hi : v; }

/* ===========================================================================
 * Literals
 * ======================================================================== */

static void test_literals(void) {
    ueval_env env; ueval_init(&env);

    check_d("0", 0.0, &env);
    check_d("42", 42.0, &env);
    check_d("3.14", 3.14, &env);
    check_d(".5", 0.5, &env);
    check_d("1e3", 1000.0, &env);
    check_d("1.5e2", 150.0, &env);
    check_d("0xFF", 255.0, &env);
    check_d("0x10", 16.0, &env);
    check_d("0X1A", 26.0, &env); /* uppercase X */
}

/* ===========================================================================
 * Arithmetic
 * ======================================================================== */

static void test_arithmetic(void) {
    ueval_env env; ueval_init(&env);

    check_d("2 + 3", 5.0, &env);
    check_d("10 - 4", 6.0, &env);
    check_d("3 * 4", 12.0, &env);
    check_d("10 / 4", 2.5, &env);
    check_d("10 % 3", 1.0, &env);
    check_d("2 + 3 * 4", 14.0, &env);       /* precedence */
    check_d("(2 + 3) * 4", 20.0, &env);     /* grouping */
    check_d("2 - 3 - 4", -5.0, &env);       /* left-assoc subtraction */
    check_d("100 / 10 / 2", 5.0, &env);     /* left-assoc division */
}

static void test_power(void) {
    ueval_env env; ueval_init(&env);

    check_d("2**10", 1024.0, &env);
    check_d("2**0.5", sqrt(2.0), &env);
    check_d("2**2**3", 256.0, &env);        /* right-assoc: 2**(2**3) */
    check_d("(2**2)**3", 64.0, &env);       /* explicit left grouping differs */
}

/* ===========================================================================
 * Bitwise
 * ======================================================================== */

static void test_bitwise(void) {
    ueval_env env; ueval_init(&env);

    check_d("0xFF & 0x0F", 15.0, &env);
    check_d("0x0F | 0xF0", 255.0, &env);
    check_d("0x0F ^ 0xFF", 240.0, &env);
    check_d("~0", -1.0, &env);
    check_d("~0xF0 & 0xFF", 0x0F, &env);
    check_d("1 << 4", 16.0, &env);
    check_d("256 >> 4", 16.0, &env);
    /* operator-vs-prefix disambiguation */
    check_d("4 & 5", 4.0, &env);            /* bitwise AND, not &&  */
    check_d("0 | 2", 2.0, &env);            /* bitwise OR, not ||   */
}

/* ===========================================================================
 * Logical / comparison
 * ======================================================================== */

static void test_logical(void) {
    ueval_env env; ueval_init(&env);

    check_d("1 && 1", 1.0, &env);
    check_d("1 && 0", 0.0, &env);
    check_d("0 || 0", 0.0, &env);
    check_d("0 || 5", 1.0, &env);           /* any non-zero -> 1 */
    check_d("!0", 1.0, &env);
    check_d("!1", 0.0, &env);
    check_d("!5", 0.0, &env);               /* only 0 is falsy */
    /* QUIRK: consecutive unary '!' are XOR-toggled as a group, not applied
     * value-by-value, so an even count cancels out entirely instead of
     * idiomatic-C "boolify". !!5 is therefore 5, not 1. See README
     * "Known Quirks". */
    check_d("!!5", 5.0, &env);
    check_d("!!!5", 0.0, &env);             /* odd count behaves like a single ! */

    check_d("3 < 5", 1.0, &env);
    check_d("5 < 3", 0.0, &env);
    check_d("5 > 3", 1.0, &env);
    /* FIXED: '<=' and '>=' previously collided with the shift dispatch
     * (len==2 alone was used to mean "it's a shift"), so "5 <= 5" used to
     * silently evaluate as "5 << 5" (160). The operator's second character
     * is now captured before the recursive right-hand-side parse and used
     * to disambiguate << from <=, and >> from >=. */
    check_d("5 <= 5", 1.0, &env);
    check_d("5 <= 4", 0.0, &env);
    check_d("4 <= 5", 1.0, &env);
    check_d("5 >= 5", 1.0, &env);
    check_d("3 >= 5", 0.0, &env);
    check_d("5 >= 3", 1.0, &env);

    /* shift operators still work correctly alongside the fix */
    check_d("1 << 4", 16.0, &env);
    check_d("256 >> 4", 16.0, &env);

    check_d("5 == 5", 1.0, &env);
    check_d("5 != 5", 0.0, &env);
    check_d("5 != 4", 1.0, &env);

    /* comparisons sit below shifts in precedence */
    check_d("1 << 2 < 8", 1.0, &env);   /* (1<<2)<8 -> 4<8 -> 1 */

    /* comparisons are left-associative and do NOT chain like Python:
     * "1 < 2 < 3" parses as "(1 < 2) < 3" -> "1 < 3" -> 1.0, which happens
     * to read like the chained result here but is NOT (a<b)&&(b<c). */
    check_d("1 < 2 < 3", 1.0, &env);
    check_d("(1 < 2) && (2 < 3)", 1.0, &env);  /* idiomatic chained form */
    check_d("5 < 2 < 100", 1.0, &env);          /* (5<2)<100 -> 0<100 -> 1, NOT chained-false */

    /* == on doubles is exact, no epsilon - documents standard float behavior */
    check_d("0.1 + 0.2 == 0.3", 0.0, &env);
}

/* ===========================================================================
 * Ternary (short-circuit)
 * ======================================================================== */

static double g_side_effect_calls = 0;
static double side_effect_marker(double x) { g_side_effect_calls++; return x; }

static void test_ternary(void) {
    ueval_env env; ueval_init(&env);

    check_d("1 ? 10 : 20", 10.0, &env);
    check_d("0 ? 10 : 20", 20.0, &env);
    check_d("(1 == 1) ? 1 : 2", 1.0, &env);
    check_d("(2 > 5) ? 1 : 2", 2.0, &env);

    /* nested ternaries */
    check_d("1 ? (1 ? 100 : 200) : 300", 100.0, &env);
    check_d("0 ? 1 : (0 ? 2 : 3)", 3.0, &env);

    /* short-circuit: division by zero in untaken branch must not error */
    ueval_bind(&env, "x", 0.0);
    check_d("x != 0 ? 1/x : -1", -1.0, &env);
    ueval_bind(&env, "x", 2.0);
    check_d("x != 0 ? 1/x : -1", 0.5, &env);

    /* short-circuit: untaken branch's function calls are not evaluated */
    ueval_init(&env);
    ueval_bind_f1(&env, "se", side_effect_marker);
    g_side_effect_calls = 0;
    check_d("1 ? 42 : se(99)", 42.0, &env);
    if (g_side_effect_calls != 0) {
        printf("FAIL: ternary false-branch function was evaluated despite short-circuit\n");
        g_fail++;
    } else {
        g_pass++;
    }

    g_side_effect_calls = 0;
    check_d("0 ? se(99) : 7", 7.0, &env);
    if (g_side_effect_calls != 0) {
        printf("FAIL: ternary true-branch function was evaluated despite short-circuit\n");
        g_fail++;
    } else {
        g_pass++;
    }

    /* ternary used as a function argument: false branch must skip to the
     * matching comma/paren, not the first one it sees */
    ueval_init(&env);
    ueval_bind_f2(&env, "clamp", clampf);
    ueval_bind(&env, "v", 5.0);
    check_d("clamp(v, 1 ? 10 : 20)", 5.0, &env);
}

/* ===========================================================================
 * Unary operators
 * ======================================================================== */

static void test_unary(void) {
    ueval_env env; ueval_init(&env);

    check_d("-5", -5.0, &env);
    check_d("+5", 5.0, &env);
    check_d("--5", 5.0, &env);              /* double negation via loop */
    check_d("-(2+3)", -5.0, &env);
    /* Unary minus binds to the primary BEFORE ** is applied (unlike Python,
     * where unary binds looser than **). So -2**2 parses as (-2)**2 = 4,
     * not -(2**2) = -4. Use explicit parens if you mean the latter. */
    check_d("-2**2", 4.0, &env);
    check_d("-(2**2)", -4.0, &env);
    ueval_bind(&env, "flag", 0.0);
    check_d("!flag", 1.0, &env);
    check_d("~0xF0 & 0xFFFF", 0xFF0F, &env);
}

/* ===========================================================================
 * Variables
 * ======================================================================== */

static void test_variables(void) {
    ueval_env env; ueval_init(&env);

    ueval_bind(&env, "x", 2.0);
    check_d("x", 2.0, &env);
    check_d("x * 3", 6.0, &env);

    /* re-binding updates in place, doesn't duplicate */
    ueval_bind(&env, "x", 100.0);
    check_d("x", 100.0, &env);

    /* multiple variables */
    ueval_bind(&env, "y", 5.0);
    check_d("x + y", 105.0, &env);

    /* ueval_ptr gives a stable, writable pointer */
    double *xp = ueval_ptr(&env, "x");
    if (!xp) { printf("FAIL: ueval_ptr returned NULL for bound var\n"); g_fail++; }
    else {
        *xp = 7.0;
        check_d("x", 7.0, &env);
        g_pass++;
    }

    double *missing = ueval_ptr(&env, "nope");
    if (missing != NULL) { printf("FAIL: ueval_ptr should return NULL for unbound var\n"); g_fail++; }
    else g_pass++;

    /* unbound variable is an error */
    check_err("undefined_var", UEVAL_ERR_SYMBOL_NOT_FOUND, &env);
}

/* ===========================================================================
 * Functions
 * ======================================================================== */

static void test_functions(void) {
    ueval_env env; ueval_init(&env);

    ueval_bind_f1(&env, "sqrt", sqrt);
    ueval_bind_f1(&env, "sq", sq);
    ueval_bind_f2(&env, "clamp", clampf);

    check_d("sqrt(16)", 4.0, &env);
    check_d("sq(5)", 25.0, &env);
    check_d("sqrt(sq(3))", 3.0, &env);       /* nested calls */
    check_d("clamp(5, 3)", 3.0, &env);
    check_d("clamp(2, 3)", 2.0, &env);
    check_d("sqrt(4) + sq(2)", 6.0, &env);

    /* re-binding a function name replaces it */
    ueval_bind_f1(&env, "sq", sqrt);
    check_d("sq(16)", 4.0, &env);

    /* unknown function */
    check_err("nope(1)", UEVAL_ERR_SYMBOL_NOT_FOUND, &env);

    /* Missing arguments to a 2-arg function ARE caught. */
    check_err("clamp(5)", UEVAL_ERR_INVALID_ARGS, &env);

    /* QUIRK: extra arguments are NOT caught for either arity. The parser
     * reads the arguments it expects, then if a 1-arg call sees a ','
     * instead of ')' it simply does not advance, and on a 2-arg call any
     * tokens after the second argument up to the next ')' are unconsumed.
     * Either way, no UEVAL_ERR_INVALID_ARGS is raised and the leftover
     * text is silently dropped (see "trailing garbage" note below). */
    check_d("sqrt(16,99)", 4.0, &env);          /* extra arg silently ignored */
    check_d("clamp(5,3,9)", 3.0, &env);         /* extra 3rd arg silently ignored */
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
    ueval_env env; ueval_init(&env);

    check_d("1 + 1 @", 2.0, &env);       /* trailing junk silently dropped */
    check_d("0x", 0.0, &env);            /* "0x" with no hex digits -> 0, no error */
    check_d("0xZZ", 0.0, &env);          /* same: trailing non-hex chars dropped */
    check_d("5 5", 5.0, &env);           /* second literal never reached, no error */
}

/* ===========================================================================
 * Dollar-variable mode
 * ======================================================================== */

static void test_dollar_mode(void) {
    ueval_env env; ueval_init(&env);
    env.dollar_vars = 1;

    ueval_bind(&env, "x", 10.0);
    ueval_bind(&env, "y", 3.0);
    ueval_bind_f1(&env, "sqrt", sqrt);

    check_d("$x", 10.0, &env);
    check_d("$x + $y", 13.0, &env);
    check_d("$x * $y + sqrt($x)", 33.16227766, &env);

    /* functions still called bare, without $ */
    check_d("sqrt($x + 6)", 4.0, &env);

    /* bare identifier (no $) is now an error */
    check_err("x", UEVAL_ERR_SYMBOL_NOT_FOUND, &env);
    check_err("x * 2", UEVAL_ERR_SYMBOL_NOT_FOUND, &env);

    /* turning it back off restores bare-name lookup */
    env.dollar_vars = 0;
    check_d("x * 2", 20.0, &env);
}

/* ===========================================================================
 * Error handling
 * ======================================================================== */

static void test_errors(void) {
    ueval_env env; ueval_init(&env);

    check_err("1 / 0", UEVAL_ERR_DIVISION_BY_ZERO, &env);
    check_err("1 % 0", UEVAL_ERR_DIVISION_BY_ZERO, &env);
    check_err("(1 + 2", UEVAL_ERR_EXPECTED_CLOSE_PAREN, &env);
    check_err("1 ? 2", UEVAL_ERR_EXPECTED_COLON, &env);
    check_err("@", UEVAL_ERR_UNEXPECTED_CHAR, &env);
    check_err("unknown_symbol", UEVAL_ERR_SYMBOL_NOT_FOUND, &env);

    /* error message is populated and human-readable */
    ueval_result r = ueval_eval(&env, "1 / 0");
    if (strlen(r.error_msg) == 0) {
        printf("FAIL: error_msg empty for division by zero\n");
        g_fail++;
    } else {
        g_pass++;
    }

    /* env is reusable after an error - state doesn't get stuck */
    check_d("2 + 2", 4.0, &env);

    /* empty / whitespace-only input evaluates to 0 with no error, rather
     * than e.g. UEVAL_ERR_UNEXPECTED_CHAR - there's no "empty expression"
     * status. Worth knowing if you evaluate user-typed text live. */
    check_d("", 0.0, &env);
    check_d("   ", 0.0, &env);
}

static void test_recursion_guard(void) {
    ueval_env env; ueval_init(&env);

    /* deeply nested parens should hit the depth guard, not crash */
    char expr[4096];
    int n = 0;
    for (int i = 0; i < 600 && n < (int)sizeof(expr) - 10; i++) expr[n++] = '(';
    expr[n++] = '1';
    for (int i = 0; i < 600 && n < (int)sizeof(expr) - 2; i++) expr[n++] = ')';
    expr[n] = '\0';

    ueval_result r = ueval_eval(&env, expr);
    if (r.status != UEVAL_ERR_STACK_OVERFLOW) {
        printf("FAIL: deeply nested expr did not trigger UEVAL_ERR_STACK_OVERFLOW (got status %d)\n", r.status);
        g_fail++;
    } else {
        g_pass++;
    }
}

/* ===========================================================================
 * Env reuse / thread-safety shape (single-threaded sanity check)
 * ======================================================================== */

static void test_multiple_envs_independent(void) {
    ueval_env a, b;
    ueval_init(&a);
    ueval_init(&b);

    ueval_bind(&a, "x", 1.0);
    ueval_bind(&b, "x", 2.0);

    check_d("x", 1.0, &a);
    check_d("x", 2.0, &b);
}

/* ===========================================================================
 * Configuration overrides (UEVAL_MAX_VARS / FUNCS / DEPTH)
 * NOTE: this file does not redefine the macros (ueval.h is shared with other
 * tests in this binary), so this just exercises the default table-full path.
 * ======================================================================== */

static void test_table_full(void) {
    ueval_env env; ueval_init(&env);
    char name[8];
    int rc = 0;
    for (int i = 0; i < UEVAL_MAX_VARS; i++) {
        snprintf(name, sizeof(name), "v%d", i);
        rc = ueval_bind(&env, name, (double)i);
        if (rc != 0) break;
    }
    /* table should now be full */
    rc = ueval_bind(&env, "overflow", 1.0);
    if (rc != -1) {
        printf("FAIL: ueval_bind should return -1 once UEVAL_MAX_VARS is reached\n");
        g_fail++;
    } else {
        g_pass++;
    }
}

/* ===========================================================================
 * main
 * ======================================================================== */

int main(void) {
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

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
