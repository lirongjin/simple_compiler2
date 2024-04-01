#include "bison.h"
#include "grammar_symbol.h"
#include "lalr_parse_table.h"
#include "error_recover.h"
#include "lr_parse_algorithm.h"
#include "context_free_grammar.h"
#include "list.h"
#include "cpl_common.h"
#include "cpl_debug.h"
#include "cpl_errno.h"
#include "stdlib.h"
#include "string.h"
#include "lex.h"
#include "cpl_mm.h"
#include "quadruple.h"

#include "abstract_tree.h"
#include "environ.h"
#include "generator.h"

#define PrErr(...)      Pr(__FILE__, __LINE__, __FUNCTION__, "error", __VA_ARGS__)

#define BISON_DEBUG
#ifdef BISON_DEBUG
#define PrDbg(...)      Pr(__FILE__, __LINE__, __FUNCTION__, "debug", __VA_ARGS__)
#else
#define PrDbg(...)
#endif

/*
 * 功能：分配内存，如果失败则程序退出。
 * 返回值：
 **/
void *BisonMAlloc(size_t size)
{
    void *p;

    p = CplAlloc(size);
    if (!p) {
        PrErr("no memory\n");
        exit(-1);
    }
    return p;
}

/*
 * 功能：添加文法中用到的符号。
 * 返回值：
 **/
static int GrammarAddSymbol(ContextFreeGrammar *grammar)
{
    int error = 0;

    error = CfgAddSymbol(grammar, SID_STARTx, "S'", SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_EMPTY, "empty", SYMBOL_TYPE_EMPTY);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_END, "$",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;

    /*非终结符号*/
    error = CfgAddSymbol(grammar, SID_PROGRAM, "program",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_BLOCK, "block",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_DECL, "decl",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_DECLS, "decls",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_STMTS, "stmts",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TYPE, "type",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_BASE_TYPE, "base_type",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_ARR_DECL_DIM, "arr_decl_dim",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_STMT, "stmt",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_EXPR, "expr",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_COND, "cond",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_ASSIGN_EXPR, "assign_expr",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_LVALUE, "lvalue",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_LOR_EXPR, "lor_expr",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_LAND_EXPR, "land_expr",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_REL_L0_EXPR, "rel_l0_expr",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_ARITH_L0_EXPR, "arith_l0_expr",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_REL_L1_EXPR, "rel_l1_expr",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_ARITH_L1_EXPR, "arith_l1_expr",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_LEVEL2_EXPR, "level2_expr",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TYPE_CAST, "type_cast",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_REF_POINTER, "deref_pointer",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_LEVEL1_EXPR, "level1_expr",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_ACCESS_MEMBER, "access_member",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_ACCESS_ELM, "access_element",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_ARR_DEREF_DIM, "arr_deref_dim",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_ARR_DEREF_DIMS, "arr_deref_dims",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_FUN_CALL, "fun_call",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_FUN_PARAMETERS, "fun_parameters",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_FUN_PARAS, "fun_paras",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_ID_EXPR, "id_expr",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_FACTOR, "factor",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_ARR_IDX, "arr_idx",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_CASE_STMT, "case_stmt",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_CASE_STMTS, "case_stmts",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    //error = CfgAddSymbol(grammar, SID_FUN_DEFINES, "fun_defines",  SYMBOL_TYPE_NONTERMINAL);
    //if (error != -ENOERR)
    //    goto err;
    error = CfgAddSymbol(grammar, SID_FUN_DEFINE, "fun_define",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_FUN_RET_TYPE, "fun_ret_type",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_FUN_ARGS, "fun_args",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_FUN_ARG, "fun_arg",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_FUN_ARGUES, "fun_argues",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_FUN_BLOCK, "fun_block",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;

    error = CfgAddSymbol(grammar, SID_STRUCT_TYPE, "struct_type",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_STRUCT_DEFINE, "struct_define",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    //error = CfgAddSymbol(grammar, SID_STRUCT_DEFINES, "struct_defines",  SYMBOL_TYPE_NONTERMINAL);
    //if (error != -ENOERR)
    //    goto err;
    error = CfgAddSymbol(grammar, SID_STRUCT_FUN_DEFINE, "struct_fun_define",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_COMPONENT_DEFINES, "component_defines",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_STRUCT_DEFINE_HEAD, "struct_define_head",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;

    /*终结符号*/
    error = CfgAddSymbol(grammar, SID_TM_ID, "id",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_IDIGIT, "int_digit",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_FDIGIT, "float_digit",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;

    error = CfgAddSymbol(grammar, SID_TM_CONTINUE, "continue",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_BREAK, "break",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_ELSE, "else",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_WHILE, "while",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_DO, "do",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_IF, "if",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_INT, "int",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_CHAR, "char",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_SHORT, "short",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_BOOL, "bool",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_FLOAT, "float",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_TRUE, "true",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_FALSE, "false",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_FOR, "for",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_RETURN, "return",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_SWITCH, "switch",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_CASE, "case",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_DEFAULT, "default",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_VOID, "void",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_STRUCT, "struct",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;

    error = CfgAddSymbol(grammar, SID_TM_LBRACE, "{",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_RBRACE, "}",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_LBRACKET, "(",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_RBRACKET, ")",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_LSBRACKET, "[",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_RSBRACKET, "]",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;

    error = CfgAddSymbol(grammar, SID_TM_SEMI_COLON, ";",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_COLON, ":",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;

    error = CfgAddSymbol(grammar, SID_TM_LNOT, "!",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_MUL, "*",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_DIV, "/",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_MOD, "%",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_ADD, "+",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_SUB, "-",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_LT, "<",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_LE, "<=",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_GT, ">",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_GE, ">=",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_EQ, "==",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_NE, "!=",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_LAND, "&&",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_LOR, "||",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_ASSIGN, "=",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_COMMA, ",",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_SHARP, "#",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_DOT, ".",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_ARROW, "->",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_BNAD, "&",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_BOR, "|",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_BNOR, "^",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_SPACE, "space",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_TAB, "tab",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_CR, "cr",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_TM_LN, "ln",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;

    error = CfgAddSymbol(grammar, SID_M1, "M1",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_M2, "M2",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;

    error = CfgAddSymbol(grammar, SID_M3, "M3",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_M4, "M4",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;

    error = CfgAddSymbol(grammar, SID_M5, "M5",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_M6, "M6",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;

    error = CfgAddSymbol(grammar, SID_M7, "M7",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_M8, "M8",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;

    error = CfgAddSymbol(grammar, SID_M9, "M9",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SID_M10, "M10",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;

    return 0;
err:
    return error;
}

/*此处并没有过多在意归约函数命名，因为函数太多，且仅用于回调。通过注释可以一目了然的知道函数用途。*/

/*
 * 功能：产生式处理函数。
 *      start' -> program
 * headArg: 输入型参数。产生式头对应的参数。记录归约的最终结果。
 * prevArg: 产生式体前面的一个参数，往往用来获取产生式头的继承属性参数。
 * bodyArg: 产生式体参数数组。
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int BisonStartxPHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    (void)headArg, (void)prevArg, (void)bodyArg;
    return 0;
}

/*program -> fun_defines*/
static int BisonProgDHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    FunDefines *funDefines;

    (void)prevArg;
    funDefines = bodyArg[0];
    bison->program = ATreeNewProgram(funDefines);
    *headArg = bison->program;
    return 0;
}

/*struct_fun_defines -> struct_fun_defines struct_fun_define*/
static int BisonStFunDefinesSfDssfdHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    FunDefines *funDefines;
    FunDefines *prevFunDefines;
    FunDefine *funDefine;

    (void)prevArg;
    prevFunDefines = bodyArg[0];
    funDefine = bodyArg[1];
    funDefines = ATreeNewFunDefines(prevFunDefines, funDefine);
    *headArg = funDefines;
    return 0;
}

/*component_defines -> component_defines struct_define*/
static int BisonConponentdefCdsstdefHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    FunDefines *funDefines;
    FunDefines *prevFunDefines;

    (void)prevArg;
    prevFunDefines = bodyArg[0];
    funDefines = prevFunDefines;
    *headArg = funDefines;
    return 0;
}

/*component_defines -> component_defines fun_define*/
static int BisonConponentdefCdsfundefHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    FunDefines *funDefines;
    FunDefines *prevFunDefines;
    FunDefine *funDefine;

    (void)prevArg;
    prevFunDefines = bodyArg[0];
    funDefine = bodyArg[1];
    funDefines = ATreeNewFunDefines(prevFunDefines, funDefine);
    *headArg = funDefines;
    return 0;
}

/*component_defines -> empty*/
static int BisonConponentdefCdsEmptyHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    FunDefines *funDefines;

    (void)headArg, (void)prevArg, (void)bodyArg;
    funDefines = NULL;
    *headArg = funDefines;
    return 0;
}

/*struct_fun_define -> struct_define fun_define*/
static int BisonStfundefineStfunHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    FunDefine *funDefine;

    (void)prevArg;
    funDefine = bodyArg[1];
    *headArg = funDefine;
    return 0;
}

/*M1 -> empty*/
static int BisonM1eHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    (void)headArg, (void)prevArg, (void)bodyArg;
    bison->env->domain = EnvDomainPush(bison->env->domain, DT_BLK);
    return 0;
}

/*M2 -> empty*/
static int BisonM2eHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    (void)headArg, (void)prevArg, (void)bodyArg;
    bison->env->domain = EnvDomainPop(bison->env->domain);
    return 0;
}

/*block -> { M1 decls stmts M2 }*/
static int BisonBlockLdsrHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Stmt *blockStmt;
    Stmts *stmts;

    (void)prevArg;
    stmts = bodyArg[3];
    blockStmt = ATreeNewBlockStmt(bison->env->domain, stmts);
    *headArg = blockStmt;
    return 0;
}

/*decls -> decls decl*/
static int BisonDeclsDdHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    DomainEntry *domainEntry;

    (void)headArg, (void)prevArg;
    domainEntry = bodyArg[1];
    EnvDomainPutEntry(bison->env->domain, domainEntry);
    domainEntry->domain = bison->env->domain;
    return 0;
}

/*decls -> empty*/
static int BisonDeclsEHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    (void)headArg, (void)prevArg, (void)bodyArg;
    return 0;
}

/*decl -> type id*/
static int BisonDeclTiHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    DomainEntry *domainEntry;
    Type *type;
    const Token *id;
    Domain *domain = bison->env->domain;

    (void)prevArg;
    id = bodyArg[1];
    type = bodyArg[0];
    domain->offset_num = ATreeAddrAlignByType(domain->offset_num, type);
    domainEntry = EnvNewDomainEntry(id, type, domain->offset_num, NULL);
    domain->offset_num += ATreeTypeSize(type);
    *headArg = domainEntry;

    if (domain->type == DT_STRUCT) {
        AlignType newAlignType;

        if (type->type == TT_BASE
                || type->type == TT_ARRAY) {
            Type *bType;
            if (type->type == TT_ARRAY) {
                bType = ATreeArrayBaseType(&type->array);
            } else {
                bType = type;
            }
            switch (bType->baseType.type) {
            case BTT_BOOL:
            case BTT_CHAR:
                newAlignType = AT_1BYTE;
                break;
            case BTT_SHORT:
                newAlignType = AT_2BYTE;
                break;
            case BTT_INT:
            case BTT_FLOAT:
                newAlignType = AT_4BYTE;
                break;
            }
        } else if (type->type == TT_STRUCTURE) {
            newAlignType = ATreeTypeGetAlignType(type);
        } else if (type->type == TT_POINTER) {
            newAlignType = AT_4BYTE;
        } else {
            PrErr("type->type error: %d", type->type);
            exit(-1);
        }
        if (newAlignType > domain->alignType) {
            domain->alignType = newAlignType;
        }
    }

    return 0;
}

typedef struct _DeclDims {
    Type *inhType;      /*继承属性类型*/
    Type *synType;      /*综合属性类型*/
} DeclDims;

DeclDims *BisonNewDim(void)
{
    DeclDims *declDims;

    declDims = BisonMAlloc(sizeof (*declDims));
    return declDims;
}

/*M3 -> empty*/
static int BisonM3eHandle(void **headArg, void *parentPrevArg, void *parentBodyArg[])
{
    DeclDims *declDims;
    Type *baseType;

    (void)parentPrevArg;

    baseType = parentBodyArg[0];
    declDims = BisonNewDim();
    declDims->inhType = baseType;
    *headArg = declDims;
    return 0;
}

/*type -> base_type M3 decl_dims*/
static int BisonTypeBdHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Type *type;
    DeclDims *declDims;

    (void)prevArg;
    declDims = bodyArg[2];
    type = declDims->synType;
    *headArg = type;
    return 0;
}

/*type -> type **/
static int BisonTypeTsHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Type *type;
    Type *baseType;

    (void)prevArg;
    baseType = bodyArg[0];
    type = ATreeNewPointerType(baseType);
    *headArg = type;
    return 0;
}

/*type -> struct_type*/
static int BisonTypeStHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Type *type;
    Type *structType;

    (void)prevArg;
    structType = bodyArg[0];
    type = structType;
    *headArg = type;
    return 0;
}

/*struct_type -> struct id*/
static int BisonStructTypeSiHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Type *structType;
    const Token *id;
    DomainEntry *entry;

    (void)prevArg, (void)bodyArg;

    id = bodyArg[1];
    entry = EnvDomainGetEntry(bison->env->domain, id);
    if (!entry) {
        PrErr("entry error: %p", entry);
        exit(-1);
    }
    structType = entry->type;
    *headArg = structType;
    return 0;
}

/*base_type -> int*/
static int BisonBaseIntHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Type *baseType;

    (void)prevArg, (void)bodyArg;
    baseType = bison->env->intType;
    *headArg = baseType;
    return 0;
}

/*base_type -> char*/
static int BisonBaseCharHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Type *baseType;

    (void)prevArg, (void)bodyArg;
    baseType = bison->env->charType;
    *headArg = baseType;
    return 0;
}

/*base_type -> short*/
static int BisonBaseShortHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Type *baseType;

    (void)prevArg, (void)bodyArg;
    baseType = bison->env->shortType;
    *headArg = baseType;
    return 0;
}

/*base_type -> bool*/
static int BisonBaseBoolHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Type *baseType;

    (void)prevArg, (void)bodyArg;
    baseType = bison->env->boolType;
    *headArg = baseType;
    return 0;
}

/*base_type -> float*/
static int BisonBaseFloatHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Type *baseType;

    (void)prevArg, (void)bodyArg;
    baseType = bison->env->floatType;
    *headArg = baseType;
    return 0;
}

/*M4 -> empty*/
static int BisonM4eHandle(void **headArg, void *parentPrevArg, void *parentBodyArg[])
{
    DeclDims *declDims, *headDeclDims;

    (void)parentBodyArg;
    headDeclDims = parentPrevArg;
    declDims = BisonNewDim();
    declDims->inhType = headDeclDims->inhType;
    *headArg = declDims;
    return 0;
}

/*decl_dims -> [ d ] M4 decl_dims1*/
static int BisonDimLdrdHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    DeclDims *declDims;
    const Token *digitToken;
    DeclDims *declDims1;
    AlignType alignType;

    digitToken = bodyArg[1];
    declDims1 = bodyArg[4];
    declDims = prevArg;
    alignType = ATreeTypeGetAlignType(declDims1->synType);
    declDims->synType = ATreeNewArrayType(declDims1->synType, digitToken->digit, alignType);
    *headArg = declDims;
    return 0;
}

/*decl_dims -> empty*/
static int BisonDimEHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    DeclDims *declDims;

    (void)bodyArg;
    declDims = prevArg;
    declDims->synType = declDims->inhType;
    *headArg = declDims;
    return 0;
}

/*stmts -> stmts1 stmt*/
static int BisonStmtsSSHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Stmts *stmts;
    Stmts *stmts1;
    Stmt *stmt;

    (void)prevArg;
    stmts1 = bodyArg[0];
    stmt = bodyArg[1];
    stmts = ATreeNewStmts(stmts1, stmt);
    *headArg = stmts;
    return 0;
}

/*stmts -> empty*/
static int BisonStmtsEHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Stmts *stmts;

    (void)prevArg, (void)bodyArg;
    stmts = NULL;
    *headArg = stmts;
    return 0;
}

/*stmt -> expr ;*/
static int BisonStmtEsHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Stmt *stmt;
    Expr *expr;

    (void)prevArg;
    expr = bodyArg[0];
    stmt = ATreeNewExprStmt(bison->env->domain, expr);
    *headArg = stmt;
    return 0;
}

/*stmt -> if ( cond ) stmt1*/
static int BisonStmtIlcrsHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Stmt *stmt;
    Cond *cond;
    Stmt *stmt1;

    (void)prevArg;
    cond = bodyArg[2];
    stmt1 = bodyArg[4];
    stmt = ATreeNewIfStmt(bison->env->domain, cond, stmt1);
    *headArg = stmt;
    return 0;
}

/*stmt -> if ( cond ) stmt1 else stmt2*/
static int BisonStmtIlcrsesHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Stmt *stmt;
    Cond *cond;
    Stmt *stmt1, *stmt2;

    (void)prevArg;
    cond = bodyArg[2];
    stmt1 = bodyArg[4];
    stmt2 = bodyArg[6];
    stmt = ATreeNewElseStmt(bison->env->domain, cond, stmt1, stmt2);
    *headArg = stmt;
    return 0;
}

/*stmt -> while ( cond ) stmt1*/
static int BisonStmtWlcrsHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Stmt *stmt;
    Cond *cond;
    Stmt *stmt1;

    (void)prevArg;
    cond = bodyArg[2];
    stmt1 = bodyArg[4];
    stmt = ATreeNewWhileStmt(bison->env->domain, cond, stmt1);
    *headArg = stmt;
    return 0;
}

/*stmt -> do stmt1 while ( cond ) ;*/
static int BisonStmtDswlcrHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Stmt *stmt;
    Cond *cond;
    Stmt *stmt1;

    (void)prevArg;
    cond = bodyArg[4];
    stmt1 = bodyArg[1];
    stmt = ATreeNewDoStmt(bison->env->domain, cond, stmt1);
    *headArg = stmt;
    return 0;
}

/*case_stmt -> case expr: stmts*/
static int BisonCasestmtCecsHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    CaseStmt *caseStmt;
    Expr *expr;
    Stmts *stmts;

    (void)prevArg;
    expr = bodyArg[1];
    stmts = bodyArg[3];
    caseStmt = ATreeNewCaseExprStmt(expr, stmts);
    *headArg = caseStmt;
    return 0;
}

/*case_stmt -> default: stmts*/
static int BisonCasestmtDcsHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    CaseStmt *caseStmt;
    Stmts *stmts;

    (void)prevArg;
    stmts = bodyArg[2];
    caseStmt = ATreeNewCaseDefaultStmt(stmts);
    *headArg = caseStmt;
    return 0;
}

/*case_stmts -> case_stmts1 case_stmt*/
static int BisonCasestmtsCcHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    CaseStmts *caseStmts;
    CaseStmts *prev;
    CaseStmt *caseStmt;

    (void)prevArg;
    prev = bodyArg[0];
    caseStmt = bodyArg[1];
    caseStmts = ATreeNewCaseStmts(prev, caseStmt);
    *headArg = caseStmts;
    return 0;
}

/*case_stmts -> empty*/
static int BisonCasestmtsEHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    CaseStmts *caseStms;

    (void)prevArg, (void)bodyArg;
    caseStms = NULL;
    *headArg = caseStms;
    return 0;
}

/*stmt -> switch ( E ) { case_stmts }*/
static int BisonStmtSlerlcrHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Stmt *stmt;
    Expr *expr;
    CaseStmts *caseStmts;

    (void)prevArg;
    expr = bodyArg[2];
    caseStmts = bodyArg[5];
    stmt = ATreeNewSwitchStmt(expr, caseStmts);
    *headArg = stmt;
    return 0;
}

/*stmt -> block*/
static int BisonStmtBHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Stmt *stmt;
    Stmt *blockStmt;

    (void)prevArg;
    blockStmt = bodyArg[0];
    stmt = blockStmt;
    *headArg = stmt;
    return 0;
}

/*stmt -> break ;*/
static int BisonStmtBsHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Stmt *stmt;

    (void)prevArg, (void)bodyArg;
    stmt = ATreeNewBreakStmt(bison->env->domain, 0);
    *headArg = stmt;
    return 0;
}

/*stmt -> continue ;*/
static int BisonStmtCsHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Stmt *stmt;

    (void)prevArg, (void)bodyArg;
    stmt = ATreeNewContinueStmt(bison->env->domain, 0);
    *headArg = stmt;
    return 0;
}

/*cond -> lor_expr*/
static int BisonCondLorHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Cond *cond;
    Expr *lorExpr;

    (void)prevArg;
    lorExpr = bodyArg[0];
    cond = lorExpr;
    *headArg = cond;
    return 0;
}

/*fun_defines -> fun_defines fun_define*/
static int BisonFundefinesFfHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    FunDefines *funDefines;
    FunDefines *prev;
    FunDefine *funDefine;

    (void)prevArg;
    prev = bodyArg[0];
    funDefine = bodyArg[1];
    funDefines = ATreeNewFunDefines(prev, funDefine);
    *headArg = funDefines;
    return 0;
}

/*fun_defines -> empty*/
static int BisonFundefinesEmptyHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    FunDefines *funDefines;

    (void)prevArg, (void)bodyArg;
    funDefines = NULL;
    *headArg = funDefines;
    return 0;
}

/*M7 -> empty*/
static int BisonM7EmptyHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Domain *lastDomain;

    (void)prevArg, (void)bodyArg;
    lastDomain = bison->env->domain;
    bison->env->domain = EnvDomainPush(bison->env->domain, DT_FUN);
    *headArg = lastDomain;
    return 0;
}

/*M8 -> empty*/
static int BisonM8EmptyHandle(void **headArg, void *parentPrevArg, void *parentBodyArg[])
{
    //FunType *funType;
    FunArgs *funArgs;
    Domain *lastDomain;
    DomainEntry *domainEntry;
    const Token *id;
    FunRetType *funRetType;
    Type *type;

    (void)headArg, (void)parentPrevArg;
    id = parentBodyArg[2];
    funRetType = parentBodyArg[1];
    funArgs = parentBodyArg[4];
    lastDomain = parentBodyArg[0];
    type = ATreeNewFunctionType(funRetType, funArgs);
    domainEntry = EnvNewDomainEntry(id, type, 0, lastDomain);
    EnvDomainPutEntry(lastDomain, domainEntry);
    bison->env->domain->funType = &type->funType;
    *headArg = bison->env->domain->funType;
    return 0;
}

/*fun_define -> M7 fun_ret_type id ( fun_argues ) M8 fun_block */
static int BisonFundefineFilfrbHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    FunDefine *funDefine;
    FunType *funType;
    Stmt *blockStmt;
    const Token *id;

    (void)prevArg;
    bison->env->domain = EnvDomainPop(bison->env->domain);
    id = bodyArg[2];
    funType = bodyArg[6];
    blockStmt = bodyArg[7];
    funDefine = ATreeNewFunDefine(id, funType, blockStmt);
    *headArg = funDefine;
    if (strcmp(id->cplString->str, "main") == 0) {
        bison->mainToken = id;
        bison->mainFunType = funType;
    }
    return 0;
}

/*fun_ret_type -> void*/
static int BisonFunrettypeVHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    FunRetType *funRetType;

    (void)prevArg, (void)bodyArg;
    funRetType = ATreeNewFunRetTypeVoid();
    *headArg = funRetType;
    return 0;
}

/*fun_ret_type -> base_type*/
static int BisonFunrettypeBtHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    FunRetType *funRetType;
    Type *baseType;

    (void)prevArg;
    baseType = bodyArg[0];
    funRetType = ATreeNewFunRetType(baseType);
    *headArg = funRetType;
    return 0;
}

/*fun_ret_type -> struct_type*/
static int BisonFunrettypeSttHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    FunRetType *funRetType;
    Type *structType;

    (void)prevArg;
    structType = bodyArg[0];
    funRetType = ATreeNewFunRetType(structType);
    *headArg = funRetType;
    return 0;
}

/*fun_argues -> fun_args*/
static int BisonFunarguesFHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    FunArgs *funArgs;

    (void)prevArg;
    funArgs = bodyArg[0];
    *headArg = funArgs;
    return 0;
}

/*fun_argues -> void*/
static int BisonFunarguesVHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    FunArgs *funArgs;

    (void)prevArg, (void)bodyArg;
    funArgs = NULL;
    *headArg = funArgs;
    return 0;
}

/*fun_argues -> empty*/
static int BisonFunarguesEmptyHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    FunArgs *funArgs;

    (void)prevArg, (void)bodyArg;
    funArgs = NULL;
    *headArg = funArgs;
    return 0;
}

/*fun_arg -> type id*/
static int BisonFunargTiHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    FunArg *funArg;
    Type *type;
    const Token *id;
    DomainEntry *domainEntry;

    (void)prevArg;
    id = bodyArg[1];
    type = bodyArg[0];
    funArg = ATreeNewFunArg(id, type);
    *headArg = funArg;

    domainEntry = EnvNewDomainEntry(id, type, 0, bison->env->domain);
    domainEntry->funArgFlag = 1;
    EnvDomainPutEntry(bison->env->domain, domainEntry);
    return 0;
}

/*fun_args -> fun_arg , fun_args1 */
static int BisonFunargsFcfHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    FunArgs *funArgs;
    FunArg *funArg;
    FunArgs *next;

    (void)prevArg;
    funArg = bodyArg[0];
    next = bodyArg[2];
    funArgs = ATreeNewFunArgs(funArg, next);
    *headArg = funArgs;
    return 0;
}

/*fun_args -> fun_arg*/
static int BisonFunargsFHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    FunArgs *funArgs;
    FunArg *funArg;

    (void)prevArg;
    funArg = bodyArg[0];
    funArgs = ATreeNewFunArgs(funArg, NULL);
    *headArg = funArgs;
    return 0;
}

/*fun_block -> { decls stmts }*/
static int BisonFunblockLdsrHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Stmt *blockStmt;
    Stmts *stmts;

    (void)prevArg;
    stmts = bodyArg[2];
    blockStmt = ATreeNewBlockStmt(bison->env->domain, stmts);
    *headArg = blockStmt;
    return 0;
}

/*struct_define_head -> struct id {*/
static int BisonStructdhStidHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Token *id;

    (void)prevArg;
    id = bodyArg[1];
    *headArg = id;
    return 0;
}

/*struct_defines -> struct_defines struct_define*/
static int BisonStructDefinesSdssdHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    (void)prevArg, (void)bodyArg;
    *headArg = NULL;
    return 0;
}

/*struct_defines -> struct_defines struct_define*/
static int BisonStructDefinesEmptyHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    (void)prevArg, (void)bodyArg;
    *headArg = NULL;
    return 0;
}

/*struct_define -> struct_define_head M9 decls } M10 ;*/
static int BisonStructDefineStimldsrmsHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Type *structType;
    Domain *domain;
    const Token *id;
    DomainEntry *entry;
    Domain *lastDomain;

    (void)prevArg;
    id = bodyArg[0];
    lastDomain = bodyArg[1];
    domain = bison->env->domain;
    if (domain->type != DT_STRUCT) {
        PrErr("domain type error: %d", domain->type);
        exit(-1);
    }
    structType = ATreeNewStructType(domain, domain->alignType, domain->offset_num);
    *headArg = structType;
    bison->env->domain = EnvDomainPop(bison->env->domain);

    entry = EnvNewDomainEntry(id, structType, 0, lastDomain);
    EnvDomainPutEntry(lastDomain, entry);
    return 0;
}

/*M9 -> empty*/
static int BisonStructM9EmptyHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Domain *lastDomain;

    (void)prevArg, (void)bodyArg;
    lastDomain = bison->env->domain;
    bison->env->domain = EnvDomainPush(bison->env->domain, DT_STRUCT);
    *headArg = lastDomain;
    return 0;
}

/*M10 -> empty*/
static int BisonStructM10EmptyHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    (void)headArg, (void)prevArg, (void)bodyArg;
    return 0;
}

/*type_cast -> ( type )*/
static int BisonTypecastLtrHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Type *type;

    (void)prevArg;
    type = bodyArg[1];
    *headArg = type;
    return 0;
}

/*stmt -> return E ;*/
static int BisonStmtResHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Stmt *stmt;
    Expr *expr;

    (void)prevArg;
    expr = bodyArg[1];
    stmt = ATreeNewReturnValueStmt(bison->env->domain, expr);
    *headArg = stmt;
    return 0;
}

/*stmt -> return ;*/
static int BisonStmtRsHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Stmt *stmt;

    (void)prevArg, (void)bodyArg;
    stmt = ATreeNewReturnVoidStmt(bison->env->domain);
    *headArg = stmt;
    return 0;
}

/*fun_call -> id ( fun_parameters )*/
static int BisonFuncallIlfrHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *level1Expr;
    FunRef *funRef;
    const Token *id;
    FunRefParas *paras;
    FunType *funType;
    DomainEntry *domainEntry;

    (void)prevArg;
    id = bodyArg[0];
    paras = bodyArg[2];
    domainEntry = EnvDomainGetEntry(bison->env->domain, id);
    funType = &domainEntry->type->funType;
    funRef = ATreeNewFunRef(id, funType, paras);
    level1Expr = ATreeNewFunRefExpr(bison->env->domain, funRef);
    *headArg = level1Expr;
    return 0;
}

/*fun_parameters -> fun_paras*/
static int BisonFunparametersFpHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    FunRefParas *paras;

    (void)prevArg;
    paras = bodyArg[0];
    *headArg = paras;
    return 0;
}

/*fun_parameters -> empty*/
static int BisonFunparametersEmptyHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    FunRefParas *paras;

    (void)prevArg, (void)bodyArg;
    paras = NULL;
    *headArg = paras;
    return 0;
}

/*fun_paras -> E , fun_paras*/
static int BisonFunparasEcfHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    FunRefParas *paras;
    Expr *expr;
    FunRefParas *next;

    (void)prevArg;
    expr = bodyArg[0];
    next = bodyArg[2];
    paras = ATreeNewFunRefParas(expr, next);
    *headArg = paras;
    return 0;
}

/*fun_paras -> E*/
static int BisonFunparasEHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    FunRefParas *paras;
    Expr *expr;

    (void)prevArg;
    expr = bodyArg[0];
    paras = ATreeNewFunRefParas(expr, NULL);
    *headArg = paras;
    return 0;
}

/*expr -> assign_expr*/
static int BisonExprAssignHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *expr;
    Expr *assignExpr;

    (void)prevArg;
    assignExpr = bodyArg[0];
    expr = assignExpr;
    *headArg = expr;
    return 0;
}

/*assign_expr -> lvalue = assign_expr1*/
static int BisonAssigLeAnHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *assignExpr;
    Expr *lValueExpr, *AssignExpr1;

    (void)prevArg;
    lValueExpr = bodyArg[0];
    AssignExpr1 = bodyArg[2];
    assignExpr = ATreeNewBinExpr(bison->env->domain, ET_ASSIGN, ATreeExprType(lValueExpr), lValueExpr, AssignExpr1);
    *headArg = assignExpr;
    return 0;
}

/*assign_expr -> deref_pointer = assign_expr1*/
static int BisonAssigRpAnHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *assignExpr;
    Expr *refPointer, *AssignExpr1;

    (void)prevArg;
    refPointer = bodyArg[0];
    AssignExpr1 = bodyArg[2];
    assignExpr = ATreeNewBinExpr(bison->env->domain, ET_ASSIGN, ATreeExprType(refPointer), refPointer, AssignExpr1);
    *headArg = assignExpr;
    return 0;
}

/*assign_expr -> lor_expr*/
static int BisonAssignLHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *assignExpr;
    Expr *lorExpr;

    (void)prevArg;
    lorExpr = bodyArg[0];
    assignExpr = lorExpr;
    *headArg = assignExpr;
    return 0;
}

/*lor_expr -> lor_expr1 || land_expr*/
static int BisonLorLolandHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *lorExpr;
    Expr *lorExpr1, *landExpr;

    (void)prevArg;
    lorExpr1 = bodyArg[0];
    landExpr = bodyArg[2];
    lorExpr = ATreeNewBinExpr(bison->env->domain, ET_LOR, bison->env->boolType, lorExpr1, landExpr);
    *headArg = lorExpr;
    return 0;
}

/*lor_expr -> land_expr*/
static int BisonLorLandHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *lorExpr;
    Expr *landExpr;

    (void)prevArg;
    landExpr = bodyArg[0];
    lorExpr = landExpr;
    *headArg = lorExpr;
    return 0;
}

/*land_expr -> land_expr1 && rel_l0_expr*/
static int BisonLandlaRelHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *landExpr;
    Expr *landExpr1, *relL0Expr;

    (void)prevArg;
    landExpr1 = bodyArg[0];
    relL0Expr = bodyArg[2];
    landExpr = ATreeNewBinExpr(bison->env->domain, ET_LAND, bison->env->boolType, landExpr1, relL0Expr);
    *headArg = landExpr;
    return 0;
}

/*land_expr -> rel_l0_expr*/
static int BisonLandRelHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *landExpr;
    Expr *relL0Expr;

    (void)prevArg;
    relL0Expr = bodyArg[0];
    landExpr = relL0Expr;
    *headArg = landExpr;
    return 0;
}

/*rel_l0_expr -> arith_l0_expr1 == arith_l0_expr2*/
static int BisonRel0AritheaHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *relL0Expr;
    Expr *arithL0Expr1, *arithL0Expr2;

    (void)prevArg;
    arithL0Expr1 = bodyArg[0];
    arithL0Expr2 = bodyArg[2];
    relL0Expr = ATreeNewBinExpr(bison->env->domain, ET_EQ, bison->env->boolType, arithL0Expr1, arithL0Expr2);
    *headArg = relL0Expr;
    return 0;
}

/*rel_l0_expr -> arith_l0_expr1 != arith_l0_expr2*/
static int BisonRel0ArithneaHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *relL0Expr;
    Expr *arithL0Expr1, *arithL0Expr2;

    (void)prevArg;
    arithL0Expr1 = bodyArg[0];
    arithL0Expr2 = bodyArg[2];
    relL0Expr = ATreeNewBinExpr(bison->env->domain, ET_NE, bison->env->boolType, arithL0Expr1, arithL0Expr2);
    *headArg = relL0Expr;
    return 0;
}

/*rel_l0_expr -> rel_l1_expr*/
static int BisonRel0Rel1Handle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *relL0Expr;
    Expr *relL1Expr;

    (void)prevArg;
    relL1Expr = bodyArg[0];
    relL0Expr = relL1Expr;
    *headArg = relL0Expr;
    return 0;
}

/*rel_l1_expr -> arith_l0_expr > arith_l0_expr*/
static int BisonRel1ArithgtaHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *relL1Expr;
    Expr *arithL0Expr1, *arithL0Expr2;

    (void)prevArg;
    arithL0Expr1 = bodyArg[0];
    arithL0Expr2 = bodyArg[2];
    relL1Expr = ATreeNewBinExpr(bison->env->domain, ET_GT, bison->env->boolType, arithL0Expr1, arithL0Expr2);
    *headArg = relL1Expr;
    return 0;
}

/*rel_l1_expr -> arith_l0_expr >= arith_l0_expr*/
static int BisonRel1ArithgeaHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *relL1Expr;
    Expr *arithL0Expr1, *arithL0Expr2;

    (void)prevArg;
    arithL0Expr1 = bodyArg[0];
    arithL0Expr2 = bodyArg[2];
    relL1Expr = ATreeNewBinExpr(bison->env->domain, ET_GE, bison->env->boolType, arithL0Expr1, arithL0Expr2);
    *headArg = relL1Expr;
    return 0;
}

/*rel_l1_expr -> arith_l0_expr < arith_l0_expr*/
static int BisonRel1ArithltaHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *relL1Expr;
    Expr *arithL0Expr1, *arithL0Expr2;

    (void)prevArg;
    arithL0Expr1 = bodyArg[0];
    arithL0Expr2 = bodyArg[2];
    relL1Expr = ATreeNewBinExpr(bison->env->domain, ET_LT, bison->env->boolType, arithL0Expr1, arithL0Expr2);
    *headArg = relL1Expr;
    return 0;
}

/*rel_l1_expr -> arith_l0_expr <= arith_l0_expr*/
static int BisonRel1ArithleaHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *relL1Expr;
    Expr *arithL0Expr1, *arithL0Expr2;

    (void)prevArg;
    arithL0Expr1 = bodyArg[0];
    arithL0Expr2 = bodyArg[2];
    relL1Expr = ATreeNewBinExpr(bison->env->domain, ET_LE, bison->env->boolType, arithL0Expr1, arithL0Expr2);
    *headArg = relL1Expr;
    return 0;
}

/*rel_l1_expr -> arith_l0_expr*/
static int BisonRel1ArithHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *relL1Expr;
    Expr *arithL0Expr;

    (void)prevArg;
    arithL0Expr = bodyArg[0];
    relL1Expr = arithL0Expr;
    *headArg = relL1Expr;
    return 0;
}

/*arith_l0_expr -> arith_l0_expr1 + arith_l1_expr*/
static int BisonArith0AaaHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *arithL0Expr;
    Expr *arithL0Expr1, *arithL1Expr;
    Type *exprType;

    (void)prevArg;
    arithL0Expr1 = bodyArg[0];
    arithL1Expr = bodyArg[2];

    exprType = ATreeExprWiddenType(ET_ADD, arithL0Expr1, arithL1Expr);
    arithL0Expr = ATreeNewBinExpr(bison->env->domain, ET_ADD, exprType, arithL0Expr1, arithL1Expr);
    *headArg = arithL0Expr;
    return 0;
}

/*arith_l0_expr -> arith_l0_expr - arith_l1_expr*/
static int BisonArith0AsaHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *arithL0Expr;
    Expr *arithL0Expr1, *arithL1Expr;
    Type *exprType;

    (void)prevArg;
    arithL0Expr1 = bodyArg[0];
    arithL1Expr = bodyArg[2];
    exprType = ATreeExprWiddenType(ET_SUB, arithL0Expr1, arithL1Expr);
    arithL0Expr = ATreeNewBinExpr(bison->env->domain, ET_SUB, exprType, arithL0Expr1, arithL1Expr);
    *headArg = arithL0Expr;
    return 0;
}

/*arith_l0_expr -> arith_l1_expr*/
static int BisonArith0AHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *arithL0Expr;
    Expr *arithL1Expr;

    (void)prevArg;
    arithL1Expr = bodyArg[0];
    arithL0Expr = arithL1Expr;
    *headArg = arithL0Expr;
    return 0;
}

/*arith_l1_expr -> arith_l1_expr1 * level2_expr*/
static int BisonArith1AmlHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *arithL1Expr;
    Expr *arithL1Expr1, *level2Expr;
    Type *exprType;

    (void)prevArg;
    arithL1Expr1 = bodyArg[0];
    level2Expr = bodyArg[2];
    exprType = ATreeExprWiddenType(ET_MUL, arithL1Expr1, level2Expr);
    arithL1Expr = ATreeNewBinExpr(bison->env->domain, ET_MUL, exprType, arithL1Expr1, level2Expr);
    *headArg = arithL1Expr;
    return 0;
}

/*arith_l1_expr -> arith_l1_expr / level2_expr*/
static int BisonArith1AdlHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *arithL1Expr;
    Expr *arithL1Expr1, *level2Expr;
    Type *exprType;

    (void)prevArg;
    arithL1Expr1 = bodyArg[0];
    level2Expr = bodyArg[2];
    exprType = ATreeExprWiddenType(ET_DIV, arithL1Expr1, level2Expr);
    arithL1Expr = ATreeNewBinExpr(bison->env->domain, ET_DIV, exprType, arithL1Expr1, level2Expr);
    *headArg = arithL1Expr;
    return 0;
}

/*arith_l1_expr -> arith_l1_expr % level2_expr*/
static int BisonArith1AmodlHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *arithL1Expr;
    Expr *arithL1Expr1, *level2Expr;
    Type *exprType;

    (void)prevArg;
    arithL1Expr1 = bodyArg[0];
    level2Expr = bodyArg[2];
    exprType = ATreeExprWiddenType(ET_MOD, arithL1Expr1, level2Expr);
    arithL1Expr = ATreeNewBinExpr(bison->env->domain, ET_MOD, exprType, arithL1Expr1, level2Expr);
    *headArg = arithL1Expr;
    return 0;
}

/*arith_l1_expr -> level2_expr*/
static int BisonArith1LHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *arithL1Expr;
    Expr *level2Expr;

    (void)prevArg;
    level2Expr = bodyArg[0];
    arithL1Expr = level2Expr;
    *headArg = arithL1Expr;
    return 0;
}

/*level2_expr -> ! level2_expr2*/
static int BisonLevel2Nl2Handle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *level2Expr;
    Expr *level2Expr2;

    (void)prevArg;
    level2Expr2 = bodyArg[1];
    level2Expr = ATreeNewUnaryExpr(bison->env->domain, ET_LNOT, bison->env->boolType, level2Expr2);
    *headArg = level2Expr;
    return 0;
}

/*ref_pointer -> * level2_expr2*/
static int BisonRefpointerSl2Handle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *level2Expr;
    Expr *level2Expr2;
    Type *type;
    Type *refExprType;

    (void)prevArg;
    level2Expr2 = bodyArg[1];
    refExprType = ATreeExprType(level2Expr2);
    if (refExprType->type == TT_POINTER) {
        type = refExprType->pointer.type;
    } else {
        PrErr("refExprType->type error: %d", refExprType->type);
        exit(-1);
    }
    level2Expr = ATreeNewUnaryExpr(bison->env->domain, ET_REF_POINTER, type, level2Expr2);
    *headArg = level2Expr;
    return 0;
}

/* level2_expr -> ref_pointer*/
static int BisonLevel2RefpHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *level2Expr;

    (void)prevArg;
    level2Expr = bodyArg[0];
    *headArg = level2Expr;
    return 0;
}

/*level2_expr -> & level2_expr2*/
static int BisonLevel2Al2Handle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *level2Expr;
    Expr *level2Expr2;
    Type *type;
    Type *pointType;

    (void)prevArg;
    level2Expr2 = bodyArg[1];
    pointType = ATreeExprType(level2Expr2);
    type = ATreeNewPointerType(pointType);
    level2Expr = ATreeNewUnaryExpr(bison->env->domain, ET_GET_ADDR, type, level2Expr2);
    *headArg = level2Expr;
    return 0;
}

/*level2_expr -> type_cast level2_expr2*/
static int BisonLevel2Tcl2Handle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *level2Expr;
    Expr *level2Expr2;
    Type *type;

    (void)prevArg;
    level2Expr2 = bodyArg[1];
    type = bodyArg[0];
    level2Expr = ATreeNewUnaryExpr(bison->env->domain, ET_TYPE_CAST, type, level2Expr2);
    *headArg = level2Expr;
    return 0;
}

/*level2_expr -> factor*/
static int BisonLevel2FactorHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *level2Expr;
    Expr *factorExpr;

    (void)prevArg;
    factorExpr = bodyArg[0];
    level2Expr = factorExpr;
    *headArg = level2Expr;
    return 0;
}

/*factor -> ( E )*/
static int BisonFactorLerHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *factorExpr;
    Expr *eExpr;

    (void)prevArg;
    eExpr = bodyArg[1];
    factorExpr = eExpr;
    *headArg = factorExpr;
    return 0;
}

/*factor -> idigit*/
static int BisonFactorIHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *factorExpr;
    const Token *idigitToken;

    (void)prevArg;
    idigitToken = bodyArg[0];
    factorExpr = ATreeNewCValueExpr(bison->env->domain, CVT_IDIGIT, idigitToken);
    *headArg = factorExpr;
    return 0;
}

/*factor -> fdigit*/
static int BisonFactorFHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *factorExpr;
    const Token *fdigitToken;

    (void)prevArg;
    fdigitToken = bodyArg[0];
    factorExpr = ATreeNewCValueExpr(bison->env->domain, CVT_FDIGIT, fdigitToken);
    *headArg = factorExpr;
    return 0;
}

/*factor -> lvalue*/
static int BisonFactorLvalueHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *factorExpr;
    Expr *lValueExpr;

    (void)prevArg;
    lValueExpr = bodyArg[0];
    factorExpr = lValueExpr;
    *headArg = factorExpr;
    return 0;
}

/*factor -> mem_access*/
static int BisonFactorMaHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *factorExpr;
    Expr *dmaExpr;

    (void)prevArg;
    dmaExpr = bodyArg[0];
    factorExpr = dmaExpr;
    *headArg = factorExpr;
    return 0;
}

/*M5 -> empty*/
static int BisonM5EHandle(void **headArg, void *parentPrevArg, void *parentBodyArg[])
{
    AEDims *arrDerefDims;
    Type *parentType;
    const Token *id;

    (void)parentPrevArg;
    id = parentBodyArg[0];
    parentType = EnvDomainGetTokenType(bison->env->domain, id);
    if (parentType->type == TT_ARRAY) {
        Type *elementType;

        elementType = parentType->array.type;
        arrDerefDims = ATreeNewAEDims(elementType, NULL, NULL);
    } else {
        arrDerefDims = NULL;
    }
    *headArg = arrDerefDims;
    return 0;
}

/*lvalue -> id_expr*/
static int BisonLvalueIdHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *lValueExpr;

    (void)prevArg;
    lValueExpr = bodyArg[0];
    *headArg = lValueExpr;
    return 0;
}

/*lvalue -> access_element*/
static int BisonLvalueAccessElmHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *lValueExpr;

    (void)prevArg;
    lValueExpr = bodyArg[0];
    *headArg = lValueExpr;
    return 0;
}

/*lvalue -> access_member*/
static int BisonLvalueAccessmemberHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *lValueExpr;

    (void)prevArg;
    lValueExpr = bodyArg[0];
    *headArg = lValueExpr;
    return 0;
}

static Type *BisonGetAccessMemExprType(Expr *baseExpr, const Token *memId, Expr **offset)
{
    DomainEntry *entry;
    Type *baseType;
    Domain *stDomain;
    Expr *memOffset;

    baseType = ATreeExprType(baseExpr);
    if (baseType->type != TT_STRUCTURE) {
        PrErr("baseType->type error: %d", baseType->type);
        exit(-1);
    }
    stDomain = baseType->st.domain;
    entry = EnvDomainGetEntry(stDomain, memId);
    memOffset = ATreeNewCValueSpcIntExpr(bison->env->domain, entry->offset);
    *offset = memOffset;
    return entry->type;
}

/*mem_access -> lvalue . id*/
static int BisonMemaccessLvdiHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *memAccessExpr;
    const Token *memId;
    Expr *baseExpr;
    Type *memType;
    Expr *memOffset;

    (void)prevArg;
    baseExpr = bodyArg[0];
    memId = bodyArg[2];
    memType = BisonGetAccessMemExprType(baseExpr, memId, &memOffset);
    memAccessExpr = ATreeNewAccessMem(bison->env->domain, AMTYPE_DIRECT, baseExpr, memId, memType, memOffset);
    *headArg = memAccessExpr;
    return 0;
}

/*mem_access -> level1_expr . id*/
static int BisonMemaccessMadiHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *memAccessExpr;
    const Token *memId;
    Expr *baseExpr;
    Type *memType;
    Expr *memOffset;

    (void)prevArg;
    baseExpr = bodyArg[0];
    memId = bodyArg[2];
    memType = BisonGetAccessMemExprType(baseExpr, memId, &memOffset);
    memAccessExpr = ATreeNewAccessMem(bison->env->domain, AMTYPE_DIRECT, baseExpr, memId, memType, memOffset);
    *headArg = memAccessExpr;
    return 0;
}

static Type *BisonGetAccessMemExprType2(Expr *baseExpr, const Token *memId, Expr **offset)
{
    DomainEntry *entry;
    Type *baseType;
    Domain *stDomain;
    Expr *memOffset;

    baseType = ATreeExprType(baseExpr);

    if (baseType->type != TT_POINTER) {
        PrErr("baseType->type error: %d", baseType->type);
        exit(-1);
    }
    baseType = baseType->pointer.type;
    if (baseType->type != TT_STRUCTURE) {
        PrErr("baseType->type error: %d", baseType->type);
        exit(-1);
    }
    stDomain = baseType->st.domain;
    entry = EnvDomainGetEntry(stDomain, memId);
    memOffset = ATreeNewCValueSpcIntExpr(bison->env->domain, entry->offset);
    *offset = memOffset;
    return entry->type;
}

/*mem_access -> level1_expr -> id*/
static int BisonMemaccessLvpiHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *memAccessExpr;
    const Token *memId;
    Expr *baseExpr;
    Type *memType;
    Expr *memOffset;

    (void)prevArg;
    baseExpr = bodyArg[0];
    memId = bodyArg[2];
    memType = BisonGetAccessMemExprType2(baseExpr, memId, &memOffset);
    memAccessExpr = ATreeNewAccessMem(bison->env->domain, AMTYPE_INDIRECT, baseExpr, memId, memType, memOffset);
    *headArg = memAccessExpr;
    return 0;
}

/*mem_access -> mem_access -> id*/
static int BisonMemaccessMapiHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *memAccessExpr;
    const Token *memId;
    Expr *baseExpr;
    Type *memType;
    Expr *memOffset;

    (void)prevArg;
    baseExpr = bodyArg[0];
    memId = bodyArg[2];
    memType = BisonGetAccessMemExprType2(baseExpr, memId, &memOffset);
    memAccessExpr = ATreeNewAccessMem(bison->env->domain, AMTYPE_INDIRECT, baseExpr, memId, memType, memOffset);
    *headArg = memAccessExpr;
    return 0;
}

/*level1_expr -> access_mem*/
static int BisonLevel1exprAccessmemHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *level1Expr;

    (void)prevArg;
    level1Expr = bodyArg[0];
    *headArg = level1Expr;
    return 0;
}

/*level1_expr -> fun_call*/
static int BisonLevel1exprFuncallHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *level1Expr;

    (void)prevArg;
    level1Expr = bodyArg[0];
    *headArg = level1Expr;
    return 0;
}

/*level1_expr -> access_element*/
static int BisonLevel1exprAccesselmHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *level1Expr;

    (void)prevArg;
    level1Expr = bodyArg[0];
    *headArg = level1Expr;
    return 0;
}

/*level1_expr -> factor*/
static int BisonLevel1exprFactorHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *level1Expr;

    (void)prevArg;
    level1Expr = bodyArg[0];
    *headArg = level1Expr;
    return 0;
}

/*M6 -> empty*/
static int BisonM6EHandle(void **headArg, void *parentPrevArg, void *parentBodyArg[])
{
    AEDims *arrDerefDims;
    AEDims *parentArrDerefDims;
    Type *prevType;

    (void)parentBodyArg;
    parentArrDerefDims = parentPrevArg;

    if (!parentArrDerefDims) {
        PrErr("syntax error, decl is not array\n");
        exit(-1);
    }
    prevType = parentArrDerefDims->type;
    if (prevType->type == TT_ARRAY) {
        Type *elementType;

        elementType = prevType->array.type;
        arrDerefDims = ATreeNewAEDims(elementType, NULL, NULL);
    } else {
        arrDerefDims = NULL;
    }
    *headArg = arrDerefDims;
    return 0;
}

/*arr_deref_dim -> [ arr_idx ] M6 arr_deref_dim1*/
static int BisonDedimLidHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    AEDims *arrDerefDims;
    AEDims *arrDerefDims1;
    Expr *idxExpr;

    arrDerefDims = prevArg;
    arrDerefDims1 = bodyArg[4];
    idxExpr = bodyArg[1];

    arrDerefDims->nextDims = arrDerefDims1;
    arrDerefDims->idx = idxExpr;
    *headArg = arrDerefDims;
    return 0;
}

/*arr_deref_dim -> empty*/
static int BisonDedimEHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    AEDims *arrDerefDims;

    (void)headArg, (void)prevArg, (void)bodyArg;
    arrDerefDims = NULL;
    *headArg = arrDerefDims;
    return 0;
}

/*arr_idx -> idigit*/
static int BisonArridxIdigitHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *arrIdxExpr;
    const Token *idigitToken;

    (void)prevArg;
    idigitToken = bodyArg[0];
    arrIdxExpr = ATreeNewCValueExpr(bison->env->domain, CVT_IDIGIT, idigitToken);
    *headArg = arrIdxExpr;
    return 0;
}

/*id_expr -> id */
static int BisonIdexprIdHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *idExpr;
    const Token *id;
    DomainEntry *entry;
    Domain *domain = bison->env->domain;

    (void)prevArg;
    id = bodyArg[0];
    entry = EnvDomainGetEntry(domain, id);
    if (!entry) {
        PrErr("entry error: %p\n", entry);
        exit(-1);
    }
    idExpr = ATreeNewIdExpr(domain, id, entry->type);
    *headArg = idExpr;
    return 0;
}

/*
 * 功能：把访问元素的类型补充完整。
 * 返回值：
 **/
static void BisonAccessEmlDimsInferType(Type *superiorType, AEDims *dims, Type **pType)
{
    Type *type;

    if (superiorType->type == TT_ARRAY) {
        type = superiorType->array.type;
    } else if (superiorType->type == TT_POINTER) {
        type = superiorType->pointer.type;
    } else {
        PrErr("superiorType->type error: %d", superiorType->type);
        exit(-1);
    }
    dims->type = type;
    *pType = type;
    if (dims->nextDims) {
        BisonAccessEmlDimsInferType(type, dims->nextDims, pType);
    }
}

/*access_elm -> id_expr dim dims*/
static int BisonAccesselmIddimdimsHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    AEDims *dims;
    AEDims *nextDims;
    Expr *idx;
    Expr *accessElm;
    Expr *baseExpr;
    Type *accessElmType;

    (void)prevArg;
    idx = bodyArg[1];
    nextDims = bodyArg[2];
    dims = ATreeNewAEDims(NULL, idx, nextDims);
    baseExpr = bodyArg[0];
    accessElm = ATreeNewAccessElmExpr(bison->env->domain, baseExpr, dims, NULL);
    BisonAccessEmlDimsInferType(ATreeExprType(baseExpr), accessElm->accessElm.dims, &accessElmType);
    accessElm->accessElm.type = accessElmType;
    *headArg = accessElm;
    return 0;
}

/*dims -> dim dims1*/
static int BisonDimsDimdimsHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    AEDims *dims;
    AEDims *nextDims;
    Expr *idx;

    (void)prevArg;
    idx = bodyArg[0];
    nextDims = bodyArg[1];
    dims = ATreeNewAEDims(NULL, idx, nextDims);
    *headArg = dims;
    return 0;
}

/*dims -> empty*/
static int BisonDimsEmptyHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    AEDims *dims;

    (void)bodyArg, (void)prevArg;
    dims = NULL;
    *headArg = dims;
    return 0;
}

/*dim -> [ idx ] */
static int  BisonDimLidxrHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Expr *idx;

    (void)prevArg;
    idx = bodyArg[1];
    *headArg = idx;
    return 0;
}

/*
 * 功能：添加文法的产生式和相关的归约处理函数。
 * 返回值：
 **/
static int GrammarAddProduct(ContextFreeGrammar *grammar)
{
    int error = 0;

    error = CfgAddProductH(grammar, RHT_SELF, BisonStartxPHandle, SYMID_ARR(SID_STARTx, SID_PROGRAM));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonProgDHandle, SYMID_ARR(SID_PROGRAM, SID_COMPONENT_DEFINES));
    if (error != -ENOERR)
        goto err;
    //error = CfgAddProductH(grammar, RHT_SELF, BisonStFunDefinesSfDssfdHandle, SYMID_ARR(SID_STRUCT_FUN_DEFINES, SID_STRUCT_FUN_DEFINES, SID_STRUCT_FUN_DEFINE));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonConponentdefCdsstdefHandle, SYMID_ARR(SID_COMPONENT_DEFINES, SID_COMPONENT_DEFINES, SID_STRUCT_DEFINE));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonConponentdefCdsfundefHandle, SYMID_ARR(SID_COMPONENT_DEFINES, SID_COMPONENT_DEFINES, SID_FUN_DEFINE));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonConponentdefCdsEmptyHandle, SYMID_ARR(SID_COMPONENT_DEFINES, SID_EMPTY));
    if (error != -ENOERR)
        goto err;
    //error = CfgAddProductH(grammar, RHT_SELF, BisonStfundefineStfunHandle, SYMID_ARR(SID_STRUCT_FUN_DEFINE, SID_STRUCT_DEFINE, SID_FUN_DEFINE));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonM1eHandle, SYMID_ARR(SID_M1, SID_EMPTY));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonM2eHandle, SYMID_ARR(SID_M2, SID_EMPTY));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonBlockLdsrHandle, SYMID_ARR(SID_BLOCK, SID_TM_LBRACE, SID_M1, SID_DECLS, SID_STMTS, SID_M2, SID_TM_RBRACE));
    if (error != -ENOERR)
        goto err;

    /*声明*/
    error = CfgAddProductH(grammar, RHT_SELF, BisonDeclsDdHandle, SYMID_ARR(SID_DECLS, SID_DECLS, SID_DECL));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonDeclsEHandle, SYMID_ARR(SID_DECLS, SID_EMPTY));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonDeclTiHandle, SYMID_ARR(SID_DECL, SID_TYPE, SID_TM_ID, SID_TM_SEMI_COLON));
    if (error != -ENOERR)
        goto err;

    /*类型*/
    error = CfgAddProductH(grammar, RHT_PARENT, BisonM3eHandle, SYMID_ARR(SID_M3, SID_EMPTY));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonTypeBdHandle, SYMID_ARR(SID_TYPE, SID_BASE_TYPE, SID_M3, SID_ARR_DECL_DIM));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonTypeTsHandle, SYMID_ARR(SID_TYPE, SID_TYPE, SID_TM_MUL));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonTypeStHandle, SYMID_ARR(SID_TYPE, SID_STRUCT_TYPE));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonStructTypeSiHandle, SYMID_ARR(SID_STRUCT_TYPE, SID_TM_STRUCT, SID_TM_ID));
    if (error != -ENOERR)
        goto err;

    /*基本类型*/
    error = CfgAddProductH(grammar, RHT_SELF, BisonBaseIntHandle, SYMID_ARR(SID_BASE_TYPE, SID_TM_INT));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonBaseCharHandle, SYMID_ARR(SID_BASE_TYPE, SID_TM_CHAR));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonBaseShortHandle, SYMID_ARR(SID_BASE_TYPE, SID_TM_SHORT));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonBaseBoolHandle, SYMID_ARR(SID_BASE_TYPE, SID_TM_BOOL));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonBaseFloatHandle, SYMID_ARR(SID_BASE_TYPE, SID_TM_FLOAT));
    if (error != -ENOERR)
        goto err;

    /*数组*/
    error = CfgAddProductH(grammar, RHT_PARENT, BisonM4eHandle, SYMID_ARR(SID_M4, SID_EMPTY));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonDimLdrdHandle, SYMID_ARR(SID_ARR_DECL_DIM, SID_TM_LSBRACKET, SID_TM_IDIGIT, SID_TM_RSBRACKET, SID_M4, SID_ARR_DECL_DIM));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonDimEHandle, SYMID_ARR(SID_ARR_DECL_DIM, SID_EMPTY));
    if (error != -ENOERR)
        goto err;

    /*语句*/
    error = CfgAddProductH(grammar, RHT_SELF, BisonStmtsSSHandle, SYMID_ARR(SID_STMTS, SID_STMTS, SID_STMT));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonStmtsEHandle, SYMID_ARR(SID_STMTS, SID_EMPTY));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonStmtEsHandle, SYMID_ARR(SID_STMT, SID_EXPR, SID_TM_SEMI_COLON));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonStmtIlcrsHandle, SYMID_ARR(SID_STMT, SID_TM_IF, SID_TM_LBRACKET, SID_COND, SID_TM_RBRACKET, SID_STMT));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonStmtIlcrsesHandle, SYMID_ARR(SID_STMT, SID_TM_IF, SID_TM_LBRACKET, SID_COND, SID_TM_RBRACKET, SID_STMT, SID_TM_ELSE, SID_STMT));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonStmtWlcrsHandle, SYMID_ARR(SID_STMT, SID_TM_WHILE, SID_TM_LBRACKET, SID_COND, SID_TM_RBRACKET, SID_STMT));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonStmtDswlcrHandle, SYMID_ARR(SID_STMT, SID_TM_DO, SID_STMT, SID_TM_WHILE, SID_TM_LBRACKET, SID_COND, SID_TM_RBRACKET, SID_TM_SEMI_COLON));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonStmtResHandle, SYMID_ARR(SID_STMT, SID_TM_RETURN, SID_EXPR, SID_TM_SEMI_COLON));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonStmtRsHandle, SYMID_ARR(SID_STMT, SID_TM_RETURN, SID_TM_SEMI_COLON));
    if (error != -ENOERR)
        goto err;

    /*switch语句*/
    error = CfgAddProductH(grammar, RHT_SELF, BisonCasestmtCecsHandle, SYMID_ARR(SID_CASE_STMT, SID_TM_CASE, SID_EXPR, SID_TM_COLON, SID_STMTS));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonCasestmtDcsHandle, SYMID_ARR(SID_CASE_STMT, SID_TM_DEFAULT, SID_TM_COLON, SID_STMTS));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonCasestmtsCcHandle, SYMID_ARR(SID_CASE_STMTS, SID_CASE_STMTS, SID_CASE_STMT));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonCasestmtsEHandle, SYMID_ARR(SID_CASE_STMTS, SID_EMPTY));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonStmtSlerlcrHandle, SYMID_ARR(SID_STMT, SID_TM_SWITCH, SID_TM_LBRACKET, SID_EXPR, SID_TM_RBRACKET, SID_TM_LBRACE, SID_CASE_STMTS, SID_TM_RBRACE));
    if (error != -ENOERR)
        goto err;
    /*块语句*/
    error = CfgAddProductH(grammar, RHT_SELF, BisonStmtBHandle, SYMID_ARR(SID_STMT, SID_BLOCK));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonStmtBsHandle, SYMID_ARR(SID_STMT, SID_TM_BREAK, SID_TM_SEMI_COLON));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonStmtCsHandle, SYMID_ARR(SID_STMT, SID_TM_CONTINUE, SID_TM_SEMI_COLON));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonCondLorHandle, SYMID_ARR(SID_COND, SID_LOR_EXPR));
    if (error != -ENOERR)
        goto err;

    /*函数定义*/
#if 0
    error = CfgAddProductH(grammar, RHT_SELF, BisonFundefinesFfHandle, SYMID_ARR(SID_FUN_DEFINES, SID_FUN_DEFINES, SID_FUN_DEFINE));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonFundefinesEmptyHandle, SYMID_ARR(SID_FUN_DEFINES, SID_EMPTY));
    if (error != -ENOERR)
        goto err;
#endif
    error = CfgAddProductH(grammar, RHT_SELF, BisonFundefineFilfrbHandle, SYMID_ARR(SID_FUN_DEFINE, SID_M7, SID_FUN_RET_TYPE, SID_TM_ID, SID_TM_LBRACKET, SID_FUN_ARGUES, SID_TM_RBRACKET, SID_M8, SID_FUN_BLOCK));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonM7EmptyHandle, SYMID_ARR(SID_M7, SID_EMPTY));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_PARENT, BisonM8EmptyHandle, SYMID_ARR(SID_M8, SID_EMPTY));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonFunrettypeVHandle, SYMID_ARR(SID_FUN_RET_TYPE, SID_TM_VOID));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonFunrettypeBtHandle, SYMID_ARR(SID_FUN_RET_TYPE, SID_BASE_TYPE));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonFunrettypeSttHandle, SYMID_ARR(SID_FUN_RET_TYPE, SID_STRUCT_TYPE));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonFunarguesFHandle, SYMID_ARR(SID_FUN_ARGUES, SID_FUN_ARGS));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonFunarguesVHandle, SYMID_ARR(SID_FUN_ARGUES, SID_TM_VOID));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonFunarguesEmptyHandle, SYMID_ARR(SID_FUN_ARGUES, SID_EMPTY));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonFunargTiHandle, SYMID_ARR(SID_FUN_ARG, SID_TYPE, SID_TM_ID));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonFunargsFcfHandle, SYMID_ARR(SID_FUN_ARGS, SID_FUN_ARG, SID_TM_COMMA, SID_FUN_ARGS));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonFunargsFHandle, SYMID_ARR(SID_FUN_ARGS, SID_FUN_ARG));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonFunblockLdsrHandle, SYMID_ARR(SID_FUN_BLOCK, SID_TM_LBRACE, SID_DECLS, SID_STMTS, SID_TM_RBRACE));
    if (error != -ENOERR)
        goto err;

    /*结构体定义*/
    error = CfgAddProductH(grammar, RHT_SELF, BisonStructdhStidHandle, SYMID_ARR(SID_STRUCT_DEFINE_HEAD, SID_TM_STRUCT, SID_TM_ID, SID_TM_LBRACE));
    if (error != -ENOERR)
        goto err;
#if 0
    error = CfgAddProductH(grammar, RHT_SELF, BisonStructDefinesSdssdHandle, SYMID_ARR(SID_STRUCT_DEFINES, SID_STRUCT_DEFINES, SID_STRUCT_DEFINE));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonStructDefinesEmptyHandle, SYMID_ARR(SID_STRUCT_DEFINES, SID_EMPTY));
    if (error != -ENOERR)
        goto err;
#endif
    error = CfgAddProductH(grammar, RHT_SELF, BisonStructDefineStimldsrmsHandle, SYMID_ARR(SID_STRUCT_DEFINE, SID_STRUCT_DEFINE_HEAD, SID_M9, SID_DECLS, SID_TM_RBRACE, SID_M10, SID_TM_SEMI_COLON));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonStructM9EmptyHandle, SYMID_ARR(SID_M9, SID_EMPTY));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonStructM10EmptyHandle, SYMID_ARR(SID_M10, SID_EMPTY));
    if (error != -ENOERR)
        goto err;

    /*类型转换*/
    error = CfgAddProductH(grammar, RHT_SELF, BisonTypecastLtrHandle, SYMID_ARR(SID_TYPE_CAST, SID_TM_LBRACKET, SID_TYPE, SID_TM_RBRACKET));
    if (error != -ENOERR)
        goto err;

    /*表达式*/
    error = CfgAddProductH(grammar, RHT_SELF, BisonExprAssignHandle, SYMID_ARR(SID_EXPR, SID_ASSIGN_EXPR));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonAssigLeAnHandle, SYMID_ARR(SID_ASSIGN_EXPR, SID_LVALUE, SID_TM_ASSIGN, SID_ASSIGN_EXPR));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonAssigRpAnHandle, SYMID_ARR(SID_ASSIGN_EXPR, SID_REF_POINTER, SID_TM_ASSIGN, SID_ASSIGN_EXPR));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonAssignLHandle, SYMID_ARR(SID_ASSIGN_EXPR, SID_LOR_EXPR));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonLorLolandHandle, SYMID_ARR(SID_LOR_EXPR, SID_LOR_EXPR, SID_TM_LOR, SID_LAND_EXPR));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonLorLandHandle, SYMID_ARR(SID_LOR_EXPR, SID_LAND_EXPR));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonLandlaRelHandle, SYMID_ARR(SID_LAND_EXPR, SID_LAND_EXPR, SID_TM_LAND, SID_REL_L0_EXPR));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonLandRelHandle, SYMID_ARR(SID_LAND_EXPR, SID_REL_L0_EXPR));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonRel0AritheaHandle, SYMID_ARR(SID_REL_L0_EXPR, SID_ARITH_L0_EXPR, SID_TM_EQ, SID_ARITH_L0_EXPR));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonRel0ArithneaHandle, SYMID_ARR(SID_REL_L0_EXPR, SID_ARITH_L0_EXPR, SID_TM_NE, SID_ARITH_L0_EXPR));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonRel0Rel1Handle, SYMID_ARR(SID_REL_L0_EXPR, SID_REL_L1_EXPR));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonRel1ArithltaHandle, SYMID_ARR(SID_REL_L1_EXPR, SID_ARITH_L0_EXPR, SID_TM_LT, SID_ARITH_L0_EXPR));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonRel1ArithleaHandle, SYMID_ARR(SID_REL_L1_EXPR,  SID_ARITH_L0_EXPR, SID_TM_LE, SID_ARITH_L0_EXPR));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonRel1ArithgtaHandle, SYMID_ARR(SID_REL_L1_EXPR, SID_ARITH_L0_EXPR, SID_TM_GT, SID_ARITH_L0_EXPR));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonRel1ArithgeaHandle, SYMID_ARR(SID_REL_L1_EXPR, SID_ARITH_L0_EXPR, SID_TM_GE, SID_ARITH_L0_EXPR));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonRel1ArithHandle, SYMID_ARR(SID_REL_L1_EXPR, SID_ARITH_L0_EXPR));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonArith0AaaHandle, SYMID_ARR(SID_ARITH_L0_EXPR, SID_ARITH_L0_EXPR, SID_TM_ADD, SID_ARITH_L1_EXPR));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonArith0AsaHandle, SYMID_ARR(SID_ARITH_L0_EXPR, SID_ARITH_L0_EXPR, SID_TM_SUB, SID_ARITH_L1_EXPR));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonArith0AHandle, SYMID_ARR(SID_ARITH_L0_EXPR, SID_ARITH_L1_EXPR));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonArith1AmlHandle, SYMID_ARR(SID_ARITH_L1_EXPR, SID_ARITH_L1_EXPR, SID_TM_MUL, SID_LEVEL2_EXPR));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonArith1AdlHandle, SYMID_ARR(SID_ARITH_L1_EXPR, SID_ARITH_L1_EXPR, SID_TM_DIV, SID_LEVEL2_EXPR));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonArith1AmodlHandle, SYMID_ARR(SID_ARITH_L1_EXPR, SID_ARITH_L1_EXPR, SID_TM_MOD, SID_LEVEL2_EXPR));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonArith1LHandle, SYMID_ARR(SID_ARITH_L1_EXPR, SID_LEVEL2_EXPR));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonLevel2Nl2Handle, SYMID_ARR(SID_LEVEL2_EXPR, SID_TM_LNOT, SID_LEVEL2_EXPR));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonRefpointerSl2Handle, SYMID_ARR(SID_REF_POINTER, SID_TM_MUL, SID_LEVEL2_EXPR));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonLevel2RefpHandle, SYMID_ARR(SID_LEVEL2_EXPR, SID_REF_POINTER));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonLevel2Al2Handle, SYMID_ARR(SID_LEVEL2_EXPR, SID_TM_BNAD, SID_LEVEL2_EXPR));
    if (error != -ENOERR)
        goto err;

    error = CfgAddProductH(grammar, RHT_SELF, BisonLevel2Tcl2Handle, SYMID_ARR(SID_LEVEL2_EXPR, SID_TYPE_CAST, SID_LEVEL2_EXPR));
    if (error != -ENOERR)
        goto err;

    error = CfgAddProductH(grammar, RHT_SELF, BisonLevel2FactorHandle, SYMID_ARR(SID_LEVEL2_EXPR, SID_LEVEL1_EXPR));
    if (error != -ENOERR)
        goto err;

    /*成员访问*/
    error = CfgAddProductH(grammar, RHT_SELF, BisonMemaccessMadiHandle, SYMID_ARR(SID_ACCESS_MEMBER, SID_LEVEL1_EXPR, SID_TM_DOT, SID_TM_ID));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonMemaccessLvpiHandle, SYMID_ARR(SID_ACCESS_MEMBER, SID_LEVEL1_EXPR, SID_TM_ARROW, SID_TM_ID));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonLevel1exprAccessmemHandle, SYMID_ARR(SID_LEVEL1_EXPR, SID_ACCESS_MEMBER));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonLevel1exprFuncallHandle, SYMID_ARR(SID_LEVEL1_EXPR, SID_FUN_CALL));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonFuncallIlfrHandle, SYMID_ARR(SID_FUN_CALL, SID_TM_ID, SID_TM_LBRACKET, SID_FUN_PARAMETERS, SID_TM_RBRACKET));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonLevel1exprAccesselmHandle, SYMID_ARR(SID_LEVEL1_EXPR, SID_ACCESS_ELM));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonLevel1exprFactorHandle, SYMID_ARR(SID_LEVEL1_EXPR, SID_FACTOR));
    if (error != -ENOERR)
        goto err;


    error = CfgAddProductH(grammar, RHT_SELF, BisonFactorLerHandle, SYMID_ARR(SID_FACTOR, SID_TM_LBRACKET, SID_EXPR, SID_TM_RBRACKET));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonFactorIHandle, SYMID_ARR(SID_FACTOR, SID_TM_IDIGIT));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonFactorFHandle, SYMID_ARR(SID_FACTOR, SID_TM_FDIGIT));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonFactorLvalueHandle, SYMID_ARR(SID_FACTOR, SID_ID_EXPR));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonFunparametersFpHandle, SYMID_ARR(SID_FUN_PARAMETERS, SID_FUN_PARAS));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonFunparametersEmptyHandle, SYMID_ARR(SID_FUN_PARAMETERS, SID_EMPTY));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonFunparasEcfHandle, SYMID_ARR(SID_FUN_PARAS, SID_EXPR, SID_TM_COMMA, SID_FUN_PARAS));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonFunparasEHandle, SYMID_ARR(SID_FUN_PARAS, SID_EXPR));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_PARENT, BisonM5EHandle, SYMID_ARR(SID_M5, SID_EMPTY));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonLvalueIdHandle, SYMID_ARR(SID_LVALUE, SID_ID_EXPR));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonLvalueAccessElmHandle, SYMID_ARR(SID_LVALUE, SID_ACCESS_ELM));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonLvalueAccessmemberHandle, SYMID_ARR(SID_LVALUE, SID_ACCESS_MEMBER));
    if (error != -ENOERR)
        goto err;
    //error = CfgAddProductH(grammar, RHT_SELF, BisonLvalueIDedimHandle, SYMID_ARR(SID_LVALUE, SID_TM_ID, SID_M5, SID_ARR_DEREF_DIMx));
    if (error != -ENOERR)
        goto err;

    error = CfgAddProductH(grammar, RHT_PARENT, BisonM6EHandle, SYMID_ARR(SID_M6, SID_EMPTY));
    if (error != -ENOERR)
        goto err;
    //error = CfgAddProductH(grammar, RHT_SELF, BisonDedimLidHandle, SYMID_ARR(SID_ARR_DEREF_DIMx, SID_TM_LSBRACKET, SID_EXPR, SID_TM_RSBRACKET, SID_M6, SID_ARR_DEREF_DIMx));
    if (error != -ENOERR)
        goto err;
    //error = CfgAddProductH(grammar, RHT_SELF, BisonDedimEHandle, SYMID_ARR(SID_ARR_DEREF_DIMx, SID_EMPTY));
    if (error != -ENOERR)
        goto err;
    //error = CfgAddProductH(grammar, RHT_SELF, BisonArridxIdigitHandle, SYMID_ARR(SID_ARR_IDX, SID_TM_IDIGIT));
    if (error != -ENOERR)
        goto err;
    //error = CfgAddProductH(grammar, RHT_SELF, BisonArridxIdHandle, SYMID_ARR(SID_ARR_IDX, SID_TM_ID));
    if (error != -ENOERR)
        goto err;

    error = CfgAddProductH(grammar, RHT_SELF, BisonIdexprIdHandle, SYMID_ARR(SID_ID_EXPR, SID_TM_ID));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonAccesselmIddimdimsHandle, SYMID_ARR(SID_ACCESS_ELM, SID_ACCESS_MEMBER, SID_ARR_DEREF_DIM, SID_ARR_DEREF_DIMS));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonAccesselmIddimdimsHandle, SYMID_ARR(SID_ACCESS_ELM, SID_FACTOR, SID_ARR_DEREF_DIM, SID_ARR_DEREF_DIMS));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonAccesselmIddimdimsHandle, SYMID_ARR(SID_ACCESS_ELM, SID_FUN_CALL, SID_ARR_DEREF_DIM, SID_ARR_DEREF_DIMS));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonDimsDimdimsHandle, SYMID_ARR(SID_ARR_DEREF_DIMS, SID_ARR_DEREF_DIM, SID_ARR_DEREF_DIMS));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonDimsEmptyHandle, SYMID_ARR(SID_ARR_DEREF_DIMS, SID_EMPTY));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, BisonDimLidxrHandle, SYMID_ARR(SID_ARR_DEREF_DIM, SID_TM_LSBRACKET, SID_EXPR, SID_TM_RSBRACKET));
    if (error != -ENOERR)
        goto err;

    return 0;

err:
    return error;
}

/*
 * 功能：设计文法
 * 返回值：
 **/
static int DesignGrammar(ContextFreeGrammar **pGrammar)
{
    int error = 0;
    ContextFreeGrammar *grammar;

    error = ContextFreeGrammarAlloc(&grammar, SID_EMPTY, SID_END, SID_START);
    if (error != -ENOERR) {
        return error;
    }
    *pGrammar = grammar;
    error = GrammarAddSymbol(grammar);
    if (error != -ENOERR)
        goto err;
    error = GrammarAddProduct(grammar);
    if (error != -ENOERR)
        goto err;
    return 0;

err:
    printf("Design grammar fail.\n");
    ContextFreeGrammarFree(grammar);
    *pGrammar = NULL;
    return error;
}

/*
 * 功能：生成LR语法分析表。
 * 返回值：
 **/
static int GenLrParseTable(Lalr *lalr)
{
    int error = 0;

    error = LalrGenItemSet(lalr);
    if (error != -ENOERR)
        return error;
    return LalrGenParseTable(lalr);
}

/*
 * 功能：当前输入前进一个位置。
 * 返回值：
 **/
static int BisonInputPosInc(struct _LrReader *reader)
{
    Lex *lex;

    lex = reader->arg;
    LexScan(lex);
    return 0;
}

/*
 * 功能：获取当前的输入对应的文法符号id。
 * 返回值：
 **/
static int BisonGetCurrentSymId(struct _LrReader *reader)
{
    Lex *lex = reader->arg;
    const Token *token;

    token = LexCurrent(lex);

    if (token->tag != TT_EOF) {
        if (token->tag == TT_DIGIT) {
            //PrDbg("current token: %d\n", token->digit);
        } else if (token->tag == TT_DIGIT) {
            //PrDbg("current token: %f\n", token->fDigit);
        } else {
            //PrDbg("current token: %s\n", token->cplString->str);
        }
    }

    switch (token->tag) {
    case TT_EOF:
        return SID_END;
    case TT_BREAK:
        return SID_TM_BREAK;
    case TT_DO:
        return SID_TM_DO;
    case TT_ELSE:
        return SID_TM_ELSE;
    case TT_FALSE:
        return SID_TM_FALSE;
    case TT_IF:
        return SID_TM_IF;
    case TT_TRUE:
        return SID_TM_TRUE;
    case TT_FOR:
        return SID_TM_FOR;
    case TT_WHILE:
        return SID_TM_WHILE;
    case TT_RETURN:
        return SID_TM_RETURN;
    case TT_ID:
        return SID_TM_ID;
    case TT_DIGIT:
        return SID_TM_IDIGIT;
    case TT_STR:
        return SID_TM_STR;
    case TT_LBRACE:
        return SID_TM_LBRACE;
    case TT_RBRACE:
        return SID_TM_RBRACE;
    case TT_LBRACKET:
        return SID_TM_LBRACKET;
    case TT_RBRACKET:
        return SID_TM_RBRACKET;
    case TT_LSBRACKET:
        return SID_TM_LSBRACKET;
    case TT_RSBRACKET:
        return SID_TM_RSBRACKET;
    case TT_SEMI_COLON:
        return SID_TM_SEMI_COLON;
    case TT_SHARP:
        return SID_TM_SHARP;
    case TT_DOT:
        return SID_TM_DOT;
    case TT_ARROW:
        return SID_TM_ARROW;
    case TT_ADD:
        return SID_TM_ADD;
    case TT_SUB:
        return SID_TM_SUB;
    case TT_MUL:
        return SID_TM_MUL;
    case TT_DIV:
        return SID_TM_DIV;
    case TT_MOD:
        return SID_TM_MOD;
    case TT_EQ:
        return SID_TM_EQ;
    case TT_NE:
        return SID_TM_NE;
    case TT_LT:
        return SID_TM_LT;
    case TT_LE:
        return SID_TM_LE;
    case TT_GT:
        return SID_TM_GT;
    case TT_GE:
        return SID_TM_GE;
    case TT_ASSIGN:
        return SID_TM_ASSIGN;
    case TT_AND:
        return SID_TM_LAND;
    case TT_OR:
        return SID_TM_LOR;
    case TT_NOT:
        return SID_TM_LNOT;
    case TT_BAND:
        return SID_TM_BNAD;
    case TT_BOR:
        return SID_TM_BOR;
    case TT_BNOR:
        return SID_TM_BNOR;
    case TT_SPACE:
    case TT_TAB:
    case TT_CR:
    case TT_LN:
        BisonInputPosInc(reader);
        return BisonGetCurrentSymId(reader);
    case TT_INT:
        return SID_TM_INT;
    case TT_SHORT:
        return SID_TM_SHORT;
    case TT_CHAR:
        return SID_TM_CHAR;
    case TT_BOOL:
        return SID_TM_BOOL;
    case TT_FLOAT:
        return SID_TM_FLOAT;
    case TT_SWITCH:
        return SID_TM_SWITCH;
    case TT_CASE:
        return SID_TM_CASE;
    case TT_DEFAULT:
        return SID_TM_DEFAULT;
    case TT_COLON:
        return SID_TM_COLON;
    case TT_CONTINUE:
        return SID_TM_CONTINUE;
    case TT_VOID:
        return SID_TM_VOID;
    case TT_COMMA:
        return SID_TM_COMMA;
    case TT_STRUCT:
        return SID_TM_STRUCT;
    }
    PrErr("token tag %u, no match sym id\n", token->tag);
    exit(-1);
}

/*
 * 功能：文法分析
 * 返回值：
 **/
static int GrammarParse(LrParser *lrParser, Lalr *lalr, Lex *lex)
{
    LrReader lrReader;

    lrReader.getCurrentSymId = BisonGetCurrentSymId;
    lrReader.inputPosInc = BisonInputPosInc;
    lrReader.arg = lex;

    LexScan(lex);
    return LrParse(lrParser, lalr, &lrReader);
}

/*
 * 功能：LR语法分析器移入处理函数。
 * 返回值：
 **/
static int LrShiftHandle(void **pNode, LrReader *reader)
{
    *pNode = (void *)LexCurrent(reader->arg);
    return 0;
}

/*
 * 功能：LR文法Action节点冲突处理函数。
 * 返回值：
 **/
static int BisonActionNodeConflictHandle(Lalr *lalr, ItemSet *itemSet,
                                   ActionNode *actionNode, ActionType type, int la, int id)
{
    (void)lalr, (void)actionNode, (void)type, (void)id;

    return 0;

    if (itemSet->id == 8) {
        if (actionNode->type == LAT_REDUCT
                && type == LAT_REDUCT) {
            if (actionNode->lookaheadId == SID_TM_STRUCT
                    || actionNode->lookaheadId == SID_TM_FLOAT
                    || actionNode->lookaheadId == SID_TM_INT
                    || actionNode->lookaheadId == SID_TM_CHAR
                    || actionNode->lookaheadId == SID_TM_SHORT
                    || actionNode->lookaheadId == SID_TM_BOOL
                    || actionNode->lookaheadId == SID_TM_VOID) {
                actionNode->itemSetId = id;                  ///////////////////////////
                printf("conflict has handled. [reduct %d].\n", actionNode->itemSetId);
                return 0;
            }
        }
    }
    if (itemSet->id == 3) {
        if (actionNode->type == LAT_SHIFT
                && type == LAT_REDUCT) {
            if (actionNode->lookaheadId == SID_TM_STRUCT) {
                printf("conflict has handled. [shift %d].\n", actionNode->itemId);
                return 0;
            }
        }
    }
    if (itemSet->id == 1) {
        if (actionNode->type == LAT_REDUCT
                && type == LAT_REDUCT) {
            if (la == SID_END) {
                printf("conflict has handled. [reduct %d].\n", actionNode->itemSetId);
                return 0;
            }
        }
    }
    if (itemSet->id == 173) {     /*处理if语句和if else语句的冲突，else与最近的if相结合。*/
        if (actionNode->type == LAT_SHIFT
                && type == LAT_REDUCT)
            if (la == SID_TM_ELSE) {
                printf("conflict has handled\n");
                return 0;
            }
    }
    return -ESYNTAX;
}

/*
 * 功能：构建抽象语法树。
 * 返回值：
 **/
static int BuildTree(Lex *lex)
{
    int error = 0;
    ContextFreeGrammar *grammar = NULL;
    Lalr *lalr;
    LrErrorRecover *lrErrorRecover;
    LrParser *lrParser;

    error = DesignGrammar(&grammar);
    if (error != -ENOERR)
        goto err0;
    CfgPrintProduct(grammar);

    error = LalrAlloc(&lalr, grammar, NULL, BisonActionNodeConflictHandle);
    if (error != -ENOERR)
        goto err1;
    error = GenLrParseTable(lalr);
    if (error != -ENOERR)
        goto err2;

    //LalrPrintItemSet(lalr);
    //LalrPrintParseTable(lalr);

    error = LrErrorRecoverAlloc(&lrErrorRecover, SYMID_ARR(SID_BLOCK, SID_STMTS));
    if (error != -ENOERR)
        goto err2;
    error = LrParserAlloc(&lrParser, LrShiftHandle, lrErrorRecover);
    if (error != -ENOERR)
        goto err3;
    error = GrammarParse(lrParser, lalr, lex);
    if (error != -ENOERR)
        goto err4;
    if (lrParser->syntaxErrorFlag == 1) {
        error = -ESYNTAX;
        goto err4;
    }

    LrParserFree(lrParser);
    LrErrorReocverFree(lrErrorRecover);
    LalrFree(lalr);
    ContextFreeGrammarFree(grammar);
    return error;

err4:
    LrParserFree(lrParser);
err3:
    LrErrorReocverFree(lrErrorRecover);
err2:
    LalrFree(lalr);
err1:
    ContextFreeGrammarFree(grammar);
err0:
    printf("error: %d\n", error);
    PrErr("syntax error\n");
    exit(-1);
}

Bison bisonObj, *bison = &bisonObj;

/*
 * 功能：构建抽象语法树，并生成中间代码。
 * 返回值：
 **/
void BisonExec(void)
{
    Lex *lex;

    lex = LexAlloc();
    LexSetInFile(lex, "test.txt");
    bison->lex = lex;
    bison->env = EnvAlloc();
    bison->record = QRRecordAlloc();
    BuildTree(lex);
    GenProgram(bison->program);
}


