# Resources

If the docs aren't clear, these provide deeper explanations. One resource per concept.

## Core Concepts

### Tagged Unions

**Tagged union — Wikipedia**  
https://en.wikipedia.org/wiki/Tagged_union

Perfect for small interpreters. The `type` field tags which union variant is active, all variants share memory. Type-safe without wasting space. I used this pattern for both ASTNode and Value structs.

### Arena Allocators

**Memory Allocation Strategies Part 2: Linear Allocators** — gingerBill  
https://www.gingerbill.org/article/2019/02/08/memory-allocation-strategies-002/

Step-by-step C implementation with `align_forward`. One big buffer upfront, bump a pointer for each allocation, free everything at once. Faster and simpler than malloc/free for interpreter workloads. Memory layout diagrams show why.

### Recursive Descent Parsing

**Abstract Syntax Tree: An Example in C** — Vladimir Keleshev  
https://keleshev.com/abstract-syntax-tree-an-example-in-c/

Building ASTs with tagged unions in C. Covers struct design and switch-based traversal. The approach directly translates grammar into C functions, which made porting from Racket straightforward.

### Closures in C

**How to Implement Closures** — Vidar Hokstad  
http://hokstad.com/how-to-implement-closures

Technical explanation with C code. Covers heap environments and captured variables. Good resource to understand that a closure is just a struct holding parameter names, body pointer, and environment pointer. No magic.

### Lisp Interpreter Example

**minilisp** — Rui Ueyama (GitHub)  
https://github.com/rui314/minilisp

Most helpful resource overall. 1000 lines showing environment chains, lambda evaluation, and closures in practice. Readable code covering tokenization, parsing, environments, and closures.

## C Syntax

### Function Pointers

**Function Pointer in C** — GeeksforGeeks  
https://www.geeksforgeeks.org/c/function-pointer-in-c/

Store functions in structs, call them dynamically. I used this for primitive dispatch where each builtin gets stored as a function pointer in PrimV. Declaration syntax and typedef usage for setting up `PrimFn`.

### Double Pointers

**Double Pointer (char**) in C\*\* — GeeksforGeeks  
https://www.geeksforgeeks.org/c/c-pointer-to-pointer-double-pointer/

Lambda parameter lists need arrays of strings. In C that's `char **params`. A string is `char *`, so an array of strings is `char **`. Covers pointer arithmetic and allocation.

### Arrow Operator

**Arrow Operator in C/C++** — GeeksforGeeks  
https://www.geeksforgeeks.org/c/arrow-operator-in-c-c-with-examples/

Shorthand for dereferencing and accessing fields. `node->type` instead of `(*node).type`. When to use it versus dot operator.

### Static Keyword

**Static Keyword in C** — TutorialsPoint  
https://www.tutorialspoint.com/cprogramming/c_static_keyword.htm

Static buffers persist between function calls without heap allocation. I used a static 4096-byte buffer in `serialize()`. Explains static variables, static globals, and when they're useful.

## POSIX C

### Feature Test Macros

**feature_test_macros(7)** — Linux Manual Page  
https://man7.org/linux/man-pages/man7/feature_test_macros.7.html

Defining `_POSIX_C_SOURCE 200809L` enables POSIX.1-2008 features like `strdup()`. Documents which value for which POSIX version.

### strdup Function

**strdup(3)** — Linux Manual Page  
https://man7.org/linux/man-pages/man3/strdup.3.html

Allocates memory and copies a string in one call. Simpler than manual `malloc()` and `strcpy()`. Requires `_POSIX_C_SOURCE >= 200809L`.

## C Man Pages

Ground truth reference for C syntax:

**sizeof operator**  
https://man7.org/linux/man-pages/man3/sizeof.3.html

**struct keyword**  
https://man7.org/linux/man-pages/man3/struct.3type.html

**union keyword**  
https://man7.org/linux/man-pages/man3/union.3type.html

**typedef declaration**  
https://man7.org/linux/man-pages/man3/typedef.3type.html

**enum keyword**  
https://man7.org/linux/man-pages/man3/enum.3type.html

**malloc(3) and free(3)**  
https://man7.org/linux/man-pages/man3/malloc.3.html

**memcpy(3)**  
https://man7.org/linux/man-pages/man3/memcpy.3.html

**memset(3)**  
https://man7.org/linux/man-pages/man3/memset.3.html

**strlen(3)**  
https://man7.org/linux/man-pages/man3/strlen.3.html

**strcmp(3)**  
https://man7.org/linux/man-pages/man3/strcmp.3.html

**fprintf(3) and printf(3)**  
https://man7.org/linux/man-pages/man3/fprintf.3.html

**snprintf(3)**  
https://man7.org/linux/man-pages/man3/snprintf.3.html

**strtod(3)**  
https://man7.org/linux/man-pages/man3/strtod.3.html

The docs explain my concepts. These resources provide technical details.
