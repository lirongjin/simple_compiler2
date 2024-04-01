#ifndef __QUADRUPLE_H__
#define __QUADRUPLE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"
#include "abstract_tree.h"

typedef struct _QROperand QROperand;

/*四元式操作码。*/
typedef enum {
    QROC_ADD,           /*加法*/
    QROC_SUB,           /*减法*/
    QROC_MUL,           /*乘法*/
    QROC_DIV,           /*除法*/
    QROC_MOD,           /*求余*/
    QROC_JUMP,          /*绝对跳转*/
    QROC_TRUE_JUMP,     /*真时跳转*/
    QROC_FALSE_JUMP,    /*假时跳转*/
    QROC_ASSIGN,        /*赋值*/
    QROC_EQ,            /*相等*/
    QROC_NE,            /*不等*/
    QROC_LT,            /*小于*/
    QROC_LE,            /*小于等于*/
    QROC_GT,            /*大于*/
    QROC_GE,            /*大于等于*/
    QROC_RETURN_VOID,   /*return;*/
    QROC_RETURN_VALUE,  /*return E;*/
    QROC_FUN_PARAM,     /*param E;*/
    QROC_FUN_CALL,      /*t = call fun, cnt*/
    QROC_FUN_CALL_NRV,  /*call fun, cnt*/
    QROC_FUN_START,     /*函数定义开始标志*/
    QROC_FUN_END,       /*函数定义结束*/
    QROC_BLK_START,     /*块开始*/
    QROC_BLK_END,       /*块结束*/
    QROC_PROGRAM_START, /*程序开始*/
    QROC_PROGRAM_END,   /*程序结束*/
} QROpCode;

/*四元式*/
typedef struct _QuadRuple {
    QROpCode op;        /*操作码*/
    union {
        struct {
            QROperand *arg1;         /*参数一*/
            QROperand *arg2;         /*参数二*/
            QROperand *result;       /*结果*/
        } binOp;                /*二目操作*/
        struct {
            QROperand *arg1;         /*参数一*/
            QROperand *result;       /*结果*/
        } unaryOp;              /*单目操作*/
        struct {
            size_t dstLabel;    /*目标标号*/
        } uncondJump;           /*绝对跳转*/
        struct {
            Expr *cond;         /*条件表达式*/
            size_t dstLabel;    /*目标标号*/
        } condJump;             /*条件跳转*/
        struct {
            QROperand *expr;
            Domain *domain;
        } returnObj;
        struct {
            QROperand *expr;
        } funParam;
        struct {
            const Token *token;
            size_t paraCnt;
            QROperand *result;
            Domain *domain;
        } funCall;
        struct {
            const Token *id;
            FunType *funType;
        } funStartFlag;
        struct {
            const Token *id;
            Domain *domain;
        } funEnd;
        struct {
            Domain *domain;
        } blkStart;
        struct {
            Domain *domain;
        } blkEnd;
        struct {
            Domain *domain;
        } programStart;
        struct {
            Domain *domain;
        } programEnd;
    };
} QuadRuple;

/*记录*/
typedef struct _QRRecord {
    QuadRuple *qrArr;   /*四元式指令数组。*/
    size_t idx;         /*四元式指令数组索引。*/
    size_t size;        /*为四元式指令数组分配的内存容量，单位：字节。*/
} QRRecord;

QRRecord *QRRecordAlloc(void);
void QRRecordFree(QRRecord *record);
int QRRecordAddBin(QRRecord *record, QROpCode op, QROperand *arg1, QROperand *arg2, QROperand *result);
int QRRecordAddUnary(QRRecord *record, QROpCode op, QROperand *arg1, QROperand *result);
int QRRecordAddUncondJump(QRRecord *record, size_t label);
int QRRecordAddCondJump(QRRecord *record, QROpCode op, Expr *cond, size_t label);
int QRRecordAddReturnVoid(QRRecord *record, Domain *domian);
int QRRecordAddReturnValue(QRRecord *record, QROperand *expr, Domain *domain);
int QRRecordAddFunParam(QRRecord *record, QROperand *expr);
int QRRecordAddFunCall(QRRecord *record, const Token *token, size_t paraCnt, QROperand *result, Domain *domain);
int QRRecordAddFunCallNrv(QRRecord *record, const Token *token, size_t paraCnt, Domain *domain);
int QRRecordAddFunStart(QRRecord *record, const Token *id, FunType *funType);
int QRRecordAddFunEnd(QRRecord *record, const Token *id, Domain *domain);
int QRRecordAddBlkStart(QRRecord *record, Domain *domain);
int QRRecordAddBlkEnd(QRRecord *record, Domain *domain);
int QRRecordAddProgramStart(QRRecord *record, Domain *domain);
int QRRecordAddProgramEnd(QRRecord *record, Domain *domain);

void QRPrintRecord(const QRRecord *record);

typedef struct {
    Type *dataType;
    const Token *token;
} QRId;

typedef enum {
    QRAMOT_VAL,
    QRAMOT_ID,
    QRAMOT_TEMP,
} QRAMOffsetType;

typedef struct {
    QRAMOffsetType offsetType;
    union {
        int valOffset;
        struct {
            Domain *domain;
            QRId id;
        } idOffset;
        struct {
            Domain *domain;
            Temp *temp;
        } tempOffset;
    };
} QRAccessMbrOffset;

typedef enum {
    QRAMT_DIRECT,
    QRAMT_INDIRECT,
} QRAccessMbrType;

typedef enum {
    QRAMBT_ID,
    QRAMBT_TEMP,
} QRAccessMbrBaseType;

typedef struct {
    QRAccessMbrType type;
    QRAccessMbrBaseType baseType;
    union {
        QRId baseId;
        Temp *baseTemp;
    };
    Type *dataType;     /*被访问成员的类型*/
    QRAccessMbrOffset *offset;
} QRAccessMbr;

typedef enum {
    QRVT_INT,
    QRVT_FLOAT,
} QRValueType;

typedef struct {
    QRValueType type;
    union {
        int ival;
        float fval;
    };
} QRValue;

typedef enum {
    QROMT_ID,
    QROMT_ACCESS_MEM,
    QROMT_TEMP,
} QROpMemType;

typedef enum {
    QRGAT_ID,
    QRGAT_ACCESS_MEM,
} QRGetAddrType;

typedef struct {
    QRGetAddrType type;
    Type *dataType;     /*取地址后的类型*/
    union {
        QRId id;
        QRAccessMbr accessMbr;
    };
} QRGetAddr;

typedef enum {
    QROT_ID,
    QROT_ACCESS_MBR,    /*访问成员*/
    QROT_VAL,
    QROT_TEMP,
    QROT_OP_MEM,        /*操作内存*/
    QROT_GET_ADDR,
} QROperandType;

typedef struct _QROperand {
    QROperandType type;
    Domain *domain;
    union {
        QRId id;
        QRAccessMbr accessMbr;
        QRValue val;
        Temp *temp;
        QRGetAddr getAddr;
    };
} QROperand;

QROperand *QRNewIdOperand(Domain *domain, Type *dataType, const Token *token);
QROperand *QRNewAccessMemOperandIdBase(Domain *domain, Type *mbrDataType, const Token *token, QRAccessMbrOffset *offset, QRAccessMbrType accessType, Type *baseDataType);
QROperand *QRNewAccessMemOperandTempBase(Domain *domain, Type *mbrDataType, Temp *temp, QRAccessMbrOffset *offset, QRAccessMbrType accessType);
QROperand *QRNewValIntOperand(Domain *domain, int val);
QROperand *QRNewValFloatOperand(Domain *domain, float val);
QROperand *QRNewTempOperand(Domain *domain, Temp *temp);

QROperand *QRNewGetAddrIdOperand(Domain *domain, Type *idDataType, const Token *token, Type *dataType);
QROperand *QRNewGetAddrAccessMemOperand(Domain *domain, Type *amDataType, const Token *token, QRAccessMbrOffset *offset, QRAccessMbrType accessType, Type *dataType);
QROperand *QRNewGetAddrAccessMemOperandTempBase(Domain *domain, Type *amDataType, Temp *temp, QRAccessMbrOffset *offset, QRAccessMbrType accessType, Type *dataType);

QRAccessMbrOffset *QRNewAccessMemOffsetVal(int val);
QRAccessMbrOffset *QRNewAccessMemOffsetId(Domain *domain, Type *dataType, const Token *token);
QRAccessMbrOffset *QRNewAccessMemOffsetTemp(Domain *domain, Temp *temp);

QROperand *QROperandFromExpr(Expr *expr);
Type *QROperandDataType(QROperand *operand);

#ifdef __cplusplus
}
#endif

#endif /*__QUADRUPLE_H__*/

