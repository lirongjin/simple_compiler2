#include "quadruple.h"
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
#include "stdlib.h"
#include "string.h"
#include "cpl_string.h"

#define PrErr(...)      Pr(__FILE__, __LINE__, __FUNCTION__, "error", __VA_ARGS__)

#define QR_DEBUG
#ifdef QR_DEBUG
#define PrDbg(...)      Pr(__FILE__, __LINE__, __FUNCTION__, "debug", __VA_ARGS__)
#endif

static void *QRMAlloc(size_t size)
{
    void *p;

    p = CplAlloc(size);
    if (!p) {
        PrErr("no memory");
        exit(-1);
    }
    return p;
}

QRRecord *QRRecordAlloc(void)
{
    QRRecord *record;

    record = QRMAlloc(sizeof (*record));
    record->qrArr = NULL;
    record->idx = 0;
    record->size = 0;
    return record;
}

void QRRecordFree(QRRecord *record)
{
    if (record) {
        if (record->qrArr)
            CplFree(record->qrArr);
        CplFree(record);
    }
}

QuadRuple *QRRecordAllocQuadRuple(QRRecord *record)
{
    if (record->size == 0) {
        record->size = 8 * sizeof (QuadRuple);
        record->qrArr = QRMAlloc(record->size);
    } else {
        if (record->size <= record->idx * sizeof (QuadRuple)) {
            size_t newSize;
            QuadRuple *newQrArr;

            if (record->size <= 256 * sizeof (QuadRuple)) {
                newSize = record->size * 2;
            } else {
                newSize = record->size + 256 * sizeof (QuadRuple);
            }
            newQrArr = QRMAlloc(newSize);
            memcpy(newQrArr, record->qrArr, record->idx * sizeof (QuadRuple));
            CplFree(record->qrArr);
            record->qrArr = newQrArr;
            record->size = newSize;
        }
    }
    return &record->qrArr[record->idx++];
}

int QRRecordAddBin(QRRecord *record, QROpCode op, QROperand *arg1, QROperand *arg2, QROperand *result)
{
    QuadRuple *qr;

    if (!record)
        return -EINVAL;

    qr = QRRecordAllocQuadRuple(record);
    qr->op = op;
    qr->binOp.arg1 = arg1;
    qr->binOp.arg2 = arg2;
    qr->binOp.result = result;
    return 0;
}

int QRRecordAddUnary(QRRecord *record, QROpCode op, QROperand *arg1, QROperand *result)
{
    QuadRuple *qr;

    if (!record)
        return -EINVAL;

    qr = QRRecordAllocQuadRuple(record);
    qr->op = op;
    qr->unaryOp.arg1 = arg1;
    qr->unaryOp.result = result;
    return 0;
}

int QRRecordAddUncondJump(QRRecord *record, size_t label)
{
    QuadRuple *qr;

    if (!record)
        return -EINVAL;

    qr = QRRecordAllocQuadRuple(record);
    qr->op = QROC_JUMP;
    qr->uncondJump.dstLabel = label;
    return 0;
}

int QRRecordAddCondJump(QRRecord *record, QROpCode op, Expr *cond, size_t label)
{
    QuadRuple *qr;

    if (!record)
        return -EINVAL;

    qr = QRRecordAllocQuadRuple(record);
    qr->op = op;
    qr->condJump.cond = cond;
    qr->condJump.dstLabel = label;
    return 0;
}

int QRRecordAddReturnVoid(QRRecord *record, Domain *domain)
{
    QuadRuple *qr;

    if (!record)
        return -EINVAL;

    qr = QRRecordAllocQuadRuple(record);
    qr->op = QROC_RETURN_VOID;
    qr->returnObj.expr = NULL;
    qr->returnObj.domain = domain;
    return 0;
}

int QRRecordAddReturnValue(QRRecord *record, QROperand *expr, Domain *domain)
{
    QuadRuple *qr;

    if (!record)
        return -EINVAL;

    qr = QRRecordAllocQuadRuple(record);
    qr->op = QROC_RETURN_VALUE;
    qr->returnObj.expr = expr;
    qr->returnObj.domain = domain;
    return 0;
}

int QRRecordAddFunParam(QRRecord *record, QROperand *expr)
{
    QuadRuple *qr;

    if (!record)
        return -EINVAL;

    qr = QRRecordAllocQuadRuple(record);
    qr->op = QROC_FUN_PARAM;
    qr->funParam.expr = expr;
    return 0;
}

int QRRecordAddFunCall(QRRecord *record, const Token *token, size_t paraCnt,
                       QROperand *result, Domain *domain)
{
    QuadRuple *qr;

    if (!record)
        return -EINVAL;

    qr = QRRecordAllocQuadRuple(record);
    qr->op = QROC_FUN_CALL;
    qr->funCall.token = token;
    qr->funCall.paraCnt = paraCnt;
    qr->funCall.result = result;
    qr->funCall.domain = domain;
    return 0;
}

int QRRecordAddFunCallNrv(QRRecord *record, const Token *token, size_t paraCnt,
                          Domain *domain)
{
    QuadRuple *qr;

    if (!record)
        return -EINVAL;

    qr = QRRecordAllocQuadRuple(record);
    qr->op = QROC_FUN_CALL_NRV;
    qr->funCall.token = token;
    qr->funCall.paraCnt = paraCnt;
    qr->funCall.result = NULL;
    qr->funCall.domain = domain;
    return 0;
}

int QRRecordAddFunStart(QRRecord *record, const Token *id, FunType *funType)
{
    QuadRuple *qr;

    if (!record)
        return -EINVAL;

    qr = QRRecordAllocQuadRuple(record);
    qr->op = QROC_FUN_START;
    qr->funStartFlag.id = id;
    qr->funStartFlag.funType = funType;
    return 0;
}

int QRRecordAddFunEnd(QRRecord *record, const Token *id, Domain *domain)
{
    QuadRuple *qr;

    if (!record)
        return -EINVAL;

    qr = QRRecordAllocQuadRuple(record);
    qr->op = QROC_FUN_END;
    qr->funEnd.id = id;
    qr->funEnd.domain = domain;
    return 0;
}

int QRRecordAddBlkStart(QRRecord *record, Domain *domain)
{
    QuadRuple *qr;

    if (!record)
        return -EINVAL;

    qr = QRRecordAllocQuadRuple(record);
    qr->op = QROC_BLK_START;
    qr->blkStart.domain = domain;
    return 0;
}

int QRRecordAddBlkEnd(QRRecord *record, Domain *domain)
{
    QuadRuple *qr;

    if (!record)
        return -EINVAL;

    qr = QRRecordAllocQuadRuple(record);
    qr->op = QROC_BLK_END;
    qr->blkEnd.domain = domain;
    return 0;
}

int QRRecordAddProgramStart(QRRecord *record, Domain *domain)
{
    QuadRuple *qr;

    if (!record)
        return -EINVAL;

    qr = QRRecordAllocQuadRuple(record);
    qr->op = QROC_PROGRAM_START;
    qr->programStart.domain = domain;
    return 0;
}

int QRRecordAddProgramEnd(QRRecord *record, Domain *domain)
{
    QuadRuple *qr;

    if (!record)
        return -EINVAL;

    qr = QRRecordAllocQuadRuple(record);
    qr->op = QROC_PROGRAM_END;
    qr->programEnd.domain = domain;
    return 0;
}

size_t QRecordCount(QRRecord *record)
{
    if (record)
        return record->idx;
    else
        return -EINVAL;
}

/*
 * 功能：字符串追加
 * 返回值：
 */
static void QRStringAppend(CplString *cplString, const char *str)
{
    int error = 0;

    error = CplStringAppendString(cplString, str);
    if (error != -ENOERR) {
        PrErr("append string error");
        exit(-1);
    }
}

static CplString *QROperandVarString(const QRId *var)
{
    CplString *cplString;

    cplString = CplStringAlloc();
    QRStringAppend(cplString, var->token->cplString->str);
    return cplString;
}

static CplString *QROperandTempString(const Temp *temp);

static CplString *QRAccessMemOffsetString(const QRAccessMbrOffset *amOffset)
{
    switch (amOffset->offsetType) {
    case QRAMOT_VAL:
    {
        CplString *cplString;
        char buf[100] = {0};

        snprintf(buf, sizeof (buf), "%d", amOffset->valOffset);
        cplString = CplStringAlloc();
        CplStringAppendString(cplString, buf);
        return cplString;
    }
    case QRAMOT_ID:
        return QROperandVarString(&amOffset->idOffset.id);
    case QRAMOT_TEMP:
        return QROperandTempString(amOffset->tempOffset.temp);
    default:
        PrErr("amOffset->offsetType error: %d", amOffset->offsetType);
        exit(-1);
    }
}

static CplString *QROperandAccessMemString(const QRAccessMbr *accessMem)
{
    CplString *cplString;
    CplString *idxString;

    idxString = QRAccessMemOffsetString(accessMem->offset);
    cplString = CplStringAlloc();
    if (accessMem->type == QRAMT_DIRECT)  {
        if (accessMem->baseType == QRAMBT_ID) {
            QRStringAppend(cplString, accessMem->baseId.token->cplString->str);
        } else {
            PrErr("accessMem->baseType error: %d", accessMem->baseType);
            exit(-1);
        }
    } else if (accessMem->type == QRAMT_INDIRECT) {
        if (accessMem->baseType == QRAMBT_ID) {
            QRStringAppend(cplString, "(*");
            QRStringAppend(cplString, accessMem->baseId.token->cplString->str);
            QRStringAppend(cplString, ")");
        } else if (accessMem->baseType == QRAMBT_TEMP) {
            char tStr[100];

            snprintf(tStr, sizeof (tStr), "t%u", accessMem->baseTemp->num);
            QRStringAppend(cplString, "(*");
            QRStringAppend(cplString, tStr);
            QRStringAppend(cplString, ")");
        } else {
            PrErr("accessMem->baseType error: %d", accessMem->baseType);
            exit(-1);
        }
    } else {
        PrErr("accessMem->accessType error: %d", accessMem->type);
        exit(-1);
    }
    QRStringAppend(cplString, ".");
    QRStringAppend(cplString, idxString->str);
    return cplString;
}

static CplString *QROperandValString(const QRValue *val)
{
    CplString *cplString;
    char buffer[100] = "?";

    cplString = CplStringAlloc();
    if (val->type == QRVT_INT) {
        snprintf(buffer, sizeof (buffer), "%d", val->ival);
    } else if (val->type == QRVT_FLOAT) {
        snprintf(buffer, sizeof (buffer), "%f", val->fval);
    } else {
        PrErr("val->type error: %d", val->type);
        exit(-1);
    }
    QRStringAppend(cplString, buffer);
    return cplString;
}

static CplString *QROperandTempString(const Temp *temp)
{
    char buffer[30] = {0};
    CplString *cplString;

    cplString = CplStringAlloc();
    snprintf(buffer, sizeof (buffer), "t%u", temp->num);
    QRStringAppend(cplString, buffer);
    return cplString;
}

static CplString *QROperandGetAddrString(const QRGetAddr *getAddr)
{
    CplString *cplString, *tempStr;

    cplString = CplStringAlloc();
    CplStringAppendString(cplString, "&");
    switch (getAddr->type) {
    case QRGAT_ID:
        tempStr = QROperandVarString(&getAddr->id);
        break;
    case QRGAT_ACCESS_MEM:
        tempStr = QROperandAccessMemString(&getAddr->accessMbr);
        break;
    default:
        PrErr("getAddr->type error: %d", getAddr->type);
        exit(-1);
    }
    CplStringAppendString(cplString, tempStr->str);
    CplStringFree(tempStr);
    return cplString;
}

static CplString *QROperandString(QROperand *operand)
{
    switch (operand->type) {
    case QROT_ID:
        return QROperandVarString(&operand->id);
    case QROT_ACCESS_MBR:
        return QROperandAccessMemString(&operand->accessMbr);
    case QROT_VAL:
        return QROperandValString(&operand->val);
    case QROT_TEMP:
        return QROperandTempString(operand->temp);
    case QROT_GET_ADDR:
        return QROperandGetAddrString(&operand->getAddr);
    default:
        PrErr("operand->type error: %d", operand->type);
        exit(-1);
    }
}

#define QREmit(...)     printf(__VA_ARGS__), fflush(stdout)

void QRPrintBinOp(const QuadRuple *ruple)
{
    const char *opStr = "?";

    switch (ruple->op) {
    case QROC_ADD:
        opStr = "+";
        break;
    case QROC_SUB:
        opStr = "-";
        break;
    case QROC_MUL:
        opStr = "*";
        break;
    case QROC_DIV:
        opStr = "/";
        break;
    case QROC_MOD:
        opStr = "%";
        break;
    default:
        PrErr("");
        exit(-1);
    }
    QREmit("%s = %s %s %s\n",
           QROperandString(ruple->binOp.result)->str,
           QROperandString(ruple->binOp.arg1)->str,
           opStr,
           QROperandString(ruple->binOp.arg2)->str);

}

void QRPrintJump(const QuadRuple *ruple)
{
    QREmit("goto %u\n", ruple->uncondJump.dstLabel);
}

void QRPrintCondJump(const QuadRuple *ruple)
{
    if (ruple->op == QROC_TRUE_JUMP) {
        QREmit("if %s goto %u\n",
                ATreeExprString(ruple->condJump.cond)->str,
                ruple->condJump.dstLabel);
    } else if (ruple->op == QROC_FALSE_JUMP) {
        QREmit("iffalse %s goto %u\n",
                ATreeExprString(ruple->condJump.cond)->str,
                ruple->condJump.dstLabel);
    } else {
        PrErr("");
        exit(-1);
    }
}

void QRPrintAssign(const QuadRuple *ruple)
{
    QREmit("%s = %s\n",
           QROperandString(ruple->unaryOp.result)->str,
           QROperandString(ruple->unaryOp.arg1)->str);
}

void QRPrintRel(const QuadRuple *ruple)
{
    (void)ruple;
    PrErr("error");
    exit(-1);
}

void QRPrintReturnVoid(const QuadRuple *ruple)
{
    (void)ruple;
    QREmit("return\n");
}

void QRPrintReturnValue(const QuadRuple *ruple)
{
    QREmit("return %s\n",
           QROperandString(ruple->returnObj.expr)->str);
}

void QRPrintFunParam(const QuadRuple *ruple)
{
    QREmit("param %s\n",
           QROperandString(ruple->funParam.expr)->str);
}

void QRPrintFunCall(const QuadRuple *ruple)
{
    QREmit("%s = call %s, %u\n",
           QROperandString(ruple->funCall.result)->str,
           ruple->funCall.token->cplString->str,
           ruple->funCall.paraCnt);
}

void QRPrintFunCallNrv(const QuadRuple *ruple)
{
    QREmit("call %s, %u\n",
           ruple->funCall.token->cplString->str,
           ruple->funCall.paraCnt);
}

void QRPrintFunStart(const QuadRuple *ruple)
{
    QREmit("fun start [%s]\n", ruple->funStartFlag.id->cplString->str);
    (void)ruple;
}

void QRPrintFunEnd(const QuadRuple *ruple)
{
    QREmit("fun end [%s]\n", ruple->funEnd.id->cplString->str);
    (void)ruple;
}

void QRPrintBlkStart(const QuadRuple *ruple)
{
    QREmit("blk start\n");
    (void)ruple;
}

void QRPrintBlkEnd(const QuadRuple *ruple)
{
    QREmit("blk end\n");
    (void)ruple;
}

void QRPrintProgramStart(const QuadRuple *ruple)
{
    QREmit("program start\n");
    (void)ruple;
}

void QRPrintProgramEnd(const QuadRuple *ruple)
{
    QREmit("program end\n");
    (void)ruple;
}

void QRQuardRuplePrint(const QuadRuple *ruple)
{
    switch (ruple->op) {
    case QROC_ADD:
    case QROC_SUB:
    case QROC_MUL:
    case QROC_DIV:
    case QROC_MOD:
        return QRPrintBinOp(ruple);
    case QROC_JUMP:
        return QRPrintJump(ruple);
    case QROC_TRUE_JUMP:
    case QROC_FALSE_JUMP:
        return QRPrintCondJump(ruple);
    case QROC_ASSIGN:
        return QRPrintAssign(ruple);
    case QROC_EQ:
    case QROC_NE:
    case QROC_LT:
    case QROC_LE:
    case QROC_GT:
    case QROC_GE:
        return QRPrintRel(ruple);
    case QROC_RETURN_VOID:
        return QRPrintReturnVoid(ruple);
    case QROC_RETURN_VALUE:
        return QRPrintReturnValue(ruple);
    case QROC_FUN_PARAM:
        return QRPrintFunParam(ruple);
    case QROC_FUN_CALL:
        return QRPrintFunCall(ruple);
    case QROC_FUN_CALL_NRV:
        return QRPrintFunCallNrv(ruple);
    case QROC_FUN_START:
        return QRPrintFunStart(ruple);
    case QROC_FUN_END:
        return QRPrintFunEnd(ruple);
    case QROC_BLK_START:
        return QRPrintBlkStart(ruple);
    case QROC_BLK_END:
        return QRPrintBlkEnd(ruple);
    case QROC_PROGRAM_START:
        return QRPrintProgramStart(ruple);
    case QROC_PROGRAM_END:
        return QRPrintProgramEnd(ruple);
    default:
        PrErr("unknown ruple->op: %u", ruple->op);
        exit(-1);
    }
}

void QRPrintRecord(const QRRecord *record)
{
    size_t w;

    if (!record)
        return;
    for (w = 0; w < record->idx; w++) {
        QREmit("%04u: ", w);
        QRQuardRuplePrint(&record->qrArr[w]);
    }
}

QROperand *QRNewIdOperand(Domain *domain, Type *dataType, const Token *token)
{
    QROperand *operand;

    operand = QRMAlloc(sizeof (*operand));
    operand->domain = domain;
    operand->type = QROT_ID;
    operand->id.dataType = dataType;
    operand->id.token = token;
    return operand;
}

QROperand *QRNewAccessMemOperandIdBase(Domain *domain, Type *mbrDataType, const Token *token, QRAccessMbrOffset *offset, QRAccessMbrType accessType, Type *baseDataType)
{
    QROperand *operand;

    operand = QRMAlloc(sizeof (*operand));
    operand->domain = domain;
    operand->type = QROT_ACCESS_MBR;
    operand->accessMbr.type = accessType;
    operand->accessMbr.baseType = QRAMBT_ID;
    operand->accessMbr.dataType = mbrDataType;
    operand->accessMbr.baseId.token = token;
    operand->accessMbr.baseId.dataType = baseDataType;
    operand->accessMbr.offset = offset;
    return operand;
}

QROperand *QRNewAccessMemOperandTempBase(Domain *domain, Type *mbrDataType, Temp *temp, QRAccessMbrOffset *offset, QRAccessMbrType accessType)
{
    QROperand *operand;

    operand = QRMAlloc(sizeof (*operand));
    operand->domain = domain;
    operand->type = QROT_ACCESS_MBR;
    operand->accessMbr.type = accessType;
    operand->accessMbr.baseType = QRAMBT_ID;
    operand->accessMbr.dataType = mbrDataType;
    operand->accessMbr.baseType = QRAMBT_TEMP;
    operand->accessMbr.baseTemp = temp;
    operand->accessMbr.offset = offset;
    return operand;
}

QROperand *QRNewValIntOperand(Domain *domain, int val)
{
    QROperand *operand;

    operand = QRMAlloc(sizeof (*operand));
    operand->domain = domain;
    operand->type = QROT_VAL;
    operand->val.type = QRVT_INT;
    operand->val.ival = val;
    return operand;
}

QROperand *QRNewValFloatOperand(Domain *domain, float val)
{
    QROperand *operand;

    operand = QRMAlloc(sizeof (*operand));
    operand->domain = domain;
    operand->type = QROT_VAL;
    operand->val.type = QRVT_FLOAT;
    operand->val.fval = val;
    return operand;
}

QROperand *QRNewTempOperand(Domain *domain, Temp *temp)
{
    QROperand *operand;

    operand = QRMAlloc(sizeof (*operand));
    operand->domain = domain;
    operand->type = QROT_TEMP;
    operand->temp = temp;
    return operand;
}

QROperand *QRNewGetAddrIdOperand(Domain *domain, Type *idDataType, const Token *token, Type *dataType)
{
    QROperand *operand;

    operand = QRMAlloc(sizeof (*operand));
    operand->domain = domain;
    operand->type = QROT_GET_ADDR;
    operand->getAddr.dataType = dataType;
    operand->getAddr.type = QRGAT_ID;
    operand->getAddr.id.dataType = idDataType;
    operand->getAddr.id.token = token;
    return operand;
}

QROperand *QRNewGetAddrAccessMemOperand(Domain *domain, Type *amDataType, const Token *token, QRAccessMbrOffset *offset, QRAccessMbrType accessType, Type *dataType)
{
    QROperand *operand;

    operand = QRMAlloc(sizeof (*operand));
    operand->domain = domain;
    operand->type = QROT_GET_ADDR;
    operand->getAddr.type = QRGAT_ACCESS_MEM;
    operand->getAddr.dataType = dataType;
    operand->getAddr.accessMbr.type = accessType;
    operand->getAddr.accessMbr.baseType = QRAMBT_ID;
    operand->getAddr.accessMbr.dataType = amDataType;
    operand->getAddr.accessMbr.baseId.token = token;
    operand->getAddr.accessMbr.offset = offset;
    return operand;
}

QROperand *QRNewGetAddrAccessMemOperandTempBase(Domain *domain, Type *amDataType, Temp *temp, QRAccessMbrOffset *offset, QRAccessMbrType accessType, Type *dataType)
{
    QROperand *operand;

    operand = QRMAlloc(sizeof (*operand));
    operand->domain = domain;
    operand->type = QROT_GET_ADDR;
    operand->getAddr.type = QRGAT_ACCESS_MEM;
    operand->getAddr.dataType = dataType;
    operand->getAddr.accessMbr.type = accessType;
    operand->getAddr.accessMbr.baseType = QRAMBT_TEMP;
    operand->getAddr.accessMbr.dataType = amDataType;
    operand->getAddr.accessMbr.baseTemp = temp;
    operand->getAddr.accessMbr.offset = offset;
    return operand;
}

QRAccessMbrOffset *QRNewAccessMemOffsetVal(int val)
{
    QRAccessMbrOffset *amOffset;

    amOffset = QRMAlloc(sizeof (*amOffset));
    amOffset->offsetType = QRAMOT_VAL;
    amOffset->valOffset = val;
    return amOffset;
}

QRAccessMbrOffset *QRNewAccessMemOffsetId(Domain *domain, Type *dataType, const Token *token)
{
    QRAccessMbrOffset *amOffset;

    amOffset = QRMAlloc(sizeof (*amOffset));
    amOffset->offsetType = QRAMOT_ID;
    amOffset->idOffset.domain = domain;
    amOffset->idOffset.id.dataType = dataType;
    amOffset->idOffset.id.token = token;
    return amOffset;
}

QRAccessMbrOffset *QRNewAccessMemOffsetTemp(Domain *domain, Temp *temp)
{
    QRAccessMbrOffset *amOffset;

    amOffset = QRMAlloc(sizeof (*amOffset));
    amOffset->offsetType = QRAMOT_TEMP;
    amOffset->tempOffset.domain = domain;
    amOffset->tempOffset.temp = temp;
    return amOffset;
}

static QRAccessMbrOffset *QRAMOffsetFromConstExpr(CValue *cValue)
{
    switch (cValue->type) {
    case CVT_IDIGIT:
        return QRNewAccessMemOffsetVal(cValue->token->digit);
    case CVT_TRUE:
        return QRNewAccessMemOffsetVal(1);
    case CVT_FALSE:
        return QRNewAccessMemOffsetVal(0);
    default:
        PrErr("cValue->type error: %d", cValue->type);
        exit(-1);
    }
}

static QRAccessMbrOffset *QRAMOffsetFromLValueExpr(Domain *domain, LValue *lvalue)
{
    if (lvalue->dims == NULL) {
        return QRNewAccessMemOffsetId(domain, lvalue->type, lvalue->id);
    } else {
        PrErr("lvalue->dims error: %p", lvalue->dims);
        exit(-1);
    }
}

static QRAccessMbrOffset *QRAMOffsetFromIdExpr(Domain *domain, Id *idExpr)
{
    return QRNewAccessMemOffsetId(domain, idExpr->type, idExpr->token);
}

static QRAccessMbrOffset *QRAMOffsetFromTempExpr(Domain *domain, Temp *temp)
{
    return QRNewAccessMemOffsetTemp(domain, temp);
}

static QRAccessMbrOffset *QRAMOffsetFromConsSpcExpr(CValueSpc *cValueSpc)
{
    switch (cValueSpc->type) {
    case CVTS_IDIGIT:
        return QRNewAccessMemOffsetVal(cValueSpc->iValue);
    case CVTS_TRUE:
        return QRNewAccessMemOffsetVal(1);
    case CVTS_FALSE:
        return QRNewAccessMemOffsetVal(0);
    default:
        PrErr("cValue->type error: %d", cValueSpc->type);
        exit(-1);
    }
}

static QRAccessMbrOffset *QRAMOffsetFromExpr(Expr *expr)
{
    switch (expr->type) {
    case ET_CONST:
        return QRAMOffsetFromConstExpr(&expr->cValue);
    case ET_TEMP:
        return QRAMOffsetFromTempExpr(expr->domain, &expr->temp);
    case ET_CONST_SPC:
        return QRAMOffsetFromConsSpcExpr(&expr->cValueSpc);
    case ET_ID:
        return QRAMOffsetFromIdExpr(expr->domain, &expr->id);
    default:
        PrErr("expr->type error: %d", expr->type);
        exit(-1);
    }
}

static QROperand *QROperandFromConstExpr(Domain *domain, CValue *cValue)
{
    switch (cValue->type) {
    case CVT_IDIGIT:
        return QRNewValIntOperand(domain, cValue->token->digit);
    case CVT_FDIGIT:
        return QRNewValFloatOperand(domain, cValue->token->fDigit);
    case CVT_TRUE:
        return QRNewValIntOperand(domain, 1);
    case CVT_FALSE:
        return QRNewValIntOperand(domain, 0);
    default:
        PrErr("cValue->type error: %d", cValue->type);
        exit(-1);
    }
}

static QROperand *QROperandFromLValueExpr(Domain *domain, LValue *lValue)
{
    if (lValue->dims == NULL) {
        return QRNewIdOperand(domain, lValue->type, lValue->id);
    } else {
        if (lValue->dims->nextDims == NULL) {
            return QRNewAccessMemOperandIdBase(domain, lValue->dims->type, lValue->id, QRAMOffsetFromExpr(lValue->dims->idx), QRAMT_DIRECT, lValue->type);
        } else {
            PrErr("lValue->dims->nextDims error: %p", lValue->dims->nextDims);
            exit(-1);
        }
    }
}

static QROperand *QROperandFromIdExpr(Domain *domain, Id *id)
{
    return QRNewIdOperand(domain, id->type, id->token);
}

static QROperand *QROperandFromTempExpr(Domain *domain, Temp *temp)
{
    return QRNewTempOperand(domain, temp);
}

static QROperand *QROperandFromConstSpcExpr(Domain *domain, CValueSpc *cValueSpc)
{
    switch (cValueSpc->type) {
    case CVTS_IDIGIT:
        return QRNewValIntOperand(domain, cValueSpc->iValue);
    case CVTS_FDIGIT:
        return QRNewValFloatOperand(domain, cValueSpc->fValue);
    case CVTS_TRUE:
        return QRNewValIntOperand(domain, 1);
    case CVTS_FALSE:
        return QRNewValIntOperand(domain, 0);
    default:
        PrErr("cValueSpc->type error: %d", cValueSpc->type);
        exit(-1);
    }
}

static QROperand *QROperandFromMemAccessExpr(Domain *domain, AccessMbr *memAccess)
{
    if (memAccess->type == AMTYPE_DIRECT) {
        if (memAccess->base->type == ET_ID) {
            return QRNewAccessMemOperandIdBase(domain, memAccess->memtype, memAccess->base->id.token,
                                               QRAMOffsetFromExpr(memAccess->memOffset), QRAMT_DIRECT, memAccess->base->id.type);
        } else {
            PrErr("memAccess->base->type error: %d", memAccess->base->type);
            exit(-1);
        }
    } else if (memAccess->type == AMTYPE_INDIRECT) {
        if (memAccess->base->type == ET_ID) {
            return QRNewAccessMemOperandIdBase(domain, memAccess->memtype, memAccess->base->id.token,
                                               QRAMOffsetFromExpr(memAccess->memOffset), QRAMT_INDIRECT, memAccess->base->id.type);
        } else if (memAccess->base->type == ET_TEMP) {
            return QRNewAccessMemOperandTempBase(domain, memAccess->memtype, &memAccess->base->temp,
                                               QRAMOffsetFromExpr(memAccess->memOffset), QRAMT_INDIRECT);
        } else {
            PrErr("memAccess->base->type error: %d", memAccess->base->type);
            exit(-1);
        }
    } else {
        PrErr("memAccess->type error: %d", memAccess->type);
        exit(-1);
    }
}

QROperand *QRNewOpMemVarOperand(Domain *domain, Type *memType, const Token *token, Type *idType)
{
    QRAccessMbrOffset *offset;

    offset = QRNewAccessMemOffsetVal(0);
    return QRNewAccessMemOperandIdBase(domain, memType, token, offset, QRAMT_INDIRECT, idType);
}

QROperand *QRNewOpMemTempOperand(Domain *domain, Type *memType, Temp *temp)
{
    QRAccessMbrOffset *offset;

    offset = QRNewAccessMemOffsetVal(0);
    return QRNewAccessMemOperandTempBase(domain, memType, temp, offset, QRAMT_INDIRECT);
}

static QROperand *QROperandFromRefPointerExpr(Domain *domain, UnaryOp *unaryOp)
{
    if (unaryOp->expr->type == ET_ID) {
        return QRNewOpMemVarOperand(domain, unaryOp->type, unaryOp->expr->id.token, unaryOp->expr->id.type);
    } else if(unaryOp->expr->type == ET_TEMP) {
        return QRNewOpMemTempOperand(domain, unaryOp->type, &unaryOp->expr->temp);
    } else {
        PrErr("unaryOp->expr->type error: %d", unaryOp->expr->type);
        exit(-1);
    }
}

static QROperand *QROperandFromGetAddrExpr(Domain *domain, UnaryOp *unaryOp)
{
    if (unaryOp->expr->type == ET_ID) {
        return QRNewGetAddrIdOperand(domain, unaryOp->expr->id.type, unaryOp->expr->id.token, unaryOp->type);
    } else if (unaryOp->expr->type == ET_ACCESS_MBR) {
        Expr *maExpr = unaryOp->expr;

        if (maExpr->accessMbr.type == AMTYPE_DIRECT) {
            if (maExpr->accessMbr.base->type == ET_ID) {
                return QRNewGetAddrAccessMemOperand(domain, maExpr->accessMbr.memtype,
                                                    maExpr->accessMbr.base->id.token,
                                                    QRAMOffsetFromExpr(maExpr->accessMbr.memOffset),
                                                    QRAMT_DIRECT, unaryOp->type);
            } else {
                PrErr("maExpr->memAccess.base->type error: %d", maExpr->accessMbr.base->type);
                exit(-1);
            }
        } else if (maExpr->accessMbr.type == AMTYPE_INDIRECT) {
            if (maExpr->accessMbr.base->type == ET_ID) {
                return QRNewGetAddrAccessMemOperand(domain, maExpr->accessMbr.memtype,
                                                    maExpr->accessMbr.base->id.token,
                                                    QRAMOffsetFromExpr(maExpr->accessMbr.memOffset),
                                                    QRAMT_INDIRECT, unaryOp->type);
            } else if (maExpr->accessMbr.base->type == ET_TEMP) {
                return QRNewGetAddrAccessMemOperandTempBase(domain, maExpr->accessMbr.memtype,
                                                            &maExpr->accessMbr.base->temp,
                                                            QRAMOffsetFromExpr(maExpr->accessMbr.memOffset),
                                                            QRAMT_INDIRECT, unaryOp->type);
            } else {
                PrErr("maExpr->memAccess.base->type error: %d", maExpr->accessMbr.base->type);
                exit(-1);
            }
        } else {
            PrErr("maExpr->memAccess.type error: %d", maExpr->accessMbr.type);
            exit(-1);
        }
    } else if (unaryOp->expr->type == ET_ID) {
        return QRNewGetAddrIdOperand(domain, unaryOp->expr->id.type, unaryOp->expr->id.token, unaryOp->type);
    } else {
        PrErr("unaryOp->expr->type error: %d", unaryOp->expr->type);
        exit(-1);
    }
}

static QROperand *QROperandFromTypeCastExpr(Domain *domain, UnaryOp *unaryOp)
{
    (void)domain, (void)unaryOp;
    PrErr("error");
    exit(-1);
}

QROperand *QROperandFromExpr(Expr *expr)
{
    if (!expr) {
        PrErr("expr: %p", expr);
        exit(-1);
    }
    switch (expr->type) {
    case ET_CONST:
        return QROperandFromConstExpr(expr->domain, &expr->cValue);
    case ET_TEMP:
        return QROperandFromTempExpr(expr->domain, &expr->temp);
    case ET_CONST_SPC:
        return QROperandFromConstSpcExpr(expr->domain, &expr->cValueSpc);
    case ET_ACCESS_MBR:
        return QROperandFromMemAccessExpr(expr->domain, &expr->accessMbr);
    case ET_REF_POINTER:
        return QROperandFromRefPointerExpr(expr->domain, &expr->unaryOp);
    case ET_GET_ADDR:
        return QROperandFromGetAddrExpr(expr->domain, &expr->unaryOp);
    case ET_TYPE_CAST:
        return QROperandFromTypeCastExpr(expr->domain, &expr->unaryOp);
    case ET_ID:
        return QROperandFromIdExpr(expr->domain, &expr->id);
    default:
        PrErr("expr->type error: %d", expr->type);
        exit(-1);
    }

}

#if 0
static Type *QRGetDomainEntryDataType(Domain *domain, const Token *token, Type *type)
{
    DomainEntry *entry;

    entry = EnvDomainGetEntry(domain, token);
    if (entry->funArgFlag == 0) {
        return type;
    } else {
        if (type->type == TT_ARRAY) {
            return ATreeNewPointerType(type->array.type);
        } else if (type->type == TT_FUNCTION) {
            return ATreeNewPointerType(type);
        } else {
            return type;
        }
    }
}
#endif

Type *QROperandDataType(QROperand *operand)
{
    switch (operand->type) {
    case QROT_ID:
        return operand->id.dataType;
    case QROT_ACCESS_MBR:
        return operand->accessMbr.dataType;
    case QROT_VAL:
        if (operand->val.type == QRVT_INT) {
            return bison->env->intType;
        } else if (operand->val.type == QRVT_FLOAT) {
            return bison->env->floatType;
        } else {
            PrErr("operand->val.type error: %d", operand->val.type);
            exit(-1);
        }
    case QROT_TEMP:
        return operand->temp->type;
    case QROT_GET_ADDR:
        return operand->getAddr.dataType;
    default:
        PrErr("operand->type error: %d", operand->type);
        exit(-1);
    }
}
