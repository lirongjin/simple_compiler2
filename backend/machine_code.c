#include "machine_code.h"
#include "abstract_tree.h"
#include "lex_words.h"
#include "cpl_mm.h"
#include "cpl_debug.h"
#include "stdlib.h"
#include "string.h"
#include "cpl_common.h"
#include "pseudo_code.h"

#define PrErr(...)      Pr(__FILE__, __LINE__, __FUNCTION__, "error", __VA_ARGS__)

#define MCDEBUG
#ifdef MCDEBUG
#define PrDbg(...)      Pr(__FILE__, __LINE__, __FUNCTION__, "debug", __VA_ARGS__)
#else
#define PrDbg(...)
#endif

#define MCEmit(...)     printf(__VA_ARGS__), printf("\n")

#define REG_CNT         (17)
#define RSTR(id)        ((id >= 0 && id < REG_CNT) ? regStr[id] : "R?")

extern const char *regStr[];

static void *MCMalloc(size_t size)
{
    void *p;

    p = CplAlloc(size);
    if (!p) {
        PrErr("no memory");
        exit(-1);
    }
    return p;
}

static void MCFree(void *p)
{
    CplFree(p);
}

MCInstArr *MCALLocInsArr(void)
{
    MCInstArr *mciArr;

    mciArr = MCMalloc(sizeof (*mciArr));
    mciArr->insArr = NULL;
    mciArr->idx = 0;
    mciArr->memSize = 0;
    mciArr->dataStart = 0;
    return mciArr;
}

void MCFreeInsArr(MCInstArr *mciArr)
{
    if (mciArr) {
        if (mciArr->insArr) {
            MCFree(mciArr->insArr);
        }
        MCFree(mciArr);
    }
}

static int MCInsArrAdd(MCInstArr *mciArr, uint32_t instruct)
{
    if (mciArr->idx == 0) {
        mciArr->memSize = sizeof (uint32_t) * 8;
        mciArr->insArr = MCMalloc(mciArr->memSize);
    } else {
        if (mciArr->memSize <= sizeof (uint32_t) * mciArr->idx) {
            size_t newSize;
            uint32_t *newInsArr;

            if (mciArr->memSize >= sizeof (uint32_t) * 256) {
                newSize = mciArr->memSize + sizeof (uint32_t) * 256;
            } else {
                newSize = mciArr->memSize * 2;
            }
            newInsArr = MCMalloc(newSize);
            memcpy(newInsArr, mciArr->insArr, mciArr->memSize);
            MCFree(mciArr->insArr);
            mciArr->insArr = newInsArr;
            mciArr->memSize = newSize;
        }
    }
    mciArr->insArr[mciArr->idx++] = instruct;
    return 0;
}

char MCDataIsOverflow(int32_t data, size_t bitsCnt)
{
    bitsCnt--;
    return (data > ((1 << bitsCnt) - 1))
            || (data < (-(1 << bitsCnt)));
}

static int MCGenLdInstruct(MCInstArr *mciArr, const PInstruct *pInstruct)
{
    MLdInstruct mld;
    const PILd *pld = &pInstruct->piLd;
    uint32_t instruct;

    mld.opCode = MOC_LD;
    switch (pld->type) {
    case PILDT_1BYTE:
        mld.type = MCLT_1BYTE;
        break;
    case PILDT_2BYTE:
        mld.type = MCLT_2BYTE;
        break;
    case PILDT_4BYTE:
        mld.type = MCLT_4BYTE;
        break;
    case PILDT_FLOAT:
        mld.type = MCLT_FLOAT;
        break;
    default:
        PrErr("pld->type error: %d", pld->type);
        exit(-1);
        break;
    }
    if (pld->pseudoFlag == 1) {
        PrErr("pld->pseudoFlag error: %d", pld->pseudoFlag);
        exit(-1);
    }
    mld.dstReg = pld->dstRegId;
    mld.memBaseReg = pld->machine.memRegId;
    if (pld->machine.offsetType == PIOT_IMM) {
        mld.offsetType = MCOT_IMM;
        mld.offset = *(uint32_t *)&pld->machine.offset.imm;
    } else if (pld->machine.offsetType == PIOT_REG) {
        mld.offsetType = MCOT_REG;
        mld.offset = pld->machine.offset.reg;
    } else {
        PrErr("pld->machine.offsetType error: %d", pld->machine.offsetType);
        exit(-1);
    }

    instruct = *(uint32_t *)&mld;
    MCInsArrAdd(mciArr, instruct);
    return 0;
}

static int MCGenStInstruct(MCInstArr *mciArr, const PInstruct *pInstruct)
{
    MStInstruct mst;
    const PISt *pst = &pInstruct->piSt;
    uint32_t instruct;

    mst.opCode = MOC_ST;
    switch (pst->type) {
    case PISTT_1BYTE:
        mst.type = MCST_1BYTE;
        break;
    case PISTT_2BYTE:
        mst.type = MCST_2BYTE;
        break;
    case PISTT_4BYTE:
        mst.type = MCST_4BYTE;
        break;
    case PISTT_FLOAT:
        mst.type = MCST_FLOAT;
        break;
    default:
        PrErr("pst->type error: %d", pst->type);
        exit(-1);
    }
    mst.srcReg = pst->srcRegId;
    mst.memBaseReg = pst->memRegId;
    if (pst->offsetType == PIOT_IMM) {
        mst.offsetType = MCOT_IMM;
        mst.offset = *(uint32_t *)&pst->offset.imm; //处理立即数负数的情况是否正确。
    } else if (pst->offsetType == PIOT_REG) {
        mst.offsetType = MCOT_REG;
        mst.offset = pst->offset.reg;
    } else {
        PrErr("pst->offsetType error: %d", pst->offsetType);
        exit(-1);
    }

    instruct = *(uint32_t *)&mst;
    MCInsArrAdd(mciArr, instruct);
    return 0;
}

static int MCGenMovInstruct(MCInstArr *mciArr, const PInstruct *pInstruct)
{
    MMovInstruct mmov;
    const PIMov *pmov = &pInstruct->piMov;
    uint32_t instruct;

    mmov.opCode = MOC_MOV;
    mmov.dstReg = pmov->dstRegId;
    if (pInstruct->piMov.type == PIMT_IMM){
        if (MCDataIsOverflow(pmov->imm, 19)) {
            PrErr("data overflow error", pmov->imm);
            exit(-1);
        }
        mmov.srcType = PIMT_IMM;
        mmov.src = *(uint32_t *)&pmov->imm;
    } else if (pInstruct->piMov.type == PIMT_REG) {
        mmov.srcType = PIMT_REG;
        mmov.src = pmov->srcReg;
    } else {
        PrErr("pInstruct->piMov.type error: %d", pInstruct->piMov.type);
        exit(-1);
    }
    instruct = *(uint32_t *)&mmov;
    MCInsArrAdd(mciArr, instruct);
    return 0;
}

static int MCGenCallInstruct(MCInstArr *mciArr, const PInstruct *pInstruct)
{
    MBInstruct mb;
    const PICall *pcall = &pInstruct->piCall;
    uint32_t instruct;
    int32_t offset;

    mb.opCode = MOC_B;
    offset = pcall->dstAddr - pcall->curAddr;
    if (MCDataIsOverflow(offset, 24)) {
        PrErr("offset error: %d", offset);
        exit(-1);
    }
    mb.pcOffset = *(uint32_t *)&offset;
    instruct = *(uint32_t *)&mb;
    MCInsArrAdd(mciArr, instruct);
    return 0;
}

static int MCGenBInstruct(MCInstArr *mciArr, const PInstruct *pInstruct)
{
    MBInstruct mb;
    const PIB *pb = &pInstruct->piB;
    uint32_t instruct;
    int32_t offset;

    offset = pb->pcDstAddr - pb->curAddr;
    if (MCDataIsOverflow(offset, 24)) {
        PrErr("offset error: %d", offset);
        exit(-1);
    }
    mb.opCode = MOC_B;
    mb.pcOffset = *(uint32_t *)&offset;
    instruct = *(uint32_t *)&mb;
    MCInsArrAdd(mciArr, instruct);
    return 0;
}

static int MCGenArithInstruct(MCInstArr *mciArr, PInstType type, const PInstruct *pInstruct)
{
    MArithInstruct marith;
    const PIArith *parith = &pInstruct->piArith;
    uint32_t instruct;

    switch (type) {
    case PIT_ADD:
        marith.opCode = MOC_ADD;
        break;
    case PIT_SUB:
        marith.opCode = MOC_SUB;
        break;
    case PIT_MUL:
        marith.opCode = MOC_MUL;
        break;
    case PIT_DIV:
        marith.opCode = MOC_DIV;
        break;
    case PIT_MOD:
        marith.opCode = MOC_MOD;
        break;
    default:
        PrErr("type error: %d", type);
        exit(-1);
    }
    marith.dstReg = parith->dstRegId;
    marith.src1Reg = parith->src1RegId;
    if (MCDataIsOverflow(parith->src2, 15)) {
        PrErr("num: 0x%04x, MCDataIsOverflow error: %d", mciArr->idx * 4, parith->src2);
        exit(-1);
    }
    marith.src2 = *(uint32_t *)&parith->src2;
    marith.src2Type = parith->src2Type;
    instruct = *(uint32_t *)&marith;
    MCInsArrAdd(mciArr, instruct);
    return 0;
}

static int MCGenBCondInstruct(MCInstArr *mciArr, const PInstruct *pInstruct)
{
    MBCondInstruct mbcond;
    const PIBCond *pbcond = &pInstruct->piBCond;
    uint32_t instruct;
    int32_t offset;

    mbcond.opCode = MOC_BCOND;
    mbcond.testReg = pbcond->testRegID;
    offset = pbcond->pcDstAddr - pbcond->curAddr;
    if (MCDataIsOverflow(offset, 17)) {
        PrErr("offset error: %d", offset);
        exit(-1);
    }
    mbcond.pcOffset = *(uint32_t *)&offset;
    switch (pbcond->type) {
    case PIBCT_EQ:
        mbcond.testFlag = MBCITF_EQ;
        break;
    case PIBCT_NE:
        mbcond.testFlag = MBCITF_NE;
        break;
    case PIBCT_GT:
        mbcond.testFlag = MBCITF_GT;
        break;
    case PIBCT_GE:
        mbcond.testFlag = MBCITF_GE;
        break;
    case PIBCT_LT:
        mbcond.testFlag = MBCITF_LT;
        break;
    case PIBCT_LE:
        mbcond.testFlag = MBCITF_LE;
        break;
    default:
        PrErr("pbcond->type error: %d", pbcond->type);
        exit(-1);
    }
    instruct = *(uint32_t *)&mbcond;
    MCInsArrAdd(mciArr, instruct);
    return 0;
}

static int MCGenHaltInstruct(MCInstArr *mciArr, const PInstruct *pInstruct)
{
    MHalt mhalt;
    uint32_t instruct;

    (void)pInstruct;
    mhalt.opCode = MOC_HALT;
    mhalt.reserve = 0;
    instruct = *(uint32_t *)&mhalt;
    MCInsArrAdd(mciArr, instruct);
    return 0;
}

static int MCGenLdData(MCInstArr *mciArr, const PInstruct *pInstruct)
{
    MCInsArrAdd(mciArr, pInstruct->piLdData.data);
    return 0;
}

static int MCGenInstruct(MCInstArr *mciArr, const PInstruct *pInstruct)
{
    switch (pInstruct->type) {
    case PIT_LD:
        MCGenLdInstruct(mciArr, pInstruct);
        break;
    case PIT_ST:
        MCGenStInstruct(mciArr, pInstruct);
        break;
    case PIT_MOV:
        MCGenMovInstruct(mciArr, pInstruct);
        break;
    case PIT_CALL:
        MCGenCallInstruct(mciArr, pInstruct);
        break;
    case PIT_B:
        MCGenBInstruct(mciArr, pInstruct);
        break;
    case PIT_ADD:
    case PIT_SUB:
    case PIT_MUL:
    case PIT_DIV:
    case PIT_MOD:
        MCGenArithInstruct(mciArr, pInstruct->type, pInstruct);
        break;
    case PIT_BCOND:
        MCGenBCondInstruct(mciArr, pInstruct);
        break;
    case PIT_HALT:
        MCGenHaltInstruct(mciArr, pInstruct);
        break;
    default:
        PrErr("pInstruct->type error: %d", pInstruct->type);
        exit(-1);
    }
    return 0;
}

int MCGenInstructs(MCInstArr *mciArr, const PInstructArr *pciArr)
{
    size_t i;

    for (i = 0; i < pciArr->dataStart; i++) {
        MCGenInstruct(mciArr, &pciArr->insArr[i]);
    }
    i = mciArr->dataStart = pciArr->dataStart;
    for (; i < pciArr->idx; i++) {
        MCGenLdData(mciArr, &pciArr->insArr[i]);
    }
    return 0;
}

static int32_t MCSignExtend(uint32_t data, size_t bitsCnt)
{
    if ((data >> (bitsCnt - 1) & 1)) {
        data = 0xffffffff << bitsCnt | data;
    }
    return *(int32_t *)&data;
}

static void MCLdInsPrint(MLdInstruct mld)
{
    const char *ldStr = "?";

    switch (mld.type) {
    case MCLT_1BYTE:
        ldStr = "ldb";
        break;
    case MCLT_2BYTE:
        ldStr = "ldh";
        break;
    case MCLT_4BYTE:
        ldStr = "ldw";
        break;
    case MCLT_FLOAT:
        ldStr = "ldf";
        break;
    default:
        PrErr("mld.type error: %d", mld.type);
        exit(-1);
    }

    if (mld.offsetType == MCOT_IMM) {
        int32_t offset;

        offset = mld.offset;
        if (offset == 0) {
            MCEmit("%s %s, [%s]", ldStr, RSTR(mld.dstReg), RSTR(mld.memBaseReg));
        } else if (offset > 0) {
            MCEmit("%s %s, [%s, #0x%04x]", ldStr, RSTR(mld.dstReg),
                   RSTR(mld.memBaseReg), offset);
        } else {
            MCEmit("%s %s, [%s, #-0x%04x]", ldStr, RSTR(mld.dstReg),
                   RSTR(mld.memBaseReg), -offset);
        }
    } else if (mld.offsetType == MCOT_REG) {
        MCEmit("%s, %s, [%s %s]", ldStr, RSTR(mld.dstReg), RSTR(mld.memBaseReg),
               RSTR(mld.offset));
    } else {
        PrErr("mld.offsetType error: %d", mld.offsetType);
        exit(-1);
    }
}

static void MCStInsPrint(MStInstruct mst)
{
    const char *stStr = "?";

    switch (mst.type) {
    case MCLT_1BYTE:
        stStr = "stb";
        break;
    case MCLT_2BYTE:
        stStr = "sth";
        break;
    case MCLT_4BYTE:
        stStr = "stw";
        break;
    case MCLT_FLOAT:
        stStr = "stf";
        break;
    default:
        PrErr("mst.type error: %d", mst.type);
        exit(-1);
    }

    if (mst.offsetType == MCOT_IMM) {
        int32_t offset;

        offset = mst.offset;
        if (offset == 0) {
            MCEmit("%s %s, [%s]", stStr, RSTR(mst.srcReg), RSTR(mst.memBaseReg));
        } else if (offset > 0) {
            MCEmit("%s %s, [%s, #0x%04x]", stStr, RSTR(mst.srcReg),
                   RSTR(mst.memBaseReg), offset);
        } else {
            MCEmit("%s %s, [%s, #-0x%04x]", stStr, RSTR(mst.srcReg),
                   RSTR(mst.memBaseReg), -offset);
        }
    } else if (mst.offsetType == MCOT_REG) {
        MCEmit("%s, %s, [%s %s]", stStr, RSTR(mst.srcReg), RSTR(mst.memBaseReg),
               RSTR(mst.offset));
    } else {
        PrErr("mst.offsetType error: %d", mst.offsetType);
        exit(-1);
    }
}

static void MCMovInsPrint(MMovInstruct mmov)
{
    if (mmov.srcType == PIMT_IMM) {
        int32_t imm = mmov.src;

        if (imm >= 0) {
            MCEmit("mov %s #0x%04x", RSTR(mmov.dstReg), imm);
        } else {
            MCEmit("mov %s #-0x%04x", RSTR(mmov.dstReg), -imm);
        }
    } else if (mmov.srcType == PIMT_REG) {
        MCEmit("mov %s %s", RSTR(mmov.dstReg), RSTR(mmov.src));
    } else {
        PrErr("mmov.srcType error: %d", mmov.srcType);
        exit(-1);
    }
}

static void MCBInsPrint(MBInstruct mb)
{
    int32_t pcOffset = MCSignExtend(mb.pcOffset, 24);

    if (pcOffset >= 0) {
        MCEmit("b [pc, #0x%04x]", pcOffset);
    } else {
        MCEmit("b [pc, #-0x%04x]", -pcOffset);
    }
}

static void MCBrInsPrint(MBrInstruct mbr)
{
    MCEmit("br [%s]", RSTR(mbr.reg));
}

static void MCArithInsPrint(MArithInstruct marith)
{
    const char *opStr;

    switch (marith.opCode) {
    case MOC_ADD:
        opStr = "add";
        break;
    case MOC_SUB:
        opStr = "sub";
        break;
    case MOC_MUL:
        opStr = "mul";
        break;
    case MOC_DIV:
        opStr = "div";
        break;
    case MOC_MOD:
        opStr = "mod";
        break;
    default:
        PrErr("marith.opCode error: %d", marith.opCode);
        exit(-1);
    }
    if (marith.src2Type == PIAS2T_REG) {
        MCEmit("%s %s, %s, %s", opStr, RSTR(marith.dstReg),
               RSTR(marith.src1Reg), RSTR(marith.src2));
    } else if (marith.src2Type == PIAS2T_IMM) {
        if (marith.src2 >= 0) {
            MCEmit("%s %s, %s, 0x%04x", opStr, RSTR(marith.dstReg),
                   RSTR(marith.src1Reg), marith.src2);
        } else {
            MCEmit("%s %s, %s, -0x%04x", opStr, RSTR(marith.dstReg),
                   RSTR(marith.src1Reg), -marith.src2);
        }
    } else {
        PrErr("marith.src2Type error: %d", marith.src2Type);
        exit(-1);
    }
}

static void MCBCondInsPrint(MBCondInstruct mbcond)
{
    const char *tstr;
    int32_t pcOffset = MCSignExtend(mbcond.pcOffset, 17);

    switch (mbcond.testFlag) {
    case MBCITF_EQ:
        tstr = "eq";
        break;
    case MBCITF_NE:
        tstr = "ne";
        break;
    case MBCITF_GT:
        tstr = "gt";
        break;
    case MBCITF_GE:
        tstr = "ge";
        break;
    case MBCITF_LT:
        tstr = "lt";
        break;
    case MBCITF_LE:
        tstr = "le";
        break;
    }
    if (pcOffset >= 0) {
        MCEmit("b%sr %s, [pc, #0x%04x]", tstr, RSTR(mbcond.testReg), pcOffset);
    } else {
        MCEmit("b%sr %s, [pc, #-0x%04x]", tstr, RSTR(mbcond.testReg), -pcOffset);
    }
}

static void MCHaltPrint(MHalt mhalt)
{
    (void)mhalt;
    MCEmit("halt");
}

static void MCInsPrint(uint32_t instruct)
{
    MachineOpCode opCode;

    opCode = instruct & 0x0f;
    switch (opCode) {
    case MOC_LD:
        return MCLdInsPrint(*(MLdInstruct *)&instruct);
    case MOC_ST:
        return MCStInsPrint(*(MStInstruct *)&instruct);
    case MOC_MOV:
        return MCMovInsPrint(*(MMovInstruct *)&instruct);
    case MOC_B:
        return MCBInsPrint(*(MBInstruct *)&instruct);
    case MOC_BR:
        return MCBrInsPrint(*(MBrInstruct *)&instruct);
    case MOC_ADD:
    case MOC_SUB:
    case MOC_MUL:
    case MOC_DIV:
    case MOC_MOD:
        return MCArithInsPrint(*(MArithInstruct *)&instruct);
    case MOC_BCOND:
        return MCBCondInsPrint(*(MBCondInstruct *)&instruct);
    case MOC_HALT:
        return MCHaltPrint(*(MHalt *)&instruct);
    default:
        PrErr("opCode error: %d", opCode);
        exit(-1);
    }
}

void MCInsArrPrint(const MCInstArr *mciArr)
{
    size_t i;

    if (!mciArr)
        return;
    for (i = 0; i < mciArr->idx; i++) {
        printf("%04x(h): ", i * 4);
        if (i < mciArr->dataStart) {
            MCInsPrint(mciArr->insArr[i]);
        } else {
            MCEmit("data: 0x%04x", mciArr->insArr[i]);
        }
    }
}

