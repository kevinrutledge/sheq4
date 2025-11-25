#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct Arena {
    unsigned char *buf;
    size_t buf_len;
    size_t curr_offset;
} Arena;

size_t align_up(size_t size, size_t align) {
    return ((size + align - 1) / align) * align;
}

// size bytes from arena, 8-byte aligned, zeroed; NULL on exhaustion
void *arena_alloc(Arena *arena, size_t size) {
    size_t aligned_offset = align_up(arena->curr_offset, 8);
    if (aligned_offset + size > arena->buf_len) {
        fprintf(stderr, "SHEQ: arena exhausted\n");
        return NULL;
    }
    void *ptr = &arena->buf[aligned_offset];
    arena->curr_offset = aligned_offset + size;
    memset(ptr, 0, size);
    return ptr;
}

Arena *arena_create(size_t capacity) {
    Arena *arena = malloc(sizeof(Arena));
    if (!arena) {
        fprintf(stderr, "SHEQ: malloc failed\n");
        return NULL;
    }
    arena->buf = malloc(capacity);
    if (!arena->buf) {
        fprintf(stderr, "SHEQ: malloc failed\n");
        free(arena);
        return NULL;
    }
    arena->buf_len = capacity;
    arena->curr_offset = 0;
    return arena;
}

void arena_destroy(Arena *arena) {
    if (arena) {
        free(arena->buf);
        free(arena);
    }
}

typedef enum {
    TOK_ERROR = 0,
    TOK_NUMBER,
    TOK_STRING,
    TOK_ID,
    TOK_IF,
    TOK_LAMBDA,
    TOK_LET,
    TOK_IN,
    TOK_END,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_LBRACKET,
    TOK_RBRACKET,
    TOK_COLON,
    TOK_EQUALS,
    TOK_TRUE,
    TOK_FALSE,
    TOK_EOF
} TokenType;

typedef struct {
    TokenType type;
    char *text;
    int line;
    int col;
} Token;

typedef struct {
    Token *tokens;
    int count;
    int capacity;
    int current;
} TokenStream;

typedef enum {
    NODE_NUMC,
    NODE_STRC,
    NODE_IDC,
    NODE_IFC,
    NODE_LAMC,
    NODE_APPC
} NodeType;

typedef struct ASTNode {
    NodeType type;
    union {
        double num_val;
        char *str_val;
        char *var;
        struct {
            struct ASTNode *test;
            struct ASTNode *then_expr;
            struct ASTNode *else_expr;
        } if_node;
        struct {
            int param_count;
            char **params;
            struct ASTNode *body;
        } lam_node;
        struct {
            int child_count;
            struct ASTNode **children;
        } app_node;
    } as;
} ASTNode;

typedef enum {
    VAL_NUMV,
    VAL_STRV,
    VAL_BOOLV,
    VAL_CLOSV,
    VAL_PRIMV
} ValueType;

typedef struct Env Env;
typedef struct Value Value;

typedef Value *(*PrimFn)(Value *args, int n_args, Arena *arena);

struct Value {
    ValueType type;
    union {
        double num;
        struct {
            char *data;
            size_t len;
        } str;
        int boolval;
        struct {
            int param_count;
            char **params;
            ASTNode *body;
            Env *env;
        } clos;
        PrimFn prim;
    } as;
};

typedef struct Binding {
    char *name;
    Value val;
    struct Binding *next;
} Binding;

struct Env {
    Binding *bindings;
    Env *parent;
};

Env *create_env(Arena *arena, Env *parent) {
    Env *env = arena_alloc(arena, sizeof(Env));
    if (!env) return NULL;
    env->bindings = NULL;
    env->parent = parent;
    return env;
}

// walk env chain for name binding; NULL if unbound
Value *lookup(Env *env, const char *name) {
    for (; env; env = env->parent) {
        for (Binding *binding = env->bindings; binding; binding = binding->next) {
            if (strcmp(binding->name, name) == 0)
                return &binding->val;
        }
    }
    return NULL;
}

// add name->val binding to env; returns success
int bind_env(Arena *arena, Env *env, const char *name, Value val) {
    Binding *binding = arena_alloc(arena, sizeof(Binding));
    if (!binding) return 0;
    size_t len = strlen(name);
    binding->name = arena_alloc(arena, len + 1);
    if (!binding->name) return 0;
    memcpy(binding->name, name, len + 1);
    binding->val = val;
    binding->next = env->bindings;
    env->bindings = binding;
    return 1;
}

// new env with count bindings; parent chain provides outer scope
Env *extend_env(Arena *arena, Env *parent, int count, char **names, Value *vals) {
    Env *env = create_env(arena, parent);
    if (!env) return NULL;
    // reverse order so first param ends up at head of binding list
    for (int i = count - 1; i >= 0; i--) {
        Binding *binding = arena_alloc(arena, sizeof(Binding));
        if (!binding) return NULL;
        size_t len = strlen(names[i]);
        binding->name = arena_alloc(arena, len + 1);
        if (!binding->name) return NULL;
        memcpy(binding->name, names[i], len + 1);
        binding->val = vals[i];
        binding->next = env->bindings;
        env->bindings = binding;
    }
    return env;
}

ASTNode *make_num(Arena *arena, double val) {
    ASTNode *node = arena_alloc(arena, sizeof(ASTNode));
    if (!node) return NULL;
    node->type = NODE_NUMC;
    node->as.num_val = val;
    return node;
}

ASTNode *make_str(Arena *arena, const char *str, size_t len) {
    ASTNode *node = arena_alloc(arena, sizeof(ASTNode));
    if (!node) return NULL;
    node->type = NODE_STRC;
    node->as.str_val = arena_alloc(arena, len + 1);
    if (!node->as.str_val) return NULL;
    memcpy(node->as.str_val, str, len);
    node->as.str_val[len] = '\0';
    return node;
}

ASTNode *make_id(Arena *arena, const char *name) {
    ASTNode *node = arena_alloc(arena, sizeof(ASTNode));
    if (!node) return NULL;
    node->type = NODE_IDC;
    size_t len = strlen(name);
    node->as.var = arena_alloc(arena, len + 1);
    if (!node->as.var) return NULL;
    memcpy(node->as.var, name, len + 1);
    return node;
}

ASTNode *make_if(Arena *arena, ASTNode *test, ASTNode *then_expr, ASTNode *else_expr) {
    ASTNode *node = arena_alloc(arena, sizeof(ASTNode));
    if (!node) return NULL;
    node->type = NODE_IFC;
    node->as.if_node.test = test;
    node->as.if_node.then_expr = then_expr;
    node->as.if_node.else_expr = else_expr;
    return node;
}

ASTNode *make_lambda(Arena *arena, int n_params, char **params, ASTNode *body) {
    ASTNode *node = arena_alloc(arena, sizeof(ASTNode));
    if (!node) return NULL;
    node->type = NODE_LAMC;
    node->as.lam_node.param_count = n_params;
    node->as.lam_node.params = arena_alloc(arena, sizeof(char *) * n_params);
    if (!node->as.lam_node.params && n_params > 0) return NULL;
    for (int i = 0; i < n_params; i++) {
        size_t len = strlen(params[i]);
        node->as.lam_node.params[i] = arena_alloc(arena, len + 1);
        if (!node->as.lam_node.params[i]) return NULL;
        memcpy(node->as.lam_node.params[i], params[i], len + 1);
    }
    node->as.lam_node.body = body;
    return node;
}

ASTNode *make_app(Arena *arena, ASTNode *func, int n_args, ASTNode **args) {
    ASTNode *node = arena_alloc(arena, sizeof(ASTNode));
    if (!node) return NULL;
    node->type = NODE_APPC;
    // children[0] = function, children[1..n] = arguments
    node->as.app_node.child_count = n_args + 1;
    node->as.app_node.children = arena_alloc(arena, sizeof(ASTNode *) * (n_args + 1));
    if (!node->as.app_node.children) return NULL;
    node->as.app_node.children[0] = func;
    for (int i = 0; i < n_args; i++)
        node->as.app_node.children[i + 1] = args[i];
    return node;
}

// input string -> token stream; NULL on lexical error
TokenStream *tokenize(Arena *arena, const char *input) {
    TokenStream *ts = arena_alloc(arena, sizeof(TokenStream));
    if (!ts) return NULL;

    // 64 covers most expressions; grows as needed
    ts->capacity = 64;
    ts->tokens = arena_alloc(arena, sizeof(Token) * ts->capacity);
    if (!ts->tokens) return NULL;
    ts->count = 0;
    ts->current = 0;

    int pos = 0;
    int len = strlen(input);
    int line = 1, col = 1;

    while (pos < len) {
        while (pos < len && isspace(input[pos])) {
            if (input[pos] == '\n') { line++; col = 1; }
            else col++;
            pos++;
        }
        if (pos >= len) break;

        Token tok = {0};
        tok.line = line;
        tok.col = col;

        if (isdigit(input[pos]) || (input[pos] == '-' && pos + 1 < len && isdigit(input[pos + 1]))) {
            int start = pos;
            if (input[pos] == '-') pos++;
            while (pos < len && isdigit(input[pos])) pos++;
            if (pos < len && input[pos] == '.') {
                pos++;
                while (pos < len && isdigit(input[pos])) pos++;
            }
            int num_len = pos - start;
            tok.type = TOK_NUMBER;
            tok.text = arena_alloc(arena, num_len + 1);
            if (!tok.text) return NULL;
            memcpy(tok.text, input + start, num_len);
            tok.text[num_len] = '\0';
            col += num_len;
        }
        else if (input[pos] == '"') {
            int start = pos++;
            while (pos < len && input[pos] != '"') {
                if (input[pos] == '\\' && pos + 1 < len) pos++;
                pos++;
            }
            if (pos >= len) {
                fprintf(stderr, "SHEQ: unterminated string at line %d col %d\n", line, col);
                return NULL;
            }
            pos++;
            int str_len = pos - start;
            tok.type = TOK_STRING;
            tok.text = arena_alloc(arena, str_len + 1);
            if (!tok.text) return NULL;
            memcpy(tok.text, input + start, str_len);
            tok.text[str_len] = '\0';
            col += str_len;
        }
        else if (input[pos] == '{') { tok.type = TOK_LBRACE; tok.text = "{"; pos++; col++; }
        else if (input[pos] == '}') { tok.type = TOK_RBRACE; tok.text = "}"; pos++; col++; }
        else if (input[pos] == '(') { tok.type = TOK_LPAREN; tok.text = "("; pos++; col++; }
        else if (input[pos] == ')') { tok.type = TOK_RPAREN; tok.text = ")"; pos++; col++; }
        else if (input[pos] == '[') { tok.type = TOK_LBRACKET; tok.text = "["; pos++; col++; }
        else if (input[pos] == ']') { tok.type = TOK_RBRACKET; tok.text = "]"; pos++; col++; }
        else if (input[pos] == ':') { tok.type = TOK_COLON; tok.text = ":"; pos++; col++; }
        else if (input[pos] == '=') { tok.type = TOK_EQUALS; tok.text = "="; pos++; col++; }
        // operators (+, -, *, etc.) are valid identifiers in SHEQ4
        // minus is tricky: `-5` is negative number, `-` alone or `-foo` is identifier
        else if (isalpha(input[pos]) || input[pos] == '_' ||
                 input[pos] == '+' || input[pos] == '*' ||
                 input[pos] == '/' || input[pos] == '<' ||
                 input[pos] == '>' || input[pos] == '?' || input[pos] == '!' ||
                 (input[pos] == '-' && (pos + 1 >= len || !isdigit(input[pos + 1])))) {
            int start = pos;
            while (pos < len && (isalnum(input[pos]) || input[pos] == '_' ||
                   input[pos] == '-' || input[pos] == '?' || input[pos] == '!' ||
                   input[pos] == '+' || input[pos] == '*' || input[pos] == '/' ||
                   input[pos] == '<' || input[pos] == '=' || input[pos] == '>'))
                pos++;
            int id_len = pos - start;
            tok.text = arena_alloc(arena, id_len + 1);
            if (!tok.text) return NULL;
            memcpy(tok.text, input + start, id_len);
            tok.text[id_len] = '\0';

            if (strcmp(tok.text, "if") == 0) tok.type = TOK_IF;
            else if (strcmp(tok.text, "lambda") == 0) tok.type = TOK_LAMBDA;
            else if (strcmp(tok.text, "let") == 0) tok.type = TOK_LET;
            else if (strcmp(tok.text, "in") == 0) tok.type = TOK_IN;
            else if (strcmp(tok.text, "end") == 0) tok.type = TOK_END;
            else if (strcmp(tok.text, "true") == 0) tok.type = TOK_TRUE;
            else if (strcmp(tok.text, "false") == 0) tok.type = TOK_FALSE;
            else tok.type = TOK_ID;

            col += id_len;
        }
        else {
            fprintf(stderr, "SHEQ: unexpected '%c' at line %d col %d\n", input[pos], line, col);
            return NULL;
        }

        if (ts->count >= ts->capacity) {
            ts->capacity *= 2;
            Token *newtoks = arena_alloc(arena, sizeof(Token) * ts->capacity);
            if (!newtoks) return NULL;
            memcpy(newtoks, ts->tokens, sizeof(Token) * ts->count);
            ts->tokens = newtoks;
        }
        ts->tokens[ts->count++] = tok;
    }

    Token eof = {TOK_EOF, NULL, line, col};
    ts->tokens[ts->count++] = eof;
    return ts;
}

typedef struct {
    TokenStream *ts;
    Arena *arena;
} Parser;

Token peek(Parser *parser) {
    return parser->ts->tokens[parser->ts->current];
}

Token advance(Parser *parser) {
    if (parser->ts->current < parser->ts->count - 1)
        parser->ts->current++;
    return parser->ts->tokens[parser->ts->current - 1];
}

int match(Parser *parser, TokenType type) {
    if (peek(parser).type == type) {
        advance(parser);
        return 1;
    }
    return 0;
}

Token expect(Parser *parser, TokenType type, const char *msg) {
    Token tok = peek(parser);
    if (tok.type != type) {
        fprintf(stderr, "SHEQ: %s at line %d col %d\n", msg, tok.line, tok.col);
        return (Token){TOK_ERROR, NULL, tok.line, tok.col};
    }
    return advance(parser);
}

ASTNode *parse_expr(Parser *parser);

ASTNode *parse_lambda(Parser *parser) {
    expect(parser, TOK_LPAREN, "lambda needs '('");

    // most lambdas have <8 params; grows if needed
    int cap = 8;
    int count = 0;
    char **params = arena_alloc(parser->arena, sizeof(char *) * cap);
    if (!params) return NULL;

    while (!match(parser, TOK_RPAREN)) {
        if (count >= cap) {
            cap *= 2;
            char **new_params = arena_alloc(parser->arena, sizeof(char *) * cap);
            if (!new_params) return NULL;
            memcpy(new_params, params, count * sizeof(char *));
            params = new_params;
        }
        Token param = expect(parser, TOK_ID, "expected param name");
        if (param.type == TOK_ERROR) return NULL;

        Token next = peek(parser);
        if (next.type == TOK_IF || next.type == TOK_LAMBDA || next.type == TOK_LET) {
            fprintf(stderr, "SHEQ: keyword cannot be param name\n");
            return NULL;
        }

        for (int i = 0; i < count; i++) {
            if (strcmp(params[i], param.text) == 0) {
                fprintf(stderr, "SHEQ: duplicate param '%s'\n", param.text);
                return NULL;
            }
        }
        params[count++] = param.text;
    }

    expect(parser, TOK_COLON, "lambda needs ':'");
    ASTNode *body = parse_expr(parser);
    if (!body) return NULL;
    return make_lambda(parser->arena, count, params, body);
}

ASTNode *parse_app(Parser *parser, ASTNode *func) {
    int cap = 8;
    int count = 0;
    ASTNode **args = arena_alloc(parser->arena, sizeof(ASTNode *) * cap);
    if (!args) return NULL;

    while (peek(parser).type != TOK_RBRACE) {
        if (count >= cap) {
            cap *= 2;
            ASTNode **new_args = arena_alloc(parser->arena, sizeof(ASTNode *) * cap);
            if (!new_args) return NULL;
            memcpy(new_args, args, count * sizeof(ASTNode *));
            args = new_args;
        }
        args[count] = parse_expr(parser);
        if (!args[count]) return NULL;
        count++;
    }
    return make_app(parser->arena, func, count, args);
}

ASTNode *parse_if(Parser *parser) {
    ASTNode *test = parse_expr(parser);
    if (!test) return NULL;
    ASTNode *then_expr = parse_expr(parser);
    if (!then_expr) return NULL;
    ASTNode *else_expr = parse_expr(parser);
    if (!else_expr) return NULL;
    return make_if(parser->arena, test, then_expr, else_expr);
}

// desugars to ((lambda (names...) body) vals...)
ASTNode *parse_let(Parser *parser) {
    expect(parser, TOK_LBRACE, "let needs '{'");

    int cap = 8;
    int count = 0;
    char **names = arena_alloc(parser->arena, sizeof(char *) * cap);
    ASTNode **vals = arena_alloc(parser->arena, sizeof(ASTNode *) * cap);
    if (!names || !vals) return NULL;

    while (match(parser, TOK_LBRACKET)) {
        if (count >= cap) {
            cap *= 2;
            char **new_names = arena_alloc(parser->arena, sizeof(char *) * cap);
            ASTNode **new_vals = arena_alloc(parser->arena, sizeof(ASTNode *) * cap);
            if (!new_names || !new_vals) return NULL;
            memcpy(new_names, names, count * sizeof(char *));
            memcpy(new_vals, vals, count * sizeof(ASTNode *));
            names = new_names;
            vals = new_vals;
        }

        Token name = expect(parser, TOK_ID, "expected binding name");
        if (name.type == TOK_ERROR) return NULL;

        Token next = peek(parser);
        if (next.type == TOK_IF || next.type == TOK_LAMBDA || next.type == TOK_LET) {
            fprintf(stderr, "SHEQ: keyword cannot be binding name\n");
            return NULL;
        }

        for (int i = 0; i < count; i++) {
            if (strcmp(names[i], name.text) == 0) {
                fprintf(stderr, "SHEQ: duplicate binding '%s'\n", name.text);
                return NULL;
            }
        }
        names[count] = name.text;

        expect(parser, TOK_EQUALS, "binding needs '='");
        vals[count] = parse_expr(parser);
        if (!vals[count]) return NULL;
        count++;

        expect(parser, TOK_RBRACKET, "binding needs ']'");
    }

    expect(parser, TOK_RBRACE, "let needs '}'");
    expect(parser, TOK_IN, "let needs 'in'");
    ASTNode *body = parse_expr(parser);
    if (!body) return NULL;
    expect(parser, TOK_END, "let needs 'end'");

    ASTNode *lam = make_lambda(parser->arena, count, names, body);
    if (!lam) return NULL;
    return make_app(parser->arena, lam, count, vals);
}

ASTNode *parse_braced(Parser *parser) {
    expect(parser, TOK_LBRACE, "expected '{'");
    Token tok = peek(parser);
    ASTNode *node;

    if (tok.type == TOK_IF) {
        advance(parser);
        node = parse_if(parser);
    }
    else if (tok.type == TOK_LAMBDA) {
        advance(parser);
        node = parse_lambda(parser);
    }
    else if (tok.type == TOK_LET) {
        advance(parser);
        node = parse_let(parser);
    }
    else {
        ASTNode *func = parse_expr(parser);
        if (!func) return NULL;
        node = parse_app(parser, func);
    }

    if (!node) return NULL;
    expect(parser, TOK_RBRACE, "expected '}'");
    return node;
}

// token stream -> ExprC (AST node); NULL on syntax error
ASTNode *parse_expr(Parser *parser) {
    Token tok = peek(parser);

    switch (tok.type) {
        case TOK_LBRACE:
            return parse_braced(parser);
        case TOK_NUMBER: {
            advance(parser);
            return make_num(parser->arena, strtod(tok.text, NULL));
        }
        case TOK_STRING: {
            advance(parser);
            // strip surrounding quotes from token text
            size_t len = strlen(tok.text) - 2;
            return make_str(parser->arena, tok.text + 1, len);
        }
        case TOK_ID:
        case TOK_TRUE:
        case TOK_FALSE:
            advance(parser);
            return make_id(parser->arena, tok.text);
        default:
            fprintf(stderr, "SHEQ: unexpected token at line %d col %d\n", tok.line, tok.col);
            return NULL;
    }
}

// Value -> string representation (caller must free)
char *serialize(Value *val) {
    // static buffer simplifies memory management; 4KB sufficient for typical values
    static char buf[4096];
    switch (val->type) {
        case VAL_NUMV:
            snprintf(buf, sizeof(buf), "%.15g", val->as.num);
            break;
        case VAL_STRV: {
            char *ptr = buf;
            *ptr++ = '"';
            // stop at 4000 to leave room for escape expansion and closing quote
            for (size_t i = 0; i < val->as.str.len && ptr < buf + 4000; i++) {
                char ch = val->as.str.data[i];
                if (ch == '"') { *ptr++ = '\\'; *ptr++ = '"'; }
                else if (ch == '\\') { *ptr++ = '\\'; *ptr++ = '\\'; }
                else if (ch == '\n') { *ptr++ = '\\'; *ptr++ = 'n'; }
                else *ptr++ = ch;
            }
            *ptr++ = '"';
            *ptr = '\0';
            break;
        }
        case VAL_BOOLV:
            snprintf(buf, sizeof(buf), "%s", val->as.boolval ? "true" : "false");
            break;
        case VAL_CLOSV:
            snprintf(buf, sizeof(buf), "#<procedure>");
            break;
        case VAL_PRIMV:
            snprintf(buf, sizeof(buf), "#<primop>");
            break;
        default:
            snprintf(buf, sizeof(buf), "#<unknown>");
    }
    return strdup(buf);
}

const char *type_str(ValueType type) {
    switch (type) {
        case VAL_NUMV: return "number";
        case VAL_STRV: return "string";
        case VAL_BOOLV: return "boolean";
        case VAL_CLOSV: return "closure";
        case VAL_PRIMV: return "primitive";
        default: return "unknown";
    }
}

int check_type(Value *val, ValueType want, const char *op) {
    if (val->type != want) {
        fprintf(stderr, "SHEQ: %s expects %s, got %s\n", op, type_str(want), type_str(val->type));
        return 0;
    }
    return 1;
}

Value *interp(ASTNode *node, Env *env, Arena *arena);

Value *prim_add(Value *args, int argc, Arena *arena) {
    if (argc != 2) { fprintf(stderr, "SHEQ: + needs 2 args\n"); return NULL; }
    if (!check_type(&args[0], VAL_NUMV, "+")) return NULL;
    if (!check_type(&args[1], VAL_NUMV, "+")) return NULL;
    Value *out = arena_alloc(arena, sizeof(Value));
    if (!out) return NULL;
    out->type = VAL_NUMV;
    out->as.num = args[0].as.num + args[1].as.num;
    return out;
}

Value *prim_sub(Value *args, int argc, Arena *arena) {
    if (argc != 2) { fprintf(stderr, "SHEQ: - needs 2 args\n"); return NULL; }
    if (!check_type(&args[0], VAL_NUMV, "-")) return NULL;
    if (!check_type(&args[1], VAL_NUMV, "-")) return NULL;
    Value *out = arena_alloc(arena, sizeof(Value));
    if (!out) return NULL;
    out->type = VAL_NUMV;
    out->as.num = args[0].as.num - args[1].as.num;
    return out;
}

Value *prim_mul(Value *args, int argc, Arena *arena) {
    if (argc != 2) { fprintf(stderr, "SHEQ: * needs 2 args\n"); return NULL; }
    if (!check_type(&args[0], VAL_NUMV, "*")) return NULL;
    if (!check_type(&args[1], VAL_NUMV, "*")) return NULL;
    Value *out = arena_alloc(arena, sizeof(Value));
    if (!out) return NULL;
    out->type = VAL_NUMV;
    out->as.num = args[0].as.num * args[1].as.num;
    return out;
}

Value *prim_div(Value *args, int argc, Arena *arena) {
    if (argc != 2) { fprintf(stderr, "SHEQ: / needs 2 args\n"); return NULL; }
    if (!check_type(&args[0], VAL_NUMV, "/")) return NULL;
    if (!check_type(&args[1], VAL_NUMV, "/")) return NULL;
    if (args[1].as.num == 0.0) {
        fprintf(stderr, "SHEQ: division by zero\n");
        return NULL;
    }
    Value *out = arena_alloc(arena, sizeof(Value));
    if (!out) return NULL;
    out->type = VAL_NUMV;
    out->as.num = args[0].as.num / args[1].as.num;
    return out;
}

Value *prim_lte(Value *args, int argc, Arena *arena) {
    if (argc != 2) { fprintf(stderr, "SHEQ: <= needs 2 args\n"); return NULL; }
    if (!check_type(&args[0], VAL_NUMV, "<=")) return NULL;
    if (!check_type(&args[1], VAL_NUMV, "<=")) return NULL;
    Value *out = arena_alloc(arena, sizeof(Value));
    if (!out) return NULL;
    out->type = VAL_BOOLV;
    out->as.boolval = (args[0].as.num <= args[1].as.num);
    return out;
}

int str_eq(Value *lhs, Value *rhs) {
    if (lhs->as.str.len != rhs->as.str.len) return 0;
    return memcmp(lhs->as.str.data, rhs->as.str.data, lhs->as.str.len) == 0;
}

Value *prim_equal(Value *args, int argc, Arena *arena) {
    if (argc != 2) { fprintf(stderr, "SHEQ: equal? needs 2 args\n"); return NULL; }
    Value *lhs = &args[0], *rhs = &args[1];
    int eq = 0;

    if (lhs->type != rhs->type) {
        eq = 0;
    } else if (lhs->type == VAL_CLOSV || lhs->type == VAL_PRIMV) {
        // closures/prims never equal
        eq = 0;
    } else {
        switch (lhs->type) {
            case VAL_NUMV:  eq = (lhs->as.num == rhs->as.num); break;
            case VAL_STRV:  eq = str_eq(lhs, rhs); break;
            case VAL_BOOLV: eq = (lhs->as.boolval == rhs->as.boolval); break;
            default: eq = 0;
        }
    }

    Value *out = arena_alloc(arena, sizeof(Value));
    if (!out) return NULL;
    out->type = VAL_BOOLV;
    out->as.boolval = eq;
    return out;
}

Value *prim_substring(Value *args, int argc, Arena *arena) {
    if (argc != 3) { fprintf(stderr, "SHEQ: substring needs 3 args\n"); return NULL; }
    if (!check_type(&args[0], VAL_STRV, "substring")) return NULL;
    if (!check_type(&args[1], VAL_NUMV, "substring")) return NULL;
    if (!check_type(&args[2], VAL_NUMV, "substring")) return NULL;

    int start = (int)args[1].as.num;
    int stop = (int)args[2].as.num;
    size_t len = args[0].as.str.len;

    if (start < 0 || start > (int)len) {
        fprintf(stderr, "SHEQ: substring start %d out of bounds\n", start);
        return NULL;
    }
    if (stop < start || stop > (int)len) {
        fprintf(stderr, "SHEQ: substring stop %d out of bounds\n", stop);
        return NULL;
    }

    Value *out = arena_alloc(arena, sizeof(Value));
    if (!out) return NULL;
    out->type = VAL_STRV;
    out->as.str.len = stop - start;
    out->as.str.data = arena_alloc(arena, out->as.str.len + 1);
    if (!out->as.str.data) return NULL;
    memcpy(out->as.str.data, args[0].as.str.data + start, out->as.str.len);
    out->as.str.data[out->as.str.len] = '\0';
    return out;
}

Value *prim_strlen(Value *args, int argc, Arena *arena) {
    if (argc != 1) { fprintf(stderr, "SHEQ: strlen needs 1 arg\n"); return NULL; }
    if (!check_type(&args[0], VAL_STRV, "strlen")) return NULL;
    Value *out = arena_alloc(arena, sizeof(Value));
    if (!out) return NULL;
    out->type = VAL_NUMV;
    out->as.num = (double)args[0].as.str.len;
    return out;
}

Value *prim_error(Value *args, int argc, Arena *arena) {
    (void)arena;
    if (argc != 1) { fprintf(stderr, "SHEQ: error needs 1 arg\n"); return NULL; }
    char *msg = serialize(&args[0]);
    fprintf(stderr, "SHEQ: user-error: %s\n", msg);
    free(msg);
    return NULL;
}

// (ExprC, Env) -> Value; NULL on runtime error
Value *interp(ASTNode *node, Env *env, Arena *arena) {
    if (!node) {
        fprintf(stderr, "SHEQ: null AST\n");
        return NULL;
    }

    Value *out = arena_alloc(arena, sizeof(Value));
    if (!out) return NULL;

    switch (node->type) {
        case NODE_NUMC:
            out->type = VAL_NUMV;
            out->as.num = node->as.num_val;
            return out;

        case NODE_STRC:
            out->type = VAL_STRV;
            out->as.str.data = node->as.str_val;
            out->as.str.len = strlen(node->as.str_val);
            return out;

        case NODE_IDC: {
            Value *val = lookup(env, node->as.var);
            if (!val) {
                fprintf(stderr, "SHEQ: unbound: %s\n", node->as.var);
                return NULL;
            }
            return val;
        }

        case NODE_IFC: {
            Value *test_val = interp(node->as.if_node.test, env, arena);
            if (!test_val) return NULL;
            if (!check_type(test_val, VAL_BOOLV, "if")) return NULL;
            return test_val->as.boolval
                ? interp(node->as.if_node.then_expr, env, arena)
                : interp(node->as.if_node.else_expr, env, arena);
        }

        case NODE_LAMC:
            out->type = VAL_CLOSV;
            out->as.clos.param_count = node->as.lam_node.param_count;
            out->as.clos.params = node->as.lam_node.params;
            out->as.clos.body = node->as.lam_node.body;
            out->as.clos.env = env;
            return out;

        case NODE_APPC: {
            ASTNode **children = node->as.app_node.children;
            int n_args = node->as.app_node.child_count - 1;

            Value *func = interp(children[0], env, arena);
            if (!func) return NULL;

            Value *argv = NULL;
            if (n_args > 0) {
                argv = arena_alloc(arena, sizeof(Value) * n_args);
                if (!argv) return NULL;
            }
            for (int i = 0; i < n_args; i++) {
                Value *arg = interp(children[i + 1], env, arena);
                if (!arg) return NULL;
                argv[i] = *arg;
            }

            if (func->type == VAL_CLOSV) {
                if (func->as.clos.param_count != n_args) {
                    fprintf(stderr, "SHEQ: arity mismatch: want %d, got %d\n",
                            func->as.clos.param_count, n_args);
                    return NULL;
                }
                // extend closure's captured env, not call-site env (lexical scoping)
                Env *call_env = extend_env(arena, func->as.clos.env,
                                           func->as.clos.param_count,
                                           func->as.clos.params, argv);
                if (!call_env) return NULL;
                return interp(func->as.clos.body, call_env, arena);
            }
            else if (func->type == VAL_PRIMV) {
                return func->as.prim(argv, n_args, arena);
            }
            else {
                fprintf(stderr, "SHEQ: cannot apply non-function\n");
                return NULL;
            }
        }
    }

    fprintf(stderr, "SHEQ: unknown node type\n");
    return NULL;
}

// top-level env with primitives (+, -, *, /, <=, equal?, etc.) and true/false
Env *make_top_env(Arena *arena) {
    Env *env = create_env(arena, NULL);
    if (!env) return NULL;

    Value prim_val;
    prim_val.type = VAL_PRIMV;

    prim_val.as.prim = prim_add;        if (!bind_env(arena, env, "+", prim_val)) return NULL;
    prim_val.as.prim = prim_sub;        if (!bind_env(arena, env, "-", prim_val)) return NULL;
    prim_val.as.prim = prim_mul;        if (!bind_env(arena, env, "*", prim_val)) return NULL;
    prim_val.as.prim = prim_div;        if (!bind_env(arena, env, "/", prim_val)) return NULL;
    prim_val.as.prim = prim_lte;        if (!bind_env(arena, env, "<=", prim_val)) return NULL;
    prim_val.as.prim = prim_equal;      if (!bind_env(arena, env, "equal?", prim_val)) return NULL;
    prim_val.as.prim = prim_substring;  if (!bind_env(arena, env, "substring", prim_val)) return NULL;
    prim_val.as.prim = prim_strlen;     if (!bind_env(arena, env, "strlen", prim_val)) return NULL;
    prim_val.as.prim = prim_error;      if (!bind_env(arena, env, "error", prim_val)) return NULL;

    Value bool_val;
    bool_val.type = VAL_BOOLV;
    bool_val.as.boolval = 1; if (!bind_env(arena, env, "true", bool_val)) return NULL;
    bool_val.as.boolval = 0; if (!bind_env(arena, env, "false", bool_val)) return NULL;

    return env;
}

// source string -> prints serialized result; returns 0 on success
int top_interp(const char *src) {
    // 1MB sufficient for typical programs with deep nesting
    Arena *arena = arena_create(1024 * 1024);
    if (!arena) return 1;

    TokenStream *ts = tokenize(arena, src);
    if (!ts) { arena_destroy(arena); return 1; }

    Parser parser = {ts, arena};
    ASTNode *ast = parse_expr(&parser);
    if (!ast) { arena_destroy(arena); return 1; }

    Env *env = make_top_env(arena);
    if (!env) { arena_destroy(arena); return 1; }

    Value *val = interp(ast, env, arena);
    if (!val) { arena_destroy(arena); return 1; }

    char *out = serialize(val);
    if (out) {
        printf("%s\n", out);
        free(out);
    }

    arena_destroy(arena);
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: sheq4 '<expr>'\n");
        return 1;
    }
    return top_interp(argv[1]);
}