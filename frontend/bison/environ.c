#include "environ.h"
#include "abstract_tree.h"
#include "list.h"
#include "stdint.h"
#include "cpl_mm.h"
#include "cpl_errno.h"
#include "cpl_debug.h"
#include "stdlib.h"

#define PrErr(...)      Pr(__FILE__, __LINE__, __FUNCTION__, "error", __VA_ARGS__)

#define ENV_DEBUG
#ifdef ENV_DEBUG
#define PrDbg(...)      Pr(__FILE__, __LINE__, __FUNCTION__, "debug", __VA_ARGS__)
#endif

/*
 * 功能：内存分配函数，如果失败则直接退出程序。
 * 返回值：
 */
static inline void *EnvMAlloc(size_t size)
{
    void *p;

    p = CplAlloc(size);
    if (!p) {
        PrErr("no memory\n");
        exit(-1);
    }
    return p;
}

/*
 * 功能：新建一条作用域条目
 * 返回值：
 */
DomainEntry *EnvNewDomainEntry(const Token *id, Type *type, size_t offset, Domain *domain)
{
    DomainEntry *entry;

    entry = EnvMAlloc(sizeof (*entry));
    entry->id = id;
    entry->type = type;
    entry->funArgFlag = 0;
    entry->offset = offset;
    entry->domain = domain;
    return entry;
}

/*
 * 功能：释放作用域条目
 * 返回值：
 */
static inline void EnvFreeDomainEntry(DomainEntry *entry)
{
    CplFree(entry);
}

;

static void EnvNextDomainListAdd(struct list_head *list, Domain *domain)
{
    NextDomain *nextDomain;

    nextDomain = EnvMAlloc(sizeof (*nextDomain));
    nextDomain->domain = domain;
    list_add_tail(&nextDomain->node, list);
}

/*
 * 功能：新建作用域
 * 返回值：
 */
static inline Domain *EnvNewDomain(Domain *prev, DomainType type)
{
    Domain *domain;

    domain = EnvMAlloc(sizeof (*domain));
    domain->type = type;
    domain->prev = prev;
    domain->offset_num = 0;
    domain->tmp_offset_num = 0;
    domain->funDomain = NULL;
    domain->dynamicArrSize = 0;
    domain->allLocalVarSize = 0;
    domain->allTmpVarSize = 0;
    INIT_LIST_HEAD(&domain->entryList);
    INIT_LIST_HEAD(&domain->nextList);
    domain->alignType = AT_1BYTE;
    if (prev) { /*把当前作用域添加到上一级作用域的下一级作用域链表*/
        EnvNextDomainListAdd(&prev->nextList, domain);
    }
    return domain;
}

/*
 * 功能：释放作用域
 * 返回值：
 */
static void EnvFreeDomain(Domain *domain)
{
    struct list_head *pos;
    DomainEntry *entry;

    for (pos = domain->entryList.next; pos != &domain->entryList; ) {
        entry = container_of(pos, DomainEntry, node);
        pos = pos->next;
        list_del(&entry->node);
        EnvFreeDomainEntry(entry);
    }
}

/*
 * 功能：释放作用域链表
 * 返回值：
 */
void EnvFreeDomainList(Domain *domain)
{
    Domain *prevDomain;

    for (; domain != NULL;) {
        prevDomain = domain->prev;
        EnvFreeDomain(domain);
        domain = prevDomain;
    }
}

/*
 * 功能：向作用域中放置条目
 * 返回值：
 */
int EnvDomainPutEntry(Domain *domain, DomainEntry *entry)
{
    if (!domain || !entry)
        return -EINVAL;
    list_add_tail(&entry->node, &domain->entryList);
    return 1;
}

/*
 * 功能：从作用域获取条目
 * 返回值：
 */
static DomainEntry *_EnvDomainGetEntry(Domain *domain, const Token *id)
{
    DomainEntry *entry;
    struct list_head *pos;

    list_for_each(pos, &domain->entryList) {
        entry = container_of(pos, DomainEntry, node);
        if (id == entry->id)
            return entry;
    }
    return NULL;
}

/*
 * 功能：从作用域获取条目
 * 返回值：
 */
DomainEntry *EnvDomainGetEntry(Domain *domain, const Token *token)
{
    Domain *iterDomain;
    DomainEntry *entry;

    if (!domain || !token)
        return NULL;

    for (iterDomain = domain; iterDomain != NULL; iterDomain = iterDomain->prev) {
        entry = _EnvDomainGetEntry(iterDomain, token);
        if (entry)
            return entry;
    }
    return NULL;
}

/*
 * 功能：从作用域中获取token的类型信息。
 * 返回值：
 */
Type *EnvDomainGetTokenType(Domain *domain, const Token *token)
{
    DomainEntry *entry;

    if (!domain || !token)
        return NULL;

    entry = EnvDomainGetEntry(domain, token);
    if (!entry) {
        PrErr("no %s domain entry\n", token->cplString->str);
        exit(-1);
    }
    return entry->type;
}

/*
 * 功能：返回domain所在的函数作用域
 * 返回值：
 */
static Domain *EnvDomainTopFunDomain(Domain *domain)
{
    if (domain->type == DT_BLK) {
        return EnvDomainTopFunDomain(domain->prev);
    } else if (domain->type == DT_FUN) {
        return domain;
    } else {
        PrErr("domain->type error: %d", domain->type);
        exit(-1);
    }
}

/*
 * 功能：新建作用域。
 * 返回值：
 */
Domain *EnvDomainPush(Domain *prev, DomainType type)
{
    Domain *domain;

    domain = EnvNewDomain(prev, type);
    if (type == DT_BLK) {
        domain->funDomain = EnvDomainTopFunDomain(domain);
    } else if (type == DT_FUN) {
        domain->funDomain = domain;
    } else {
        domain->funDomain = NULL;
    }
    return domain;
}

/*
 * 功能：弹出作用域
 * 返回值：
 */
Domain *EnvDomainPop(Domain *domain)
{
    Domain *prevDomain;

    if (!domain)
        return NULL;
    prevDomain = domain->prev;
    return prevDomain;
}

/*
 * 功能：新增lastLevel
 * 返回值：
 */
int EnvLastLevelListPush(Environ *env, Stmt *stmt)
{
    LastLevel *lastLevel;

    if (!env || !stmt)
        return -EINVAL;
    lastLevel = EnvMAlloc(sizeof (*lastLevel));
    lastLevel->stmt = stmt;
    list_add_tail(&lastLevel->node, &env->lastLevelList);
    return 0;
}

/*
 * 功能：弹出lastLevel
 * 返回值：
 */
void EnvLastLevelListPop(Environ *env)
{
    LastLevel *lastLevel;

    if (!env || list_empty(&env->lastLevelList))
        return;
    lastLevel = container_of(env->lastLevelList.prev, LastLevel, node);
    list_del(&lastLevel->node);
    CplFree(lastLevel);
}

/*
 * 功能：释放lastLevel链表
 * 返回值：
 */
static void EnvLastLevelListFree(struct list_head *list)
{
    struct list_head *pos;
    LastLevel *lastLevel;

    for (pos = list->next; pos != list; ) {
        lastLevel = container_of(pos, LastLevel, node);
        pos = pos->next;
        list_del(&lastLevel->node);
        CplFree(lastLevel);
    }
}

/*
 * 功能：根据lastLevel链表获取break语句所在的上级语句
 * 返回值：
 */
Stmt *EnvBreakLastLevelListTop(Environ *env)
{
    struct list_head *pos;
    LastLevel *lastLevel;

    if (!env)
        return NULL;
    list_for_each_prev(pos, &env->lastLevelList) {
        lastLevel = container_of(pos, LastLevel, node);
        switch (lastLevel->stmt->type) {
        case ST_WHILE:
        case ST_DO:
        case ST_SWITCH:
            return lastLevel->stmt;
        default:;
        }
    }
    return NULL;
}

/*
 * 功能：根据lastLevel链表获取continue语句所在的上级语句
 * 返回值：
 */
Stmt *EnvContinueLastLevelListTop(Environ *env)
{
    struct list_head *pos;
    LastLevel *lastLevel;

    if (!env)
        return NULL;
    list_for_each_prev(pos, &env->lastLevelList) {
        lastLevel = container_of(pos, LastLevel, node);
        switch (lastLevel->stmt->type) {
        case ST_WHILE:
        case ST_DO:
            return lastLevel->stmt;
        default:;
        }
    }
    return NULL;
}

/*
 * 功能：初始化环境
 * 返回值：
 */
static inline void EnvInit(Environ *env)
{
    env->domain = EnvNewDomain(NULL, DT_GLOBAL);
    env->boolType = ATreeNewBaseType(BTT_BOOL, 1, AT_1BYTE);
    env->charType = ATreeNewBaseType(BTT_CHAR, 1, AT_1BYTE);
    env->shortType = ATreeNewBaseType(BTT_SHORT, 2, AT_2BYTE);
    env->intType = ATreeNewBaseType(BTT_INT, 4, AT_4BYTE);
    env->floatType = ATreeNewBaseType(BTT_FLOAT, 4, AT_4BYTE);
    env->trueExpr = ATreeNewCValueSpcBoolExpr(env->domain, CVTS_TRUE);
    env->falseExpr = ATreeNewCValueSpcBoolExpr(env->domain, CVTS_FALSE);
    INIT_LIST_HEAD(&env->lastLevelList);
}

/*
 * 功能：分配环境
 * 返回值：
 */
Environ *EnvAlloc(void)
{
    Environ *env;

    env = EnvMAlloc(sizeof (*env));
    EnvInit(env);
    return env;
}

/*
 * 功能：释放环境
 * 返回值：
 */
void EnvFree(Environ *env)
{
    if (!env)
        return;
    if (env->domain)
        EnvFreeDomain(env->domain);
    ATreeFreeBaseType(env->boolType);
    ATreeFreeBaseType(env->charType);
    ATreeFreeBaseType(env->shortType);
    ATreeFreeBaseType(env->intType);
    ATreeFreeBaseType(env->floatType);
    ATreeFreeBaseType(env->boolType);
    ATreeFreeBaseType(env->boolType);
    EnvLastLevelListFree(&env->lastLevelList);
    CplFree(env);
}

