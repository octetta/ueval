#include "ueval.h"

// Example custom function
double my_gain(double val, double boost) { return val * boost; }

int main() {
    ueval_env env;
    ueval_init(&env);

    // Bind Standard Lib
    ueval_bind_f1(&env, "sin", sin);
    ueval_bind_f1(&env, "sqrt", sqrt);
    
    // Bind Custom 2-param function
    ueval_bind_f2(&env, "gain", my_gain);
    
    // Bind variable
    ueval_bind(&env, "X", 16.0);

    ueval_result res = ueval_evaluate(&env, "gain(sqrt(X), 2.0) + sin(0)");
    printf("Result: %.2f\n", res.value); // (sqrt(16)*2) + 0 = 8.00
    return 0;
}
