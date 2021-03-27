#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "lib/mpc.h"
#include "compat.h"
#include "parser-util.h"

mpc_parser_t* Number;
mpc_parser_t* Symbol;
mpc_parser_t* String;
mpc_parser_t* Comment;
mpc_parser_t* Sexpr;
mpc_parser_t* Qexpr;
mpc_parser_t* Expr;
mpc_parser_t* Lispy;

int main(int argc, char** argv) { 
    /* Parsers */
    Number = mpc_new("number");
    Symbol = mpc_new("symbol");
    String = mpc_new("string");
    Comment = mpc_new("comment");
    Sexpr = mpc_new("sexpr");
    Qexpr = mpc_new("qexpr");
    Expr = mpc_new("expr");
    Lispy = mpc_new("lispy");

    /* Grammar */
    mpca_lang(MPCA_LANG_DEFAULT,
        "\
        number  : /-?[0-9]+/ ; \
        symbol  : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ; \
        string  : /\"(\\\\.|[^\"])*\"/ ; \
        comment : /;[^\\r\\n]*/ ; \
        sexpr   : '(' <expr>* ')' ; \
        qexpr   : '{' <expr>* '}' ; \
        expr    : <number> | <symbol> | <string> \
                | <comment> | <sexpr> | <qexpr>; \
        lispy   : /^/ <expr>* /$/ ; \
        ",
        Number, Symbol, String, Comment, Sexpr, Qexpr, Expr, Lispy);

    /* Print Version and Exit Information */
    puts("Lispy Version 0.0.0.1");
    puts("Press Ctrl+c to Exit\n");

    lenv* e = lenv_new();
    lenv_add_builtins(e);

    while (1) {
        char* input = readline("lispy> ");
        add_history(input);

        /* Parse the input */
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Lispy, &r)) {
            /* Success: print and delete AST */
            lval* x = lval_eval(e, lval_read(r.output));
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

    lenv_del(e);

    /* Parser cleanup */
    mpc_cleanup(8, Number, Symbol, String, Comment, Sexpr, Qexpr, Expr, Lispy);

    return 0;
}

/* hack */
lval* builtin_load(lenv* e, lval* a) {
    LASSERT_NUM("load", a, 1);
    LASSERT_TYPE("load", a, 0, LVAL_STR);

    /* parse File given by string name */
    mpc_result_t r;

    if (mpc_parse_contents(a->cell[0]->str, Lispy, &r)) {
        /* read */
        lval* expr = lval_read(r.output);
        mpc_ast_delete(r.output);

        /* eval */
        while (expr->count) {
            lval* x = lval_eval(e, lval_pop(expr, 0));
            /* if error, print */
            if(x->type == LVAL_ERR) { lval_println(x); }
            lval_del(x);
        }

        /* delete expressions and arguments */
        lval_del(expr);
        lval_del(a);

        /* return empty list */
        return lval_sexpr();
    } else {
        /* get error text */
        char* err_msg = mpc_err_string(r.error);
        mpc_err_delete(r.error);

        /* create new message */
        lval* err = lval_err("Could not load library %s", err_msg);
        free(err_msg);
        lval_del(a);

        /* cleanup */
        return err;
    }
}