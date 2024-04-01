#ifndef __NFA_H__
#define __NFA_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "list.h"

#define EMPYT_SIDE_CHAR     0   /*空字符对应的边的编号。*/
/*其他字符都有ASCII码作为边的编号。*/

typedef struct _Nfa Nfa ;

/*nfa边编号节点。*/
typedef struct {
    struct list_head node;
    char ch;
} NfaSideId;

/*nfa边，记录了转换状态链表*/
typedef struct {
    struct list_head node;
    char ch;    /*边的符号*/
    struct list_head moveStateIdList;  /*转换状态链表，节点是NfaStateId*/
} NfaSide;

/*不确定的有穷自动机的状态id节点*/
typedef struct {
    struct list_head node;
    int id;    /*状态id*/
} NfaStateId;

Nfa *NfaAlloc(const char *regExStr);
void NfaFree(Nfa *nfa);
int NfaParse(Nfa *nfa);
void NfaPrint(Nfa *nfa);
int NfaGetStartStateId(const Nfa *nfa);
int NfaGetFinishStateId(const Nfa *nfa);

int NfaStateIdListGetSideIdSet(const Nfa *nfa, const struct list_head *stateIdList,
                                struct list_head *sideList);

int NfaStateIdListEmptyClosure(Nfa *nfa, struct list_head *list);
int NfaStateIdListIsContain(const struct list_head *list, int id);
int NfaStateIdListPush(struct list_head *list, int id);
int NfaStateIdListPop(struct list_head *list);
void NfaFreeStateIdList(struct list_head *list);
int NfaStateIdListMove(Nfa *nfa, struct list_head *stateIdList, char ch,
                              struct list_head *moveStateIdList);

#ifdef __cplusplus
}
#endif

#endif /*__NFA_H__*/
