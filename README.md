# uEval: Micro C-Expression Evaluator

**uEval** is a standalone, thread-safe, header-only C library for evaluating mathematical and logical expressions. It uses a recursive-descent parser to support hexadecimal literals, bitwise operations, C-style logical operators, and custom C-function bindings.

## 🚀 Key Features

* **Header-Only**: Drop `ueval.h` into your project and go.
* **Thread-Safe**: No global state; all context is maintained in an `ueval_env` struct.
* **C-Standard Precedence**: Logic and bitwise operators behave exactly like native C.
* **Zero Dependencies**: Uses only standard C headers (`stdio.h`, `math.h`, etc.).
* **Flexible Binding**: Map C variables and functions (1 or 2 parameters) to the engine.

---

## 🧮 Precedence Ladder

uEval follows the standard C hierarchy to ensure complex logic evaluates correctly.

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

### Error Handling
The `ueval_result` struct returns:
* `double value`: The final result of the evaluation.
* `ueval_status status`: `UEVAL_OK`, `UEVAL_ERR_SYMBOL_NOT_FOUND`, etc.
* `char error_msg[64]`: The specific symbol name or operator that triggered the error.

---

## 💡 Examples

### 1. Simple Math & Hex
```c
ueval_env env;
ueval_init(&env);
ueval_result res = ueval_evaluate(&env, "(0xFF >> 4) + 10.5");
// res.value = 25.5
```

### 2. Logic & Feature Flags
```c
ueval_bind(&env, "VERSION", 2.0);
ueval_bind(&env, "ENABLED", 1.0);

ueval_result res = ueval_evaluate(&env, "VERSION >= 2 && ENABLED == 1");
// res.value = 1.0 (True)
```

### 3. Custom Function Binding
```c
double my_gain(double signal, double boost) { return signal * boost; }

// ... inside main ...
ueval_bind_f1(&env, "sqrt", sqrt);
ueval_bind_f2(&env, "gain", my_gain);

ueval_result res = ueval_evaluate(&env, "gain(sqrt(16), 1.5)");
// res.value = 6.0
```

### 4. Handling Errors
```c
ueval_result res = ueval_evaluate(&env, "10 / UNDEFINED_VAR");

if (res.status != UEVAL_OK) {
    printf("Eval Error: %s (Status: %d)\n", res.error_msg, res.status);
}
```
