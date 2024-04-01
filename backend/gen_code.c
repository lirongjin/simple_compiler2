#include "gen_code.h"
#include "quadruple.h"
#include "environ.h"
#include "abstract_tree.h"
#include "cpl_errno.h"
#include "cpl_mm.h"
#include "cpl_debug.h"
#include "stdlib.h"
#include "stdarg.h"
#include "cpl_common.h"
#include "bison.h"
#include "pseudo_code.h"
#include "machine_code.h"
#include "string.h"
#include "virtual_machine.h"

#define PrErr(...)      Pr(__FILE__, __LINE__, __FUNCTION__, "error", __VA_ARGS__)

#define GCDEBUG
#ifdef GCDEBUG
#define PrDbg(...)      Pr(__FILE__, __LINE__, __FUNCTION__, "debug", __VA_ARGS__)
#else
#define PrDbg(...)
#endif

#define REG_CNT             (16)        /*通用寄存器数量*/

#define GLOBAL_BASE         (0X2000)    /*静态存储区起始地址*/
#define SP_BASE             (0x4000)    /*动态存储区起始地址*/

#define TMP_REG             (6)     //用于捯饬函数返回值结果
#define SCRATCH_REG1        (7)     //根据偏移量访问成员时寄存器，存储偏移量
#define SCRATCH_REG0        (8)     //用于加载全局地址的寄存器
#define BASE_REG_ID         (9)     //用于计算数组地址的寄存器编号
#define SP_OFFSET_REG_ID    (10)    //用于处理函数调用时的SP偏移量
#define TRAN_DATA_REG_ID    (11)    //传递实际参数寄存器
#define SP_REG_ID           (13)
#define LR_REG_ID           (14)
#define PC_REG_ID           (15)
#define PSR_REG_ID          (16)

#define PC_ADDR(piArr)      ((piArr)->idx * 4)  //当前伪代码地址

/*中间代码地址和伪指令地址的映射表*/
typedef struct {
    size_t *maps;   /*映射表数组，中间代码地址对应的伪指令地址。*/
    size_t idx;     /*当前映射表数组的大小。*/
    size_t memSize; /*映射表数组的内存空间。*/
} AddrMapTable;

/*被分配的地址类型*/
typedef enum {
    MAT_GLOBAL,     /*全局地址*/
    MAT_LOCAL,      /*局部地址*/
} MemAddrType;

/*用于操作寄存器结构体*/
typedef struct _Register {
    size_t id;                  /*寄存器编号*/
    MemAddrType memAddrType;    /*关联的内存的地址类型*/
    int memAddr;           /*关联的地址在作用域中的偏移量*/
    uint32_t syncFlag: 1;       /*寄存器中的内容同步标志*/
    uint32_t indirFlag: 1;      /*间接寻址*/
    Type *dataType;                /*关联的内存对应的变量类型*/
    uint8_t spReg;              /*堆栈指针寄存器*/
    QRAccessMbrOffset *amOffset;    /*偏移量信息，用于访问基于某地址的相对位置上的数据。*/
} Register;

/*param指令节点*/
typedef struct {
    struct list_head node;
    const QuadRuple *ruple; /*param四元组指令*/
} ParamInsNode;

/*生成代码过程中需要的信息*/
typedef struct {
    size_t addrNum;         /*地址生成器*/
    Register reg[REG_CNT];  /*寄存器*/
    struct list_head paramList; /*param指令链表*/
    PInstructArr *piArr;    /*伪指令容器*/
    AddrMapTable *amTable;  /*中间代码和伪指令代码地址映射表*/
    MCInstArr *miArr;       /*机器指令容器*/
    VMachine *vm;           /*虚拟机*/
    struct list_head setActSpList;  /*设置活动记录栈基地址链表*/
} GCInfo;

static void *GCMAlloc(size_t size)
{
    void *p;

    p = CplAlloc(size);
    if (!p) {
        PrErr("no memory");
        exit(-1);
    }
    return p;
}

static void GCMFree(void *p)
{
    return CplFree(p);
}

/*
 * 功能：申请地址映射表
 * 返回值：
 **/
AddrMapTable *GCAMTableAlloc(void)
{
    AddrMapTable *amTable;

    amTable = GCMAlloc(sizeof (*amTable));
    amTable->idx = 0;
    amTable->memSize = 0;
    amTable->maps = NULL;
    return amTable;
}

/*
 * 功能：释放地址映射表
 * 返回值：
 **/
void GCAMTableFree(AddrMapTable *amTable)
{
    if (amTable) {
        if (amTable->maps) {
            GCMFree(amTable->maps);
        }
        GCMFree(amTable);
    }
}

/*
 * 功能：向地址映射表添加映射节点
 * 返回值：
 **/
int GCAMTableAdd(AddrMapTable *amTable, size_t addr)
{
    if (amTable->idx == 0) {
        amTable->memSize = 8 * sizeof (size_t);
        amTable->maps = GCMAlloc(amTable->memSize);
    } else {
        if (amTable->memSize <= amTable->idx * sizeof (size_t)) {
            size_t newSize;
            size_t *newMaps;

            if (amTable->memSize > 256 * sizeof (size_t)) {
                newSize = amTable->memSize + 256 * sizeof (size_t);
            } else {
                newSize = amTable->memSize * 2;
            }
            newMaps = GCMAlloc(newSize);
            memcpy(newMaps, amTable->maps, amTable->memSize);
            GCMFree(amTable->maps);
            amTable->maps = newMaps;
            amTable->memSize = newSize;
        }
    }
    amTable->maps[amTable->idx++] = addr;
    return 0;
}

/*
 * 功能：向param指令链表的尾部添加节点。
 * 返回值：
 **/
void GCParamListPush(struct list_head *paramList, const QuadRuple *ruple)
{
    ParamInsNode *piNode;

    piNode = GCMAlloc(sizeof (*piNode));
    piNode->ruple = ruple;
    list_add_tail(&piNode->node, paramList);
}

/*
 * 功能：从param指令链表尾部弹出一个节点。
 * 返回值：
 **/
const QuadRuple *GCParamListPop(struct list_head *paramList)
{
    ParamInsNode *piNode;
    struct list_head *pos;

    if (list_empty(paramList)) {
        PrErr("param List empty");
        exit(-1);
    }
    pos = list_get_tail(paramList);
    piNode = container_of(pos, ParamInsNode, node);
    list_del(&piNode->node);
    return piNode->ruple;
}

#if 0
static void GCEmit(GCInfo *info, const char *fmt, ...)
{
    va_list ap;
    char buffer[1024] = {0};

    va_start(ap, fmt);
    vsnprintf(buffer, sizeof (buffer), fmt, ap);
    va_end(ap);
    printf("%04x(H): %s\n", info->addrNum, buffer);
    info->addrNum += 4;
}
#endif

static Type *GCFunParamType(Type *type)
{
    if (type->type == TT_ARRAY) {
        return ATreeNewPointerType(type->array.type);
    } else if (type->type == TT_FUNCTION) {
        return ATreeNewPointerType(type);
    } else {
        return type;
    }
}

/*
 * 功能：根据地址关联的数据类型进行地址对齐，仅用于函数形式参数。
 * 返回值：
 **/
static size_t GCFunArgAddrAlignByType(size_t addr, const Type *type)
{
    if (type->type == TT_BASE
            || type->type == TT_POINTER
            || type->type == TT_STRUCTURE) {
        addr = ATreeAddrAlignByType(addr, type);
    } else if (type->type == TT_ARRAY) {
        /*传递的是第一维首个元素的地址，按指针进行对齐。*/
        addr = Align4Byte(addr);
    } else {
        PrErr("type->type: %d", type->type);
        exit(-1);
    }
    return addr;
}

/*
 * 功能：计算函数的形式参数的偏移量。
 * 返回值：
 **/
static size_t GCGetFunArgsOffset(size_t offset, const FunArgs *funArgs)
{
    if (funArgs) {
        FunArg *funArg = funArgs->funArg;
        Type *funParamType;

        funParamType = GCFunParamType(funArg->type);
        offset = ATreeAddrAlignByType(offset, funParamType);
        offset += ATreeTypeSize(funParamType);
        offset = GCGetFunArgsOffset(offset, funArgs->next);
    }
    return offset;
}

static size_t GCGetFunArgsCount(const FunArgs *funArgs)
{
    size_t cnt = 0;

    while (funArgs) {
        cnt++;
        funArgs = funArgs->next;
    }
    return cnt;
}

/*
 * 功能：计算函数形式参id的偏移量
 * 返回值：
 **/
static size_t GCGetFunArgOffset(const FunType *funType, const Token *id)
{
    size_t offset = 0;
    FunArgs *funArgs;

    funArgs = ATreeFunArgsReverse(funType->funArgs);
    while (funArgs) {
        FunArg *funArg;
        Type *funParamType;

        funArg = funArgs->funArg;
        funParamType = GCFunParamType(funArg->type);
        offset = ATreeAddrAlignByType(offset, funParamType);
        if (funArg->id == id) {
            return offset;
        }
        offset += ATreeTypeSize(funParamType);
        funArgs = funArgs->next;
    }
    PrErr("funArgs: %p", funArgs);
    exit(-1);
}

/*
 * 功能：计算函数返回值在活动记录栈帧上的偏移位置
 * 返回值：
 **/
static size_t GCGetFunRetOffset(const FunType *funType)
{
    size_t offset = 0;
    FunArgs *funArgs;
    Type *funRetType;

    funArgs = ATreeFunArgsReverse(funType->funArgs);
    offset = GCGetFunArgsOffset(offset, funArgs);
    if (funType->retType->typeType != ERTT_VOID) {
        funRetType = GCFunParamType(funType->retType->type);
        offset = ATreeAddrAlignByType(offset, funRetType);
    }
    return offset;
}

/*
 * 功能：计算函数形式参数加返回值所占内存空间的大小。
 * 返回值：
 **/
static size_t GCGetFunTypeSize(const FunType *funType)
{
    size_t offset;

    offset = GCGetFunRetOffset(funType);
    if (funType->retType->typeType != ERTT_VOID) {
        Type *funRetType;

        funRetType = GCFunParamType(funType->retType->type);
        //offset = ATreeAddrAlignByType(offset, funRetType);
        return offset + ATreeTypeSize(funRetType);
    } else {
        return offset;
    }
}

/*
 * 功能：计算寄存器在活动记录帧的偏移位置。
 * 返回值：
 **/
static size_t GCGetFrameRegBase(const FunType *funType)
{
    size_t offset;

    offset = GCGetFunTypeSize(funType);
    offset = Align8Byte(offset);
    return offset;
}

#if 0
/*
 * 功能：计算寄存器R0在活动记录栈帧的偏移位置。
 * 返回值：
 **/
static size_t GCGetSpR0Offset(const FunType *funType)
{
    size_t offset;

    offset = GCGetFrameRegBase(funType);
    offset += 4;    //LR
    offset += 4;    //SP
    offset += 4;    //PSR
    offset += 4;    //R3
    offset += 4;    //R2
    offset += 4;    //R1
    return offset;
}
#endif

/*
 * 功能：获取函数作用域局部变量区域在活动记录上的开始偏移位置。
 * 返回值：
 **/
static size_t GCGetFunLocalOffsetBase(const FunType *funType)
{
    size_t offset;

    offset = GCGetFrameRegBase(funType);
    offset += 7 * 4;
    offset = Align8Byte(offset);
    return offset;
}

/*
 * 功能：获取作用域中局部变量区域在活动记录上的开始偏移位置。
 * 返回值：
 **/
static size_t GCGetLocalDomainOffsetBase(Domain *domain)
{
    size_t offset = 0;

    if (domain->type == DT_FUN) {
        offset = GCGetFunLocalOffsetBase(domain->funType);
    } else if (domain->type == DT_BLK) {
        offset = GCGetLocalDomainOffsetBase(domain->prev);
        offset += domain->prev->offset_num;
        offset = Align8Byte(offset);
        offset += domain->prev->tmp_offset_num;
        offset = Align8Byte(offset);
    } else {
        PrErr("error: %d", domain->type);
        exit(-1);
    }
    return offset;
}

/*
 * 功能：获取作用域中局部变量在活动记录上的偏移位置。
 * 返回值：
 **/
static size_t GCGetLocalTokenOffset(Domain *domain, const Token *token)
{
    DomainEntry *domainEntry;
    size_t offset;

    domainEntry = EnvDomainGetEntry(domain, token);
    if (domainEntry->funArgFlag == 0) {    //非形式参数变量
        offset = GCGetLocalDomainOffsetBase(domainEntry->domain) + domainEntry->offset;
        return offset;
    } else {    //形式参数变量
        FunType *funType;

        if (domain->type == DT_FUN) {
            funType = domain->funType;
            offset = GCGetFunArgOffset(funType, token);
            return offset;
        } else {
            PrErr("domain->type: %d", domain->type);
            exit(-1);
        }
    }
}

/*
 * 功能：获取作用域临时变量区域在活动记录上的开始偏移位置。
 * 返回值：
 **/
static size_t GCGetLocalDomainTmpOffsetBase(Domain *domain)
{
    size_t offset;

    offset = GCGetLocalDomainOffsetBase(domain) + domain->offset_num;
    return Align8Byte(offset);
}

/*
 * 功能：获取临时变量在当前作用域中的偏移位置。
 * 返回值：
 **/
static size_t GCGetTmpOffset(Domain *domain, Temp *temp)
{
    if (temp->offsetFlag == 1) {    /*如果已经被分配了内存空间则返回它的偏移位置。*/
        return temp->offset;
    } else {    /*给临时变量分配内存空间。*/
        temp->offsetFlag = 1;

        if (temp->type->type == TT_BASE
                || temp->type->type == TT_POINTER) {
            domain->tmp_offset_num = ATreeAddrAlignByType(domain->tmp_offset_num, temp->type);
            temp->offset = domain->tmp_offset_num;
            domain->tmp_offset_num += ATreeTypeSize(temp->type);
        } else {
            PrErr("temp->type->type error: %d", temp->type->type);
            exit(-1);
        }
        return temp->offset;
    }
}

/*
 * 功能：获取作用域中临时变量在活动记录栈帧上的偏移位置。
 * 返回值：
 **/
static size_t GCGetDomainTmpOffset(Domain *domain, Temp *temp)
{
    size_t offset;

    if (domain->type == DT_FUN
            || domain->type == DT_BLK) {
        offset = GCGetTmpOffset(domain, temp);
        return GCGetLocalDomainTmpOffsetBase(domain) + offset;
    } else {
        PrErr("domain->type error: %d", domain->type);
        exit(-1);
    }
}

#if 0
/*
 * 功能：获取作用域中当有函数调用时新的活动记录相对于当前活动记录的开始偏移位置。
 * 返回值：函数调用的新的活动记录相对于当前活动记录的开始偏移位置。
 **/
static size_t GCGetFunSpOffset(Domain *domain)
{
    size_t offset;

    offset = GCGetLocalDomainTmpOffsetBase(domain) + domain->tmp_offset_num;
    offset = Align8Byte(offset);
    return offset;
}
#endif

static void GCRegLdId(GCInfo *info, uint8_t reg, Domain *idDomain, QRId *id, uint8_t spReg, int extraOffset);
static void GCRegLdAccessMem(GCInfo *info, uint8_t reg, QROperand *operand, uint8_t spReg, int extraOffset);
static void GCRegLdTemp(GCInfo *info, uint8_t reg, Domain *domain, Temp *temp, uint8_t spReg);
static void GCRegLdVal(GCInfo *info, uint8_t reg, QRValue *val);
static void GCRegLdMbrOffset(GCInfo *info, uint8_t reg, QRAccessMbrOffset *mbrOffset, uint8_t spReg);

static PIStType GCTypePIStType(const Type *type)
{
    AlignType alignType;

    alignType = ATreeTypeGetAlignType(type);
    switch (alignType) {
    case AT_1BYTE:
        return PISTT_1BYTE;
    case AT_2BYTE:
        return PISTT_2BYTE;
    case AT_4BYTE:
        return PISTT_4BYTE;
    default:
        PrErr("alignType error: %d", alignType);
        exit(-1);
    }
}

static PILdType GCTypePILdType(const Type *type)
{
    AlignType alignType;

    alignType = ATreeTypeGetAlignType(type);
    switch (alignType) {
    case AT_1BYTE:
        return PILDT_1BYTE;
    case AT_2BYTE:
        return PILDT_2BYTE;
    case AT_4BYTE:
        return PILDT_4BYTE;
    default:
        PrErr("alignType error: %d", alignType);
        exit(-1);
    }
}

/*
    arr[0] = 103;
    arr[1] = 104;
    arr[2] = 105;
*/

/*
 * 功能：把寄存器中的数据同步到关联的内存空间中。
 *      用到了scratch0, scratch1寄存器。
 * 返回值：
 **/
static void GCRegSt(GCInfo *info, uint8_t regId, int extraOffset)
{
    Register *reg = &info->reg[regId];

    if (reg->syncFlag) {
        Type *dataType = reg->dataType;
        PIStType pistType;

        reg->syncFlag = 0;
        pistType = GCTypePIStType(dataType);
        if (reg->memAddrType == MAT_GLOBAL) {
            uint8_t baseReg, offsetReg;

            baseReg = SCRATCH_REG0;
            offsetReg = SCRATCH_REG1;
            //ld rx, =v
            PCInsArrAddLdWPseudo(info->piArr, baseReg, GLOBAL_BASE + reg->memAddr);
            if (reg->amOffset != NULL) {
                GCRegLdMbrOffset(info, offsetReg, reg->amOffset, reg->spReg);
                if (reg->indirFlag) {
                    PCInsArrAddLdW(info->piArr, baseReg, baseReg, 0);
                }
                if (extraOffset) {
                    PCInsArrAddArith(info->piArr, PIT_ADD, offsetReg, offsetReg, extraOffset, PIAS2T_IMM);
                }
                PCInsArrAddStOffsetReg(info->piArr, pistType, reg->id, baseReg, offsetReg);
            } else {
                if (reg->indirFlag) {
                    PCInsArrAddLdW(info->piArr, baseReg, baseReg, 0);
                    PCInsArrAddSt(info->piArr, pistType, reg->id, baseReg, extraOffset);
                } else {
                    //stx rx, [ry]
                    PCInsArrAddSt(info->piArr, pistType, reg->id, baseReg, extraOffset);
                }
            }
        } else if (reg->memAddrType == MAT_LOCAL) {
            if (0 == reg->indirFlag) {
                if (reg->amOffset == NULL) {
                    //stx rx, [sp, #i]
                    PCInsArrAddSt(info->piArr, pistType, reg->id, reg->spReg, reg->memAddr + extraOffset);
                } else {
                    uint8_t offsetReg;

                    offsetReg = SCRATCH_REG1;
                    GCRegLdMbrOffset(info, offsetReg, reg->amOffset, reg->spReg);
                    PCInsArrAddArith(info->piArr, PIT_ADD, offsetReg, offsetReg, reg->memAddr + extraOffset, PIAS2T_IMM);
                    PCInsArrAddStOffsetReg(info->piArr, pistType, reg->id, reg->spReg, offsetReg);
                }
            } else {
                uint8_t baseReg;

                baseReg = SCRATCH_REG0;
                PCInsArrAddLdW(info->piArr, baseReg, reg->spReg, reg->memAddr);
                if (reg->amOffset == NULL) {
                    PCInsArrAddSt(info->piArr, pistType, reg->id, baseReg, extraOffset);
                } else {
                    uint8_t offsetReg;

                    offsetReg = SCRATCH_REG1;
                    GCRegLdMbrOffset(info, offsetReg, reg->amOffset, reg->spReg);
                    if (extraOffset) {
                        PCInsArrAddArith(info->piArr, PIT_ADD, offsetReg, offsetReg, extraOffset, PIAS2T_IMM);
                    }
                    PCInsArrAddStOffsetReg(info->piArr, pistType, reg->id, baseReg, offsetReg);
                }
            }
        } else {
            PrErr("reg->memAddrType error: %d", reg->memAddrType);
            exit(-1);
        }
    }
}

static void GCRegLdMbrOffset(GCInfo *info, uint8_t reg, QRAccessMbrOffset *mbrOffset, uint8_t spReg)
{
    if (mbrOffset->offsetType == QRAMOT_VAL) {
        PInstructArr *piArr = info->piArr;

        PCInsArrAddLdWPseudo(piArr, reg, mbrOffset->valOffset);
    } else if (mbrOffset->offsetType == QRAMOT_ID) {
        GCRegLdId(info, reg, mbrOffset->idOffset.domain, &mbrOffset->idOffset.id, spReg, 0);
    } else if (mbrOffset->offsetType == QRAMOT_TEMP) {
        GCRegLdTemp(info, reg, mbrOffset->tempOffset.domain, mbrOffset->tempOffset.temp, spReg);
    } else {
        PrErr("amOffset->offsetType error: %d", mbrOffset->offsetType);
        exit(-1);
    }
}

static Type *GCGetQRIdDataType(Domain *domain, QRId *id)
{
    DomainEntry *entry;
    Type *type;

    entry = EnvDomainGetEntry(domain, id->token);
    type = id->dataType;
    if (entry->funArgFlag) {
        if (type->type == TT_ARRAY) {
            return ATreeNewPointerType(type->array.type);
        } else if (type->type == TT_FUNCTION) {
            return ATreeNewPointerType(type);
        } else {
            return type;
        }
    } else {
        return type;
    }
}

static void GCRegLdId(GCInfo *info, uint8_t reg, Domain *idDomain, QRId *id, uint8_t spReg, int extraOffset)
{
    DomainEntry *domainEntry;
    Domain *declDomain;
    PILdType pildType;
    Type *type;

    domainEntry = EnvDomainGetEntry(idDomain, id->token);
    declDomain = domainEntry->domain;
    type = GCGetQRIdDataType(idDomain, id);
    pildType = GCTypePILdType(type);
    if (declDomain->type == DT_GLOBAL) {
        PCInsArrAddLdWPseudo(info->piArr, reg, GLOBAL_BASE + domainEntry->offset + extraOffset);
        PCInsArrAddLd(info->piArr, pildType, reg, reg, 0);
    } else if (declDomain->type == DT_FUN
               || declDomain->type == DT_BLK) {
        size_t offset;

        offset = GCGetLocalTokenOffset(declDomain, id->token);
        offset += extraOffset;
        PCInsArrAddLd(info->piArr, pildType, reg, spReg, offset);
    } else {
        PrErr("var domain->type error: %d", declDomain->type);
        exit(-1);
    }
}

static void GCRegLdAccessMem(GCInfo *info, uint8_t reg, QROperand *operand, uint8_t spReg, int extraOffset)
{
    PInstructArr *piArr = info->piArr;
    DomainEntry *domainEntry;
    PILdType piLdType;
    Domain *amDomain;
    QRAccessMbr *accessMbr;

    amDomain = operand->domain;
    accessMbr = &operand->accessMbr;
    piLdType = GCTypePILdType(QROperandDataType(operand));
    if (accessMbr->baseType == QRAMBT_ID) {
        Domain *declDomain;

        domainEntry = EnvDomainGetEntry(amDomain, accessMbr->baseId.token);
        declDomain = domainEntry->domain;
        if (declDomain->type == DT_GLOBAL) {
            PCInsArrAddLdWPseudo(piArr, reg, GLOBAL_BASE + domainEntry->offset);
            if (accessMbr->type == QRAMT_INDIRECT) {
                PCInsArrAddLd(piArr, PILDT_4BYTE, reg, reg, 0);
            }
            if (accessMbr->offset || extraOffset) {
                if (accessMbr->offset) {
                    GCRegLdMbrOffset(info, SCRATCH_REG0, accessMbr->offset, spReg);
                    if (extraOffset) {
                        PCInsArrAddArith(piArr, PIT_ADD, SCRATCH_REG0, SCRATCH_REG0, extraOffset, PIAS2T_IMM);
                    }
                } else {
                    PCInsArrAddLdWPseudo(piArr, SCRATCH_REG0, extraOffset);
                }
                PCInsArrAddLdOffsetReg(piArr, piLdType, reg, reg, SCRATCH_REG0);
            } else {
                PCInsArrAddLd(piArr, piLdType, reg, reg, 0);
            }
        } else if (declDomain->type == DT_FUN
                   || declDomain->type == DT_BLK) {
            int offset;

            offset = GCGetLocalTokenOffset(declDomain, accessMbr->baseId.token);
            if (accessMbr->offset || extraOffset) {
                if (accessMbr->offset) {
                    GCRegLdMbrOffset(info, SCRATCH_REG0, accessMbr->offset, spReg);
                    if (extraOffset) {
                        PCInsArrAddArith(piArr, PIT_ADD, SCRATCH_REG0, SCRATCH_REG0, extraOffset, PIAS2T_IMM);
                    }
                } else {
                    PCInsArrAddLdWPseudo(piArr, SCRATCH_REG0, extraOffset);
                }
                if (accessMbr->type == QRAMT_DIRECT) {
                    PCInsArrAddArith(piArr, PIT_ADD, SCRATCH_REG0, SCRATCH_REG0, offset, PIAS2T_IMM);
                    PCInsArrAddLdOffsetReg(piArr, piLdType, reg, spReg, SCRATCH_REG0);
                } else if (accessMbr->type == QRAMT_INDIRECT) {
                    PCInsArrAddLd(piArr, PILDT_4BYTE, reg, spReg, offset);
                    PCInsArrAddLdOffsetReg(piArr, piLdType, reg, reg, SCRATCH_REG0);
                } else {
                    PrErr("accessMem->type error: %d", accessMbr->type);
                    exit(-1);
                }
            } else {
                if (accessMbr->type == QRAMT_DIRECT) {
                    PCInsArrAddLd(piArr, piLdType, reg, spReg, offset);
                } else if (accessMbr->type == QRAMT_INDIRECT) {
                    PCInsArrAddLd(piArr, PILDT_4BYTE, reg, spReg, offset);
                    PCInsArrAddLd(piArr, piLdType, reg, reg, 0);
                } else {
                    PrErr("accessMem->type error: %d", accessMbr->type);
                    exit(-1);
                }
            }
        } else {
            PrErr("declDomain->type error: %d", declDomain->type);
            exit(-1);
        }
    } else if (accessMbr->baseType == QRAMBT_TEMP) {
        if (amDomain->type == DT_BLK
                || amDomain->type == DT_FUN) {
            if (accessMbr->type == QRAMT_INDIRECT) {
                size_t offset;

                offset = GCGetDomainTmpOffset(amDomain, accessMbr->baseTemp);
                PCInsArrAddLd(piArr, PILDT_4BYTE, reg, spReg, offset);
                if (accessMbr->offset || extraOffset) {
                    if (accessMbr->offset) {
                        GCRegLdMbrOffset(info, SCRATCH_REG0, accessMbr->offset, spReg);
                        if (extraOffset) {
                            PCInsArrAddArith(piArr, PIT_ADD, SCRATCH_REG0, SCRATCH_REG0, extraOffset, PIAS2T_IMM);
                        }
                    } else {
                        PCInsArrAddLdWPseudo(piArr, SCRATCH_REG0, extraOffset);
                    }
                    PCInsArrAddLdOffsetReg(piArr, piLdType, reg, reg, SCRATCH_REG0);
                } else {
                    PCInsArrAddLd(piArr, piLdType, reg, reg, 0);
                }
            } else {
                PrErr("accessMem->type error: %d", accessMbr->type);
                exit(-1);
            }
        } else {
            PrErr("amDomain->type error: %d", amDomain->type);
            exit(-1);
        }
    } else {
        PrErr("accessMem->baseType error: %d", accessMbr->baseType);
        exit(-1);
    }
}

static void GCRegLdTemp(GCInfo *info, uint8_t reg, Domain *domain, Temp *temp, uint8_t spReg)
{
    size_t offset;
    PILdType pildType;

    pildType = GCTypePILdType(temp->type);
    if (domain->type == DT_GLOBAL) {
        PrErr("temp domain->type error: %d", domain->type);
        exit(-1);
    } else {
        offset = GCGetDomainTmpOffset(domain, temp);
        PCInsArrAddLd(info->piArr, pildType, reg, spReg, offset);
    }
}

static void GCRegLdVal(GCInfo *info, uint8_t reg, QRValue *val)
{
    PInstructArr *piArr = info->piArr;

    if (val->type == QRVT_INT) {
        PCInsArrAddLdWPseudo(piArr, reg, val->ival);
    } else if (val->type == QRVT_FLOAT) {
        PrErr("val->type error: %d", val->type);
        exit(-1);
    } else {
        PrErr("val->type error: %d", val->type);
        exit(-1);
    }
}

static void GCRegLdGetAddr(GCInfo *info, uint8_t reg, Domain *gaDomain, QRGetAddr *getAddr)
{
    if (getAddr->type == QRGAT_ID) {
        DomainEntry *domainEntry;
        QRId *id = &getAddr->id;
        Domain *declDomain;

        domainEntry = EnvDomainGetEntry(gaDomain, id->token);
        declDomain = domainEntry->domain;
        if (declDomain->type == DT_GLOBAL) {
            PCInsArrAddLdWPseudo(info->piArr, reg, MAT_GLOBAL + domainEntry->offset);
        } else if (declDomain->type == DT_FUN
                   || declDomain->type == DT_BLK) {
            size_t offset;

            offset = GCGetLocalTokenOffset(declDomain, id->token);
            PCInsArrAddLdWPseudo(info->piArr, reg, offset);
            PCInsArrAddAdd(info->piArr, reg, reg, SP_REG_ID);
        } else {
            PrErr("var domain->type error: %d", declDomain->type);
            exit(-1);
        }
    } else if (getAddr->type == QRGAT_ACCESS_MEM) {
        QRAccessMbr *accessmbr = &getAddr->accessMbr;

        if (accessmbr->baseType == QRAMBT_ID) {
            DomainEntry *domainEntry;
            Domain *declDomain;

            domainEntry = EnvDomainGetEntry(gaDomain, accessmbr->baseId.token);
            declDomain = domainEntry->domain;
            if (declDomain->type == DT_GLOBAL) {
                PCInsArrAddLdWPseudo(info->piArr, reg, MAT_GLOBAL + domainEntry->offset);
                if (accessmbr->type == QRAMT_INDIRECT) {
                    PCInsArrAddLd(info->piArr, PILDT_4BYTE, reg, reg, 0);
                }
                if (accessmbr->offset) {
                    GCRegLdMbrOffset(info, SCRATCH_REG0, accessmbr->offset, SP_REG_ID);
                    PCInsArrAddAdd(info->piArr, reg, reg, SCRATCH_REG0);
                }
            } else if (declDomain->type == DT_FUN
                       || declDomain->type == DT_BLK) {
                size_t offset;

                offset = GCGetLocalTokenOffset(declDomain, accessmbr->baseId.token);
                if (accessmbr->type == QRAMT_DIRECT) {
                    PCInsArrAddLdWPseudo(info->piArr, reg, offset);

                    PCInsArrAddAdd(info->piArr, reg, reg, SP_REG_ID);
                } else if (accessmbr->type == QRAMT_INDIRECT){
                    PCInsArrAddLd(info->piArr, PILDT_4BYTE, reg, SP_REG_ID, offset);
                } else {
                    PrErr("accessmbr->type error: %d", accessmbr->type);
                    exit(-1);
                }
                if (accessmbr->offset) {
                    GCRegLdMbrOffset(info, SCRATCH_REG0, accessmbr->offset, SP_REG_ID);
                    PCInsArrAddAdd(info->piArr, reg, reg, SCRATCH_REG0);
                }
            } else {
                PrErr("var domain->type error: %d", declDomain->type);
                exit(-1);
            }
        } else if (accessmbr->baseType == QRAMBT_TEMP) {
            if (accessmbr->type == QRAMT_INDIRECT) {
                if (gaDomain->type == DT_BLK
                        || gaDomain->type == DT_FUN) {
                    GCRegLdTemp(info, reg, gaDomain, accessmbr->baseTemp, SP_REG_ID);
                    if (accessmbr->offset) {
                        GCRegLdMbrOffset(info, SCRATCH_REG0, accessmbr->offset, SP_REG_ID);
                        PCInsArrAddAdd(info->piArr, reg, reg, SCRATCH_REG0);
                    }
                } else {
                    PrErr("gaDomain->type error: %d", gaDomain->type);
                    exit(-1);
                }
            } else {
                PrErr("accessmbr->type error: %d", accessmbr->type);
                exit(-1);
            }
        } else {
            PrErr("accessmbr->baseType error: %d", accessmbr->baseType);
            exit(-1);
        }
    } else {
        PrErr("getAddr->gaType error: %d", getAddr->type);
        exit(-1);
    }
}

static void GCRegLdSpRegExtraOffset(GCInfo *info, uint8_t reg, QROperand *operand, uint8_t spReg, int extraOffset)
{
    if (operand->type == QROT_ID) {
        GCRegLdId(info, reg, operand->domain, &operand->id, spReg, extraOffset);
    } else if (operand->type == QROT_ACCESS_MBR) {
        GCRegLdAccessMem(info, reg, operand, spReg, extraOffset);
    } else if (operand->type == QROT_VAL) {
        GCRegLdVal(info, reg, &operand->val);
    } else if (operand->type == QROT_TEMP) {
        GCRegLdTemp(info, reg, operand->domain, operand->temp, spReg);
    } else if (operand->type == QROT_GET_ADDR) {
        GCRegLdGetAddr(info, reg, operand->domain, &operand->getAddr);
    } else {
        PrErr("operand->type error: %d", operand->type);
        exit(-1);
    }
}

static void GCRegLd(GCInfo *info, uint8_t reg, QROperand *operand)
{
    GCRegLdSpRegExtraOffset(info, reg, operand, SP_REG_ID, 0);
}

static void GCRegBoundId(GCInfo *info, uint8_t regId, Domain *idDomain, QRId *id, Type *type)
{
    DomainEntry *domainEntry;
    Domain *declDomain;
    Register *reg = &info->reg[regId];

    domainEntry = EnvDomainGetEntry(idDomain, id->token);
    declDomain = domainEntry->domain;
    if (declDomain->type == DT_GLOBAL) {
        reg->memAddrType = MAT_GLOBAL;
        reg->memAddr = domainEntry->offset;
    } else if (declDomain->type == DT_FUN
                || declDomain->type == DT_BLK) {
        reg->memAddrType = MAT_LOCAL;
        reg->memAddr = GCGetLocalTokenOffset(declDomain, id->token);
    } else {
        PrErr("domain->type error: %d", declDomain->type);
        exit(-1);
    }
    reg->dataType = type;
}

static void GCRegBoundTemp(GCInfo *info, uint8_t regId, Domain *domain, Temp *temp, Type *type)
{
    Register *reg = &info->reg[regId];

    if (domain->type == DT_GLOBAL) {
        PrErr("domain->type: %d", domain->type);
        exit(-1);
    } else if (domain->type == DT_FUN
               || domain->type == DT_BLK) {
        reg->memAddrType = MAT_LOCAL;
        reg->memAddr = GCGetDomainTmpOffset(domain, temp);
    } else {
        PrErr("expr->domain->type error: %d", domain->type);
        exit(-1);
    }
    reg->dataType = type;
}

static void GCRegBoundAccessMem(GCInfo *info, uint8_t regId, Domain *amDomain, QRAccessMbr *accessMem, Type *type)
{
    Register *reg = &info->reg[regId];

    if (accessMem->baseType == QRAMBT_ID) {
        GCRegBoundId(info, regId, amDomain, &accessMem->baseId, type);
    } else if (accessMem->baseType == QRAMBT_TEMP) {
        GCRegBoundTemp(info, regId, amDomain, accessMem->baseTemp, type);
    } else {
        PrErr("accessMem->baseType error: %d", accessMem->baseType);
        exit(-1);
    }
    reg->dataType = accessMem->dataType;
    reg->amOffset = accessMem->offset;
    if (accessMem->type == QRAMT_DIRECT) {
        reg->indirFlag = 0;
    } else if (accessMem->type == QRAMT_INDIRECT) {
        reg->indirFlag = 1;
    } else {
        PrErr("accessMem->accessType error: %d", accessMem->type);
        exit(-1);
    }
}

static void GCRegBoundAssignSp(GCInfo *info, uint8_t regId, QROperand *operand, uint8_t spReg)
{
    Register *reg = &info->reg[regId];

    reg->amOffset = NULL;
    reg->dataType = NULL;
    reg->indirFlag = 0;
    reg->syncFlag = 1;
    reg->spReg = spReg;
    if (operand->type == QROT_ID) {
        GCRegBoundId(info, regId, operand->domain, &operand->id, QROperandDataType(operand));
    } else if (operand->type == QROT_ACCESS_MBR) {
        GCRegBoundAccessMem(info, regId, operand->domain, &operand->accessMbr, QROperandDataType(operand));
    } else if (operand->type == QROT_TEMP) {
        GCRegBoundTemp(info, regId, operand->domain, operand->temp, QROperandDataType(operand));
    } else {
        PrErr("operand->type error: %d", operand->type);
        exit(-1);
    }
}

static void GCRegBound(GCInfo *info, uint8_t regId, QROperand *operand)
{
    GCRegBoundAssignSp(info, regId, operand, SP_REG_ID);
}

#if 0
static void GCRegBoundAddr(GCInfo *info, uint8_t regId, MemAddrType addrType, size_t addr, Type *dataType, uint8_t spReg)
{
    Register *reg = &info->reg[regId];

    reg->syncFlag = 1;
    reg->indirFlag = 0;
    reg->amOffset = NULL;
    reg->dataType = dataType;
    reg->memAddrType = addrType;
    reg->memAddr = addr;
    reg->spReg = spReg;
}
#endif

static int GCGenArithInstruct(GCInfo *info, const QuadRuple *quadRuple)
{
    PInstType pInstType;
    uint8_t dstReg, src1Reg, src2Reg;

    dstReg = 0;
    src1Reg = 0;
    src2Reg = 1;
    GCRegLd(info, src1Reg, quadRuple->binOp.arg1);
    GCRegLd(info, src2Reg, quadRuple->binOp.arg2);
    GCRegBound(info, dstReg, quadRuple->binOp.result);
    switch (quadRuple->op) {
    case QROC_ADD:
        pInstType = PIT_ADD;
        break;
    case QROC_SUB:
        pInstType = PIT_SUB;
        break;
    case QROC_MUL:
        pInstType = PIT_MUL;
        break;
    case QROC_DIV:
        pInstType = PIT_DIV;
        break;
    case QROC_MOD:
        pInstType = PIT_MOD;
        break;
    default:
        PrErr("quadRuple->op error: %d", quadRuple->op);
        exit(-1);
    }
    //opc rx, ry, rz
    PCInsArrAddArith(info->piArr, pInstType, dstReg, src1Reg, src2Reg, PIAS2T_REG);
    GCRegSt(info, dstReg, 0);
    return 0;
}

static int GCGenUncondJumpInstruct(GCInfo *info, const QuadRuple *quadRuple)
{
    //b #v
    PCInsArrAddB(info->piArr, quadRuple->uncondJump.dstLabel, PC_ADDR(info->piArr));
    return 0;
}

/*
 * 功能：生成条件跳转指令
 * 返回值：
 */
static int GCGenCondJumpInstuct(GCInfo *info, const QuadRuple *quadRuple)
{
    const Expr *cond;
    uint8_t dstReg, src1Reg, src2Reg;
    PInstructArr *piArr = info->piArr;
    PIBCondType piBCondType;

    dstReg = 0;
    src1Reg = 0;
    src2Reg = 1;
    cond = quadRuple->condJump.cond;

    if (cond->type == ET_EQ
            || cond->type == ET_NE
            || cond->type == ET_GT
            || cond->type == ET_GE
            || cond->type == ET_LT
            || cond->type == ET_LE) {
        GCRegLd(info, src1Reg, QROperandFromExpr(cond->binOp.lExpr));
        GCRegLd(info, src2Reg, QROperandFromExpr(cond->binOp.rExpr));
        //sub rx, ry, rz
        PCInsArrAddSub(piArr, dstReg, src1Reg, src2Reg);

        if (quadRuple->op == QROC_TRUE_JUMP) {
            switch (cond->type) {
            case ET_EQ:
                piBCondType = PIBCT_EQ;
                break;
            case ET_NE:
                piBCondType = PIBCT_NE;
                break;
            case ET_GT:
                piBCondType = PIBCT_GT;
                break;
            case ET_GE:
                piBCondType = PIBCT_GE;
                break;
            case ET_LT:
                piBCondType = PIBCT_LT;
                break;
            case ET_LE:
                piBCondType = PIBCT_LE;
                break;
            default:
                PrErr("error exprType: %d", cond->type);
                exit(-1);
                break;
            }
        } else if (quadRuple->op == QROC_FALSE_JUMP) {
            switch (cond->type) {
            case ET_EQ:
                piBCondType = PIBCT_NE;
                break;
            case ET_NE:
                piBCondType = PIBCT_EQ;
                break;
            case ET_GT:
                piBCondType = PIBCT_LE;
                break;
            case ET_GE:
                piBCondType = PIBCT_LT;
                break;
            case ET_LT:
                piBCondType = PIBCT_GE;
                break;
            case ET_LE:
                piBCondType = PIBCT_GT;
                break;
            default:
                PrErr("error exprType: %d", cond->type);
                exit(-1);
                break;
            }
        } else {
            PrErr("error quadRuple->op: %d", quadRuple->op);
            exit(-1);
        }
        //bcz rx, #v
        PCInsArrAddBCond(piArr, piBCondType, dstReg, quadRuple->condJump.dstLabel, PC_ADDR(info->piArr));
    } else if (cond->type == ET_CONST
               || cond->type == ET_ID
               || cond->type == ET_TEMP
               || cond->type == ET_ACCESS_ELM
               || cond->type == ET_CONST_SPC
               || cond->type == ET_ACCESS_MBR
               || cond->type == ET_REF_POINTER) {
        GCRegLd(info, dstReg, QROperandFromExpr(quadRuple->condJump.cond));
        if (quadRuple->op == QROC_TRUE_JUMP) {
            //bnez rx, #v
            PCInsArrAddBCond(piArr, PIBCT_NE, dstReg, quadRuple->condJump.dstLabel, PC_ADDR(info->piArr));
        } else if (quadRuple->op == QROC_FALSE_JUMP) {
            //beqz rx, #v
            PCInsArrAddBCond(piArr, PIBCT_EQ, dstReg, quadRuple->condJump.dstLabel, PC_ADDR(info->piArr));
        } else {
            PrErr("error quadRuple->op: %d", quadRuple->op);
            exit(-1);
        }
    } else {
        PrErr("exprType error: %d", cond->type);
        exit(-1);
    }

    return 0;
}

static size_t GCAlignTypeStep(AlignType align)
{
    switch (align) {
    case AT_1BYTE:
        return 1;
    case AT_2BYTE:
        return 2;
    case AT_4BYTE:
        return 4;
    case AT_8BYTE:
        return 8;
    default:
        PrErr("align error:%d", align);
        exit(-1);
    }
}

/*
 * 功能：生成赋值指令
 * 返回值：
 */
static int GCGenAssignInstruct(GCInfo *info, const QuadRuple *quadRuple)
{
    uint8_t dstReg;
    QROperand *rValue, *lValue;
    size_t size, step;
    Type *type;

    dstReg = 0;
    rValue = quadRuple->unaryOp.arg1;
    lValue = quadRuple->unaryOp.result;
    type = QROperandDataType(rValue);
    size = ATreeTypeSize(type);
    step = GCAlignTypeStep(ATreeTypeGetAlignType(type));
    for (size_t i = 0; i < size; i += step) {
        GCRegLdSpRegExtraOffset(info, dstReg, rValue, SP_REG_ID, i);
        GCRegBound(info, dstReg, lValue);
        GCRegSt(info, dstReg, i);
    }
    return 0;
}

/*
 * 功能：生成返回指令
 * 返回值：
 **/
static int GCGenReturnInstruct(GCInfo *info, const QuadRuple *quadRuple)
{
    uint8_t dstReg;
    FunType *funType;
    size_t regOffset, lrOffset;
    PInstructArr *piArr = info->piArr;

    /*获取返回指令所在的函数作用域的函数类型。*/
    funType = quadRuple->returnObj.domain->funDomain->funType;
    /*如果有返回值，则把返回值存储到活动记录栈帧函数返回值处。*/
    if (quadRuple->op == QROC_RETURN_VALUE) {
        Type *retType;

        retType = funType->retType->type;
        if (funType->retType->typeType == ERTT_VOID) {
            PrErr("funType->retType->typeType error: %d", funType->retType->typeType);
            exit(-1);
        }

        dstReg = TMP_REG;
        {
            PIStType piStType;
            size_t step;
            size_t size;
            size_t retOffset;

            size = ATreeTypeSize(retType);
            piStType = GCTypePIStType(retType);
            step = GCAlignTypeStep(ATreeTypeGetAlignType(retType));
            retOffset = GCGetFunRetOffset(funType);
            for (size_t i = 0; i < size; i += step) {
                GCRegLdSpRegExtraOffset(info, dstReg, quadRuple->returnObj.expr, SP_REG_ID, i);
                PCInsArrAddSt(piArr, piStType, dstReg, SP_REG_ID, retOffset + i);
            }
        }
    }

    /*恢复上一级活动的上下文*/
    regOffset = GCGetFrameRegBase(funType);
    lrOffset = regOffset;
    regOffset += 4;     //LR寄存器
    regOffset += 4;     //SP寄存器
    //ld psr, [sp, #i]
    PCInsArrAddLdW(piArr, PSR_REG_ID, SP_REG_ID, regOffset);
    regOffset += 4;
    //ld r3, [sp, #i]
    PCInsArrAddLdW(piArr, 3, SP_REG_ID, regOffset);
    regOffset += 4;
    //ld r2, [sp, #i]
    PCInsArrAddLdW(piArr, 2, SP_REG_ID, regOffset);
    regOffset += 4;
    //ld r1, [sp, #i]
    PCInsArrAddLdW(piArr, 1, SP_REG_ID, regOffset);
    regOffset += 4;
    //ld r0, [sp, #i]
    PCInsArrAddLdW(piArr, 0, SP_REG_ID, regOffset);
    //ld pc, [sp, #i]
    PCInsArrAddLdW(piArr, PC_REG_ID, SP_REG_ID, lrOffset);

    return 0;
}

/*
 * 功能：先把实际参数压入参数链表
 * 返回值：
 **/
static int GCGenFunParamInstruct(GCInfo *info, const QuadRuple *quadRuple)
{
    GCParamListPush(&info->paramList, quadRuple);
    return 0;
}

/*
 * 功能：生成函数调用实际参数指令，把实际参数传递到函数活动记录中。
 * 返回值：
 **/
static int GCGenFunCallParam(GCInfo *info, size_t *spOffset, size_t paramCnt, char sign)
{
    if (paramCnt == 0) {
        return 0;
    } else {
        const QuadRuple *paramRuple;
        uint8_t transReg;
        QROperand *operand;
        Type *paramType;
        uint8_t spOffsetReg;

        spOffsetReg = SP_OFFSET_REG_ID;
        /*把参数加载到寄存器，然后再保存到函数活动记录栈帧指定的地方。*/
        transReg = TRAN_DATA_REG_ID;
        paramRuple = GCParamListPop(&info->paramList);

        operand = paramRuple->funParam.expr;
        paramType = GCFunParamType(QROperandDataType(operand));
        *spOffset = GCFunArgAddrAlignByType(*spOffset, paramType);
        {
            size_t typeSize;
            size_t offset = *spOffset;
            size_t step;
            PIStType piStType;

            typeSize = ATreeTypeSize(paramType);
            piStType = GCTypePIStType(paramType);
            step = GCAlignTypeStep(ATreeTypeGetAlignType(paramType));
            for (size_t i = 0; i < typeSize; i+= step) {
                GCRegLdSpRegExtraOffset(info, transReg, operand, spOffsetReg, i);
                PCInsArrAddSt(info->piArr, piStType, transReg, SP_REG_ID, (offset + i) * sign);
            }
        }
        *spOffset += ATreeTypeSize(paramType);
        GCGenFunCallParam(info, spOffset, --paramCnt, sign);
    }
    return 0;
}

typedef struct {
    struct list_head node;
    size_t instNum;
} SetActSpNode;

void GCSetActSpListAdd(struct list_head *list, size_t instNum)
{
    SetActSpNode *sasNode;

    sasNode = GCMAlloc(sizeof (*sasNode));
    sasNode->instNum = instNum;
    list_add(&sasNode->node, list);
}

/*
 * 功能：生成函数调用指令
 * 返回值：
 **/
static int GCGenFunCallInstruct(GCInfo *info, const QuadRuple *quadRuple)
{
    DomainEntry *domainEntry;
    FunType *funType;
    Domain *domain;
    size_t regOffset;
    uint8_t spOffsetReg;
    PInstructArr *piArr = info->piArr;
    Type *funRetType;
    size_t paramsSize = 0;
    size_t vArgSize;  /*传递的变参大小*/
    PInstruct *setActSp;    //设置活动记录栈基地址伪指令
    uint8_t tmpReg;
    size_t funArgsCnt = 0;

    tmpReg = TMP_REG;
    spOffsetReg = SP_OFFSET_REG_ID;
    domain = quadRuple->funCall.domain;     /*函数调用所在的作用域*/
    domainEntry = EnvDomainGetEntry(domain, quadRuple->funCall.token);
    funType = &domainEntry->type->funType;

    /*设置下一级的堆栈基地址*/
    PCInsArrAddMovReg(piArr, spOffsetReg, SP_REG_ID);
    PCInsArrAddSetActSp(piArr);
    setActSp = &piArr->insArr[piArr->idx-1];
    GCSetActSpListAdd(&info->setActSpList, piArr->idx-1);

    /*传递函数实际参数*/
    funArgsCnt = GCGetFunArgsCount(funType->funArgs);
    if (funArgsCnt == quadRuple->funCall.paraCnt) {
        vArgSize = 0;
    } else {
        GCGenFunCallParam(info, &vArgSize, quadRuple->funCall.paraCnt - funArgsCnt, -1);
        vArgSize = Align8Byte(vArgSize);
    }
    GCGenFunCallParam(info, &paramsSize, funArgsCnt, 1);
    setActSp->piSetActSp.vArgSize = vArgSize;

    /*设置返回地址*/
    regOffset = GCGetFrameRegBase(funType);
    //st r0, [sp, #i]
    PCInsArrAddStW(piArr, spOffsetReg, SP_REG_ID, regOffset + 4);     //保存上一级活动记录栈基地址。
    //ld r0, =v
    PCInsArrAddLdWPseudo(piArr, tmpReg, PC_ADDR(piArr) + 4 * 3);
    PCInsArrAddStW(piArr, tmpReg, SP_REG_ID, regOffset);   //保存上一级活动记录的栈基地址

    /*调用函数*/
    //call fun
    PCInsArrAddCall(piArr, quadRuple->funCall.token, funType, PC_ADDR(info->piArr));

    /*恢复堆栈基地址*/
    PCInsArrAddMovReg(piArr, spOffsetReg, SP_REG_ID);
    regOffset = GCGetFrameRegBase(funType);
    regOffset += 4;
    PCInsArrAddLd(piArr, PILDT_4BYTE, SP_REG_ID, SP_REG_ID, regOffset);

    /*处理函数返回值*/
    if (quadRuple->op == QROC_FUN_CALL) {
        PILdType piLdType;
        size_t retOffset;

        if (funType->retType->typeType == ERTT_VOID) {
            PrErr("funType->retType->typeType error: %d", funType->retType->typeType);
            exit(-1);
        }
        retOffset = GCGetFunRetOffset(funType);
        funRetType = funType->retType->type;
        if (funRetType->type != TT_STRUCTURE) {
            piLdType = GCTypePILdType(funRetType);
            PCInsArrAddLd(piArr, piLdType, tmpReg, spOffsetReg, retOffset);
            GCRegBound(info, tmpReg, quadRuple->funCall.result);
            GCRegSt(info, tmpReg, 0);
        } else {
            size_t size;
            size_t step;
            size_t i;

            size = ATreeTypeSize(funRetType);
            piLdType = GCTypePILdType(funRetType);
            step = GCAlignTypeStep(ATreeTypeGetAlignType(funRetType));
            for (i = 0; i < size; i += step) {
                PCInsArrAddLd(piArr, piLdType, tmpReg, spOffsetReg, retOffset + i);
                GCRegBound(info, tmpReg, quadRuple->funCall.result);
                GCRegSt(info, tmpReg, i);
            }
        }
    }

    return 0;
}

/*
 * 功能：生成函数定义开始处的指令
 * 返回值：
 **/
static int GCGenFunStartInstruct(GCInfo *info, const QuadRuple *quadRuple)
{
    size_t regOffset;
    FunType *funType;
    PInstructArr *piArr = info->piArr;

    funType = quadRuple->funStartFlag.funType;
    funType->addr = PC_ADDR(piArr);

    /*保存上一级调用的机器状态*/
    regOffset = GCGetFrameRegBase(funType);
    regOffset += 4; //LR寄存器
    regOffset += 4; //SP寄存器
    //st psr [sp, #i]
    PCInsArrAddStW(piArr, PSR_REG_ID, SP_REG_ID, regOffset);
    regOffset += 4; //PSR寄存器
    //st r3 [sp, #i]
    PCInsArrAddStW(piArr, 3, SP_REG_ID, regOffset);
    regOffset += 4; //R3寄存器
    //st r2 [sp, #i]
    PCInsArrAddStW(piArr, 2, SP_REG_ID, regOffset);
    regOffset += 4; //R2寄存器
    //st r1 [sp, #i]
    PCInsArrAddStW(piArr, 1, SP_REG_ID, regOffset);
    regOffset += 4; //R1寄存器
    //st r0 [sp, #i]
    PCInsArrAddStW(piArr, 0, SP_REG_ID, regOffset);

    return 0;
}

static size_t GCGetDomainSize(Domain *domain)
{
    size_t curSize;
    size_t maxNextSize = 0;
    NextDomain *nextDomain;
    struct list_head *pos;

    if (domain->type != DT_FUN
            && domain->type != DT_BLK) {
        return 0;
    }
    curSize = Align8Byte(domain->offset_num);
    curSize += Align8Byte(domain->tmp_offset_num);

    list_for_each(pos, &domain->nextList) {
        size_t nextDomainSize;

        nextDomain = container_of(pos, NextDomain, node);
        nextDomainSize = GCGetDomainSize(nextDomain->domain);
        if (nextDomainSize > maxNextSize)
            maxNextSize = nextDomainSize;
    }
    return curSize + maxNextSize;
}

static int GCGenFunEndInstruct(GCInfo *info, const QuadRuple *quadRuple)
{
    SetActSpNode *sasNode;
    struct list_head *pos;
    struct list_head *list = &info->setActSpList;
    Domain *funDomain = quadRuple->funEnd.domain;
    FunType *funType = funDomain->funType;
    size_t offset;
    PInstruct *pInst;

    offset = GCGetFunLocalOffsetBase(funType);
    offset = Align8Byte(offset);

    offset += GCGetDomainSize(funDomain);
    offset = Align8Byte(offset);
    for (pos = list->next; pos != list; ) {
        size_t vArgSize;

        sasNode = container_of(pos, SetActSpNode, node);
        pos = pos->next;
        list_del(&sasNode->node);
        pInst = &info->piArr->insArr[sasNode->instNum];
        vArgSize = pInst->piSetActSp.vArgSize;

        pInst->type = PIT_ADD;
        pInst->piArith.dstRegId = SP_REG_ID;
        pInst->piArith.src1RegId = SP_REG_ID;
        pInst->piArith.src2Type = PIAS2T_IMM;
        pInst->piArith.src2 = offset + vArgSize;
    }
    return 0;
}

static int GCGenBlkStartInstruct(GCInfo *info, const QuadRuple *quadRuple)
{
    (void)info, (void)quadRuple;
    return 0;
}

static int GCGenBlkEndInstruct(GCInfo *info, const QuadRuple *quadRuple)
{
    Domain *domain = quadRuple->blkEnd.domain;
    Domain *funDomain = domain->funDomain;

    (void)info;
    if (domain->type == DT_BLK
            || domain->type == DT_FUN) {
        funDomain->allLocalVarSize += domain->offset_num;
        funDomain->allTmpVarSize += domain->tmp_offset_num;
    }
    return 0;
}

static int GCGenInstruct(GCInfo *info, const QuadRuple *quadRuple)
{
    switch (quadRuple->op) {
    case QROC_ADD:
    case QROC_SUB:
    case QROC_MUL:
    case QROC_DIV:
    case QROC_MOD:
        return GCGenArithInstruct(info, quadRuple);
    case QROC_JUMP:
        return GCGenUncondJumpInstruct(info, quadRuple);
    case QROC_TRUE_JUMP:
    case QROC_FALSE_JUMP:
        return GCGenCondJumpInstuct(info, quadRuple);
    case QROC_ASSIGN:
        return GCGenAssignInstruct(info, quadRuple);
    case QROC_RETURN_VOID:
    case QROC_RETURN_VALUE:
        return GCGenReturnInstruct(info, quadRuple);
    case QROC_FUN_PARAM:
        return GCGenFunParamInstruct(info, quadRuple);
    case QROC_FUN_CALL:
    case QROC_FUN_CALL_NRV:
        return GCGenFunCallInstruct(info, quadRuple);
    case QROC_FUN_START:
        return GCGenFunStartInstruct(info, quadRuple);
    case QROC_FUN_END:
        return GCGenFunEndInstruct(info, quadRuple);;
    case QROC_BLK_START:
        return GCGenBlkStartInstruct(info, quadRuple);;
    case QROC_BLK_END:
        return GCGenBlkEndInstruct(info, quadRuple);;
    case QROC_PROGRAM_START:
        return 0;
    case QROC_PROGRAM_END:
        return 0;
    default:
        PrErr("quadRuple->op error: %u", quadRuple->op);
        exit(-1);
    }
}

static GCInfo *GCAllocInfo(void)
{
    GCInfo *gcInfo;

    gcInfo = GCMAlloc(sizeof (*gcInfo));
    gcInfo->addrNum = 0;
    for (size_t k = 0; k < REG_CNT; k++) {
        gcInfo->reg[k].memAddr = 0;
        gcInfo->reg[k].id = k;
        gcInfo->reg[k].syncFlag = 0;
        gcInfo->reg[k].dataType = NULL;
    }
    INIT_LIST_HEAD(&gcInfo->paramList);
    gcInfo->piArr = PCAllocInsArr();
    gcInfo->amTable = GCAMTableAlloc();
    gcInfo->miArr = MCALLocInsArr();
    gcInfo->vm = VMAllocMachine();
    INIT_LIST_HEAD(&gcInfo->setActSpList);
    return gcInfo;
}

/*
 * 功能：生成启动指令
 * 返回值：
 **/
static int GCGenBootInstructs(GCInfo *info)
{
    size_t offset = 0;
    Register *dstReg;
    PInstructArr *piArr = info->piArr;

    dstReg = &info->reg[0];

    /*设置堆栈起始地址*/
    //ld sp, =v
    PCInsArrAddLdWPseudo(piArr, SP_REG_ID, SP_BASE);

    /*为main函数设置堆栈结构*/
    //mov rx, 0
    //PCInsArrAddMov(piArr, dstReg->id, 0);
    //st rx [sp, #i]
    //PCInsArrAddStW(piArr, dstReg->id, SP_REG_ID, offset);
    //offset += 4   //R0
    //mov rx, 0
    //PCInsArrAddMov(piArr, dstReg->id, 0);
    //st rx [sp, #i]
    //PCInsArrAddStW(piArr, dstReg->id, SP_REG_ID, offset);
    //offset += 4   //R1

    offset += 4;    //返回值

    offset = Align8Byte(offset);
    /*设置返回地址, LR*/
    //ld rx, =v
    PCInsArrAddLdWPseudo(piArr, dstReg->id, PC_ADDR(info->piArr) + 3 * 4);
    //st rx, [sp, #i]
    PCInsArrAddStW(piArr, dstReg->id, SP_REG_ID, offset);
    //call main
    PCInsArrAddCall(piArr, bison->mainToken, bison->mainFunType, PC_ADDR(info->piArr));

    //halt
    PCInsArrAddHalt(piArr);
    return 0;
}

static int GCFillPCDstAddr(GCInfo *info, PInstructArr *piArr)
{
    size_t i;
    PInstruct *pInstruct;

    for (i = 0; i < piArr->idx; i++) {
        pInstruct = &piArr->insArr[i];
        if (pInstruct->type == PIT_B) {
            PIB *piB = &pInstruct->piB;

            if (piB->pcAddrFlag == 0) {
                piB->pcAddrFlag = 1;
                if (piB->icDstAddr >= info->amTable->idx) {
                    PrErr("piB->icDstAddr too big: 0x%04x", piB->icDstAddr);
                    exit(-1);
                }
                piB->pcDstAddr = info->amTable->maps[piB->icDstAddr];
            }
        } else if (pInstruct->type == PIT_BCOND) {
            PIBCond *piBCond = &pInstruct->piBCond;

            if (piBCond->icDstAddr >= info->amTable->idx) {
                PrErr("piBCond->icDstAddr too big: 0x%04x", piBCond->icDstAddr);
                exit(-1);
            }
            piBCond->pcDstAddr = info->amTable->maps[piBCond->icDstAddr];
        } else if (pInstruct->type == PIT_CALL) {
            PICall *piCall = &pInstruct->piCall;

            piCall->dstAddr = piCall->funType->addr;
        }
    }
    return 0;
}

void GCHandleLdPseudo(PInstructArr *piArr)
{
    size_t i = 0;
    size_t cnt = piArr->idx;

    for (i = 0; i < cnt; i++) {
        PInstruct *pInst;

        pInst = &piArr->insArr[i];
        if (pInst->type == PIT_LD) {
            PILd *piLd = &pInst->piLd;

            if (piLd->pseudoFlag) {
                uint32_t data;

                data = *(uint32_t *)&piLd->pseudo.data;
                piLd->pseudoFlag = 0;
                piLd->machine.memRegId = PC_REG_ID;
                piLd->machine.offsetType = PIOT_IMM;
                piLd->machine.offset.imm = (piArr->idx - i) * 4;
                PCInsArrAddLdData(piArr, data);
            }
        }
    }
}

int GCGenCode(const QRRecord *record)
{
    size_t i;
    GCInfo *info;

    if (!record)
        return -EINVAL;
    info = GCAllocInfo();
    GCGenBootInstructs(info);
    for (i = 0; i < record->idx; i++) {
        GCAMTableAdd(info->amTable, PC_ADDR(info->piArr));
        GCGenInstruct(info, &record->qrArr[i]);
    }
    PCInsArrPrint(info->piArr);

#if 1
    GCFillPCDstAddr(info, info->piArr);
    PCInstArrSetDataStart(info->piArr, info->piArr->idx);
    GCHandleLdPseudo(info->piArr);
    printf("=================================\n");
    PCInsArrPrint(info->piArr);

    MCGenInstructs(info->miArr, info->piArr);
    printf("------------------------------\n");
    MCInsArrPrint(info->miArr);

    printf("++++++++++++++++++++++++++++++++++\n");
    VMachineExec(info->vm, info->miArr->insArr, info->miArr->idx);
#endif

    return 0;
}

/**
 * 1. 生成4字节的记录形式。
 * 2. 翻译4字节的记录，生成代码。
 * 3. 规范指令的使用形式。
 * 4. 伪指令该有哪些。
 * 5. 调用函数时的地址。
 * 6. 地址对齐是前端处理还是后端处理。
 * 7. 函数传参顺序
 * 8. 中间代码的地址和生成代码的地址不一致
 * 9. 生成中间代码时，当检测到函数调用时应该检测实际参数跟形式参数是否一致。
 *
**/
