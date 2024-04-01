#include "context_free_grammar.h"
#include "lalr_parse_table.h"
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

/*
 * 功能：申请产生式体资源
 * 返回值：成功时返回产生式体指针，否则返回NULL。
 **/
static ProductBody *CfgAllocProductBody(void)
{
    ProductBody *productBody;

    productBody = malloc(sizeof (*productBody));
    if (productBody) {
        INIT_LIST_HEAD(&productBody->firstSymIdList);
        INIT_LIST_HEAD(&productBody->symIdList);
        productBody->handle = NULL;
        productBody->handleType = RHT_SELF;
    }
    return productBody;
}

/*
 * 功能：释放产生式体资源
 * productBody: 产生式体指针。
 * 返回值：无。
 **/
static void CfgFreeProductBody(ProductBody *productBody)
{
    CfgSymIdListFree(&productBody->firstSymIdList);
    CfgSymIdListFree(&productBody->symIdList);
    free(productBody);
}

/*
 * 功能：释放产生式体链表资源
 * list：产生式体链表
 * 返回值：无。
 **/
static void CfgProductBodyListFree(struct list_head *list)
{
    struct list_head *pos;
    ProductBody *productBody;

    for (pos = list->next; pos != list; ) {
        productBody = container_of(pos, ProductBody, node);
        pos = pos->next;
        list_del(&productBody->node);
        CfgFreeProductBody(productBody);
    }
}

/*
 * 功能：申请文法符号资源
 * id: 符号id
 * str：文法符号的字符串
 * type：文法符号的类型，
 *         SYMBOL_TYPE_TERMINAL: 终结符号
 *         SYMBOL_TYPE_NONTERMINAL: 非终结符号
 *         SYMBOL_TYPE_EMPTY：空字符串符号
 * 返回值：成功时返回文法符号指针，否则返回NULL。
 **/
static Symbol *CfgAllocSymbol(int id, const char *str, char type)
{
    Symbol *symbol;

    symbol = malloc(sizeof (*symbol));
    if (symbol) {
        symbol->id = id;
        symbol->str = malloc(strlen(str) + 1);
        if (!symbol->str) {
            free(symbol);
            return NULL;
        }
        strcpy((char *)symbol->str, str);
        symbol->type = type;
        symbol->bodyNum = 0;
        INIT_LIST_HEAD(&symbol->bodyList);
        INIT_LIST_HEAD(&symbol->firstSymIdList);
        INIT_LIST_HEAD(&symbol->followSymIdList);
        INIT_LIST_HEAD(&symbol->tailSymIdList);
    }
    return symbol;
}

/*
 * 功能：释放文法符号资源
 * symbol: 文法符号指针
 * 返回值：无。
 **/
static void CfgFreeSymbol(Symbol *symbol)
{
    CfgProductBodyListFree(&symbol->bodyList);
    CfgSymIdListFree(&symbol->firstSymIdList);
    CfgSymIdListFree(&symbol->followSymIdList);
    CfgSymIdListFree(&symbol->tailSymIdList);
    free((char *)symbol->str);
    free(symbol);
}

/*
 * 功能：给文法添加产生式
 * grammar: 文法
 * product：产生式，第一个符号是产生式头，其它符号是产生式体。
 * len：产生式长度。
 * 返回值：成功时返回0，否则返回错误码。
 **/
int CfgAddProduct(ContextFreeGrammar *grammar, int product[], unsigned int len)
{
    ProductBody *productBody;
    Symbol *headSymbol;
    unsigned int w;
    int error = 0;

    if (!grammar || !product || len < 2)
        return -EINVAL;

    productBody = CfgAllocProductBody();
    if (!productBody) {
        PrErr("ENOMEM");
        return -ENOMEM;
    }
    headSymbol = CfgGetSymbol(grammar, product[0]);
    if (!headSymbol) {
        PrErr("ENOSYM");
        return -ENOSYM;
    }
    list_add_tail(&productBody->node, &headSymbol->bodyList);

    for (w = 1; w < len; w++) {
        error = CfgSymIdListTailAddId(&productBody->symIdList, product[w]);
        if (error != -ENOERR)
            return error;
    }
    productBody->id = headSymbol->bodyNum++;
    productBody->handle = NULL;
    return error;
}

int CfgAddProductH(ContextFreeGrammar *grammar, ReduceHandleType handleType, ProductBodyHandle *handle, int product[], unsigned int len)
{
    ProductBody *productBody;
    Symbol *headSymbol;
    unsigned int w;
    int error = 0;

    if (!grammar || !product || len < 2)
        return -EINVAL;

    productBody = CfgAllocProductBody();
    if (!productBody) {
        PrErr("ENOMEM");
        return -ENOMEM;
    }
    headSymbol = CfgGetSymbol(grammar, product[0]);
    if (!headSymbol) {
        PrErr("ENOSYM, sym id: %d", product[0]);
        return -ENOSYM;
    }
    list_add_tail(&productBody->node, &headSymbol->bodyList);

    for (w = 1; w < len; w++) {
        error = CfgSymIdListTailAddId(&productBody->symIdList, product[w]);
        if (error != -ENOERR)
            return error;
    }
    productBody->id = headSymbol->bodyNum++;
    productBody->handle = handle;
    productBody->handleType = handleType;
    return error;
}


/*
 * 功能：向文法添加符号
 * grammar: 文法
 * id：符号id
 * str: 符号id字符串
 * type: 符号类型
 *          SYMBOL_TYPE_TERMINAL: 终结符号
 *          SYMBOL_TYPE_NONTERMINAL: 非终结符号
 *          SYMBOL_TYPE_EMPTY：空字符串符号
 * 返回值：成功时返回0，否则返回错误码。
 **/
int CfgAddSymbol(ContextFreeGrammar *grammar, int id, const char *str, char type)
{
    Symbol *symbol;

    if (!grammar || !str
            || (type != SYMBOL_TYPE_NONTERMINAL
                && type != SYMBOL_TYPE_TERMINAL
                && type != SYMBOL_TYPE_EMPTY))
        return -EINVAL;

    symbol = CfgAllocSymbol(id, str, type);
    if (!symbol) {
        PrErr("ENOMEM");
        return -ENOMEM;
    }
    if (id >= grammar->maxSymId)
        grammar->maxSymId = id + 1;
    list_add_tail(&symbol->node, &grammar->symbolList);
    return 0;
}

/*
 * 功能：申请上下文无关文法资源
 * pGrammar：文法
 * conflictHandle：生成预测分析表时的冲突处理函数
 * emptyId：空符号id
 * endId: 结束符号id
 * startId: 开始符号id
 * 返回值：成功时返回0，否则返回错误码。
 **/
int ContextFreeGrammarAlloc(ContextFreeGrammar **pGrammar, int emptyId, int endId, int startId)
{
    ContextFreeGrammar *grammar;

    if (!pGrammar)
        return -EINVAL;

    *pGrammar = NULL;
    grammar = malloc(sizeof (*grammar));
    if (!grammar) {
        PrErr("ENOMEM");
        return -ENOMEM;
    }
    INIT_LIST_HEAD(&grammar->symbolList);
    INIT_LIST_HEAD(&grammar->genFirstSymIdList);
    grammar->emptySymId = emptyId;
    grammar->endSymId = endId;
    grammar->startSymId = startId;
    grammar->maxSymId = 0;
    *pGrammar = grammar;
    return 0;
}

/*
 * 功能：释放文法资源
 * grammar: 文法
 * 返回值：无
 **/
void ContextFreeGrammarFree(ContextFreeGrammar *grammar)
{
    struct list_head *pos;
    Symbol *symbol;

    if (!grammar)
        return;

    for (pos = grammar->symbolList.next; pos != &grammar->symbolList; ) {
        symbol = container_of(pos, Symbol, node);
        pos = pos->next;
        list_del(&symbol->node);
        CfgFreeSymbol(symbol);
    }
    CfgSymIdListFree(&grammar->genFirstSymIdList);
    free(grammar);
}

/*
 * 功能：根据符号id获取文法符号
 * grammar: 文法
 * symId: 符号id
 * 返回值：如果存在返回文法符号指针，否则返回NULL。
 **/
Symbol *CfgGetSymbol(const ContextFreeGrammar *grammar, int symId)
{
    struct list_head *pos;
    Symbol *symbol;

    if (!grammar)
        return NULL;

    list_for_each(pos, &grammar->symbolList) {
        symbol = container_of(pos, Symbol, node);
        if (symbol->id == symId)
            return symbol;
    }
    return NULL;
}

/*
 * 功能：获取非终结符的产生式体
 * symbol: 文法符号
 * bodyId: 产生式体id
 * 返回值：成功时返回产生式体，否则返回NULL。
 **/
ProductBody *CfgSymbolGetProductBody(Symbol *symbol, int bodyId)
{
    int w = 0;
    struct list_head *pos;

    if (!symbol)
        return NULL;

    list_for_each(pos, &symbol->bodyList) {
        if (w == bodyId) {
            return container_of(pos, ProductBody, node);
        }
        w++;
    }
    return NULL;
}

/*
 * 功能：获取非终结符的产生式体
 * gramar: 文法
 * symId: 符号id
 * body: 产生式体id
 * 返回值：成功时返回产生式体，否则返回NULL。
 **/
ProductBody *CfgGetProductBody(const ContextFreeGrammar *grammar, int symId, int bodyId)
{
    Symbol *symbol;

    symbol = CfgGetSymbol(grammar, symId);
    if (!symbol)
        return NULL;
    return CfgSymbolGetProductBody(symbol, bodyId);
}

/*
 * 功能：获取符号的字符串
 * symbol: 文法符号
 * 返回值：成功时返回符号的字符串，否则返回"???"。
 **/
const char *CfgSymbolStr(const Symbol *symbol)
{
    return symbol ? symbol->str : "???";
}

/*
 * 功能：判断产生式体是否是空字符串产生式体，即形如：A -> empty  （empty表示空字符串）
 * 返回值：
 **/
int CfgProductBodyIsEmpty(const ContextFreeGrammar *grammar, const ProductBody *productBody)
{
    const struct list_head *list;

    if (!grammar || !productBody)
        return -EINVAL;

    list = &productBody->symIdList;
    if (list_empty(list))
        return -ENOSYM;
    return container_of(list->next, SymId, node)->id == grammar->emptySymId;
}

/*
 * 功能：打印产生式
 * grammar: 文法
 * symbol: 非终结符号
 * productBody: 产生式体。
 * 返回值：无。
 **/
void CfgProductBodyPrint(ContextFreeGrammar *grammar, Symbol *symbol,
                         ProductBody *productBody)
{
    struct list_head *pos;
    SymId *symId;
    Symbol *iterSymbol;

    if (!grammar || !symbol || !productBody)
        return;

    printf("<%d, %d> ", symbol->id, productBody->id);
    printf("%s -> ", CfgSymbolStr(symbol));
    list_for_each(pos, &productBody->symIdList) {
        symId = container_of(pos, SymId, node);
        iterSymbol = CfgGetSymbol(grammar, symId->id);
        printf("%s ", CfgSymbolStr(iterSymbol));
    }
    printf("\n");
}

/*
 * 功能：打印非终结符的各个产生式
 * grammar: 文法
 * symbol: 非终结符
 * 返回值：无。
 **/
static void CfgSymbolPrintProduct(ContextFreeGrammar *grammar, Symbol *symbol)
{
    struct list_head *pos;
    ProductBody *pb;

    if (symbol->type != SYMBOL_TYPE_NONTERMINAL)
        return;

    list_for_each(pos, &symbol->bodyList) {
        pb = container_of(pos, ProductBody, node);
        CfgProductBodyPrint(grammar, symbol, pb);
    }
}

/*
 * 功能：打印文法中的产生式
 * grammar: 文法
 * 返回值：无。
 **/
void CfgPrintProduct(ContextFreeGrammar *grammar)
{
    struct list_head *pos;
    Symbol *symbol;

    if (!grammar)
        return;

    list_for_each(pos, &grammar->symbolList) {
        symbol = container_of(pos, Symbol, node);
        CfgSymbolPrintProduct(grammar, symbol);
    }
}






