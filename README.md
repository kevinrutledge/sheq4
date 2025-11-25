# SHEQ4

A tree-walking interpreter for SHEQ4, a higher-order functional language with lexical scoping. Written in C.

## Documentation

See `docs/` for full implementation details. Covers the C implementation, memory management, parsing, and evaluation with diagrams. Start with [Overview](docs/01-overview.md).

## Getting Started

Clone the repo, then create your `.env` file:

```bash
touch .env
```

Add your info:

```dotenv
UNIX_USER=your_calpoly_username
UNIX_HOST=unixN.csc.calpoly.edu
UNIX_PATH=path/to/your/project
```

Replace `unixN` with whichever server you use (unix1 through unix5). Set the path to wherever you want the project in your home directory.

Example:

```dotenv
UNIX_USER=my_calpoly_username
UNIX_HOST=unix3.csc.calpoly.edu
UNIX_PATH=csc_430/sheq4
```

Make the deploy script executable:

```bash
chmod +x deploy.sh
```

Create the directory on the unix server:

```bash
ssh your_username@unixN.csc.calpoly.edu "mkdir -p ~/path/to/your/project"
```

## Deploying

```bash
./deploy.sh
```

This copies `sheq4.c`, `Makefile`, and `test.sh` to your unix server.

## Building and Testing

SSH into the server and run:

```bash
cd ~/path/to/your/project
chmod +x test.sh
make
make test
```

Run a single expression:

```bash
./sheq4 '{+ 3 4}'
```

## Language

SHEQ4 supports numbers, strings, booleans, conditionals, lambdas, and let bindings.

**Primitives:** `+`, `-`, `*`, `/`, `<=`, `equal?`, `substring`, `strlen`, `error`

**Examples:**

```
7                              => 7
"hello"                        => "hello"
{+ 3 4}                        => 7
{if true 1 2}                  => 1
{lambda (x) : {+ x 1}}         => #<procedure>
{{lambda (x) : {+ x 1}} 5}     => 6
{let {[x = 5]} in {+ x 3} end} => 8
```

Lambdas capture their environment at definition time:

```
{let {[add = {lambda (x) : {lambda (y) : {+ x y}}}]}
  in {{add 2} 3}
  end}
=> 5
```

## Files

- `sheq4.c` — the interpreter
- `test.sh` — test suite
- `Makefile` — build configuration
- `deploy.sh` — pushes code to unix server
- `.env` — your local credentials (not tracked by git)
- `docs/` — implementation documentation
