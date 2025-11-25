# Complete Walkthrough

A brief walk through of `{{lambda (x) : {+ x 1} 2}` shows how all the pieces connect. This expression creates a lambda that adds 1 to its argument, then applies it to 2.

<img src="figures/full-trace.png" alt="Full Trace" height="1500">

## Step 1: Tokenize

The lexer scans the source string character by character.

```
{       -> LBRACE
{       -> LBRACE
lambda  -> LAMBDA
(       -> LPAREN
x       -> ID("x")
)       -> RPAREN
:       -> COLON
{       -> LBRACE
+       -> ID("+")
x       -> ID("x")
1       -> NUMBER("1")
}       -> RBRACE
2       -> NUMBER("2")
}       -> RBRACE
```

14 tokens get stored in an array. The lexer allocates this array in the arena and fills it in one pass.

## Step 2: Parse

The parser builds an AST from those tokens. `parse_expr` sees LBRACE and calls `parse_braced`. `parse_braced` sees another LBRACE and calls itself recursively. The inner `parse_braced` sees LAMBDA and calls `parse_lambda`.

`parse_lambda` expects LPAREN, collects parameter "x", expects RPAREN, expects COLON, then calls `parse_expr` to parse the body. The body `{+ x 1}` parses as an AppC node calling `+` with arguments `x` and `1`.

Back in the outer `parse_braced`, there's now a LamC node. The next token is NUMBER("2"), so `parse_app` collects that as an argument.

Final AST:

```
AppC
├── LamC
│   ├── params: ["x"]
│   └── body: AppC
│       ├── IdC("+")
│       ├── IdC("x")
│       └── NumC(1)
└── NumC(2)
```

## Step 3: Create Arena and Environment

Before interpretation starts, the program sets up memory and the top environment.

```c
Arena *arena = arena_create(1024 * 1024);
Env *env = make_top_env(arena);
```

The arena gets 1MB. The top environment gets bindings for all primitives: `+`, `-`, `*`, `/`, `<=`, `equal?`, `substring`, `strlen`, `error`, `true`, `false`.

## Step 4: Interpret the AppC Node

The interpreter sees an AppC node and evaluates the function and arguments.

Evaluating the LamC:

```c
Value *func = interp(LamC_node, env, arena);
```

This creates a ClosV with params `["x"]`, body `AppC(IdC("+"), [IdC("x"), NumC(1)])`, and env `top_env`. The closure packages up the lambda for later use.

Evaluating the NumC(2):

```c
Value *arg = interp(NumC(2), env, arena);
```

Returns NumV(2).

## Step 5: Apply the Closure

The interpreter has a ClosV and NumV(2). Time to apply the closure.

Arity check passes (1 param, 1 arg). The closure's captured environment gets extended:

```c
Env *new_env = extend_env(arena, clos->env, 1, ["x"], [NumV(2)]);
```

The extended environment:

```
x -> NumV(2)
parent -> top_env
```

The closure's captured environment gets extended, not the call-site environment. This implements lexical scoping as I designed it.

The body gets evaluated:

```c
return interp(clos->body, new_env, arena);
```

## Step 6: Evaluate the Body

The body is `{+ x 1}`:

```
AppC
├── IdC("+")
├── IdC("x")
└── NumC(1)
```

The interpreter evaluates this AppC.

Evaluating the function:

```c
Value *func = interp(IdC("+"), new_env, arena);
```

IdC("+") triggers a lookup. The local environment gets checked (x -> NumV(2)), doesn't find it. The parent pointer gets followed to top_env, finds `+ -> PrimV(prim_add)`. Returns PrimV(prim_add).

Evaluating the arguments:

```c
Value *arg1 = interp(IdC("x"), new_env, arena);
```

IdC("x") does a lookup. The local environment gets checked and finds `x -> NumV(2)`. Returns NumV(2).

```c
Value *arg2 = interp(NumC(1), new_env, arena);
```

Returns NumV(1).

Applying the primitive. The function is a PrimV, so the interpreter calls through the function pointer:

```c
return func->as.prim([NumV(2), NumV(1)], 2, arena);
```

This calls `prim_add`, which checks both args are numbers, then adds them. Returns NumV(3).

## Step 7: Unwind

The body returned NumV(3). That's the result of applying the closure. That's the result of evaluating the outer AppC. That's the final result.

## Step 8: Serialize and Print

```c
char *out = serialize(val);
printf("%s\n", out);
```

`serialize` converts NumV(3) to the string "3":

```c
snprintf(buf, sizeof(buf), "%.15g", val->as.num);
return strdup(buf);
```

Prints "3" to stdout.

## Step 9: Clean Up

```c
free(out);
arena_destroy(arena);
```

The serialized string gets freed (allocated with strdup). The arena gets destroyed, freeing the entire 1MB buffer.

## The Complete Flow

The full execution:

Tokenize: string becomes 14 tokens.

Parse: tokens become AppC(LamC, NumC) tree.

Setup: arena created, top environment initialized.

Interpret: AST gets walked.

- LamC evaluates to ClosV
- NumC(2) evaluates to NumV(2)
- Closure gets applied by extending environment with x->2
- Body AppC(+, x, 1) gets evaluated
- Lookup + finds PrimV(prim_add)
- Lookup x finds NumV(2)
- 1 evaluates to NumV(1)
- prim_add(2, 1) returns NumV(3)

Serialize: NumV(3) becomes "3".

Print: outputs "3".

Cleanup: memory freed.

## Running It

```bash
./sheq4 '{{lambda (x) : {+ x 1} 2}'
```

Output:

```
3
```

---

**Need more clarification?** [Resources](08-resources.md)
