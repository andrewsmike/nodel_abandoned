#include "eval.h"

#include <math.h>

/* This file generates all of the opcode functions used by nodel.
 * For brevity, a wide variety of macros are provided.
 * May move to a separate description file and generator later.
 *
 * Assertion macros abort the thread if their condition is not met.
 * Load and store macros do as advertised. They may use assertions.
 * BEGINOP(), ADVANCE, and LOADSYM[a[b[c]]] are convenience macros.
 */

#define BEGINOP(name) ndl_ref ndl_opcode_ ## name(ndl_graph *graph, ndl_ref local, ndl_ref pc)

#define ASSERTTYPE(value, etype)  \
    do {                         \
        if (value.type != etype)  \
            return NDL_NULL_REF; \
    } while (0)

#define ASSERTNOTNONE(value)         \
    do {                             \
        if (value.type == EVAL_NONE) \
            return NDL_NULL_REF;     \
    } while (0)

#define ASSERTREF(value)               \
    ASSERTTYPE(value, EVAL_REF);       \
    do {                               \
        if (value.ref == NDL_NULL_REF) \
            return NDL_NULL_REF;       \
    } while (0)

#define LOAD(node, name, sym, type)             \
    ndl_value name;                             \
    do {                                        \
        name = ndl_graph_get(graph, node, sym); \
        ASSERTTYPE(name, type);                 \
    } while (0)

#define LOADREF(node, name, sym)                \
    ndl_value name;                             \
    do {                                        \
        name = ndl_graph_get(graph, node, sym); \
        ASSERTREF(name);                        \
    } while (0)

#define NTLOAD(node, name, sym)                 \
    ndl_value name;                             \
    do {                                        \
        name = ndl_graph_get(graph, node, sym); \
        ASSERTNOTNONE(name);                    \
    } while (0)

#define STORE(node, value, sym)                           \
    do {                                                  \
        int err = ndl_graph_set(graph, node, sym, value); \
        if (err != 0)                                     \
            return NDL_NULL_REF;                          \
    } while (0)

#define DROP(node, sym)                            \
    do {                                           \
        int err = ndl_graph_del(graph, node, sym); \
        if (err != 0)                              \
            return NDL_NULL_REF;                   \
    } while (0)

#define ADVANCE                             \
    do {                                    \
        LOADREF(pc, next, DS("next    "));  \
        STORE(local, next, DS("instpntr")); \
        return local;                       \
    } while (0)

#define DS(name) NDL_SYM(name)

#define LOADSYMA LOAD(pc, syma, DS("syma    "), EVAL_SYM)
#define LOADSYMAB LOADSYMA; LOAD(pc, symb, DS("symb    "), EVAL_SYM)
#define LOADSYMABC LOADSYMAB; LOAD(pc, symc, DS("symc    "), EVAL_SYM)

#define NEWLINKED(node, name, sym)                     \
    ndl_value name;                                    \
    do {                                               \
        name.type = EVAL_REF;                          \
        name.ref = ndl_graph_salloc(graph, node, sym); \
        ASSERTREF(name);                               \
    } while (0)

BEGINOP(new) {
    LOADSYMA;
    NEWLINKED(local, new, syma.sym);

    ADVANCE;
}

BEGINOP(copy) {
    LOADSYMAB;
    NTLOAD(local, val, syma.sym);
    STORE(local, val, symb.sym);

    ADVANCE;
}

BEGINOP(load) {
    LOADSYMABC;

    LOADREF(local, sec, syma.sym);
    NTLOAD(sec.ref, val, symb.sym);
    STORE(local, val, symc.sym);

    ADVANCE;
}

BEGINOP(save) {
    LOADSYMABC;

    NTLOAD(local, val, syma.sym);
    LOADREF(local, sec, symc.sym);
    STORE(sec.ref, val, symb.sym);

    ADVANCE;
}

BEGINOP(drop) {
    LOADSYMAB;

    LOADREF(local, sec, syma.sym);
    DROP(sec.ref, symb.sym);

    ADVANCE;
}

BEGINOP(count) {
    LOADSYMAB;

    LOADREF(local, sec, syma.sym);
    int size = ndl_graph_size(graph, sec.ref);
    STORE(local, NDL_VALUE(EVAL_INT, num=size), symb.sym);

    ADVANCE;
}

BEGINOP(iload) {
    LOADSYMABC;

    LOADREF(local, sec, syma.sym);
    LOAD(local, i, symb.sym, EVAL_INT);
    ndl_sym key = ndl_graph_index(graph, sec.ref, i.num);
    STORE(local, NDL_VALUE(EVAL_REF, ref=key), symc.sym);

    ADVANCE;
}

#define ONEARGFPOP(name, expr)                                      \
    BEGINOP(name) {                                                 \
        LOADSYMAB;                                                  \
        LOAD(local, a, syma.sym, EVAL_FLOAT);                       \
        STORE(local, NDL_VALUE(EVAL_FLOAT, real=(expr)), symb.sym); \
        ADVANCE;                                                    \
    }

#define TWOARGFPOP(name, expr)                                      \
    BEGINOP(name) {                                                 \
        LOADSYMABC;                                                 \
        LOAD(local, a, syma.sym, EVAL_FLOAT);                       \
        LOAD(local, b, symb.sym, EVAL_FLOAT);                       \
        STORE(local, NDL_VALUE(EVAL_FLOAT, real=(expr)), symc.sym); \
        ADVANCE;                                                    \
    }

TWOARGFPOP(fadd, a.real + b.real)
TWOARGFPOP(fsub, a.real - b.real)
ONEARGFPOP(fneg, - a.real)
TWOARGFPOP(fmul, a.real * b.real)
TWOARGFPOP(fdiv, a.real / b.real)
TWOARGFPOP(fmod, fmod(a.real, b.real))
ONEARGFPOP(fsqrt, sqrt(a.real))

BEGINOP(ftoi) {
    LOADSYMAB;

    LOAD(local, a, syma.sym, EVAL_FLOAT);
    STORE(local, NDL_VALUE(EVAL_INT, num=(int)a.real), symb.sym);

    ADVANCE;
}

#define ONEARGINTOP(name, expr)                                  \
    BEGINOP(name) {                                              \
        LOADSYMAB;                                               \
        LOAD(local, a, syma.sym, EVAL_INT);                      \
        STORE(local, NDL_VALUE(EVAL_INT, num=(expr)), symb.sym); \
        ADVANCE;                                                 \
    }

#define TWOARGINTOP(name, expr)                                  \
    BEGINOP(name) {                                              \
        LOADSYMABC;                                              \
        LOAD(local, a, syma.sym, EVAL_INT);                      \
        LOAD(local, b, symb.sym, EVAL_INT);                      \
        STORE(local, NDL_VALUE(EVAL_INT, num=(expr)), symc.sym); \
        ADVANCE;                                                 \
    }

TWOARGINTOP(and, a.num & b.num)
TWOARGINTOP(or,  a.num | b.num)
TWOARGINTOP(xor, a.num ^ b.num)
ONEARGINTOP(not, ~a.num)
TWOARGINTOP(lshift, a.num << b.num)
TWOARGINTOP(rshift, a.num >> b.num)
TWOARGINTOP(ulshift, ((uint64_t) a.num) << b.num)
TWOARGINTOP(urshift, ((uint64_t) a.num) >> b.num)

TWOARGINTOP(add, a.num + b.num)
TWOARGINTOP(sub, a.num - b.num)
ONEARGINTOP(neg, - a.num)
TWOARGINTOP(mul, a.num * b.num)
TWOARGINTOP(div, a.num / b.num)
TWOARGINTOP(mod, a.num % b.num)

BEGINOP(itof) {
    LOADSYMAB;

    LOAD(local, a, syma.sym, EVAL_INT);
    STORE(local, NDL_VALUE(EVAL_FLOAT, real=(double)a.num), symb.sym);

    ADVANCE;
}

BEGINOP(itos) {
    LOADSYMAB;

    LOAD(local, a, syma.sym, EVAL_INT);
    STORE(local, NDL_VALUE(EVAL_SYM, sym=(ndl_sym) a.num), symb.sym);

    ADVANCE;
}

BEGINOP(stoi) {
    LOADSYMAB;

    LOAD(local, a, syma.sym, EVAL_SYM);
    STORE(local, NDL_VALUE(EVAL_INT, num=(int64_t) a.sym), symb.sym);

    ADVANCE;
}

BEGINOP(branch) {
    LOADSYMAB;

    NTLOAD(local, a, syma.sym);
    LOAD(local, b, symb.sym, a.type);

    int cmp;

    switch (a.type) {
    case EVAL_INT:
    case EVAL_SYM:
    case EVAL_REF:
        if (a.num < b.num) cmp = -1;
        else if (a.num == b.num) cmp = 0;
        else cmp = 1;
        break;
    case EVAL_FLOAT:
        if (a.real < b.real) cmp = -1;
        else if (a.real == b.real) cmp = 0;
        else cmp = 1;
        break;
    case EVAL_NONE:
        cmp = 0;
        break;
    default:
        return NDL_NULL_REF;
    }

    ndl_sym branch;

    if (cmp == -1) branch = DS("lt      ");
    if (cmp ==  0) branch = DS("eq      ");
    if (cmp ==  1) branch = DS("gt      ");

    LOADREF(pc, next, branch);
    STORE(local, next, DS("instrpntr"));

    return local;
}

BEGINOP(push) {
    LOADSYMA;

    LOADREF(pc, next, DS("next    "));
    STORE(local, next, DS("instrpntr"));

    LOADREF(local, invoke, syma.sym);

    return invoke.ref;
}

#include <stdio.h>

/* Temporary. */
BEGINOP(print) {
    LOADSYMA;

    NTLOAD(local, val, syma.sym);

    char buff[16];
    buff[15] = '\0';

    ndl_value_to_string(val, 15, buff);

    printf("[%03d@%03d]: %s.\n", pc, local, buff);

    ADVANCE;
}
