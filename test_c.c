#include "ueval.h"

int main() {
    ueval_env env;
    ueval_init(&env);
    
    // Evaluate an expression with a missing variable
    ueval_result res = ueval_evaluate(&env, "100 / (X + 2)");

    if (res.status != UEVAL_OK) {
        if (res.status == UEVAL_ERR_SYMBOL_NOT_FOUND) {
            printf("Error: Variable '%s' is not defined.\n", res.error_msg);
        } else if (res.status == UEVAL_ERR_DIVISION_BY_ZERO) {
            printf("Error: Cannot divide by zero.\n");
        }
    } else {
        printf("Result: %.2f\n", res.value);
    }

    return 0;
}
