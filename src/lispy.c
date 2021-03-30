#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "compat.h"
#include "parser-util.h"

int main(int argc, char** argv) {
    lenv* e = lenv_new();
    lenv_add_builtins(e);

    /* load standard library */
    lval* std_lib_val = lval_add(lval_sexpr(), lval_str("lib-std.lispy"));
    builtin_load(e, std_lib_val);

    /* repl */
    if (argc == 1) {        
        puts("Lispy Version 1.0.0");
        puts("Press Ctrl+c to Exit\n");

        while (1) {
            char* input = readline("lispy> ");
            add_history(input);

            /* parse input */
            int pos = 0;
            lval* expr = lval_read_expr(input, &pos, '\0');

            /* eval and print */
            lval* x = lval_eval(e, expr);
            lval_println(x);
            lval_del(x);

            // todo?
            free(input);
        }
    }

    /* file list args */
    if (argc >= 2) {
        /* for each file name */
        for (int i = 1; i < argc; i++) {
            /* filename */
            lval* args = lval_add(lval_sexpr(), lval_str(argv[i]));

            lval* x = builtin_load(e, args);

            if(x->type == LVAL_ERR) { lval_println(x); }
            lval_del(x);
        }        
    }

    lenv_del(e);

    return 0;
}