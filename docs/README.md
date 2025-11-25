# Documentation

How SHEQ4 works with C as a host language.

## Reading Order

1. **[Overview](01-overview.md)** — Pipeline architecture, four stages
2. **[C Basics](02-c-basics.md)** — Pointers, `->` vs `.`, `size_t`, POSIX
3. **[Memory Management](03-memory.md)** — Arena allocator
4. **[Data Structures](04-data-structures.md)** — Token, ASTNode, Value, Env
5. **[Parsing](05-parsing.md)** — Tokenization and tree building
6. **[Evaluation](06-evaluation.md)** — Interpreter and primitives
7. **[Complete Walkthrough](07-walkthrough.md)** — Full trace of `{{lambda (x) : {+ x 1} 2}`
8. **[Resources](08-resources.md)** — External references for clarification

## Diagrams

All diagrams live in `figures/` as `.png` files rendered from mermaid source:

1. `architecture.png` — High-level pipeline
2. `arena-memory.png` — Memory layout with byte offsets
3. `token-struct.png` — Token struct
4. `ast-hierarchy.png` — AST node types
5. `value-types.png` — Value types
6. `tokenize-logic.png` — Tokenization decisions
7. `parse-calls.png` — Parse function calls
8. `interp-dispatch.png` — Interpreter dispatch
9. `lambda-apply.png` — Lambda application sequence
10. `lambda-eval-steps.png` — Lambda evaluation steps
11. `full-trace.png` — Complete execution trace

The markdown docs reference these images directly.

## What's Inside

**Overview** covers the pipeline. Tokenize, parse, interpret, serialize. That's it.

**C Basics** refreshes pointer syntax and systems programming concepts. Why `->` exists, what `size_t` does, how fprintf differs from printf. Stuff that trips people up.

**Memory** explains arena allocation. One malloc at the start, bump pointer for allocations, single free at the end. Simple.

**Data Structures** shows the four core structs: Token (lexer output), ASTNode (parser output), Value (interpreter output), Env (variable bindings). Each one builds on what came before.

**Parsing** walks through lexing and parsing. Characters become tokens, tokens become an AST. The lexer scans linearly, the parser recurses through braced expressions.

**Evaluation** covers interpretation. Walking the AST, applying closures, calling primitives. Environment extension for lexical scoping.

**Walkthrough** traces `{{lambda (x) : {+ x 1} 2}` from start to finish. Every step, every function call, every pointer chase. See how the pieces connect.

**Resources** provides external references if the docs aren't clear. Focused links to tutorials and man pages for C syntax, patterns, and concepts.
