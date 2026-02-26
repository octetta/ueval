#include "ueval.h"

int main() {
    ueval_env env;
    ueval_init(&env);
    ueval_bind_f1(&env, "abs", fabs);
    ueval_bind(&env, "X", 10.0);

    // Test Unary Minus and Nested Functions
    ueval_result res = ueval_evaluate(&env, "-abs(-X * 2)");
    
    printf("Result: %.1f\n", res.value); // -20.0
    return 0;
}
