#ifndef __GRAMMAR_SYMBOL_H__
#define __GRAMMAR_SYMBOL_H__

#ifdef __cplusplus
extern "C" {
#endif

#define SID_START         SID_STARTx   /*上下文无关文法第一个符号。*/

typedef enum _SymbolId {
    SID_EMPTY           = 0,    /*empty string*/
    SID_END             ,       /*'$'*/
    SID_STARTx          ,       /*增广文法开始符号*/

    /*非终结符号*/
    SID_PROGRAM         =  10,  //program
    SID_BLOCK           ,       //block
    SID_DECL            ,       //decl
    SID_DECLS           ,       //decls
    SID_STMTS           ,       //stmts
    SID_TYPE            ,       //type
    SID_BASE_TYPE       ,       //base_type
    SID_ARR_DECL_DIM    ,       //arr_decl_dim [d]
    SID_STMT            ,       //stmt
    SID_EXPR            ,       //expr
    SID_COND            ,       //cond
    SID_ASSIGN_EXPR     ,       //assign_expr
    SID_LVALUE          ,       //lvalue
    SID_LOR_EXPR        ,       //lor_expr
    SID_LAND_EXPR       ,       //land_expr
    SID_REL_L0_EXPR     ,       //rel_l0_expr
    SID_ARITH_L0_EXPR   ,       //arith_l0_expr
    SID_REL_L1_EXPR     ,       //rel_l1_expr
    SID_ARITH_L1_EXPR   ,       //arith_l1_expr
    SID_LEVEL2_EXPR,            //第二优先级表达式
    SID_LEVEL1_EXPR,            //第一优先级表达式
    SID_FACTOR          ,       //factor
    SID_ARR_DEREF_DIMx   ,       //arr_deref_dim [d]
    SID_ARR_DEREF_DIM,          //arr_deref_dim [d]
    SID_ARR_DEREF_DIMS,         //arr_deref_dim [d]
    SID_ARR_IDX         ,       //arr_idx
    SID_CASE_STMT       ,       //case E: stmts
    SID_CASE_STMTS      ,       //case_stmts
    SID_FUN_DEFINE      ,       //函数定义
    SID_FUN_RET_TYPE    ,       //函数返回值类型
    SID_FUN_ARGUES    ,         //函数形式参数
    SID_FUN_ARGS        ,       //多个函数形式参数
    SID_FUN_ARG         ,       //函数形式参数
    SID_FUN_BLOCK       ,       //函数块
    SID_FUN_PARAMETERS  ,       //函数实际参数尾部
    SID_FUN_PARAS       ,       //函数实际参数
    SID_STRUCT_TYPE     ,       //结构体类型
    SID_STRUCT_DEFINE_HEAD,     //结构体定义头，struct id {
    SID_STRUCT_DEFINE,          //结构体定义
    SID_STRUCT_FUN_DEFINE,      //结构体/函数定义混合定义
    SID_COMPONENT_DEFINES,      //程序各个组成元素定义
    SID_ACCESS_MEMBER,             //成员访问, b.c, b->c
    SID_TYPE_CAST,              //类型转换 (type)
    SID_REF_POINTER,            //解引用指针，*p
    SID_ACCESS_ELM,             //访问元素
    SID_ID_EXPR,               //ID表达式
    SID_FUN_CALL,               //函数调用

    /*终结符号*/
    SID_TM_ID           = 200,  //id
    SID_TM_IDIGIT       ,       //int digit
    SID_TM_FDIGIT       ,       //float digit

    SID_TM_STR          ,       //str

    SID_TM_CONTINUE     ,       //continue
    SID_TM_BREAK        ,       //break
    SID_TM_ELSE         ,       //else
    SID_TM_WHILE        ,       //while
    SID_TM_DO           ,       //do
    SID_TM_IF           ,       //if
    SID_TM_INT          ,       //int
    SID_TM_CHAR         ,       //char
    SID_TM_SHORT        ,       //short
    SID_TM_BOOL         ,       //bool
    SID_TM_FLOAT        ,       //float
    SID_TM_TRUE         ,       //true
    SID_TM_FALSE        ,       //false
    SID_TM_FOR          ,       //for
    SID_TM_RETURN       ,       //return
    SID_TM_SWITCH       ,       //switch
    SID_TM_CASE         ,       //case
    SID_TM_DEFAULT      ,       //default
    SID_TM_VOID         ,       //void
    SID_TM_STRUCT       ,       //struct

    SID_TM_LBRACE       = 250,  //{
    SID_TM_RBRACE       ,       //}
    SID_TM_LBRACKET     ,       //(
    SID_TM_RBRACKET     ,       //)
    SID_TM_LSBRACKET    ,       //[
    SID_TM_RSBRACKET    ,       //]

    SID_TM_SEMI_COLON   = 270,  //;
    SID_TM_COLON        ,       //:

    SID_TM_LNOT         = 290,  //'!'
    SID_TM_MUL          ,       //*
    SID_TM_DIV          ,       //'/'
    SID_TM_MOD          ,       //%
    SID_TM_ADD          ,       //+
    SID_TM_SUB          ,       //-
    SID_TM_LT           ,       //<
    SID_TM_LE           ,       //<=
    SID_TM_GT           ,       //>
    SID_TM_GE           ,       //>=
    SID_TM_EQ           ,       //==
    SID_TM_NE           ,       //'!='
    SID_TM_LAND         ,       //&&
    SID_TM_LOR          ,       //||
    SID_TM_ASSIGN       ,       //=
    SID_TM_COMMA        ,       //,

    SID_TM_SHARP        = 305,  //#
    SID_TM_DOT          ,       //.
    SID_TM_ARROW        ,       //->
    SID_TM_BNAD         ,       //&
    SID_TM_BOR          ,       //|
    SID_TM_BNOR         ,       //^
    SID_TM_SPACE        ,       //' '
    SID_TM_TAB          ,       //'\t'
    SID_TM_CR           ,       //'\r'
    SID_TM_LN           ,       //'\n'

    /*哑非终结符*/
    SID_M1              = 1001, //{ M1 decls stmts M2 }
    SID_M2              ,       //{ M1 decls stmts M2 }

    SID_M3              ,       //type -> base_type M3 dim
    SID_M4              ,       //dim -> [ d ] M4 dim1

    SID_M5              ,       //lvalue -> id M5 arr_deref_dim
    SID_M6              ,       //arr_deref_dim -> [ arr_idx ] M6 arr_deref_dim

    SID_M7              ,       /*fun_define -> M7 fun_ret_type id ( fun_argues ) fun_block M8*/
    SID_M8              ,       /*fun_define -> M7 fun_ret_type id ( fun_argues ) fun_block M8*/

    SID_M9              ,       /*struct_define -> struct id M9 { decls } M10;*/
    SID_M10             ,       /*struct_define -> struct id M9 { decls } M10;*/
} SymbolId;

#ifdef __cplusplus
}
#endif

#endif /*__GRAMMAR_SYMBOL_H__*/

