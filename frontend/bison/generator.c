#include "generator.h"
#include "lex.h"
#include "abstract_tree.h"
#include "environ.h"

#include "list.h"
#include "stdint.h"
#include "cpl_mm.h"
#include "cpl_errno.h"
#include "cpl_debug.h"
#include "stdlib.h"
#include "cpl_string.h"
#include "bison.h"
#include "quadruple.h"
#include "backpatch.h"

#include "stdarg.h"

#define PrErr(...)      Pr(__FILE__, __LINE__, __FUNCTION__, "error", __VA_ARGS__)

#define GEN_DEBUG
#ifdef GEN_DEBUG
#define PrDbg(...)      Pr(__FILE__, __LINE__, __FUNCTION__, "debug", __VA_ARGS__)
#endif

static int GenStmt(Stmt *stmt, BPInsList **nextList);
static void GenExprJumping(Cond *cond, BPInsList **trueList, BPInsList **falseList);
static void _GenExprJumping(Cond *cond, BPInsList **trueList, BPInsList **falseList);
static Expr *GenExprForm(Expr *expr);
static Expr *GenExprReduce(Expr *expr);

#define GEN_CUR_INSTRUCT()      (bison->record->idx - 1)

static inline void *GenMAlloc(size_t size)
{
    void *p;

    p = CplAlloc(size);
    if (!p) {
        PrErr("no memory");
        exit(-1);
    }
    return p;
}

/*
 * 功能：简化赋值表达式
 * 返回值：
 */
static Expr *GenAssignExprReduce(Expr *expr)
{
    Expr *reduceExpr;

    reduceExpr = GenExprForm(expr);
    QRRecordAddUnary(bison->record, QROC_ASSIGN,
                     QROperandFromExpr(reduceExpr->binOp.rExpr),
                     QROperandFromExpr(reduceExpr->binOp.lExpr));
    return reduceExpr->binOp.lExpr;
}

//static Expr *trueExpr = x;
//static Expr *falseExpr = x;

/*
 * 功能：简化逻辑或表达式
 * 返回值：
 */
static Expr *GenLorExprReduce(Expr *expr)
{
    Expr *tempExpr;
    BPInsList *true1List = NULL, *false1List = NULL;
    BPInsList *true2List = NULL, *false2List = NULL;
    BPInsList *insList;

    tempExpr = ATreeNewTemp(expr->domain, bison->env->boolType);
    GenExprJumping(expr->binOp.lExpr, &true1List, &false1List);
    BPInsListBackPatch(false1List, bison->record, bison->record->idx);
    GenExprJumping(expr->binOp.rExpr, &true2List, &false2List);
    BPInsListBackPatch(true1List, bison->record, bison->record->idx);
    BPInsListBackPatch(true2List, bison->record, bison->record->idx);
    QRRecordAddUnary(bison->record, QROC_ASSIGN, QROperandFromExpr(bison->env->trueExpr), QROperandFromExpr(tempExpr));
    QRRecordAddUncondJump(bison->record, 0);
    insList = BPMakeInsList(GEN_CUR_INSTRUCT());
    BPInsListBackPatch(false2List, bison->record, bison->record->idx);
    QRRecordAddUnary(bison->record, QROC_ASSIGN, QROperandFromExpr(bison->env->falseExpr), QROperandFromExpr(tempExpr));
    BPInsListBackPatch(insList, bison->record, bison->record->idx);
    return tempExpr;
}

/*
 * 功能：简化逻辑与表达式
 * 返回值：
 */
static Expr *GenLandExprReduce(Expr *expr)
{
    Expr *tempExpr;
    BPInsList *true1List = NULL, *false1List = NULL;
    BPInsList *true2List = NULL, *false2List = NULL;
    BPInsList *insList;

    tempExpr = ATreeNewTemp(expr->domain, bison->env->boolType);
    GenExprJumping(expr->binOp.lExpr, &true1List, &false1List);
    BPInsListBackPatch(true1List, bison->record, bison->record->idx);
    GenExprJumping(expr->binOp.rExpr, &true2List, &false2List);
    BPInsListBackPatch(true2List, bison->record, bison->record->idx);
    QRRecordAddUnary(bison->record, QROC_ASSIGN, QROperandFromExpr(bison->env->trueExpr), QROperandFromExpr(tempExpr));
    QRRecordAddUncondJump(bison->record, 0);
    insList = BPMakeInsList(GEN_CUR_INSTRUCT());
    BPInsListBackPatch(false1List, bison->record, bison->record->idx);
    BPInsListBackPatch(false2List, bison->record, bison->record->idx);
    QRRecordAddUnary(bison->record, QROC_ASSIGN, QROperandFromExpr(bison->env->falseExpr), QROperandFromExpr(tempExpr));
    BPInsListBackPatch(insList, bison->record, bison->record->idx);
    return tempExpr;
}

/*
 * 功能：简化关系表达式
 * 返回值：
 */
static Expr *GenRelExprReduce(Expr *expr)
{
    Expr *tempExpr;
    BPInsList *trueList = NULL, *falseList = NULL;
    BPInsList *insList;
    Expr *reduceExpr;

    reduceExpr = GenExprForm(expr);
    tempExpr = ATreeNewTemp(expr->domain, bison->env->boolType);
    _GenExprJumping(reduceExpr, &trueList, &falseList);
    BPInsListBackPatch(trueList, bison->record, bison->record->idx);
    QRRecordAddUnary(bison->record, QROC_ASSIGN,  QROperandFromExpr(bison->env->trueExpr), QROperandFromExpr(tempExpr));
    QRRecordAddUncondJump(bison->record, 0);
    insList = BPMakeInsList(GEN_CUR_INSTRUCT());
    BPInsListBackPatch(falseList, bison->record, bison->record->idx);
    QRRecordAddUnary(bison->record, QROC_ASSIGN, QROperandFromExpr(bison->env->falseExpr), QROperandFromExpr(tempExpr));
    BPInsListBackPatch(insList, bison->record, bison->record->idx);
    return tempExpr;
}

/*
 * 功能：简化算术表达式
 * 返回值：
 */
static Expr *GenArithExprReduce(Expr *expr)
{
    Expr *tempExpr;
    Expr *reduceExpr;
    QROpCode opCode;

    tempExpr = ATreeNewTemp(expr->domain, ATreeExprType(expr));
    reduceExpr = GenExprForm(expr);
    switch (expr->type) {
    case ET_ADD:
        opCode = QROC_ADD;
        break;
    case ET_SUB:
        opCode = QROC_SUB;
        break;
    case ET_MUL:
        opCode = QROC_MUL;
        break;
    case ET_DIV:
        opCode = QROC_DIV;
        break;
    case ET_MOD:
        opCode = QROC_MOD;
        break;
    default:
        PrErr("unknown expr type");
        exit(-1);
    }
    QRRecordAddBin(bison->record, opCode,
                   QROperandFromExpr(reduceExpr->binOp.lExpr), QROperandFromExpr(reduceExpr->binOp.rExpr),
                   QROperandFromExpr(tempExpr));

    return tempExpr;
}

/*
 * 功能：简化逻辑非表达式
 * 返回值：
 */
static Expr *GenLnotExprReduce(Expr *expr)
{
    Expr *tempExpr;
    BPInsList *trueList = NULL, *falseList = NULL;
    BPInsList *insList;

    tempExpr = ATreeNewTemp(expr->domain, bison->env->boolType);
    GenExprJumping(expr->unaryOp.expr, &trueList, &falseList);
    BPInsListBackPatch(trueList, bison->record, bison->record->idx);
    QRRecordAddUnary(bison->record, QROC_ASSIGN, QROperandFromExpr(bison->env->falseExpr), QROperandFromExpr(tempExpr));
    QRRecordAddUncondJump(bison->record, 0);
    insList = BPMakeInsList(GEN_CUR_INSTRUCT());
    BPInsListBackPatch(falseList, bison->record, bison->record->idx);
    QRRecordAddUnary(bison->record, QROC_ASSIGN, QROperandFromExpr(bison->env->trueExpr), QROperandFromExpr(tempExpr));
    BPInsListBackPatch(insList, bison->record, bison->record->idx);
    return tempExpr;
}

/*
 * 功能：计算左值解引用维度索引
 * pLastType: 最后一级的元素类型。
 * 返回值：
 */
static Expr *GenLValueDerefDimsIdx(Domain *domain, const AEDims *dims, Type **pLastType)
{
    Expr *tempExpr, *nextExpr;
    Expr *typeSize;
    Type *nextType;

    if (!dims)
        return NULL;
    nextExpr = GenLValueDerefDimsIdx(domain, dims->nextDims, &nextType);
    if (dims->nextDims == NULL) {
        *pLastType = dims->type;
    } else {
        *pLastType = nextType;
    }
    tempExpr = ATreeNewTemp(domain, bison->env->charType);
    typeSize = ATreeNewCValueSpcIntExpr(domain, ATreeTypeSize(dims->type));
    QRRecordAddBin(bison->record, QROC_MUL,
                   QROperandFromExpr(dims->idx), QROperandFromExpr(typeSize),
                   QROperandFromExpr(tempExpr));
    if (nextExpr != NULL) {
        QRRecordAddBin(bison->record, QROC_ADD,
                       QROperandFromExpr(tempExpr), QROperandFromExpr(nextExpr),
                       QROperandFromExpr(tempExpr));
    }
    return tempExpr;
}

static Expr *GenLValueExprForm(Expr *expr);

static Expr *GenAccessElmExprReduce(Expr *expr)
{
    Expr *tempExpr;
    Expr *reduceExpr;

    reduceExpr = GenExprForm(expr);
    tempExpr = ATreeNewTemp(expr->domain, ATreeExprType(reduceExpr));
    QRRecordAddUnary(bison->record, QROC_ASSIGN,
                     QROperandFromExpr(reduceExpr), QROperandFromExpr(tempExpr));
    return tempExpr;
}

/*
 * 功能：正序生成param a，这样第1个参数离函数调用最远，第2个参数第2远，以此类推。方便生成伪代码。
 *      活动记录中，最后一个参数在栈底，方便处理变参函数。
 * 返回值：
 **/
static int GenFunRefParas(Domain *domain, FunRefParas *paras, size_t *parasCnt, FunArgs *funArgs, FunType *funType)
{
    size_t nextParasCnt = 0;
    Expr *reduceExpr;
    QROperand *operand;
    Type *paraType;
    Type *argType;

    if (!paras) {
        *parasCnt = 0;
        return 0;
    }
    if (!funArgs && !funType->vargFlag) {
        PrErr("param too many: %p", funArgs);
        exit(-1);
    }

    if (funArgs) {
        argType = funArgs->funArg->type;
        if (argType->type == TT_ARRAY) {
            argType = ATreeNewPointerType(argType->array.type);
        } else if (argType->type == TT_FUNCTION) {
            argType = ATreeNewPointerType(argType);
        }
    } else {
        argType = NULL;
    }

    paraType = ATreeExprType(paras->expr);
    if (paraType->type == TT_BASE) {
        Expr *tempExpr;
        Type *tempType;

        if (argType) {
            if (argType->type != TT_BASE) {
                PrErr("argType->type error: %d", argType->type);
                exit(-1);
            }
            tempType = argType;
        } else {
            if (paraType->baseType.type == BTT_FLOAT) {
                tempType = bison->env->floatType;
            } else if (paraType->baseType.type == BTT_BOOL
                       || paraType->baseType.type == BTT_CHAR
                       || paraType->baseType.type == BTT_SHORT
                       || paraType->baseType.type == BTT_INT) {
                tempType = bison->env->intType;
            } else {
                PrErr("paraType->baseType.type error: %d", paraType->baseType.type);
                exit(-1);
            }
        }
        tempExpr = ATreeNewTemp(domain, tempType);
        reduceExpr = GenExprReduce(paras->expr);
        QRRecordAddUnary(bison->record, QROC_ASSIGN,
                         QROperandFromExpr(reduceExpr), QROperandFromExpr(tempExpr));
        operand = QROperandFromExpr(tempExpr);
    } else if (paraType->type == TT_ARRAY) {
        Expr *tempExpr;
        Expr *getAddrExpr;
        Expr *accessElmExpr;
        AEDims *dims;
        Expr *dimsIdx;
        Type *tempType;

        if (argType) {
            if (argType->type != TT_POINTER) {
                PrErr("argType->type error: %d", argType->type);
                exit(-1);
            }
            tempType = argType;
        } else {
            tempType = ATreeNewPointerType(paraType->array.type);
        }
        tempExpr = ATreeNewTemp(domain, tempType);
        dimsIdx = ATreeNewCValueSpcIntExpr(domain, 0);
        dims = ATreeNewAEDims(paraType->array.type, dimsIdx, NULL);
        accessElmExpr = ATreeNewAccessElmExpr(domain, paras->expr, dims, paraType);
        getAddrExpr = ATreeNewUnaryExpr(domain, ET_GET_ADDR, tempType, accessElmExpr);
        getAddrExpr = GenExprReduce(getAddrExpr);
        QRRecordAddUnary(bison->record, QROC_ASSIGN,
                         QROperandFromExpr(getAddrExpr), QROperandFromExpr(tempExpr));
        operand = QROperandFromExpr(tempExpr);
    } else if (paraType->type == TT_STRUCTURE) {
        if (argType) {
            if (argType->type != TT_STRUCTURE) {
                PrErr("argType->type error: %d", argType->type);
                exit(-1);
            }
        } else {
            PrErr("argType->type error: %d", argType->type);
            exit(-1);
        }
        reduceExpr = GenExprReduce(paras->expr);
        operand = QROperandFromExpr(reduceExpr);
    } else if (paraType->type == TT_POINTER) {
        Expr *tempExpr;

        if (argType) {
            /*需要更加严格的判断，判断指针类型是否相同。*/
            if (argType->type != TT_POINTER) {
                PrErr("argType->type error: %d", argType->type);
                exit(-1);
            }
        }
        tempExpr = ATreeNewTemp(domain, paraType);
        reduceExpr = GenExprReduce(paras->expr);
        QRRecordAddUnary(bison->record, QROC_ASSIGN,
                         QROperandFromExpr(reduceExpr), QROperandFromExpr(tempExpr));
        operand = QROperandFromExpr(tempExpr);
    } else if (paraType->type == TT_FUNCTION) {
        if (argType) {
            if (argType->type != TT_POINTER) {
                PrErr("argType->type error: %d", argType->type);
                exit(-1);
            }
        } else {

        }
        PrErr("type->type error: %d", paraType->type);
        exit(-1);
    } else {
        PrErr("type->type error: %d", paraType->type);
        exit(-1);
    }
    QRRecordAddFunParam(bison->record, operand);
    GenFunRefParas(domain, paras->next, &nextParasCnt, funArgs->next, funType);
    *parasCnt = nextParasCnt + 1;
    return 0;
}

static Expr *GenFunRefExprReduce(Expr *expr)
{
    size_t parasCnt = 0;
    Expr *tempExpr;
    FunRetType *funRetType;

    funRetType = expr->funRef->funType->retType;
    if (funRetType->typeType == ERTT_VOID) {
        PrErr("functon %s have not return value", expr->funRef->token->cplString->str);
        exit(-1);
    }
    tempExpr = ATreeNewTemp(expr->domain, funRetType->type);
    GenFunRefParas(expr->domain, expr->funRef->paras, &parasCnt, expr->funRef->funType->funArgs, expr->funRef->funType);
    QRRecordAddFunCall(bison->record, expr->funRef->token, parasCnt, QROperandFromExpr(tempExpr), expr->domain);
    return tempExpr;
}

/*
 * 功能：解引用指针
 * 返回值：
 **/
static Expr *GenRefPointerExprReduce(Expr *expr)
{
    return GenExprForm(expr);
}

/*
 * 功能：取地址
 * 返回值：
 **/
static Expr *GenGetAddrExprReduce(Expr *expr)
{
    return GenExprForm(expr);
}

/*
 * 功能：类型转换
 * 返回值：
 **/
static Expr *GenTypeCaseExprReduce(Expr *expr)
{
    return GenExprForm(expr);
}

/*
 * 功能：简化表达式
 * 返回值：
 */
static Expr *GenExprReduce(Expr *expr)
{
    switch (expr->type) {
    case ET_ASSIGN:
        return GenAssignExprReduce(expr);
    case ET_LOR:
        return GenLorExprReduce(expr);
    case ET_LAND:
        return GenLandExprReduce(expr);
    case ET_EQ:
    case ET_NE:
    case ET_GT:
    case ET_GE:
    case ET_LT:
    case ET_LE:
        return GenRelExprReduce(expr);
    case ET_ADD:
    case ET_SUB:
    case ET_MUL:
    case ET_DIV:
    case ET_MOD:
        return GenArithExprReduce(expr);
    case ET_LNOT:
        return GenLnotExprReduce(expr);
    case ET_CONST:
        return expr;
    case ET_TEMP:
        return expr;
    case ET_FUN_REF:
        return GenFunRefExprReduce(expr);
    case ET_CONST_SPC:
        return expr;
    case ET_ACCESS_MBR:
        return GenExprForm(expr);
    case ET_REF_POINTER:
        return GenRefPointerExprReduce(expr);
    case ET_GET_ADDR:
        return GenGetAddrExprReduce(expr);
    case ET_TYPE_CAST:
        return GenTypeCaseExprReduce(expr);
    case ET_ID:
        return expr;
    case ET_ACCESS_ELM:
        return GenAccessElmExprReduce(expr);
    }
    PrErr("expression type error");
    exit(-1);
}

/*
 * 功能：生成简单的表达式跳转语句
 * 返回值：
 */
static void _GenExprJumping(Cond *cond, BPInsList **trueList, BPInsList **falseList)
{
    BPInsList *newTrueList, *newFalseList;

    QRRecordAddCondJump(bison->record, QROC_TRUE_JUMP, cond, 0);
    newTrueList = BPMakeInsList(GEN_CUR_INSTRUCT());
    QRRecordAddUncondJump(bison->record, 0);
    newFalseList = BPMakeInsList(GEN_CUR_INSTRUCT());
    *trueList = newTrueList;
    *falseList = newFalseList;
}

/*
 * 功能：生成逻辑或表达式跳转语句
 * 返回值：
 */
static void GenLorExprJumping(Cond *cond, BPInsList **trueList, BPInsList **falseList)
{
    BPInsList *true1List = NULL, *false1List = NULL;
    BPInsList *true2List = NULL, *false2List = NULL;

    GenExprJumping(cond->binOp.lExpr, &true1List, &false1List);
    BPInsListBackPatch(false1List, bison->record, bison->record->idx);
    GenExprJumping(cond->binOp.rExpr, &true2List, &false2List);
    BPInsListMerge(true2List, true1List);
    *trueList = true2List;
    *falseList = false2List;
}

/*
 * 功能：生成逻辑与表达式跳转语句
 * 返回值：
 */
static void GenLandExprJumping(Cond *cond, BPInsList **trueList, BPInsList **falseList)
{
    BPInsList *true1List = NULL, *false1List = NULL;
    BPInsList *true2List = NULL, *false2List = NULL;

    GenExprJumping(cond->binOp.lExpr, &true1List, &false1List);
    BPInsListBackPatch(true1List, bison->record, bison->record->idx);
    GenExprJumping(cond->binOp.rExpr, &true2List, &false2List);
    BPInsListMerge(false2List, false1List);
    *trueList = true2List;
    *falseList = false2List;
}

/*
 * 功能：生成表达式跳转语句
 * 返回值：
 */
static void GenExprJumping(Cond *cond, BPInsList **trueList, BPInsList **falseList)
{
    switch (cond->type) {
    case ET_ASSIGN:
        cond = GenExprReduce(cond);
        return _GenExprJumping(cond->binOp.lExpr, trueList, falseList);
    case ET_LOR:
        return GenLorExprJumping(cond, trueList, falseList);
    case ET_LAND:
        return GenLandExprJumping(cond, trueList, falseList);
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
        cond = GenExprReduce(cond);
        return _GenExprJumping(cond, trueList, falseList);
    case ET_LNOT:
        return GenExprJumping(cond->unaryOp.expr, falseList, trueList);
    case ET_CONST:
        return _GenExprJumping(cond, trueList, falseList);
    case ET_ID:
    case ET_ACCESS_ELM:
        cond = GenExprReduce(cond);
        return _GenExprJumping(cond, trueList, falseList);
    case ET_TEMP:
        return _GenExprJumping(cond, trueList, falseList);
    case ET_FUN_REF:
        cond = GenExprReduce(cond);
        return _GenExprJumping(cond, trueList, falseList);
    case ET_CONST_SPC:
        return _GenExprJumping(cond, trueList, falseList);
    case ET_ACCESS_MBR:
    case ET_REF_POINTER:
    case ET_GET_ADDR:
    case ET_TYPE_CAST:
        cond = GenExprReduce(cond);
        return _GenExprJumping(cond, trueList, falseList);
    }
}

/*
 * 功能：规化赋值表达式
 * 返回值：
 */
static Expr *GenAssignExprForm(Expr *expr)
{
    Expr *lExpr, *rExpr;

    lExpr = GenExprForm(expr->binOp.lExpr);
    if (lExpr->type == ET_ID) {
        switch (expr->binOp.rExpr->type) {
        case ET_ADD:
        case ET_SUB:
        case ET_MUL:
        case ET_DIV:
        case ET_MOD:
            rExpr = GenExprForm(expr->binOp.rExpr);
            break;
        default:
            rExpr = GenExprReduce(expr->binOp.rExpr);
            break;
        }
    } else {
        switch (expr->binOp.rExpr->type) {
        case ET_ADD:
        case ET_SUB:
        case ET_MUL:
        case ET_DIV:
        case ET_MOD:
        case ET_ACCESS_ELM:
            rExpr = GenExprForm(expr->binOp.rExpr);
            break;
        default:
            rExpr = GenExprReduce(expr->binOp.rExpr);
            break;
        }
    }
    return ATreeNewBinExpr(expr->domain, expr->type,
                           expr->binOp.type,
                           lExpr,
                           rExpr);
}

/*
 * 功能：规划逻辑或表达式
 * 返回值：
 */
static Expr *GenLorExprForm(Expr *expr)
{
    return ATreeNewBinExpr(expr->domain, expr->type,
                              expr->binOp.type,
                              GenExprReduce(expr->binOp.lExpr),
                              GenExprReduce(expr->binOp.rExpr));
}

/*
 * 功能：规划逻辑与表达式
 * 返回值：
 */
static Expr *GenLandExprForm(Expr *expr)
{
    return ATreeNewBinExpr(expr->domain, expr->type,
                              expr->binOp.type,
                              GenExprReduce(expr->binOp.lExpr),
                              GenExprReduce(expr->binOp.rExpr));
}

/*
 * 功能：规划关系表达式
 * 返回值：
 */
static Expr *GenRelExprForm(Expr *expr)
{
    return ATreeNewBinExpr(expr->domain, expr->type,
                              expr->binOp.type,
                              GenExprReduce(expr->binOp.lExpr),
                              GenExprReduce(expr->binOp.rExpr));
}

/*
 * 功能：规化算术表达式
 * 返回值：
 */
static Expr *GenArithExprForm(Expr *expr)
{
    return ATreeNewBinExpr(expr->domain, expr->type,
                              expr->binOp.type,
                              GenExprReduce(expr->binOp.lExpr),
                              GenExprReduce(expr->binOp.rExpr));
}

/*
 * 功能：规化逻辑非表达式
 * 返回值：
 */
static Expr *GenLnotExprForm(Expr *expr)
{
    return ATreeNewUnaryExpr(expr->domain, expr->type,
                             expr->unaryOp.type,
                             GenExprReduce(expr->unaryOp.expr));
}

/*
 * 功能：规化成员访问表达式
 * 返回值：
 */
static Expr * GenMemAccessExprForm(Expr *expr)
{
    Expr *baseExpr = GenExprForm(expr->accessMbr.base);

    if (expr->accessMbr.type == AMTYPE_DIRECT) {
        Expr *newExpr;
        Expr *memOffset;

        memOffset = expr->accessMbr.memOffset;
        if (baseExpr->type == ET_ACCESS_MBR) {
            Expr *tempExpr;

            tempExpr = ATreeNewTemp(expr->domain, bison->env->intType);
            QRRecordAddBin(bison->record, QROC_ADD, QROperandFromExpr(memOffset),
                           QROperandFromExpr(baseExpr->accessMbr.memOffset), QROperandFromExpr(tempExpr));
            if (baseExpr->accessMbr.base->type == ET_ID) {
                newExpr = ATreeNewAccessMem(expr->domain, AMTYPE_DIRECT, baseExpr->accessMbr.base, NULL, expr->accessMbr.memtype, tempExpr);
                return newExpr;
            } else {
                PrErr("baseExpr->memAccess.base->type error: %d", baseExpr->accessMbr.base->type);
                exit(-1);
            }
        } else {
            if (baseExpr->type == ET_ID) {
                newExpr = ATreeNewAccessMem(expr->domain, AMTYPE_DIRECT, baseExpr, NULL, expr->accessMbr.memtype, memOffset);
                return newExpr;
            } else {
                PrErr("baseExpr->type error: %d", baseExpr->type);
                exit(-1);
            }
        }
    } else if (expr->accessMbr.type == AMTYPE_INDIRECT) {
        if (baseExpr->type == ET_ACCESS_MBR) {
            Type *baseMemType = baseExpr->accessMbr.memtype;

            if (baseMemType->type == TT_POINTER) {
                Expr *tempExpr;
                Expr *newExpr;
                Expr *memOffset;

                tempExpr = ATreeNewTemp(expr->domain, baseMemType);
                QRRecordAddUnary(bison->record, QROC_ASSIGN,
                                 QROperandFromExpr(baseExpr), QROperandFromExpr(tempExpr));
                memOffset = expr->accessMbr.memOffset;
                newExpr = ATreeNewAccessMem(expr->domain, AMTYPE_INDIRECT, tempExpr, NULL, expr->accessMbr.memtype, memOffset);
                return newExpr;
            } else {
                PrErr("baseMemType->type error: %d", baseMemType->type);
                exit(-1);
            }
        } else {
            if (baseExpr->type == ET_ID) {
                Type *baseExprType;

                baseExprType = ATreeExprType(baseExpr);
                if (baseExprType->type == TT_POINTER) {
                    if (baseExprType->pointer.type->type == TT_STRUCTURE) {
                        return expr;
                    } else {
                        PrErr("baseExprType->pointer.type->type error: %d", baseExprType->pointer.type->type);
                        exit(-1);
                    }
                } else {
                    PrErr("baseExprType->type error: %d", baseExprType->type);
                    exit(-1);
                }
            } else {
                PrErr("baseExpr->type error: %d", baseExpr->type);
                exit(-1);
            }
        }
    } else {
        PrErr("expr->memAccess.type error: %d", expr->accessMbr.type);
        exit(-1);
    }
}

/*
 * 功能：规化解引用指针表达式
 * 返回值：
 */
static Expr *GenRefPointerExprForm(Expr *expr)
{
    Expr *baseExpr;

    baseExpr = GenExprReduce(expr->unaryOp.expr);
    if ((baseExpr->type == ET_REF_POINTER)
            || baseExpr->type == ET_ACCESS_ELM
            || baseExpr->type == ET_ACCESS_MBR) {
        Expr *tempExpr;

        tempExpr = ATreeNewTemp(expr->domain, ATreeExprType(baseExpr));
        QRRecordAddUnary(bison->record, QROC_ASSIGN,
                         QROperandFromExpr(baseExpr), QROperandFromExpr(tempExpr));
        return ATreeNewUnaryExpr(expr->domain, expr->type, expr->unaryOp.type, tempExpr);
    } else if (baseExpr->type == ET_GET_ADDR) {
        return baseExpr->unaryOp.expr;
    } else {
        return ATreeNewUnaryExpr(expr->domain, expr->type, expr->unaryOp.type, baseExpr);
    }
}

/*
 * 功能：规化取地址表达式
 * 返回值：
 */
static Expr *GenGetAddrExprForm(Expr *expr)
{
    Expr *innerExpr;

    innerExpr = GenExprForm(expr->unaryOp.expr);
    if (innerExpr->type == ET_REF_POINTER) {
        return innerExpr->unaryOp.expr;
    } else if (innerExpr->type == ET_ID
               || innerExpr->type == ET_ACCESS_MBR) {
        return ATreeNewUnaryExpr(expr->domain, expr->type, expr->unaryOp.type, innerExpr);
    } else {
        PrErr("baseExpr->type error: %d", innerExpr->type);
        exit(-1);
    }
}

/*
 * 功能：规化类型转换表达式
 * 返回值：
 */
static Expr *GenTypeCastExprForm(Expr *expr)
{
    Expr *innerExpr;
    Type *type;

    type = ATreeExprType(expr);
    innerExpr = GenExprForm(expr->unaryOp.expr);
    if (innerExpr->type == ET_ID) {
        return ATreeNewIdExpr(innerExpr->domain, innerExpr->id.token, type);
    } else if (innerExpr->type == ET_GET_ADDR) {
        return ATreeNewUnaryExpr(innerExpr->domain, ET_GET_ADDR, type, innerExpr->unaryOp.expr);
    } else if (innerExpr->type == ET_ACCESS_MBR) {
        return ATreeNewAccessMem(innerExpr->domain, innerExpr->accessMbr.type, innerExpr->accessMbr.base, innerExpr->accessMbr.memId,
                                 type, innerExpr->accessMbr.memOffset);
    } else if (innerExpr->type == ET_REF_POINTER) {
        return ATreeNewUnaryExpr(innerExpr->domain, ET_REF_POINTER, type, innerExpr->unaryOp.expr);
    } else if (innerExpr->type == ET_CONST
               || innerExpr->type == ET_CONST_SPC) {
        Expr *tempExpr;

        tempExpr = ATreeNewTemp(expr->domain, type);
        QRRecordAddUnary(bison->record, QROC_ASSIGN,
                         QROperandFromExpr(innerExpr), QROperandFromExpr(tempExpr));
        return tempExpr;
    } else {
        PrErr("innerExpr->type error: %d", innerExpr->type);
        exit(-1);
    }
}

static Expr *GenAccessElmOffsetExpr(Domain *domain, AEDims *dims, Type **type)
{
    Expr *tempExpr;
    Expr *idxReduce;
    Expr *typeSizeExpr;

    tempExpr = ATreeNewTemp(domain, bison->env->intType);
    idxReduce = GenExprReduce(dims->idx);
    typeSizeExpr = ATreeNewCValueSpcIntExpr(domain, ATreeTypeSize(dims->type));
    QRRecordAddBin(bison->record, QROC_MUL, QROperandFromExpr(idxReduce),
                   QROperandFromExpr(typeSizeExpr), QROperandFromExpr(tempExpr));
    if (dims->nextDims) {
        Expr *nextOffset;

        nextOffset = GenAccessElmOffsetExpr(domain, dims->nextDims, type);
        QRRecordAddBin(bison->record, QROC_ADD, QROperandFromExpr(nextOffset),
                       QROperandFromExpr(tempExpr), QROperandFromExpr(tempExpr));
    } else {
        *type = dims->type;
    }
    return tempExpr;
}

Type *GenAccessElmBaseExprType(Expr *expr)
{
    Type *type;

    type = ATreeExprType(expr);
    if (expr->type == ET_ID) {
        DomainEntry *entry;

        entry = EnvDomainGetEntry(expr->domain, expr->id.token);
        if (entry->funArgFlag == 1) {
            if (type->type == TT_ARRAY) {
                return ATreeNewPointerType(type->array.type);
            } else {
                return type;
            }
        } else {
            return type;
        }
    } else {
        return type;
    }
}

/*
 * 功能：规范化访问元素表达式
 * 返回值：
 */
static Expr *GenAccessElmExprForm(Expr *expr)
{
    Expr *newExpr;
    Expr *offsetExpr;
    Type *baseType;
    Type *memType;

    baseType = GenAccessElmBaseExprType(expr->accessElm.base);
    if (!expr->accessElm.dims) {
        PrErr("expr->accessElm.dims: %p", expr->accessElm.dims);
        exit(-1);
    }
    offsetExpr = GenAccessElmOffsetExpr(expr->domain, expr->accessElm.dims, &memType);
    if (baseType->type == TT_ARRAY) {
        newExpr = ATreeNewAccessMem(expr->domain, AMTYPE_DIRECT, expr->accessElm.base, NULL, memType, offsetExpr);
    } else if (baseType->type == TT_POINTER) {
        newExpr = ATreeNewAccessMem(expr->domain, AMTYPE_INDIRECT, expr->accessElm.base, NULL, memType, offsetExpr);
    } else {
        PrErr("type->type error: %d", baseType->type);
        exit(-1);
    }

    return newExpr;
}

static Expr *GenFunRefExprForm(Expr *expr)
{
    size_t parasCnt = 0;

    GenFunRefParas(expr->domain, expr->funRef->paras, &parasCnt, expr->funRef->funType->funArgs, expr->funRef->funType);
    QRRecordAddFunCallNrv(bison->record, expr->funRef->token, parasCnt, expr->domain);
    return expr;
}

/*
 * 功能：规化表达式
 * 返回值：
 */
static Expr *GenExprForm(Expr *expr)
{
    switch (expr->type) {
    case ET_ASSIGN:
        return GenAssignExprForm(expr);
    case ET_LOR:
        return GenLorExprForm(expr);
    case ET_LAND:
        return GenLandExprForm(expr);
    case ET_EQ:
    case ET_NE:
    case ET_GT:
    case ET_GE:
    case ET_LT:
    case ET_LE:
        return GenRelExprForm(expr);
    case ET_ADD:
    case ET_SUB:
    case ET_MUL:
    case ET_DIV:
    case ET_MOD:
        return GenArithExprForm(expr);
    case ET_LNOT:
        return GenLnotExprForm(expr);
    case ET_CONST:
        return expr;
    case ET_TEMP:
        return expr;
    case ET_CONST_SPC:
        return expr;
    case ET_ACCESS_MBR:
        return GenMemAccessExprForm(expr);
    case ET_REF_POINTER:
        return GenRefPointerExprForm(expr);
    case ET_GET_ADDR:
        return GenGetAddrExprForm(expr);
    case ET_TYPE_CAST:
        return GenTypeCastExprForm(expr);
    case ET_ID:
        return expr;
    case ET_ACCESS_ELM:
        return GenAccessElmExprForm(expr);
    case ET_FUN_REF:
        PrErr("expression type error\n");
        exit(-1);
    }
    PrErr("expression type error\n");
    exit(-1);
}

/*
 * 功能：生成表达式语句
 * 返回值：
 */
static int GenExprStmt(Stmt *stmt, BPInsList **nextList)
{
    Expr *expr;
    BPInsList *newNextList = NULL;

    expr = stmt->exprStmt.expr;
    if (expr->type == ET_ASSIGN) {
        Expr *rExpr;

        expr = GenExprForm(stmt->exprStmt.expr);
        rExpr = expr->binOp.rExpr;
        switch (rExpr->type) {
        case ET_ADD:
            QRRecordAddBin(bison->record, QROC_ADD, QROperandFromExpr(rExpr->binOp.lExpr),
                           QROperandFromExpr(rExpr->binOp.rExpr), QROperandFromExpr(expr->binOp.lExpr));
            break;
        case ET_SUB:
            QRRecordAddBin(bison->record, QROC_SUB, QROperandFromExpr(rExpr->binOp.lExpr),
                           QROperandFromExpr(rExpr->binOp.rExpr), QROperandFromExpr(expr->binOp.lExpr));
            break;
        case ET_MUL:
            QRRecordAddBin(bison->record, QROC_MUL, QROperandFromExpr(rExpr->binOp.lExpr),
                           QROperandFromExpr(rExpr->binOp.rExpr), QROperandFromExpr(expr->binOp.lExpr));
            break;
        case ET_DIV:
            QRRecordAddBin(bison->record, QROC_DIV, QROperandFromExpr(rExpr->binOp.lExpr),
                           QROperandFromExpr(rExpr->binOp.rExpr), QROperandFromExpr(expr->binOp.lExpr));
            break;
        case ET_MOD:
            QRRecordAddBin(bison->record, QROC_MOD, QROperandFromExpr(rExpr->binOp.lExpr),
                           QROperandFromExpr(rExpr->binOp.rExpr), QROperandFromExpr(expr->binOp.lExpr));
            break;
        case ET_CONST:
        case ET_TEMP:
        case ET_CONST_SPC:
        case ET_ACCESS_MBR:
        case ET_REF_POINTER:
        case ET_GET_ADDR:
        case ET_TYPE_CAST:
        case ET_ID:
        case ET_ACCESS_ELM:
            QRRecordAddUnary(bison->record, QROC_ASSIGN, QROperandFromExpr(expr->binOp.rExpr),
                             QROperandFromExpr(expr->binOp.lExpr));
            break;
        default:
            PrErr("rExpr->type: %u", rExpr->type);
            exit(-1);
        }
    } else if (expr->type == ET_FUN_REF) {
        GenFunRefExprForm(expr);
    } else {
        /*其它表达式不处理。*/
    }
    newNextList = BPAllocInsList();
    *nextList = newNextList;
    return 0;
}

/*
 * 功能：生成if语句
 * 返回值：
 */
static int GenIfStmt(Stmt *stmt, BPInsList **nextList)
{
    BPInsList *trueList = NULL, *falseList = NULL;
    BPInsList *subNext = NULL;

    GenExprJumping(stmt->ifStmt.cond, &trueList, &falseList);
    BPInsListBackPatch(trueList, bison->record, bison->record->idx);
    GenStmt(stmt->ifStmt.stmt, &subNext);
    BPInsListMerge(falseList, subNext);
    *nextList = falseList;
    return 0;
}

/*
 * 功能：生成if else语句
 * 返回值：
 */
static int GenElseStmt(Stmt *stmt, BPInsList **nextList)
{
    BPInsList *trueList = NULL, *falseList = NULL;
    BPInsList *sub1Next = NULL, *sub2Next = NULL;
    BPInsList *newNextList;

    GenExprJumping(stmt->elseStmt.cond, &trueList, &falseList);
    BPInsListBackPatch(trueList, bison->record, bison->record->idx);
    GenStmt(stmt->elseStmt.ifStmt, &sub1Next);
    QRRecordAddUncondJump(bison->record, 0);
    newNextList = BPMakeInsList(GEN_CUR_INSTRUCT());
    BPInsListBackPatch(falseList, bison->record, bison->record->idx);
    GenStmt(stmt->elseStmt.elseStmt, &sub2Next);
    BPInsListMerge(newNextList, sub1Next);
    BPInsListMerge(newNextList, sub2Next);
    *nextList = newNextList;
    return 0;
}

/*
 * 功能：生成while语句
 * 返回值：
 */
static int GenWhileStmt(Stmt *stmt, BPInsList **nextList)
{
    BPInsList *subNextList = NULL;
    BPInsList *trueList = NULL, *falseList = NULL;
    size_t beginInsIdx;

    EnvLastLevelListPush(bison->env, stmt);
    beginInsIdx = bison->record->idx;
    GenExprJumping(stmt->whileStmt.cond, &trueList, &falseList);
    BPInsListBackPatch(trueList, bison->record, bison->record->idx);
    GenStmt(stmt->whileStmt.stmt, &subNextList);
    QRRecordAddUncondJump(bison->record, beginInsIdx);
    BPInsListMerge(subNextList, stmt->whileStmt.continueList);
    BPInsListBackPatch(subNextList, bison->record, beginInsIdx);
    BPInsListMerge(falseList, stmt->whileStmt.breakList);
    *nextList = falseList;
    EnvLastLevelListPop(bison->env);
    return 0;
}

/*
 * 功能：生成do while语句
 * 返回值：
 */
static int GenDoStmt(Stmt *stmt, BPInsList **nextList)
{
    BPInsList *subNextList = NULL;
    BPInsList *trueList = NULL, *falseList = NULL;
    size_t beginInsIdx;

    EnvLastLevelListPush(bison->env, stmt);
    beginInsIdx = bison->record->idx;
    GenStmt(stmt->doStmt.stmt, &subNextList);
    BPInsListBackPatch(subNextList, bison->record, beginInsIdx);
    GenExprJumping(stmt->doStmt.cond, &trueList, &falseList);
    BPInsListBackPatch(trueList, bison->record, beginInsIdx);
    BPInsListBackPatch(stmt->doStmt.continueList, bison->record, beginInsIdx);
    BPInsListMerge(falseList, stmt->doStmt.breakList);
    *nextList = falseList;
    EnvLastLevelListPop(bison->env);
    return 0;
}

/*
 * 功能：生成语句序列
 * 返回值：
 */
static int GenStmts(Stmts *stmts, BPInsList **nextList)
{
    BPInsList *newNextList = NULL;

    if (!stmts)
        return 0;
    if (stmts->prevStmts) {
        GenStmts(stmts->prevStmts, &newNextList);
        BPInsListBackPatch(newNextList, bison->record, bison->record->idx);
    }
    GenStmt(stmts->stmt, nextList);
    return 0;
}

/*
 * 功能：生成块语句
 * 返回值：
 */
static int GenBlockStmt(Stmt *stmt, BPInsList **nextList)
{
    QRRecordAddBlkStart(bison->record, stmt->domain);
    GenStmts(stmt->blockStmt.stmts, nextList);
    QRRecordAddBlkEnd(bison->record, stmt->domain);
    return 0;
}

/*
 * 功能：生成break语句
 * 返回值：
 */
static int GenBreakStmt(Stmt *stmt, BPInsList **nextList)
{
    Stmt *topStmt;
    BPInsList *bpInsList = NULL;

    (void)stmt;
    *nextList = NULL;
    topStmt = EnvBreakLastLevelListTop(bison->env);
    if (!topStmt) {
        PrErr("break no last level\n");
        exit(-1);
    }
    if (topStmt->type == ST_WHILE) {
        bpInsList = topStmt->whileStmt.breakList;
    } else if (topStmt->type == ST_DO) {
        bpInsList = topStmt->doStmt.breakList;
    }  else if (topStmt->type == ST_SWITCH) {
        bpInsList = topStmt->switchStmt.breakList;
    }  else {
        PrErr("statement type error\n");
        exit(-1);
    }
    QRRecordAddUncondJump(bison->record, 0);
    BPInsListAddInstruct(bpInsList, GEN_CUR_INSTRUCT());
    return 0;
}

/*
 * 功能：生成continue语句。
 * 返回值：
 */
static int GenContinueStmt(Stmt *stmt, BPInsList **nextList)
{
    Stmt *topStmt;
    BPInsList *bpInsList = NULL;

    (void)stmt;
    *nextList = NULL;
    topStmt = EnvContinueLastLevelListTop(bison->env);
    if (!topStmt) {
        PrErr("continue no last level\n");
        exit(-1);
    }
    if (topStmt->type == ST_WHILE) {
        bpInsList = topStmt->whileStmt.continueList;
    } else if (topStmt->type == ST_DO) {
        bpInsList = topStmt->doStmt.continueList;
    } else {
        PrErr("statement type error\n");
        exit(-1);
    }
    QRRecordAddUncondJump(bison->record, 0);
    BPInsListAddInstruct(bpInsList, GEN_CUR_INSTRUCT());

    return 0;
}

typedef enum {
    ELMT_EXPR_LABEL,
    ELMT_DEFAULT_LABEL,
} ExprLabelMapType;

typedef struct {
    struct list_head node;
    ExprLabelMapType type;
    size_t label;
    Expr *expr;
} ExprLabelMap;

static ExprLabelMap *GenNewExprLabelMap(size_t label, Expr *expr)
{
    ExprLabelMap *elMap;

    elMap = GenMAlloc(sizeof (*elMap));
    elMap->type = ELMT_EXPR_LABEL;
    elMap->expr = expr;
    elMap->label = label;
    return elMap;
}

static ExprLabelMap *GenNewDefaultLabelMap(size_t label)
{
    ExprLabelMap *elMap;

    elMap = GenMAlloc(sizeof (*elMap));
    elMap->type = ELMT_DEFAULT_LABEL;
    elMap->label = label;
    return elMap;
}

static int GenCaseStmts(CaseStmts *caseStmts, BPInsList **nextList, struct list_head *list)
{
    CaseStmt *caseStmt;
    ExprLabelMap *exprLabelMap;
    BPInsList *prevStmtsNextList = NULL;
    BPInsList *newNextList = NULL;
    size_t label;

    if (!caseStmts) {
        *nextList = BPAllocInsList();
        return 0;
    }
    if (caseStmts->prev) {
        GenCaseStmts(caseStmts->prev, &prevStmtsNextList, list);
        BPInsListBackPatch(prevStmtsNextList, bison->record, bison->record->idx);
    }

    caseStmt = caseStmts->caseStmt;
    if (caseStmt->type == CST_CASE_EXPR) {
        Expr *temp;

        temp = GenExprReduce(caseStmt->caseExpr.expr);
        label = bison->record->idx;
        GenStmts(caseStmt->caseExpr.stmts, &newNextList);
        exprLabelMap = GenNewExprLabelMap(label, temp);
    } else if (caseStmt->type == CST_CASE_DEFAULT) {
        label = bison->record->idx;
        GenStmts(caseStmt->stmts, &newNextList);
        exprLabelMap = GenNewDefaultLabelMap(label);
    } else {
        PrErr("error, unknown case stmt type");
        exit(-1);
    }
    *nextList = newNextList;
    list_add_tail(&exprLabelMap->node, list);
    return 0;
}

static int GenSwichTestStmt(struct list_head *exprLabelMapList, Expr *testExpr)
{
    struct list_head *pos;
    ExprLabelMap *exprLabelMap;
    size_t defaultLabel = 0;

    list_for_each(pos, exprLabelMapList) {
        exprLabelMap = container_of(pos, ExprLabelMap, node);
        if (exprLabelMap->type == ELMT_EXPR_LABEL) {
            Expr *eqExpr, *reduceExpr;

            eqExpr = ATreeNewBinExpr(testExpr->domain, ET_EQ, bison->env->boolType,
                                     testExpr, exprLabelMap->expr);
            reduceExpr = GenExprReduce(eqExpr);
            QRRecordAddCondJump(bison->record, QROC_TRUE_JUMP,
                                     reduceExpr, exprLabelMap->label);
        } else if (exprLabelMap->type == ELMT_DEFAULT_LABEL) {
            if (defaultLabel == 0) {
                defaultLabel = exprLabelMap->label;
            } else {
                PrErr("syntax error, too many default stmt");
                exit(-1);
            }
        }
    }
    if (defaultLabel != 0) {
        QRRecordAddUncondJump(bison->record, defaultLabel);
    }
    EnvLastLevelListPop(bison->env);

    return 0;
}

static int GenSwitchStmt(Stmt *stmt, BPInsList **nextList)
{
    Expr *testExpr;
    struct list_head exprLabelMapList;
    BPInsList *testInsList = NULL;
    BPInsList *newNextList = NULL;
    BPInsList *caseNextList = NULL;

    newNextList = BPAllocInsList();
    EnvLastLevelListPush(bison->env, stmt);
    testExpr = GenExprReduce(stmt->switchStmt.expr);
    QRRecordAddUncondJump(bison->record, 0);
    testInsList = BPMakeInsList(GEN_CUR_INSTRUCT());
    INIT_LIST_HEAD(&exprLabelMapList);
    GenCaseStmts(stmt->switchStmt.caseStmts, &caseNextList, &exprLabelMapList);
    QRRecordAddUncondJump(bison->record, 0);
    BPInsListAddInstruct(newNextList, GEN_CUR_INSTRUCT());
    BPInsListMerge(newNextList, stmt->switchStmt.breakList);
    BPInsListMerge(newNextList, caseNextList);
    *nextList = newNextList;
    BPInsListBackPatch(testInsList, bison->record, bison->record->idx);
    GenSwichTestStmt(&exprLabelMapList, testExpr);
    EnvLastLevelListPop(bison->env);
    return 0;
}

static int GenReturnVoidStmt(Stmt *stmt, BPInsList **nextList)
{
    (void)stmt;
    *nextList = NULL;
    QRRecordAddReturnVoid(bison->record, stmt->domain);
    return 0;
}

static int GenReturnValueStmt(Stmt *stmt, BPInsList **nextList)
{
    Expr *tempExpr;

    *nextList = NULL;
    tempExpr = GenExprReduce(stmt->returnStmt.expr);
    QRRecordAddReturnValue(bison->record, QROperandFromExpr(tempExpr), stmt->domain);
    return 0;
}

/*
 * 功能：生成语句
 * 返回值：
 */
static int GenStmt(Stmt *stmt, BPInsList **nextList)
{
    switch (stmt->type) {
    case ST_EXPR:
        return GenExprStmt(stmt, nextList);
    case ST_IF:
        return GenIfStmt(stmt, nextList);
    case ST_ELSE:
        return GenElseStmt(stmt, nextList);
    case ST_WHILE:
        return GenWhileStmt(stmt, nextList);
    case ST_DO:
        return GenDoStmt(stmt, nextList);
    case ST_BLOCK:
        return GenBlockStmt(stmt, nextList);
    case ST_BREAK:
        return GenBreakStmt(stmt, nextList);
    case ST_CONTINUE:
        return GenContinueStmt(stmt, nextList);
    case ST_SWITCH:
        return GenSwitchStmt(stmt, nextList);
    case ST_RETURN_VOID:
        return GenReturnVoidStmt(stmt, nextList);
    case ST_RETURN_VALUE:
        return GenReturnValueStmt(stmt, nextList);
    }
    PrErr("statement type error, %d\n", stmt->type);
    exit(-1);
}

static int GenFunDefine(FunDefine *funDefine, BPInsList **nextList)
{
    QRRecordAddFunStart(bison->record, funDefine->id, funDefine->funType);
    GenStmt(funDefine->blockStmt, nextList);
    QRRecordAddFunEnd(bison->record, funDefine->id, funDefine->blockStmt->domain);
    return 0;
}

static int GenFunDefines(FunDefines *funDefines, BPInsList **nextList)
{
    BPInsList *newNextList = NULL;

    if (!funDefines) {
        *nextList = NULL;
        return 0;
    }
    if (funDefines->prev) {
        GenFunDefines(funDefines->prev, &newNextList);
        BPInsListBackPatch(newNextList, bison->record, bison->record->idx);
    }
    GenFunDefine(funDefines->funDefine, nextList);
    return 0;
}

/*
 * 功能：生成程序中间代码。
 * 返回值：
 */
int GenProgram(Program *program)
{
    BPInsList *nextList = NULL;

    if (!program)
        return -EINVAL;
    GenFunDefines(program->funDefines, &nextList);
    BPInsListBackPatch(nextList, bison->record, bison->record->idx);
    QRPrintRecord(bison->record);
    return 0;
}


