#include "backpatch.h"
#include "cpl_mm.h"
#include "cpl_errno.h"
#include "cpl_debug.h"
#include "stdlib.h"
#include "quadruple.h"

#define PrErr(...)      Pr(__FILE__, __LINE__, __FUNCTION__, "error", __VA_ARGS__)

#define BP_DEBUG
#ifdef BP_DEBUG
#define PrDbg(...)      Pr(__FILE__, __LINE__, __FUNCTION__, "debug", __VA_ARGS__)
#endif

static void *BPMAlloc(size_t size)
{
    void *p;

    p = CplAlloc(size);
    if (!p) {
        PrErr("no memory");
        exit(-1);
    }
    return p;
}

static BPInstruct *BPNewInstruct(size_t addr)
{
    BPInstruct *instruct;

    instruct = BPMAlloc(sizeof (*instruct));
    instruct->addr = addr;
    return instruct;
}

BPInsList *BPAllocInsList(void)
{
    BPInsList *bpil;

    bpil = BPMAlloc(sizeof (*bpil));
    INIT_LIST_HEAD(&bpil->list);
    return bpil;
}

int BPInsListAddInstruct(BPInsList *bpil, size_t addr)
{
    BPInstruct *instruct;

    if (!bpil)
        return -EINVAL;
    instruct = BPNewInstruct(addr);
    list_add_tail(&instruct->node, &bpil->list);
    return 0;
}

BPInsList *BPMakeInsList(size_t addr)
{
    BPInsList *bpil;
    BPInstruct *instruct;

    bpil = BPMAlloc(sizeof (*bpil));
    INIT_LIST_HEAD(&bpil->list);
    instruct = BPNewInstruct(addr);
    list_add_tail(&instruct->node, &bpil->list);
    return bpil;
}

void BPInsListMerge(BPInsList *lList, BPInsList *rList)
{
    struct list_head *pos;
    BPInstruct *instruct;

    if (!lList || !rList)
        return;

    for (pos = rList->list.next; pos != &rList->list; ) {
        instruct = container_of(pos, BPInstruct, node);
        pos = pos->next;
        list_del(&instruct->node);
        list_add_tail(&instruct->node, &lList->list);
    }
    CplFree(rList);
}

static inline void BPInstructSetLabel(QuadRuple *ruple, size_t label)
{
    if (!ruple)
        return;

    switch (ruple->op) {
    case QROC_JUMP:
        ruple->uncondJump.dstLabel = label;
        break;
    case QROC_TRUE_JUMP:
    case QROC_FALSE_JUMP:
        ruple->condJump.dstLabel = label;
        break;
    default:
        PrErr("unknown ruple op");
        exit(-1);
        break;
    }
}

void BPInsListBackPatch(BPInsList *bpil, QRRecord *record, size_t addr)
{
    struct list_head *pos;
    BPInstruct *instruct;
    QuadRuple *quadRuple;

    if (!bpil)
        return;
    list_for_each(pos, &bpil->list) {
        instruct = container_of(pos, BPInstruct, node);
        quadRuple = &record->qrArr[instruct->addr];
        BPInstructSetLabel(quadRuple, addr);
    }
    BPInsListFree(bpil);
}

void BPInsListFree(BPInsList *bpil)
{
    struct list_head *pos;
    BPInstruct *instruct;

    if (!bpil)
        return;
    for (pos = bpil->list.next; pos != &bpil->list; ) {
        instruct = container_of(pos, BPInstruct, node);
        pos = pos->next;
        list_del(&instruct->node);
        CplFree(instruct);
    }
    CplFree(bpil);
}

