#include "first_follow.h"

#include "cpl_errno.h"
#include "cpl_debug.h"
#include "list.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"

#define PrErr(...)          Pr(__FILE__, __LINE__, __FUNCTION__, "error", __VA_ARGS__)

//#define DEBUG
#ifdef DEBUG
#define PrDbg(...)          Pr(__FILE__, __LINE__, __FUNCTION__, "debug", __VA_ARGS__)
#endif


static int CfgSymbolGenFirst(ContextFreeGrammar *grammar, Symbol *symbol);

/*
 * 功能：获取文法符号的FIRST集合，如果非终结符号FIRST集合为空会尝试生成FIRST集合。
 * grammar: 文法
 * symbol: 文法符号
 * firstSymbolList：输出型参数，FIRST集合
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int CfgGetSymbolFirstSet(ContextFreeGrammar *grammar, Symbol *symbol, struct list_head *firstSymbolList)
{
    int error = 0;

    if (symbol->id == grammar->emptySymId) {
        return CfgSymIdSetAddId(firstSymbolList, grammar->emptySymId);
    }

    /*终结符号和空符号的FIRST集合是符合本身。*/
    if (symbol->type == SYMBOL_TYPE_TERMINAL
            || symbol->type == SYMBOL_TYPE_EMPTY) {
        return CfgSymIdSetAddId(firstSymbolList, symbol->id);
    } else if (symbol->type == SYMBOL_TYPE_NONTERMINAL) {
        /*如果非终结符的FIRST集合为空则生成非终结符的FIRST集合。*/
        if (list_empty(firstSymbolList)) {
            error = CfgSymbolGenFirst(grammar, symbol);
            if (error != -ENOERR)
                return error;
        }
        return CfgSymIdSetAddList(firstSymbolList, &symbol->firstSymIdList);
    } else {
        PrErr("ESYMTYPE");
        return -ESYMTYPE;
    }
}

/*
 * 功能：生成产生式体的FIRST集合
 * grammar: 文法
 * productBody：产生式体指针。
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int CfgProductBodyGenFirst(ContextFreeGrammar *grammar, ProductBody *productBody)
{
    struct list_head *pos;
    SymId *symId;
    Symbol *symbol;
    char emptyIdFlag = 1;
    int error = 0;

    if (list_empty(&productBody->symIdList)) {
        PrErr("ENONBODY");
        return -ENONBODY;
    }

    /*从产生式体第一个符号开始遍历，计算产生式体的FIRST集合*/
    list_for_each(pos, &productBody->symIdList) {
        struct list_head firstSymIdList;

        /*获取产生式体中被遍历的符号的FIRST集合。*/
        INIT_LIST_HEAD(&firstSymIdList);
        symId = container_of(pos, SymId, node);
        symbol = CfgGetSymbol(grammar, symId->id);
        if (!symbol) {
            PrErr("ENOSYM");
            return -ENOSYM;
        }

        error = CfgGetSymbolFirstSet(grammar, symbol, &firstSymIdList);
        if (error != -ENOERR)
            return error;
        /*把被遍历产生式体符号的FIRST集合中的非空符号拷贝到产生式体的FIRST集合中。*/
        error = CfgSymIdSetAddListExcludeId(&productBody->firstSymIdList, &firstSymIdList, grammar->emptySymId);
        if (error != -ENOERR)
            return error;
        /*如果被遍历产生式体符号的FIRST集合中不包含空符号则跳出循环。*/
        if (!list_empty(&firstSymIdList)
                && (0 == CfgSymIdSetIsContain(&firstSymIdList, grammar->emptySymId))) {
            emptyIdFlag = 0;
            CfgSymIdListFree(&firstSymIdList);
            break;
        }
        CfgSymIdListFree(&firstSymIdList);
    }

    /*如果产生式体的各个非终结符号的FIRST集合中都包含空符号，则把空符号添加到产生式体的FIRST集合中。*/
    if (emptyIdFlag == 1) {
        error = CfgSymIdSetAddId(&productBody->firstSymIdList, grammar->emptySymId);
        if (error != -ENOERR)
            return error;
    }

    return error;
}

/*
 * 功能：生成文法符号的FIRST集合
 * grammar: 文法
 * symbol: 文法符号
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int CfgSymbolGenFirst(ContextFreeGrammar *grammar, Symbol *symbol)
{
    struct list_head *pos;
    ProductBody *productBody;
    int error = 0;

    /*为了避免同一个符号多次调用生成函数，先进行判断*/
    if (1 == CfgSymIdSetIsContain(&grammar->genFirstSymIdList, symbol->id)) {
        return 0;
    }
    error = CfgSymIdListAddId(&grammar->genFirstSymIdList, symbol->id);
    if (error != -ENOERR)
        return error;

    /*遍历各个产生式体，生成各产生式体的FIRST集合，并添加到当前符号的FIRST集合中。*/
    list_for_each(pos, &symbol->bodyList) {
        productBody = container_of(pos, ProductBody, node);
        error = CfgProductBodyGenFirst(grammar, productBody);
        if (error != -ENOERR)
            return error;
        error = CfgSymIdSetAddList(&symbol->firstSymIdList, &productBody->firstSymIdList);
        if (error != -ENOERR)
            return error;
    }

    if (0 == CfgSymIdListDelId(&grammar->genFirstSymIdList, symbol->id)) {
        PrErr("delete id error");
        return -EMISC;
    }

    return error;
}

/*
 * 功能：生成文法的FIRST集合
 * grammar: 文法
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int CfgGenerateFirst(ContextFreeGrammar *grammar)
{
    struct list_head *pos;
    Symbol *symbol;
    int error = 0;

    if (!grammar)
        return -EINVAL;

    /*遍历文法中的各个非终结符号，并生成FIRST集合*/
    list_for_each(pos, &grammar->symbolList) {
        symbol = container_of(pos, Symbol, node);
        if (symbol->type == SYMBOL_TYPE_NONTERMINAL
                && list_empty(&symbol->firstSymIdList)) {
            error = CfgSymbolGenFirst(grammar, symbol);
            if (error != -ENOERR)
                return error;
        }
    }

    return 0;
}

static int CfgSymIdGenFollowByBetaList(ContextFreeGrammar *grammar, int symId, struct list_head *betaList)
{
    Symbol *symbol;
    struct list_head nextSymFirstSet;
    int error;

    /*获取相邻下一个符号的FIRST集合。*/
    INIT_LIST_HEAD(&nextSymFirstSet);
    symbol = CfgGetSymbol(grammar, symId);
    if (!symbol) {
        PrErr("ENOSYM");
        return -ENOSYM;
    }
    error = CfgGetSymIdListFirst(grammar, betaList, &nextSymFirstSet);
    if (error != -ENOERR)
        return error;

    /*把相邻下一个符号的FIRST集合添加到当前符号的FOLLOW集合中。*/
    error = CfgSymIdSetAddListExcludeId(&symbol->followSymIdList, &nextSymFirstSet, grammar->emptySymId);
    CfgSymIdListFree(&nextSymFirstSet);
    return error;
}

/*
 * 功能：生成产生式体中各个符号的FOLLOW集合。
 * grammar: 文法
 * productBody：产生式体。
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int CfgProductBodyGenFollowByProduct(ContextFreeGrammar *grammar, ProductBody *productBody)
{
    struct list_head *pos;
    struct list_head betaList;  /*待处理符号后面的文法符号串。*/
    SymId *symId;
    int error = 0;

    INIT_LIST_HEAD(&betaList);
    /*遍历产生式体中除尾符号外的各个符号，并生成各自的FOLLOW集合。*/
    list_for_each(pos, &productBody->symIdList) {
        if (list_get_tail(&productBody->symIdList) == pos) {
            break;
        }
        symId = container_of(pos, SymId, node);
        error = CfgSymIdListCopyFromPos(&betaList, &productBody->symIdList, pos->next);
        if (error != -ENOERR)
            return error;
        error = CfgSymIdGenFollowByBetaList(grammar, symId->id, &betaList);
        CfgSymIdListFree(&betaList);
        if (error != -ENOERR)
            return error;
    }

    return 0;
}

/*
 * 功能：根据符号的产生式体生成符号的FOLLOW集合
 * grammar: 文法
 * symbol: 文法符号
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int CfgSymbolGenFollowByProduct(ContextFreeGrammar *grammar, Symbol *symbol)
{
    struct list_head *pos;
    ProductBody *productBody;
    int error = 0;

    /*遍历文法符号的各个产生式体，生成相关符号的FOLLOW集合。*/
    list_for_each(pos, &symbol->bodyList) {
        productBody = container_of(pos, ProductBody, node);
        error = CfgProductBodyGenFollowByProduct(grammar, productBody);
        if (error != -ENOERR)
            return error;
    }

    return 0;
}

/*
 * 功能：根据产生式体生成文法的FOLLOW集合。
 * grammar: 文法。
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int CfgGenFollowByProduct(ContextFreeGrammar *grammar)
{
    struct list_head *pos;
    Symbol *symbol;
    int error = 0;

    list_for_each(pos, &grammar->symbolList) {
        symbol = container_of(pos, Symbol, node);
        error = CfgSymbolGenFollowByProduct(grammar, symbol);
        if (error != -ENOERR)
            return error;
    }

    return 0;
}

/*
 * 功能：根据产生式体生成非终结符号的尾非终结符号id链表。
 * grammar: 文法
 * symbol: 文法符号
 * productBody: 产生式体。
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int CfgProductBodyGenTailSymIdList(ContextFreeGrammar *grammar, Symbol *symbol,
                                   ProductBody *productBody)
{
    struct list_head *pos;
    SymId *symId;
    Symbol *iterSymbol;
    int error = 0;

    /*从产生式体的尾符号开始遍历。*/
    list_for_each_prev(pos, &productBody->symIdList) {
        symId = container_of(pos, SymId, node);
        iterSymbol = CfgGetSymbol(grammar, symId->id);
        if (!iterSymbol) {
            PrErr("ENOSYM");
            return -ENOSYM;
        }
        if (iterSymbol->type == SYMBOL_TYPE_NONTERMINAL) {
            error = CfgSymIdSetAddId(&symbol->tailSymIdList, symId->id);
            if (error != -ENOERR)
                return error;
            /*如果当前非终结符号不能推导出空符号，则跳出循环。*/
            if (0 == CfgSymIdSetIsContain(&iterSymbol->firstSymIdList, grammar->emptySymId)) {
                break;
            }
        } else {
            if (iterSymbol->id != grammar->emptySymId)
                break;
        }
    }

    return 0;
}

/*
 * 功能：生成符号的产生式尾符号链表
 * grammar: 文法
 * symbol: 文法符号
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int CfgSymbolGenTailSymIdList(ContextFreeGrammar *grammar, Symbol *symbol)
{
    struct list_head *pos;
    ProductBody *productBody;
    int error = 0;

    list_for_each(pos, &symbol->bodyList) {
        productBody = container_of(pos, ProductBody, node);
        error = CfgProductBodyGenTailSymIdList(grammar, symbol, productBody);
        if (error != -ENOERR)
            return error;
    }

    return 0;
}

/*
 * 功能：生成文法中各个非终结符号的尾符号链表。
 * grammar: 文法。
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int CfgGenTailSymIdList(ContextFreeGrammar *grammar)
{
    struct list_head *pos;
    Symbol *symbol;
    int error = 0;

    list_for_each(pos, &grammar->symbolList) {
        symbol = container_of(pos, Symbol, node);
        error = CfgSymbolGenTailSymIdList(grammar, symbol);
        if (error != -ENOERR)
            return error;
    }

    return 0;
}

/*
 * 功能：把符号FOLLOW集合中的符号添加到尾符号链表中各符号的FOLLOW集合中去。
 * grammar: 文法
 * symbol: 文法符号。
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int CfgSymbolGenTailSymFollow(ContextFreeGrammar *grammar, Symbol *symbol)
{
    char updateFlag = 0;
    struct list_head *pos;
    SymId *symId;
    Symbol *iterSymbol;
    int error = 0;

    list_for_each(pos, &symbol->tailSymIdList) {
        symId = container_of(pos, SymId, node);
        iterSymbol = CfgGetSymbol(grammar, symId->id);
        if (!iterSymbol) {
            PrErr("ENOSYM");
            return -ENOSYM;
        }
        error = CfgSymIdSetAddListUpdateFlag(&iterSymbol->followSymIdList, &symbol->followSymIdList, grammar->emptySymId,
                                     &updateFlag);
        if (error != -ENOERR)
            return error;
        if (updateFlag == 1) {
            error = CfgSymbolGenTailSymFollow(grammar, iterSymbol);
            if (error != -ENOERR)
                return error;
        }
    }

    return 0;
}

/*
 * 功能：根据非终结符的FOLLOW集合生成它的产生式体末尾非终结符的FOLLOW集合
 * grammar: 文法
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int CfgGenTailSymFollow(ContextFreeGrammar *grammar)
{
    struct list_head *pos;
    Symbol *symbol;
    int error = 0;

    /*生成文法中各非终结符的产生式体的末尾非终结符链表。*/
    error = CfgGenTailSymIdList(grammar);
    if (error != -ENOERR)
        return error;

    /*根据非终结符的FOLLOW集合生成它的产生式体末尾非终结符的FOLLOW集合*/
    list_for_each(pos, &grammar->symbolList) {
        symbol = container_of(pos, Symbol, node);
        if (symbol->type == SYMBOL_TYPE_NONTERMINAL) {
            error = CfgSymbolGenTailSymFollow(grammar, symbol);
            if (error != -ENOERR)
                return error;
        }
    }

    return 0;
}

/*
 * 功能：生成文法的FOLLOW集合
 * grammar: 文法
 * 返回值：成功时返回0，否则返回错误码。
 **/
static int CfgGenerateFollow(ContextFreeGrammar *grammar)
{
    Symbol *startSymbol;
    int error = 0;

    /*把结束符$添加到开始符号的FOLLOW集合*/
    startSymbol = CfgGetSymbol(grammar, grammar->startSymId);
    if (!startSymbol) {
        PrErr("ENOSYM");
        return -ENOSYM;
    }
    error = CfgSymIdSetAddId(&startSymbol->followSymIdList, grammar->endSymId);
    if (error != -ENOERR)
        return error;
    /*根据产生式体生成FOLLOW集合*/
    error = CfgGenFollowByProduct(grammar);
    if (error != -ENOERR)
        return error;
    /*产生式体的末尾非终结符继承产生式头的FOLLOW集合*/
    error = CfgGenTailSymFollow(grammar);
    if (error != -ENOERR)
        return error;

    return error;
}

/*
 * 功能：获取文法符号的symId的FIRST集合
 * grammar：文法
 * symId：id
 * list：FIRST集合
 * 返回值：成功时返回0，否则返回错误码。
 **/
int CfgGetSymIdFirst(ContextFreeGrammar *grammar, int symId, struct list_head *list)
{
    Symbol *symbol;
    int error = 0;

    if (!grammar || !list)
        return -EINVAL;
    symbol = CfgGetSymbol(grammar, symId);
    if (!symbol) {
        PrErr("ENOSYM: %d", symId);
        return -ENOSYM;
    }
    /*终结符合和空符号的FIRST集合是其自身*/
    if (symbol->type == SYMBOL_TYPE_TERMINAL
            || symbol->type == SYMBOL_TYPE_EMPTY) {
        error = CfgSymIdSetAddId(list, symId);
        if (error != -ENOERR)
            return error;
    } else if (symbol->type == SYMBOL_TYPE_NONTERMINAL) {
        error = CfgSymIdSetAddList(list, &symbol->firstSymIdList);
        if (error != -ENOERR)
            return error;
    } else {
        return -EINVAL;
    }

    return 0;
}

/*
 * 功能：获取一串文法符号的FIRST的集合
 * grammar：文法
 * symIdList：符号id链表
 * firstSet：FIRST集合
 * 返回值：成功时返回0，否则返回错误码。
 **/
int CfgGetSymIdListFirst(ContextFreeGrammar *grammar, const struct list_head *symIdList, struct list_head *firstSet)
{
    struct list_head tempFirstSet; /*临时FIRST集合*/
    char emptyFlag = 1;     /*FIRST集合包含空符号标志*/
    struct list_head *pos;
    SymId *symId;
    int error = 0;

    /*获取第1个符号的FIRST集合，把所有符号（空符号除外）添加到firstSet中，如果包含空符号
        则继续计算第2个符号的FIRST集合否则处理完毕，以此类推。如果所有符号的FIRST集合都
        包含空符号，则把空符号添加到最终的firstSet集合中。*/
    INIT_LIST_HEAD(&tempFirstSet);
    list_for_each(pos, symIdList) {
        symId = container_of(pos, SymId, node);
        error = CfgGetSymIdFirst(grammar, symId->id, &tempFirstSet);
        if (error != -ENOERR) {
            CfgSymIdListFree(&tempFirstSet);
            CfgSymIdListFree(firstSet);
            return error;
        }
        error = CfgSymIdSetAddListExcludeId(firstSet, &tempFirstSet, grammar->emptySymId);
        if (error != -ENOERR) {
            CfgSymIdListFree(&tempFirstSet);
            CfgSymIdListFree(firstSet);
            return error;
        }
        if (CfgSymIdSetIsContain(&tempFirstSet, grammar->emptySymId) == 0) {
            emptyFlag = 0;
            CfgSymIdListFree(&tempFirstSet);
            break;
        }
        CfgSymIdListFree(&tempFirstSet);
    }
    if (emptyFlag == 1) {
        error = CfgSymIdSetAddId(firstSet, grammar->emptySymId);
        if (error != -ENOERR) {
            CfgSymIdListFree(firstSet);
            return error;
        }
    }
    return error;
}

/*
 * 功能：获取非终结符的FOLLOW集合
 * grammar：文法
 * symId：符号id
 * list：FOLLOW集合
 * 返回值：成功时返回0，否则返回错误码。
 **/
int CfgGetSymIdFollow(ContextFreeGrammar *grammar, int symId, struct list_head *list)
{
    Symbol *symbol;

    if (!grammar || !list)
        return -EINVAL;

    symbol = CfgGetSymbol(grammar, symId);
    if (!symbol) {
        PrErr("ENOSYM");
        return -ENOSYM;
    }
    if (symbol->type != SYMBOL_TYPE_NONTERMINAL)
        return -EINVAL;
    return CfgSymIdSetAddList(list, &symbol->followSymIdList);
}

/*
 * 功能：生成文法的FIRST和FOLLOW集合
 * 返回值：成功时返回0，否则返回错误码。
 **/
int CfgGenFirstFollow(ContextFreeGrammar *grammar)
{
    int error;

    if (!grammar)
        return -EINVAL;

    error = CfgGenerateFirst(grammar);
    if (error != -ENOERR)
        return error;
    return CfgGenerateFollow(grammar);
}

/*
 * 功能：打印非终结符号的FIRST集合
 * grammar: 文法
 * symbol: 非终结符号
 * 返回值：无
 **/
static void CfgSymbolPrintFirst(ContextFreeGrammar *grammar, Symbol *symbol)
{
    struct list_head *pos;
    SymId *symId;

    if (symbol->type != SYMBOL_TYPE_NONTERMINAL)
        return;

    printf("-----------%s FIRST-------------\n", CfgSymbolStr(symbol));
    list_for_each(pos, &symbol->firstSymIdList) {
        symId = container_of(pos, SymId, node);
        printf("%s ", CfgSymbolStr(CfgGetSymbol(grammar, symId->id)));
    }
    printf("\n");
}

/*
 * 功能：打印非终结符的FOLLOW集合
 * grammar: 文法
 * symbol: 非终结符
 * 返回值：无
 **/
static void CfgSymbolPrintFollow(ContextFreeGrammar *grammar, Symbol *symbol)
{
    struct list_head *pos;
    SymId *symId;

    if (symbol->type != SYMBOL_TYPE_NONTERMINAL)
        return;

    printf("-----------%s FOLLOW-------------\n", CfgSymbolStr(symbol));

    list_for_each(pos, &symbol->followSymIdList) {
        symId = container_of(pos, SymId, node);
        printf("%s ", CfgSymbolStr(CfgGetSymbol(grammar, symId->id)));
    }
    printf("\n");
}

/*
 * 功能：打印文法的FIRST集合
 * grammar: 文法
 * 返回值：无
 **/
static void CfgPrintFirst(ContextFreeGrammar *grammar)
{
    struct list_head *pos;
    Symbol *symbol;

    if (!grammar)
        return;

    list_for_each(pos, &grammar->symbolList) {
        symbol = container_of(pos, Symbol, node);
        CfgSymbolPrintFirst(grammar, symbol);
    }
}

/*
 * 功能：打印文法的FOLLOW集合
 * grammar: 文法
 * 返回值：无。
 **/
static void CfgPrintFollow(ContextFreeGrammar *grammar)
{
    struct list_head *pos;
    Symbol *symbol;

    if (!grammar)
        return;

    list_for_each(pos, &grammar->symbolList) {
        symbol = container_of(pos, Symbol, node);
        CfgSymbolPrintFollow(grammar, symbol);
    }
}

/*
 * 功能：打印文法的FIRST和FOLLOW集合
 * grammar: 文法
 * 返回值：无。
 **/
void CfgPrintFirstFollow(ContextFreeGrammar *grammar)
{
    CfgPrintFirst(grammar);
    CfgPrintFollow(grammar);
}



