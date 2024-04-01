#ifndef __MACHINE_CODE_H__
#define __MACHINE_CODE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"

typedef enum {
    MCOT_IMM,
    MCOT_REG,
} MCOffsetType;

typedef enum {
    MCLT_1BYTE,
    MCLT_2BYTE,
    MCLT_4BYTE,
    MCLT_FLOAT,
} MCLdType;

typedef struct {
    uint32_t opCode: 8;
    uint32_t type: 2;
    uint32_t offsetType: 1;
    uint32_t dstReg: 5;
    uint32_t memBaseReg: 4;
    uint32_t offset: 12;
} MLdInstruct;

typedef enum {
    MCST_1BYTE,
    MCST_2BYTE,
    MCST_4BYTE,
    MCST_FLOAT,
} MCSType;

typedef struct {
    uint32_t opCode: 8;
    uint32_t type: 2;
    uint32_t offsetType: 1;
    uint32_t srcReg: 5;
    uint32_t memBaseReg: 4;
    uint32_t offset: 12;
} MStInstruct;

typedef struct {
    uint32_t opCode: 8;
    uint32_t dstReg: 4;
    uint32_t srcType: 1;
    uint32_t src: 19;
} MMovInstruct;

typedef struct {
    uint32_t opCode: 8;
    uint32_t pcOffset: 24;
} MBInstruct;

typedef struct {
    uint32_t opCode: 8;
    uint32_t reg: 4;
    uint32_t reserve: 20;
} MBrInstruct;

typedef struct {
    uint32_t opCode: 8;
    uint32_t dstReg: 4;
    uint32_t src1Reg: 4;
    uint32_t src2Type: 1;
    uint32_t src2: 15;
} MArithInstruct;

typedef enum {
    MBCITF_EQ,
    MBCITF_NE,
    MBCITF_GT,
    MBCITF_GE,
    MBCITF_LT,
    MBCITF_LE,
} MBCITestFlag;

typedef struct {
    uint32_t opCode: 8;
    uint32_t testFlag: 3;
    uint32_t testReg: 4;
    uint32_t pcOffset: 17;
} MBCondInstruct;

typedef struct {
    uint32_t opCode: 8;
    uint32_t reserve: 24;
} MHalt;

typedef enum {
    MOC_LD,
    MOC_ST,
    MOC_MOV,
    MOC_B,
    MOC_BR,
    MOC_ADD,
    MOC_SUB,
    MOC_MUL,
    MOC_DIV,
    MOC_MOD,
    MOC_BCOND,
    MOC_HALT,
} MachineOpCode;

typedef struct {
    uint32_t *insArr;
    size_t idx;
    size_t memSize;
    size_t dataStart;
} MCInstArr;

typedef struct _PInstructArr PInstructArr;

MCInstArr *MCALLocInsArr(void);
void MCFreeInsArr(MCInstArr *mciArr);

int MCGenInstructs(MCInstArr *mciArr, const PInstructArr *pciArr);
void MCInsArrPrint(const MCInstArr *mciArr);

#ifdef __cplusplus
}
#endif

#endif /*__MACHINE_CODE_H__*/

