#ifndef UEVAL_EVAL_H
#define UEVAL_EVAL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define UEVAL_MAX_BINDS 32

typedef enum {
    UEVAL_OK = 0,
    UEVAL_ERR_SYMBOL_NOT_FOUND,
    UEVAL_ERR_EXPECTED_PAREN,
    UEVAL_ERR_INVALID_SYNTAX,
    UEVAL_ERR_DIVISION_BY_ZERO
} ueval_status;

typedef struct {
    double value;
    ueval_status status;
    char error_msg[64];
} ueval_result;

typedef double (*ueval_func1)(double);
typedef double (*ueval_func2)(double, double);

typedef struct { char name[32]; double value; } ueval_binding;
typedef struct { char name[32]; ueval_func1 f1; ueval_func2 f2; int args; } ueval_fbind;

typedef struct {
    const char *ptr;
    ueval_binding binds[UEVAL_MAX_BINDS];
    ueval_fbind funcs[UEVAL_MAX_BINDS];
    int b_count;
    int f_count;
    ueval_status last_err;
    char err_info[64];
} ueval_env;

static double _ueval_parse_expr(ueval_env *env, int priority);

static void _ueval_set_err(ueval_env *env, ueval_status s, const char *info) {
    if (env->last_err == UEVAL_OK) {
        env->last_err = s;
        strncpy(env->err_info, info, 63);
    }
}

static double _ueval_get_token(ueval_env *env) {
    while (isspace(*env->ptr)) env->ptr++;
    if (*env->ptr == '\0') return 0;

    if (*env->ptr == '-') {
        env->ptr++;
        return -_ueval_get_token(env);
    }

    if (*env->ptr == '(') {
        env->ptr++;
        double v = _ueval_parse_expr(env, 0);
        while (isspace(*env->ptr)) env->ptr++;
        if (*env->ptr == ')') env->ptr++;
        else _ueval_set_err(env, UEVAL_ERR_EXPECTED_PAREN, ")");
        return v;
    }

    if (isalpha(*env->ptr) || *env->ptr == '_') {
        const char *start = env->ptr;
        while (isalnum(*env->ptr) || *env->ptr == '_') env->ptr++;
        int len = env->ptr - start;
        char name[32] = {0}; strncpy(name, start, len > 31 ? 31 : len);

        for (int i = 0; i < env->f_count; i++) {
            if (strcmp(name, env->funcs[i].name) == 0) {
                while (isspace(*env->ptr)) env->ptr++;
                if (*env->ptr == '(') {
                    env->ptr++;
                    double a1 = _ueval_parse_expr(env, 0);
                    if (env->funcs[i].args == 1) {
                        while (isspace(*env->ptr)) env->ptr++;
                        if (*env->ptr == ')') env->ptr++;
                        return env->funcs[i].f1(a1);
                    } else {
                        if (*env->ptr == ',') env->ptr++;
                        double a2 = _ueval_parse_expr(env, 0);
                        while (isspace(*env->ptr)) env->ptr++;
                        if (*env->ptr == ')') env->ptr++;
                        return env->funcs[i].f2(a1, a2);
                    }
                }
            }
        }
        for (int i = 0; i < env->b_count; i++) {
            if (strcmp(name, env->binds[i].name) == 0) return env->binds[i].value;
        }
        _ueval_set_err(env, UEVAL_ERR_SYMBOL_NOT_FOUND, name);
        return 0;
    }

    if (isdigit(*env->ptr)) {
        char *end;
        double v = (env->ptr[0] == '0' && (env->ptr[1] == 'x' || env->ptr[1] == 'X')) 
                   ? (double)strtoll(env->ptr, &end, 0) : strtod(env->ptr, &end);
        env->ptr = end;
        return v;
    }
    return 0;
}

static double _ueval_parse_expr(ueval_env *env, int priority) {
    double left = _ueval_get_token(env);
    while (1) {
        while (isspace(*env->ptr)) env->ptr++;
        char op = *env->ptr;
        if (!op || op == ')' || op == '}' || op == ';' || op == ',') break;

        int p = 0, step = 1;
        if (op == '*' || op == '/' || op == '%') p = 7;
        else if (op == '+' || op == '-') p = 6;
        else if (op == '<' && env->ptr[1] == '<') { p = 5; step = 2; }
        else if (op == '>' && env->ptr[1] == '>') { p = 5; step = 2; }
        else if (op == '>' || op == '<') p = 4;
        else if (op == '=' && env->ptr[1] == '=') { p = 3; step = 2; }
        else if (op == '!' && env->ptr[1] == '=') { p = 3; step = 2; }
        else if (op == '&' && env->ptr[1] == '&') { p = 2; step = 2; }
        else if (op == '|' && env->ptr[1] == '|') { p = 1; step = 2; }
        else if (op == '&' || op == '^' || op == '|') p = 3;

        if (p <= priority) break;
        env->ptr += step;
        double right = _ueval_parse_expr(env, p);
        if (op == '+') left += right;
        else if (op == '-') left -= right;
        else if (op == '*') left *= right;
        else if (op == '/') { if(right == 0) _ueval_set_err(env, UEVAL_ERR_DIVISION_BY_ZERO, "/"); else left /= right; }
        else if (op == '%') left = (double)((long long)left % (long long)right);
        else if (op == '&' && step == 1) left = (double)((long long)left & (long long)right);
        else if (op == '|' && step == 1) left = (double)((long long)left | (long long)right);
        else if (op == '^') left = (double)((long long)left ^ (long long)right);
        else if (op == '<' && step == 2) left = (double)((long long)left << (long long)right);
        else if (op == '>' && step == 2) left = (double)((long long)left >> (long long)right);
        else if (op == '=' && step == 2) left = (left == right);
        else if (op == '!' && step == 2) left = (left != right);
        else if (op == '&' && step == 2) left = (left != 0 && right != 0);
        else if (op == '|' && step == 2) left = (left != 0 || right != 0);
        else if (op == '>') left = (left > right);
        else if (op == '<') left = (left < right);
    }
    return left;
}

void ueval_init(ueval_env *env) { env->b_count = 0; env->f_count = 0; env->ptr = NULL; env->last_err = UEVAL_OK; }
void ueval_bind(ueval_env *env, const char *name, double val) {
    if (env->b_count < UEVAL_MAX_BINDS) { strncpy(env->binds[env->b_count].name, name, 31); env->binds[env->b_count++].value = val; }
}
void ueval_bind_f1(ueval_env *env, const char *name, ueval_func1 f) {
    if (env->f_count < UEVAL_MAX_BINDS) { strncpy(env->funcs[env->f_count].name, name, 31); env->funcs[env->f_count].f1 = f; env->funcs[env->f_count++].args = 1; }
}
void ueval_bind_f2(ueval_env *env, const char *name, ueval_func2 f) {
    if (env->f_count < UEVAL_MAX_BINDS) { strncpy(env->funcs[env->f_count].name, name, 31); env->funcs[env->f_count].f2 = f; env->funcs[env->f_count++].args = 2; }
}

ueval_result ueval_evaluate(ueval_env *env, const char *expr) {
    env->ptr = expr;
    env->last_err = UEVAL_OK;
    memset(env->err_info, 0, 64);
    
    ueval_result res;
    res.value = _ueval_parse_expr(env, 0);
    res.status = env->last_err;
    strncpy(res.error_msg, env->err_info, 63);
    return res;
}

#endif
