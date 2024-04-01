#ifndef __SYM_ID_H__
#define __SYM_ID_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "list.h"

/*符号id*/
typedef struct {
    struct list_head node;
    int id;
} SymId;

int CfgSymIdSetIsContain(const struct list_head *list, int id);
int CfgSymIdListIsContainList(const struct list_head *lList, const struct list_head *rList);
int CfgSymIdListIsEqual(const struct list_head *lList, const struct list_head *rList);

int CfgSymIdListAddId(struct list_head *list, int id);
int CfgSymIdListTailAddId(struct list_head *list, int id);
int CfgSymIdListTailAddList(struct list_head *lList, const struct list_head *rList);
int CfgSymIdListCopyFromPos(struct list_head *lList, const struct list_head *rList, const struct list_head *pos);

int CfgSymIdSetAddId(struct list_head *list, int id);
int CfgSymIdSetAddList(struct list_head *lList, const struct list_head *rList);
int CfgSymIdSetAddListExcludeId(struct list_head *lList, const struct list_head *rList, int id);
int CfgSymIdSetAddListUpdateFlag(struct list_head *lList, const struct list_head *rList, int id,
                                 char *updageFlag);

int CfgSymIdListDelId(struct list_head *list, int id);
void CfgSymIdListFree(struct list_head *list);

void CfgSymIdListPrint(const struct list_head *list, const char *(*symIdStr)(int, const void *), const void *arg);

#ifdef __cplusplus
}
#endif

#endif /*__SYM_ID_H__*/
