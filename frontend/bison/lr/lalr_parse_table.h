#ifndef __LALR_PARSE_TABLE_H__
#define __LALR_PARSE_TABLE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "list.h"

typedef struct _ContextFreeGrammar ContextFreeGrammar;

/*产生式引用*/
typedef struct {
    int headSymId;  /*产生式头的符号id*/
    int bodyId;     /*非终结符中产生式体编号。*/
} ProductRef;

/*用于查看向前看符号从当前项传播到了哪个项集的哪一项*/
typedef struct {
    struct list_head node;
    int itemSetId;  /*项集id*/
    int itemId;     /*项id*/
} FollowTransmitterItem;

/*LR0中的项*/
typedef struct {
    struct list_head node;
    int id;                 /*项编号*/
    ProductRef productRef;  /*产生式引用*/
    int pos;                /*项中点所在的位置*/
    struct list_head followList;    /*跟在这个项后面否符号id集合，节点类型：SymId*/
    struct list_head transmitterList;   /*向前看符号传播链表，节点类型：FollowTransmitterItem*/
} Item;

/*GOTO(I, X)函数节点*/
typedef struct {
    struct list_head node;
    int symId;      /*文法符号*/
    int itemSetId;  /*项集id*/
} MapNode;

/*ACTION类型*/
typedef enum {
    LAT_SHIFT,  /*移入*/
    LAT_REDUCT, /*归约*/
    LAT_ACCEPT,  /*接受*/
    LAT_ERROR,  /*错误*/
} ActionType;

/*ACTION节点*/
typedef struct {
    struct list_head node;
    ActionType type; /*动作类型*/
    int lookaheadId;    /*向前看符号，终结符号id*/
    union {
        int itemSetId;  /*项集id*/
        int itemId;     /*项id*/
    };
} ActionNode;

/*GOTO节点*/
typedef struct {
    struct list_head node;
    int symId;          /*非终结符号id*/
    int nextItemSetId;  /*下一个项集状态id*/
} GotoNode;

/*LR0的项集*/
typedef struct _ItemSet {
    struct list_head node;
    int id;         /*项集编号*/
    int itemNum;    /*项编号生产器。*/
    struct list_head itemList;      /*项链表，节点类型：Item*/
    struct list_head mapList;       /*在各个文法符号上到相关项集的映射表，节点类型：MapNode*/
    struct list_head actionList;    /*ACTION链表，节点类型：ActionNode*/
    struct list_head gotoList;      /*GOTO链表节点类型：GotoNode*/
} ItemSet;

typedef struct _Lalr Lalr;
typedef struct _ContextFreeGrammar ContextFreeGrammar;

/*冲突处理函数*/
typedef int (GotoNodeConflictHandle)(Lalr *lalr, ItemSet *itemSet,
                             GotoNode *gotoNode, int symId, int nextItemSetId) ;
typedef int (ActionNodeConflictHandle)(Lalr *lalr, ItemSet *itemSet,
                                   ActionNode *actionNode, ActionType type, int la, int id);

typedef struct _Lalr {
    ContextFreeGrammar *grammar;
    struct list_head itemSetList;   /*项集链表：节点类型：ItemSet*/
    int itemSetNum;                 /*项集编号生成器。*/
    GotoNodeConflictHandle *gotoNodeConflictHandle;
    ActionNodeConflictHandle *actionNodeConflictHandle;
} Lalr;

int LalrAlloc(Lalr **lalr, ContextFreeGrammar *grammar, GotoNodeConflictHandle *gotoHandle, ActionNodeConflictHandle *actionHandle);
void LalrFree(Lalr *lalr);
int LalrGenItemSet(Lalr *lalr);
int LalrGoto(Lalr *lalr, int itemSetId, int symId);
int LalrGenParseTable(Lalr *lalr);

ItemSet *LalrGetItemSet(Lalr *lalr, int itemSetId);
Item *LalrItemSetGetItem(const ItemSet *itemSet, int id);

ActionNode *LalrItemSetGetAction(const ItemSet *itemSet, int lookahead);
GotoNode *LalrItemSetGetGoto(const ItemSet *itemSet, int symId);

int LrItemSetGetItemByNextSymId(Lalr *lalr, const ItemSet *itemSet, int symId, Item **pItem);

void LalrPrintItemSet(const Lalr *lalr);
void LalrPrintParseTable(const Lalr *lalr);

#ifdef __cplusplus
}
#endif

#endif /*__LALR_PARSE_TABLE_H__*/
