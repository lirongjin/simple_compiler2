#include "abstract_tree.h"
#include "lex.h"
#include "list.h"
#include "stdint.h"
#include "cpl_mm.h"
#include "cpl_errno.h"
#include "cpl_debug.h"
#include "stdlib.h"
#include "bison.h"
#include "environ.h"
#include "backpatch.h"
#include "cpl_common.h"

#define PrErr(...)      Pr(__FILE__, __LINE__, __FUNCTION__, "error", __VA_ARGS__)

#define AT_DEBUG
#ifdef AT_DEBUG
#define PrDbg(...)      Pr(__FILE__, __LINE__, __FUNCTION__, "debug", __VA_ARGS__)
#endif

/*
 * 功能：分配内存，如果失败则程序直接退出。
 * 返回值：
 **/
static void *ATreeMAlloc(size_t size)
{
    void *p;

    p = CplAlloc(size);
    if (!p) {
        PrErr("no memory\n");
        exit(-1);
    }
    return p;
}

void ATreeFreeType(Type *type);

/*
 * 功能：创建数组解引用时的维度数据结构。
 * 返回值：
 **/
AEDims *ATreeNewAEDims(Type *type, Expr *idx, AEDims *nextDims)
{
    AEDims *dims;

    dims = ATreeMAlloc(sizeof (*dims));
    dims->type = type;
    dims->idx = idx;
    dims->nextDims = nextDims;
    return dims;
}

/*
 * 功能：释放数组解引用时的维度数据结构。
 * 返回值：
 **/
void ATreeFreeAEDims(AEDims *dims)
{
    if (!dims)
        return;
    ATreeFreeAEDims(dims->nextDims);
    ATreeFreeType(dims->type);
    CplFree(dims);
}

/*
 * 功能：创建左值。
 * 返回值：
 **/
LValue *ATreeNewLValue(Type *type, const Token *id, AEDims *dims)
{
    LValue *lValue;

    lValue = ATreeMAlloc(sizeof (*lValue));
    lValue->type = type;
    lValue->id = id;
    lValue->dims = dims;
    return lValue;
}

/*
 * 功能：释放左值。
 * 返回值：
 **/
void ATreeFreeLValue(LValue *lValue)
{
    if (!lValue)
        return;
    ATreeFreeType(lValue->type);
    ATreeFreeAEDims(lValue->dims);
    CplFree(lValue);
}

/*
 * 功能：分配表达式
 * 返回值：
 **/
static Expr *ATreeAllocExpr(ExprType type)
{
    Expr *expr;

    expr = ATreeMAlloc(sizeof (*expr));
    expr->type = type;
    return expr;
}

/*
 * 功能：新建id表达式
 * 返回值：
 */
Expr *ATreeNewIdExpr(Domain *domain, const Token *token, Type *type)
{
    Expr *newExpr;

    newExpr = ATreeAllocExpr(ET_ID);
    newExpr->domain = domain;
    newExpr->id.token = token;
    newExpr->id.type = type;
    return newExpr;
}

/*
 * 功能：新建访问元素表达式
 * 返回值：
 */
Expr *ATreeNewAccessElmExpr(Domain *domain, Expr *baseExpr, AEDims *dims, Type *type)
{
    Expr *newExpr;

    newExpr = ATreeAllocExpr(ET_ACCESS_ELM);
    newExpr->domain = domain;
    newExpr->accessElm.base = baseExpr;
    newExpr->accessElm.dims = dims;
    newExpr->accessElm.type = type;
    return newExpr;
}

/*
 * 功能：新建二目运算符表达式。
 * 返回值：
 **/
Expr *ATreeNewBinExpr(Domain *domain, ExprType exprtype, Type *type, Expr *lExpr, Expr *rExpr)
{
    Expr *newExpr;

    newExpr = ATreeAllocExpr(exprtype);
    newExpr->domain = domain;
    newExpr->binOp.type = type;
    newExpr->binOp.lExpr = lExpr;
    newExpr->binOp.rExpr = rExpr;
    return newExpr;
}

void ATreeFreeExpr(Expr *expr);

/*
 * 功能：释放二目运算符表达式。
 * 返回值：
 **/
void ATreeFreeBinExpr(Expr *expr)
{
    if (!expr)
        return;
    ATreeFreeType(expr->binOp.type);
    ATreeFreeExpr(expr->binOp.lExpr);
    ATreeFreeExpr(expr->binOp.rExpr);
    CplFree(expr);
}

/*
 * 功能：新建单目运算符表达式
 * 返回值：
 **/
Expr *ATreeNewUnaryExpr(Domain *domain, ExprType exprType, Type *type, Expr *expr)
{
    Expr *newExpr;

    newExpr = ATreeAllocExpr(exprType);
    newExpr->domain = domain;
    newExpr->unaryOp.type = type;
    newExpr->unaryOp.expr = expr;
    return newExpr;
}

/*
 * 功能：释放单目运算符表达式
 * 返回值：
 **/
void ATreeFreeUnaryExpr(Expr *expr)
{
    if (!expr)
        return;
    ATreeFreeType(expr->unaryOp.type);
    ATreeFreeExpr(expr->unaryOp.expr);
    CplFree(expr);
}

/*
 * 功能：新建常量值表达式
 * 返回值：
 **/
Expr *ATreeNewCValueExpr(Domain *domain, CValueType type, const Token *token)
{
    Expr *newExpr;

    newExpr = ATreeAllocExpr(ET_CONST);
    newExpr->domain = domain;
    newExpr->cValue.type = type;
    newExpr->cValue.token = token;
    return newExpr;
}

/*
 * 功能：新建常量值表达式
 * 返回值：
 **/
Expr *ATreeNewCValueSpcIntExpr(Domain *domain, int iValue)
{
    Expr *newExpr;

    newExpr = ATreeAllocExpr(ET_CONST_SPC);
    newExpr->domain = domain;
    newExpr->cValueSpc.type = CVTS_IDIGIT;
    newExpr->cValueSpc.iValue = iValue;
    return newExpr;
}

/*
 * 功能：新建常量值表达式
 * 返回值：
 **/
Expr *ATreeNewCValueSpcFloatExpr(Domain *domain, float fValue)
{
    Expr *newExpr;

    newExpr = ATreeAllocExpr(ET_CONST_SPC);
    newExpr->domain = domain;
    newExpr->cValueSpc.type = CVTS_FDIGIT;
    newExpr->cValueSpc.fValue = fValue;
    return newExpr;
}

/*
 * 功能：新建常量值表达式
 * 返回值：
 **/
Expr *ATreeNewCValueSpcBoolExpr(Domain *domain, CValueSpcType type)
{
    Expr *newExpr;

    newExpr = ATreeAllocExpr(ET_CONST_SPC);
    newExpr->domain = domain;
    newExpr->cValueSpc.type = type;
    return newExpr;
}

/*
 * 功能：释放常量值表达式
 * 返回值：
 **/
void ATreeFreeCValueExpr(Expr *expr)
{
    CplFree(expr);
}

/*
 * 功能：新建临时量表达式
 * 返回值：
 **/
Expr *ATreeNewTemp(Domain *domain, Type *type)
{
    Expr *expr;
    static size_t num = 1;

    expr = ATreeMAlloc(sizeof (*expr));
    expr->type = ET_TEMP;
    expr->domain = domain;
    expr->temp.type = type;
    expr->temp.num = num++;
    expr->temp.offsetFlag = 0;
    expr->temp.offset = 0;
    return expr;
}

Expr *ATreeNewAccessMem(Domain *domain, AccessMbrType type, Expr *base, const Token *memId, Type *memType, Expr *offset)
{
    Expr *expr;
    expr = ATreeMAlloc(sizeof (*expr));
    expr->domain = domain;
    expr->type = ET_ACCESS_MBR;
    expr->accessMbr.type = type;
    expr->accessMbr.base = base;
    expr->accessMbr.memId = memId;
    expr->accessMbr.memtype = memType;
    expr->accessMbr.memOffset = offset;
    return expr;
}

Expr *ATreeNewFunRefExpr(Domain *domain, FunRef *funRef)
{
    Expr *expr;

    expr = ATreeMAlloc(sizeof (*expr));
    expr->domain = domain;
    expr->type = ET_FUN_REF;
    expr->funRef = funRef;
    return expr;
}

/*
 * 功能：释放临时量表达式
 * 返回值：
 **/
void ATreeFreeTemp(Expr *expr)
{
    if (!expr)
        return;
    ATreeFreeType(expr->temp.type);
    CplFree(expr);
}

/*
 * 功能：释放表达式
 * 返回值：
 **/
void ATreeFreeExpr(Expr *expr)
{
    switch (expr->type) {
    case ET_ASSIGN:
    case ET_LOR:
    case ET_LAND:
    case ET_EQ:
    case ET_NE:
    case ET_GT:
    case ET_GE:
    case ET_LT:
    case ET_LE:
    case ET_ADD:
    case ET_SUB:
    case ET_MUL:
    case ET_DIV:
    case ET_MOD:
        ATreeFreeBinExpr(expr);
        break;
    case ET_LNOT:
        ATreeFreeUnaryExpr(expr);
        break;
    case ET_CONST:
        ATreeFreeCValueExpr(expr);
        break;
    case ET_TEMP:
        ATreeFreeTemp(expr);
        break;
    case ET_FUN_REF:

        break;
    }
}

/*
 * 功能：判断表达式类型。
 * 返回值：
 **/
Type *ATreeExprType(Expr *expr)
{
    switch (expr->type) {
    case ET_ASSIGN:
    case ET_LOR:
    case ET_LAND:
    case ET_EQ:
    case ET_NE:
    case ET_GT:
    case ET_GE:
    case ET_LT:
    case ET_LE:
    case ET_ADD:
    case ET_SUB:
    case ET_MUL:
    case ET_DIV:
    case ET_MOD:
        return expr->binOp.type;
    case ET_LNOT:
        return expr->unaryOp.type;
    case ET_CONST:
        if (expr->cValue.type == CVT_IDIGIT) {
            return bison->env->intType;
        } else if (expr->cValue.type == CVT_FDIGIT) {
            return bison->env->floatType;
        } else if (expr->cValue.type == CVT_TRUE) {
            return bison->env->boolType;
        } else if (expr->cValue.type == CVT_FALSE) {
            return bison->env->boolType;
        } else {
            PrErr("unknown type");
            exit(-1);
        }
    case ET_TEMP:
        return expr->temp.type;
    case ET_FUN_REF:
        if (expr->funRef->funType->retType->typeType == ERTT_BASE_TYPE) {
            return expr->funRef->funType->retType->type;
        } else {
            return NULL;
        }
    case ET_CONST_SPC:
        if (expr->cValueSpc.type == CVTS_IDIGIT) {
            return bison->env->intType;
        } else if (expr->cValueSpc.type == CVTS_FDIGIT) {
            return bison->env->floatType;
        } else if (expr->cValueSpc.type == CVTS_TRUE) {
            return bison->env->boolType;
        } else if (expr->cValueSpc.type == CVTS_FALSE) {
            return bison->env->boolType;
        } else {
            PrErr("unknown type");
            exit(-1);
        }
    case ET_ACCESS_MBR:
        return expr->accessMbr.memtype;
    case ET_REF_POINTER:
        return expr->unaryOp.type;
    case ET_GET_ADDR:
        return expr->unaryOp.type;
    case ET_TYPE_CAST:
        return expr->unaryOp.type;
    case ET_ID:
        return expr->id.type;
    case ET_ACCESS_ELM:
        return expr->accessElm.type;
    }
    PrErr("expression type error");
    exit(-1);
}

/*
 * 功能：分配语句
 * 返回值：
 **/
static inline Stmt *ATreeAllocStmt(StmtType type)
{
    Stmt *stmt;

    stmt = ATreeMAlloc(sizeof (*stmt));
    stmt->type = type;
    return stmt;
}

/*
 * 功能：新建表达式语句。
 * 返回值：
 **/
Stmt *ATreeNewExprStmt(Domain *domain, Expr *expr)
{
    Stmt *stmt;

    stmt = ATreeAllocStmt(ST_EXPR);
    stmt->domain = domain;
    stmt->exprStmt.expr = expr;
    return stmt;
}

/*
 * 功能：新建if语句
 * 返回值：
 **/
Stmt *ATreeNewIfStmt(Domain *domain, Cond *cond, Stmt *stmt)
{
    Stmt *newStmt;

    newStmt = ATreeAllocStmt(ST_IF);
    newStmt->domain = domain;
    newStmt->ifStmt.cond = cond;
    newStmt->ifStmt.stmt = stmt;
    return newStmt;
}

/*
 * 功能：新建if else语句
 * 返回值：
 **/
Stmt *ATreeNewElseStmt(Domain *domain, Cond *cond, Stmt *ifStmt, Stmt *elseStmt)
{
    Stmt *newStmt;

    newStmt = ATreeAllocStmt(ST_ELSE);
    newStmt->domain = domain;
    newStmt->elseStmt.cond = cond;
    newStmt->elseStmt.ifStmt = ifStmt;
    newStmt->elseStmt.elseStmt = elseStmt;
    return newStmt;
}

/*
 * 功能：新建while语句
 * 返回值：
 **/
Stmt *ATreeNewWhileStmt(Domain *domain, Cond *cond, Stmt *stmt)
{
    Stmt *newStmt;

    newStmt = ATreeAllocStmt(ST_WHILE);
    newStmt->domain = domain;
    newStmt->whileStmt.cond = cond;
    newStmt->whileStmt.stmt = stmt;
    newStmt->whileStmt.breakList = BPAllocInsList();
    newStmt->whileStmt.continueList = BPAllocInsList();
    return newStmt;
}

/*
 * 功能：新建do while 语句
 * 返回值：
 **/
Stmt *ATreeNewDoStmt(Domain *domain, Cond *cond, Stmt *stmt)
{
    Stmt *newStmt;

    newStmt = ATreeAllocStmt(ST_DO);
    newStmt->domain = domain;
    newStmt->doStmt.cond = cond;
    newStmt->doStmt.stmt = stmt;
    newStmt->doStmt.breakList = BPAllocInsList();
    newStmt->doStmt.continueList = BPAllocInsList();
    return newStmt;
}

/*
 * 功能：新建块语句
 * 返回值：
 **/
Stmt *ATreeNewBlockStmt(Domain *domain, Stmts *stmts)
{
    Stmt *newStmt;

    newStmt = ATreeAllocStmt(ST_BLOCK);
    newStmt->domain = domain;
    newStmt->blockStmt.stmts = stmts;
    return newStmt;
}

/*
 * 功能：新建break语句
 * 返回值：
 **/
Stmt *ATreeNewBreakStmt(Domain *domain, size_t after)
{
    Stmt *newStmt;

    newStmt = ATreeAllocStmt(ST_BREAK);
    newStmt->domain = domain;
    newStmt->breakStmt.lastLevelAfter = after;
    return newStmt;
}

/*
 * 功能：新建continue语句
 * 返回值：
 **/
Stmt *ATreeNewContinueStmt(Domain *domain, size_t begin)
{
    Stmt *newStmt;

    newStmt = ATreeAllocStmt(ST_CONTINUE);
    newStmt->domain = domain;
    newStmt->continueStmt.lastLevelBegin = begin;
    return newStmt;
}

Stmt *ATreeNewReturnVoidStmt(Domain *domain)
{
    Stmt *newStmt;

    newStmt = ATreeAllocStmt(ST_RETURN_VOID);
    newStmt->domain = domain;
    return newStmt;
}

Stmt *ATreeNewReturnValueStmt(Domain *domain, Expr *expr)
{
    Stmt *newStmt;

    newStmt = ATreeAllocStmt(ST_RETURN_VALUE);
    newStmt->domain = domain;
    newStmt->returnStmt.expr = expr;
    return newStmt;
}

CaseStmt *ATreeNewCaseExprStmt(Expr *expr, Stmts *stmts)
{
    CaseStmt *caseStmt;

    caseStmt = ATreeMAlloc(sizeof (*caseStmt));
    caseStmt->type = CST_CASE_EXPR;
    caseStmt->caseExpr.expr = expr;
    caseStmt->caseExpr.stmts = stmts;
    return caseStmt;
}

CaseStmt *ATreeNewCaseDefaultStmt(Stmts *stmts)
{
    CaseStmt *caseStmt;

    caseStmt = ATreeMAlloc(sizeof (*caseStmt));
    caseStmt->type = CST_CASE_DEFAULT;
    caseStmt->stmts = stmts;
    return caseStmt;
}

CaseStmts *ATreeNewCaseStmts(CaseStmts *prev, CaseStmt *caseStmt)
{
    CaseStmts *newCaseStmts;

    newCaseStmts = ATreeMAlloc(sizeof (*newCaseStmts));
    newCaseStmts->prev = prev;
    newCaseStmts->caseStmt = caseStmt;
    return newCaseStmts;
}

Stmt *ATreeNewSwitchStmt(Expr *expr, CaseStmts *caseStmts)
{
    Stmt *newStmt;

    newStmt = ATreeAllocStmt(ST_SWITCH);
    newStmt->switchStmt.expr = expr;
    newStmt->switchStmt.caseStmts = caseStmts;
    newStmt->switchStmt.breakList = BPAllocInsList();
    return newStmt;
}

/*
 * 功能：新建语句序列
 * 返回值：
 **/
Stmts *ATreeNewStmts(Stmts *prevStmts, Stmt *stmt)
{
    Stmts *stmts;

    stmts = ATreeMAlloc(sizeof (*stmts));
    stmts->prevStmts = prevStmts;
    stmts->stmt = stmt;
    return stmts;
}

/*
 * 功能：分配类型
 * 返回值：
 **/
static inline Type *ATreeAllocType(TypeType type)
{
    Type *newType;

    newType = ATreeMAlloc(sizeof (*newType));
    newType->type = type;
    return newType;
}

/*
 * 功能：新建基本类型。
 * 返回值：
 **/
Type *ATreeNewBaseType(BaseTypeType type, size_t size, AlignType alignType)
{
    Type *newType;

    newType = ATreeAllocType(TT_BASE);
    newType->baseType.type = type;
    newType->size = size;
    newType->alignType = alignType;
    return newType;
}

/*
 * 功能：释放基本类型。
 * 返回值：
 **/
void ATreeFreeBaseType(Type *type)
{
    CplFree(type);
}

/*
 * 功能：获取类型尺寸。
 * 返回值：
 **/
size_t ATreeTypeSize(const Type *type)
{
    if (!type) {
        PrErr("type error invalid param: %p", type);
        exit(-1);
    }
    switch (type->type) {
    case TT_BASE:
    case TT_ARRAY:
    case TT_STRUCTURE:
    case TT_POINTER:
    case TT_FUNCTION:
        return type->size;
    default:
        PrErr("type->type error: %d", type->type);
        exit(-1);
    }
}

/*
 * 功能：新建数组类型
 * 返回值：
 **/
Type *ATreeNewArrayType(Type *elementType, size_t count, AlignType alignType)
{
    Type *newType;

    newType = ATreeAllocType(TT_ARRAY);
    newType->array.type = elementType;
    newType->array.count = count;
    newType->size = ATreeTypeSize(elementType) * count;
    newType->alignType = alignType;
    return newType;
}

/*
 * 功能：新建结构体类型
 * 返回值：
 **/
Type *ATreeNewStructType(Domain *domain, AlignType alignType, size_t size)
{
    Type *newType;

    newType = ATreeAllocType(TT_STRUCTURE);
    newType->alignType = alignType;
    newType->st.domain = domain;
    newType->size = size;
    return newType;
}

/*
 * 功能：新建指针类型
 * 返回值：
 **/
Type *ATreeNewPointerType(Type *pointType)
{
    Type *newType;

    newType = ATreeAllocType(TT_POINTER);
    newType->alignType = AT_4BYTE;
    newType->pointer.type = pointType;
    newType->size = 4;
    return newType;
}

/*
 * 功能：新建指针类型
 * 返回值：
 **/
Type *ATreeNewFunctionType(FunRetType *retType, FunArgs *funArgs)
{
    Type *newType;

    newType = ATreeAllocType(TT_FUNCTION);
    newType->alignType = AT_4BYTE;
    newType->size = 4;
    newType->funType.funArgs = funArgs;
    newType->funType.retType = retType;
    newType->funType.addr = 0;
    newType->funType.vargFlag = 0;
    return newType;
}

/*
 * 功能：释放数组类型。
 * 返回值：
 **/
void ATreeFreeArray(Array *array)
{
    if (!array)
        return;
    ATreeFreeType(array->type);
    CplFree(array);
}

/*
 * 功能：释放类型.
 * 返回值：
 **/
void ATreeFreeType(Type *type)
{
    if (!type || type->type == TT_BASE)
        return;
    ATreeFreeArray(&type->array);
    CplFree(type);
}

static int ATreeArrayIsEqual(const Array *lArray, const Array *rArray);

/*
 * 功能：判断类型释放相同。
 * 返回值：
 **/
static int ATreeTypeIsEqual(const Type *lType, const Type *rType)
{
    if (lType->type == TT_BASE
            && rType->type == TT_BASE) {
        return lType == rType;
    } else {
        return ATreeArrayIsEqual(&lType->array, &rType->array);
    }
}

/*
 * 功能：判断数组类型是否相同。
 * 返回值：
 **/
static int ATreeArrayIsEqual(const Array *lArray, const Array *rArray)
{
    return lArray->count == rArray->count
            &&ATreeTypeIsEqual(lArray->type, rArray->type);
}

/*
 * 功能：判断解引用的维度是否相同。
 * 返回值：
 **/
static int ATreeArrayDerefDimsIsEqual(const AEDims *lDims, const AEDims *rDims)
{
    if (lDims->nextDims && rDims->nextDims) {
        return ATreeArrayDerefDimsIsEqual(lDims->nextDims, rDims->nextDims);
    } else {
        if (!lDims->nextDims && !rDims->nextDims) {
            return 1;
        } else {
            return 0;
        }
    }
}

/*
 * 功能：获取数组接引用时的类型。
 * 返回值：
 **/
static Type *ATreeArrayDerefDimsType(const AEDims *dims)
{
    if (dims->nextDims) {
        return ATreeArrayDerefDimsType(dims->nextDims);
    } else {
        return dims->type;
    }
}

/*
 * 功能：拓宽表达式的类型。
 * 返回值：
 **/
Type *ATreeExprWiddenType(ExprType exprType, Expr *lExpr, Expr *rExpr)
{
    Type *lType, *rType;

    lType = ATreeExprType(lExpr);
    rType = ATreeExprType(rExpr);
    if (lType->type == TT_BASE
            && rType->type == TT_BASE) {
        if (lType == rType) {
            if (lType == bison->env->floatType) {
                return lType;
            } else {
                return bison->env->intType;
            }
        } else {
            if (lType->baseType.type == BTT_FLOAT
                    || rType->baseType.type == BTT_FLOAT) {
                return bison->env->floatType;
            } else if (lType->baseType.type == BTT_INT
                       || rType->baseType.type == BTT_INT) {
                return bison->env->intType;
            } else if (lType->baseType.type == BTT_SHORT
                       || rType->baseType.type == BTT_SHORT) {
                return bison->env->intType;
            } else if (lType->baseType.type == BTT_CHAR
                       || rType->baseType.type == BTT_CHAR) {
                return bison->env->intType;
            } else if (lType->baseType.type == BTT_BOOL
                       || rType->baseType.type == BTT_BOOL) {
                return bison->env->intType;
            } else {
                PrErr("syntax error, type error\n");
                exit(-1);
            }
        }
    } else if (lType->type == TT_POINTER
               && (rType->type == TT_BASE
                   && rType->baseType.type == BTT_FLOAT)) {
        if (exprType == ET_ADD
                || exprType == ET_SUB) {
            return lType;
        } else {
            PrErr("exprType error: %d", exprType);
            exit(-1);
        }
    } else {
        PrErr("lType->type error: %d", lType->type);
        PrErr("rType->type error: %d", rType->type);
        if (rType->type == TT_BASE) {
            PrErr("rType->baseType.type error: %d", rType->baseType.type);
        }
        PrErr("syntax error, widden type error\n");
        exit(-1);
    }
    return NULL;
}

/*
 * 功能：新建程序。
 * 返回值：
 **/
Program *ATreeNewProgram(FunDefines *funDefines)
{
    Program *program;

    program = ATreeMAlloc(sizeof (*program));
    program->funDefines = funDefines;
    return program;
}

/*
 * 功能：释放程序。
 * 返回值：
 **/
void ATreeFreeProgram(Program *program)
{
    (void)program;
}

FunRetType *ATreeNewFunRetTypeVoid(void)
{
    FunRetType *funRetType;

    funRetType = ATreeMAlloc(sizeof (*funRetType));
    funRetType->typeType = ERTT_VOID;
    return funRetType;
}

FunRetType *ATreeNewFunRetType(Type *type)
{
    FunRetType *funRetType;

    funRetType = ATreeMAlloc(sizeof (*funRetType));
    funRetType->typeType = ERTT_BASE_TYPE;
    funRetType->type = type;
    return funRetType;
}

FunArg *ATreeNewFunArg(const Token *id, Type *type)
{
    FunArg *funArg;

    funArg = ATreeMAlloc(sizeof (*funArg));
    funArg->type = type;
    funArg->id = id;
    return funArg;
}

FunArgs *ATreeNewFunArgs(FunArg *funArg, FunArgs *next)
{
    FunArgs *funArgs;

    funArgs = ATreeMAlloc(sizeof (*funArgs));
    funArgs->funArg = funArg;
    funArgs->next = next;
    return funArgs;
}

static FunArgs *_ATreeFunArgsReverse(FunArgs *nextFunArgs, FunArgs *funArgs)
{
    FunArgs *newFunArgs;

    newFunArgs = ATreeNewFunArgs(nextFunArgs->funArg, funArgs);
    if (nextFunArgs->next) {
        newFunArgs = _ATreeFunArgsReverse(nextFunArgs->next, newFunArgs);
    }
    return newFunArgs;
}

/*
 * 功能：返回函数参数的倒序
 * 返回值：
 **/
FunArgs *ATreeFunArgsReverse(FunArgs *funArgs)
{
    if (!funArgs) {
        return funArgs;
    } else {
        if (funArgs->next) {
            FunArgs *newFunArgs;

            newFunArgs = ATreeNewFunArgs(funArgs->funArg, NULL);
            return _ATreeFunArgsReverse(funArgs->next, newFunArgs);
        } else {
            return funArgs;
        }
    }
}

FunDefine *ATreeNewFunDefine(const Token *id, FunType *funType, Stmt *blockStmt)
{
    FunDefine *funDefine;

    funDefine = ATreeMAlloc(sizeof (*funDefine));
    funDefine->id = id;
    funDefine->funType = funType;
    funDefine->blockStmt = blockStmt;
    return funDefine;
}

FunDefines *ATreeNewFunDefines(FunDefines *prev, FunDefine *funDefine)
{
    FunDefines *funDefines;

    if (!funDefine) {
        return prev;
    }
    funDefines = ATreeMAlloc(sizeof (*funDefines));
    funDefines->prev = prev;
    funDefines->funDefine = funDefine;
    return funDefines;
}

FunDefines *ATreeNewFunDefinesByFunDefines(FunDefines *prev, FunDefines *funDefines)
{
    FunDefines *newFunDefines;
    FunDefines *tempFunDefines = NULL;

    if (!prev) {
        return funDefines;
    }
    if (!funDefines) {
        return prev;
    }
    while (funDefines->prev) {
        tempFunDefines = funDefines;
        funDefines = funDefines->prev;
    }
    newFunDefines = ATreeNewFunDefines(prev, funDefines->funDefine);
    return ATreeNewFunDefinesByFunDefines(newFunDefines, tempFunDefines);
}

FunRefParas *ATreeNewFunRefParas(Expr *expr, FunRefParas *next)
{
    FunRefParas *paras;

    paras = ATreeMAlloc(sizeof (*paras));
    paras->expr = expr;
    paras->next = next;
    return paras;
}

FunRef *ATreeNewFunRef(const Token *token, FunType *funType, FunRefParas *paras)
{
    FunRef *funRef;

    funRef = ATreeMAlloc(sizeof (*funRef));
    funRef->token = token;
    funRef->paras = paras;
    funRef->funType = funType;
    return funRef;
}

/*
 * 功能：字符串追加
 * 返回值：
 */
static void ATreeStringAppend(CplString *cplString, const char *str)
{
    int error = 0;

    error = CplStringAppendString(cplString, str);
    if (error != -ENOERR) {
        PrErr("append string error");
        exit(-1);
    }
}

/*
 * 功能：双目操作符表达式的字符串。
 * 返回值：
 */
static CplString *ATreeBinOpExprString(Expr *expr)
{
    CplString *cplString;
    CplString *lString, *rString;
    const char *opStr = "?";

    switch (expr->type) {
    case ET_ASSIGN:
        opStr = "=";
        break;
    case ET_LOR:
        //opStr = "||"; error
        break;
    case ET_LAND:
        //opStr = "&&"; error
        break;
    case ET_EQ:
        opStr = "==";
        break;
    case ET_NE:
        opStr = "!=";
        break;
    case ET_GT:
        opStr = ">";
        break;
    case ET_GE:
        opStr = ">=";
        break;
    case ET_LT:
        opStr = "<";
        break;
    case ET_LE:
        opStr = "<=";
        break;
    case ET_ADD:
        opStr = "+";
        break;
    case ET_SUB:
        opStr = "-";
        break;
    case ET_MUL:
        opStr = "*";
        break;
    case ET_DIV:
        opStr = "/";
        break;
    case ET_MOD:
        opStr = "%";
        break;
    default:;
    }

    cplString = CplStringAlloc();
    lString = ATreeExprString(expr->binOp.lExpr);
    rString = ATreeExprString(expr->binOp.rExpr);
    ATreeStringAppend(cplString, lString->str);
    ATreeStringAppend(cplString, opStr);
    ATreeStringAppend(cplString, rString->str);
    CplStringFree(lString);
    CplStringFree(rString);
    return cplString;
}

/*
 * 功能：单目操作符表达式的字符串
 * 返回值：
 */
static CplString *ATreeUnaryOpExprString(Expr *expr)
{
    CplString *cplString, *rString;
    const char *opStr = "?";

    switch (expr->type) {
    default:;
    }

    cplString = CplStringAlloc();
    rString = ATreeExprString(expr->unaryOp.expr);
    ATreeStringAppend(cplString, opStr);
    ATreeStringAppend(cplString, rString->str);
    CplStringFree(rString);
    return cplString;
}

/*
 * 功能：常量表达式的字符串。
 * 返回值：
 */
static CplString *ATreeConstExprString(Expr *expr)
{
    CplString *cplString;
    char buffer[100] = "?";

    cplString = CplStringAlloc();
    if (expr->cValue.type == CVT_IDIGIT) {
        snprintf(buffer, sizeof (buffer), "%d", expr->cValue.token->digit);
    } else if (expr->cValue.type == CVT_FDIGIT) {
        snprintf(buffer, sizeof (buffer), "%f", expr->cValue.token->fDigit);
    } else if (expr->cValue.type == CVT_TRUE) {
        snprintf(buffer, sizeof (buffer), "%s", "true");
    } else if (expr->cValue.type == CVT_FALSE) {
        snprintf(buffer, sizeof (buffer), "%s", "false");
    }

    ATreeStringAppend(cplString, buffer);
    return cplString;
}

/*
 * 功能：常量表达式的字符串。
 * 返回值：
 */
static CplString *ATreeConstSpcExprString(Expr *expr)
{
    CplString *cplString;
    char buffer[100] = "?";

    cplString = CplStringAlloc();
    if (expr->cValueSpc.type == CVTS_IDIGIT) {
        snprintf(buffer, sizeof (buffer), "%d", expr->cValueSpc.iValue);
    } else if (expr->cValueSpc.type == CVTS_FDIGIT) {
        snprintf(buffer, sizeof (buffer), "%f", expr->cValueSpc.fValue);
    } else if (expr->cValueSpc.type == CVTS_TRUE) {
        snprintf(buffer, sizeof (buffer), "%s", "true");
    } else if (expr->cValueSpc.type == CVTS_FALSE) {
        snprintf(buffer, sizeof (buffer), "%s", "false");
    }

    ATreeStringAppend(cplString, buffer);
    return cplString;
}

/*
 * 功能：临时值表达式的字符串
 * 返回值：
 */
static CplString *ATreeTempExprString(Expr *expr)
{
    char buffer[30] = {0};
    CplString *cplString;

    cplString = CplStringAlloc();
    snprintf(buffer, sizeof (buffer), "t%u", expr->temp.num);
    ATreeStringAppend(cplString, buffer);
    return cplString;
}

static CplString *ATreeIdExprString(Expr *expr)
{
    CplString *cplString;

    cplString = CplStringAlloc();
    CplStringAppendString(cplString, expr->id.token->cplString->str);
    return cplString;
}

static CplString *ATreeAccessElmString(Expr *expr)
{
    CplString *cplString;

    cplString = CplStringAlloc();
    CplStringAppendString(cplString, ATreeExprString(expr->accessElm.base)->str);
    CplStringAppendString(cplString, "[");
    CplStringAppendString(cplString, ATreeExprString(expr->accessElm.dims->idx)->str);
    CplStringAppendString(cplString, "]");
    return cplString;
}

/*
 * 功能：表达式的字符串
 * 返回值：
 */
CplString *ATreeExprString(Expr *expr)
{
    switch (expr->type) {
    case ET_ASSIGN:
    case ET_LOR:
    case ET_LAND:
    case ET_EQ:
    case ET_NE:
    case ET_GT:
    case ET_GE:
    case ET_LT:
    case ET_LE:
    case ET_ADD:
    case ET_SUB:
    case ET_MUL:
    case ET_DIV:
    case ET_MOD:
        return ATreeBinOpExprString(expr);
    case ET_LNOT:
        return ATreeUnaryOpExprString(expr);
    case ET_CONST:
        return ATreeConstExprString(expr);
    case ET_TEMP:
        return ATreeTempExprString(expr);
    case ET_CONST_SPC:
        return ATreeConstSpcExprString(expr);
    case ET_FUN_REF:
        PrErr("function have no string");
        exit(-1);
    case ET_ID:
        return ATreeIdExprString(expr);
    case ET_ACCESS_ELM:
        return ATreeAccessElmString(expr);
    }
    PrErr("expression type error: %d", expr->type);
    exit(-1);
}

Type *ATreeArrayBaseType(const Array *array)
{
    if (array->type->type == TT_ARRAY) {
        return ATreeArrayBaseType(&array->type->array);
    } else {
        return array->type;
    }
}

size_t ATreeAddrAlignByType(size_t addr, const Type *type)
{
    AlignType alignType;

    alignType = ATreeTypeGetAlignType(type);
    switch (alignType) {
    case AT_1BYTE:
        break;
    case AT_2BYTE:
        addr = Align2Byte(addr);
        break;
    case AT_4BYTE:
        addr = Align4Byte(addr);
        break;
    case AT_8BYTE:
        addr = Align8Byte(addr);
        break;
    default:
        PrErr("alignType error: %d", alignType);
        exit(-1);
    }
    return addr;
}

AlignType ATreeTypeGetAlignType(const Type *type)
{
    switch (type->type) {
    case TT_BASE:
    case TT_ARRAY:
    case TT_STRUCTURE:
    case TT_POINTER:
    case TT_FUNCTION:
        return type->alignType;
    default:
        PrErr("type->type error: %d", type->type);
        exit(-1);
    }
}
