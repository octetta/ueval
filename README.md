# `ueval`: Micro C-Expression Evaluator

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

Unary operators `-`, `!`, `~` are applied before any binary operator.

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

## Memory & Thread Safety

`ueval` uses no dynamic allocation. Every `ueval_env` is fully self-contained, so multiple environments can exist simultaneously in different threads without any locking. Do not share a single `ueval_env` between threads without external synchronisation.
