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
        struct { char *param; Expr *body; } lamc;
        struct { Expr *fun; Expr *arg; } appc;
    };
};

typedef struct Env Env;
typedef struct Value Value;
typedef Value (*PrimFn)(double, double);

struct Value {
    enum { NumV, BoolV, ClosV, PrimV, PartialV } type;
    union {
        double num;
        int bool;
        struct { char *param; Expr *body; Env *env; } closv;
        PrimFn prim;
        struct { PrimFn fn; double arg; } partial;
    };
};

typedef struct { char *name; Value val; } Binding;
struct Env {
    Binding bindings[64];
    int size;
    Env *parent;
};

// compound literals: &(Expr){...} allocates on stack, no malloc needed
#define NUM(n)    &(Expr){.type = NumC, .num = n}
#define ID(s)     &(Expr){.type = IdC, .var = s}
#define IF(c,t,e) &(Expr){.type = IfC, .ifc = {c, t, e}}
#define LAM(p,b)  &(Expr){.type = LamC, .lamc = {p, b}}
#define APP(f,a)  &(Expr){.type = AppC, .appc = {f, a}}

Value numv(double n)   { return (Value){.type = NumV, .num = n}; }
Value boolv(int b)     { return (Value){.type = BoolV, .bool = b}; }
Value primv(PrimFn fn) { return (Value){.type = PrimV, .prim = fn}; }

Value prim_add(double a, double b) { return numv(a + b); }
Value prim_sub(double a, double b) { return numv(a - b); }
Value prim_mul(double a, double b) { return numv(a * b); }
Value prim_div(double a, double b) { return numv(a / b); }
Value prim_leq(double a, double b) { return boolv(a <= b); }

Value *lookup(char *name, Env *env) {
    while (env) {
        for (int i = 0; i < env->size; i++)
            if (strcmp(env->bindings[i].name, name) == 0)
                return &env->bindings[i].val;
        env = env->parent;
    }
    return NULL;
}

Env extend(Env *parent, char *name, Value val) {
    Env e = {.size = 1, .parent = parent};
    e.bindings[0] = (Binding){name, val};
    return e;
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
        // closure captures current env for lexical scope
        return (Value){.type = ClosV, .closv = {e->lamc.param, e->lamc.body, env}};

    case AppC: {
        Value fn = interp(e->appc.fun, env);
        Value arg = interp(e->appc.arg, env);

        // curried primitives: {+ 3 4} is really {{+ 3} 4}
        // first app saves the arg, second app runs the operation
        if (fn.type == PrimV) {
            if (arg.type != NumV) { printf("SHEQ: prim needs num\n"); exit(1); }
            return (Value){.type = PartialV, .partial = {fn.prim, arg.num}};
        }
        if (fn.type == PartialV) {
            if (arg.type != NumV) { printf("SHEQ: prim needs num\n"); exit(1); }
            return fn.partial.fn(fn.partial.arg, arg.num);
        }
        if (fn.type != ClosV) { printf("SHEQ: not a function\n"); exit(1); }

        Env new_env = extend(fn.closv.env, fn.closv.param, arg);
        return interp(fn.closv.body, &new_env);
    }
    }
    exit(1);
}

char *serialize(Value v) {
    static char buf[64];
    switch (v.type) {
    case NumV:     snprintf(buf, 64, "%.6g", v.num); return buf;
    case BoolV:    return v.bool ? "true" : "false";
    case ClosV:    return "#<procedure>";
    case PrimV:    return "#<primop>";
    case PartialV: return "#<primop partial>";
    }
    return "???";
}

Env top_env(void) {
    Env env = {.size = 7, .parent = NULL};
    env.bindings[0] = (Binding){"+",     primv(prim_add)};
    env.bindings[1] = (Binding){"-",     primv(prim_sub)};
    env.bindings[2] = (Binding){"*",     primv(prim_mul)};
    env.bindings[3] = (Binding){"/",     primv(prim_div)};
    env.bindings[4] = (Binding){"<=",    primv(prim_leq)};
    env.bindings[5] = (Binding){"true",  boolv(1)};
    env.bindings[6] = (Binding){"false", boolv(0)};
    return env;
}

void test(char *expr, Expr *e, Env *env) {
    printf("%-28s => %s\n", expr, serialize(interp(e, env)));
}

int main(void) {
    Env env = top_env();

    test("{+ 3 4}",   APP(APP(ID("+"), NUM(3)), NUM(4)), &env);
    test("{- 10 3}",  APP(APP(ID("-"), NUM(10)), NUM(3)), &env);
    test("{* 6 7}",   APP(APP(ID("*"), NUM(6)), NUM(7)), &env);
    test("{/ 15 3}",  APP(APP(ID("/"), NUM(15)), NUM(3)), &env);
    test("{<= 3 5}",  APP(APP(ID("<="), NUM(3)), NUM(5)), &env);
    test("{<= 5 3}",  APP(APP(ID("<="), NUM(5)), NUM(3)), &env);

    test("{if true 1 2}",  IF(ID("true"), NUM(1), NUM(2)), &env);
    test("{if false 1 2}", IF(ID("false"), NUM(1), NUM(2)), &env);

    test("{{lam (x) {+ x 1}} 5}",
         APP(LAM("x", APP(APP(ID("+"), ID("x")), NUM(1))), NUM(5)), &env);

    // let desugars to lambda application
    test("{let [x=5] {+ x 3}}",
         APP(LAM("x", APP(APP(ID("+"), ID("x")), NUM(3))), NUM(5)), &env);

    return 0;
}
