#ifndef __CONTEXT_FREE_GRAMMAR_H__
#define __CONTEXT_FREE_GRAMMAR_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "list.h"
#include "sym_id.h"
#include "cpl_errno.h"
#include "cpl_common.h"

/*上下文无关文法符号类型*/
#define SYMBOL_TYPE_TERMINAL        0       /*终结符号*/
#define SYMBOL_TYPE_NONTERMINAL     1       /*非终结符号*/
#define SYMBOL_TYPE_EMPTY           2       /*空字符串符号*/

/*用于支持变参。*/
#define SYMID_TO_ARR(...)       (int []){__VA_ARGS__}
#define SYMID_ARR(...)          SYMID_TO_ARR(__VA_ARGS__), ARRAY_SIZE(SYMID_TO_ARR(__VA_ARGS__))

typedef int ProductBodyHandle(void **headArg, void *prevArg, void *bodyArg[]);
//typedef int ProductBodyHandle(void **headArg, void *bodyArg[], struct list_head *);

/*规约句柄类型*/
typedef enum {
    RHT_SELF,   /*传递规约产生式的自身参数*/
    RHT_PARENT, /*传递规约产生式头所在产生式体的参数，用于标记非终结符的数据传递。*/
} ReduceHandleType;

/*产生式体*/
typedef struct {
    struct list_head node;
    int id;
    struct list_head symIdList;         /*产生式体，节点类型：SymId*/
    struct list_head firstSymIdList;    /*FIRST集合链表，节点类型：SymId*/
    ReduceHandleType handleType;        /*规约句柄类型*/
    ProductBodyHandle *handle;          /*归约处理句柄*/
} ProductBody;

/*上下文无关文法符号*/
typedef struct _Symbol {
    struct list_head node;
    char type;          /*符号类型，0：终结符号，1：非终结符号*/
    int id;             /*符号id*/
    int bodyNum;        /*用于生成产生式体编号。*/
    const char *str;    /*符号的字符串*/
    struct list_head bodyList;          /*产生式体链表，节点类型：ProductBody*/
    struct list_head firstSymIdList;    /*FIRST集合，节点类型：SymId*/
    struct list_head followSymIdList;   /*FOLLOW集合，节点类型：SymId*/
    struct list_head tailSymIdList;     /*产生式体尾部符号id链表，节点类型：SymId*/
} Symbol;

typedef struct _Lalr Lalr;

/*上下文无关文法*/
typedef struct _ContextFreeGrammar {
    struct list_head symbolList;            /*符号链表，节点类型：Symbol*/
    int emptySymId, endSymId, startSymId;   /*空字符串符号id、结束符号id、起始符号id*/
    int maxSymId;                   /*始终保证它是文法中编号最大的符号id，用于判断向前看符号传播关系。*/

    struct list_head genFirstSymIdList;     /*正在生成FIRST集合的符号id，节点类型：SymId*/
} ContextFreeGrammar;

int ContextFreeGrammarAlloc(ContextFreeGrammar **pGrammar, int emptyId, int endId, int startId);
void ContextFreeGrammarFree(ContextFreeGrammar *grammar);
int CfgAddSymbol(ContextFreeGrammar *grammar, int id, const char *str, char type);
int CfgAddProduct(ContextFreeGrammar *grammar, int product[], unsigned int len);
int CfgAddProductH(ContextFreeGrammar *grammar, ReduceHandleType handleType, ProductBodyHandle *handle, int product[], unsigned int len);

Symbol *CfgGetSymbol(const ContextFreeGrammar *grammar, int symId);
const char *CfgSymbolStr(const Symbol *symbol);
ProductBody *CfgSymbolGetProductBody(Symbol *symbol, int bodyId);
ProductBody *CfgGetProductBody(const ContextFreeGrammar *grammar, int symId, int bodyId);
int CfgProductBodyIsEmpty(const ContextFreeGrammar *grammar, const ProductBody *productBody);

void CfgPrintProduct(ContextFreeGrammar *grammar);
void CfgProductBodyPrint(ContextFreeGrammar *grammar, Symbol *symbol,
                         ProductBody *productBody);

static inline int CfgGetProductBodyHeadSymId(struct list_head *list)
{
    if (list_empty(list))
        return -EINVAL;
    return container_of(list->next, SymId, node)->id;
}

#ifdef __cplusplus
}
#endif

#endif /*__CONTEXT_FREE_GRAMMAR_H__*/
