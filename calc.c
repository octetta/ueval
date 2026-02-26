#include "tiny_readline.h" // Assuming the previous code is here
#include <stdio.h>
#include <string.h>

#include "ueval.h"

int main() {
    ueval_env env;
    ueval_init(&env);

    char input_buffer[MAX_LINE];

    while (1) {
        get_line_compact("# ", input_buffer);

        if (strcmp(input_buffer, "exit") == 0 || strcmp(input_buffer, "q") == 0) {
            break;
        }

        if (strlen(input_buffer) > 0) {
            ueval_result res = ueval_evaluate(&env, input_buffer);
            printf("= %.g\n", res.value);
        }

        // Optional: clear buffer for next iteration
        memset(input_buffer, 0, MAX_LINE);
    }

    return 0;
}
