#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// forward declaration needed because Expr contains pointers to itself
typedef struct Expr Expr;
struct Expr {
    enum { NumC, IdC, IfC, LamC, AppC } type;
    union {
        double num;
        char *var;
        struct { Expr *test, *then_e, *else_e; } ifc;
        struct { char **params; int paramc; Expr *body; } lamc;
        struct { Expr *fun; Expr **args; int argc; } appc;
    };
};

typedef struct Env Env;
typedef struct Value Value;

struct Value {
    enum { NumV, BoolV, ClosV, PrimV } type;
    union {
        double num;
        int bool;
        struct { char **params; int paramc; Expr *body; Env *env; } closv;
        char *prim;
    };
};

typedef struct { char *name; Value val; } Binding;
struct Env {
    Binding bindings[64];
    int size;
    Env *parent;
};

// compound literals: &(Expr){...} allocates on stack, no malloc needed
#define NUM(n)      &(Expr){.type = NumC, .num = n}
#define ID(s)       &(Expr){.type = IdC, .var = s}
#define IF(c,t,e)   &(Expr){.type = IfC, .ifc = {c, t, e}}
#define LAM(x,y,b)  &(Expr){.type = LamC, .lamc = {(char*[]){x, y}, 2, b}}
#define APP(f,a,b)  &(Expr){.type = AppC, .appc = {f, (Expr*[]){a, b}, 2}}

Value numv(double n)   { return (Value){.type = NumV, .num = n}; }
Value boolv(int b)     { return (Value){.type = BoolV, .bool = b}; }
Value primv(char *op)  { return (Value){.type = PrimV, .prim = op}; }

Value *lookup(char *name, Env *env) {
    while (env) {
        for (int i = 0; i < env->size; i++)
            if (strcmp(env->bindings[i].name, name) == 0)
                return &env->bindings[i].val;
        env = env->parent;
    }
    return NULL;
}

Env extend(Env *parent, char **params, Value *vals, int n) {
    Env e = {.size = n, .parent = parent};
    for (int i = 0; i < n; i++)
        e.bindings[i] = (Binding){params[i], vals[i]};
    return e;
}

Value apply_primitive(char *op, Value *args) {
    double a = args[0].num, b = args[1].num;
    if (strcmp(op, "+") == 0)  return numv(a + b);
    if (strcmp(op, "-") == 0)  return numv(a - b);
    if (strcmp(op, "*") == 0)  return numv(a * b);
    if (strcmp(op, "/") == 0)  return numv(a / b);
    if (strcmp(op, "<=") == 0) return boolv(a <= b);
    printf("SHEQ: unknown primitive '%s'\n", op); exit(1);
}

Value interp(Expr *e, Env *env) {
    switch (e->type) {
    case NumC:
        return numv(e->num);

    case IdC: {
        Value *v = lookup(e->var, env);
        if (!v) { printf("SHEQ: unbound '%s'\n", e->var); exit(1); }
        return *v;
    }

    case IfC: {
        Value c = interp(e->ifc.test, env);
        if (c.type != BoolV) { printf("SHEQ: if needs bool\n"); exit(1); }
        return c.bool ? interp(e->ifc.then_e, env) : interp(e->ifc.else_e, env);
    }

    case LamC:
        return (Value){.type = ClosV, .closv = {
            e->lamc.params, e->lamc.paramc, e->lamc.body, env
        }};

    case AppC: {
        Value fn = interp(e->appc.fun, env);
        int argc = e->appc.argc;

        Value args[argc];
        for (int i = 0; i < argc; i++)
            args[i] = interp(e->appc.args[i], env);

        if (fn.type == PrimV)
            return apply_primitive(fn.prim, args);

        if (fn.type == ClosV) {
            Env new_env = extend(fn.closv.env, fn.closv.params, args, argc);
            return interp(fn.closv.body, &new_env);
        }

        printf("SHEQ: not a function\n"); exit(1);
    }
    }
    exit(1);
}

char *serialize(Value v) {
    static char buf[64];
    switch (v.type) {
    case NumV:  snprintf(buf, 64, "%.6g", v.num); return buf;
    case BoolV: return v.bool ? "true" : "false";
    case ClosV: return "#<procedure>";
    case PrimV: return "#<primop>";
    }
    return "???";
}

Env top_env(void) {
    Env env = {.size = 7, .parent = NULL};
    env.bindings[0] = (Binding){"+",     primv("+")};
    env.bindings[1] = (Binding){"-",     primv("-")};
    env.bindings[2] = (Binding){"*",     primv("*")};
    env.bindings[3] = (Binding){"/",     primv("/")};
    env.bindings[4] = (Binding){"<=",    primv("<=")};
    env.bindings[5] = (Binding){"true",  boolv(1)};
    env.bindings[6] = (Binding){"false", boolv(0)};
    return env;
}

void test(char *expr, Expr *e, Env *env) {
    printf("%-28s => %s\n", expr, serialize(interp(e, env)));
}

int main(void) {
    Env env = top_env();

    test("{+ 3 4}",   APP(ID("+"), NUM(3), NUM(4)), &env);
    test("{- 10 3}",  APP(ID("-"), NUM(10), NUM(3)), &env);
    test("{* 6 7}",   APP(ID("*"), NUM(6), NUM(7)), &env);
    test("{/ 15 3}",  APP(ID("/"), NUM(15), NUM(3)), &env);
    test("{<= 3 5}",  APP(ID("<="), NUM(3), NUM(5)), &env);
    test("{<= 5 3}",  APP(ID("<="), NUM(5), NUM(3)), &env);

    test("{if true 1 2}",  IF(ID("true"), NUM(1), NUM(2)), &env);
    test("{if false 1 2}", IF(ID("false"), NUM(1), NUM(2)), &env);

    test("{{lam (x y) {+ x y}} 3 4}",
         APP(LAM("x", "y", APP(ID("+"), ID("x"), ID("y"))), NUM(3), NUM(4)), &env);

    // let desugars to immediate lambda application
    test("{let [x=3 y=4] {+ x y}}",
         APP(LAM("x", "y", APP(ID("+"), ID("x"), ID("y"))), NUM(3), NUM(4)), &env);

    return 0;
}
