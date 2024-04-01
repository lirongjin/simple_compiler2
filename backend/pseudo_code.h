#ifndef __PSEUDO_CODE_H__
#define __PSEUDO_CODE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"
#include "lex_words.h"

typedef enum {
    PILDT_1BYTE,
    PILDT_2BYTE,
    PILDT_4BYTE,
    PILDT_FLOAT,
} PILdType;

typedef enum {
    PISTT_1BYTE,
    PISTT_2BYTE,
    PISTT_4BYTE,
    PISTT_FLOAT,
} PIStType;

typedef enum {
    PIBCT_EQ,
    PIBCT_NE,
    PIBCT_GT,
    PIBCT_GE,
    PIBCT_LT,
    PIBCT_LE,
} PIBCondType;

typedef enum {
    PIOT_IMM,
    PIOT_REG,
} PIOffsetType;

typedef struct {
    PILdType type;
    uint32_t pseudoFlag: 1;
    uint8_t dstRegId;
    union {
        struct {
            uint8_t memRegId;
            PIOffsetType offsetType;
            union {
                int16_t imm;
                uint8_t reg;
            } offset;
        } machine;
        struct {
            int32_t data;
        } pseudo;
    };
} PILd;

typedef struct {
    PIStType type;
    uint8_t srcRegId;
    uint8_t memRegId;
    PIOffsetType offsetType;
    union {
        int16_t imm;
        uint8_t reg;
    } offset;
} PISt;

typedef enum {
    PIMT_REG,
    PIMT_IMM,
} PIMovType;

typedef struct {
    uint8_t dstRegId;
    PIMovType type;
    union {
        uint8_t srcReg;
        int32_t imm;
    };
} PIMov;

typedef struct _FunType FunType;

typedef struct {
    const Token *funToken;
    FunType *funType;
    uint32_t dstAddr;
    uint32_t curAddr;
} PICall;

typedef struct {
    uint8_t pcAddrFlag;
    uint32_t icDstAddr;
    uint32_t pcDstAddr;
    uint32_t curAddr;
} PIB;

typedef enum {
    PIAS2T_IMM,
    PIAS2T_REG,
} PIArithSrc2Type;

typedef struct {
    uint8_t dstRegId;
    uint8_t src1RegId;
    uint8_t src2Type;
    int32_t src2;
} PIArith;

typedef struct {
    PIBCondType type;
    uint8_t testRegID;
    uint32_t icDstAddr;
    uint32_t pcDstAddr;
    uint32_t curAddr;
} PIBCond;

typedef struct {
    uint32_t data;
} PILDData;

typedef struct {
    uint32_t vArgSize;  //函数变参大小
} PISetActSp;

typedef enum {
    PIT_LD,
    PIT_ST,
    PIT_MOV,
    PIT_CALL,
    PIT_B,
    PIT_ADD,
    PIT_SUB,
    PIT_MUL,
    PIT_DIV,
    PIT_MOD,
    PIT_BCOND,
    PIT_LDDATA, //ldr rx [ry]   被加载的数据
    PIT_HALT,
    PIT_SET_ACTSP,  //设置活动记录栈基地址伪指令
} PInstType;

typedef struct {
    PInstType type;
    union {
        PILd piLd;
        PISt piSt;
        PIMov piMov;
        PICall piCall;
        PIB piB;
        PIArith piArith;
        PIBCond piBCond;
        PILDData piLdData;
        PISetActSp piSetActSp;
    };
} PInstruct;

typedef struct _PInstructArr {
    PInstruct *insArr;
    uint32_t idx;
    uint32_t memSize;
    size_t dataStart;
} PInstructArr;

PInstructArr *PCAllocInsArr(void);
void PCFreeInsArr(PInstructArr *piArr);

int PCInsArrAddLdBPseudo(PInstructArr *piArr, uint8_t dstRegId, int32_t data);
int PCInsArrAddLdHPseudo(PInstructArr *piArr, uint8_t dstRegId, int32_t data);
int PCInsArrAddLdWPseudo(PInstructArr *piArr, uint8_t dstRegId, int32_t data);
int PCInsArrAddLdFPseudo(PInstructArr *piArr, uint8_t dstRegId, int32_t data);
int PCInsArrAddLd(PInstructArr *piArr, PILdType type, uint8_t dstRegId, uint8_t memRegId, int16_t offset);
int PCInsArrAddLdOffsetReg(PInstructArr *piArr, PILdType type, uint8_t dstRegId, uint8_t memRegId, uint8_t offsetRegId);
int PCInsArrAddLdB(PInstructArr *piArr, uint8_t dstRegId, uint8_t memRegId, int16_t offset);
int PCInsArrAddLdH(PInstructArr *piArr, uint8_t dstRegId, uint8_t memRegId, int16_t offset);
int PCInsArrAddLdW(PInstructArr *piArr, uint8_t dstRegId, uint8_t memRegId, int16_t offset);
int PCInsArrAddLdF(PInstructArr *piArr, uint8_t dstRegId, uint8_t memRegId, int16_t offset);
int PCInsArrAddSt(PInstructArr *piArr, PIStType type, uint8_t srcRegId, uint8_t memRegId, int16_t offset);
int PCInsArrAddStB(PInstructArr *piArr, uint8_t srcRegId, uint8_t memRegId, int16_t offset);
int PCInsArrAddStH(PInstructArr *piArr, uint8_t srcRegId, uint8_t memRegId, int16_t offset);
int PCInsArrAddStW(PInstructArr *piArr, uint8_t srcRegId, uint8_t memRegId, int16_t offset);
int PCInsArrAddStF(PInstructArr *piArr, uint8_t srcRegId, uint8_t memRegId, int16_t offset);
int PCInsArrAddStOffsetReg(PInstructArr *piArr, PIStType type, uint8_t srcRegId, uint8_t memRegId, uint8_t offsetRegId);
int PCInsArrAddMov(PInstructArr *piArr, uint8_t dstRegId, int32_t immData);
int PCInsArrAddMovReg(PInstructArr *piArr, uint8_t dstRegId, uint8_t srcReg);
int PCInsArrAddCall(PInstructArr *piArr, const Token *funToken, FunType *funType, uint32_t curAddr);
int PCInsArrAddB(PInstructArr *piArr, uint32_t icDstAddr, uint32_t curAddr);
int PCInsArrAddArith(PInstructArr *piArr, PInstType type, uint8_t dstRegID, uint8_t src1RegId, int32_t src2, PIArithSrc2Type src2Type);
int PCInsArrAddAdd(PInstructArr *piArr, uint8_t dstRegID, uint8_t src1RegId, uint8_t src2RegId);
int PCInsArrAddSub(PInstructArr *piArr, uint8_t dstRegID, uint8_t src1RegId, uint8_t src2RegId);
int PCInsArrAddMul(PInstructArr *piArr, uint8_t dstRegID, uint8_t src1RegId, uint8_t src2RegId);
int PCInsArrAddDiv(PInstructArr *piArr, uint8_t dstRegID, uint8_t src1RegId, uint8_t src2RegId);
int PCInsArrAddMod(PInstructArr *piArr, uint8_t dstRegID, uint8_t src1RegId, uint8_t src2RegId);
int PCInsArrAddBCond(PInstructArr *piArr, PIBCondType type, uint8_t testRegId, uint32_t icDstAddr, uint32_t curAddr);
int PCInsArrAddLdData(PInstructArr *piArr, uint32_t data);
int PCInsArrAddHalt(PInstructArr *piArr);
int PCInsArrAddSetActSp(PInstructArr *piArr);

void PCInsArrPrint(PInstructArr *piArr);
void PCInstArrSetDataStart(PInstructArr *piArr, size_t dataStart);

#ifdef __cplusplus
}
#endif

#endif /*__PSEUDO_CODE_H__*/

