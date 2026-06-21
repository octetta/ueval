# `ueval`: Micro C-Expression Evaluator

[![CodeFactor](https://www.codefactor.io/repository/github/octetta/ueval/badge)](https://www.codefactor.io/repository/github/octetta/ueval)

`ueval` is a standalone, thread-safe, header-only C library for evaluating mathematical and logical expressions. It uses a precedence-climbing parser to support hexadecimal literals, bitwise operations, C-style logical operators, short-circuit ternary evaluation, and custom C-function bindings.

## Features

* **Header-Only**: Drop `ueval.h` into your project and go.
* **Thread-Safe**: No global state; all context is in a `ueval_env` struct.
* **C-Standard Precedence**: Operators behave exactly like native C.
* **Short-Circuit Ternary**: `cond ? a : b` skips the untaken branch entirely — so `x != 0 ? 1/x : 0` is safe even when `x` is zero.
* **Re-bindable Variables**: Calling `ueval_bind` again with the same name updates the value rather than creating a duplicate.
* **Unary Operators**: `-` (negate), `!` (logical NOT), `~` (bitwise NOT).
* **Power Operator**: `**` (right-associative, backed by `pow()`).
* **Recursion Guard**: Built-in depth counter prevents stack overflow.
* **Dollar-Variable Mode**: Opt-in `$name` syntax for variables — useful when bare identifiers conflict with surrounding systems.

---

## Quick Start

```c
#include "ueval.h"
#include <math.h>
#include <stdio.h>

int main(void) {
    ueval_env env;
    ueval_init(&env);

    ueval_bind(&env, "x", 2.0);
    ueval_bind_f1(&env, "sqrt", sqrt);

    ueval_result r = ueval_eval(&env, "sqrt(x) + x**3");
    if (r.status == UEVAL_OK)
        printf("%g\n", r.value); /* 9.41421 */
    else
        fprintf(stderr, "error: %s\n", r.error_msg);
}
```

---

## How Evaluation Works

`ueval` parses with a single-pass **precedence-climbing (Pratt-style) parser** — there is no separate tokenization pass, no AST, and no dynamic allocation. `ueval_eval` walks the input string once, character by character, and produces a `double` directly.

A few consequences of that design are worth knowing up front:

- **All values are `double`.** There is no integer type. Bitwise and shift operators (`& | ^ ~ << >>`) cast their operands to `long long` for the operation, then cast the result back to `double`. This is exact for the range of integers a `double` can represent exactly (up to 2^53), and matches what you'd expect from C, but it means very large bit patterns can lose precision.
- **Truthiness is C-style.** `0` is false, anything else (including negative numbers) is true. Logical operators (`&& || !`) always *return* `0.0` or `1.0`.
- **The environment is reset on every call.** `ueval_eval` clears the error state and recursion depth at the start of each call, so a single `ueval_env` can be reused indefinitely — including after an error — without needing to call `ueval_init` again.
- **Parsing stops at the first thing it can't consume — it does not require consuming the whole string.** This is the single most important quirk to know about; see [Known Quirks](#known-quirks).
- **Errors are "first wins."** Once `env->err` is set during a parse, no further parsing work overwrites it — the first error encountered is the one reported, even if other problems exist later in the string.

---

## Operator Precedence

Parentheses `()` always have the highest priority. The table below lists binary/unary operators from highest to lowest precedence.

| Prec | Operators           | Description                     | Notes              |
| :--- | :------------------ | :------------------------------ | :----------------- |
| 9    | `**`                | Power                           | Right-associative  |
| 8    | `*`  `/`  `%`       | Multiply, Divide, Modulo        |                    |
| 7    | `+`  `-`            | Add, Subtract                   |                    |
| 6    | `<<`  `>>`          | Bitwise Shift                   |                    |
| 5    | `<`  `>`  `<=`  `>=`| Relational Comparison           |                    |
| 4    | `==`  `!=`          | Equality                        |                    |
| 3    | `&`  `^`  `\|`      | Bitwise AND, XOR, OR            |                    |
| 2    | `&&`                | Logical AND                     |                    |
| 1    | `\|\|`              | Logical OR                      |                    |
| 0    | `? :`               | Ternary Conditional             | Short-circuits     |

Unary operators `-`, `!`, `~` are applied before any binary operator, and bind *tighter* than `**` — see [Known Quirks](#known-quirks) below, `-2**2` is `4`, not `-4`.

---

## Comparison Operators

`ueval` supports the full set of C-style relational and equality operators. All six return `1.0` for true or `0.0` for false — there's no separate boolean type, just `double`.

| Operator | Meaning                  | Precedence | Associativity |
| :------- | :------------------------ | :--------: | :------------ |
| `<`      | Less than                 | 5          | Left           |
| `>`      | Greater than               | 5          | Left           |
| `<=`     | Less than or equal to     | 5          | Left           |
| `>=`     | Greater than or equal to  | 5          | Left           |
| `==`     | Equal to                  | 4          | Left           |
| `!=`     | Not equal to               | 4          | Left           |

```c
ueval_eval(&env, "3 < 5");    /* 1.0 */
ueval_eval(&env, "5 < 3");    /* 0.0 */
ueval_eval(&env, "5 > 3");    /* 1.0 */
ueval_eval(&env, "3 > 5");    /* 0.0 */
ueval_eval(&env, "5 <= 5");   /* 1.0 */
ueval_eval(&env, "4 <= 5");   /* 1.0 */
ueval_eval(&env, "6 <= 5");   /* 0.0 */
ueval_eval(&env, "5 >= 5");   /* 1.0 */
ueval_eval(&env, "5 >= 3");   /* 1.0 */
ueval_eval(&env, "3 >= 5");   /* 0.0 */
ueval_eval(&env, "5 == 5");   /* 1.0 */
ueval_eval(&env, "5 == 4");   /* 0.0 */
ueval_eval(&env, "5 != 4");   /* 1.0 */
ueval_eval(&env, "5 != 5");   /* 0.0 */
```

A few things worth knowing:

- **Comparisons operate on `double`, including equality.** `==` and `!=` do a direct floating-point comparison with no epsilon/tolerance, so the result of any computation feeding into one should be an exact value (e.g. integers, or values built only from `+`/`-`/`*` on exact inputs) if you want reliable equality checks. `0.1 + 0.2 == 0.3` is `0.0`, just as in native C, because of standard floating-point representation error.
- **`<` / `>` / `<=` / `>=` sit one precedence level below shifts and one above equality.** `1 << 2 < 8` parses as `(1 << 2) < 8`, and `a < b == c < d` parses as `(a < b) == (c < d)`.
- **Comparisons chain left-to-right like any other same-precedence binary operator, not like Python.** `1 < 2 < 3` does **not** mean `(1 < 2) && (2 < 3)`. It parses as `(1 < 2) < 3`, i.e. `1.0 < 3`, which is `1.0`. Use `(1 < 2) && (2 < 3)` if you want the chained-comparison meaning.
- **Combine with `&&` / `||` for compound conditions**, since `&&`/`||` sit at lower precedence and don't need extra parens around simple comparisons: `x > 0 && x < 10` works as expected.
- **Comparisons compose naturally with the ternary operator**: `x >= 18 ? 1 : 0` is the idiomatic way to turn a comparison into an explicit value, though the comparison's own `1.0`/`0.0` result usually makes the ternary redundant.

```c
/* compound condition, no extra parens needed */
ueval_bind(&env, "x", 5.0);
ueval_eval(&env, "x > 0 && x < 10");   /* 1.0 */

/* chained comparison pitfall */
ueval_eval(&env, "1 < 2 < 3");          /* 1.0 — parses as (1 < 2) < 3, not chained */
ueval_eval(&env, "(1 < 2) && (2 < 3)"); /* 1.0 — the idiomatic chained-comparison form */
```

---

## API Reference

### Lifecycle

```c
void ueval_init(ueval_env *env);
```
Initialises the environment. Call once before anything else. To reset and reuse, call again.

```c
ueval_result ueval_eval(ueval_env *env, const char *expr);
```
Parses and evaluates `expr`. Returns a `ueval_result` with:
- `.value`     — the computed `double` (0 on error)
- `.status`    — `UEVAL_OK` or an error code
- `.error_msg` — human-readable description (empty string on success)

### Binding Variables

```c
int ueval_bind(ueval_env *env, const char *name, double value);
```
Binds `name` to `value`. If `name` already exists, its value is updated. Returns `0` on success, `-1` if the variable table is full (`UEVAL_MAX_VARS`, default 32).

```c
double *ueval_ptr(ueval_env *env, const char *name);
```
Returns a pointer to the stored value of a bound variable, or `NULL` if not found. The pointer is stable for the lifetime of the env. Use it to read or update the variable directly on the hot path — no function call needed after the initial lookup:

```c
ueval_bind(&env, "vel", 0.0);
double *vel = ueval_ptr(&env, "vel");
// later, in a MIDI callback or tight loop:
*vel = incoming_value;
ueval_eval(&env, expr);
```

### Binding Functions

```c
int ueval_bind_f1(ueval_env *env, const char *name, ueval_func1 f);
int ueval_bind_f2(ueval_env *env, const char *name, ueval_func2 f);
```
Binds a 1- or 2-argument C function. Replaces an existing binding with the same name. Returns `0` on success, `-1` if the function table is full (`UEVAL_MAX_FUNCS`, default 32).

### Error Codes

| Code | Meaning |
| :--- | :------ |
| `UEVAL_OK` | Success |
| `UEVAL_ERR_SYMBOL_NOT_FOUND` | Unbound variable or function name |
| `UEVAL_ERR_EXPECTED_CLOSE_PAREN` | Missing `)` |
| `UEVAL_ERR_EXPECTED_COLON` | Missing `:` in ternary |
| `UEVAL_ERR_INVALID_ARGS` | Wrong number of arguments to a function |
| `UEVAL_ERR_DIVISION_BY_ZERO` | Division or modulo by zero |
| `UEVAL_ERR_STACK_OVERFLOW` | Expression exceeds `UEVAL_MAX_DEPTH` (default 64) |
| `UEVAL_ERR_UNEXPECTED_CHAR` | Unrecognised character in input |

---

## Configuration

Define these before including `ueval.h` to override the defaults:

```c
#define UEVAL_MAX_VARS  64   /* default: 32 */
#define UEVAL_MAX_FUNCS 16   /* default: 32 */
#define UEVAL_MAX_DEPTH 32   /* default: 64 */
#include "ueval.h"
```

---

## Examples

### Ternary with Short-Circuit (Safe Division)

The false branch is never evaluated when the condition is true, and vice versa.

```c
ueval_bind(&env, "x", 0.0);
ueval_result r = ueval_eval(&env, "x != 0 ? 1/x : -1");
/* r.value = -1.0 — no division-by-zero error */
```

### Direct Variable Access (Hot Path)

Bind once during setup, then get a stable pointer with `ueval_ptr` and update it directly — no function call overhead on the hot path.

```c
ueval_bind(&env, "vel", 0.0);
ueval_bind(&env, "cc7", 0.0);
double *vel = ueval_ptr(&env, "vel");
double *cc7 = ueval_ptr(&env, "cc7");

// In a real-time MIDI callback:
*vel = msg.velocity / 127.0;
*cc7 = msg.cc_value / 127.0;
ueval_result r = ueval_eval(&env, expr);
```

### Re-binding a Variable

```c
ueval_bind(&env, "t", 0.0);
for (int i = 0; i < 10; i++) {
    ueval_bind(&env, "t", (double)i);   /* updates in place */
    ueval_result r = ueval_eval(&env, "t * t + 2*t + 1");
    printf("%g\n", r.value);
}
```

### Bitwise / Hex Literals

```c
/* Check if bit 0 is set, return 0xAA or 0xBB accordingly */
ueval_result r = ueval_eval(&env, "(0xFF & 0x01) ? 0xAA : 0xBB");
/* r.value = 170.0 (0xAA) */
```

### Unary Operators

```c
ueval_bind(&env, "flag", 0.0);
ueval_eval(&env, "!flag");   /* 1.0 — logical NOT  */
ueval_eval(&env, "~0xF0");   /* bitwise NOT        */
ueval_eval(&env, "-(2+3)");  /* -5.0               */
```

Consecutive `!` characters are collapsed into one toggle (XOR'd) before being applied — they are **not** applied value-by-value like idiomatic C. An even count cancels out completely:

```c
ueval_eval(&env, "!!5");     /* 5.0  — NOT 1.0; two '!' cancel out      */
ueval_eval(&env, "!!!5");    /* 0.0  — odd count behaves like one '!'   */
```

Unary minus also binds *tighter* than `**`, so it applies to the base before the power is taken:

```c
ueval_eval(&env, "-2**2");    /* 4.0  — parses as (-2)**2, not -(2**2) */
ueval_eval(&env, "-(2**2)");  /* -4.0 — use explicit parens for this   */
```

### Power Operator

```c
ueval_eval(&env, "2**10");       /* 1024.0 */
ueval_eval(&env, "2**2**3");     /* 256.0  — right-associative: 2**(2**3) */
```

### Handling Errors

```c
ueval_result r = ueval_eval(&env, "1 / 0");
if (r.status != UEVAL_OK)
    fprintf(stderr, "error: %s\n", r.error_msg);
/* error: division by zero: '/' */
```

### Dollar-Variable Mode

Set `env.dollar_vars = 1` to require variables to be written as `$name`. Functions are unaffected and are still called without a prefix.

```c
ueval_env env;
ueval_init(&env);
env.dollar_vars = 1;

ueval_bind(&env, "x", 10.0);
ueval_bind(&env, "y", 3.0);
ueval_bind_f1(&env, "sqrt", sqrt);

ueval_result r = ueval_eval(&env, "$x * $y + sqrt($x)");
/* r.value = 33.162 */
```

In this mode, a bare identifier that isn't a function call is an error:

```c
ueval_result r = ueval_eval(&env, "x * 2"); /* forgot the $ */
/* r.status = UEVAL_ERR_SYMBOL_NOT_FOUND, r.error_msg = "symbol not found: '$x'" */
```

The error message includes the `$` hint so the cause is immediately obvious.

---

### Custom Two-Argument Function

```c
double clamp(double v, double hi) { return v > hi ? hi : v; }

ueval_bind_f2(&env, "clamp", clamp);
ueval_bind(&env, "VOL", 1.2);
ueval_result r = ueval_eval(&env, "clamp(VOL, 1.0)");
/* r.value = 1.0 */
```

---

## Known Quirks

These behaviors were verified directly against the current `ueval.h` source while building out the test suite. None of them crash or corrupt state — they're silent semantic surprises, which is exactly why they're worth documenting and pinning down with tests.

> **Note:** an earlier version of this README documented a bug where `<=` and `>=` silently evaluated as `<<` and `>>` (the dispatch switch used token length alone to mean "this is a shift," and `<=`/`>=` are also 2 characters). This has been **fixed** — the operator's second character is now captured before the recursive right-hand-side parse and used to correctly disambiguate `<<` from `<=`, and `>>` from `>=`. `5 <= 5` now correctly evaluates to `1`. The test suite asserts the corrected behavior.

### Extra function arguments are silently accepted

`UEVAL_ERR_INVALID_ARGS` is correctly raised when a 2-argument function is called with *too few* arguments. It is **not** raised when either a 1- or 2-argument function is called with *too many*:

```c
ueval_eval(&env, "sqrt(16, 99)");   /* 4.0 — extra arg ", 99" silently ignored   */
ueval_eval(&env, "clamp(5, 3, 9)"); /* 3.0 — extra arg ", 9" silently ignored    */
ueval_eval(&env, "clamp(5)");       /* error: UEVAL_ERR_INVALID_ARGS (too few)  */
```

### Trailing garbage after a valid expression is never reported

`ueval_eval` does not verify that parsing consumed the whole input string. As soon as the precedence-climbing loop hits something it can't interpret as an operator, it simply returns whatever value it has computed so far:

```c
ueval_eval(&env, "1 + 1 @");  /* 2.0 — the '@' is silently dropped, no error */
ueval_eval(&env, "5 5");      /* 5.0 — second literal is never reached       */
ueval_eval(&env, "0xZZ");     /* 0.0 — "0x" parses as 0, "ZZ" is dropped     */
```

If you're evaluating live user-typed input (e.g. a REPL or text field), don't rely on `UEVAL_OK` to mean "the whole string was meaningful" — it only means "the prefix we managed to parse didn't hit an error."

### Empty and whitespace-only input is `0.0`, not an error

```c
ueval_eval(&env, "");    /* 0.0, status = UEVAL_OK */
ueval_eval(&env, "   "); /* 0.0, status = UEVAL_OK */
```

There's no dedicated "empty expression" error code — an empty string is treated the same as a literal `0`.

---

## Testing

A standalone test suite (`test_ueval.c`) exercises literals, every operator and its precedence, both ternary branches' short-circuit behavior, variable rebinding, `ueval_ptr`, function binding/arity, dollar-variable mode, every error code, the recursion guard, and all of the quirks above — each quirk is asserted against its *actual* current behavior so a regression (or an upstream fix that changes behavior) shows up immediately as a failing test rather than silently.

```sh
make test
```

or directly:

```sh
gcc -Wall -Wextra test_ueval.c -o test_ueval -lm
./test_ueval
```

It has no dependencies beyond the standard library and `ueval.h` itself, exits `0` on success / `1` on any failure, and is suitable as a CI gate.

---

## Memory & Thread Safety

`ueval` uses no dynamic allocation. Every `ueval_env` is fully self-contained, so multiple environments can exist simultaneously in different threads without any locking. Do not share a single `ueval_env` between threads without external synchronisation.
