/** nodel/src/test.c: Test implemented functionality. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "node.h"
#include "graph.h"
#include "runtime.h"

int testprettyprint(void) {

    printf("Testing value pretty printing.\n");

    ndl_value values[] = {
        NDL_VALUE(EVAL_NONE, ref=27),
        NDL_VALUE(EVAL_REF, ref=NDL_NULL_REF),
        NDL_VALUE(EVAL_REF, ref=0x0001),
        NDL_VALUE(EVAL_REF, ref=0xABC4),
        NDL_VALUE(EVAL_REF, ref=0xABCDEF),
        NDL_VALUE(EVAL_SYM, sym=NDL_SYM("hello   ")),
        NDL_VALUE(EVAL_SYM, sym=NDL_SYM("\0hello ")),
        NDL_VALUE(EVAL_SYM, sym=NDL_SYM("\0\0\0\0\0\0\0\0")),
        NDL_VALUE(EVAL_SYM, sym=NDL_SYM("        ")),
        NDL_VALUE(EVAL_SYM, sym=NDL_SYM("next    ")),
        NDL_VALUE(EVAL_SYM, sym=NDL_SYM("    last")),
        NDL_VALUE(EVAL_INT, num=0),
        NDL_VALUE(EVAL_INT, num=-1),
        NDL_VALUE(EVAL_INT, num=1),
        NDL_VALUE(EVAL_INT, num=10000),
        NDL_VALUE(EVAL_INT, num=10000009),
        NDL_VALUE(EVAL_INT, num=-10000000009),
        NDL_VALUE(EVAL_FLOAT, real=-100000000.0),
        NDL_VALUE(EVAL_FLOAT, real=100000000.0),
        NDL_VALUE(EVAL_FLOAT, real=1000.03),
        NDL_VALUE(EVAL_FLOAT, real=0.00000000004),
        NDL_VALUE(EVAL_FLOAT, real=-0.00000000004),
        NDL_VALUE(EVAL_FLOAT, real=-0.00434),
        NDL_VALUE(EVAL_FLOAT, real=1.3287),
        NDL_VALUE(EVAL_NONE, ref=NDL_NULL_REF)
    };

    char buff[16];
    buff[15] = '\0';

    int i;
    for (i = 0; values[i].type != EVAL_NONE || values[i].ref != NDL_NULL_REF; i++) {
        ndl_value_to_string(values[i], 15, buff);
        printf("%2ith value: %s.\n", i, buff);
    }

    return 0;
}

void testgraphprintnode(ndl_graph *graph, ndl_ref node) {

    int size = ndl_graph_size(graph, node);

    printf("Pairs: %d.\n", size);

    char symbuff[16];
    symbuff[15] = '\0';

    char valbuff[16];
    valbuff[15] = '\0';

    int i;
    for (i = 0; i < size; i++) {
        ndl_sym key = ndl_graph_index(graph, node, i);
        ndl_value val = ndl_graph_get(graph, node, key);

        ndl_value_to_string(NDL_VALUE(EVAL_SYM, sym=key), 15, symbuff);
        ndl_value_to_string(val, 15, valbuff);

        printf("(%s:%s)\n", symbuff, valbuff);
    }

    int count = ndl_graph_backref_size(graph, node);
    printf("Backrefs: %d\n", count);

    for (i = 0; i < count; i++) {
        ndl_ref back = ndl_graph_backref_index(graph, node, i);
        ndl_value_to_string(NDL_VALUE(EVAL_REF, ref=back), 15, valbuff);
        printf("'%s'.\n", valbuff);
    }
}

ndl_ref testgraphalloc(ndl_graph *graph) {

    ndl_ref ret = ndl_graph_alloc(graph);

    if (ret == NDL_NULL_REF) {
        fprintf(stderr, "Failed to allocate graph node.\n");
        exit(EXIT_FAILURE);
    }

    return ret;
}

void testgraphaddedge(ndl_graph *graph, ndl_ref a, ndl_ref b, const char *name) {

    int err = ndl_graph_set(graph, a, NDL_SYM(name), NDL_VALUE(EVAL_REF, ref=b));

    if (err != 0) {
        fprintf(stderr, "Failed to add edge\n");
        exit(EXIT_FAILURE);
    }
}

void testgraphdeledge(ndl_graph *graph, ndl_ref a, const char *name) {

    int err = ndl_graph_del(graph, a, NDL_SYM(name));

    if (err != 0) {
        fprintf(stderr, "Failed to delete edge\n");
        exit(EXIT_FAILURE);
    }
}

int testgraph(void) {

    printf("Testing graph.\n");

    ndl_graph *graph = ndl_graph_init();

    if (graph == NULL) {
        fprintf(stderr, "Failed to allocate graph.\n");
        return -1;
    }

    printf("Allocating nodes A, B, and C.\n");
    ndl_ref a = testgraphalloc(graph);
    ndl_ref b = testgraphalloc(graph);
    ndl_ref c = testgraphalloc(graph);

    ndl_graph_print(graph);

    printf("Adding edges a.b = b, b.c = c, c.a = a.\n");
    testgraphaddedge(graph, a, b, "b       ");
    testgraphaddedge(graph, b, c, "c       ");
    testgraphaddedge(graph, c, a, "a       ");

    ndl_graph_print(graph);

    printf("Adding edges b.last = c, b.first = c.\n");
    testgraphaddedge(graph, b, c, "last    ");
    testgraphaddedge(graph, b, c, "first   ");

    ndl_graph_print(graph);

    printf("Removing edges b.* = c.\n");
    testgraphdeledge(graph, b, "c       ");
    testgraphdeledge(graph, b, "last    ");
    testgraphdeledge(graph, b, "first   ");
    ndl_graph_print(graph);

    ndl_graph_unmark(graph, a);
    ndl_graph_salloc(graph, b, NDL_SYM("back    "));

    ndl_graph_print(graph);

    ndl_graph_kill(graph);

    graph = ndl_graph_init();

    if (graph == NULL) {
        fprintf(stderr, "Failed to allocate graph.\n");
        return -1;
    }

    a = testgraphalloc(graph);
    b = testgraphalloc(graph);
    c = testgraphalloc(graph);
    testgraphaddedge(graph, a, b, "next    ");

    ndl_ref d = ndl_graph_salloc(graph, b, NDL_SYM("next    "));
    ndl_ref e = ndl_graph_salloc(graph, d, NDL_SYM("next    "));

    ndl_ref f = ndl_graph_salloc(graph, e, NDL_SYM("next    "));
    ndl_ref g = ndl_graph_salloc(graph, f, NDL_SYM("next    "));
    ndl_ref h = ndl_graph_salloc(graph, g, NDL_SYM("next    "));
    testgraphaddedge(graph, h, f, "next    ");
    testgraphaddedge(graph, h, a, "root    ");

    printf("Testing GC.\n");
    ndl_graph_print(graph);

    ndl_graph_clean(graph);

    printf("Post GC.\n");
    ndl_graph_print(graph);

    testgraphdeledge(graph, e, "next    ");

    printf("Pre GC.\n");
    ndl_graph_print(graph);

    ndl_graph_clean(graph);

    printf("Post GC.\n");
    ndl_graph_print(graph);

    ndl_graph_unmark(graph, a);
    ndl_graph_clean(graph);
    printf("Unmarking a as root node.\n");
    ndl_graph_print(graph);

    ndl_graph_unmark(graph, b);
    ndl_graph_mark(graph, e);
    ndl_graph_clean(graph);
    printf("Unmarking b as root node and marking e.\n");
    ndl_graph_print(graph);

    ndl_graph_kill(graph);

    return 0;
}

#define SET(node, key, type, val) \
    ndl_graph_set(graph, node, NDL_SYM(key), NDL_VALUE(type, val))

int testruntimeadd(void) {

    printf("Beginning runtime addition tests.\n");

    ndl_runtime *runtime = ndl_runtime_init();

    if (runtime == NULL) {
        fprintf(stderr, "Failed to allocate runtime.\n");
        exit(EXIT_FAILURE);
    }

    ndl_graph *graph = ndl_runtime_graph(runtime);

    ndl_ref i0 = testgraphalloc(graph);
    ndl_ref i1 = ndl_graph_salloc(graph, i0, NDL_SYM("next    "));
    ndl_ref i2 = ndl_graph_salloc(graph, i1, NDL_SYM("next    "));
    ndl_ref i3 = ndl_graph_salloc(graph, i2, NDL_SYM("next    "));
    ndl_ref i4 = ndl_graph_salloc(graph, i3, NDL_SYM("next    "));
    ndl_ref i5 = ndl_graph_salloc(graph, i4, NDL_SYM("next    "));
    ndl_ref i6 = ndl_graph_salloc(graph, i5, NDL_SYM("next    "));
    ndl_graph_salloc(graph, i6, NDL_SYM("next    "));
    SET(i0, "opcode  ", EVAL_SYM, sym=NDL_SYM("load    "));
    SET(i0, "syma    ", EVAL_SYM, sym=NDL_SYM("instpntr"));
    SET(i0, "symb    ", EVAL_SYM, sym=NDL_SYM("const   "));
    SET(i0, "symc    ", EVAL_SYM, sym=NDL_SYM("a       "));
    SET(i0, "const   ", EVAL_INT, num=2);

    SET(i1, "opcode  ", EVAL_SYM, sym=NDL_SYM("load    "));
    SET(i1, "syma    ", EVAL_SYM, sym=NDL_SYM("instpntr"));
    SET(i1, "symb    ", EVAL_SYM, sym=NDL_SYM("const   "));
    SET(i1, "symc    ", EVAL_SYM, sym=NDL_SYM("b       "));
    SET(i1, "const   ", EVAL_INT, num=2);

    SET(i2, "opcode  ", EVAL_SYM, sym=NDL_SYM("add     "));
    SET(i2, "syma    ", EVAL_SYM, sym=NDL_SYM("a       "));
    SET(i2, "symb    ", EVAL_SYM, sym=NDL_SYM("b       "));
    SET(i2, "symc    ", EVAL_SYM, sym=NDL_SYM("c       "));

    SET(i3, "opcode  ", EVAL_SYM, sym=NDL_SYM("print   "));
    SET(i3, "syma    ", EVAL_SYM, sym=NDL_SYM("c       "));

    ndl_ref local = testgraphalloc(graph);
    SET(local, "instpntr", EVAL_REF, ref=i0);

    int pid = ndl_runtime_proc_init(runtime, local);

    printf("[%3d] Process started. Instruction@frame: %3d@%03d.\n", pid, i0, local);

    ndl_runtime_print(runtime);

    ndl_runtime_step(runtime, 1); ndl_runtime_print(runtime);
    ndl_runtime_step(runtime, 1); ndl_runtime_print(runtime);
    ndl_runtime_step(runtime, 1); ndl_runtime_print(runtime);
    ndl_runtime_step(runtime, 1); ndl_runtime_print(runtime);
    ndl_runtime_step(runtime, 1); ndl_runtime_print(runtime);

    ndl_graph_print(graph);

    printf("Testing GC and infinite loop.\n");
    SET(i3, "next    ", EVAL_REF, ref=i0);
    SET(local, "instpntr", EVAL_REF, ref=NDL_NULL_REF);
    ndl_graph_clean(graph);
    ndl_ref local2 = ndl_graph_salloc(graph, local, NDL_SYM("local2  "));
    SET(local2, "instpntr", EVAL_REF, ref=i0);
    ndl_graph_print(graph);

    ndl_runtime_proc_init(runtime, local2);
    printf("[%3d] Process started. Instruction@frame: %3d@%03d.\n", pid, i0, local);

    ndl_runtime_step(runtime, 20);
    ndl_runtime_print(runtime);
    ndl_graph_print(graph);


    ndl_runtime_kill(runtime);

    return 0;
}

/* Run the canonical test fibonacci program.
 * Will be *much* more readable once constant symbols are set up for most operations.
 * Can't wait for that assembler. :P
 * count = count
 * zero = 0
 * dec = -1
 *
 * a = 0
 * b = 1
 * branch count zero -> exit exit printb
 *
 * printb:
 * print b
 * a = a + b
 * count = count - dec
 * branch count zero -> exit exit printa
 *
 * printa:
 * print a
 * b = a + b
 * count = count - dec
 * branch count zero -> exit exit printb
 *
 * exit:
 * exit
 */
#define OPCODE(node, opcode)                              \
    SET(node, "opcode  ", EVAL_SYM, sym=NDL_SYM(opcode))
    
#define AOPCODE(node, opcode, a)                          \
    OPCODE(node, opcode);                                 \
    SET(node, "syma    ", EVAL_SYM, sym=NDL_SYM(a))

#define ABOPCODE(node, opcode, a, b)                      \
    AOPCODE(node, opcode, a);                             \
    SET(node, "symb    ", EVAL_SYM, sym=NDL_SYM(b))

#define ABCOPCODE(node, opcode, a, b, c)                  \
    ABOPCODE(node, opcode, a, b);                         \
    SET(node, "symc    ", EVAL_SYM, sym=NDL_SYM(c))

int testruntimefibo(int steps) {

    printf("Beginning fibonacci runtime tests.\n");

    ndl_runtime *runtime = ndl_runtime_init();

    if (runtime == NULL) {
        fprintf(stderr, "Failed to allocate runtime.\n");
        exit(EXIT_FAILURE);
    }

    ndl_graph *graph = ndl_runtime_graph(runtime);

    int instid = 0;
    ndl_ref insts[64];

    insts[instid] = testgraphalloc(graph);
    ABCOPCODE(insts[instid], "load    ", "instpntr", "const   ", "count   ");
    SET(insts[instid], "const   ", EVAL_INT, num=steps);
    instid++;

    insts[instid] = ndl_graph_salloc(graph, insts[instid - 1], NDL_SYM("next    "));
    ABCOPCODE(insts[instid], "load    ", "instpntr", "const   ", "a       ");
    SET(insts[instid], "const   ", EVAL_INT, num=0);
    instid++;

    insts[instid] = ndl_graph_salloc(graph, insts[instid - 1], NDL_SYM("next    "));
    ABCOPCODE(insts[instid], "load    ", "instpntr", "const   ", "b       ");
    SET(insts[instid], "const   ", EVAL_INT, num=1);
    instid++;

    insts[instid] = ndl_graph_salloc(graph, insts[instid - 1], NDL_SYM("next    "));
    ABCOPCODE(insts[instid], "load    ", "instpntr", "const   ", "zero    ");
    SET(insts[instid], "const   ", EVAL_INT, num=0);
    instid++;

    insts[instid] = ndl_graph_salloc(graph, insts[instid - 1], NDL_SYM("next    "));
    ABCOPCODE(insts[instid], "load    ", "instpntr", "const   ", "dec     ");
    SET(insts[instid], "const   ", EVAL_INT, num=1);
    instid++;

    int brancha = instid;
    insts[instid] = ndl_graph_salloc(graph, insts[instid - 1], NDL_SYM("next    "));
    ABOPCODE(insts[instid], "branch  ", "count   ", "zero    ");
    instid++;

    int printb = instid;
    insts[instid] = ndl_graph_salloc(graph, insts[instid - 1], NDL_SYM("gt      "));
    AOPCODE(insts[instid], "print   ", "b       ");
    instid++;
    
    insts[instid] = ndl_graph_salloc(graph, insts[instid - 1], NDL_SYM("next    "));
    ABCOPCODE(insts[instid], "add     ", "a       ", "b       ", "a       ");
    instid++;
    
    insts[instid] = ndl_graph_salloc(graph, insts[instid - 1], NDL_SYM("next    "));
    ABCOPCODE(insts[instid], "sub     ", "count   ", "dec     ", "count   ");
    instid++;

    int branchb = instid;
    insts[instid] = ndl_graph_salloc(graph, insts[instid - 1], NDL_SYM("next    "));
    ABOPCODE(insts[instid], "branch  ", "count   ", "zero    ");
    instid++;

    insts[instid] = ndl_graph_salloc(graph, insts[instid - 1], NDL_SYM("gt      "));
    AOPCODE(insts[instid], "print   ", "a       ");
    instid++;
    
    insts[instid] = ndl_graph_salloc(graph, insts[instid - 1], NDL_SYM("next    "));
    ABCOPCODE(insts[instid], "add     ", "a       ", "b       ", "b       ");
    instid++;
    
    insts[instid] = ndl_graph_salloc(graph, insts[instid - 1], NDL_SYM("next    "));
    ABCOPCODE(insts[instid], "sub     ", "count   ", "dec     ", "count   ");
    instid++;

    int branchc = instid;
    insts[instid] = ndl_graph_salloc(graph, insts[instid - 1], NDL_SYM("next    "));
    ABOPCODE(insts[instid], "branch  ", "count   ", "zero    ");
    SET(insts[instid], "gt      ", EVAL_REF, ref=insts[printb]);
    instid++;

    int exit = instid;
    insts[instid] = ndl_graph_salloc(graph, insts[instid - 1], NDL_SYM("lt      "));
    OPCODE(insts[instid], "exit    ");

    SET(insts[brancha], "lt      ", EVAL_REF, ref=insts[exit]);
    SET(insts[brancha], "eq      ", EVAL_REF, ref=insts[exit]);

    SET(insts[branchb], "lt      ", EVAL_REF, ref=insts[exit]);
    SET(insts[branchb], "eq      ", EVAL_REF, ref=insts[exit]);

    SET(insts[branchc], "eq      ", EVAL_REF, ref=insts[exit]);

    ndl_ref local = testgraphalloc(graph);
    SET(local, "instpntr", EVAL_REF, ref=insts[0]);

    int pid = ndl_runtime_proc_init(runtime, local);

    printf("[%3d] Process started. Instruction@frame: %3d@%03d.\n", pid, insts[0], local);

    ndl_runtime_print(runtime);

    ndl_runtime_step(runtime, 100); ndl_runtime_print(runtime);

    ndl_graph_print(graph);

    ndl_runtime_kill(runtime);

    return 0;
}

int testruntimefork(int threads) {

    printf("Beginning fork tests.\n");

    ndl_runtime *runtime = ndl_runtime_init();

    if (runtime == NULL) {
        fprintf(stderr, "Failed to allocate runtime.\n");
        exit(EXIT_FAILURE);
    }

    ndl_graph *graph = ndl_runtime_graph(runtime);

    int instid = 0;
    ndl_ref insts[64];

    /* count = threads
     * one = 1
     * thrdfunc = REF(threadfunc)
     *
     * loop:
     * count --;
     * branch count one -> exit, next, next
     * invoke = new node()
     * invoke.instpntr = thrdfunc
     * invoke.id = count
     * fork invoke | next=REF(loop)
     *
     * threadfunc:
     * print count | next=REF(threadfunc)
     *
     * exit:
     * exit
     *
     */

    insts[instid] = testgraphalloc(graph);
    ABCOPCODE(insts[instid], "load    ", "instpntr", "const   ", "count   ");
    SET(insts[instid], "const   ", EVAL_INT, num=threads);
    instid++;

    insts[instid] = ndl_graph_salloc(graph, insts[instid - 1], NDL_SYM("next    "));
    ABCOPCODE(insts[instid], "load    ", "instpntr", "const   ", "one     ");
    SET(insts[instid], "const   ", EVAL_INT, num=1);
    instid++;

    int thrdload = instid;
    insts[instid] = ndl_graph_salloc(graph, insts[instid - 1], NDL_SYM("next    "));
    ABCOPCODE(insts[instid], "load    ", "instpntr", "const   ", "thrdfunc");
    /* SET(insts[instid], "const   ", EVAL_REF, ref=thrdfunc); */
    instid++;

    int base = instid;
    insts[instid] = ndl_graph_salloc(graph, insts[instid - 1], NDL_SYM("next    "));
    ABCOPCODE(insts[instid], "sub     ", "count   ", "one     ", "count   ");
    instid++;

    int branch = instid;
    insts[instid] = ndl_graph_salloc(graph, insts[instid - 1], NDL_SYM("next    "));
    ABOPCODE(insts[instid], "branch  ", "count   ", "one     ");
    instid++;

    insts[instid] = ndl_graph_salloc(graph, insts[instid - 1], NDL_SYM("eq      "));
    AOPCODE(insts[instid], "new     ", "invoke  ");
    SET(insts[instid - 1], "gt      ", EVAL_REF, ref=insts[instid]);
    instid++;

    insts[instid] = ndl_graph_salloc(graph, insts[instid - 1], NDL_SYM("next    "));
    ABCOPCODE(insts[instid], "save    ", "thrdfunc", "instpntr", "invoke  ");
    SET(insts[instid], "const   ", EVAL_INT, num=0);
    instid++;

    insts[instid] = ndl_graph_salloc(graph, insts[instid - 1], NDL_SYM("next    "));
    ABCOPCODE(insts[instid], "save    ", "count   ", "id      ", "invoke  ");
    instid++;

    insts[instid] = ndl_graph_salloc(graph, insts[instid - 1], NDL_SYM("next    "));
    AOPCODE(insts[instid], "fork    ", "invoke  ");
    SET(insts[instid], "next    ", EVAL_REF, ref=insts[base]);
    instid++;

    
    insts[instid] = ndl_graph_salloc(graph, insts[branch], NDL_SYM("lt      "));
    OPCODE(insts[instid], "exit    ");
    instid++;

    insts[instid] = ndl_graph_salloc(graph, insts[thrdload], NDL_SYM("const   "));
    AOPCODE(insts[instid], "print   ", "id      ");
    SET(insts[instid], "next    ", EVAL_REF, ref=insts[instid]);

    ndl_ref rootlocal = testgraphalloc(graph);
    SET(rootlocal, "instpntr", EVAL_REF, ref=insts[0]);

    int pid = ndl_runtime_proc_init(runtime, rootlocal);

    printf("[%3d] Process started. Instruction@frame: %3d@%03d.\n", pid, insts[0], rootlocal);

    ndl_runtime_print(runtime);

    ndl_runtime_step(runtime, 4 + 6*10);
    ndl_runtime_print(runtime);

    ndl_graph_print(graph);

    ndl_runtime_kill(runtime);

    return 0;
}

int main(int argc, const char *argv[]) {

    printf("Beginning tests.\n");

    int err;
    err  = testprettyprint();
    /* err |= testgraph(); */
    /* err |= testruntimeadd(); */
    /* err |= testruntimefibo(10); */
    /* err |= testruntimefork(10); */

    printf("Finished tests.\n");

    return err;
}
