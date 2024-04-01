#ifndef __BACKPATCH_H__
#define __BACKPATCH_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "list.h"
#include "stdint.h"

typedef struct _QRRecord QRRecord;

/*回填指令链表节点*/
typedef struct {
    struct list_head node;
    size_t addr;        /*待回填指令地址*/
} BPInstruct;

/*回填指令链表*/
typedef struct _BPInsList {
    struct list_head list;  /*指令链表，节点类型：BPInstruct*/
} BPInsList;

BPInsList *BPAllocInsList(void);
//int BPInsListAddInstruct(BPInsList *bpil, QuadRuple *ruple);
int BPInsListAddInstruct(BPInsList *bpil, size_t addr);
//BPInsList *BPMakeInsList(QuadRuple *ruple);
BPInsList *BPMakeInsList(size_t addr);
void BPInsListMerge(BPInsList *lBpil, BPInsList *rBpil);
void BPInsListBackPatch(BPInsList *bpil, QRRecord *record, size_t addr);
void BPInsListFree(BPInsList *bpil);

#ifdef __cplusplus
}
#endif

#endif /*__BACKPATCH_H__*/

