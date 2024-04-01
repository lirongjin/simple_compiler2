#include "error_recover.h"
#include "lalr_parse_table.h"
#include "lr_parse_algorithm.h"
#include "first_follow.h"
#include "list.h"
#include "cpl_common.h"
#include "cpl_debug.h"
#include "cpl_errno.h"
#include "stdlib.h"
#include "string.h"

static int CfgSymIdSetAddArr(struct list_head *list, int arr[], unsigned int len)
{
    int error = 0;
    unsigned int k;

    if (!list || !arr)
        return -EINVAL;

    for (k = 0; k < len; k++) {
        error = CfgSymIdSetAddId(list, arr[k]);
        if (error != -ENOERR)
            return error;
    }
    return error;
}


/*
 * 功能：获取项集中可用于错误恢复的GOTO节点
 * item：项集
 * list：错误恢复非终结符号链表
 * 返回值：成功时返回项集中可用于错误恢复的GOTO节点，否则返回NULL。
 **/
static GotoNode *LrErrorRecoverGetGoto(ItemSet *itemSet, struct list_head *list)
{
    struct list_head *pos;
    SymId *symId;
    GotoNode *gotoNode;

    list_for_each(pos, list) {
        symId = container_of(pos, SymId, node);
        gotoNode = LalrItemSetGetGoto(itemSet, symId->id);
        if (gotoNode)
            return gotoNode;
    }
    return NULL;
}

/*
 * 功能：弹出格局栈中的状态，直到找到一个可用于错误恢复非终结符号。
 * grammar: 文法
 * stack: 格局栈
 * list: 错误恢复非终结符号链表
 * 返回值：成功时返回错误恢复非终结符号，否则返回错误码。
 **/
static int LrErrorRecoverGetSymId(Lalr *lalr, struct list_head *stack, struct list_head *list)
{
    ItemSet *itemSet;
    int topStateId;
    GotoNode *gotoNode;
    int error = 0;

    while (1) {
        topStateId = ConfigurationStackTopState(stack, 0)->id;
        if (topStateId < 0) {
            return -ESYNTAX;
        }
        itemSet = LalrGetItemSet(lalr, topStateId);
        if (!itemSet)
            return -ENOSTATE;

        /*获取当前项集中可用于错误恢复的GOTO节点*/
        gotoNode = LrErrorRecoverGetGoto(itemSet, list);
        if (gotoNode) {
            printf("GOTO(%d, %s) -> %d\n", ConfigurationStackTopState(stack, 0)->id,
                   CfgSymbolStr(CfgGetSymbol(lalr->grammar, gotoNode->symId)),
                   gotoNode->nextItemSetId);
            /*把错误恢复非终结符号对应的状态压入格局栈*/
            ConfigurationStackPushState(stack, gotoNode->nextItemSetId, NULL);
            return gotoNode->symId;
        }
        printf("Pop up state: %d\n", itemSet->id);
        if (ConfigurationStackTopState(stack, 0)->id == 0)
            return -ESYNTAX;
        error = ConfigurationStackPopState(stack, 1);
        if (error != -ENOERR)
            return error;
    }

    return -ESYNTAX;
}

/*
 * 功能：恐慌模式错误恢复处理。
 * grammar：文法
 * itemSet：项集
 * stack：格局栈
 * input：输入
 * pIdx：输入索引
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int LrErrorRecoverHandle(LrErrorRecover *lrErrorRecover, Lalr *lalr,
                      LrParser *lrParser, LrReader *reader)
{
    int recoverSymId;
    int error = 0;
    struct list_head followList;
    struct list_head *stack;

    stack = &lrParser->configurationStack;
    INIT_LIST_HEAD(&followList);
    /*弹出格局栈中的状态，直到找到一个可用于错误恢复非终结符号。*/
    error = recoverSymId = LrErrorRecoverGetSymId(lalr, stack, &lrErrorRecover->recoverSymIdList);
    if (error < 0)
        return error;
    /*获取错误恢复非终结符号的FOLLOW集合*/
    error = CfgGetSymIdFollow(lalr->grammar, recoverSymId, &followList);
    if (error != -ENOERR) {
        CfgSymIdListFree(&followList);
        return error;
    }

    /*消耗输入中的符号，直到遇到FOLLOW集合中的符号。为避免死循环，至少消耗一个输入符号。*/
    while (1) {
        if (reader->getCurrentSymId(reader) == lalr->grammar->endSymId) {
            CfgSymIdListFree(&followList);
            return -ESYNTAX;
        }
        printf("Drop the letter '%s'\n", CfgSymbolStr(CfgGetSymbol(lalr->grammar, reader->getCurrentSymId(reader))));
        reader->inputPosInc(reader);
        if (CfgSymIdSetIsContain(&followList, reader->getCurrentSymId(reader)) == 1) {
            CfgSymIdListFree(&followList);
            return 0;
        }
    }
}

void LrErrorReocverFree(LrErrorRecover *lrErrorRecover)
{
    if (lrErrorRecover) {
        CfgSymIdListFree(&lrErrorRecover->recoverSymIdList);
    }
}

/*
 * 功能：
 * pLrErroeRecover:
 * recoverSymIdArr: 用于错误恢复的非终结符号id数组，元素通常是比较常用的非终结符号。
 * len: 数组长度。
 * 返回值：
 */
int LrErrorRecoverAlloc(LrErrorRecover **pLrErroeRecover, int recoverSymIdArr[], unsigned int len)
{
    int error = 0;
    LrErrorRecover *lrErrorRecover;

    if (!pLrErroeRecover || !recoverSymIdArr)
        return -EINVAL;

    *pLrErroeRecover = NULL;
    lrErrorRecover = malloc(sizeof (*lrErrorRecover));
    if (!lrErrorRecover)
        return -ENOMEM;
    *pLrErroeRecover = lrErrorRecover;

    INIT_LIST_HEAD(&lrErrorRecover->recoverSymIdList);
    error = CfgSymIdSetAddArr(&lrErrorRecover->recoverSymIdList, recoverSymIdArr, len);
    if (error != -ENOERR) {
        CfgSymIdListFree(&lrErrorRecover->recoverSymIdList);
        free(lrErrorRecover);
        return error;
    }
    lrErrorRecover->recoverHandle = LrErrorRecoverHandle;
    return 0;
}

