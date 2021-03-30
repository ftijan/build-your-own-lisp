#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>

#include "parser-util.h"

/* number type lval */
lval* lval_num(long x) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}

/* error type lval */
lval* lval_err(char* fmt, ...) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_ERR;
    
    /* crate and init a list */
    va_list va;
    va_start(va, fmt);

    /* allocate 512 bytes of space */
    v->err = malloc(512);

    /* printf max 511 chars of error string */
    vsnprintf(v->err, 511, fmt, va);

    /* reallocate to number of bytes used */
    v->err = realloc(v->err, strlen(v->err) + 1);

    /* list cleanup */
    va_end(va);
    
    return v;
}

/* symbol type lval */
lval* lval_sym(char* s) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->sym = malloc(strlen(s) + 1);
    strcpy(v->sym, s);
    return v;
}

/* s-expression type lval */
lval* lval_sexpr(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

/* q-expression type lval */
lval* lval_qexpr(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_QEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

void lval_del(lval* v) {
    switch (v->type)
    {
        case LVAL_NUM: break;
        case LVAL_ERR: free(v->err); break;
        case LVAL_SYM: free(v->sym); break;
        case LVAL_STR: free(v->str); break;
        case LVAL_FUN:
            if(!v->builtin) {
                lenv_del(v->env);
                lval_del(v->formals);
                lval_del(v->body);
            }
        break;
        case LVAL_QEXPR:
        case LVAL_SEXPR:
            for (int i = 0; i < v->count; i++) {
                lval_del(v->cell[i]);
            }
            free(v->cell);
        break;
    }
    free(v);
}

lval* lval_add(lval* v, lval* x) {
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    v->cell[v->count-1] = x;
    return v;
}

void lval_expr_print(lval* v, char open, char close) {
    putchar(open);    
    
    for (int i = 0; i < v->count; i++) {
        /* print value */
        lval_print(v->cell[i]);

        /* skip last element's trailing space */
        if (i != (v->count-1)) {
            putchar(' ');
        }
    }

    putchar(close);
}

/* Print an "lval" */
void lval_print(lval* v) {
    switch (v->type)
    {
        case LVAL_NUM: printf("%li", v->num); break;
        case LVAL_ERR: printf("Error: %s", v->err); break;
        case LVAL_SYM: printf("%s", v->sym); break;
        case LVAL_STR: lval_print_str(v); break;
        case LVAL_FUN:
            if(v->builtin) {
                printf("<builtin>"); 
            } else {
                printf("(\\ "); lval_print(v->formals);
                putchar(' '); lval_print(v->body); putchar(')');
            }        
        break;
        case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
        case LVAL_QEXPR: lval_expr_print(v, '{', '}'); break;
    }
}

/* Print an "lval" followed by a newline */
void lval_println(lval* v) { lval_print(v); putchar('\n'); }

lval* lval_eval_sexpr(lenv* e, lval* v) {
    /* evaluate children */
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(e, v->cell[i]);
    }
    /* error check */
    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
    }
    /* empty */
    if (v->count == 0) { return v; }
    /* single */
    if (v->count == 1) { return lval_take(v, 0); }
    /* ensure func as first element */
    lval* f = lval_pop(v, 0);
    if (f->type != LVAL_FUN) {
        lval* err = lval_err(
            "S-Expression starts with incorrect type. "
            "Got %s, Expected %s.",
            ltype_name(f->type), ltype_name(LVAL_FUN));
        lval_del(f); lval_del(v);
        return err;
    }
    
    lval* result = lval_call(e, f, v);
    lval_del(f);
    return result;
}

lval* lval_eval(lenv* e, lval* v) {
    if(v->type == LVAL_SYM) {
        lval* x = lenv_get(e, v);
        lval_del(v);
        return x;
    }    
    if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(e, v); }    
    return v;
}

lval* lval_pop(lval* v, int i) {
    /* find [i] */
    lval* x = v->cell[i];
    /* remove [i] from list */
    memmove(&v->cell[i], &v->cell[i+1], sizeof(lval*) * (v->count-i-1));    
    v->count--;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);

    return x;
}

lval* lval_take(lval* v, int i) {
    lval* x = lval_pop(v, i);
    lval_del(v);
    return x;
}

lval* builtin_op(lenv* e, lval* a, char* op) {
    /* ensure all args are numbers */
    for (int i = 0; i < a->count; i++) {
        LASSERT_TYPE(op, a, i, LVAL_NUM);
    }

    lval* x = lval_pop(a, 0);
    
    /* unary negation */
    if ((strcmp(op, "-") == 0) && a->count == 0) {
        x->num = -x->num;
    }

    while (a->count > 0) {
        lval* y = lval_pop(a, 0);
        
        if (strcmp(op, "+") == 0) { x->num += y->num; }
        if (strcmp(op, "-") == 0) { x->num -= y->num; }
        if (strcmp(op, "*") == 0) { x->num *= y->num; }
        if (strcmp(op, "/") == 0) {
           if (y->num == 0) {
              lval_del(x); lval_del(y);
              x= lval_err("Division by zero."); break;
           }
           x->num /= y->num;
        }

        lval_del(y);
    }
    
    lval_del(a);
    return x;
}

lval* builtin_head(lenv* e, lval* a) {
    LASSERT_NUM("head", a, 1);
    LASSERT_TYPE("head", a, 0, LVAL_QEXPR);
    LASSERT_NOT_EMPTY("head", a, 0);

    lval* v = lval_take(a, 0);

    while(v->count > 1) { lval_del(lval_pop(v, 1)); }
    return v;
}

lval* builtin_tail(lenv* e, lval* a) {
    LASSERT_NUM("tail", a, 1);
    LASSERT_TYPE("tail", a, 0, LVAL_QEXPR);
    LASSERT_NOT_EMPTY("tail", a, 0);

    lval* v = lval_take(a, 0);

    lval_del(lval_pop(v, 0));
    return v;
}

lval* builtin_list(lenv* e, lval* a) {
    a->type = LVAL_QEXPR;
    return a;
}

lval* builtin_eval(lenv* e, lval* a) {
    LASSERT_NUM("eval", a, 1);
    LASSERT_TYPE("eval", a, 0, LVAL_QEXPR);

    lval* x = lval_take(a, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(e, x);
}

lval* builtin_join(lenv* e, lval* a) {
    for (int i = 0; i < a->count; i++) {
        LASSERT_TYPE("join", a, i, LVAL_QEXPR);
    }

    lval* x = lval_pop(a, 0);

    while (a->count) {
        lval* y = lval_pop(a, 0);
        x = lval_join(x, y);
    }

    lval_del(a);   
    return x;
}

lval* lval_join(lval* x, lval* y) {
    while (y->count) {
        x = lval_add(x, lval_pop(y, 0));
    }
    
    lval_del(y);
    return x;
}

lval* lval_fun(lbuiltin func) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_FUN;
    v->builtin = func;
    return v;
}

lval* lval_copy(lval* v) {
    lval* x = malloc(sizeof(lval));
    x->type = v->type;

    switch (v->type) {
        /* copy func and num directly */
        case LVAL_FUN:
            if(v->builtin) {
                x->builtin = v->builtin;
            } else {
                x->builtin = NULL;
                x->env = lenv_copy(v->env);
                x->formals = lval_copy(v->formals);
                x->body = lval_copy(v->body);
            }         
        break;
        case LVAL_NUM: x->num = v->num; break;
        /* copy strings with malloc and strcpy  */
        case LVAL_ERR:
            x->err = malloc(strlen(v->err) + 1);
            strcpy(x->err, v->err); break;
        case LVAL_SYM:
            x->sym = malloc(strlen(v->sym) + 1);
            strcpy(x->sym, v->sym); break;
        case LVAL_STR:
            x->str = malloc(strlen(v->str) + 1);
            strcpy(x->str, v->str); break;
        /* copy lists iteratively and recursively */
        case LVAL_SEXPR:
        case LVAL_QEXPR:
            x->count = v->count;
            x->cell = malloc(sizeof(lval*) * x->count);
            for (int i = 0; i < x->count; i++) {
                x->cell[i] = lval_copy(v->cell[i]);
            }
        break;            
    }

    return x;
}

void lenv_del(lenv* e) {
    for (int i = 0; i < e->count; i++) {
        free(e->syms[i]);
        lval_del(e->vals[i]);
    }
    free(e->syms);
    free(e->vals);
    free(e);
}

lval* lenv_get(lenv* e, lval* k) {
    /* iterate through all environment items */
    for (int i = 0; i < e->count; i++) {
        /* check by sym string, return if match */
        if(strcmp(e->syms[i], k->sym) == 0) {
            return lval_copy(e->vals[i]);
        }
    }
    /* if no match in parent return error */
    if (e->par) {
        return lenv_get(e->par, k);
    } else {
        return lval_err("Unbound symbol '%s'", k->sym);
    }    
}

void lenv_put(lenv* e, lval* k, lval* v) {
    /* iterate through all environment items */
    for (int i = 0; i < e->count; i++) {
        /* if found, replace */
        if(strcmp(e->syms[i], k->sym) == 0) {
            lval_del(e->vals[i]);
            e->vals[i] = lval_copy(v);
            return;
        }
    }

    /* new entry - allocate space */
    e->count++;
    e->vals = realloc(e->vals, sizeof(lval*) * e->count);
    e->syms = realloc(e->syms, sizeof(char*) * e->count);
    
    /* set value */
    e->vals[e->count-1] = lval_copy(v);
    e->syms[e->count-1] = malloc(strlen(k->sym)+1);
    strcpy(e->syms[e->count-1], k->sym);
}

lval* builtin_add(lenv* e, lval* a) {
    return builtin_op(e, a, "+");
}
lval* builtin_sub(lenv* e, lval* a) {
    return builtin_op(e, a, "-");
}
lval* builtin_mul(lenv* e, lval* a) {
    return builtin_op(e, a, "*");
}
lval* builtin_div(lenv* e, lval* a) {
    return builtin_op(e, a, "/");
}

void lenv_add_builtin(lenv* e, char* name, lbuiltin func) {
    lval* k = lval_sym(name);
    lval* v = lval_fun(func);
    lenv_put(e, k, v);
    lval_del(k); lval_del(v);
}

void lenv_add_builtins(lenv* e) {
    /* Var funcs */
    lenv_add_builtin(e, "\\", builtin_lambda);
    lenv_add_builtin(e, "def", builtin_def);
    lenv_add_builtin(e, "=", builtin_put);    

    /* List funcs */    
    lenv_add_builtin(e, "list", builtin_list);
    lenv_add_builtin(e, "head", builtin_head);
    lenv_add_builtin(e, "tail", builtin_tail);
    lenv_add_builtin(e, "eval", builtin_eval);
    lenv_add_builtin(e, "join", builtin_join);

    /* Math funcs */
    lenv_add_builtin(e, "+", builtin_add);
    lenv_add_builtin(e, "-", builtin_sub);
    lenv_add_builtin(e, "*", builtin_mul);
    lenv_add_builtin(e, "/", builtin_div);

    /* Compare funcs */
    lenv_add_builtin(e, "if", builtin_if);
    lenv_add_builtin(e, "==", builtin_eq);
    lenv_add_builtin(e, "!=", builtin_ne);
    lenv_add_builtin(e, ">", builtin_gt);
    lenv_add_builtin(e, "<", builtin_lt);
    lenv_add_builtin(e, ">=", builtin_ge);
    lenv_add_builtin(e, "<=", builtin_le);

    /* String functions */
    lenv_add_builtin(e, "load", builtin_load);
    lenv_add_builtin(e, "error", builtin_error);
    lenv_add_builtin(e, "print", builtin_print);
}

char* ltype_name(int t) {
    switch (t) {
        case LVAL_FUN: return "Function";
        case LVAL_NUM: return "Number";
        case LVAL_ERR: return "Error";
        case LVAL_SYM: return "Symbol";
        case LVAL_STR: return "String";
        case LVAL_SEXPR: return "S-Expression";
        case LVAL_QEXPR: return "Q-Expression";
        default: return "Unknown";
    }
}

lval* builtin_var(lenv* e, lval* a, char* func) {
    LASSERT_TYPE("def", a, 0, LVAL_QEXPR);
    
    /* first arg is a symbol list */
    lval* syms = a->cell[0];

    /* all first list items should be symbols */ 
    for (int i = 0; i < syms->count; i++) {
        LASSERT(a, (syms->cell[i]->type == LVAL_SYM),
        "Function '%s' cannot define non-symbol. "
        "Got %s, Expected %s.", func,
        ltype_name(syms->cell[i]->type), ltype_name(LVAL_SYM));
    }

    /* check correct number of symbols and values */
    LASSERT(a, (syms->count == a->count-1),
        "Function '%s' passed too many arguments for symbols. "
        "Got %i, Expected %i.", func,
        syms->count, a->count-1);

    /* assign copies of values to symbols */
    for (int i = 0; i < syms->count; i++) {
        /* if 'def' define globally; if 'put' define locally */
        if (strcmp(func, "def") == 0) {
            lenv_def(e, syms->cell[i], a->cell[i+1]);
        } 

        if (strcmp(func, "=") == 0){
            lenv_put(e, syms->cell[i], a->cell[i+1]);
        }        
    }
        
    lval_del(a);
    return lval_sexpr();
}

lval* lval_lambda(lval* formals, lval* body) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_FUN;

    /* no builtin for lambdas */
    v->builtin = NULL;

    /* build new env */
    v->env = lenv_new();

    /* set formals and body */
    v->formals = formals;
    v->body = body;
    return v;
}

lval* builtin_lambda(lenv* e, lval* a) {
    /* check q-expr args  */
    LASSERT_NUM("\\", a, 2);
    LASSERT_TYPE("\\", a, 0, LVAL_QEXPR);
    LASSERT_TYPE("\\", a, 1, LVAL_QEXPR);

    /* 1st q-expr only symbols */
    for (int i = 0; i < a->cell[0]->count; i++) {
        LASSERT(a, (a->cell[0]->cell[i]->type == LVAL_SYM),
            "Cannot define non-symbol. Got %s, Expected %s.",
            ltype_name(a->cell[0]->cell[i]->type), ltype_name(LVAL_SYM));
    }
    
    /* pop first 2 args and pass them to lval_lambda */
    lval* formals = lval_pop(a, 0);
    lval* body = lval_pop(a, 0);
    lval_del(a);

    return lval_lambda(formals, body);
}

lenv* lenv_new(void) {
    lenv* e = malloc(sizeof(lenv));
    e->par = NULL;
    e->count = 0;
    e->syms = NULL;
    e->vals = NULL;
    return e;
}

lenv* lenv_copy(lenv* e) {
    lenv* n = malloc(sizeof(lenv));
    n->par = e->par;
    n->count = e->count;
    n->syms = malloc(sizeof(char*) * n->count);
    n->vals = malloc(sizeof(lval*) * n->count);
    for (int i = 0; i < e->count; i++) {
        n->syms[i] = malloc(strlen(e->syms[i]) + 1);
        strcpy(n->syms[i], e->syms[i]);
        n->vals[i] = lval_copy(e->vals[i]);
    }
    return n;    
}

void lenv_def(lenv* e, lval* k, lval* v) {
    /* iterate until no parent  */
    while (e->par) { e = e->par; }
    /* put value in e */
    lenv_put(e, k, v);
}

lval* builtin_def(lenv* e, lval* a) {
    return builtin_var(e, a, "def");
}

lval* builtin_put(lenv* e, lval* a) {
    return builtin_var(e, a, "=");
}

lval* lval_call(lenv* e, lval* f, lval* a) {
    /* builtin: just return it */
    if (f->builtin) { return f->builtin(e, a); }

    /* record arg counts */
    int given = a->count;
    int total = f->formals->count;

    /* while args to process remain */
    while (a->count) {
        /* if no more formals to bind */
        if (f->formals->count == 0) {
            lval_del(a); return lval_err(
                "Function passed too many arguments. "
                "Got %i, Expected %i.", given, total);
        }

        /* get the symbol and arg value */
        lval* sym = lval_pop(f->formals, 0);

        /* deal with '&' */
        if (strcmp(sym->sym, "&") == 0) {
            /* '&' shouldn't hang */
            if (f->formals->count != 1) {
                lval_del(a);
                return lval_err("Function format invalid. "
                "Symbol '&' not followed by single symbol.");
            }

            /* next formal should be bound to remaining args */
            lval* nsym = lval_pop(f->formals, 0);
            lenv_put(f->env, nsym, builtin_list(e, a));
            lval_del(sym); lval_del(nsym);
            break;
        }

        lval* val = lval_pop(a, 0);
        /* bind into func env */
        lenv_put(f->env, sym, val);
        /* cleanup */
        lval_del(sym); lval_del(val);
    }

    /* arg list cleanup */
    lval_del(a);

    /* if '&' reamins in formal list bind to empty list */
    if (f->formals->count > 0 && strcmp(f->formals->cell[0]->sym, "&") == 0) {
        /* check validity */
        if (f->formals->count != 2) {
            return lval_err("Function format invalid. "
                "Symbol '&' not followed by single symbol.");
        }

        /* remove '&' */
        lval_del(lval_pop(f->formals, 0));

        /* bind next sym, create empty list */
        lval* sym = lval_pop(f->formals, 0);
        lval* val = lval_qexpr();

        /* bind to env and delete */
        lenv_put(f->env, sym, val);
        lval_del(sym); lval_del(val);
    }

    /* if all formals bound */
    if (f->formals->count == 0) {
        /* set env parent to eval env */
        f->env->par = e;

        /* eval and return */
        return builtin_eval(f->env, lval_add(lval_sexpr(), lval_copy(f->body)));
    } else {
        /* return partially evaluated func */
        return lval_copy(f);
    }    
}

lval* builtin_gt(lenv* e, lval* a) {
    return builtin_ord(e, a, ">");
}

lval* builtin_lt(lenv* e, lval* a) {
    return builtin_ord(e, a, "<");
}

lval* builtin_ge(lenv* e, lval* a) {
    return builtin_ord(e, a, ">=");
}

lval* builtin_le(lenv* e, lval* a) {
    return builtin_ord(e, a, "<=");
}

lval* builtin_ord(lenv* e, lval* a, char* op) {
    LASSERT_NUM(op, a, 2);
    LASSERT_TYPE(op, a, 0, LVAL_NUM);
    LASSERT_TYPE(op, a, 1, LVAL_NUM);

    int r;
    if (strcmp(op, ">") == 0) { 
        r = (a->cell[0]->num > a->cell[1]->num); 
    }
    if (strcmp(op, "<") == 0) { 
        r = (a->cell[0]->num < a->cell[1]->num); 
    }
    if (strcmp(op, ">=") == 0) { 
        r = (a->cell[0]->num >= a->cell[1]->num); 
    }
    if (strcmp(op, "<=") == 0) { 
        r = (a->cell[0]->num <= a->cell[1]->num); 
    }
    
    lval_del(a);
    return lval_num(r);
}

int lval_eq(lval* x, lval* y) {
    /* types */
    if(x->type != y->type) { return 0; }

    switch (x->type)
    {
        /* number */
        case LVAL_NUM: return (x->num == y->num);

        /* strings */
        case LVAL_ERR: return (strcmp(x->err, y->err) == 0);
        case LVAL_SYM: return (strcmp(x->sym, y->sym) == 0);
        case LVAL_STR: return (strcmp(x->str, y->str) == 0);

        /* funcs */
        case LVAL_FUN:
            if (x->builtin || y->builtin) {
                return x->builtin == y->builtin;
            } else {
                return lval_eq(x->formals, y->formals)
                && lval_eq(x->body, y->body);
            }

        /* lists - compare count and every individual element */
        case LVAL_QEXPR:
        case LVAL_SEXPR:
            if (x->count != y->count) { return 0; }
            for (int i = 0; i < x->count; i++) {
                if(!lval_eq(x->cell[i], y->cell[i])) { return 0; }
            }

            return 1;
        break;
            
    }

    return 0;
}

lval* builtin_cmp(lenv* e, lval* a, char* op) {
    LASSERT_NUM(op, a, 2);

    int r;
    if(strcmp(op, "==") == 0) {
        r = lval_eq(a->cell[0], a->cell[1]);
    }
    if(strcmp(op, "!=") == 0) {
        r = !lval_eq(a->cell[0], a->cell[1]);
    }
    lval_del(a);
    return lval_num(r); 
}

lval* builtin_eq(lenv* e, lval* a) {
    return builtin_cmp(e, a, "==");
}

lval* builtin_ne(lenv* e, lval* a) {
    return builtin_cmp(e, a, "!=");
}

lval* builtin_if(lenv* e, lval* a) {
    LASSERT_NUM("if", a, 3);
    LASSERT_TYPE("if", a, 0, LVAL_NUM);
    LASSERT_TYPE("if", a, 1, LVAL_QEXPR);
    LASSERT_TYPE("if", a, 2, LVAL_QEXPR);
    
    /* mark both exprs as evaluable */
    lval* x;
    a->cell[1]->type = LVAL_SEXPR;
    a->cell[2]->type = LVAL_SEXPR;

    if (a->cell[0]->num) {
        /* if condition is true evaluate first expr */
        x = lval_eval(e, lval_pop(a, 1));
    } else {
        x = lval_eval(e, lval_pop(a, 2));
    }

    /* cleanup args list */
    lval_del(a);
    return x;
}

lval* lval_str(char* s) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_STR;
    v->str = malloc(strlen(s) + 1);
    strcpy(v->str, s);
    return v;
}

lval* builtin_print(lenv* e, lval* a) {
    /* print args + space */
    for (int i = 0; i < a->count; i++) {
        lval_print(a->cell[i]); putchar(' ');
    }
    
    /* print \n and delete args */
    putchar('\n');
    lval_del(a);

    return lval_sexpr();
}

lval* builtin_error(lenv* e, lval* a) {
    LASSERT_NUM("error", a, 1);
    LASSERT_TYPE("error", a, 0, LVAL_STR);

    /* construct error from arg */
    lval* err = lval_err(a->cell[0]->str);

    /* delete args and return */
    lval_del(a);
    return err;
}

lval* lval_read_expr(char* s, int* i, char end) {
    /* create new seqxp or qexpr */
    lval* x = (end == '}') ? lval_qexpr() : lval_sexpr();

    /* while not end char keep reading */
    while (s[*i] != end) {
        lval* y = lval_read(s, i);
        /* handle error*/
        if (y->type == LVAL_ERR) {
            lval_del(x);
            return y;
        } else {
            lval_add(x, y);
        }
    }

    /* move past end char */
    (*i)++;

    return x;
}

lval* lval_read(char* s, int* i) {
    /* skip whitespace, comments, etc. */
    while (strchr(" \t\v\r\n;", s[*i]) && s[*i] != '\0') {
        if (s[*i] == ';') {
            while (s[*i] != '\n' && s[*i] != '\n') { (*i)++; }
        }
        (*i)++;
    }

    lval* x = NULL;

    /* if end of input, error */
    if (s[*i] == '\0') {
        return lval_err("Unexpected end of input");
    }

    /* if '(', read s-expr */
    else if (s[*i] == '(') {
        (*i)++;
        x = lval_read_expr(s, i, ')');
    }

    /* if '{', read s-expr */
    else if (s[*i] == '{') {
        (*i)++;
        x = lval_read_expr(s, i, '}');
    }

    /* symbol part -> read symbol */
    else if (strchr(
            "abcdefghijklmnopqrstuvwxyz"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "0123456789_+-*\\/=<>!&", s[*i])) {
        x = lval_read_sym(s, i);
    }

    /* '"' -> read string" */
    else if (strchr("\"", s[*i])) {
        x = lval_read_str(s, i);
    }

    /* if unexpected char*/
    else {
        x = lval_err("Unexpected character %c", s[*i]);
    }

    /* skip whitespace, comments, etc. */
    while (strchr(" \t\v\r\n;", s[*i]) && s[*i] != '\0') {
        if (s[*i] == ';') {
            while (s[*i] != '\n' && s[*i] != '\n') { (*i)++; }
        }
        (*i)++;
    }

    return x;
}

lval* lval_read_sym(char* s, int* i) {
    /* alloc empty string */
    char* part = calloc(1,1);

    /* valid chars only */
    while (strchr(
            "abcdefghijklmnopqrstuvwxyz"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "0123456789_+-*\\/=<>!&", s[*i]) && s[*i] != '\0') {
        
        /* append char to string end */
        part = realloc(part, strlen(part)+2);
        part[strlen(part)+1] = '\0';
        part[strlen(part)+0] = s[*i];
        (*i)++;
    }
        
    /* check if number */
    int is_num = strchr("-0123456789", part[0]) != NULL;
    for (int i = 0; i < strlen(part); i++) {
        if (strchr("0123456789", part[i]) == NULL) { is_num = 0; break; }
    }
        
    /* add symbol or number as lval */
    lval* x = NULL;
    if (is_num) {
        errno = 0;
        long v = strtol(part, NULL, 10);
        x = (errno != ERANGE) ? lval_num(v) : lval_err("Invalid Number %s", part);
    } else {
        x = lval_sym(part);
    }

    /* free temp string */
    free(part);

    /* return updated position in input */
    return x;
}

char lval_str_unescape(char x) {
    switch (x) {
        case 'a': return '\a';
        case 'b': return '\b';
        case 'f': return '\f';
        case 'n': return '\n';
        case 'r': return '\r';
        case 't': return '\t';
        case 'v': return '\v';
        case '\\': return '\\';
        case '\'': return '\'';
        case '\"': return '\"';
    }
    return '\0';
}

char* lval_str_unescapable = "abfnrtv\\\'\"";
char * lval_str_escapable = "\a\b\f\n\r\t\v\\\'\"";

char* lval_str_escape(char x) {
    switch (x) {
        case '\a': return "\\a";
        case '\b': return "\\b";
        case '\f': return "\\f";
        case '\n': return "\\n";
        case '\r': return "\\r";
        case '\t': return "\\t";
        case '\v': return "\\v";
        case '\\': return "\\\\";
        case '\'': return "\\\'";
        case '\"': return "\\\"";
    }
    return "";
}

lval* lval_read_str(char* s, int* i) {
    /* string alloc */
    char* part = calloc(1,1);

    /* skip initial '"' char */
    (*i)++;
    while (s[*i] != '"') {
        char c = s[*i];

        /* check for unterminated string literals */
        if (c == '\0') {
            free(part);
            return lval_err("Unexpected end of input");
        }

        /* unescape */
        if (c == '\\') {
            (*i)++;
            /* check next char */
            if (strchr(lval_str_unescapable, s[*i])) {
                c = lval_str_unescape(s[*i]);
            } else {
                free(part);
                return lval_err("Invalid escape sequence \\%c", s[*i]);
            }
        }

        /* append char to string */
        part = realloc(part, strlen(part)+2);
        part[strlen(part)+1] = '\0';
        part[strlen(part)+0] = c;
        (*i)++;
    }
    /* skip final '"' char */
    (*i)++;

    lval* x = lval_str(part);

    free(part);

    return x;    
}

void lval_print_str(lval* v) {
    putchar('"');
    /* loop over string chars */
    for (int i = 0; i < strlen(v->str); i++) {
        if (strchr(lval_str_escapable, v->str[i])) {
            /* escape escapable chars */
            printf("%s", lval_str_escape(v->str[i]));
        } else {
            /* otherwise print as is */
            putchar(v->str[i]);
        }
    }
    putchar('"');
}

lval* builtin_load(lenv* e, lval* a) {
    LASSERT_NUM("load", a, 1);
    LASSERT_TYPE("load", a, 0, LVAL_STR);

    /* open file and check it exists */
    FILE* f = fopen(a->cell[0]->str, "rb");
    if (f == NULL) {
        lval* err = lval_err("Could not load Library %s", a->cell[0]->str);
        lval_del(a);
        return err;
    }

    /* read file */
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* input = calloc(length+1, 1);
    fread(input, 1, length, f);
    fclose(f);

    /* read from input to create sexpr */
    int pos = 0;
    lval* expr = lval_read_expr(input, &pos, '\0');
    free(input);

    /* evaluate all expression contained in sexpr */
    if (expr->type != LVAL_ERR) {
        while (expr->count) {
            lval* x = lval_eval(e, lval_pop(expr, 0));
            if (x->type == LVAL_ERR) { lval_println(x); }
            lval_del(x);
        }
    } else {
        lval_println(expr);
    }

    lval_del(expr);
    lval_del(a);

    return lval_sexpr();
}