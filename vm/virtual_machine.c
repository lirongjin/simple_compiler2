#include "virtual_machine.h"
#include "pseudo_code.h"
#include "machine_code.h"
#include "cpl_errno.h"
#include "cpl_debug.h"
#include "stdlib.h"
#include "cpl_mm.h"

#define PrErr(...)      Pr(__FILE__, __LINE__, __FUNCTION__, "error", __VA_ARGS__)

#define VMDEBUG
#ifdef VMDEBUG
#define PrDbg(...)      Pr(__FILE__, __LINE__, __FUNCTION__, "debug", __VA_ARGS__)
#else
#define PrDbg(...)
#endif

#if 0
#define VMEmit(...)     printf(__VA_ARGS__), printf("\n"), fflush(stdout)
#else
#define VMEmit(...)
#endif

#define VMREG_CNT       (16)        /*通用寄存器数量*/
#define MEM_CNT         (0x10000)   /*内存大小*/
#define REG_PC          (15)        /*PC寄存器索引*/
#define REG_LR          (14)        /*链接寄存器索引*/
#define REG_SP          (13)        /*堆栈寄存器索引*/

#define OUT_ADDR        (0x8000)    /*向屏幕输出字符串的地址*/

#define CREG_CNT        (17)        /*寄存器数量，用于获取寄存器对应的字符串*/
#define RSTR(id)        ((id >= 0 && id < CREG_CNT) ? regStr[id] : "R?")

extern const char *regStr[];

/*虚拟机*/
typedef struct _VMachine {
    uint32_t reg[VMREG_CNT];    /*通用寄存器组。*/
    uint32_t psr;               /*状态寄存器。*/
    uint8_t memory[MEM_CNT];    /*虚拟机内存空间。*/
    uint32_t startAddr;         /*程序运行起始地址。*/
    uint32_t curPc;             /*当前PC寄存器的值。*/
    uint32_t runFlag: 1;        /*虚拟机运行标志。*/
} VMachine;

static void *VMMalloc(size_t size)
{
    void *p;

    p = CplAlloc(size);
    if (!p) {
        PrErr("no memory");
        exit(-1);
    }
    return p;
}

static void VMFree(void *p)
{
    CplFree(p);
}

VMachine *VMAllocMachine(void)
{
    VMachine *vm;

    vm = VMMalloc(sizeof (*vm));
    vm->startAddr = 0x0000;
    return vm;
}

void VMFreeMachine(VMachine *vm)
{
    if (vm) {
        VMFree(vm);
    }
}

static uint32_t VMGetReg(VMachine *vm, uint8_t reg)
{
    if (reg == REG_PC) {
        return vm->curPc;
    } else {
        return vm->reg[reg];
    }
}

/*
 * 功能：带符号扩展
 * data: 被扩展的数据
 * bitsCnt: data的位数量
 * 返回值：带符号扩展后的数据。
 **/
static int32_t VmSignExtend(uint32_t data, size_t bitsCnt)
{
    if ((data >> (bitsCnt - 1) & 1)) {
        data = 0xffffffff << bitsCnt | data;
    }
    return *(int32_t *)&data;
}

/*LD/ST操作内存空间地址的类型*/
typedef enum {
    MOT_1BYTE,  /*操作一个字节*/
    MOT_2BYTE,  /*操作半个字*/
    MOT_4BYTE,  /*操作一个字*/
} MemOpType;

/*
 * 功能：向虚拟机内存地址空间写入内容
 * vm: 虚拟机
 * moType: 内存操作类型
 * addr: 内存地址。内存地址需要对齐。
 * data: 待写入数据
 * 返回值：无
 **/
static void VmMemorySet(VMachine *vm, MemOpType moType, uint32_t addr, uint32_t data)
{
    /*内存地址对齐*/
    if (addr % (1 << moType) != 0) {
        PrErr("addr error: %d", addr);
        exit(-1);
    }

    if (addr == OUT_ADDR) { /*如果地址是OUT_ADDR则向屏幕输出此数据。*/
        putchar(data);
    } else {
        if (moType >= MOT_1BYTE) {
            vm->memory[addr] = data;
        }
        if (moType >= MOT_2BYTE) {
            vm->memory[addr+1] = data >> 8;
        }
        if (moType >= MOT_4BYTE) {
            vm->memory[addr+2] = data >> 16;
            vm->memory[addr+3] = data >> 24;
        }
    }
}

/*
 * 功能：从虚拟机内存地址空间读取内容
 * vm: 虚拟机
 * moType: 内存操作类型
 * addr: 内存地址。内存地址需要对齐。
 * 返回值：虚拟机内存空间指定地址处的内容。
 **/
static uint32_t VmMemoryGet(VMachine *vm, MemOpType moType, uint32_t addr)
{
    uint32_t data = 0;

    /*检查地址是否对齐*/
    if (addr % (1 << moType) != 0) {
        PrErr("addr error: %d", addr);
        exit(-1);
    }
    if (moType >= MOT_1BYTE) {
        data = vm->memory[addr];
    }
    if (moType >= MOT_2BYTE) {
        data |= vm->memory[addr+1] << 8;
    }
    if (moType >= MOT_4BYTE) {
        data |= vm->memory[addr+2] << 16;
        data |= vm->memory[addr+3] << 24;
    }
    return data;
}

static char immStr[100];

static char *VmImmStr(int32_t imm)
{
    if (imm > 0) {
        snprintf(immStr, sizeof (immStr), "0x%04x", imm);
    } else if (imm < 0) {
        snprintf(immStr, sizeof (immStr), "-0x%04x", -imm);
    } else {
        snprintf(immStr, sizeof (immStr), "%d", imm);
    }
    return immStr;
}

/*
 * 功能：加载指令
 * 返回值：
 **/
static void VmLd(VMachine *vm, MLdInstruct mld)
{
    MemOpType moType;
    int32_t offset;
    const char *ldStr = "?";

    switch (mld.type) {
    case MCLT_1BYTE:
        moType = MOT_1BYTE;
        ldStr = "ldb";
        break;
    case MCLT_2BYTE:
        moType = MOT_2BYTE;
        ldStr = "ldh";
        break;
    case MCLT_4BYTE:
        moType = MOT_4BYTE;
        ldStr = "ldw";
        break;
    case MCLT_FLOAT:
        moType = MOT_4BYTE;
        ldStr = "ldf";
        break;
    default:
        PrErr("mld.type error: %d", mld.type);
        exit(-1);
    }

    if (mld.offsetType == MCOT_IMM) {
        offset = VmSignExtend(mld.offset, 12);
        VMEmit("%s %s, [%s, #%s]", ldStr, RSTR(mld.dstReg), RSTR(mld.memBaseReg), VmImmStr(offset));
    } else if (mld.offsetType == MCOT_REG) {
        offset = VmSignExtend(VMGetReg(vm, mld.offset), 32);
        VMEmit("%s %s, [%s, %s]", ldStr, RSTR(mld.dstReg), RSTR(mld.memBaseReg), RSTR(mld.offset));
    } else {
        PrErr("mld.offsetType error: %d", mld.offsetType);
        exit(-1);
    }

    vm->reg[mld.dstReg] = VmMemoryGet(vm, moType, VMGetReg(vm, mld.memBaseReg) + offset);
}

static void VmSt(VMachine *vm, MStInstruct mst)
{
    MemOpType moType;
    int32_t offset;
    const char *stStr = "?";

    switch (mst.type) {
    case MCLT_1BYTE:
        moType = MOT_1BYTE;
        stStr = "stb";
        break;
    case MCLT_2BYTE:
        moType = MOT_2BYTE;
        stStr = "sth";
        break;
    case MCLT_4BYTE:
        moType = MOT_4BYTE;
        stStr = "stw";
        break;
    case MCLT_FLOAT:
        moType = MOT_4BYTE;
        stStr = "stw";
        break;
    default:
        PrErr("mst.type error: %d", mst.type);
        exit(-1);
    }

    if (mst.offsetType == MCOT_IMM) {
        offset = VmSignExtend(mst.offset, 12);
        if (offset > 0) {
            VMEmit("%s %s, [%s, #0x%04x]", stStr, RSTR(mst.srcReg), RSTR(mst.memBaseReg), offset);
        } else if (offset < 0) {
            VMEmit("%s %s, [%s, #-0x%04x]", stStr, RSTR(mst.srcReg), RSTR(mst.memBaseReg), -offset);
        } else {
            VMEmit("%s %s, [%s]", stStr, RSTR(mst.srcReg), RSTR(mst.memBaseReg));
        }
    } else if (mst.offsetType == MCOT_REG) {
        offset = VmSignExtend(VMGetReg(vm, mst.offset), 32);
        VMEmit("%s %s, [%s, %s]", stStr, RSTR(mst.srcReg), RSTR(mst.memBaseReg), RSTR(mst.offset));
    } else {
        PrErr("mst.offsetType error: %d", mst.offsetType);
        exit(-1);
    }
    VmMemorySet(vm, moType, VMGetReg(vm, mst.memBaseReg) + offset, VMGetReg(vm, mst.srcReg));
}

static void VmMov(VMachine *vm, MMovInstruct mmov)
{
    if (mmov.srcType == PIMT_IMM) {
        int32_t imm;

        imm = VmSignExtend(mmov.src, 19);
        VMEmit("mov %s, #%s", RSTR(mmov.dstReg), VmImmStr(imm));
        vm->reg[mmov.dstReg] = imm;
    } else if (mmov.srcType == PIMT_REG) {
        VMEmit("mov %s, %s", RSTR(mmov.dstReg), RSTR(mmov.src));
        vm->reg[mmov.dstReg] = vm->reg[mmov.src];
    } else {
        PrErr("mmov.srcType error: %d", mmov.srcType);
        exit(-1);
    }
}

static void VmB(VMachine *vm, MBInstruct mb)
{
    int32_t pcOffset;

    pcOffset = VmSignExtend(mb.pcOffset, 24);
    //vm->reg[REG_PC] += pcOffset;
    vm->reg[REG_PC] = VMGetReg(vm, REG_PC) + pcOffset;
    VMEmit("b 0x%04x", VMGetReg(vm, REG_PC));
}

static void VmBr(VMachine *vm, MBrInstruct mbr)
{
    VMEmit("br %s", RSTR(mbr.reg));
    vm->reg[REG_PC] = VMGetReg(vm, mbr.reg);
}

static void VmAdd(VMachine *vm, MArithInstruct mai)
{
    if (mai.src2Type == PIAS2T_REG) {
        VMEmit("add %s, %s, %s", RSTR(mai.dstReg), RSTR(mai.src1Reg), RSTR(mai.src2));
        vm->reg[mai.dstReg] = VMGetReg(vm, mai.src1Reg) + VMGetReg(vm, mai.src2);
    } else if (mai.src2Type == PIAS2T_IMM) {
        int32_t src2;

        src2 = VmSignExtend(mai.src2, 15);
        if (src2 >= 0) {
            VMEmit("add %s, %s, 0x%04x", RSTR(mai.dstReg), RSTR(mai.src1Reg), src2);
        } else {
            VMEmit("add %s, %s, -0x%04x", RSTR(mai.dstReg), RSTR(mai.src1Reg), -src2);
        }
        vm->reg[mai.dstReg] = VMGetReg(vm, mai.src1Reg) + src2;
    } else {
        PrErr("mai.src2Type error: %d", mai.src2Type);
        exit(-1);
    }
}

static void VmSub(VMachine *vm, MArithInstruct mai)
{
    if (mai.src2Type == PIAS2T_REG) {
        VMEmit("sub %s, %s, %s", RSTR(mai.dstReg), RSTR(mai.src1Reg), RSTR(mai.src2));
        vm->reg[mai.dstReg] = VMGetReg(vm, mai.src1Reg) - VMGetReg(vm, mai.src2);
    } else if (mai.src2Type == PIAS2T_IMM) {
        int32_t src2;

        src2 = VmSignExtend(mai.src2, 15);
        if (src2 >= 0) {
            VMEmit("sub %s, %s, 0x%04x", RSTR(mai.dstReg), RSTR(mai.src1Reg), src2);
        } else {
            VMEmit("sub %s, %s, -0x%04x", RSTR(mai.dstReg), RSTR(mai.src1Reg), -src2);
        }
        vm->reg[mai.dstReg] = VMGetReg(vm, mai.src1Reg) - src2;
    } else {
        PrErr("mai.src2Type error: %d", mai.src2Type);
        exit(-1);
    }
}

static void VmMul(VMachine *vm, MArithInstruct mai)
{
    if (mai.src2Type == PIAS2T_REG) {
        VMEmit("mul %s, %s, %s", RSTR(mai.dstReg), RSTR(mai.src1Reg), RSTR(mai.src2));
        vm->reg[mai.dstReg] = VMGetReg(vm, mai.src1Reg) * VMGetReg(vm, mai.src2);
    } else if (mai.src2Type == PIAS2T_IMM) {
        int32_t src2;

        src2 = VmSignExtend(mai.src2, 15);
        if (src2 >= 0) {
            VMEmit("mul %s, %s, 0x%04x", RSTR(mai.dstReg), RSTR(mai.src1Reg), src2);
        } else {
            VMEmit("mul %s, %s, -0x%04x", RSTR(mai.dstReg), RSTR(mai.src1Reg), -src2);
        }
        vm->reg[mai.dstReg] = VMGetReg(vm, mai.src1Reg) * src2;
    } else {
        PrErr("mai.src2Type error: %d", mai.src2Type);
        exit(-1);
    }
}

static void VmDiv(VMachine *vm, MArithInstruct mai)
{
    if (mai.src2Type == PIAS2T_REG) {
        VMEmit("div %s, %s, %s", RSTR(mai.dstReg), RSTR(mai.src1Reg), RSTR(mai.src2));
        vm->reg[mai.dstReg] = VMGetReg(vm, mai.src1Reg) / VMGetReg(vm, mai.src2);
    } else if (mai.src2Type == PIAS2T_IMM) {
        int32_t src2;

        src2 = VmSignExtend(mai.src2, 15);
        if (src2 >= 0) {
            VMEmit("div %s, %s, 0x%04x", RSTR(mai.dstReg), RSTR(mai.src1Reg), src2);
        } else {
            VMEmit("div %s, %s, -0x%04x", RSTR(mai.dstReg), RSTR(mai.src1Reg), -src2);
        }
        vm->reg[mai.dstReg] = VMGetReg(vm, mai.src1Reg) / src2;
    } else {
        PrErr("mai.src2Type error: %d", mai.src2Type);
        exit(-1);
    }
}

static void VmMod(VMachine *vm, MArithInstruct mai)
{
    if (mai.src2Type == PIAS2T_REG) {
        VMEmit("mod %s, %s, %s", RSTR(mai.dstReg), RSTR(mai.src1Reg), RSTR(mai.src2));
        vm->reg[mai.dstReg] = VMGetReg(vm, mai.src1Reg) % VMGetReg(vm, mai.src2);
    } else if (mai.src2Type == PIAS2T_IMM) {
        int32_t src2;

        src2 = VmSignExtend(mai.src2, 15);
        if (src2 >= 0) {
            VMEmit("mod %s, %s, 0x%04x", RSTR(mai.dstReg), RSTR(mai.src1Reg), src2);
        } else {
            VMEmit("mod %s, %s, -0x%04x", RSTR(mai.dstReg), RSTR(mai.src1Reg), -src2);
        }
        vm->reg[mai.dstReg] = VMGetReg(vm, mai.src1Reg) % src2;
    } else {
        PrErr("mai.src2Type error: %d", mai.src2Type);
        exit(-1);
    }
}

static void VmBCond(VMachine *vm, MBCondInstruct mbr)
{
    int32_t offset;
    int32_t testVal;
    const char *testStr = "?";

    offset = VmSignExtend(mbr.pcOffset, 17);
    testVal = VmSignExtend(VMGetReg(vm, mbr.testReg), 32);
    if ((mbr.testFlag == MBCITF_EQ
         && testVal == 0)
            || (mbr.testFlag == MBCITF_NE
                && testVal != 0)
            || (mbr.testFlag == MBCITF_GT
                && testVal > 0)
            || (mbr.testFlag == MBCITF_GE
                && testVal >= 0)
            || (mbr.testFlag == MBCITF_LT
                && testVal < 0)
            || (mbr.testFlag == MBCITF_LE
                && testVal <= 0)) {
        //vm->reg[REG_PC] += offset;
        vm->reg[REG_PC] = VMGetReg(vm, REG_PC) + offset;
        switch (mbr.testFlag) {
        case MBCITF_EQ:
            testStr = "eq";
            break;
        case MBCITF_NE:
            testStr = "ne";
            break;
        case MBCITF_GT:
            testStr = "gt";
            break;
        case MBCITF_GE:
            testStr = "ge";
            break;
        case MBCITF_LT:
            testStr = "lt";
            break;
        case MBCITF_LE:
            testStr = "le";
            break;
        }
        VMEmit("b%s %s 0x%04x", testStr, RSTR(mbr.testReg), VMGetReg(vm, REG_PC));
    }
}

static void VmHalt(VMachine *vm, MHalt mhalt)
{
    (void)mhalt;
    VMEmit("halt");
    vm->runFlag = 0;
}

static int VMachineLoad(VMachine *vm, uint32_t instArr[], uint32_t size)
{
    uint32_t i;
    int k;

    for (i = 0; i < size; i++) {
        k = i * 4;
        vm->memory[k] = instArr[i];
        vm->memory[k+1] = instArr[i] >> 8;
        vm->memory[k+2] = instArr[i] >> 16;
        vm->memory[k+3] = instArr[i] >> 24;
    }
    return 0;
}

int VMachineExec(VMachine *vm, uint32_t instArr[], uint32_t size)
{
    uint32_t instruct;
    MachineOpCode opCode;

    if (!vm || !instArr) {
        return -EINVAL;
    }
    VMachineLoad(vm, instArr, size);
    vm->reg[REG_PC] = vm->startAddr;
    vm->runFlag = 1;
    printf("\n------virtual machine run-----------\n");
    while (vm->runFlag) {
        uint32_t pc;

        pc = vm->reg[REG_PC];
        if (pc % 4 != 0) {
            PrErr("pc error: %d", pc);
            exit(-1);
        }
        instruct = vm->memory[pc];
        instruct |= vm->memory[pc+1] << 8;
        instruct |= vm->memory[pc+2] << 16;
        instruct |= vm->memory[pc+3] << 24;
        opCode = instruct & 0xff;
        vm->curPc = vm->reg[REG_PC];
        vm->reg[REG_PC] += 4;
        switch (opCode) {
        case MOC_LD:
            VmLd(vm, *(MLdInstruct *)&instruct);
            break;
        case MOC_ST:
            VmSt(vm, *(MStInstruct *)&instruct);
            break;
        case MOC_MOV:
            VmMov(vm, *(MMovInstruct *)&instruct);
            break;
        case MOC_B:
            VmB(vm, *(MBInstruct *)&instruct);
            break;
        case MOC_BR:
            VmBr(vm, *(MBrInstruct *)&instruct);
            break;
        case MOC_ADD:
            VmAdd(vm, *(MArithInstruct *)&instruct);
            break;
        case MOC_SUB:
            VmSub(vm, *(MArithInstruct *)&instruct);
            break;
        case MOC_MUL:
            VmMul(vm, *(MArithInstruct *)&instruct);
            break;
        case MOC_DIV:
            VmDiv(vm, *(MArithInstruct *)&instruct);
            break;
        case MOC_MOD:
            VmMod(vm, *(MArithInstruct *)&instruct);
            break;
        case MOC_BCOND:
            VmBCond(vm, *(MBCondInstruct *)&instruct);
            break;
        case MOC_HALT:
            VmHalt(vm, *(MHalt *)&instruct);
            break;
        default:
            PrErr("op code error: %d", opCode);
            exit(-1);
        }
    }
    printf("\nvirtual machine normal termination.\n");
    return 0;
}

