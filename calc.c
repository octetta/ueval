#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "ueval.h"
#include "uedit.h"

// Returns 0 on success, -1 if no = or more than one =
int split_and_trim(char *str, char **left, char **right)
{
    if (!str || !left || !right) return -1;

    *left = *right = NULL;

    // Find the first =
    char *eq = strchr(str, '=');
    if (!eq) return -1;

    // Check there's no second =
    if (strchr(eq + 1, '=')) return -1;

    // Split
    *eq = '\0';           // destroy original string
    *left  = str;
    *right = eq + 1;

    // Trim left part (from right)
    char *end = *left + strlen(*left) - 1;
    while (end >= *left && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }

    // Trim right part (from left)
    while (**right && isspace((unsigned char)**right)) {
        (*right)++;
    }

    // Trim right part (from right) - optional but usually wanted
    if (**right) {
        end = *right + strlen(*right) - 1;
        while (end >= *right && isspace((unsigned char)*end)) {
            *end = '\0';
            end--;
        }
    }

    // If after trimming either side became empty string → you may want to reject
    // (optional - comment out if you want to allow empty values)
    if (**left == '\0' || **right == '\0') {
        return -1;
    }

    return 0;
}

int main() {
    ueval_env env;
    ueval_init(&env);

    char line[1024];

    while (1) {
        int r = uedit("# ", line, sizeof(line));
        if (r < 0) {
          puts("");
          break;
        }

        if (strcmp(line, "exit") == 0 || strcmp(line, "q") == 0) {
            break;
        }

        if (line[0] == '\\') {
          char *left;
          char *right;
          int r = split_and_trim(&line[1], &left, &right);
          if (r == 0) {
            double val = strtod(right, NULL);
            ueval_bind(&env, left, val);
          }
          //printf("%d = %s %s\n", r, left, right);
        }

        if (strlen(line) > 0) {
            ueval_result res = ueval_evaluate(&env, line);
            printf("= %g\n", res.value);
        }

        // Optional: clear buffer for next iteration
        memset(line, 0, sizeof(line));
    }

    return 0;
}
