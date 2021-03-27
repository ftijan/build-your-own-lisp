#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "lib/mpc.h"
#include "compat.h"
#include "parser-util.h"

int main(int argc, char** argv) {
    /* Parsers */
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Symbol = mpc_new("symbol");
    mpc_parser_t* Sexpr = mpc_new("sexpr");
    mpc_parser_t* Qexpr = mpc_new("qexpr");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lispy = mpc_new("lispy");

    /* Grammar */
    mpca_lang(MPCA_LANG_DEFAULT,
        "\
        number  : /-?[0-9]+/ ; \
        symbol  : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ; \
        sexpr   : '(' <expr>* ')' ; \
        qexpr   : '{' <expr>* '}' ; \
        expr    : <number> | <symbol> | <sexpr> | <qexpr> ; \
        lispy   : /^/ <expr>* /$/ ; \
        ",
        Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

    /* Print Version and Exit Information */
    puts("Lispy Version 0.0.0.1");
    puts("Press Ctrl+c to Exit\n");

    while (1) {
        char* input = readline("lispy> ");
        add_history(input);

        /* Parse the input */
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Lispy, &r)) {
            /* Success: print and delete AST */
            lval* x = lval_eval(lval_read(r.output));
            lval_println(x);
            lval_del(x);
            
            mpc_ast_delete(r.output);
        } else {
            /* Fail: print and delete the error */
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }

    /* Parser cleanup */
    mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

    return 0;
}