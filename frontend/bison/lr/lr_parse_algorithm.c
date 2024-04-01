#include "lr_parse_algorithm.h"
#include "context_free_grammar.h"
#include "error_recover.h"
#include "stdio.h"
#include "stdlib.h"
#include "cpl_debug.h"

#define PrErr(...)      Pr(__FILE__, __LINE__, __FUNCTION__, "error", __VA_ARGS__)

#define LPA_DEBUG
#ifdef LPA_DEBUG
#define PrDbg(...)      Pr(__FILE__, __LINE__, __FUNCTION__, "debug", __VA_ARGS__)
#else
#define PrDbg(...)
#endif

int ConfigurationStackPushState(struct list_head *list, int id, void *arg)
{
    StackStateNode *stateNode;

    if (!list)
        return -EINVAL;

    stateNode = malloc(sizeof (*stateNode));
    if (!stateNode)
        return -ENOMEM;
    stateNode->id = id;
    stateNode->arg = arg;
    list_add_tail(&stateNode->node, list);
    return 0;
}

StackStateNode *ConfigurationStackTopState(struct list_head *list, unsigned int idx)
{
    struct list_head *pos;
    StackStateNode *stateNode;

    if (!list)
        return NULL;

    list_for_each_prev(pos, list) {
        if (idx == 0) {
            stateNode = container_of(pos, StackStateNode, node);
            return stateNode;
        }
        idx--;
    }
    return NULL;
}

int ConfigurationStackPopState(struct list_head *list, int len)
{
    struct list_head *pos;
    StackStateNode *stateNode;

    for (pos = list->prev; pos != list && len > 0; ) {
        stateNode = container_of(pos, StackStateNode, node);
        pos = pos->prev;
        list_del(&stateNode->node);
        free(stateNode);
        len--;
    }
    if (len == 0)
        return 0;
    else
        return -ENOSYM;
}

int LrParserAlloc(LrParser **pParser, int (shiftHandle)(void **, LrReader *), LrErrorRecover *errorRecover)
{
    LrParser *parser;

    if (!pParser || !shiftHandle)
        return -EINVAL;

    *pParser = NULL;
    parser = malloc(sizeof (*parser));
    if (!parser)
        return -ENOMEM;
    *pParser = parser;
    parser->shiftHandle = shiftHandle;
    INIT_LIST_HEAD(&parser->configurationStack);
    parser->errorRecover = errorRecover;
    parser->syntaxErrorFlag = 0;
    return 0;
}

static int LrConfigurationStackGetArg(LrParser *lrParser, void *argArr[], unsigned int argArrLen)
{
    unsigned int i;
    StackStateNode *iterState;

    for (i = 0; i < argArrLen; i++) {
        iterState = ConfigurationStackTopState(&lrParser->configurationStack, i);
        if (!iterState)
            return -ENOSTATE;
        argArr[argArrLen - 1 - i] = iterState->arg;
    }
    return 0;
}

static void LrParserFreeStack(struct list_head *list)
{
    struct list_head *pos;
    StackStateNode *state;

    for (pos = list->next; pos != list; ) {
        state = container_of(pos, StackStateNode, node);
        pos = pos->next;
        list_del(&state->node);
        free(state);
    }
}

void LrParserFree(LrParser *parser)
{
    LrParserFreeStack(&parser->configurationStack);
    free(parser);
}

/*
 * 功能：LR语法分析算法
 * grammar: 文法
 * input: 输出数据
 * 返回值：成功时返回0，否则返回错误码。
 **/
int LrParse(LrParser *lrParser, Lalr *lalr, LrReader *reader)
{
    int error = 0;
    StackStateNode *topState;
    ContextFreeGrammar *grammar;

    if (!lrParser || !lalr || !reader)
        return -EINVAL;

    grammar = lalr->grammar;
    /*向格局栈压入起始状态0。*/
    ConfigurationStackPushState(&lrParser->configurationStack, 0, NULL);
    while (1) {
        ProductBody *productBody;
        ItemSet *itemSet;
        ActionNode *action;
        int lookahead;

        /*获取格局栈栈顶状态和对应的项集*/
        topState = ConfigurationStackTopState(&lrParser->configurationStack, 0);
        if (!topState) {
            return -ENOSTATE;
        }
        itemSet = LalrGetItemSet(lalr, topState->id);
        if (!itemSet) {
            printf("error: no item set, item set: %d, la: %d\n", itemSet->id, topState->id);
            return -ENOSTATE;
        }
        /*获取项集在向前看符号上的动作。*/
        error = lookahead = reader->getCurrentSymId(reader);
        if (error < 0)
            return error;
        action = LalrItemSetGetAction(itemSet, lookahead);
        if (action == NULL) {   /*未找到合适的动作，该报错，输入不符合LR文法。*/
            lrParser->syntaxErrorFlag = 1;
            if (lrParser->errorRecover) {
                error = lrParser->errorRecover->recoverHandle(lrParser->errorRecover, lalr, lrParser,
                                                       reader);
                if (error == -ENOERR) {
                    continue;
                } else {
                    return error;
                }
            } else {
                printf("error: no action node\n");
                return -ESYNTAX;
            }
        }
        if (action->type == LAT_SHIFT) {    /*移入。*/
            void *arg;

            error = lrParser->shiftHandle(&arg, reader);
            if (error != -ENOERR)
                return error;
            //printf("shift to %d\n", action->itemSetId);
            /*格局栈压入下一个状态*/
            error = ConfigurationStackPushState(&lrParser->configurationStack, action->itemSetId, arg);
            if (error != -ENOERR) {
                return error;
            }
            error = reader->inputPosInc(reader);
            if (error != -ENOERR)
                return error;
        } else if (action->type == LAT_REDUCT) {    /*归约*/
            Item *item;
            GotoNode *gotoNode;
            void *headArg = NULL;

            /*获取归约项。*/
            item = LalrItemSetGetItem(itemSet, action->itemId);
            if (!item) {
                printf("reduct error: no item. item set: %d, action->itemId: %d\n", itemSet->id, action->itemId);
                return -ENOITEM;
            }
            /*找到归约项对应的产生式*/
            productBody = CfgGetProductBody(grammar, item->productRef.headSymId, item->productRef.bodyId);
            if (!productBody) {
                printf("error: no product body\n");
                return -ENOPDTB;
            }
            //CfgProductBodyPrint(grammar, CfgGetSymbol(grammar, item->productRef.headSymId), productBody);
            /*弹出格局栈中归约项的产生式体。*/
            if (productBody->handle) {
                if (CfgProductBodyIsEmpty(grammar, productBody) == 0) {
                    void *argArr[item->pos+1];

                    error = LrConfigurationStackGetArg(lrParser, argArr, ARRAY_SIZE(argArr));
                    if (error != -ENOERR)
                        return error;
                    error = productBody->handle(&headArg, argArr[0], &argArr[1]);
                } else {
                    if (productBody->handleType == RHT_SELF) {
                        void *argArr[1];

                        error = LrConfigurationStackGetArg(lrParser, argArr, ARRAY_SIZE(argArr));
                        if (error != -ENOERR)
                            return error;
                        error = productBody->handle(&headArg, argArr[0], NULL);
                    } else if (productBody->handleType == RHT_PARENT) {
                        StackStateNode *lastState;
                        Item *tempItem = NULL;

                        lastState = ConfigurationStackTopState(&lrParser->configurationStack, item->pos);
                        if (!lastState) {
                            return -ENOSTATE;
                        }
                        error = LrItemSetGetItemByNextSymId(lalr, LalrGetItemSet(lalr, lastState->id),
                                                            item->productRef.headSymId, &tempItem);
                        if (error != -ENOERR)
                            return error;

                        void *argArr[tempItem->pos+1];

                        error = LrConfigurationStackGetArg(lrParser, argArr, ARRAY_SIZE(argArr));
                        if (error != -ENOERR)
                            return error;
                        error = productBody->handle(&headArg, argArr[0], &argArr[1]);
                    }
                }
                if (error != -ENOERR)
                    return error;
            }
            error = ConfigurationStackPopState(&lrParser->configurationStack, item->pos);
            if (error != -ENOERR)
                return error;
            /*获取格局栈栈顶状态在归约项产生式头上的下一个状态。*/
            topState = ConfigurationStackTopState(&lrParser->configurationStack, 0);
            if (!topState) {
                return -ENOSTATE;
            }
            gotoNode = LalrItemSetGetGoto(LalrGetItemSet(lalr, topState->id),
                                         item->productRef.headSymId);
            if (!gotoNode) {
                printf("error: no goto node\n");
                return -ESYNTAX;
            }
            /*格局栈压入下一个状态*/
            error = ConfigurationStackPushState(&lrParser->configurationStack, gotoNode->nextItemSetId, headArg);
            if (error != -ENOERR)
                return error;
        } else if (action->type == LAT_ACCEPT) {    /*接受*/
            Item *item;
            void *headArg = NULL;

            item = LalrItemSetGetItem(itemSet, action->itemId);
            if (!item) {
                printf("accept error: no item. item set: %d, action->itemId: %d\n", itemSet->id, action->itemId);
                return -ENOITEM;
            }
            productBody = CfgGetProductBody(grammar, item->productRef.headSymId, item->productRef.bodyId);
            if (!productBody) {
                printf("error: no product body\n");
                return -ENOPDTB;
            }
            if (productBody->handle) {
                void *argArr[item->pos+1];

                error = LrConfigurationStackGetArg(lrParser, argArr, ARRAY_SIZE(argArr));
                if (error != -ENOERR)
                    return error;
                error = productBody->handle(&headArg, argArr[0], &argArr[1]);
            }
            printf("accept\n");
            break;
        }
    }

    return 0;
}

