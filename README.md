# `ueval`: Micro C-Expression Evaluator

**uEval** is a standalone, thread-safe, header-only C library for evaluating mathematical and logical expressions. It uses a recursive-descent parser to support hexadecimal literals, bitwise operations, C-style logical operators, and custom C-function bindings.

## y Features

* **Header-Only**: Drop `ueval.h` into your project and go.
* **Thread-Safe**: No global state; all context is maintained in an `ueval_env` struct.
* **C-Standard Precedence**: Logic and bitwise operators behave exactly like native C.
* **Ternary Operator**: Supports `cond ? true : false` for inline branching.
* **Parentheses & Nesting**: Full support for grouping and nested function calls.
* **Recursion Guard**: Built-in depth counter to prevent Stack Overflow.

---

## ecedence Ladder

`ueval` follows the standard C hierarchy. Parentheses `()` always have the highest priority.

| Priority | Operators | Description | Type |
| :--- | :--- | :--- | :--- |
| **8** | `-` | Unary Minus (Negation) | Unary |
| **7** | `*`, `/`, `%` | Multiplication, Division, Modulo | Math |
| **6** | `+`, `-` | Addition, Subtraction | Math |
| **5** | `<<`, `>>` | Bitwise Left/Right Shift | Bitwise |
| **4** | `<`, `>`, `<=`, `>=` | Relational Comparisons | Comparison |
| **3** | `==`, `!=`, `&`, `^`, `\|` | Equality, Bitwise logic | Mixed |
| **2** | `&&` | Logical AND | Logical |
| **1** | `\|\|` | Logical OR | Logical |
| **0** | `? :` | Ternary Conditional | Ternary |

---

## I Reference

### Environment Lifecycle
* `void ueval_init(ueval_env *env)`: Initializes the environment and resets error states.
* `ueval_result ueval_evaluate(ueval_env *env, const char *expr)`: Parses and executes the expression.

### Binding C Objects
* `void ueval_bind(ueval_env *env, const char *name, double val)`: Binds a C double to a variable name.
* `void ueval_bind_f1(ueval_env *env, const char *name, ueval_func1 f)`: Binds a 1-parameter C function (e.g., `sin`).
* `void ueval_bind_f2(ueval_env *env, const char *name, ueval_func2 f)`: Binds a 2-parameter C function (e.g., `pow`).

---

## amples

### 1. Complex Expression (Nested Functions & Ternary)
This example shows a volume-clamping logic using a custom `clamp` style ternary.

```c
#include "ueval.h"
#include <math.h>

int main() {
    ueval_env env;
    ueval_init(&env);

    ueval_bind(&env, "VOL", 1.2);
    ueval_bind(&env, "LIMIT", 1.0);
    ueval_bind_f1(&env, "sin", sin);

    // If VOL exceeds LIMIT, return LIMIT, else return VOL * sin(0.5)
    const char *expr = "VOL > LIMIT ? LIMIT : VOL * sin(0.5)";
    
    ueval_result res = ueval_evaluate(&env, expr);
    if (res.status == UEVAL_OK) {
        printf("Result: %.2f\n", res.value); // Result: 1.00
    }
    return 0;
}
```

### 2. Logical Branching with Hex Literals
```c
// Check if a bit is set using bitwise AND, then return a specific hex value
ueval_result res = ueval_evaluate(&env, "(0xFF & 0x01) ? 0xAA : 0xBB");
// res.value = 170.0 (0xAA)
```

### 3. Handling Errors (Missing Colon)
```c
ueval_result res = ueval_evaluate(&env, "1 > 0 ? 100"); // Missing :
if (res.status == UEVAL_ERR_EXPECTED_COLON) {
    printf("Syntax Error: %s\n", res.error_msg); 
}
```

### 4. Memory Management
`ueval` is designed with a zero-allocation policy. It does not use `malloc`, `free`, or any other heap-related functions. 
* All state is held within the `ueval_env` struct.
* To clear the environment, simply call `ueval_init` again.
* No `ueval_free` is required.
