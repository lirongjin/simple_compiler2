#ifndef __ABSTRACT_H__
#define __ABSTRACT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "lex.h"
#include "stdint.h"

typedef struct _CValue CValue;
typedef struct _LValue LValue;
typedef struct _Expr Expr;
typedef struct _Type Type;
typedef struct _BPInsList BPInsList;
typedef struct _Domain Domain;

/*访问成员的维度信息。*/
typedef struct _AEDims {
    Type *type;                 /*元素的类型信息。*/
    Expr *idx;                  /*当前维度的索引*/
    struct _AEDims *nextDims; /*如果有下级维度则不为空。*/
} AEDims;

/*左值*/
typedef struct _LValue {
    Type *type;                 /*左值声明时的类型。*/
    const Token *id;
    AEDims *dims;         /*引用信息。如果为空则为普通id左值，否则为数组引用左值*/
} LValue;

/*常量值类型*/
typedef enum {
    CVT_IDIGIT,     /*整型数字*/
    CVT_FDIGIT,     /*浮点型数字*/
    CVT_TRUE,       /*true*/
    CVT_FALSE,      /*false*/
} CValueType;

/*常量值*/
typedef struct _CValue {
    CValueType type;        /*常量值类型*/
    const Token *token;     /*token, 可以获取整型的数字，或者浮点型的数字。*/
} CValue;

/*特殊常量值类型*/
typedef enum {
    CVTS_IDIGIT,     /*整型数字*/
    CVTS_FDIGIT,     /*浮点型数字*/
    CVTS_TRUE,       /*true*/
    CVTS_FALSE,      /*false*/
} CValueSpcType;

typedef struct {
    CValueSpcType type;
    union {
        int iValue;
        float fValue;
    };
} CValueSpc;

typedef struct {
    Type *type;
    const Token *token;
} Id;

typedef struct {
    Expr *base;
    Type *type;           /*最后一维度访问成员的类型*/
    AEDims *dims;         /*引用信息。*/
} AccessElm;

/*二目运算符表达式*/
typedef struct {
    Type *type;     /*表达式类型，int, float...*/
    Expr *lExpr;    /*左边的表达式*/
    Expr *rExpr;    /*右边的表达式*/
} BinOp;

/*单目运算符*/
typedef struct {
    Type *type;     /*表达式类型，int, float...*/
    Expr *expr;     /*子表达式*/
} UnaryOp;

/*临时量*/
typedef struct {
    Type *type;     /*临时量类型，int, float...*/
    size_t num;     /*临时量编号*/
    size_t offset;
    uint32_t offsetFlag: 1;
} Temp;

typedef enum {
    AMTYPE_DIRECT,
    AMTYPE_INDIRECT,
} AccessMbrType;

typedef struct {
    AccessMbrType type; /*访问方式*/
    Expr *base;         /*基表达式*/
    const Token *memId; /*成员id*/
    Type *memtype;      /*成员的类型*/
    Expr *memOffset;   /*成员偏移量*/
} AccessMbr;

/*函数调用*/
typedef struct _FunRef FunRef;

/*表达式的类型*/
typedef enum {
    ET_ID,          /*id表达式*/
    ET_ACCESS_ELM,  /*访问元素*/
    ET_ASSIGN,      /*赋值表达式*/
    ET_LOR,         /*逻辑或表达式*/
    ET_LAND,        /*逻辑与表达式*/
    ET_EQ,          /*等于表达式*/
    ET_NE,          /*不等于表达式*/
    ET_GT,          /*大于表达式*/
    ET_GE,          /*大于等于表达式*/
    ET_LT,          /*小于表达式*/
    ET_LE,          /*小于等于表达式*/
    ET_ADD,         /*加法表达式*/
    ET_SUB,         /*减法表达式*/
    ET_MUL,         /*乘法表达式*/
    ET_DIV,         /*除法表达式*/
    ET_MOD,         /*取余表达式*/
    ET_LNOT,        /*逻辑非表达式*/
    ET_CONST,       /*常量表达式*/
    ET_TEMP,        /*临时表达式*/
    ET_FUN_REF,     /*函数调用*/
    ET_CONST_SPC,   /*中间代码生产的特殊常量值*/
    ET_ACCESS_MBR,  /*成员访问*/
    ET_REF_POINTER, /*解引用指针*/
    ET_GET_ADDR,    /*取址运算*/
    ET_TYPE_CAST,   /*类型转换*/
} ExprType;

/*表达式*/
typedef struct _Expr {
    ExprType type;      /*表达式的类型*/
    Domain *domain;     /*表达式所在的作用域*/
    union {
        Id id;          /*id表达式*/
        AccessElm accessElm;   /*访问元素*/
        BinOp binOp;            /*双目运算表达式*/
        UnaryOp unaryOp;        /*单目运算表达式*/
        CValue cValue;          /*常量值*/
        Temp temp;              /*临时值*/
        FunRef *funRef;         /*函数调用*/
        CValueSpc cValueSpc;    /*特殊常量值*/
        AccessMbr accessMbr;    /*访问成员*/
    };
} Expr;

/*条件表达式*/
typedef Expr Cond;
typedef struct _Stmt Stmt;

/*表达式语句*/
typedef struct {
    Expr *expr;     /*表达式*/
} ExprStmt;

/*if语句*/
typedef struct {
    Cond *cond;     /*条件表达式*/
    Stmt *stmt;     /*语句*/
} IfStmt;

/*if else语句*/
typedef struct {
    Cond *cond;     /*条件表达式*/
    Stmt *ifStmt;   /*if语句*/
    Stmt *elseStmt; /*else语句*/
} ElseStmt;

/*while语句*/
typedef struct {
    Cond *cond;     /*条件表达式*/
    Stmt *stmt;     /*语句*/
    BPInsList *breakList;       /*待回填的break指令链表*/
    BPInsList *continueList;    /*待回填的continue指令链表*/
} WhileStmt;

/*do while语句*/
typedef struct {
    Cond *cond;     /*条件表达式*/
    Stmt *stmt;     /*语句*/
    BPInsList *breakList;       /*待回填的break指令链表*/
    BPInsList *continueList;    /*待回填的continue指令链表*/
} DoStmt;

/*语句序列*/
typedef struct _Stmts {
    struct _Stmts *prevStmts;   /*前一个语句序列，如果为空则当前语句为第一个语句序列*/
    Stmt *stmt;                 /*语句*/
} Stmts;

/*块语句*/
typedef struct _BlockStmt {
    Stmts *stmts;           /*语句序列*/
} BlockStmt;

/*break语句*/
typedef struct {
    size_t lastLevelAfter;   /*最近的包含break的循环或者switch语句的下一块语句的开始表号。*/
} BreakStmt;

/*continue语句*/
typedef struct {
    size_t lastLevelBegin;   /*最近的包含continue的循环语句的开始表号。*/
} ContinueStmt;

/*case e: stmts */
typedef struct {
    Expr *expr;
    Stmts *stmts;
} CaseExpr;

/*case语句类型*/
typedef enum {
    CST_CASE_EXPR,      /*case e: stmts*/
    CST_CASE_DEFAULT,   /*default: stmts*/
} CaseStmtType;

/*switch内部的语句类型*/
typedef struct {
    CaseStmtType type;
    union {
        CaseExpr caseExpr;
        Stmts *stmts;
    };
} CaseStmt;

/* case stmts 语句*/
typedef struct _CaseStmts {
    struct _CaseStmts *prev;    /*前一个case stmts语句序列*/
    CaseStmt *caseStmt;
} CaseStmts;

typedef struct {
    Expr *expr;
    CaseStmts *caseStmts;
    BPInsList *breakList;       /*待回填的break指令链表*/
} SwitchStmt;

typedef struct {
    Expr *expr;
} ReturnStmt;

/*语句的类型*/
typedef enum {
    ST_EXPR,            /*表达式语句*/
    ST_IF,              /*if语句*/
    ST_ELSE,            /*if else 语句*/
    ST_WHILE,           /*while语句*/
    ST_DO,              /*do while语句*/
    ST_BLOCK,           /*块语句*/
    ST_BREAK,           /*break语句*/
    ST_CONTINUE,        /*continue语句*/
    ST_SWITCH,          /*switch*/
    ST_RETURN_VOID,     /*return;*/
    ST_RETURN_VALUE,    /*return E;*/
} StmtType;

/*语句*/
typedef struct _Stmt {
    StmtType type;      /*语句类型*/
    Domain *domain;
    union {
        ExprStmt exprStmt;         /*表达式语句*/
        IfStmt ifStmt;             /*if语句*/
        ElseStmt elseStmt;         /*if else 语句*/
        WhileStmt whileStmt;       /*while语句*/
        DoStmt doStmt;             /*do while语句*/
        BlockStmt blockStmt;       /*块语句*/
        BreakStmt breakStmt;       /*break语句*/
        ContinueStmt continueStmt; /*continue语句*/
        SwitchStmt switchStmt;     /*switch语句*/
        ReturnStmt returnStmt;     /*return语句*/
    };
} Stmt;

typedef struct _FunDefines FunDefines;

/*程序*/
typedef struct _Program {
    FunDefines *funDefines;
} Program;

/*类型的类型*/
typedef enum {
    TT_BASE,        /*基本类型*/
    TT_ARRAY,       /*数组类型*/
    TT_STRUCTURE,   /*结构体类型*/
    TT_POINTER,     /*指针类型*/
    TT_FUNCTION,    /*函数类型*/
} TypeType;

/*基本类型的类型*/
typedef enum {
    BTT_BOOL,   /*bool类型*/
    BTT_CHAR,   /*char类型*/
    BTT_SHORT,  /*short类型*/
    BTT_INT,    /*int类型*/
    BTT_FLOAT,  /*float类型*/
} BaseTypeType;

/*基本类型*/
typedef struct {
    BaseTypeType type;      /*种类*/
} BaseType;

typedef struct _Type Type;
/*数组类型*/
typedef struct {
    size_t count;           /*元素数量*/
    Type *type;             /*元素类型*/
} Array;

typedef enum _AlignType {
    AT_1BYTE,
    AT_2BYTE,
    AT_4BYTE,
    AT_8BYTE,
} AlignType;

typedef struct {
    Domain *domain;
} Struct;

typedef struct {
    Type *type;
} PointerType;

/*函数返回值类型类型*/
typedef enum {
    ERTT_VOID,      /*空类型*/
    ERTT_BASE_TYPE, /*基本类型*/
} FunRetTypeType;

/*函数返回值类型*/
typedef struct {
    FunRetTypeType typeType;
    Type *type;
} FunRetType;

/*函数形式参数*/
typedef struct {
    Type *type; /*函数形式参数类型*/
    const Token *id; /*id*/
} FunArg;

/*函数的参数，可级联*/
typedef struct _FunArgs {
    FunArg *funArg;          /*函数形式参数*/
    struct _FunArgs *next;  /*下一个函数形式参数，如果为空，则表示是最后一级函数形式参数。*/
} FunArgs;

/*函数类型*/
typedef struct _FunType {
    FunRetType *retType;    /*返回值类型*/
    FunArgs *funArgs;       /*形式参数类型，如果为NULL则表示空类型*/
    size_t addr;
    uint32_t vargFlag: 1;
} FunType;

/*类型*/
typedef struct _Type {
    TypeType type;              /*类型的类型*/
    AlignType alignType;
    size_t size;            /*类型尺寸，单位：字节（byte）*/
    union {
        BaseType baseType;      /*基本类型*/
        Array array;            /*数组*/
        Struct st;              /*结构体*/
        PointerType pointer;    /*指针*/
        FunType funType;        /*函数类型*/
    };
} Type;

/*函数定义*/
typedef struct {
    const Token *id;
    FunType *funType;   /*函数类型*/
    Stmt *blockStmt;    /*函数块语句*/
} FunDefine;

typedef struct _FunDefines {
    struct _FunDefines *prev;
    FunDefine *funDefine;
} FunDefines;

/*函数实际参数*/
typedef struct _FunRefParas {
    Expr *expr;     /*参数表达式*/
    struct _FunRefParas *next;  /*下一个实际参数，如果为空则表示为最后一级实际参数。*/
} FunRefParas;

/*函数调用*/
typedef struct _FunRef {
    const Token *token;     /*标识符*/
    FunType *funType;       /*函数类型*/
    FunRefParas *paras; /*函数实际参数。如果为空则表示不存在实际参数*/
} FunRef;

AEDims *ATreeNewAEDims(Type *type, Expr *idx,
                              AEDims *nextDims);
LValue *ATreeNewLValue(Type *type, const Token *id, AEDims *dims);

Expr *ATreeNewIdExpr(Domain *domain, const Token *token, Type *type);
Expr *ATreeNewAccessElmExpr(Domain *domain, Expr *baseExpr, AEDims *dims, Type *type);
Expr *ATreeNewBinExpr(Domain *domain, ExprType exprtype, Type *type, Expr *lExpr, Expr *rExpr);
Expr *ATreeNewUnaryExpr(Domain *domain, ExprType exprType, Type *type, Expr *expr);
Expr *ATreeNewCValueExpr(Domain *domain, CValueType type, const Token *token);
Expr *ATreeNewCValueSpcIntExpr(Domain *domain, int iValue);
Expr *ATreeNewCValueSpcFloatExpr(Domain *domain, float fValue);
Expr *ATreeNewCValueSpcBoolExpr(Domain *domain, CValueSpcType type);
Expr *ATreeNewTemp(Domain *domain, Type *type);
Expr *ATreeNewFunRefExpr(Domain *domain, FunRef *funRef);
Expr *ATreeNewAccessMem(Domain *Domain, AccessMbrType type, Expr *base, const Token *memId, Type *memType, Expr *memOffset);
Type *ATreeExprType(Expr *expr);

Stmt *ATreeNewExprStmt(Domain *domain, Expr *expr);
Stmt *ATreeNewIfStmt(Domain *domain, Cond *cond, Stmt *stmt);
Stmt *ATreeNewElseStmt(Domain *domain, Cond *cond, Stmt *ifStmt, Stmt *elseStmt);
Stmt *ATreeNewWhileStmt(Domain *domain, Cond *cond, Stmt *stmt);
Stmt *ATreeNewDoStmt(Domain *domain, Cond *cond, Stmt *stmt);
Stmt *ATreeNewBlockStmt(Domain *domain, Stmts *stmts);
Stmt *ATreeNewBreakStmt(Domain *domain, size_t after);
Stmt *ATreeNewContinueStmt(Domain *domain, size_t begin);
Stmt *ATreeNewReturnVoidStmt(Domain *domain);
Stmt *ATreeNewReturnValueStmt(Domain *domain, Expr *expr);

CaseStmt *ATreeNewCaseExprStmt(Expr *expr, Stmts *stmts);
CaseStmt *ATreeNewCaseDefaultStmt(Stmts *stmts);
CaseStmts *ATreeNewCaseStmts(CaseStmts *prev, CaseStmt *caseStmt);
Stmt *ATreeNewSwitchStmt(Expr *expr, CaseStmts *caseStmts);

Stmts *ATreeNewStmts(Stmts *prevStmts, Stmt *stmt);
Type *ATreeExprWiddenType(ExprType exprType, Expr *lExpr, Expr *rExpr);
Program *ATreeNewProgram(FunDefines *funDefines);

Type *ATreeNewBaseType(BaseTypeType type, size_t size, AlignType alignType);
void ATreeFreeBaseType(Type *type);
size_t ATreeTypeSize(const Type *type);
Type *ATreeNewArrayType(Type *elementType, size_t count, AlignType alignType);
Type *ATreeNewStructType(Domain *domain, AlignType alignType, size_t size);
Type *ATreeNewPointerType(Type *pointType);
Type *ATreeNewFunctionType(FunRetType *retType, FunArgs *funArgs);

FunRetType *ATreeNewFunRetTypeVoid(void);
FunRetType *ATreeNewFunRetType(Type *type);
FunArg *ATreeNewFunArg(const Token *id, Type *type);
FunArgs *ATreeNewFunArgs(FunArg *funArg, FunArgs *next);
FunArgs *ATreeFunArgsReverse(FunArgs *funArgs);
FunDefine *ATreeNewFunDefine(const Token *id, FunType *funType, Stmt *blockStmt);
FunDefines *ATreeNewFunDefines(FunDefines *prev, FunDefine *funDefine);
FunDefines *ATreeNewFunDefinesByFunDefines(FunDefines *prev, FunDefines *funDefines);

FunRefParas *ATreeNewFunRefParas(Expr *expr, FunRefParas *next);
FunRef *ATreeNewFunRef(const Token *token, FunType *funType, FunRefParas *paras);

CplString *ATreeExprString(Expr *expr);

Type *ATreeArrayBaseType(const Array *array);
size_t ATreeAddrAlignByType(size_t addr, const Type *type);

AlignType ATreeTypeGetAlignType(const Type *type);

#ifdef __cplusplus
}
#endif

#endif /*__ABSTRACT_H__*/
