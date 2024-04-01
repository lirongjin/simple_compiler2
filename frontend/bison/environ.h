#ifndef __ENVIRON_H__
#define __ENVIRON_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "list.h"
#include "lex.h"
#include "abstract_tree.h"

/*作用域中的声明条目*/
typedef struct {
    struct list_head node;
    const Token *id;            /*标识*/
    Type *type;                 /*标识类型*/
    size_t offset;              /*所在作用域的偏移量*/
    uint32_t funArgFlag: 1;     /*token是函数形式参数标志*/
    Domain *domain;             /*条目所在的作用域。*/
} DomainEntry;

/*下一级作用域链表节点*/
typedef struct {
    struct list_head node;
    Domain *domain;
} NextDomain;

typedef enum {
    DT_GLOBAL,  /*全局作用域*/
    DT_FUN,     /*函数作用域*/
    DT_BLK,     /*块作用域*/
    DT_STRUCT,  /*结构体作用域*/
} DomainType;

/*作用域*/
typedef struct _Domain {
    DomainType type;
    struct _Domain *prev;       /*上一级作用域*/
    struct list_head entryList; /*条目链表，节点类型：DomainEntry*/
    size_t offset;              /*当前作用域所在作用域的偏移量*/
    size_t offset_num;          /*生成作用域中变量的偏移量*/
    size_t tmp_offset_num;      /*临时变量偏移量生成器*/
    FunType *funType;           /*函数作用域的函数类型信息。*/
    struct _Domain *funDomain;  /*块作用域所在的函数作用域。*/
    size_t dynamicArrSize;      /*块作用域/函数作用域中动态数组的大小。*/
    size_t allLocalVarSize;     /*函数作用域及其子块作用域中所有的局部变量的大小*/
    size_t allTmpVarSize;       /*函数作用域及其子块作用域中所有临时变量的大小*/
    struct list_head nextList;  /*下一级子作用域链表，节点类型：NextDomain*/
    AlignType alignType;  /*结构体对齐类型*/
} Domain;

/*break/continue语句所在的上级while/switch/...语句信息*/
typedef struct {
    struct list_head node;
    Stmt *stmt;     /*while/switch...语句*/
} LastLevel;

/*环境*/
typedef struct _Environ {
    Domain *domain; /*当前作用域*/
    Type *boolType, *charType, *shortType, *intType, *floatType;    /*基本类型*/
    Expr *trueExpr, *falseExpr;
    struct list_head lastLevelList;     /*节点类型：LastLevel*/
} Environ;

DomainEntry *EnvNewDomainEntry(const Token *id, Type *type, size_t offset, Domain *domain);
int EnvDomainPutEntry(Domain *domain, DomainEntry *entry);
DomainEntry *EnvDomainGetEntry(Domain *domain, const Token *token);
Type *EnvDomainGetTokenType(Domain *domain, const Token *id);
Domain *EnvDomainPush(Domain *prev, DomainType type);
Domain *EnvDomainPop(Domain *domain);
void EnvFreeDomainList(Domain *domain);

int EnvLastLevelListPush(Environ *env, Stmt *stmt);
void EnvLastLevelListPop(Environ *env);
Stmt *EnvBreakLastLevelListTop(Environ *env);
Stmt *EnvContinueLastLevelListTop(Environ *env);

Environ *EnvAlloc(void);
void EnvFree(Environ *env);

#ifdef __cplusplus
}
#endif

#endif /*__ENVIRON_H__*/
