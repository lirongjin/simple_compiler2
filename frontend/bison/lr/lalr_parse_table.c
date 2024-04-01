#include "lalr_parse_table.h"
#include "context_free_grammar.h"
#include "first_follow.h"
#include "list.h"
#include "cpl_debug.h"
#include "cpl_errno.h"
#include "stdlib.h"
#include "string.h"

#define PrErr(...)      Pr(__FILE__, __LINE__, __FUNCTION__, "error", __VA_ARGS__)

#define LPT_DEBUG
#ifdef LPT_DEBUG
#define PrDbg(...)      Pr(__FILE__, __LINE__, __FUNCTION__, "debug", __VA_ARGS__)
#endif

/*
 * 功能：判断项item中是否包含某个传播关系节点
 * item: 项
 * itemSet: 项集id
 * itemId: 项id
 * 返回值：如果项item中包含这个传播关系节点则返回1，否则返回0。
 **/
static int LrItemIsContainTransmitterItem(const Item *item, int itemSetId, int itemId)
{
    struct list_head *pos;
    const FollowTransmitterItem *transmitter;

    list_for_each(pos, &item->transmitterList) {
        transmitter = container_of(pos, FollowTransmitterItem, node);
        if (transmitter->itemSetId == itemSetId
                && transmitter->itemId == itemId)
            return 1;
    }
    return 0;
}

/*
 * 功能：给项item添加一个向前看符号传播关系节点
 * item：项
 * itemSet: 项集id
 * itemId: 项id
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int LrItemAddFollowTransmitterItem(Item *item, int itemSetId, int itemId)
{
    FollowTransmitterItem *transmitter;

    if (!item)
        return -EINVAL;
    if (LrItemIsContainTransmitterItem(item, itemSetId, itemId) == 1)
        return 0;

    transmitter = malloc(sizeof (*transmitter));
    if (!transmitter)
        return -ENOMEM;
    transmitter->itemSetId = itemSetId;
    transmitter->itemId = itemId;
    list_add_tail(&transmitter->node, &item->transmitterList);
    return 0;
}

/*
 * 功能：释放项的传播关系链表
 * list：传播关系链表
 * 返回值：无
 **/
static void LrItemFreeTransmitterItemList(struct list_head *list)
{
    struct list_head *pos;
    FollowTransmitterItem *transmitter;

    for (pos = list->next; pos != list; ) {
        transmitter = container_of(pos, FollowTransmitterItem, node);
        pos = pos->next;
        list_del(&transmitter->node);
        free(transmitter);
    }
}

/*
 * 功能：申请项资源
 * itemId：项id
 * headSymId：产生式头符号id
 * bodyId: 产生式体id
 * pos: 点的位置
 * 返回值：成功时返回项指针，否则返回NULL。
 **/
static Item *LrAllocItem(int itemId, int headSymId, int bodyId, unsigned int pos)
{
    Item *item;

    item = malloc(sizeof (*item));
    if (item) {
        item->id = itemId;
        item->pos = pos;
        item->productRef.headSymId = headSymId;
        item->productRef.bodyId = bodyId;
        INIT_LIST_HEAD(&item->followList);
        INIT_LIST_HEAD(&item->transmitterList);
    }
    return item;
}

/*
 * 功能：释放项资源
 * item：项
 * 返回值：无
 **/
static void LrFreeItem(Item *item)
{
    if (!item)
        return;
    CfgSymIdListFree(&item->followList);
    LrItemFreeTransmitterItemList(&item->transmitterList);
    free(item);
}

/*
 * 功能：释放项集中的项链表
 * list: 项链表
 * 返回值：无
 **/
static void LrItemSetFreeItemList(struct list_head *list)
{
    struct list_head *pos;
    Item *item;

    for (pos = list->next; pos != list; ) {
        item = container_of(pos, Item, node);
        pos = pos->next;
        list_del(&item->node);
        LrFreeItem(item);
        item = NULL;
    }
}

/*
 * 功能：申请项集GOTO(I, X)节点
 * symId: 文法符号id
 * itemSetId: 项集id
 * 返回值：成功时返回项集GOTO(I, X)节点指针，否则返回NULL
 **/
static MapNode *LrAllocMapNode(int symId, int itemSetId)
{
    MapNode *mapNode;

    mapNode = malloc(sizeof (*mapNode));
    if (mapNode) {
        mapNode->symId = symId;
        mapNode->itemSetId = itemSetId;
    }
    return mapNode;
}

/*
 * 功能：释放项集中的GOTO(I, X)节点链表
 * list: GOTO(I, X)节点链表
 * 返回值：无
 **/
static void LrItemSetFreeMapList(struct list_head *list)
{
    struct list_head *pos;
    MapNode *mapNode;

    for (pos = list->next; pos != list; ) {
        mapNode = container_of(pos, MapNode, node);
        pos = pos ->next;
        free(mapNode);
        mapNode = NULL;
    }
}

/*
 * 功能：申请ACTION节点
 * type: 动作类型
 * lookaheadId：向前看符号
 * id: 项集id或者项id
 * 返回值：成功时返回ACTION指针，否则返回NULL。
 **/
static ActionNode *LrAllocActionNode(ActionType type, int lookaheadId, int id)
{
    ActionNode *actionNode;

    actionNode = malloc(sizeof (*actionNode));
    if (actionNode) {
        actionNode->type = type;
        actionNode->lookaheadId = lookaheadId;
        if (type == LAT_SHIFT) {
            actionNode->itemSetId = id;
        } else if (type == LAT_REDUCT
                   || type == LAT_ACCEPT) {
            actionNode->itemId = id;
        }
    }
    return actionNode;
}

/*
 * 功能：释放项集中的ACTION链表
 * list: ACTION链表
 * 返回值：无
 **/
static void LrItemSetFreeActionList(struct list_head *list)
{
    struct list_head *pos;
    ActionNode *actionNode;

    for (pos = list->next; pos != list; ) {
        actionNode = container_of(pos, ActionNode, node);
        pos = pos->next;
        list_del(&actionNode->node);
        free(actionNode);
    }
}

/*
 * 功能：申请GOTO节点
 * symId: 文法符号
 * nextItemSetId：下一个项集id
 * 返回值：成功时返回GOTO节点指针，否则返回NULL。
 **/
static GotoNode *LrAllocGotoNode(int symId, int nextItemSetId)
{
    GotoNode *gotoNode;

    gotoNode = malloc(sizeof (*gotoNode));
    if (gotoNode) {
        gotoNode->symId = symId;
        gotoNode->nextItemSetId = nextItemSetId;
    }
    return gotoNode;
}

/*
 * 功能：释放项集的GOTO链表
 * list: GOTO链表
 * 返回值：无
 **/
static void LrItemSetFreeGotoList(struct list_head *list)
{
    struct list_head *pos;
    GotoNode *gotoNode;

    for (pos = list->next; pos != list; ) {
        gotoNode = container_of(pos, GotoNode, node);
        pos = pos->next;
        list_del(&gotoNode->node);
        free(gotoNode);
    }
}

/*
 * 功能：申请项集资源
 * id: 项集id
 * 返回值：成功时返回项集指针，否则返回NULL。
 **/
static ItemSet *LrAllocItemSet(int id)
{
    ItemSet *itemSet;

    itemSet = malloc(sizeof (*itemSet));
    if (itemSet) {
        itemSet->id = id;
        itemSet->itemNum = 0;
        INIT_LIST_HEAD(&itemSet->itemList);
        INIT_LIST_HEAD(&itemSet->mapList);
        INIT_LIST_HEAD(&itemSet->actionList);
        INIT_LIST_HEAD(&itemSet->gotoList);
    }
    return itemSet;
}

/*
 * 功能：释放项集资源
 * itemSet：项集
 * 返回值：无
 **/
static void LrFreeItemSet(ItemSet *itemSet)
{
    if (!itemSet)
        return;
    LrItemSetFreeItemList(&itemSet->itemList);
    LrItemSetFreeMapList(&itemSet->itemList);
    LrItemSetFreeActionList(&itemSet->actionList);
    LrItemSetFreeGotoList(&itemSet->gotoList);
    free(itemSet);
    itemSet = NULL;
}

/*
 * 功能：释放项集链表
 * list: 项集链表
 * 返回值：无
 **/
static void LrFreeItemSetList(struct list_head *list)
{
    struct list_head *pos;
    ItemSet *itemSet;

    for (pos = list->next; pos != list; ) {
        itemSet = container_of(pos, ItemSet, node);
        pos = pos->next;
        list_del(&itemSet->node);
        LrFreeItemSet(itemSet);
        itemSet = NULL;
    }
}

int LalrAlloc(Lalr **pLalr, ContextFreeGrammar *grammar, GotoNodeConflictHandle *gotoHandle, ActionNodeConflictHandle *actionHandle)
{
    Lalr *lalr;

    if (!pLalr || !grammar)
        return -EINVAL;

    *pLalr = NULL;
    lalr = malloc(sizeof (*lalr));
    if (!lalr)
        return -ENOMEM;

    *pLalr = lalr;
    INIT_LIST_HEAD(&lalr->itemSetList);
    lalr->gotoNodeConflictHandle = gotoHandle;
    lalr->actionNodeConflictHandle = actionHandle;
    lalr->itemSetNum = 0;
    lalr->grammar = grammar;
    return 0;
}

void LalrFree(Lalr *lalr)
{
    LrFreeItemSetList(&lalr->itemSetList);
    free(lalr);
}

/*
 * 功能：获取项中点(.)右边的文法符号id
 * grammar: 文法
 * item：项
 * 返回值：成功时返回项中点(.)右边的文法符号id，否则返回错误码。
 **/
static int LrItemNextSymId(Lalr *lalr, const Item *item)
{
    ProductBody *productBody;
    struct list_head *pos;
    SymId *symId;
    int e = 0;

    productBody = CfgGetProductBody(lalr->grammar, item->productRef.headSymId, item->productRef.bodyId);
    if (!productBody) {
        return -ENOPDTB;
    }

    if (CfgProductBodyIsEmpty(lalr->grammar, productBody) == 1) {
        if (item->pos == 0) {
            return lalr->grammar->emptySymId;
        } else {
            return -ENOSYM;
        }
    }
    list_for_each(pos, &productBody->symIdList) {
        if (e == item->pos) {
            symId = container_of(pos, SymId, node);
            return symId->id;
        }
        e++;
    }
    return -ENOSYM;
}

/*
 * 功能：获取项点右边符号后边的产生式体部分的符号id链表。
 * grammar: 文法
 * item: 项
 * betaSymIdList：项点右边符号后边的产生式体部分的符号id链表。
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int LrItemNextSymIdBetaSymIdList(Lalr *lalr, const Item *item, struct list_head *betaSymIdList)
{
    ProductBody *productBody;
    struct list_head *pos;
    SymId *symId;
    int e = 0;
    int error = 0;

    productBody = CfgGetProductBody(lalr->grammar, item->productRef.headSymId, item->productRef.bodyId);
    if (!productBody) {
        return -ENOPDTB;
    }

    /*如果是空字符串产生式体，则不存在剩余产生式体，直接返回。*/
    if (CfgProductBodyIsEmpty(lalr->grammar, productBody) == 1) {
        return 0;
    }
    /*获取项点右边符号后边的产生式体部分的符号id链表。*/
    list_for_each(pos, &productBody->symIdList) {
        if (e > item->pos) {
            symId = container_of(pos, SymId, node);
            error = CfgSymIdListTailAddId(betaSymIdList, symId->id);
            if (error != -ENOERR) {
                CfgSymIdListFree(betaSymIdList);
                return error;
            }
        }
        e++;
    }
    return 0;
}

/*
 * 功能：判断项集itemSet中是否包含以headSymId, bodyId, position为核心的项
 * itemSet:项集
 * headSymId：产生式头
 * bodyId: 产生式体id
 * position: 点的位置
 * 返回值：如果存在则返回项集中以headSymId, bodyId, position为核心的项，否则返回NULL。
 */
static Item *LrItemSetIsContainCore(const ItemSet *itemSet, int headSymId, int bodyId, int position)
{
    struct list_head *pos;
    Item *item;

    list_for_each(pos, &itemSet->itemList) {
        item = container_of(pos, Item, node);
        if (item->productRef.headSymId == headSymId
                && item->productRef.bodyId == bodyId
                && item->pos == position) {
            return item;
        }
    }
    return NULL;
}

/*
 * 功能：往项集中添加某项
 * itemSet: 项集
 * headSymId: 产生式头符号id
 * bodyId: 产生式体Id
 * pos: 点的位置
 * followList：项的向前看符号id链表
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int LrItemSetAddItem(ItemSet *itemSet, int headSymId, int bodyId, int pos, const struct list_head *followList)
{
    Item *item;

    item = LrItemSetIsContainCore(itemSet, headSymId, bodyId, pos);
    if (item) {
        if (CfgSymIdListIsContainList(&item->followList, followList) == 1) {
            return 0;
        } else {
            return CfgSymIdSetAddList(&item->followList, followList);
        }
    } else {
        item = LrAllocItem(itemSet->itemNum++, headSymId, bodyId, pos);
        if (!item) {
            return -ENOMEM;
        }
        list_add_tail(&item->node, &itemSet->itemList);
        return CfgSymIdSetAddList(&item->followList, followList);
    }
}

/*
 * 功能：往项集中添加某项，如果添加了一项则updateFlag置1，否则置0
 * grammar: 文法
 * itemSet: 项集
 * transmitterItem：需要添加传播关系节点的项，即待创建项的父项
 * headSymId: 产生式头符号id
 * bodyId: 产生式体Id
 * pos: 点的位置
 * followList：项的向前看符号id链表
 * flag：如果添加了一项则updateFlag置1，否则置0
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int LrItemSetAddItemUpdateFlag(Lalr *lalr, ItemSet *itemSet, Item *transmitterItem, int headSymId, int bodyId, int pos,
                                       const struct list_head *followList, char *flag)
{
    Item *item;
    int error = 0;

     *flag = 0;
    item = LrItemSetIsContainCore(itemSet, headSymId, bodyId, pos);
    if (item) {
        if (CfgSymIdListIsContainList(&item->followList, followList) == 0) {
            *flag = 1;
            error = CfgSymIdSetAddList(&item->followList, followList);
            if (error != -ENOERR)
                return error;
        }
    } else {
        *flag = 1;
        item = LrAllocItem(itemSet->itemNum++, headSymId, bodyId, pos);
        if (!item) {
            return -ENOMEM;
        }
        list_add_tail(&item->node, &itemSet->itemList);
        error = CfgSymIdSetAddList(&item->followList, followList);
        if (error != -ENOERR)
            return error;
    }
    /*如果待添加项的向前看符号链表中包含了传播符号，则给父项添加一个传播关系节点*/
    if (CfgSymIdSetIsContain(followList, lalr->grammar->maxSymId) == 1) {
        error = LrItemAddFollowTransmitterItem(transmitterItem, itemSet->id, item->id);
        if (error != -ENOERR)
            return error;
    }
    return error;
}

/*
 * 功能：获取项中点右边符号的右边剩余的产生式体的FIRST集合
 * grammar: 文法
 * betaSymIdList:项中点右边符号的右边剩余的产生式体
 * firstSymIdList:项中点右边符号的右边剩余的产生式体的FIRST集合
 * 返回值：成功是返回0，否则返回错误码。
 **/
static int LrBetaSymIdListGenFirst(Lalr *lalr, const struct list_head *betaSymIdList, struct list_head *firstSymIdList)
{
    struct list_head iterFirstList;
    struct list_head *pos;
    SymId *symId;
    int error = 0;
    char emptyFlag = 1;

    /*如果项中点右边符号的右边没有剩余的产生式体，则不存在FIRST集合，直接返回。*/
    if (list_empty(betaSymIdList)) {
        return 0;
    }

    INIT_LIST_HEAD(&iterFirstList);
    /*从靠近头的地方遍历产生式体中的各个文法符号，添加文法符号的FIRT集合，如果FIRST集合包含空符号id，
        则继续添加下一个符号的FIRST集合，以此类推，FIRST集合中不包含空符号id则退出。如果所有的文法符号都
        包含空符号id，则把空符号id添加到待计算的FIRST集合中。*/
    list_for_each(pos, betaSymIdList) {
        symId = container_of(pos, SymId, node);
        INIT_LIST_HEAD(&iterFirstList);
        error = CfgGetSymIdFirst(lalr->grammar, symId->id, &iterFirstList);
        if (error != -ENOERR) {
            CfgSymIdListFree(&iterFirstList);
            CfgSymIdListFree(firstSymIdList);
            return error;
        }
        error = CfgSymIdSetAddListExcludeId(firstSymIdList, &iterFirstList, lalr->grammar->emptySymId);
        if (error != -ENOERR) {
            CfgSymIdListFree(&iterFirstList);
            CfgSymIdListFree(firstSymIdList);
            return error;
        }
        if (CfgSymIdSetIsContain(&iterFirstList, lalr->grammar->emptySymId) == 0) {
            CfgSymIdListFree(&iterFirstList);
            emptyFlag = 0;
            break;
        }
        CfgSymIdListFree(&iterFirstList);
    }
    if (emptyFlag == 1) {
        error = CfgSymIdSetAddId(firstSymIdList, lalr->grammar->emptySymId);
        if (error != -ENOERR) {
            CfgSymIdListFree(firstSymIdList);
            return error;
        }
    }

    return error;
}

/*
 * 功能：根据项中点右边符号的右边剩余的产生式体和项所在的向前看符号id链表，生成新项的向前看符号链表
 * grammar: 文法
 * betaSymIdList：项中点右边符号的右边剩余的产生式体
 * itemFollowList：项所在的向前看符号id链表
 * followList：新项的向前看符号链表
 * 返回值：成功是返回0，否则返回错误码。
 **/
static int LrItemGenFollow(Lalr *lalr, const struct list_head *betaSymIdList, const struct list_head *itemFollowList, struct list_head *followList)
{
    int error = 0;
    struct list_head firstSymIdList;

    /*获取项中点右边符号的右边剩余的产生式体的FIRST集合*/
    INIT_LIST_HEAD(&firstSymIdList);
    error = LrBetaSymIdListGenFirst(lalr, betaSymIdList, &firstSymIdList);
    if (error != -ENOERR)
        return error;

    /*添加FIRST集合中除空符号id外的所有符号id到向前看符号链表。*/
    error = CfgSymIdSetAddListExcludeId(followList, &firstSymIdList, lalr->grammar->emptySymId);
    if (error != -ENOERR) {
        CfgSymIdListFree(followList);
        CfgSymIdListFree(&firstSymIdList);
        return error;
    }

    /*如果FIRST集合中包含空符号id或者项中点右边符号的右边没有剩余的产生式体，则把项所在的向前看符号id链表添加到新项的向前看符号链表。*/
    if (CfgSymIdSetIsContain(&firstSymIdList, lalr->grammar->emptySymId) == 1
            || list_empty(betaSymIdList)) {
        error = CfgSymIdSetAddListExcludeId(followList, itemFollowList, lalr->grammar->emptySymId);
        if (error != -ENOERR) {
            CfgSymIdListFree(followList);
            CfgSymIdListFree(&firstSymIdList);
            return error;
        }
    }
    CfgSymIdListFree(&firstSymIdList);

    return error;
}

/*
 * 功能：删除项集中各项向前看链表中的不可能出现的符号id。
 * grammar: 文法
 * itemSet：项集
 * 返回值：无
 **/
static void LrItemSetRemoveItemsFollowMaxSymId(const Lalr *lalr, ItemSet *itemSet)
{
    struct list_head *pos;
    Item *item;

    list_for_each(pos, &itemSet->itemList) {
        item = container_of(pos, Item, node);
        CfgSymIdListDelId(&item->followList, lalr->grammar->maxSymId);
    }
}

/*
 * 功能：根据非终结符添加项集的非内核项
 * grammar:文法
 * itemSet: 项集
 * symbol: 文法符号
 * betaSymIdList：项点右边符号后边的产生式体部分的符号id链表。
 * item：所在项
 * updateFlag: 如果添加了一项则updateFlag置1，否则置0
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int LrItemSetExtendByNonterminal(Lalr *lalr, ItemSet *itemSet, const Symbol *symbol,
                                         const struct list_head *betaSymIdList, Item *item, char *updateFlag)
{
    int r = 0;
    int error = 0;
    struct list_head newFollowList; /*待生成项的向前看符号id链表*/
    char localUpdateFlag;
    struct list_head maxSymIdFollow;    /*不可能出现的文法符号id，用于判断传播关系。*/

    INIT_LIST_HEAD(&maxSymIdFollow);
    error = CfgSymIdSetAddId(&maxSymIdFollow, lalr->grammar->maxSymId);
    if (error != -ENOERR) {
        return error;
    }

    INIT_LIST_HEAD(&newFollowList);
    /*创建新项的向前看符号链表*/
    error = LrItemGenFollow(lalr, betaSymIdList, &maxSymIdFollow, &newFollowList);
    if (error != -ENOERR) {
        goto err0;
    }

    *updateFlag = 0;
    /*根据非终结符号id和它的产生式个数添加项。*/
    for (r = 0; r < symbol->bodyNum; r++) {
        error = LrItemSetAddItemUpdateFlag(lalr, itemSet, item, symbol->id, r, 0, &newFollowList, &localUpdateFlag);
        if (error != -ENOERR) {
            goto err1;
        }
        if (localUpdateFlag == 1) {
            *updateFlag = 1;
        }
    }

err1:
    CfgSymIdListFree(&newFollowList);
err0:
    CfgSymIdListFree(&maxSymIdFollow);
    return error;
}

/*
 * 功能：计算项集的闭包，生成能自发生成的向前看符号，建立项集内部各项的向前看符号传播关系。
 * grammar: 文法
 * itemSet：项集
 * 返回值：成功时返回0，否则返回错误码。
 **/
int LrClosure(Lalr *lalr, ItemSet *itemSet)
{
    char updateFlag;    /*是否有新项生成的标记*/
    int error = 0;
    struct list_head *pos;
    Item *item;

    if (!lalr || !itemSet)
        return -EINVAL;

    /*循环遍历项集中所有的项，直到没有新的项和向前看符号生成。
        （也许不必考虑向前看符号的变动，即使考虑也没关系，可能会多执行几遍。）*/
    do {
        updateFlag = 0;
        for (pos = itemSet->itemList.next; pos != &itemSet->itemList; ) {
            Symbol *nextSymbol;
            int nextSymId;

            item = container_of(pos, Item, node);
            pos = pos->next;

            /*获取项中点(.)右边的符号id*/
            error = nextSymId = LrItemNextSymId(lalr, item);
            if (error == -ENOSYM) {
                continue;
            } else if (error < 0) {
                return error;
            }
            nextSymbol = CfgGetSymbol(lalr->grammar, nextSymId);
            if (!nextSymbol) {
                return -ENOSYM;
            }
            if (nextSymbol->type == SYMBOL_TYPE_NONTERMINAL) {
                char flag;
                struct list_head betaSymIdList; /*项点右边符号后边的产生式体部分的符号id链表。*/

                INIT_LIST_HEAD(&betaSymIdList);
                error = LrItemNextSymIdBetaSymIdList(lalr, item, &betaSymIdList);
                if (error != -ENOERR)
                    return error;
                /*根据非终结符添加项集的非内核项*/
                error = LrItemSetExtendByNonterminal(lalr, itemSet, nextSymbol,
                                                      &betaSymIdList, item, &flag);
                CfgSymIdListFree(&betaSymIdList);
                if (error != -ENOERR)
                    return error;
                if (flag) {
                    updateFlag = 1;
                }
            }
        }
    } while (updateFlag);
    /*删除项集中各项向前看链表中的不可能出现的符号id。*/
    LrItemSetRemoveItemsFollowMaxSymId(lalr, itemSet);

    return 0;
}

/*
 * 功能：生成起始项集。
 * grammar: 文法
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int LrGenStartItemSet(Lalr *lalr)
{
    ItemSet *itemSet;
    Item *item;
    int error = 0;

    itemSet = LrAllocItemSet(lalr->itemSetNum++);
    if (!itemSet) {
        return -ENOMEM;
    }
    list_add_tail(&itemSet->node, &lalr->itemSetList);
    item = LrAllocItem(itemSet->itemNum++, lalr->grammar->startSymId, 0, 0);
    if (!item) {
        return -ENOMEM;
    }
    list_add_tail(&item->node, &itemSet->itemList);
    error = CfgSymIdSetAddId(&item->followList, lalr->grammar->endSymId);
    if (error != -ENOERR)
        return error;

    return LrClosure(lalr, itemSet);
}

/*
 * 功能：判断项集itemSet是否包含某个映射节点，如果包含返回1，否则返回0。
 * itemSet: 项集
 * symId： 符号id
 * itemSetId: 项集id
 * 返回值：如果项集itemSet包含某个映射节点则含返回1，否则返回0。
 **/
static int LrItemSetIsContainMapNode(const ItemSet *itemSet, int symId, int itemSetId)
{
    MapNode *mapNode;
    struct list_head *pos;

    list_for_each(pos, &itemSet->mapList) {
        mapNode = container_of(pos, MapNode, node);
        if (mapNode->symId == symId
                && mapNode->itemSetId == itemSetId) {
            return 1;
        }
    }
    return 0;
}

/*
 * 功能：往项集itemSet添加一个映射节点
 * itemSet: 项集
 * symId：符号id
 * itemSetId: 项集id
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int LrItemSetAddMap(ItemSet *itemSet, int symId, int itemSetId)
{
    MapNode *mapNode;

    if (LrItemSetIsContainMapNode(itemSet, symId, itemSetId) == 1)
        return 0;
    mapNode = LrAllocMapNode(symId, itemSetId);
    if (!mapNode) {
        return -ENOMEM;
    }
    list_add_tail(&mapNode->node, &itemSet->mapList);
    return 0;
}

/*
 * 功能：判断两个项的内核是否相等
 * lItem：左项
 * rItem：右项
 * 返回值：如果两个项的内核相等则返回1，否则返回0。
 **/
static inline int LrItemIsCoreEqual(const Item *lItem, const Item *rItem)
{
    return lItem->productRef.headSymId == rItem->productRef.headSymId
            && lItem->productRef.bodyId == rItem->productRef.bodyId
            && lItem->pos == rItem->pos;
}

/*
 * 功能：向项集itemSet中的相关项中添加项item的向前看符号
 * itemSet：项集
 * item：项
 * updateFlag：如果添加了向前看符号则置1，否则置0。
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int LrItemSetAddFollowByItem(ItemSet *itemSet, const Item *item, char *updateFlag)
{
    struct list_head *pos;
    Item *iterItem;

    *updateFlag = 0;
    list_for_each(pos, &itemSet->itemList) {
        iterItem = container_of(pos, Item, node);
        if (LrItemIsCoreEqual(iterItem, item) == 1) {
            if (CfgSymIdListIsContainList(&iterItem->followList, &item->followList) == 0) {
                *updateFlag = 1;
                return CfgSymIdSetAddList(&iterItem->followList, &item->followList);
            }
        }
    }
    return 0;
}

/*
 * 功能：把项集rItemSet项中的向前看符号添加到项集lItemSet的相关项中
 * lItemSet：左项集
 * rItemSet：右项集
 * updateFlag：如果添加了向前看符号则置1，否则置0。
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int LrItemSetAddFollowByItemSet(ItemSet *lItemSet, const ItemSet *rItemSet, char *updateFlag)
{
    const struct list_head *pos;
    const Item *item;
    int error = 0;
    char localUpdateFlag = 0;

    *updateFlag = 0;
    list_for_each(pos, &rItemSet->itemList) {
        item = container_of(pos, Item, node);
        error = LrItemSetAddFollowByItem(lItemSet, item, &localUpdateFlag);
        if (error != -ENOERR)
            return error;
        if (localUpdateFlag == 1)
            *updateFlag = 1;
    }
    return error;
}

static ItemSet *LrGetItemSetByItemSetCore(Lalr *lalr, ItemSet *itemSet);

/*
 * 功能：创建LR文法的GOTO(I, X)函数映射表
 * grammar: 文法
 * itemSet: 项集
 * symId: 符号id
 * updateFlag: 如果添加了新节点则updateFlag置1，否则置0
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int LrItemSetGotoBySymId(Lalr *lalr, ItemSet *itemSet, int symId, char *updateFlag)
{
    ItemSet *alphaItemSet;
    int error = 0;
    struct list_head *pos;
    const Item *item;
    char has = 0;

    *updateFlag = 0;
    /*申请新项集alpha*/
    alphaItemSet = LrAllocItemSet(lalr->itemSetNum);
    if (!alphaItemSet) {
        return -ENOMEM;
    }

    /*遍历项集中的所有项，找到下一个符号是symId的项并添加到新项集alpha*/
    list_for_each(pos, &itemSet->itemList) {
        item = container_of(pos, Item, node);
        if (LrItemNextSymId(lalr, item) == symId) {
            struct list_head nullFollowList;

            INIT_LIST_HEAD(&nullFollowList);
            error = LrItemSetAddItem(alphaItemSet, item->productRef.headSymId, item->productRef.bodyId,
                          item->pos + 1, &nullFollowList);
            if (error != -ENOERR) {
                LrFreeItemSet(alphaItemSet);
                return error;
            }
            has = 1;
        }
    }
    /*如果找到一个符号的项则计算新项集alpha的闭包，如果项集族中不存在这个项集alpha则添加到项集族。
      创建项集itemSet在符号symId上的下一个项集nextItemSet的GOTO(I, X)节点。*/
    if (has == 1) {
        ItemSet *nextItemSet;

        error = LrClosure(lalr, alphaItemSet);
        if (error != -ENOERR) {
            LrFreeItemSet(alphaItemSet);
            return error;
        }

        /*寻找项集族中跟项集alpha核心相同的项集*/
        nextItemSet = LrGetItemSetByItemSetCore(lalr, alphaItemSet);
        if (!nextItemSet) {
            *updateFlag = 1;
            list_add_tail(&alphaItemSet->node, &lalr->itemSetList);
            lalr->itemSetNum++;
            nextItemSet = alphaItemSet;
        } else {
            error = LrItemSetAddFollowByItemSet(nextItemSet, alphaItemSet, updateFlag);
            LrFreeItemSet(alphaItemSet);
            if (error != -ENOERR) {
                return error;
            }
        }
        /*创建项集itemSet在符号symId上的下一个项集nextItemSet的GOTO(I, X)节点。*/
        error = LrItemSetAddMap(itemSet, symId, nextItemSet->id);
        if (error != -ENOERR) {
            return error;
        }
    }

    return error;
}

/*
 * 功能：生成项集在各个文法符号上的下一个项集和GOTO(I, X)映射节点
 * grammar: 文法
 * itemSet: 项集
 * symIdList: 符号id链表
 * updateFlag: 如果添加了新节点则updateFlag置1，否则置0
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int LrItemSetGotoBySymIdList(Lalr *lalr, ItemSet *itemSet, struct list_head *symIdList, char *updateFlag)
{
    int error = 0;
    struct list_head *pos;
    SymId *symId;

    *updateFlag = 0;
    list_for_each(pos, symIdList) {
        char flag = 0;

        symId = container_of(pos, SymId, node);
        error = LrItemSetGotoBySymId(lalr, itemSet, symId->id, &flag);
        if (error != -ENOERR) {
            return error;
        }
        if (flag == 1) {
            *updateFlag = 1;
        }
    }
    return error;
}

/*
 * 功能：获取项集itemSet中所有项点(.)右边的符号id
 * grammar: 文法
 * itemSet: 项集
 * symIdList: 符号id链表
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int LrItemSetGetNextSymIdList(Lalr *lalr, ItemSet *itemSet, struct list_head *symIdList)
{
    struct list_head *pos;
    Item *item;
    int symId;
    int error = 0;

    list_for_each(pos, &itemSet->itemList) {
        item = container_of(pos, Item, node);
        symId = LrItemNextSymId(lalr, item);
        if (symId >= 0
                && symId != lalr->grammar->emptySymId) {
            error = CfgSymIdSetAddId(symIdList, symId);
            if (error != -ENOERR)
                return error;
        }
    }
    return 0;
}

/*
 * 功能：判断项集lItemSet的核心是否包含了项集rItemSet的核心
 * lItemSet：左项集
 * rItemSet：右项集
 * 返回值：如果项集lItemSet的核心包含了项集rItemSet的核心返回1，否则返回0。
 **/
static int LrItemSetCoreIsContain(const ItemSet *lItemSet, const ItemSet *rItemSet)
{
    const struct list_head *pos;
    const Item *item;

    list_for_each(pos, &rItemSet->itemList) {
        item = container_of(pos, Item, node);
        if (LrItemSetIsContainCore(lItemSet, item->productRef.headSymId,
                                    item->productRef.bodyId, item->pos) == 0)
            return 0;
    }
    return 1;
}

/*
 * 功能：判断项集lItemSet的核心是否和项集rItemSet的核心相等。
 * lItemSet：左项集
 * rItemSet：右项集
 * 返回值：如果项集lItemSet的核心和项集rItemSet的核心相等则返回1，否则返回0。
 **/
static int LrItemSetIsCoreEqual(const ItemSet *lItemSet, const ItemSet *rItemSet)
{
    if (LrItemSetCoreIsContain(lItemSet, rItemSet) == 1
            && LrItemSetCoreIsContain(rItemSet, lItemSet) == 1)
        return 1;
    else
        return 0;
}

/*
 * 功能：找到一个项集族中和项集itemSet核心相同的项集
 * grammar: 文法
 * itemSet项集
 * 返回值：如果存在返回项集族中和项集itemSet核心相同的项集，否则返回NULL。
 **/
static ItemSet *LrGetItemSetByItemSetCore(Lalr *lalr, ItemSet *itemSet)
{
    struct list_head *pos;
    ItemSet *iter;

    list_for_each(pos, &lalr->itemSetList) {
        iter = container_of(pos, ItemSet, node);
        if (LrItemSetIsCoreEqual(iter, itemSet) == 1) {
            return iter;
        }
    }
    return NULL;
}

/*
 * 功能：生成项集itemSet的向前看符号传播关系节点，在项集gotoItemSet的项gotoItem上的。
 * itemSet：本项集
 * gotoItemSet：GOTO(I, X)项集
 * gotoItem：GOTO(I, X)项集中的项
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int LrItemSetGenTransmitterByGotoItem(ItemSet *itemSet, ItemSet *gotoItemSet, Item *gotoItem)
{
    struct list_head *pos;
    Item *item;
    int error = 0;

    list_for_each(pos, &itemSet->itemList) {
        item = container_of(pos, Item, node);
        if (item->productRef.headSymId == gotoItem->productRef.headSymId
                && item->productRef.bodyId == gotoItem->productRef.bodyId
                && item->pos + 1 == gotoItem->pos) {
            error = LrItemAddFollowTransmitterItem(item, gotoItemSet->id, gotoItem->id);
            if (error != -ENOERR)
                return error;
        }
    }
    return error;
}

/*
 * 功能：根据项集itemSet的GOTO(I, X)项集gotoItemSet，生成项集itemSet中相关向的传播关系节点。
 * 因为项集gotoItemSet中各项的向前看符号都由项集itemSet中的相关向传播而来。
 * itemSet：本项集
 * gotoItemSet：GOTO(I, X)项集
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int LrItemSetGenTransmitterByGotoItemSet(ItemSet *itemSet, ItemSet *gotoItemSet)
{
    struct list_head *pos;
    Item *item;
    int error = 0;

    list_for_each(pos, &gotoItemSet->itemList) {
        item = container_of(pos, Item, node);
        if (item->pos != 0) {
            error = LrItemSetGenTransmitterByGotoItem(itemSet, gotoItemSet, item);
            if (error != -ENOERR)
                return error;
        }
    }
    return error;
}

/*
 * 功能：生成项集在符号symId上的相关向的传播关系节点
 * grammar：文法
 * itemSet：项集
 * symId：符号id
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int LrItemGenTransmitterList(Lalr *lalr, ItemSet *itemSet, int symId)
{
    ItemSet *alphaItemSet;
    int error = 0;
    struct list_head *pos;
    const Item *item;
    char has = 0;

    /*申请新项集alpha*/
    alphaItemSet = LrAllocItemSet(lalr->itemSetNum);
    if (!alphaItemSet) {
        return -ENOMEM;
    }

    /*遍历项集中的所有项，找到下一个符号是symId的项并添加到新项集alpha*/
    list_for_each(pos, &itemSet->itemList) {
        item = container_of(pos, Item, node);
        if (LrItemNextSymId(lalr, item) == symId) {
            struct list_head nullFollowList;

            INIT_LIST_HEAD(&nullFollowList);
            error = LrItemSetAddItem(alphaItemSet, item->productRef.headSymId, item->productRef.bodyId,
                          item->pos + 1, &nullFollowList);
            if (error != -ENOERR) {
                LrFreeItemSet(alphaItemSet);
                return error;
            }
            has = 1;
        }
    }

    /*如果找到一个符号的项则计算新项集alpha的闭包。*/
    if (has == 1) {
        ItemSet *nextItemSet;

        error = LrClosure(lalr, alphaItemSet);
        if (error != -ENOERR) {
            LrFreeItemSet(alphaItemSet);
            return error;
        }

        /*寻找项集族中跟项集alpha相同的项集*/
        nextItemSet = LrGetItemSetByItemSetCore(lalr, alphaItemSet);
        if (!nextItemSet) {
            LrFreeItemSet(alphaItemSet);
            return -ENOSTATE;
        } else {
            LrItemSetGenTransmitterByGotoItemSet(itemSet, nextItemSet);
            if (error != -ENOERR) {
                LrFreeItemSet(alphaItemSet);
                return error;
            }
        }
    }
    LrFreeItemSet(alphaItemSet);

    return error;
}

/*
 * 功能：生成项集中各项的在GOTO(I, X)上的向前看符号传播关系
 * grammar：文法
 * itemSet：项集
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int LrGenItemSetTransmitterItem(Lalr *lalr, ItemSet *itemSet)
{
    struct list_head nextSymIdList;
    int error = 0;
    struct list_head *pos;
    SymId *symId;

    INIT_LIST_HEAD(&nextSymIdList);
    /*获取项集中所有项的点(.)右边的符号id*/
    error = LrItemSetGetNextSymIdList(lalr, itemSet, &nextSymIdList);
    if (error != -ENOERR) {
        CfgSymIdListFree(&nextSymIdList);
        return error;
    }

    /*生成各个项的向前看符号的传播关系节点*/
    list_for_each(pos, &nextSymIdList) {
        symId = container_of(pos, SymId, node);
        error = LrItemGenTransmitterList(lalr, itemSet, symId->id);
        if (error != -ENOERR) {
            CfgSymIdListFree(&nextSymIdList);
            return error;
        }
    }
    CfgSymIdListFree(&nextSymIdList);
    return error;
}

/*
 * 功能：生成项集族中各项集中各项的在GOTO(I, X)上的向前看符号传播关系
 *      向前看的传播关系有2种，一种是在计算项集闭包时的传播关系，
 *      一种是在GOTO（I, X）上的传播关系。
 *      在计算项集的闭包时，项集内部的向前看符号传播关系已经建立。
 * grammar: 文法
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int LrGenItemSetsTransmitterItem(Lalr *lalr)
{
    struct list_head *pos;
    ItemSet *itemSet;
    int error = 0;

    list_for_each(pos, &lalr->itemSetList) {
        itemSet = container_of(pos, ItemSet, node);
        error = LrGenItemSetTransmitterItem(lalr, itemSet);
        if (error != -ENOERR) {
            return error;
        }
    }
    return error;
}

/*
 * 功能：根据项与项之间向前看符号的传播关系传播向前看符号。
 * grammar: 文法
 * item: 项
 * transmitter: 传播关系节点
 * updateFlag: 如果发送了向前看符的传播则置1，否则置0。
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int _LrItemTransmitterFollow(Lalr *lalr, const Item *item, FollowTransmitterItem *transmitter,
                                     char *updateFlag)
{
    Item *transedItem;

    transedItem = LalrItemSetGetItem(LalrGetItemSet(lalr, transmitter->itemSetId), transmitter->itemId);
    if (!transedItem) {
        return -ENOITEM;
    }
    *updateFlag = 0;
    if (CfgSymIdListIsContainList(&transedItem->followList, &item->followList) == 0) {
        *updateFlag = 1;
        return CfgSymIdSetAddList(&transedItem->followList, &item->followList);
    }
    return 0;
}

/*
 * 功能：根据项与项之间向前看符号的传播关系传播向前看符号。
 * grammar: 文法
 * item: 项
 * updateFlag: 如果发送了向前看符的传播则置1，否则置0。
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int LrItemTransmitterFollow(Lalr *lalr, Item *item, char *updateFlag)
{
    struct list_head *pos;
    FollowTransmitterItem *transmitter;
    int error = 0;
    char localUpdateFlag;

    *updateFlag = 0;
    list_for_each(pos, &item->transmitterList) {
        transmitter = container_of(pos, FollowTransmitterItem, node);
        error = _LrItemTransmitterFollow(lalr, item, transmitter, &localUpdateFlag);
        if (error != -ENOERR)
            return error;
        if (localUpdateFlag == 1)
            *updateFlag = 1;
    }
    return error;
}

/*
 * 功能：根据项与项之间向前看符号的传播关系传播向前看符号。
 * grammar: 文法
 * itemSet:项集
 * updateFlag: 如果发送了向前看符的传播则置1，否则置0。
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int LrGenItemSetFollow(Lalr *lalr, ItemSet *itemSet, char *updateFlag)
{
    struct list_head *pos;
    Item *item;
    int error = 0;
    char localUpdateFlag;

    *updateFlag = 0;
    list_for_each(pos, &itemSet->itemList) {
        item = container_of(pos, Item, node);
        error = LrItemTransmitterFollow(lalr, item, &localUpdateFlag);
        if (error != -ENOERR)
            return error;
        if (localUpdateFlag == 1)
            *updateFlag = 1;
    }
    return error;
}

/*
 * 功能：根据项与项之间向前看符号的传播关系传播向前看符号。
 * grammar：文法
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int LrGenItemSetsFollow(Lalr *lalr)
{
    struct list_head *pos;
    ItemSet *itemSet;
    int error = 0;
    char updateFlag;

    do {
        updateFlag = 0;
        list_for_each(pos, &lalr->itemSetList) {
            char localUpdateFlag = 0;

            itemSet = container_of(pos, ItemSet, node);
            error = LrGenItemSetFollow(lalr, itemSet, &localUpdateFlag);
            if (error != -ENOERR)
                return error;
            if (localUpdateFlag == 1)
                updateFlag = 1;
        }
    } while (updateFlag == 1);
    return error;
}

/*
 * 功能：生成文法LALR项集族和GOTO(I, X)映射节点。
 * grammar: 文法
 * 返回值：成功时返回0，否则返回错误码。
 **/
int LalrGenItemSet(Lalr *lalr)
{
    int error = 0;
    char updateFlag;
    struct list_head *pos;
    ItemSet *itemSet;
    struct list_head nextSymIdList; /*项中点右边的符号id的链表*/

    if (!lalr)
        return -EINVAL;

    INIT_LIST_HEAD(&nextSymIdList);
    error = CfgGenFirstFollow(lalr->grammar);
    if (error != -ENOERR)
        return error;
    //CfgPrintFirstFollow(lalr->grammar);
    error = LrGenStartItemSet(lalr);
    if (error != -ENOERR)
        return error;
    /*循环遍历项集族中所有的项集，生成项集在各个文法符号上的下一个项集和GOTO(I, X)映射节点，
        并生成计算闭包时自发形成的向前看符号。*/
    do {
        updateFlag = 0;
        for (pos = lalr->itemSetList.next; pos != &lalr->itemSetList; ) {
            char localUpdateFlag;

            INIT_LIST_HEAD(&nextSymIdList);
            itemSet = container_of(pos, ItemSet, node);
            pos = pos->next;

            /*获取项集中所有项的点(.)右边的符号id*/
            error = LrItemSetGetNextSymIdList(lalr, itemSet, &nextSymIdList);
            if (error != -ENOERR) {
                goto err;
            }
            /*生成项集在各个文法符号上的下一个项集和GOTO(I, X)映射节点*/
            error = LrItemSetGotoBySymIdList(lalr, itemSet, &nextSymIdList, &localUpdateFlag);
            if (error != -ENOERR) {
                goto err;
            }
            if (localUpdateFlag == 1)
                updateFlag = 1;
            CfgSymIdListFree(&nextSymIdList);
        }
    } while (updateFlag == 1);

    error = LrGenItemSetsTransmitterItem(lalr);
    if (error != -ENOERR) {
        goto err;
    }
    error = LrGenItemSetsFollow(lalr);
    if (error != -ENOERR) {
        goto err;
    }

    return error;
err:
    CfgSymIdListFree(&nextSymIdList);
    return error;
}

/*
 * 功能：获取项集
 * grammar: 文法
 * itemSetId: 项集id
 * 返回值：成功时返回项集，否则返回NULL。
 **/
ItemSet *LalrGetItemSet(Lalr *lalr, int itemSetId)
{
    ItemSet *itemSet = NULL;
    struct list_head *pos;

    if (!lalr)
        return NULL;

    list_for_each(pos, &lalr->itemSetList) {
        itemSet = container_of(pos, ItemSet, node);
        if (itemSet->id == itemSetId)
            return itemSet;
    }
    return NULL;
}

/*
 * 功能：获取项集中的项
 * itemSet: 项集
 * id：项id
 * 返回值：成功时返回项集中的项指针，否则返回NULL。
 **/
Item *LalrItemSetGetItem(const ItemSet *itemSet, int id)
{
    struct list_head *pos;
    Item *item;

    if (!itemSet)
        return NULL;

    list_for_each(pos, &itemSet->itemList) {
        item = container_of(pos, Item, node);
        if (item->id == id)
            return item;
    }
    return NULL;
}

/*
 * 功能：获取项集GOTO(itemSet, symId)的项集id
 * itemSet: 项集
 * symId: 符号id
 * 返回值：成功时返回GOTO(itemSet, symId)的项集id否则返回错误码。
 **/
static int LrItemSetGoto(ItemSet *itemSet, int symId)
{
    MapNode *mapNode;
    struct list_head *pos;

    list_for_each(pos, &itemSet->mapList) {
        mapNode = container_of(pos, MapNode, node);
        if (mapNode->symId == symId)
            return mapNode->itemSetId;
    }
    return -ENOSTATE;
}

/*
 * 功能：获取项集GOTO(itemSet, symId)的id
 * grammar: 文法
 * itemSetId：项集id
 * symId：符号id
 * 返回值：成功时返回GOTO(itemSet, symId)的项集id否则返回错误码。
 **/
int LalrGoto(Lalr *lalr, int itemSetId, int symId)
{
    ItemSet *itemSet;
    int nextItemSetId;

    if (!lalr)
        return -EINVAL;
    itemSet = LalrGetItemSet(lalr, itemSetId);
    if (!itemSet) {
        return -ENOSTATE;
    }
    nextItemSetId = LrItemSetGoto(itemSet, symId);
    return nextItemSetId;
}

/*
 * 功能：判断项集的GOTO节点是否发送了冲突
 * itemSet: 项集
 * symId: 向前看文法符号
 * pGotoNode：冲突的节点
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int LrItemSetGotoNodeIsConflict(ItemSet *itemSet, int symId, GotoNode **pGotoNode)
{
    GotoNode *gotoNode;
    struct list_head *pos;

    *pGotoNode = NULL;
    list_for_each(pos, &itemSet->gotoList) {
        gotoNode = container_of(pos, GotoNode, node);
        if (gotoNode->symId == symId) {
            *pGotoNode = gotoNode;
            return 1;
        }
    }
    return 0;
}

/*
 * 功能：判断项集ACTION节点是否有冲突
 * itemSet: 项集
 * symId: 向前看文法符号
 * pActionNode：动作节点
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int LrItemSetActionNodeIsConflict(ItemSet *itemSet, int symId, ActionNode **pActionNode)
{
    struct list_head *pos;
    ActionNode *actionNode;

    list_for_each(pos, &itemSet->actionList) {
        actionNode = container_of(pos, ActionNode, node);
        if (actionNode->lookaheadId == symId) {
            *pActionNode = actionNode;
            return 1;
        }
    }
    return 0;
}

/*
 * 功能：根据GOTO(I, X)函数生成项集itemSet的语法分析表节点
 * grammar: 文法
 * itemSet: 项集
 * mapNode: GOTO(I, X)节点
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int LrItemSetGenParseTableByMapNode(Lalr *lalr, ItemSet *itemSet, MapNode *mapNode)
{
    int error = 0;
    Symbol *symbol;

    /*获取GOTO(I, X)中X对应的文法符号指针*/
    symbol = CfgGetSymbol(lalr->grammar, mapNode->symId);
    if (!symbol) {
        return -ENOSYM;
    }
    /*如果非终结符号，则生成语法分析表的GOTO节点。*/
    if (symbol->type == SYMBOL_TYPE_NONTERMINAL) {
        GotoNode *gotoNode;
        GotoNode *conflictGotoNode;

        if (LrItemSetGotoNodeIsConflict(itemSet, mapNode->symId, &conflictGotoNode) == 1) {
            printf("conflict: goto node: item set %d <%d, %d> and <%d, %d>\n", itemSet->id,
                   conflictGotoNode->symId, conflictGotoNode->nextItemSetId,
                   mapNode->symId, mapNode->itemSetId);
            if (lalr->gotoNodeConflictHandle) {
                return lalr->gotoNodeConflictHandle(lalr, itemSet, conflictGotoNode,
                                                          mapNode->symId, mapNode->itemSetId);
            }
            printf("error: grammar symtax error\n");
            return -ESYNTAX;
        }

        gotoNode = LrAllocGotoNode(mapNode->symId, mapNode->itemSetId);
        if (!gotoNode) {
            return -ENOMEM;
        }
        list_add_tail(&gotoNode->node, &itemSet->gotoList);
    } else if (symbol->type == SYMBOL_TYPE_TERMINAL) {  /*如果是终结符号，则生成ACTION节点*/
        ActionNode *actionNode;

        /*如果不是输入结束符，生成SHIFT类型ACTION节点*/
        if (symbol->id != lalr->grammar->endSymId
                && symbol->id != lalr->grammar->emptySymId) {
            ActionNode *conflictActionNode;

            if (LrItemSetActionNodeIsConflict(itemSet, mapNode->symId, &conflictActionNode) == 1) {
                const char *conflictTypeStr;

                conflictTypeStr = conflictActionNode->type == LAT_SHIFT ? "shift" :
                               conflictActionNode->type == LAT_ACCEPT ? "accept" :
                               conflictActionNode->type == LAT_REDUCT ? "reduct" : "?";
                printf("conflict: action node, item set %d, %s, %s <%d, %d> and <%d, %d>\n", itemSet->id,
                       conflictTypeStr, "shift",
                       conflictActionNode->lookaheadId, conflictActionNode->itemId,
                       mapNode->symId, mapNode->itemSetId);

                if (lalr->actionNodeConflictHandle) {
                    return lalr->actionNodeConflictHandle(lalr, itemSet, conflictActionNode,
                                                              LAT_SHIFT, mapNode->symId, mapNode->itemSetId);
                }
                printf("error: grammar symtax error\n");
                return -ESYNTAX;
            }
            actionNode = LrAllocActionNode(LAT_SHIFT, mapNode->symId, mapNode->itemSetId);
        } else {    /*如果是输入结束符，则出现错误*/
            return -ESYNTAX;
        }
        if (!actionNode) {
            return -ENOMEM;
        }
        list_add_tail(&actionNode->node, &itemSet->actionList);
    } else {
        return -EMISC;
    }

    return error;
}

/*
 * 功能：根据GOTO(I, X)函数生成项集itemSet的语法分析表节点
 * grammar: 文法
 * itemSet: 项集
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int LrItemSetGenParseTableByMap(Lalr *lalr, ItemSet *itemSet)
{
    struct list_head *pos;
    MapNode *mapNode;
    int error = 0;

    list_for_each(pos, &itemSet->mapList) {
        mapNode = container_of(pos, MapNode, node);
        error = LrItemSetGenParseTableByMapNode(lalr, itemSet, mapNode);
        if (error != -ENOERR)
            return error;
    }
    return error;
}

/*
 * 功能：添加项集itemSet的归约ACTION节点，向前看符号是follow链表中的符号id。
 * itemSet：项集
 * item: 项
 * followList: FOLLOW集合
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int LrItemSetAddReductActionByList(Lalr *lalr, ItemSet *itemSet, Item *item, const struct list_head *followList)
{
    struct list_head *pos;
    SymId *symId;
    int error = 0;

    list_for_each(pos, followList) {
        ActionNode *actionNode;
        ActionNode *conflictActionNode;

        symId = container_of(pos, SymId, node);
        if (LrItemSetActionNodeIsConflict(itemSet, symId->id, &conflictActionNode) == 1) {
            const char *conflictType;

            conflictType = conflictActionNode->type == LAT_SHIFT ? "shift" :
                           conflictActionNode->type == LAT_ACCEPT ? "accept" :
                           conflictActionNode->type == LAT_REDUCT ? "reduct" : "?";
            printf("conflict: action node, item set %d, %s, %s <%d, %d> and <%d, %d>\n", itemSet->id,
                   conflictType, "reduct",
                   conflictActionNode->lookaheadId, conflictActionNode->itemId,
                   symId->id, item->id);

            if (lalr->actionNodeConflictHandle) {
                error = lalr->actionNodeConflictHandle(lalr, itemSet, conflictActionNode,
                                                          LAT_REDUCT, symId->id, item->id);
                if (error == -ENOERR)
                    continue;
                else
                    return error;
            }
            printf("error: grammar symtax error\n");
            return -ESYNTAX;
        }
        actionNode = LrAllocActionNode(LAT_REDUCT, symId->id, item->id);
        if (!actionNode) {
            return -ENOMEM;
        }
        list_add_tail(&actionNode->node, &itemSet->actionList);
    }
    return 0;
}

/*
 * 功能：判断项item是否为归约项。
 * grammar: 文法
 * item: 项
 * 返回值：如果项item是归约项则返回1，不是返回0，失败返回错误码。
 **/
static inline int LrItemIsReduct(const Lalr *lalr, const Item *item)
{
    const ProductBody *productBody;
    int endPosNum = 0;

    productBody = CfgGetProductBody(lalr->grammar, item->productRef.headSymId, item->productRef.bodyId);
    if (!productBody) {
        return -ENOPDTB;
    }
    if (CfgProductBodyIsEmpty(lalr->grammar, productBody) == 1) {
        endPosNum = 0;
    } else {
        struct list_head *pos;

        list_for_each(pos, &productBody->symIdList)
            endPosNum++;
    }

    if (item->pos == endPosNum)
        return 1;
    else
        return 0;
}

/*
 * 功能：根据规约项生成项集itemSet的语法分析表节点
 * grammar: 文法
 * itemSet: 项集
 * item: 项
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int LrItemSetGenParseTableByReductItem(Lalr *lalr, ItemSet *itemSet, Item *item)
{
    int error = 0;

    /*如果item是归约项，则把项的产生式头的FOLLOW集合中的符号id当作向前看
      符号，添加归约ACTION。*/
    if (LrItemIsReduct(lalr, item) == 1) {
        if (item->productRef.headSymId != lalr->grammar->startSymId) {
            /*根据向前看符号id集合中的符号id添加ACTION节点*/
            error = LrItemSetAddReductActionByList(lalr, itemSet, item, &item->followList);
            if (error != -ENOERR) {
                return error;
            }
        } else {
            ActionNode *actionNode;
            ActionNode *conflictActionNode;

            if (LrItemSetActionNodeIsConflict(itemSet, lalr->grammar->endSymId, &conflictActionNode) == 1) {
                const char *conflictType;

                conflictType = conflictActionNode->type == LAT_SHIFT ? "shift" :
                               conflictActionNode->type == LAT_ACCEPT ? "accept" :
                               conflictActionNode->type == LAT_REDUCT ? "reduct" : "?";
                printf("conflict: action node, item set %d, %s, %s <%d, %d> and <%d, %d>\n", itemSet->id,
                       conflictType, "accept",
                       conflictActionNode->lookaheadId, conflictActionNode->itemId,
                       lalr->grammar->endSymId, 0);

                if (lalr->actionNodeConflictHandle) {
                    return lalr->actionNodeConflictHandle(lalr, itemSet, conflictActionNode,
                                                              LAT_ACCEPT, lalr->grammar->endSymId, 0);
                }
                printf("error: grammar symtax error\n");
                return -ESYNTAX;
            }
            actionNode = LrAllocActionNode(LAT_ACCEPT, lalr->grammar->endSymId, 0);
            if (!actionNode) {
                return -ENOMEM;
            }
            list_add_tail(&actionNode->node, &itemSet->actionList);
        }
    }
    return error;
}

/*
 * 功能：遍历项集中的各项，根据规约项，生成项集itemSet的语法分析表节点
 * grammar: 文法
 * itemSet: 项集
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int LrItemSetGenParseTableByItemSet(Lalr *lalr, ItemSet *itemSet)
{
    struct list_head *pos;
    Item *item;
    int error = 0;

    list_for_each(pos, &itemSet->itemList) {
        item = container_of(pos, Item, node);
        error = LrItemSetGenParseTableByReductItem(lalr, itemSet, item);
        if (error != -ENOERR) {
            return error;
        }
    }
    return error;
}

/*
 * 功能：生成项集itemSet的语法分析表节点
 * grammar: 文法
 * itemSet: 项集
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int LrItemSetGenParseTable(Lalr *lalr, ItemSet *itemSet)
{
    int error = 0;

    error = LrItemSetGenParseTableByMap(lalr, itemSet);
    if (error != -ENOERR)
        return error;
    return LrItemSetGenParseTableByItemSet(lalr, itemSet);
}

/*
 * 功能：释放生成语法分析表过程中的垃圾。
 * itmeSet: 项集
 * 返回值：无
 **/
static void ItemSetFreeGPTGarbage(ItemSet *itemSet)
{
    struct list_head *pos;
    Item *item;

    list_for_each(pos, &itemSet->itemList) {
        item = container_of(pos, Item, node);
        LrItemFreeTransmitterItemList(&item->transmitterList);
    }
}

/*
 * 功能：释放生成语法分析表过程中的垃圾。
 * lalr: lalr
 * 返回值：无
 **/
static void LalrFreeGPTGarbage(Lalr *lalr)
{
    struct list_head *pos;
    ItemSet *itemSet;

    list_for_each(pos, &lalr->itemSetList) {
        itemSet = container_of(pos, ItemSet, node);
        ItemSetFreeGPTGarbage(itemSet);
    }
}

/*
 * 功能：生成LR语法分析表
 * grammar: 文法
 * 返回值：成功时返回0，否则返回错误码。
 **/
int LalrGenParseTable(Lalr *lalr)
{
    struct list_head *pos;
    ItemSet *itemSet;
    int error = 0;

    if (!lalr)
        return -EINVAL;

    list_for_each(pos, &lalr->itemSetList) {
        itemSet = container_of(pos, ItemSet, node);
        error = LrItemSetGenParseTable(lalr, itemSet);
        if (error != -ENOERR) {
            LalrFreeGPTGarbage(lalr);
            return error;
        }
    }
    LalrFreeGPTGarbage(lalr);
    return error;
}

/*
 * 功能：获取项集itemSet在向前看符号lookahead上的动作
 * itemSet: 项集
 * lookahead: 向前看符号
 * 返回值：项集itemSet在向前看符号lookahead上的动作节点
 **/
ActionNode *LalrItemSetGetAction(const ItemSet *itemSet, int lookahead)
{
    struct list_head *pos;
    ActionNode *actionNode;

    if (!itemSet)
        return NULL;

    list_for_each(pos, &itemSet->actionList) {
        actionNode = container_of(pos, ActionNode, node);
        if (actionNode->lookaheadId == lookahead)
            return actionNode;
    }
    return NULL;
}

/*
 * 功能：获取项集itemSet在文法符号symId上的GOTO节点
 * itemSet：项集
 * symId: 非终结符号id
 * 返回值：GOTO节点
 **/
GotoNode *LalrItemSetGetGoto(const ItemSet *itemSet, int symId)
{
    struct list_head *pos;
    GotoNode *gotoNode;

    if (!itemSet)
        return NULL;

    list_for_each(pos, &itemSet->gotoList) {
        gotoNode = container_of(pos, GotoNode, node);
        if (gotoNode->symId == symId)
            return gotoNode;
    }
    return NULL;
}

/*
 * 功能：获取项集itemSet中下一个符号是symId的项。
 *      这个函数只应用在特定场景下，有且只能有一个项符合要求，否则报错。
 * lalr: lalr
 * itemSet：项集
 * symId：下一个符号的id
 * pItem：输出型参数，项
 * 返回值：成功时返回0，否则返回错误码。
 **/
int LrItemSetGetItemByNextSymId(Lalr *lalr, const ItemSet *itemSet, int symId, Item **pItem)
{
    Item *iterItem;
    struct list_head *pos;
    char count = 0;

    if (!lalr || !itemSet || symId < 0 || !pItem)
        return -EINVAL;

    list_for_each(pos, &itemSet->itemList) {
        iterItem = container_of(pos, Item, node);
        if (LrItemNextSymId(lalr, iterItem) == symId) {
            *pItem = iterItem;
            count++;
        }
    }
    if (count == 1) {
        return 0;
    } else {
        return -EMISC;
    }
}

static const char *LrSymIdStr(int id, const void *arg)
{
    return CfgSymbolStr(CfgGetSymbol(arg, id));
}

static void LrItemPrintTransmitter(const Item *item)
{
    struct list_head *pos;
    FollowTransmitterItem *transmitter;

    list_for_each(pos, &item->transmitterList) {
        transmitter = container_of(pos, FollowTransmitterItem, node);
        printf("{%d, %d} ", transmitter->itemSetId, transmitter->itemId);
    }
}

static void LrItemPrint(const Lalr *lalr, const Item *item)
{
    Symbol *symbol;
    ProductBody *productBody;
    int u = 0;
    struct list_head *pos;
    SymId *symId;

    symbol = CfgGetSymbol(lalr->grammar, item->productRef.headSymId);
    if (!symbol) {

    }
    productBody = CfgSymbolGetProductBody(symbol, item->productRef.bodyId);
    if (!productBody) {

    }

    printf("------------------------------\n");
    printf("%d: ", item->id);
    printf("%s -> ", CfgSymbolStr(CfgGetSymbol(lalr->grammar, item->productRef.headSymId)));
    list_for_each(pos, &productBody->symIdList) {
        if (u == item->pos) {
            printf(".");
        }
        symId = container_of(pos, SymId, node);
        printf("%s ", CfgSymbolStr(CfgGetSymbol(lalr->grammar, symId->id)));
        u++;
    }
    if (u == item->pos) {
        printf(".");
    }
    printf(", [");
    CfgSymIdListPrint(&item->followList, LrSymIdStr, lalr->grammar);
    printf("]\n");
    if (list_empty(&item->transmitterList) == 0)
        printf("transmitter item(s): ");
    LrItemPrintTransmitter(item);
    printf("\n");
}

static void LrMapNodePrint(const Lalr *lalr, int itemSetId, MapNode *mapNode)
{
    printf("GOTO(%d, '%s') -> %d", itemSetId,
           CfgSymbolStr(CfgGetSymbol(lalr->grammar, mapNode->symId)),
           mapNode->itemSetId);
}

static void LrItemSetPrint(const Lalr *lalr, const ItemSet *itemSet)
{
    struct list_head *pos;
    Item *item;
    MapNode *mapNode;

    printf("\n>>>>>>>>>>>>>>>item set %d<<<<<<<<<<<<<<<<<\n", itemSet->id);
    list_for_each(pos, &itemSet->itemList) {
        item = container_of(pos, Item, node);
        LrItemPrint(lalr, item);
    }

    if (!list_empty(&itemSet->mapList)) {
        printf("-------------GOTO(%d, X)-----------------\n", itemSet->id);
    }
    list_for_each(pos, &itemSet->mapList) {
        mapNode = container_of(pos, MapNode, node);
        LrMapNodePrint(lalr, itemSet->id, mapNode);
        printf("\n");
    }
}

void LalrPrintItemSet(const Lalr *lalr)
{
    struct list_head *pos;
    ItemSet *itemSet;

    if (!lalr)
        return;
    list_for_each(pos, &lalr->itemSetList) {
        itemSet = container_of(pos, ItemSet, node);
        LrItemSetPrint(lalr, itemSet);
    }
}

static void LrActionNodePrint(const Lalr *lalr, const ActionNode *action)
{
    switch (action->type) {
    case LAT_SHIFT:
        printf("shift: %s, %d\n", CfgSymbolStr(CfgGetSymbol(lalr->grammar, action->lookaheadId)), action->itemSetId);
        break;
    case LAT_REDUCT:
        printf("reduct: %s, %d\n", CfgSymbolStr(CfgGetSymbol(lalr->grammar, action->lookaheadId)), action->itemId);
        break;
    case LAT_ACCEPT:
        //printf("accept: %s\n", CfgSymbolStr(CfgGetSymbol(lalr->grammar, action->lookaheadId)));
        printf("accept: %s, %d\n", CfgSymbolStr(CfgGetSymbol(lalr->grammar, action->lookaheadId)), action->itemId);
        break;
    default:
        printf("error\n");
        break;
    }
    printf("\n------------------------------------------\n");
}

static void LrItemSetPrintActionList(const Lalr *lalr, const ItemSet *item)
{
    struct list_head *pos;
    ActionNode *actionNode;

    list_for_each(pos, &item->actionList) {
        actionNode = container_of(pos, ActionNode, node);
        LrActionNodePrint(lalr, actionNode);
    }
}

static void LrGotoNodePrint(const Lalr *lalr, const GotoNode *gotoNode)
{
    printf("goto: %s, %d\n", CfgSymbolStr(CfgGetSymbol(lalr->grammar, gotoNode->symId)),
           gotoNode->nextItemSetId);
    printf("\n------------------------------------------\n");
}

static void LrItemSetPrintGotoList(const Lalr *lalr, const ItemSet *item)
{
    struct list_head *pos;
    GotoNode *gotoNode;

    list_for_each(pos, &item->gotoList) {
        gotoNode = container_of(pos, GotoNode, node);
        LrGotoNodePrint(lalr, gotoNode);
    }
}

static void LrItemSetPrintParseTable(const Lalr *lalr, const ItemSet *itemSet)
{
    printf("============ item set %d ACTION/GOTO =================\n", itemSet->id);
    LrItemSetPrintActionList(lalr, itemSet);
    LrItemSetPrintGotoList(lalr, itemSet);
}

void LalrPrintParseTable(const Lalr *lalr)
{
    struct list_head *pos;
    ItemSet *itemSet;

    list_for_each(pos, &lalr->itemSetList) {
        itemSet = container_of(pos, ItemSet, node);
        LrItemSetPrintParseTable(lalr, itemSet);
    }
}





