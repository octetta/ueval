# uEval: Micro C-Expression Evaluator

**uEval** is a standalone, thread-safe, header-only C library for evaluating mathematical and logical expressions. It uses a recursive-descent parser to support hexadecimal literals, bitwise operations, C-style logical operators, and custom C-function bindings.

## 🚀 Key Features

* **Header-Only**: Drop `ueval.h` into your project and go.
* **Thread-Safe**: No global state; all context is maintained in an `ueval_env` struct.
* **C-Standard Precedence**: Logic and bitwise operators behave exactly like native C.
* **Parentheses & Nesting**: Full support for grouping and nested function calls.
* **Recursion Guard**: Built-in depth counter to prevent Stack Overflow.
* **Zero Dependencies**: Uses only standard C headers (`stdio.h`, `math.h`, etc.).

---

## 🧮 Precedence Ladder

uEval follows the standard C hierarchy. Parentheses `()` always have the highest priority.

| Priority | Operators | Description | Type |
| :--- | :--- | :--- | :--- |
| **8** | `-` | Unary Minus (Negation) | Unary |
| **7** | `*`, `/`, `%` | Multiplication, Division, Modulo | Math |
| **6** | `+`, `-` | Addition, Subtraction | Math |
| **5** | `<<`, `>>` | Bitwise Left/Right Shift | Bitwise |
| **4** | `<`, `>`, `<=`, `>=` | Relational Comparisons | Comparison |
| **3** | `==`, `!=` | Equality / Inequality | Comparison |
| **3** | `&`, `^`, `\|` | Bitwise AND, XOR, OR | Bitwise |
| **2** | `&&` | Logical AND | Logical |
| **1** | `\|\|` | Logical OR | Logical |

---

## 🛠 API Reference

### Environment Lifecycle
* `void ueval_init(ueval_env *env)`: Initializes the environment and resets error states.
* `ueval_result ueval_evaluate(ueval_env *env, const char *expr)`: Parses and executes the expression.

### Binding C Objects
* `void ueval_bind(ueval_env *env, const char *name, double val)`: Binds a C double to a variable name.
* `void ueval_bind_f1(ueval_env *env, const char *name, ueval_func1 f)`: Binds a 1-parameter C function (e.g., `sin`).
* `void ueval_bind_f2(ueval_env *env, const char *name, ueval_func2 f)`: Binds a 2-parameter C function (e.g., `pow`).

---

## 💡 Examples

### 1. Function Binding (Standard & Custom)



```c
#include "ueval.h"
#include <math.h>

// A custom 2-parameter function
double my_mix(double a, double b) { return (a + b) * 0.5; }

int main() {
    ueval_env env;
    ueval_init(&env);

    // Bind standard math.h functions (1-param)
    ueval_bind_f1(&env, "sin", sin);
    ueval_bind_f1(&env, "sqrt", sqrt);

    // Bind standard math.h functions (2-param)
    ueval_bind_f2(&env, "pow", pow);

    // Bind custom function
    ueval_bind_f2(&env, "mix", my_mix);

    // Use them in an expression
    ueval_result res = ueval_evaluate(&env, "mix(sqrt(16), pow(2, 3)) + sin(0)");
    // sqrt(16)=4, pow(2,3)=8 -> mix(4, 8) = 6.0
    printf("Result: %.1f\n", res.value); 

    return 0;
}
```

### 2. Logical Nesting with Variables
```c
ueval_bind(&env, "VOL", 0.75);
ueval_bind(&env, "MAX", 1.0);

// Use parentheses to group logic
ueval_result res = ueval_evaluate(&env, "(VOL < MAX) && (VOL > 0.5)");
// res.value = 1.0 (True)
```

### 3. Handling Errors
```c
ueval_result res = ueval_evaluate(&env, "10 / 0");

if (res.status == UEVAL_ERR_DIVISION_BY_ZERO) {
    printf("Math Error: Division by zero is not allowed.\n");
}
```

### 4. Memory Management
**uEval** is designed with a zero-allocation policy. It does not use `malloc`, `free`, or any other heap-related functions. 
* All state is held within the `ueval_env` struct.
* To clear the environment, simply call `ueval_init` again.
* No `ueval_free` is required.
