# C Basics

We all took systems programming, but here's a refresher on the C concepts that tripped me up while building this interpreter. Some of this I had to reinforce from my networks class last quarter.

## Pointers

A pointer holds a memory address.

```c
int *ptr;
```

This says "ptr is a pointer to an integer."

I use pointers for three reasons.

**Speed.** Passing a 1000-element array to a function copies 1000 elements. Passing a pointer copies 8 bytes.

**Modification.** If I want a function to modify something, I give it the address. C passes arguments by value, so without pointers you can't change the original.

**Data structures.** Trees need pointers. Each node points to other nodes. You can't know the addresses at compile time.

In this interpreter, I build a tree of AST nodes. Each parent points to children. Pointers are the only way to build this structure.

## The Arrow Operator

When you have a pointer to a struct, use `->`.

```c
Token *tok = &some_token;
tok->type = TOK_NUMBER;
tok->text = "17";
```

The arrow dereferences and accesses the field in one step. It's shorthand for `(*tok).type`.

## The Dot Operator

When you have the struct directly, use `.`.

```c
Token tok;
tok.type = TOK_NUMBER;
tok.text = "17";
```

Rule: Pointer uses `->`, direct struct uses `.`

## Address-Of Operator

The `&` operator gets the address of a variable.

```c
int x = 5;
int *ptr = &x;
```

Now `ptr` holds the memory address of `x`. You need this when calling functions that expect pointers.

```c
void modify(int *val) {
    *val = 10;
}

int x = 5;
modify(&x);  // Now x is 10
```

In the interpreter, I use `&` when passing structs to functions that need to modify them in place.

## fprintf vs printf

Both print formatted output, but `fprintf` lets you choose where to print.

```c
printf("Result: %d\n", x);           // prints to stdout
fprintf(stderr, "Error: %s\n", msg); // prints to stderr
```

I use `fprintf(stderr, ...)` for all error messages. This keeps errors separate from program output. When you run `./sheq4 'code' > output.txt`, the errors still show on screen because stderr doesn't get redirected by default.

## Double Pointers

Double pointers are pointers to pointers.

```c
char **params;
```

This says "params points to a pointer to char." I use this for arrays of strings. Here's why.

A string in C is `char *` (pointer to first character). An array of strings is an array of `char *`. When you pass an array to a function, it decays to a pointer. So an array of strings becomes `char **`.

```c
char **params = malloc(3 * sizeof(char *));
params[0] = "x";
params[1] = "y";
params[2] = "z";
```

Now `params[0]` is a `char *` pointing to "x". `params` itself is a `char **` pointing to the array.

Could I avoid this? I could use a struct that wraps the array.

```c
typedef struct {
    char *items[MAX_PARAMS];
    int count;
} ParamList;
```

But this has two problems. First, I'd need to pick MAX_PARAMS at compile time. In this interpreter, lambdas can have any number of parameters. Second, passing the struct around means copying all those pointers every time. With `char **`, I pass one pointer that points to the array.

The double pointer approach gives me dynamic sizing and efficient passing. That's why I use it for parameter lists in LamC nodes.

## Function Pointers

You can't have all procedural functions in this interpreter because primitives need dynamic dispatch. Here's the problem.

The top environment binds `+` to a function, `-` to another function, `equal?` to another. When I evaluate `{+ 2 3}`, I don't know at compile time which primitive gets called. The function name comes from the source code.

I could write a giant switch statement.

```c
if (strcmp(name, "+") == 0) {
    return prim_add(args, argc, arena);
} else if (strcmp(name, "-") == 0) {
    return prim_sub(args, argc, arena);
} // ... 10 more primitives
```

This is slow and messy. Every primitive call walks through the whole chain.

Instead, I store function pointers in the environment.

```c
typedef Value* (*PrimFn)(Value *args, int argc, Arena *arena);

struct Value {
    ValueType type;
    union {
        double num;
        PrimFn prim;  // function pointer
    } as;
};
```

Now the environment binds `"+"` to a PrimV containing a pointer to `prim_add`. When I apply it, I just call the function pointer.

```c
if (func->type == VAL_PRIMV) {
    return func->as.prim(argv, n_args, arena);
}
```

One indirect call. No string comparisons. The function pointer architecture makes primitive dispatch fast and clean.

## Token Stream

The TokenStream struct holds the token array and current position.

```c
typedef struct {
    Token *tokens;
    size_t count;
    size_t current;
} TokenStream;
```

The lexer produces all tokens at once and puts them in the array. The parser walks through with the `current` index. This is simpler than making the lexer and parser run in lockstep.

I tried writing a lazy tokenizer once that produced tokens on demand. The parser would call `next_token()` and the lexer would scan until it found one. This meant the lexer had to maintain state (current position in source, what character it's looking at, whether it's in the middle of a number or identifier). The TokenStream approach is cleaner. Tokenize everything up front, then parser just walks an array.

## Buffers

A buffer is a fixed-size array you write into before copying elsewhere.

```c
char buf[4096];
snprintf(buf, sizeof(buf), "Result: %d", x);
```

I use buffers for serialization. When converting a Value to a string, I write into a static buffer first, then use `strdup()` to copy it to heap memory. The buffer prevents repeated small allocations.

```c
static char buf[4096];

char *serialize(Value *val) {
    switch (val->type) {
        case VAL_NUMV:
            snprintf(buf, sizeof(buf), "%.15g", val->as.num);
            return strdup(buf);
        // ...
    }
}
```

The `static` keyword means the buffer persists between function calls. It's not on the stack. This is safe because serialize isn't recursive and I copy the buffer contents with `strdup` before returning.

I chose 4096 bytes because that's way more than any SHEQ4 value needs. Numbers serialize to at most 20 characters. Closures just serialize to `"#<procedure>"`. The only thing that could be long is strings, and I truncate those after 4000 characters.

## size_t

This is an unsigned integer type for sizes.

```c
size_t len = strlen(str);
```

It's guaranteed big enough to hold any object's size. On 64-bit systems it's 64 bits, on 32-bit systems it's 32 bits.

I use `size_t` instead of `int` because sizes can't be negative, and on 64-bit systems objects can be bigger than what a 32-bit `int` can represent.

## POSIX C Source

At the top of `sheq4.c`:

```c
#define _POSIX_C_SOURCE 200809L
```

This tells the compiler I'm using POSIX extensions. I need `strdup()`, which is POSIX but not in C11.

I could manually call `malloc()` and `strcpy()` instead, but `strdup()` does exactly what I need in one call. The `200809L` means POSIX.1-2008.

## Example

Here's how pointers work in this code.

```c
ASTNode *make_num(Arena *arena, double val) {
    ASTNode *node = arena_alloc(arena, sizeof(ASTNode));
    if (!node) return NULL;
    node->type = NODE_NUMC;
    node->as.num_val = val;
    return node;
}
```

The function allocates space for an ASTNode. `arena_alloc` returns a pointer to that space. It uses `->` to set fields because `node` is a pointer. It returns the pointer so the caller can use this node.

The caller gets:

```c
ASTNode *num = make_num(arena, 17);
```

Now `num` points to a node in the arena. Multiple places can hold pointers to the same node. That's how trees get built.
