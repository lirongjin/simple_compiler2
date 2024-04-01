#include "pseudo_code.h"
#include "abstract_tree.h"
#include "lex_words.h"
#include "cpl_mm.h"
#include "cpl_debug.h"
#include "stdlib.h"
#include "string.h"
#include "cpl_common.h"

#define PrErr(...)      Pr(__FILE__, __LINE__, __FUNCTION__, "error", __VA_ARGS__)

#define PCDEBUG
#ifdef PCDEBUG
#define PrDbg(...)      Pr(__FILE__, __LINE__, __FUNCTION__, "debug", __VA_ARGS__)
#else
#define PrDbg(...)
#endif

#define PCEmit(...)    printf(__VA_ARGS__), printf("\n");

#define REG_CNT         (17)
#define REG_STR(id)     ((id >= 0 && id < REG_CNT) ? regStr[id] : "R?")

static void *PCMalloc(size_t size)
{
    void *p;

    p = CplAlloc(size);
    if (!p) {
        PrErr("no memory");
        exit(-1);
    }
    return p;
}

static void PCFree(void *p)
{
    CplFree(p);
}

PInstructArr *PCAllocInsArr(void)
{
    PInstructArr *piArr;

    piArr = PCMalloc(sizeof (*piArr));
    piArr->insArr = NULL;
    piArr->idx = 0;
    piArr->memSize = 0;
    piArr->dataStart = 0;
    return piArr;
}

void PCFreeInsArr(PInstructArr *piArr)
{
    if (!piArr)
        return;
    if (piArr->insArr)
        PCFree(piArr->insArr);
    PCFree(piArr);
}

static PInstruct *PCInsArrAlloc(PInstructArr *piArr)
{
    if (piArr->memSize == 0) {
        piArr->memSize = 800 * sizeof (PInstruct);
        piArr->insArr = PCMalloc(piArr->memSize);
    } else {
        if (piArr->memSize <= sizeof (PInstruct) * piArr->idx) {
            PInstruct *newInstructs;
            size_t newSize;

            if (piArr->memSize < 256 * sizeof (PInstruct)) {
                newSize = piArr->memSize * 2;
            } else {
                newSize = piArr->memSize + 256 * sizeof (PInstruct);
            }
            newInstructs = PCMalloc(newSize);
            memcpy(newInstructs, piArr->insArr, piArr->idx * sizeof (PInstruct));
            PCFree(piArr->insArr);
            piArr->insArr = newInstructs;
            piArr->memSize = newSize;
        }
    }
    return &piArr->insArr[piArr->idx++];
}

static int PCInsArrAddLdPseudo(PInstructArr *piArr, PILdType type, uint8_t dstRegId, int32_t data)
{
    PInstruct *pIns;
    PILd *piLd;

    pIns = PCInsArrAlloc(piArr);
    pIns->type = PIT_LD;
    piLd = &pIns->piLd;
    piLd->type = type;
    piLd->dstRegId = dstRegId;
    piLd->pseudoFlag = 1;
    piLd->pseudo.data = data;
    return 0;
}

int PCInsArrAddLdBPseudo(PInstructArr *piArr, uint8_t dstRegId, int32_t data)
{
    return PCInsArrAddLdPseudo(piArr, PILDT_1BYTE, dstRegId, data);
}

int PCInsArrAddLdHPseudo(PInstructArr *piArr, uint8_t dstRegId, int32_t data)
{
    return PCInsArrAddLdPseudo(piArr, PILDT_2BYTE, dstRegId, data);
}

int PCInsArrAddLdWPseudo(PInstructArr *piArr, uint8_t dstRegId, int32_t data)
{
    return PCInsArrAddLdPseudo(piArr, PILDT_4BYTE, dstRegId, data);
}

int PCInsArrAddLdFPseudo(PInstructArr *piArr, uint8_t dstRegId, int32_t data)
{
    return PCInsArrAddLdPseudo(piArr, PILDT_FLOAT, dstRegId, data);
}

int PCInsArrAddLd(PInstructArr *piArr, PILdType type, uint8_t dstRegId, uint8_t memRegId, int16_t offset)
{
    PInstruct *pIns;
    PILd *piLd;

    pIns = PCInsArrAlloc(piArr);
    pIns->type = PIT_LD;
    piLd = &pIns->piLd;
    piLd->type = type;
    piLd->pseudoFlag = 0;
    piLd->dstRegId = dstRegId;
    piLd->machine.memRegId = memRegId;
    piLd->machine.offset.imm = offset;
    piLd->machine.offsetType = PIOT_IMM;
    return 0;
}

int PCInsArrAddLdB(PInstructArr *piArr, uint8_t dstRegId, uint8_t memRegId, int16_t offset)
{
    return PCInsArrAddLd(piArr, PILDT_1BYTE, dstRegId, memRegId, offset);
}

int PCInsArrAddLdH(PInstructArr *piArr, uint8_t dstRegId, uint8_t memRegId, int16_t offset)
{
    return PCInsArrAddLd(piArr, PILDT_2BYTE, dstRegId, memRegId, offset);
}

int PCInsArrAddLdW(PInstructArr *piArr, uint8_t dstRegId, uint8_t memRegId, int16_t offset)
{
    return PCInsArrAddLd(piArr, PILDT_4BYTE, dstRegId, memRegId, offset);
}

int PCInsArrAddLdF(PInstructArr *piArr, uint8_t dstRegId, uint8_t memRegId, int16_t offset)
{
    return PCInsArrAddLd(piArr, PILDT_FLOAT, dstRegId, memRegId, offset);
}

int PCInsArrAddLdOffsetReg(PInstructArr *piArr, PILdType type, uint8_t dstRegId, uint8_t memRegId, uint8_t offsetRegId)
{
    PInstruct *pIns;
    PILd *piLd;

    pIns = PCInsArrAlloc(piArr);
    pIns->type = PIT_LD;
    piLd = &pIns->piLd;
    piLd->type = type;
    piLd->pseudoFlag = 0;
    piLd->dstRegId = dstRegId;
    piLd->machine.memRegId = memRegId;
    piLd->machine.offset.reg = offsetRegId;
    piLd->machine.offsetType = PIOT_REG;
    return 0;
}

int PCInsArrAddSt(PInstructArr *piArr, PIStType type, uint8_t srcRegId, uint8_t memRegId, int16_t offset)
{
    PInstruct *pIns;
    PISt *piSt;

    pIns = PCInsArrAlloc(piArr);
    pIns->type = PIT_ST;
    piSt = &pIns->piSt;
    piSt->type = type;
    piSt->srcRegId = srcRegId;
    piSt->memRegId = memRegId;
    piSt->offsetType = PIOT_IMM;
    piSt->offset.imm = offset;
    return 0;
}

int PCInsArrAddStB(PInstructArr *piArr, uint8_t srcRegId, uint8_t memRegId, int16_t offset)
{
    return PCInsArrAddSt(piArr, PISTT_1BYTE, srcRegId, memRegId, offset);
}

int PCInsArrAddStH(PInstructArr *piArr, uint8_t srcRegId, uint8_t memRegId, int16_t offset)
{
    return PCInsArrAddSt(piArr, PISTT_2BYTE, srcRegId, memRegId, offset);
}

int PCInsArrAddStW(PInstructArr *piArr, uint8_t srcRegId, uint8_t memRegId, int16_t offset)
{
    return PCInsArrAddSt(piArr, PISTT_4BYTE, srcRegId, memRegId, offset);
}

int PCInsArrAddStF(PInstructArr *piArr, uint8_t dstRegId, uint8_t memRegId, int16_t offset)
{
    return PCInsArrAddSt(piArr, PISTT_FLOAT, dstRegId, memRegId, offset);
}

int PCInsArrAddStOffsetReg(PInstructArr *piArr, PIStType type, uint8_t srcRegId, uint8_t memRegId, uint8_t offsetRegId)
{
    PInstruct *pIns;
    PISt *piSt;

    pIns = PCInsArrAlloc(piArr);
    pIns->type = PIT_ST;
    piSt = &pIns->piSt;
    piSt->type = type;
    piSt->srcRegId = srcRegId;
    piSt->memRegId = memRegId;
    piSt->offsetType = PIOT_REG;
    piSt->offset.reg = offsetRegId;
    return 0;
}

int PCInsArrAddMov(PInstructArr *piArr, uint8_t dstRegId, int32_t immData)
{
    PInstruct *pIns;
    PIMov *piMov;

    pIns = PCInsArrAlloc(piArr);
    pIns->type = PIT_MOV;
    piMov = &pIns->piMov;
    piMov->dstRegId = dstRegId;
    piMov->type = PIMT_IMM;
    piMov->imm = immData;
    return 0;
}

int PCInsArrAddMovReg(PInstructArr *piArr, uint8_t dstRegId, uint8_t srcReg)
{
    PInstruct *pIns;
    PIMov *piMov;

    pIns = PCInsArrAlloc(piArr);
    pIns->type = PIT_MOV;
    piMov = &pIns->piMov;
    piMov->dstRegId = dstRegId;
    piMov->type = PIMT_REG;
    piMov->srcReg = srcReg;
    return 0;
}

int PCInsArrAddCall(PInstructArr *piArr, const Token *funToken, FunType *funType, uint32_t curAddr)
{
    PInstruct *pIns;
    PICall *piCall;

    pIns = PCInsArrAlloc(piArr);
    pIns->type = PIT_CALL;
    piCall = &pIns->piCall;
    piCall->funToken = funToken;
    piCall->funType = funType;
    piCall->dstAddr = 0;
    piCall->curAddr = curAddr;
    return 0;
}

int PCInsArrAddB(PInstructArr *piArr, uint32_t icDstAddr, uint32_t curAddr)
{
    PInstruct *pIns;
    PIB *piB;

    pIns = PCInsArrAlloc(piArr);
    pIns->type = PIT_B;
    piB = &pIns->piB;
    piB->pcAddrFlag = 0;
    piB->icDstAddr = icDstAddr;
    piB->pcDstAddr = 0;
    piB->curAddr = curAddr;
    return 0;
}

int PCInsArrAddArith(PInstructArr *piArr, PInstType type, uint8_t dstRegID, uint8_t src1RegId, int32_t src2, PIArithSrc2Type src2Type)
{
    PInstruct *pIns;
    PIArith *piArith;

    pIns = PCInsArrAlloc(piArr);
    pIns->type = type;
    piArith = &pIns->piArith;
    piArith->dstRegId = dstRegID;
    piArith->src1RegId = src1RegId;
    piArith->src2 = src2;
    piArith->src2Type = src2Type;
    return 0;
}

int PCInsArrAddAdd(PInstructArr *piArr, uint8_t dstRegID, uint8_t src1RegId, uint8_t src2RegId)
{
    return PCInsArrAddArith(piArr, PIT_ADD, dstRegID, src1RegId, src2RegId, PIAS2T_REG);
}

int PCInsArrAddSub(PInstructArr *piArr, uint8_t dstRegID, uint8_t src1RegId, uint8_t src2RegId)
{
    return PCInsArrAddArith(piArr, PIT_SUB, dstRegID, src1RegId, src2RegId, PIAS2T_REG);
}

int PCInsArrAddMul(PInstructArr *piArr, uint8_t dstRegID, uint8_t src1RegId, uint8_t src2RegId)
{
    return PCInsArrAddArith(piArr, PIT_MUL, dstRegID, src1RegId, src2RegId, PIAS2T_REG);
}

int PCInsArrAddDiv(PInstructArr *piArr, uint8_t dstRegID, uint8_t src1RegId, uint8_t src2RegId)
{
   return PCInsArrAddArith(piArr, PIT_DIV, dstRegID, src1RegId, src2RegId, PIAS2T_REG);
}

int PCInsArrAddMod(PInstructArr *piArr, uint8_t dstRegID, uint8_t src1RegId, uint8_t src2RegId)
{
    return PCInsArrAddArith(piArr, PIT_MOD, dstRegID, src1RegId, src2RegId, PIAS2T_REG);
}

int PCInsArrAddBCond(PInstructArr *piArr, PIBCondType type, uint8_t testRegId, uint32_t icDstAddr, uint32_t curAddr)
{
    PInstruct *pIns;
    PIBCond *piBCond;

    pIns = PCInsArrAlloc(piArr);
    pIns->type = PIT_BCOND;
    piBCond = &pIns->piBCond;
    piBCond->type = type;
    piBCond->testRegID = testRegId;
    piBCond->icDstAddr = icDstAddr;
    piBCond->pcDstAddr = 0;
    piBCond->curAddr = curAddr;
    return 0;
}

int PCInsArrAddLdData(PInstructArr *piArr, uint32_t data)
{
    PInstruct *pIns;
    PILDData *piLdData;

    pIns = PCInsArrAlloc(piArr);
    pIns->type = PIT_LDDATA;
    piLdData = &pIns->piLdData;
    piLdData->data = data;
    return 0;
}

int PCInsArrAddHalt(PInstructArr *piArr)
{
    PInstruct *pIns;

    pIns = PCInsArrAlloc(piArr);
    pIns->type = PIT_HALT;
    return 0;
}

int PCInsArrAddSetActSp(PInstructArr *piArr)
{
    PInstruct *pIns;

    pIns = PCInsArrAlloc(piArr);
    pIns->type = PIT_SET_ACTSP;
    return 0;
}

const char *regStr[] = {
    "R0",
    "R1",
    "R2",
    "R3",
    "R4",
    "R5",
    "R6",
    "R7",
    "R8",
    "R9",
    "R10",
    "R11",
    "R12",
    "SP",
    "LR",
    "PC",
    "PSR"
};

static void PCLdInsPrint(const PInstruct *pInstruct)
{
    const char *ldStr = "?";
    const PILd *piLd = &pInstruct->piLd;

    switch (piLd->type) {
    case PILDT_1BYTE:
        ldStr = "LDB";
        break;
    case PILDT_2BYTE:
        ldStr = "LDH";
        break;
    case PILDT_4BYTE:
        ldStr = "LDW";
        break;
    case PILDT_FLOAT:
        ldStr = "LDF";
        break;
    }
    if (piLd->pseudoFlag) {
        if (piLd->pseudo.data >= 0) {
            PCEmit("%s, %s, =0x%04x", ldStr, REG_STR(piLd->dstRegId), piLd->pseudo.data)
        } else {
            PCEmit("%s, %s, =%d", ldStr, REG_STR(piLd->dstRegId), piLd->pseudo.data);
        }
    } else {
        if (piLd->machine.offsetType == PIOT_IMM) {
            if (piLd->machine.offset.imm) {
                if (piLd->machine.offset.imm > 0) {
                    PCEmit("%s, %s, [%s, #0x%04x]", ldStr, REG_STR(piLd->dstRegId),
                           REG_STR(piLd->machine.memRegId), piLd->machine.offset.imm);
                } else {
                    PCEmit("%s, %s, [%s, #-0x%04x]", ldStr, REG_STR(piLd->dstRegId),
                           REG_STR(piLd->machine.memRegId), -piLd->machine.offset.imm);
                }

            } else {
                PCEmit("%s, %s, [%s]", ldStr, REG_STR(piLd->dstRegId),
                       REG_STR(piLd->machine.memRegId));
            }
        } else if (piLd->machine.offsetType == PIOT_REG) {
            PCEmit("%s, %s, [%s, %s]", ldStr, REG_STR(piLd->dstRegId),
                   REG_STR(piLd->machine.memRegId), REG_STR(piLd->machine.offset.reg));
        } else {
            PrErr("piLd->machine.offsetType error: %d", piLd->machine.offsetType);
            exit(-1);
        }
    }
}

static void PCStInsPrint(const PInstruct *pInstruct)
{
    const char *ldStr = "?";
    const PISt *piSt = &pInstruct->piSt;

    switch (piSt->type) {
    case PISTT_1BYTE:
        ldStr = "STB";
        break;
    case PISTT_2BYTE:
        ldStr = "STH";
        break;
    case PISTT_4BYTE:
        ldStr = "STW";
        break;
    case PISTT_FLOAT:
        ldStr = "STF";
        break;
    }
    if (piSt->offsetType == PIOT_IMM) {
        if (piSt->offset.imm) {
            if (piSt->offset.imm > 0) {
                PCEmit("%s, %s, [%s, #0x%04x]", ldStr, REG_STR(piSt->srcRegId),
                       REG_STR(piSt->memRegId), piSt->offset.imm);
            } else {
                PCEmit("%s, %s, [%s, #-0x%04x]", ldStr, REG_STR(piSt->srcRegId),
                       REG_STR(piSt->memRegId), -piSt->offset.imm);
            }
        } else {
            PCEmit("%s, %s, [%s]", ldStr, REG_STR(piSt->srcRegId),
                   REG_STR(piSt->memRegId));
        }
    } else if (piSt->offsetType == PIOT_REG) {
        PCEmit("%s %s [%s, %s]", ldStr, REG_STR(piSt->srcRegId),
               REG_STR(piSt->memRegId), REG_STR(piSt->offset.reg));
    } else {
        PrErr("piSt->offsetType error: %d", piSt->offsetType);
        exit(-1);
    }
}

static void PCMovInsPrint(const PInstruct *pInstruct)
{
    const PIMov *piMov;

    piMov = &pInstruct->piMov;
    if (piMov->type == PIMT_IMM) {
        if (piMov->imm >= 0) {
            PCEmit("MOV %s, #0x%04x", REG_STR(piMov->dstRegId), piMov->imm);
        } else {
            PCEmit("MOV %s, #-0x%04x", REG_STR(piMov->dstRegId), -piMov->imm);
        }
    } else if (piMov->type == PIMT_REG) {
        PCEmit("MOV %s, %s", REG_STR(piMov->dstRegId), REG_STR(piMov->srcReg));
    } else {
        PrErr("piMov->type error: %d", piMov->type);
        exit(-1);
    }
}

static void PCCallInsPrint(const PInstruct *pInstruct)
{
    const PICall *piCall;

    piCall = &pInstruct->piCall;
    PCEmit("CALL 0x%04x [%s]", piCall->dstAddr, piCall->funToken->cplString->str);
}

static void PCBInsPrint(const PInstruct *pInstruct)
{
    const PIB *piB;

    piB = &pInstruct->piB;
    PCEmit("B 0x%04x [0x%04x]", piB->pcDstAddr, piB->icDstAddr);
}

static void PCArithInsPrint(const PInstruct *pInstruct)
{
    const PIArith *piArith;
    const char *arithStr = "?";

    piArith = &pInstruct->piArith;
    switch (pInstruct->type) {
    case PIT_ADD:
        arithStr = "ADD";
        break;
    case PIT_SUB:
        arithStr = "SUB";
        break;
    case PIT_MUL:
        arithStr = "MUL";
        break;
    case PIT_DIV:
        arithStr = "DIV";
        break;
    case PIT_MOD:
        arithStr = "MOD";
        break;
    default:
        break;
    }
    if (piArith->src2Type == PIAS2T_REG) {
        PCEmit("%s %s, %s, %s", arithStr, REG_STR(piArith->dstRegId),
               REG_STR(piArith->src1RegId), REG_STR(piArith->src2));
    } else if (piArith->src2Type == PIAS2T_IMM) {
        if (piArith->src2 >= 0) {
            PCEmit("%s %s, %s, 0x%04x", arithStr, REG_STR(piArith->dstRegId),
                   REG_STR(piArith->src1RegId), piArith->src2);
        } else {
            PCEmit("%s %s, %s, -0x%04x", arithStr, REG_STR(piArith->dstRegId),
                   REG_STR(piArith->src1RegId), -piArith->src2);
        }
    } else {
        PrErr("piArith->src2Type error: %d", piArith->src2Type);
        exit(-1);
    }
}

static void PCBCondInsPrint(const PInstruct *pInstruct)
{
    const char *condStr = "?";
    const PIBCond *piBCond;

    piBCond = &pInstruct->piBCond;
    switch (piBCond->type) {
    case PIBCT_EQ:
        condStr = "EQ";
        break;
    case PIBCT_NE:
        condStr = "NE";
        break;
    case PIBCT_GT:
        condStr = "GT";
        break;
    case PIBCT_GE:
        condStr = "GE";
        break;
    case PIBCT_LT:
        condStr = "LT";
        break;
    case PIBCT_LE:
        condStr = "LE";
        break;
    }
    PCEmit("B%sZ 0x%04x [0x%04x]", condStr, piBCond->pcDstAddr, piBCond->icDstAddr);
}

static void PCLdDataInsPrint(const PInstruct *pInstruct)
{
    const PILDData *piLdData;

    piLdData = &pInstruct->piLdData;
    PCEmit("data: 0x%04x", piLdData->data);
}

static void PCHaltInsPrint(const PInstruct *pInstruct)
{
    (void)pInstruct;
    PCEmit("HALT");
}

static void PCInstructPrint(const PInstruct *pInstruct)
{
    switch (pInstruct->type) {
    case PIT_LD:
        return PCLdInsPrint(pInstruct);
    case PIT_ST:
        return PCStInsPrint(pInstruct);
    case PIT_MOV:
        return PCMovInsPrint(pInstruct);
    case PIT_CALL:
        return PCCallInsPrint(pInstruct);
    case PIT_B:
        return PCBInsPrint(pInstruct);
    case PIT_ADD:
    case PIT_SUB:
    case PIT_MUL:
    case PIT_DIV:
    case PIT_MOD:
        return PCArithInsPrint(pInstruct);
    case PIT_BCOND:
        return PCBCondInsPrint(pInstruct);
    case PIT_LDDATA:
        return PCLdDataInsPrint(pInstruct);
    case PIT_HALT:
        return PCHaltInsPrint(pInstruct);
    default:
        PrErr("pInstruct->type error: %d", pInstruct->type);
        exit(-1);
    }
}

void PCInsArrPrint(PInstructArr *piArr)
{
    uint32_t i;

    if (!piArr)
        return;
    for (i = 0; i < piArr->idx; i++) {
        printf("%04x(H): ", i * 4);
        PCInstructPrint(&piArr->insArr[i]);
    }
}

void PCInstArrSetDataStart(PInstructArr *piArr, size_t dataStart)
{
    piArr->dataStart = dataStart;
}
