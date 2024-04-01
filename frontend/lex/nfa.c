/*
 * 文件：nfa.c
 * 描述：此文件所实现的功能是解析正则表达式字符串，并生成不确定的有穷自动机NFA。
 * 操作符: ab: 表示字符a, b连接
 *        a|b: 表示字符a或b
 *        a*: 表示字符a的Kleene闭包
 * 操作符优先级由高到低依次为：
 *              ()
 *              *
 *              连接
 *              |
 * 作者：Li Rongjin
 * 日期：2023-11-03
 **/

#include "nfa.h"
#include "list.h"
#include "stdlib.h"
#include "ctype.h"
#include "stdio.h"
#include "cpl_errno.h"
#include "cpl_debug.h"

#define PrErr(fmt, ...)  Pr(__FILE__, __LINE__, __FUNCTION__, "error", fmt, ##__VA_ARGS__)

//#define NFA_DEBUG
#ifdef NFA_DEBUG
#define PrDbg(fmt, ...)  Pr(__FILE__, __LINE__, __FUNCTION__, "debug", fmt, ##__VA_ARGS__)
#else
#define PrDbg(fmt, ...)
#endif

/*不确定的有穷自动机状态*/
typedef struct {
    struct list_head node;
    int id;    /*状态id*/
    struct list_head sideList; /*边链表，节点是NfaSide*/
} NfaState;

/*不确定的有穷自动机语法分析树节点*/
typedef struct {
    struct list_head node;
    NfaState *start;     /*起始状态*/
    NfaState *finish;    /*结束状态*/
} NfaTreeNode;

/*不确定的有穷自动机*/
typedef struct _Nfa {
    struct list_head stateList; /*自动机的状态链表，节点是NfaState*/
    struct list_head treeList;  /*自动机的正则表达式语法分析树节点链表，节点是NfaTreeNode*/
    NfaTreeNode *rootNode;      /*自动机的正则表达式语法分析树根节点。*/
    int stateNum;               /*自动机状态id生成计数器。*/

    const char *regExStr;       /*待分析的正则表达式字符串*/
    char lookahead;             /*用于正则表达式字符串词法分析器的向前看符号*/
} Nfa;

/*
 * 功能：为nfa边添加转换状态id。
 * side: nfa边
 * id：转换状态
 * 返回值：成功时返回0，否则返回错误码。
 */
static int NfaSideAddMoveStateId(NfaSide *side, int id)
{
    NfaStateId *moveState;

    if (!side)
        return -EINVAL;

    moveState = malloc(sizeof (*moveState));
    if (!moveState) {
        PrErr("error: %d, ENOMEM", -ENOMEM);
        return -ENOMEM;
    }
    moveState->id = id;
    list_add(&moveState->node, &side->moveStateIdList);

    return -ENOERR;
}

/*
 * 功能：释放NfaStateId链表资源
 * list：NfaStateId链表
 * 返回值：无
 **/
void NfaFreeStateIdList(struct list_head *list)
{
    struct list_head *pos;
    NfaStateId *stateId;

    if (!list)
        return;

    for (pos = list->next; pos != list;) {
        stateId = container_of(pos, NfaStateId, node);
        pos = pos->next;
        list_del(&stateId->node);
        free(stateId);
    }
}

/*
 * 功能：释放nfa边资源。
 * side：nfa边
 * 返回值：无
 */
static void NfaFreeSide(NfaSide *side)
{
    if (!side)
        return;

    NfaFreeStateIdList(&side->moveStateIdList);
    free(side);
    side = NULL;
}

/*
 * 功能：初始化nfa状态state，状态id为id。
 * state: nfa状态
 * id：id
 * 返回值：无。
 */
static void NfaStateInit(NfaState *state, int id)
{
    state->id = id;
    INIT_LIST_HEAD(&state->sideList);
}

/*
 * 功能：申请nfa状态，状态id为id
 * nfa: nfa
 * id: id
 * 返回值：nfa状态。
 */
static NfaState *NfaAllocState(Nfa *nfa, int id)
{
    NfaState *state;

    PrDbg("alloc state: %d", id);
    state = malloc(sizeof (*state));
    if (state) {
        NfaStateInit(state, id);
        list_add(&state->node, &nfa->stateList);
    }
    return state;
}

/*
 * 功能：nfa状态释放边链表资源
 * nfa：状态
 * 返回值：无
 */
static void NfaStateFreeSideList(NfaState *state)
{
    NfaSide *side;
    struct list_head *pos;
    struct list_head *list = &state->sideList;

    for (pos = list->next; pos != list;) {
        side = container_of(pos, NfaSide, node);
        pos = pos->next;
        list_del(&side->node);
        NfaFreeSide(side);
    }
}

/*
 * 功能：释放nfa状态资源
 * state：nfa状态
 * 返回值：无。
 */
static void NfaFreeState(NfaState *state)
{
    if (!state)
        return;

    PrDbg("free state: %d", state->id);
    NfaStateFreeSideList(state);
    free(state);
}

/*
 * 功能：获取状态的ch边
 * state：nfa状态
 * ch：边编号。
 * 返回值：成功时返回状态的指向边为ch的转换状态链表的指针，否则返回NULL
 **/
static NfaSide *NfaStateGetSide(const NfaState *state, char ch)
{
    struct list_head *pos;
    NfaSide *side;

    if (!state)
        return NULL;

    list_for_each(pos, &state->sideList) {
        side = container_of(pos, NfaSide, node);
        if (side->ch == ch)
            return side;
    }
    return NULL;
}

/*
 * 功能：为状态state添加边为ch的转换状态moveStateId
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int NfaStateAddMoveState(NfaState *state, int moveStateId, char ch)
{
    NfaSide *side;

    if (!state)
        return -EINVAL;

    side = NfaStateGetSide(state, ch);
    if (!side) {
        side = malloc(sizeof (*side));
        if (!side) {
            PrErr("error: %d, ENOMEM", -ENOMEM);
            return -ENOMEM;
        }
        INIT_LIST_HEAD(&side->moveStateIdList);
        side->ch = ch;
        list_add(&side->node, &state->sideList);
    }
    return NfaSideAddMoveStateId(side, moveStateId);
}

/*
 * 功能：申请一个语法分析树节点，并把树节点内的状态添加到nfa自动机
 * pNode: 输出型参数，传出树节点。
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int NfaAllocTreeNode(Nfa *nfa, NfaTreeNode **pNode)
{
    NfaTreeNode *treeNode;

    if (!nfa || !pNode)
        return -EINVAL;

    treeNode = malloc(sizeof (*treeNode));
    if (!treeNode)
        goto err;
    list_add(&treeNode->node, &nfa->treeList);

    treeNode->start = NfaAllocState(nfa, nfa->stateNum++);
    if (!treeNode->start)
        goto err;
    treeNode->finish = NfaAllocState(nfa, nfa->stateNum++);
    if (!treeNode->finish)
        goto err;

    *pNode = treeNode;
    return -ENOERR;

err:
    PrErr("error: %d, ENOMEM", -ENOMEM);
    return -ENOMEM;
}

/*
 * 功能：申请一个NFA
 * regExStr：正则表达式字符串。
 * 返回值：成功时返回Nfa指针，否则返回NULL。
 */
Nfa *NfaAlloc(const char *regExStr)
{
    Nfa *nfa;

    nfa = malloc(sizeof (*nfa));
    if (nfa) {
        INIT_LIST_HEAD(&nfa->stateList);
        nfa->stateNum = 0;
        INIT_LIST_HEAD(&nfa->treeList);
        nfa->rootNode = NULL;
        nfa->regExStr = regExStr;
    }
    return nfa;
}

/*
 * 功能：释放NFA
 * 返回值：无
 */
void NfaFree(Nfa *nfa)
{
    struct list_head *pos;
    struct list_head *list;
    NfaState *state;
    NfaTreeNode *treeNode;

    if (!nfa)
        return;

    /*释放nfa状态链表。*/
    list = &nfa->stateList;
    for (pos = list->next; pos != list;) {
        state = container_of(pos, NfaState, node);
        pos = pos->next;
        list_del(&state->node);
        NfaFreeState(state);
    }

    /*释放语法分析树节点。*/
    list = &nfa->treeList;
    for (pos = list->next; pos != list;) {
        treeNode = container_of(pos, NfaTreeNode, node);
        pos = pos->next;
        list_del(&treeNode->node);
        free(treeNode);
    }
    free(nfa);
    nfa = NULL;
}

static int RegExOr(Nfa *nfa, NfaTreeNode **node);
static char RegExScan(Nfa *nfa);

/*
 * 功能：解析正则表达式字符串，构造不确定的有穷状态机。
 * 返回值：成功时返回0，否则返回错误码。
 */
int NfaParse(Nfa *nfa)
{
    int error;

    if (!nfa)
        return -EINVAL;
    nfa->lookahead = RegExScan(nfa);
    error = RegExOr(nfa, &nfa->rootNode);

#ifdef NFA_DEBUG
    if (!error) {
        NfaPrint(nfa);
        printf("%d, %d\n", nfa->rootNode->start->id, nfa->rootNode->finish->id);
    }
#endif

    return error;
}

/*
 * 功能：获取不确定的有穷自动机的起始状态id。
 * 返回值：成功时返回不确定的有穷自动机的起始状态id，否则返回错误码。
 */
int NfaGetStartStateId(const Nfa *nfa)
{
    return (nfa && nfa->rootNode && nfa->rootNode->start) ? nfa->rootNode->start->id : -EINVAL;
}

/*
 * 功能：获取不确定的有穷自动机的接受状态id。
 * 返回值：成功时返回不确定的有穷自动机的接受状态id，否则返回错误码。
 */
int NfaGetFinishStateId(const Nfa *nfa)
{
    return (nfa && nfa->rootNode && nfa->rootNode->finish) ? nfa->rootNode->finish->id : -EINVAL;
}

/*
 * 功能：获取nfa中编号为id的状态。
 * nfa：nfa
 * id：状态id
 * 返回值：成功时返回Nfa状态指针，否则返回NULL。
 **/
static NfaState *NfaGetState(const Nfa *nfa, int id)
{
    const struct list_head *pos;
    const struct list_head *list = &nfa->stateList;
    NfaState *state;

    list_for_each(pos, list) {
        state = container_of(pos, NfaState, node);
        if (state->id == id)
            return state;
    }
    return NULL;
}

/*
 * 功能：nfa边id集合中是否包含边ch。
 * list：边id集合。
 * ch：边的编号。
 * 返回值：如果包含返回1，否则返回0。
 **/
static char NfaSideIdSetIsContain(const struct list_head *list, char ch)
{
    const struct list_head *pos;
    const NfaSideId *sideId;

    list_for_each(pos, list) {
        sideId = container_of(pos, NfaSideId, node);
        if (sideId->ch == ch)
            return 1;
    }
    return 0;
}

/*
 * 功能：向Nfa边id集合中添加边ch。
 * list：边id集合。
 * ch：边的编号。
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int NfaSideIdSetAdd(struct list_head *list, char ch)
{
    NfaSideId *sideId;

    if (!NfaSideIdSetIsContain(list, ch)
            && ch != EMPYT_SIDE_CHAR) {
        sideId = malloc(sizeof (*sideId));
        if (!sideId) {
            PrErr("error: %d, ENOMEM", -ENOMEM);
            return -ENOMEM;
        }
        sideId->ch = ch;
        list_add(&sideId->node, list);
    }

    return -ENOERR;
}

/*
 * 功能：释放边id集合资源
 * list: 边id集合
 * 返回值：
 **/
void NfaSideIdSetFree(struct list_head *list)
{
    struct list_head *pos;
    NfaSideId *sideId;

    if (!list)
        return;

    for (pos = list->next; pos != list;) {
        sideId = container_of(pos, NfaSideId, node);
        pos = pos->next;
        list_del(&sideId->node);
        free(sideId);
    }
}

/*
 * 功能：获取nfa id状态的的边id集合
 * nfa：nfa
 * stateId: 状态id
 * list: 边id集合。
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int NfaStateIdGetSideIdSet(const Nfa *nfa, int stateId,  struct list_head *list)
{
    const NfaState *state;
    const struct list_head *pos;
    const struct list_head *sideList;
    const NfaSide *side;
    int error = 0;

    if (!nfa || !list)
        return -EINVAL;

    state = NfaGetState(nfa, stateId);
    if (!state)
        return -ENOSTATE;
    sideList = &state->sideList;

    list_for_each(pos, sideList) {
        side = container_of(pos, NfaSide, node);
        return NfaSideIdSetAdd(list, side->ch);
    }
    return error;
}

/*
 * 功能：获取各个id状态的边id集合
 * nfa: nfa
 * stateIdList: 状态id链表
 * sideList: 边id集合
 * 返回值：成功时返回0，否则返回错误码
 **/
int NfaStateIdListGetSideIdSet(const Nfa *nfa, const struct list_head *stateIdList,
                                struct list_head *sideList)
{
    const struct list_head *pos;
    const NfaStateId *stateId;
    int error = 0;

    list_for_each(pos, stateIdList) {
        stateId = container_of(pos, NfaStateId, node);
        error = NfaStateIdGetSideIdSet(nfa, stateId->id, sideList);
        if (error != -ENOERR)
            return error;
    }
    return error;
}

/*
 * 功能：获取id状态在边ch上的状态转换链表。
 * nfa：nfa
 * stateId：状态id
 * ch：边的编号
 * pList: 状态转换链表。
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int NfaStateGetMoveStateIdList(const Nfa *nfa, int stateId, char ch, const struct list_head **pList)
{
    const NfaState *state;
    const NfaSide *side;
    int error = 0;

    if (!nfa || !pList)
        return -EINVAL;

    state = NfaGetState(nfa, stateId);
    if (!state) {
        PrErr("error: %d, ENOSTATE", -ENOSTATE);
        return -ENOSTATE;
    }
    side = NfaStateGetSide(state, ch);
    if (side) {
        *pList = &side->moveStateIdList;
    } else {
        *pList = NULL;
    }
    return error;
}

/*
 * 功能：把NfaStateId压入栈
 * list: NfaStateId栈
 * id：待压入栈的状态id。
 * 返回值：成功时返回0，否则返回错误码。
 **/
int NfaStateIdListPush(struct list_head *list, int id)
{
    NfaStateId *stateId;

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
 * 功能：弹出NfaStateId栈顶的元素
 * list：NfaStateId栈
 * 返回值：栈非空时返回状态id，否则返回-ENOSTATE。
 **/
int NfaStateIdListPop(struct list_head *list)
{
    if (!list_empty(list)) {
        struct list_head *pos = list_get_head(list);
        int id;
        NfaStateId *stateId;

        stateId = container_of(pos, NfaStateId, node);
        id = stateId->id;
        list_del(pos);
        free(stateId);
        return id;
    } else {
        return -ENOSTATE;
    }
}

/*
 * 功能：把rList指向的NfaStateId链表拷贝到lList链表
 * lList：NfaStateId链表
 * rList：NfaStateId链表
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int NfaStateIdListCopy(struct list_head *lList, struct list_head *rList)
{
    int error = 0;
    struct list_head *pos;
    NfaStateId *stateId;

    list_for_each(pos, rList) {
        stateId = container_of(pos, NfaStateId, node);
        error = NfaStateIdListPush(lList, stateId->id);
        if (error != -ENOERR)
            return error;
    }
    return error;
}

/*
 * 功能：判断NfaStateId链表中是否包含为id的NfaStateId
 * 返回值：如果存在返回1，不存在返回0，出错时返回错误码。
 **/
int NfaStateIdListIsContain(const struct list_head *list, int id)
{
    struct list_head *pos;
    NfaStateId *stateId;

    if (!list)
        return -EINVAL;

    list_for_each(pos, list) {
        stateId = container_of(pos, NfaStateId, node);
        if (stateId->id == id)
            return 1;
    }
    return 0;
}

#ifdef NFA_DEBUG
static void NfaPrintStateIdList(struct list_head *list)
{
    struct list_head *pos;
    NfaStateId *stateId;

    if (!list)
        return;

    list_for_each(pos, list) {
        stateId = container_of(pos, NfaStateId, node);
        printf("%d ", stateId->id);
    }
}
#endif

/*
 * 功能：计算nfa状态空字符串的闭包
 * nfa：不确定的有穷自动机
 * list：NfaStateId链表。
 *       即是输入，也是输出。计算list上的NfaStateId的空闭包状态，并保存到list。
 * 返回值：成功时返回0，否则返回错误码。
 **/
int NfaStateIdListEmptyClosure(Nfa *nfa, struct list_head *list)
{
    struct list_head stack;
    int nfaStateId;
    int error = 0;

    if (!nfa || !list)
        return -EINVAL;

#ifdef NFA_DEBUG
    printf("empty closure: ");
    NfaPrintStateIdList(list);
    printf("\n");
#endif

    INIT_LIST_HEAD(&stack);
    error = NfaStateIdListCopy(&stack, list);
    if (error != -ENOERR)
        goto err;

    nfaStateId = NfaStateIdListPop(&stack);
    while (nfaStateId >= 0) {
        const struct list_head *moveStateIdList;
        NfaStateId *stateId;
        struct list_head *pos;
        int error;

        error = NfaStateGetMoveStateIdList(nfa, nfaStateId, EMPYT_SIDE_CHAR, &moveStateIdList);
        if (error == -ENOERR) {
            if (moveStateIdList) {
                list_for_each(pos, moveStateIdList) {
                    stateId = container_of(pos, NfaStateId, node);
                    if (0 == NfaStateIdListIsContain(list, stateId->id)) {
                        error = NfaStateIdListPush(list, stateId->id);
                        if (error != -ENOERR)
                            goto err;
                        error = NfaStateIdListPush(&stack, stateId->id);
                        if (error != -ENOERR)
                            goto err;
                    }
                }
            }
        } else
            goto err;

        nfaStateId = NfaStateIdListPop(&stack);
    }

#ifdef NFA_DEBUG
    printf("empty closure result: ");
    NfaPrintStateIdList(list);
    printf("\n");
#endif

    return error;
err:
    NfaFreeStateIdList(&stack);
    return error;
}

/*
 * 功能：把id添加到NfaStateId集合list
 * list: NfaStateId链表
 * id：待加入的状态id。
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int NfaStateIdSetAdd(struct list_head *list, int id)
{
    int error = 0;

    if (0 == NfaStateIdListIsContain(list, id)) {
        error = NfaStateIdListPush(list, id);
    }
    return error;
}

/*
 * 功能：把NfaStateId链表rList上的元素添加到链表lList。
 * lList：NfaStateId链表
 * rList：NfaStateId链表
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int NfaStateIdSetAddList(struct list_head *lList, const struct list_head *rList)
{
    const struct list_head *pos;
    const NfaStateId *stateId;
    int error = 0;

    list_for_each(pos, rList) {
        stateId = container_of(pos, NfaStateId, node);
        error = NfaStateIdSetAdd(lList, stateId->id);
        if (error != -ENOERR)
            return error;
    }
    return error;
}

/*
 * 功能：获取nfa状态id集合中所有状态的在边ch上的下一个状态id集合。
 * nfa：
 * stateIdList：nfa状态id集合
 * ch: 边的编号
 * moveStateIdList：下一个状态id集合
 * 返回值：成功时返回0，否则返回错误码。
 */
int NfaStateIdListMove(Nfa *nfa, struct list_head *stateIdList, char ch,
                              struct list_head *moveStateIdList)
{
    struct list_head *pos;
    NfaStateId *stateId;
    int error = 0;

    if (!nfa || !stateIdList
            || !moveStateIdList)
        return -EINVAL;

#ifdef NFA_DEBUG
    printf("state id list move: ");
    NfaPrintStateIdList(stateIdList);
    printf("\n");
#endif

    list_for_each(pos, stateIdList) {
        const struct list_head *sideMoveStateIdList;

        stateId = container_of(pos, NfaStateId, node);
        error = NfaStateGetMoveStateIdList(nfa, stateId->id, ch, &sideMoveStateIdList);
        if (error == -ENOERR) {
            if (sideMoveStateIdList) {
                error = NfaStateIdSetAddList(moveStateIdList, sideMoveStateIdList);
                if (error != -ENOERR)
                    return error;
            }
        } else
            return error;
    }

    error = NfaStateIdListEmptyClosure(nfa, moveStateIdList);

#ifdef NFA_DEBUG
    printf("move result: ");
    NfaPrintStateIdList(moveStateIdList);
    printf("\n");
#endif

    return error;
}

static void NfaSidePrint(NfaSide *side)
{
    struct list_head *pos;
    NfaStateId *moveState;

    if (list_empty(&side->moveStateIdList))
        return;

    if (side->ch == EMPYT_SIDE_CHAR)
        printf("empty: ");
    else
        printf("%c: ", side->ch);

    list_for_each(pos, &side->moveStateIdList) {
        moveState = container_of(pos, NfaStateId, node);
        printf("%d ", moveState->id);
    }
    printf("\n");
}

static void NfaStatePrint(NfaState *state)
{
    struct list_head *pos;
    NfaSide *side;

    list_for_each(pos, &state->sideList) {
        side = container_of(pos, NfaSide, node);
        NfaSidePrint(side);
    }
}

/*
 * 功能：打印NFA状态机的状态信息。
 * 返回值：无
 */
void NfaPrint(Nfa *nfa)
{
    struct list_head *pos;
    NfaState *state;

    list_for_each(pos, &nfa->stateList) {
        state = container_of(pos, NfaState, node);
        printf("------------nfa state %d -----------------\n", state->id);
        NfaStatePrint(state);
    }
}

/************************正则表达式上下文无关文法分析*********************************/

/*
 * 功能：读取正在表达式字符串中的下一个字符。
 * 返回值：正在表达式字符串中的下一个字符。
 **/
static char RegExScan(Nfa *nfa)
{
    char ch;

    if (nfa && nfa->regExStr) {
        ch = *nfa->regExStr++;
        PrDbg("read char: %c\n", ch);
        return ch;
    } else
        return '\0';
}

/*
 * 功能：判断字符ch是否与词法分析器的向前看字符匹配，匹配时读取下一个字符
 * 返回值：匹配返回0，否则返回错误码。
 **/
static int RegExMatch(Nfa *nfa, char ch)
{
    if (ch == nfa->lookahead) {
        nfa->lookahead = RegExScan(nfa);
        return 0;
    } else {
        /*符号ch和向前看符号不匹配。*/
        PrDbg("lookahead: %02x(h), %c", nfa->lookahead, nfa->lookahead);
        PrErr("%d, ELAERR\n", -ELAERR);
        return -ELAERR;
    }
}

/*
 * 功能：生成一个新节点，边为词法分析器的向前看符号
 * 返回值：成功时返回0，否则返回错误码。
 */
static int RegExId(Nfa *nfa, NfaTreeNode **node)
{
    int error = 0;

    if (!nfa || !node)
        return -EINVAL;

    error = NfaAllocTreeNode(nfa, node);
    if (error == -ENOERR) {
        PrDbg("handle id: %d(H), %c", nfa->lookahead, nfa->lookahead);
        error = NfaStateAddMoveState((*node)->start, (*node)->finish->id, nfa->lookahead);
        nfa->lookahead = RegExScan(nfa);
    }
    return error;
}

/*
 * 功能：判断字符ch是否为正则表达式中的特殊字符。
 * 返回值：如果字符ch是正则表达式中的特殊字符返回1，否则返回0。
 */
static inline char IsRegExSpecialChar(char ch)
{
    switch (ch) {
    case '|':
    case '*':
    case '(':
    case ')':
    case '[':
    case ']':
    case '.':
    case '-':
    case '?':
    case '+':
    case '\\':
        return 1;
    default:
        return 0;
    }
}

/*
 * 功能：判断字符ch是否为转义符号。
 * 返回值：如果字符ch是转义符号返回1，否则返回0。
 */
static inline char IsRegExEscapeChar(Nfa *nfa, char ch)
{
    (void)nfa;

    if (ch == '\\') {
        PrDbg("read escape char");
        return 1;
    } else
        return 0;
}

static inline char RegExEscapeChar(int ch)
{
    switch (ch) {
    case 't':
        return '\t';
    case 'r':
        return '\r';
    case 'n':
        return '\n';
    case 'b':
        return '\b';
    }
    return ch;
}

/*
 * 功能：判断词法分析器的向前看符号是否为非终结符ID。
 * 返回值：如果词法分析器的向前看符号是非终结符ID返回1，否则返回0。
 */
static char IsRegExId(Nfa *nfa)
{
    char ch = nfa->lookahead;

    if ((isprint(ch) && !IsRegExSpecialChar(ch))
            || IsRegExEscapeChar(nfa, ch)) {
        return 1;
    } else {
        PrDbg("%c not is id", nfa->lookahead);
        return 0;
    }
}

/*
 * 功能：判断词法分析器的向前看符号是否为'('
 * 返回值：如果词法分析器的向前看符号是'('返回1，否则返回0。
 */
static char IsRegExFactor0(Nfa *nfa)
{
    if (nfa->lookahead == '(') {
        return 1;
    } else {
        return 0;
    }
}

/*
 * 功能：解析文法的Factor非终结符。
 * node：生成的factor新节点
 * 返回值：成功时返回0，否则返回错误码。
 */
static int RegExFactor(Nfa *nfa, NfaTreeNode **node)
{
    int error = 0;

    if (IsRegExFactor0(nfa)) {
        error = RegExMatch(nfa, '(');
        if (error != -ENOERR)
            return error;
        PrDbg("handle (");
        error = RegExOr(nfa, node);
        if (error != -ENOERR)
            return error;
        return RegExMatch(nfa, ')');
    } else if (IsRegExId(nfa)) {
        if (nfa->lookahead == '\\') {
            nfa->lookahead = RegExScan(nfa);
            nfa->lookahead = RegExEscapeChar(nfa->lookahead);
        }
#if 0
        if (isprint(nfa->lookahead)
                ||) {
            PrDbg("id: %c", nfa->lookahead);
            return RegExId(nfa, node);
        } else {
            PrDbg("lookahead: %02x(h), %c", nfa->lookahead, nfa->lookahead);
            PrErr("%d, ELAERR\n", error);
            return -ELAERR;
        }
#else
        return RegExId(nfa, node);
#endif
    } else {
        /*语法错误*/
        error = -ESYNTAX;
        PrDbg("lookahead: %02x(h), %c", nfa->lookahead, nfa->lookahead);
        PrErr("%d, ESYNTAX\n", error);
    }

    return error;
}

/*
 * 功能：判断下词法分析器的向前看符号是否为'*'。
 * 返回值：如果下词法分析器的向前看符号是'*'返回1，否则返回0。
 */
static char IsRegExStarx0(Nfa *nfa)
{
    if (nfa->lookahead == '*')
        return 1;
    else
        return 0;
}

/*
 * 功能：文法符号star对应的翻译动作
 * subNode：文法符号star的子节点
 * starNode：生成的文法符号'*'对应的节点。
 * 返回值：成功时返回0，否则返回错误码。
 */
static int RegExStarAction(Nfa *nfa, NfaTreeNode *subNode, NfaTreeNode **starNode)
{
    int error = 0;

    if (!subNode || !starNode)
        return -EINVAL;

    /*申请新的树节点*/
    error = NfaAllocTreeNode(nfa, starNode);
    if (error == -ENOERR) {
        /*创建新节点'*'和子节点之间的关联。*/
        error = NfaStateAddMoveState((*starNode)->start, subNode->start->id, EMPYT_SIDE_CHAR);
        if (error != -ENOERR)
            return error;
        error = NfaStateAddMoveState((*starNode)->start, (*starNode)->finish->id, EMPYT_SIDE_CHAR);
        if (error != -ENOERR)
            return error;
        error = NfaStateAddMoveState(subNode->finish, (*starNode)->finish->id, EMPYT_SIDE_CHAR);
        if (error != -ENOERR)
            return error;
        error = NfaStateAddMoveState(subNode->finish, subNode->start->id, EMPYT_SIDE_CHAR);
        if (error != -ENOERR)
            return error;

        return -ENOERR;
    } else {
        return error;
    }
}

/*
 * 功能：文法符号star'的执行动作
 * node：文法符号star的子节点
 * 返回值：成功时返回0，否则返回错误码。
 */
static int RegExStarx(Nfa *nfa, NfaTreeNode *subNode, NfaTreeNode **newNode)
{
    int error = 0;

    if (IsRegExStarx0(nfa)) {
        NfaTreeNode *starNode;

        error = RegExMatch(nfa, '*');
        if (error != -ENOERR)
            return error;
        PrDbg("handle *");
        error = RegExStarAction(nfa, subNode, &starNode);
        if (error != -ENOERR)
            return error;
        *newNode = starNode;
        return RegExStarx(nfa, starNode, newNode);
    } else {
        /*空字符串，不处理。*/
        return -ENOERR;
    }
}

/*
 * 功能：文法符号star的执行动作。
 * node：生成的star新节点。
 * 返回值：成功时返回0，否则返回错误码。
 */
static int RegExStar(Nfa *nfa, NfaTreeNode **node)
{
    int error;
    NfaTreeNode *subNode;

    error = RegExFactor(nfa, &subNode);
    if (error != -ENOERR)
        return error;
    *node = subNode;
    return RegExStarx(nfa, subNode, node);
}

/*
 * 功能：判断词法分析器的向前看符号是否符合连接操作。
 * 返回值：如果词法分析器的向前看符号是符合连接操作返回1，否则返回0。
 */
static char IsRegExCatx0(Nfa *nfa)
{
    if (IsRegExFactor0(nfa)
            || IsRegExId(nfa))
        return 1;
    else
        return 0;
}

/*
 * 功能：文法符号cat对应翻译动作
 * lSubNode：左子节点
 * rSubNode：右子节点
 * catNode：创建的cat节点
 * 返回值：成功时返回0，否则返回错误码。
 */
static int RegExCatAction(Nfa *nfa, NfaTreeNode *lSubNode, NfaTreeNode *rSubNode, NfaTreeNode **catNode)
{
    NfaTreeNode *newNode;
    int error;

    if (!lSubNode || !rSubNode || !catNode)
        return -EINVAL;

    /*建立左子节点的接受状态和右子节点的开始状态之间的连接关系。*/
    error = NfaStateAddMoveState(lSubNode->finish, rSubNode->start->id, EMPYT_SIDE_CHAR);
    if (error != -ENOERR)
        return error;

    /*创建新节点cat*/
    newNode = malloc(sizeof (*newNode));
    if (!newNode) {
        PrErr("error: %d, ENOMEM", -ENOMEM);
        return -ENOMEM;
    }
    list_add(&newNode->node, &nfa->treeList);
    newNode->start = lSubNode->start;
    newNode->finish = rSubNode->finish;
    *catNode = newNode;

    return -ENOERR;
}

/*
 * 功能：文法符号cat'的执行函数
 * lNode：cat的左子节点
 * 返回值：成功时返回0，否则返回错误码。
 */
static int RegExCatx(Nfa *nfa, NfaTreeNode *lSubNode, NfaTreeNode **newNode)
{
    int error = 0;

    if (IsRegExCatx0(nfa)) {
        NfaTreeNode *rSubNode;
        NfaTreeNode *catNode;

        error = RegExStar(nfa, &rSubNode);
        if (error != -ENOERR) {
            return error;
        }
        PrDbg("handle o");
        error = RegExCatAction(nfa, lSubNode, rSubNode, &catNode);
        if (error != -ENOERR) {
            return error;
        }
        *newNode = catNode;
        return RegExCatx(nfa, catNode, newNode);
    } else {
        /*空字符串，不处理。*/
        return error;
    }
}

/*
 * 功能：文法符号cat的执行函数
 * node：生成的cat新节点
 * 返回值：成功时返回0，否则返回错误码。
 */
static int RegExCat(Nfa *nfa, NfaTreeNode **node)
{
    int error;
    NfaTreeNode *lSubNode;

    error = RegExStar(nfa, &lSubNode);
    if (error != -ENOERR) {
        return error;
    }
    *node = lSubNode;
    return RegExCatx(nfa, lSubNode, node);
}

/*
 * 功能：判断词法分析器向前看符号是否为'|'
 * 返回值：如果词法分析器向前看符号是'|'返回1，否则返回0。
 */
static char IsRegExOrx0(Nfa *nfa)
{
    if (nfa->lookahead == '|')
        return 1;
    else
        return 0;
}

/*
 * 功能：文法符号or的翻译动作。
 * lSubNode：左子节点
 * rSubNode：右子节点
 * orNode：生成的新节点or。
 * 返回值：成功时返回0，否则返回错误码。
 */
static int RegExOrAction(Nfa *nfa, NfaTreeNode *lSubNode, NfaTreeNode *rSubNode, NfaTreeNode **orNode)
{
    int error;

    if (!lSubNode || !rSubNode || !orNode)
        return -EINVAL;

    /*申请一个新节点*/
    error = NfaAllocTreeNode(nfa, orNode);
    if (error != -ENOERR)
        return error;
    /*建立新节点or和左右子节点之间的连接关系。*/
    error = NfaStateAddMoveState((*orNode)->start, lSubNode->start->id, EMPYT_SIDE_CHAR);
    if (error != -ENOERR)
        return error;
    error = NfaStateAddMoveState((*orNode)->start, rSubNode->start->id, EMPYT_SIDE_CHAR);
    if (error != -ENOERR)
        return error;
    error = NfaStateAddMoveState(lSubNode->finish, (*orNode)->finish->id, EMPYT_SIDE_CHAR);
    if (error != -ENOERR)
        return error;
    error = NfaStateAddMoveState(rSubNode->finish, (*orNode)->finish->id, EMPYT_SIDE_CHAR);
    if (error != -ENOERR)
        return error;

    return -ENOERR;
}

/*
 * 功能：文法符号or'对应的执行函数
 * lSubNode：左子节点。
 * 返回值：成功时返回0，否则返回错误码。
 */
static int RegExOrx(Nfa *nfa, NfaTreeNode *lSubNode, NfaTreeNode  **newNode)
{
    int error = 0;

    if (IsRegExOrx0(nfa)) {
        NfaTreeNode *rSubNode;
        NfaTreeNode *orNode;

        error = RegExMatch(nfa, '|');
        if (error != -ENOERR)
            return error;
        error = RegExCat(nfa, &rSubNode);
        if (error != -ENOERR)
            return error;
        PrDbg("handle |");
        error = RegExOrAction(nfa, lSubNode, rSubNode, &orNode);
        if (error != -ENOERR)
            return error;
        *newNode = orNode;
        return RegExOrx(nfa, orNode, newNode);
    } else {
        /*空字符串，不处理。*/
        return error;
    }
}

/*
 * 功能：文法符号or执行函数。
 * node：生成的新节点or。
 * 返回值：成功时返回0，否则返回错误码。
 */
static int RegExOr(Nfa *nfa, NfaTreeNode **node)
{
    int error;
    NfaTreeNode *lSubNode;

    error = RegExCat(nfa, &lSubNode);
    if (error != -ENOERR)
        return error;
    *node = lSubNode;
    return RegExOrx(nfa, lSubNode, node);
}

