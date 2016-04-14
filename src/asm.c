#include "asm.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "node.h"
#include "graph.h"

/* Basic assembler.
 * Parse a source string and generate the equivalent function graph.
 * Uses ndl_asm_script during generation.
 */

typedef struct ndl_asm_script_s {

    ndl_ref root, label_node, curr_inst, badref_head;

    ndl_graph *graph;

    const char *err;
    long int line, column;
} ndl_asm_script;

/* Initialize the graph datastructure for generation.
 * Has a root node, a label mapping node, the first instruction.
 * Root has pointers to the label, the current, and the head(same as current.)
 */
static int ndl_asm_parse_init(ndl_asm_script *script) {

    ndl_ref root = ndl_graph_alloc(script->graph);
    if (root == NDL_NULL_REF)
        return -1;

    ndl_ref labels = ndl_graph_salloc(script->graph, root, NDL_SYM("labels  "));
    if (labels == NDL_NULL_REF)
        return -1;

    ndl_ref head = ndl_graph_salloc(script->graph, root, NDL_SYM("head    "));
    if (head == NDL_NULL_REF)
        return -1;

    int err = ndl_graph_set(script->graph, root, NDL_SYM("curr    "), NDL_VALUE(EVAL_REF, ref=head));
    if (err != 0)
        return -1;

    script->root = root;
    script->label_node = labels;
    script->curr_inst = head;
    script->badref_head = NDL_NULL_REF;

    script->err = NULL;
    script->line = 0;
    script->column = 0;

    return 0;
}

/* Attempt to handle failure gracefully.
 * Unmarks the root, calls clean.
 */
static void ndl_asm_parse_kill(ndl_asm_script *script) {

    int err = ndl_graph_unmark(script->graph, script->root);

    if (err == 0)
        ndl_graph_clean(script->graph);
}

/* Useful macros. */
#define IS_TOKEN_SYMBOL(c) (((c >= 'a') && (c <= 'z')) ||       \
                            ((c >= 'A') && (c <= 'Z')) ||       \
                            ((c == '_') || (c == '-')))
#define IS_TOKEN_ISYMBOL(c) (((c >= 'a') && (c <= 'z')) ||      \
                             ((c >= 'A') && (c <= 'Z')) ||      \
                             ((c >= '0') && (c <= '9')) ||      \
                             ((c == '_') || (c == '-')))
#define IS_TOKEN_NUM(c) (((c >= '0') && (c <= '9')) || (c == '-'))
#define IS_TOKEN_INUM(c) (((c >= '0') && (c <= '9')))

#define IS_TOKEN_OBJ(c) (IS_TOKEN_SYMBOL(c) || \
                         IS_TOKEN_LABEL(c) || \
                         IS_TOKEN_NUM(c))

#define IS_TOKEN_WS(c) ((c == ' ') || (c == '\t'))
#define IS_TOKEN_EOL(c) ((c == '\n') || (c == '\r'))
#define IS_TOKEN_SEP(c) ((c == '-') || (c == ',') || (c == '.'))

#define IS_TOKEN_LABEL(c) (c == ':')
#define IS_TOKEN_NUMSEP(c) (c == '.')
#define IS_TOKEN_COMMENT(c) (c == '#')
#define IS_TOKEN_BAR(c) (c == '|')
#define IS_TOKEN_EQ(c) (c == '=')
#define IS_TOKEN_NEG(c) (c == '-')
#define IS_TOKEN_EOS(c) (c == '\0')

#define PARSEFAIL(msg)  \
    do {                \
        env->err = msg; \
        return -1;      \
    } while (0)

/* Various primitives. */
static inline long int ndl_asm_parse_eat_ws(const char *src, ndl_asm_script *env) {

    const char *curr = src;

    while (IS_TOKEN_WS(curr[0]))
           curr++;

    env->column += curr - src;

    return curr - src;
}

static inline long int ndl_asm_parse_eat_comment(const char *src, ndl_asm_script *env) {

    const char *curr = src;
    while (!IS_TOKEN_EOL(curr[0]) && !IS_TOKEN_EOS(curr[0]))
        curr++;

    if (IS_TOKEN_EOS(curr[0]))
        return curr - src;
    else
        return curr - src + 1;
}

static inline long int ndl_asm_parse_eat_sym(const char *src, ndl_sym *ret, ndl_asm_script *env) {

    const char *curr = src;
    while (IS_TOKEN_ISYMBOL(curr[0])) curr++;

    env->column += curr - src;

    long int size = curr - src;
    if (size > 8)
        PARSEFAIL("Symbols must be eight characters or fewer.");

    if (size == 0)
        PARSEFAIL("Expected symbol.");

    if (ret == NULL)
        return size;

    long int i;
    for (i = 0; i < 8; i++)
        ((char *) ret)[i] = (i < size)? src[i] : ' ';

    return size;
}

static inline long int ndl_asm_parse_eat_label(const char *src, ndl_sym *ret, ndl_asm_script *env) {

    if (!IS_TOKEN_LABEL(src[0]))
        PARSEFAIL("Expected label.");

    long int off = ndl_asm_parse_eat_sym(src + 1, ret, env);
    if (off < 0)
        return -1;

    env->column++;

    return off + 1;
}

static inline long int ndl_asm_parse_marker(const char *src, ndl_asm_script *env) {

    ndl_sym sym;
    long int off = ndl_asm_parse_eat_sym(src, &sym, env);
    if (off < 0)
        return -1;

    int err = ndl_graph_set(env->graph, env->label_node, sym, NDL_VALUE(EVAL_REF, ref=env->curr_inst));
    if (err != 0)
        PARSEFAIL("Failed to store label: internal error.");

    const char *curr = src + off + 1;

    while (IS_TOKEN_WS(curr[0]))
        curr++;

    env->column += curr - src;

    if (IS_TOKEN_COMMENT(curr[0]))
        return (curr - src) + ndl_asm_parse_eat_comment(curr, env);
    else if (IS_TOKEN_EOL(curr[0]))
        return (curr - src) + 1;
    else if (IS_TOKEN_EOS(curr[0]))
        return (curr - src);
    else
        PARSEFAIL("Expected comment, end of line, or end of input.");
}

static long int ndl_asm_parse_num(const char *src, ndl_asm_script *env, ndl_sym argname) {

    int inv = 0;
    if (IS_TOKEN_NEG(src[0])) {
        inv = 1;
        src++;
        env->column++;
    }

    int64_t ival = 0;
    const char *search = src;
    while (IS_TOKEN_INUM(search[0])) {
        ival *= 10;
        ival += (search[0] - '0');
        search++;
        env->column++;
    }

    ndl_value val;
    if (!IS_TOKEN_NUMSEP(search[0])) {
        val.type = EVAL_INT;
        val.num = ival;
    } else {
        val.type = EVAL_FLOAT;
        val.real = (double) ival;
        double scale = 0.1;

        if (!IS_TOKEN_INUM(search[1]))
            PARSEFAIL("Expected decimal portion of floating point number.");

        env->column++;
        search++;
        while (IS_TOKEN_INUM(search[0])) {
            val.real += scale * (search[0] - '0');
            scale *= 0.1;
            search++;
            env->column++;
        }
    }

    if (inv) {
        if (val.type == EVAL_FLOAT)
            val.real = - val.real;
        else
            val.num = - val.num;
    }

    int err = ndl_graph_set(env->graph, env->curr_inst, argname, val);
    if (err != 0)
        PARSEFAIL("Failed to store number argument: internal error.");

    if (inv)
        return search - src + 1;
    else
        return search - src;
}

static long int ndl_asm_parse_label(const char *src, ndl_asm_script *env, ndl_sym argname) {

    ndl_sym ret;
    long int off = ndl_asm_parse_eat_label(src, &ret, env);
    if (off < 0)
        return -1;

    /* If already in symbol table, resolve. Else, push to badref list. */
    ndl_value to = ndl_graph_get(env->graph, env->label_node, ret);
    if (to.type == EVAL_REF || to.ref != NDL_NULL_REF) {

        int err = ndl_graph_set(env->graph, env->curr_inst, argname, to);
        if (err != 0)
            PARSEFAIL("Failed to store resolved reference/label: internal error.");
    } else {

        ndl_ref badref = ndl_graph_salloc(env->graph, env->root, NDL_SYM("brefhead"));
        if (badref == NDL_NULL_REF)
            PARSEFAIL("Failed to store delayed label resolution node: internal error.");

        int err = 0;
        err |= ndl_graph_set(env->graph, badref, NDL_SYM("label   "), NDL_VALUE(EVAL_SYM, sym=ret));
        err |= ndl_graph_set(env->graph, badref, NDL_SYM("inst    "), NDL_VALUE(EVAL_REF, ref=env->curr_inst));
        err |= ndl_graph_set(env->graph, badref, NDL_SYM("symbol  "), NDL_VALUE(EVAL_SYM, sym=argname));
        err |= ndl_graph_set(env->graph, badref, NDL_SYM("line    "), NDL_VALUE(EVAL_INT, num=env->line));
        err |= ndl_graph_set(env->graph, badref, NDL_SYM("column  "), NDL_VALUE(EVAL_INT, num=(env->column - off)));
        err |= ndl_graph_set(env->graph, badref, NDL_SYM("brefnext"), NDL_VALUE(EVAL_REF, ref=env->badref_head));
        if (err != 0)
            PARSEFAIL("Failed to store data to delayed label resolution node: internal error.");

        env->badref_head = badref;
    }

    return off;
}

static long int ndl_asm_parse_sym(const char *src, ndl_asm_script *env, ndl_sym argname) {

    ndl_sym ret;
    long int off = ndl_asm_parse_eat_sym(src, &ret, env);
    if (off < 0)
        return -1;

    int err = ndl_graph_set(env->graph, env->curr_inst, argname, NDL_VALUE(EVAL_SYM, sym=ret));
    if (err != 0)
        PARSEFAIL("Failed to store symbol argument: internal error.");

    return off;
}

static long int ndl_asm_parse_obj(const char *src, ndl_asm_script *env, ndl_sym argname) {

    if (IS_TOKEN_NUM(src[0]))
        return ndl_asm_parse_num(src, env, argname);
    if (IS_TOKEN_LABEL(src[0]))
        return ndl_asm_parse_label(src, env, argname);
    if (IS_TOKEN_SYMBOL(src[0]))
        return ndl_asm_parse_sym(src, env, argname);
    else
        PARSEFAIL("Expected number, label, or symbol.");
}

static long int ndl_asm_parse_arglist(const char *src, ndl_asm_script *env) {

    char symname[8];
    memcpy(symname, "sym     ", 8);
    char *sym = symname;

    const char *curr = src;

    /* Eat whitespace. */
    while (IS_TOKEN_WS(curr[0]))
        curr++;

    env->column += curr - src;

    if (IS_TOKEN_EOL(curr[0]))
        return (curr - src) + 1;
    else if (IS_TOKEN_EOS(curr[0]))
        return (curr - src);

    /* Eat required first object. */
    if (!IS_TOKEN_OBJ(curr[0]))
        return (curr - src);

    symname[3] = 'a';
    long int off = ndl_asm_parse_obj(curr, env, NDL_SYM(sym));
    if (off < 0)
        return -1;

    curr += off;

    long int it;
    for (it = 1; it < 26; it++) {

        const char *t = curr;

        /* Eat whitespace. */
        while (IS_TOKEN_WS(curr[0]))
            curr++;

        env->column += curr - t;

        /* Eat separator. */
        if (!IS_TOKEN_SEP(curr[0]))
            return curr - src;

        if (curr[0] == '-') {
            if (curr[1] != '>')
                PARSEFAIL("Expected separator.");
            curr += 2;
            env->column += 2;
        } else {
            curr += 1;
            env->column += 1;
        }

        t = curr;

        /* Eat whitespace. */
        while (IS_TOKEN_WS(curr[0]))
            curr++;

        env->column += curr - t;

        /* Eat argument. */
        if (!IS_TOKEN_OBJ(curr[0]))
            PARSEFAIL("Expected argument.");

        symname[3] = (char) ('a' + it);
        off = ndl_asm_parse_obj(curr, env, NDL_SYM(sym));
        if (off < 0)
            return -1;

        curr += off;
    }

    PARSEFAIL("Opcodes must have fewer than 26 arguments.");
}

/* WS* SYMBOL WS* '=' WS* OBJECT */

static long int ndl_asm_parse_kvlist_pair(const char *src, ndl_asm_script *env) {

    const char *curr = src;

    /* Chew whitespace. */
    while (IS_TOKEN_WS(curr[0]))
        curr++;

    env->column += curr - src;

    /* Read symbol. */
    if (!IS_TOKEN_SYMBOL(curr[0]))
        return (curr - src);

    ndl_sym name;
    long int off = ndl_asm_parse_eat_sym(curr, &name, env);
    if (off < 0)
        return -1;

    curr += off;

    const char *t = curr;

    /* Chew whitespace. */
    while (IS_TOKEN_WS(curr[0]))
        curr++;

    env->column += curr - t;

    /* Chew '='. */
    if (!IS_TOKEN_EQ(curr[0]))
        PARSEFAIL("Expected '=' in key-value list.");

    curr++;
    env->column++;

    t = curr;

    /* Chew whitespace. */
    while (IS_TOKEN_WS(curr[0]))
        curr++;

    env->column += curr - t;

    /* Chew object. */
    off = ndl_asm_parse_obj(curr, env, name);
    if (off < 0)
        return -1;

    curr += off;

    return curr - src;
}

static long int ndl_asm_parse_kvlist(const char *src, ndl_asm_script *env) {

    if (!IS_TOKEN_BAR(src[0]))
        return 0;

    const char *curr = src + 1;

    env->column++;

    long int off = 1;
    while (off > 0) {

        off = ndl_asm_parse_kvlist_pair(curr, env);
        if (off < 0)
            return -1;

        curr += off;
    }

    return curr - src;
}

/* Parse a line starting with a symbol.
 * If we're dealing with a label, rewind, return parse_marker().
 * Else, we're parsing a typical opcode.
 * parse the opcode, save to node.
 * parse the symbol list.
 * parse the kv pairs.
 * - Comment: eat, return.
 * - EOL: eat, return.
 * - EOS: return.
 * - Error
 */
static long int ndl_asm_parse_nline(const char *src, ndl_asm_script *env) {

    ndl_sym opcode;
    const char *curr = src;
    long int off = ndl_asm_parse_eat_sym(curr, &opcode, env);
    if (off < 0)
        return -1;

    curr += off;

    if (IS_TOKEN_LABEL(curr[0])) {
        env->column -= off;
        return ndl_asm_parse_marker(src, env);
    }

    /* Parsing a regular opcode. */
    int err = ndl_graph_set(env->graph, env->curr_inst, NDL_SYM("opcode  "), NDL_VALUE(EVAL_SYM, sym=opcode));
    if (err != 0)
        PARSEFAIL("Failed to store opcode symbol: internal error.");

    off = ndl_asm_parse_arglist(curr, env);
    if (off < 0)
        return -1;

    curr += off;

    /* We create a new node each time. */
    ndl_ref next = ndl_graph_salloc(env->graph, env->curr_inst, NDL_SYM("next    "));
    if (next == NDL_NULL_REF)
        PARSEFAIL("Failed to create next instruction node: internal error.");

    /* Parse the KV list after salloc, overwrite self.next. */
    off = ndl_asm_parse_kvlist(curr, env);
    if (off < 0)
        return -1;

    curr += off;

    /* Update the instruction pointer. */
    env->curr_inst = next;

    if (IS_TOKEN_COMMENT(curr[0])) {
        return (curr - src) + ndl_asm_parse_eat_comment(curr, env);
    } else if (IS_TOKEN_EOL(curr[0])) {
        return (curr - src) + 1;
    } else if (IS_TOKEN_EOS(curr[0])) {
        return (curr - src);
    } else {
        PARSEFAIL("Expected comment, end of line, or end of input.");
    }
}

/* Switch src[0]:
 * - Whitespace: Eat whitespace, recurse.
 * - Comment: Eat comment, return.
 * - Symbol: Eat label|opcode, return.
 * - EOL: eat, return.
 * - Error
 * Eats the newline, if there is one.
 */
static long int ndl_asm_parse_line(const char *src, ndl_asm_script *env) {

    long int off;
    if (IS_TOKEN_WS(src[0])) {

        off = ndl_asm_parse_eat_ws(src, env);

        return ndl_asm_parse_line(src + off, env) + off;

    } else if (IS_TOKEN_COMMENT(src[0])) {

        return ndl_asm_parse_eat_comment(src, env);

    } else if (IS_TOKEN_EOL(src[0])) {

        return 1;

    } else if (IS_TOKEN_EOS(src[0])) {

        return 0;

    } else if (IS_TOKEN_SYMBOL(src[0])) {

        return ndl_asm_parse_nline(src, env);

    } else {

        PARSEFAIL("Expected whitespace, comment, end of line, end of string, symbol, or label.");
    }
}

ndl_asm_parse_res ndl_asm_parse(const char *source, ndl_graph *using) {

    ndl_asm_script env;
    ndl_asm_parse_res res;

    res.src = source;
    res.root = NDL_NULL_SYM;
    res.msg = NULL;
    res.line = -1;
    res.column = -1;

    if (using == NULL) {
        res.graph = env.graph = ndl_graph_init();
        if (env.graph == NULL) {
            res.msg = "Failed to allocate graph: internal error.";
            return res;
        }
    } else {
        env.graph = res.graph = using;
    }

    /* Initialize the graph datastructure. */
    if (ndl_asm_parse_init(&env) != 0) {
        ndl_asm_parse_kill(&env);
        if (using == NULL)
            ndl_graph_kill(env.graph);
        res.graph = NULL;
        res.msg = "Failed to create root nodes: internal error.";
        return res;
    }

    res.head = env.curr_inst;
    res.root = env.root;

    /* Parse each line. */
    while (*source != '\0') {
        long int used = ndl_asm_parse_line(source, &env);
        if (used <= 0) {
            ndl_asm_parse_kill(&env);
            if (using == NULL)
                ndl_graph_kill(env.graph);
            res.msg = env.err;
            res.line = env.line;
            res.column = env.column;
            if (res.msg == NULL)
                res.msg = "Unknown error.";
            res.graph = NULL;
            return res;
        }
        env.line++;
        env.column = 0;
        source += used;
    }

    /* Resolve any forward/bad references, then free badref list. */
    if (env.badref_head != NDL_NULL_REF) {

        while (env.badref_head != NDL_NULL_REF) {

            ndl_value inst = ndl_graph_get(env.graph, env.badref_head, NDL_SYM("inst    "));
            ndl_value label = ndl_graph_get(env.graph, env.badref_head, NDL_SYM("label   "));
            ndl_value symbol = ndl_graph_get(env.graph, env.badref_head, NDL_SYM("symbol  "));
            ndl_value line = ndl_graph_get(env.graph, env.badref_head, NDL_SYM("line    "));
            ndl_value column = ndl_graph_get(env.graph, env.badref_head, NDL_SYM("column  "));
            if (((inst.type != EVAL_REF) || (symbol.type != EVAL_SYM) || (label.type != EVAL_SYM) ||
                 (inst.ref == NDL_NULL_REF) || (symbol.sym == NDL_NULL_SYM) || (label.sym == NDL_NULL_SYM) ||
                 (line.type != EVAL_INT) || (line.type != EVAL_INT))) {
                ndl_asm_parse_kill(&env);
                if (using == NULL)
                    ndl_graph_kill(env.graph);
                res.msg = "Failed to load delayed label resolution node: internal error. No line number available.";
                res.graph = NULL;
                return res;
            }

            ndl_value dest = ndl_graph_get(env.graph, env.label_node, label.sym);
            if (dest.type != EVAL_REF) {
                ndl_asm_parse_kill(&env);
                if (using == NULL)
                    ndl_graph_kill(env.graph);
                res.msg = "Failed to find label in delayed reference. Possibly internal error, probably bad label.";
                res.graph = NULL;
                res.line = line.num;
                res.column = column.num;
                return res;
            }

            int err = ndl_graph_set(env.graph, inst.ref, symbol.sym, NDL_VALUE(EVAL_REF, ref=dest.ref));
            ndl_value next = ndl_graph_get(env.graph, env.badref_head, NDL_SYM("brefnext"));
            if ((next.type != EVAL_REF) || (err != 0)) {
                ndl_asm_parse_kill(&env);
                if (using == NULL)
                    ndl_graph_kill(env.graph);
                res.msg = "Failed to resolve delayed reference: internal error.";
                res.graph = NULL;
                res.line = line.num;
                res.column = column.num;
                return res;
            }

            env.badref_head = next.ref;
        }

        int err = ndl_graph_del(env.graph, env.root, NDL_SYM("brefhead"));
        if (err != 0) {
            ndl_asm_parse_kill(&env);
            if (using == NULL)
                ndl_graph_kill(env.graph);
            res.msg = "Failed to delete resolved delayed references: internal error.";
            res.graph = NULL;
            return res;
        }

        ndl_graph_clean(env.graph);
    }

    return res;
}

void ndl_asm_print_err(ndl_asm_parse_res res) {

    if (res.msg == NULL) {

        printf("Successfully assembled program.\n");
        printf("Root node, graph pointer: %d@%p\n", res.root, (void *) res.graph);
        printf("#----|Program|----------------#\n");
        printf("%s", res.src);
        printf("#-----------------------------#\n");

        return;
    }

    printf("Failed to assemble program.\n");
    printf("%03ld:%03ld: %s\n", res.line, res.column, res.msg);

    if (res.src == NULL) {
        printf("Failed to print line: Source not included.\n");
    }

    long int line = res.line;
    const char *curr = res.src;
    while (line != 0) {

        while ((*curr != '\n') && (*curr != '\r') && (*curr != '\0'))
            curr++;

        if (*curr == '\0') {
            printf("Failed to print line: line number is out of range.\n");
            return;
        }

        curr++;
        line--;
    }

    const char *base = curr;

    while ((*curr != '\n') && (*curr != '\r') && (*curr != '\0'))
        curr++;
    if (*curr == '\0')
        printf("Failed to print line: line number is out of range.\n");

    char *nline = malloc((unsigned long) (curr - base + 1));
    memcpy(nline, base, (unsigned long) (curr - base));
    nline[curr - base] = '\0';

    puts(nline);
    if ((res.column < (curr - base)) && (res.column > 0)) {
        memset(nline, ' ', (unsigned long) (curr - base));
        nline[res.column] = '^';
        puts(nline);
    }

    return;
}
