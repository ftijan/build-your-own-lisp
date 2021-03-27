#include <stdlib.h>
#include "lib/mpc.h"

enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR };
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

typedef struct lval {
    int type;
    long num;
    /* string data in Error and Symbol */
    char* err;
    char* sym;
    /* count and pointer to a list of lval* */
    int count;
    struct lval** cell;
} lval;

lval* lval_num(long x);
lval* lval_err(char* m);
lval* lval_sym(char* s);
lval* lval_sexpr(void);
void lval_del(lval* v);
lval* lval_read_num(mpc_ast_t* t);
lval* lval_read(mpc_ast_t* t);
lval* lval_add(lval* v, lval* x);
void lval_expr_print(lval* v, char open, char close);
void lval_print(lval* v);
void lval_println(lval* v);