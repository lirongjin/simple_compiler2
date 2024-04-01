/*
 * 文件：dfa.c
 * 描述：使用子集构造法根据nfa生成dfa状态转换表
 * 作者：Li Rongjin
 * 日期：2023-11-05
 **/

#include "dfa.h"
#include "nfa.h"
#include "list.h"
#include "stdlib.h"
#include "stdio.h"
#include "cpl_debug.h"
#include "cpl_errno.h"

#define PrErr(fmt, ...)  Pr(__FILE__, __LINE__, __FUNCTION__, "error", fmt, ##__VA_ARGS__)

//#define DFA_DEBUG
#ifdef DFA_DEBUG
#define PrDbg(fmt, ...)  Pr(__FILE__, __LINE__, __FUNCTION__, "debug", fmt, ##__VA_ARGS__)
#else
#define PrDbg(fmt, ...)
#endif

/*Dfa状态id节点*/
typedef struct {
    struct list_head node;
    int id;     /*Dfa状态id*/
} DfaStateId;

/*Dfa边*/
typedef struct {
    struct list_head node;/**/
    char ch;    /*边的编号。*/
    int id;     /*边所指向的状态。*/
} DfaSide;

/*Dfa状态。*/
typedef struct _DfaState {
    struct list_head node;
    int id;     /*Dfa状态id*/
    struct list_head moveStateIdList;   /*各个边的转换状态，节点类型DfaSide*/
    struct list_head nfaStateIdList;    /*包含的nfa状态id链表，节点类型NfaStateId*/
} DfaState;

/*确定的有穷自动机*/
typedef struct _Dfa {
    struct list_head stateList;     /*Dfa状态链表，节点类型DfaState*/
    struct list_head finishStateIdList; /*Dfa接受状态id链表，节点类型DfaStateId*/

    int stateNum;   /*用于生成dfa状态id。*/
    Nfa *nfa;       /*用于生成dfa的nfa。*/
} Dfa;

void DfaListPrintNfaStateIdList(const struct list_head *list);
void DfaListPrintNfaSideIdList(const struct list_head *list);

/*
 * 功能：判断DfaStateId链表中是否包含id节点
 * list：状态id链表
 * id：节点id
 * 返回值：DfaStateId链表中含id节点返回1，否则返回0。
 **/
static char DfaStateIdListContain(const struct list_head *list, int id)
{
    struct list_head *pos;
    DfaStateId *stateId;

    list_for_each(pos, list) {
        stateId = container_of(pos, DfaStateId, node);
        if (stateId->id == id)
            return 1;
    }
    return 0;
}

/*
 * 功能：初始化DfaState
 * state: DfaState指针
 * id: 状态id
 * 返回值：无
 **/
static void DfaStateInit(DfaState *state, int id)
{
    state->id = id;
    INIT_LIST_HEAD(&state->moveStateIdList);
    INIT_LIST_HEAD(&state->nfaStateIdList);
}

/*
 * 功能：申请一个Dfa状态
 * id：状态id
 * 返回值：成功时返回DfaState，否则返回NULL。
 **/
DfaState *DfaAllocState(int id)
{
    DfaState *state;

    PrDbg("alloc state %d", id);

    state = malloc(sizeof (*state));
    if (state) {
        DfaStateInit(state, id);
    }
    return state;
}

/*
 * 功能：释放DfaStateId链表资源。
 * list：DfaStateId链表。
 * 返回值：
 **/
static void DfaStateIdListFree(struct list_head *list)
{
    DfaStateId *stateId;
    struct list_head *pos;

    if (!list)
        return;

    for (pos = list->next; pos != list;) {
        stateId = container_of(pos, DfaStateId, node);
        pos = pos->next;
        list_del(&stateId->node);
        free(stateId);
    }
}

/*
 * 功能：把一个DfaStateId压入栈
 * list: DfaStateId链表
 * id：dfa状态id
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int DfaStateIdListPush(struct list_head *list, int id)
{
    DfaStateId *stateId;

    stateId = malloc(sizeof (*stateId));
    if (stateId) {
        stateId->id = id;
        list_add(&stateId->node, list);
        return -ENOERR;
    } else {
        return -ENOMEM;
    }
}

/*
 * 功能：弹出栈顶的DfaStateId
 * list：DfaStateId栈。
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int DfaStateIdListPop(struct list_head *list)
{
    if (!list_empty(list)) {
        struct list_head *pos = list_get_head(list);
        int id;
        DfaStateId *stateId;

        stateId = container_of(pos, DfaStateId, node);
        id = stateId->id;
        list_del(pos);
        free(stateId);
        return id;
    } else {
        return -ENOSTATE;
    }
}

/*
 * 功能：获取编号为id的Dfa状态。
 * dfa：dfa
 * id：状态id编号
 * 返回值：成功时返回DfaState指针，否则返回NULL。
 **/
DfaState *DfaGetState(const Dfa *dfa, int id)
{
    struct list_head *pos;
    DfaState *state;

    if (!dfa)
        return NULL;

    list_for_each(pos, &dfa->stateList) {
        state = container_of(pos, DfaState, node);
        if (state->id == id)
            return state;
    }
    return NULL;
}

/*
 * 功能：判断lState状态中的nfaStateIdList是否包含了rState状态中的nfaStateIdList中的所有状态。
 * lState：DfaState左操作数
 * rState：DfaState右操作数
 * 返回值：如果包含返回1，否则返回0。
 **/
static char DfaStateContain(const DfaState *lState, const DfaState *rState)
{
    struct list_head *pos;
    NfaStateId *nfaStateId;

    list_for_each(pos, &rState->nfaStateIdList) {
        nfaStateId = container_of(pos, NfaStateId, node);
        if (NfaStateIdListIsContain(&lState->nfaStateIdList, nfaStateId->id) == 0)
            return 0;
    }
    return 1;
}

/*
 * 功能：判断lState和rState的所有nfa状态id是否相同。
 * lState：DfaState左操作数
 * rState：DfaState右操作数
 * 返回值：如果相同返回1，否则返回0。
 **/
static char DfaStateIsEqual(const DfaState *lState, const DfaState *rState)
{
    if (DfaStateContain(lState, rState)
            && DfaStateContain(rState, lState))
        return 1;
    else
        return 0;
}

/*
 * 功能：获取dfa中包含状态DfaState中所有nfa状态id的状态id。
 * dfa：dfa
 * state：DfaState指针。
 * 返回值：如果存在这个状态返回这个状态的id，否则返回-ENOSTATE。
 **/
static int DfaGetStateId(Dfa *dfa, const DfaState *state)
{
    struct list_head *pos;
    DfaState *iterState;

    list_for_each(pos, &dfa->stateList) {
        iterState = container_of(pos, DfaState, node);
        if (DfaStateIsEqual(iterState, state)) {
            return iterState->id;
        }
    }
    return -ENOSTATE;
}

/*
 * 功能：给状态state添加在边ch上的转换状态id。
 * state：DfaState指针
 * ch：边编号
 * id：转换id。
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int DfaStateAddMoveState(DfaState *state, char ch, int id)
{
    DfaSide *side;

    if (!state)
        return -EINVAL;

    PrDbg("id %d -> %d, %c", state->id, id, ch);

    side = malloc(sizeof (*side));
    if (!side) {
        PrErr("error: %d, ENOMEM", -ENOMEM);
        return -ENOMEM;
    }
    side->ch = ch;
    side->id = id;
    list_add(&side->node, &state->moveStateIdList);
    return -ENOERR;
}

/*
 * 功能：释放DfaSide链表资源
 * list: DfaSide链表
 * 返回值：无
 **/
static void DfaStateFreeMoveState(struct list_head *list)
{
    struct list_head *pos;
    DfaSide *side;

    for (pos = list->next; pos != list;) {
        side = container_of(pos, DfaSide, node);
        pos = pos->next;
        list_del(&side->node);
        free(side);
    }
}

/*
 * 功能：释放DfaState资源
 * state: DfaState指针
 * 返回值：无。
 **/
static void DfaFreeState(DfaState *state)
{
    if (!state)
        return;

    PrDbg("free state: %d", state->id);
    NfaFreeStateIdList(&state->nfaStateIdList);
    DfaStateFreeMoveState(&state->moveStateIdList);
    free(state);
}

/*
 * 功能：申请dfa资源
 * regExStr：需要解析的正则表达式
 * pdfa：输出型参数，传出Dfa指针。
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int DfaAlloc(const char *regExStr, Dfa **pdfa)
{
    Dfa *dfa;
    int error = 0;

    if (!regExStr || !pdfa)
        return -EINVAL;

    *pdfa = NULL;
    dfa = malloc(sizeof (*dfa));
    if (dfa) {
        dfa->nfa = NfaAlloc(regExStr);
        if (dfa->nfa) {
            INIT_LIST_HEAD(&dfa->stateList);
            dfa->stateNum = 0;
            *pdfa = dfa;
            INIT_LIST_HEAD(&dfa->finishStateIdList);
            return -ENOERR;
        } else {
            free(dfa);
            dfa = NULL;
            PrErr("Nfa alloc fail");
            return -ENOMEM;
        }
    } else {
        PrErr("error: %d, ENOMEM", -ENOMEM);
        return -ENOMEM;
    }

    return error;
}

/*
 * 功能：释放dfa所有状态的NfaStateId链表。
 * dfa：dfa
 * 返回值：无。
 **/
static void DfaFreeStateNfaStateIdList(Dfa *dfa)
{
    struct list_head *pos;
    DfaState *state;

    list_for_each(pos, &dfa->stateList) {
        state = container_of(pos, DfaState, node);
        NfaFreeStateIdList(&state->nfaStateIdList);
    }
}

/*
 * 功能：释放dfa资源
 * dfa：dfa
 * 返回值：无
 **/
void DfaFree(Dfa *dfa)
{
    struct list_head *pos;
    struct list_head *list;

    if (!dfa)
        return;

    DfaState *state;

    /*释放状态链表资源*/
    list = &dfa->stateList;
    for (pos = list->next; pos != list;) {
        state = container_of(pos, DfaState, node);
        pos = pos->next;
        list_del(&state->node);
        DfaFreeState(state);
    }

    DfaStateId *stateId;

    /*释放接收状态id链表资源*/
    list = &dfa->finishStateIdList;
    for (pos = list->next; pos != list;) {
        stateId = container_of(pos, DfaStateId, node);
        pos = pos->next;
        list_del(&stateId->node);
        free(stateId);
    }

    /*释放nfa*/
    if (dfa->nfa) {
        NfaFree(dfa->nfa);
        dfa->nfa = NULL;
    }
    free(dfa);
    dfa = NULL;
}

void DfaStatePrint(const DfaState *state)
{
    struct list_head *pos;
    DfaSide *side;

    list_for_each(pos, &state->moveStateIdList) {
        side = container_of(pos, DfaSide, node);
        printf("---------side: %c, state: %d----------\n",
               side->ch, side->id);
    }
}

void DfaStatePrintNfaStateIdList(const DfaState *state)
{
    struct list_head *pos;
    NfaStateId *nfaStateId;

    list_for_each(pos, &state->nfaStateIdList) {
        nfaStateId = container_of(pos, NfaStateId, node);
        printf("%d ", nfaStateId->id);
    }
    if (!list_empty(&state->nfaStateIdList))
        printf("\n");
}

void DfaListPrintNfaStateIdList(const struct list_head *list)
{
    struct list_head *pos;
    NfaStateId *nfaStateId;

    list_for_each(pos, list) {
        nfaStateId = container_of(pos, NfaStateId, node);
        printf("%d ", nfaStateId->id);
    }
    if (!list_empty(list))
        printf("\n");
}

void DfaListPrintNfaSideIdList(const struct list_head *list)
{
    struct list_head *pos;
    NfaSideId *sideId;

    printf("-----------side----------------\n");

    list_for_each(pos, list) {
        sideId = container_of(pos, NfaSideId, node);
        printf("%c ", sideId->ch);
    }
    if (!list_empty(list))
        printf("\n");
}

void DfaStateIdListPrint(const struct list_head *list)
{
    struct list_head *pos;
    DfaStateId *stateId;

    list_for_each(pos, list) {
        stateId = container_of(pos, DfaStateId, node);
        printf("%d ", stateId->id);
    }
    if (!list_empty(list)) {
        printf("\n");
    }
}

void DfaPrint(const Dfa *dfa)
{
    struct list_head *pos;
    DfaState *state;

    list_for_each(pos, &dfa->stateList) {
        state = container_of(pos, DfaState, node);
        printf("-------------state: %d----------\n", state->id);
        DfaStatePrint(state);
        //DfaStatePrintNfaStateIdList(state);
    }

    printf("dfa finish state id list: \n");
    DfaStateIdListPrint(&dfa->finishStateIdList);
}

/*
 * 功能：从dfa状态0生成dfa的状态转换表。
 * dfa: dfa
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int _DfaGenerateTranslateTable(Dfa *dfa)
{
    struct list_head dfaStateStack;
    int dfaStateId = 0;
    Nfa *nfa = dfa->nfa;
    int error = 0;

    INIT_LIST_HEAD(&dfaStateStack);
    while (dfaStateId >= 0) {
        struct list_head sideList;
        DfaState *curDfaState;

        INIT_LIST_HEAD(&sideList);
        curDfaState = DfaGetState(dfa, dfaStateId);
        if (!curDfaState) {
            PrErr("error: %d, -ENOSTATE", -ENOSTATE);
            error = -ENOSTATE;
            goto err;
        }
        /*获取当前Dfa状态包含的Nfa状态的所有边*/
        error = NfaStateIdListGetSideIdSet(nfa, &curDfaState->nfaStateIdList, &sideList);
        if (error != -ENOERR)
            goto err;

        struct list_head *sidePos;
        NfaSide *side;

        /*遍历所有Nfa状态的边，获取Nfa状态集合中所有状态的下一个状态的集合*/
        list_for_each(sidePos, &sideList) {
            DfaState *newState;
            int moveStateId;

            newState = DfaAllocState(dfa->stateNum);
            if (!newState) {
                PrErr("error: %d, -ENOMEM", -ENOMEM);
                error = -ENOMEM;
                goto err;
            }
            side = container_of(sidePos, NfaSide, node);
            error = NfaStateIdListMove(nfa, &curDfaState->nfaStateIdList, side->ch,
                               &newState->nfaStateIdList);
            if (error != -ENOERR)
                goto err;
            moveStateId = DfaGetStateId(dfa, newState);
            if (moveStateId >= 0) {
                DfaFreeState(newState);
            } else {
                list_add(&newState->node, &dfa->stateList);
                moveStateId = dfa->stateNum;
                dfa->stateNum++;
                error = DfaStateIdListPush(&dfaStateStack, moveStateId);
                if (error != -ENOERR)
                    goto err;
            }
            error = DfaStateAddMoveState(curDfaState, side->ch, moveStateId);
            if (error != -ENOERR)
                goto err;
        }
        dfaStateId = DfaStateIdListPop(&dfaStateStack);
    }

    return error;
err:
    DfaStateIdListFree(&dfaStateStack);
    return error;
}

#ifdef DFA_DEBUG
static void DfaPrintDfaStateIdList(struct list_head *list)
{
    struct list_head *pos;
    DfaStateId *stateId;

    if (!list)
        return;

    list_for_each(pos, list) {
        stateId = container_of(pos, DfaStateId, node);
        printf("%d ", stateId->id);
    }
}
#endif

/*打印状态转换表*/
#ifdef DFA_DEBUG

typedef struct _DfaSideId {
    struct list_head node;
    char ch;
} DfaSideId;

static void DfaSideIdSetPrintHorizontalLine(const struct list_head *list)
{
    struct list_head *pos;

    printf("----");
    list_for_each(pos, list) {
        printf("------");
    }
    printf("\n");
}

static char DfaSideIdSetIsContain(const struct list_head *list, char ch)
{
    const struct list_head *pos;
    const DfaSideId *sideId;

    list_for_each(pos, list) {
        sideId = container_of(pos, DfaSideId, node);
        if (sideId->ch == ch)
            return 1;
    }
    return 0;
}

static int DfaSideIdSetAdd(struct list_head *list, char ch)
{
    DfaSideId *sideId;

    if (!DfaSideIdSetIsContain(list, ch)) {
        sideId = malloc(sizeof (*sideId));
        if (sideId) {
            sideId->ch = ch;
            list_add(&sideId->node, list);
            return 0;
        } else {
            PrErr("error: %d, ENOMEM", -ENOMEM);
            return -ENOMEM;
        }
    }
    return 0;
}

static int DfaStateGetSideIdSet(const DfaState *state, struct list_head *list)
{
    struct list_head *pos;
    const DfaSide *side;
    int error = 0;

    list_for_each(pos, &state->moveStateIdList) {
        side = container_of(pos, DfaSide, node);
        error = DfaSideIdSetAdd(list, side->ch);
        if (error)
            return error;
    }
    return error;
}

static int DfaGetSideIdSet(const Dfa *dfa, struct list_head *list)
{
    struct list_head *pos;
    const DfaState *state;
    int error = 0;

    list_for_each(pos, &dfa->stateList) {
        state = container_of(pos, DfaState, node);
        error = DfaStateGetSideIdSet(state, list);
        if (error)
            return error;
    }
    return error;
}

static int DfaSideIdSetPrint(const struct list_head *list)
{
    struct list_head *pos;
    const DfaSideId *sideId;

    DfaSideIdSetPrintHorizontalLine(list);
    printf("    | ");
    list_for_each(pos, list) {
        sideId = container_of(pos, DfaSideId, node);
        printf("%-4c| ", sideId->ch);
    }
    printf("\n");
    return 0;
}

static void DfaMoveListPrintCh(const struct list_head *list, char ch)
{
    struct list_head *pos;
    const DfaSide *side;
    char flag = 0;

    list_for_each(pos, list) {
        side = container_of(pos, DfaSide, node);
        if (side->ch == ch) {
            flag = 1;
            printf("%-4d| ", side->id);
        }
    }
    if (!flag) {
        printf("    | ");
    }
}

static int DfaStatePrintMoveStateId(const DfaState *state, struct list_head *list)
{
    struct list_head *pos;
    DfaSideId *sideId;

    DfaSideIdSetPrintHorizontalLine(list);
    printf("%-4d| ", state->id);
    list_for_each(pos, list) {
        sideId = container_of(pos, DfaSideId, node);
        DfaMoveListPrintCh(&state->moveStateIdList, sideId->ch);
    }
    printf("\n");
    return 0;
}

static int DfaPrintMoveStateId(const Dfa *dfa, struct list_head *list)
{
    struct list_head *pos;
    DfaState *state;
    int error = 0;

    list_for_each(pos, &dfa->stateList) {
        state = container_of(pos, DfaState, node);
        error = DfaStatePrintMoveStateId(state, list);
        if (error)
            return error;
    }
    printf("\n");
    return error;
}

static void DfaPrintTranslateTable(const Dfa *dfa)
{
    struct list_head sideIdSet;

    printf("translate table:\n");
    INIT_LIST_HEAD(&sideIdSet);
    DfaGetSideIdSet(dfa, &sideIdSet);
    DfaSideIdSetPrint(&sideIdSet);
    DfaPrintMoveStateId(dfa, &sideIdSet);
    DfaSideIdSetPrintHorizontalLine(&sideIdSet);
}

#endif

/*
 * 功能：生成dfa的接受状态id链表。
 * dfa：dfa
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int DfaGenerateFinishStateIdList(Dfa *dfa)
{
    struct list_head *pos;
    DfaState *state;
    int nfaFinishStateId;
    int error = 0;

    if (!dfa || !dfa->nfa) {
        PrErr("error: %d, EINVAL", -EINVAL);
        return -EINVAL;
    }
    nfaFinishStateId = NfaGetFinishStateId(dfa->nfa);
    if (nfaFinishStateId < 0) {
        return nfaFinishStateId;
    }
    list_for_each(pos, &dfa->stateList) {
        state = container_of(pos, DfaState, node);
        error = NfaStateIdListIsContain(&state->nfaStateIdList, nfaFinishStateId);
        if (error == 1) {
            error = DfaStateIdListPush(&dfa->finishStateIdList, state->id);
            if (error != -ENOERR)
                return error;
        } else if (error < 0) {
            return error;
        }
    }
    if (list_empty(&dfa->finishStateIdList)) {
        PrErr("error: %d, ENOFINISHID", -ENOFINISHID);
        return -ENOFINISHID;
    }

#ifdef DFA_DEBUG
    printf("finish state id list: ");
    DfaPrintDfaStateIdList(&dfa->finishStateIdList);
    printf("\n");
#endif

    return -ENOERR;
}

/*
 * 功能：生成dfa的状态转换表。
 * dfa：dfa
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int DfaGenerateTranslateTable(Dfa *dfa)
{
    Nfa *nfa = dfa->nfa;
    DfaState *state;
    int error = 0;

    if (!dfa || !nfa)
        return -EINVAL;

    /*创建dfa状态0。*/
    state = DfaAllocState(dfa->stateNum++);
    if (!state) {
        PrErr("error: %d, ENOMEM", -ENOMEM);
        return -ENOMEM;
    }
    list_add(&state->node, &dfa->stateList);
    error = NfaStateIdListPush(&state->nfaStateIdList, NfaGetStartStateId(nfa));
    if (error != -ENOERR)
        return error;
    /*在状态0上执行空闭包函数。*/
    error = NfaStateIdListEmptyClosure(nfa, &state->nfaStateIdList);
    if (error != -ENOERR)
        return error;
    error = _DfaGenerateTranslateTable(dfa);
    if (error != -ENOERR)
        return error;
    error = DfaGenerateFinishStateIdList(dfa);
    if (error != -ENOERR)
        return error;

#ifdef DFA_DEBUG
    DfaPrintTranslateTable(dfa);
#endif

    return error;
}

/*
 * 功能：根据正则表达式字符串生成dfa。
 * regExStr：正则表达式字符串
 * pDfa：输出型参数，传出Dfa指针
 * 返回值：成功时返回0，否则返回错误码。
 **/
int DfaGenerate(const char *regExStr, Dfa **pDfa)
{
    int error = 0;
    Dfa *dfa;

    if (!regExStr || !pDfa)
        return -EINVAL;

    *pDfa = NULL;
    error = DfaAlloc(regExStr, &dfa);
    if (error != -ENOERR)
        return error;
    error = NfaParse(dfa->nfa);
    if (error != -ENOERR)
        goto err;
    error = DfaGenerateTranslateTable(dfa);
    if (error != -ENOERR)
        goto err;
    *pDfa = dfa;

    /*释放不再使用的资源。*/
    NfaFree(dfa->nfa);
    dfa->nfa = NULL;
    DfaFreeStateNfaStateIdList(dfa);

    return error;

err:
    DfaFree(dfa);
    dfa = NULL;
    return error;
}

/*
 * 功能：判断dfa状态是否是接受状态。
 * dfa：dfa
 * state：DfaState指针
 * 返回值：成功时返回0，否则返回错误码。
 **/
int DfaIsFinishState(Dfa *dfa, DfaState *state)
{
    if (!dfa || !state)
        return -EINVAL;

    if (DfaStateIdListContain(&dfa->finishStateIdList, state->id))
        return 1;
    else
        return 0;
}

/*
 * 功能：获取状态state在边ch上的转换状态。
 * dfa：dfa
 * state：DfaState指针。
 * ch：边编号。
 * 返回值：成功时返回在边ch上的转换状态指针，否则返回NULL。
 **/
DfaState *DfaMoveState(Dfa *dfa, DfaState *state, char ch)
{
    struct list_head *pos;
    DfaSide *side;

    if (!dfa || !state)
        return NULL;

    list_for_each(pos, &state->moveStateIdList) {
        side = container_of(pos, DfaSide, node);
        if (side->ch == ch) {
            return DfaGetState(dfa, side->id);
        }
    }
    return NULL;
}
