#include "sym_id.h"
#include "cpl_errno.h"
#include "cpl_debug.h"
#include "stdlib.h"

#define PrErr(...)          Pr(__FILE__, __LINE__, __FUNCTION__, "error", __VA_ARGS__)

/*
 * 功能：判断符号集合list中是否包含id
 * list: 符号集合
 * id：id
 * 返回值：如果符号集合list包含id返回1，否则返回0。
 **/
int CfgSymIdSetIsContain(const struct list_head *list, int id)
{
    struct list_head *pos;
    SymId *symId;

    if (!list)
        return -EINVAL;

    list_for_each(pos, list) {
        symId = container_of(pos, SymId, node);
        if (symId->id == id)
            return 1;
    }
    return 0;
}

/*
 * 功能：判断符号链表lList是否包含符号链表rList
 * lList: 左符号链表
 * rList: 右符号链表
 * 返回值：如果符号链表lList包含rList返回1，否则返回0。
 **/
int CfgSymIdListIsContainList(const struct list_head *lList, const struct list_head *rList)
{
    struct list_head *pos;
    SymId *symId;

    if (!lList || !rList)
        return -EINVAL;

    list_for_each(pos, rList) {
        symId = container_of(pos, SymId, node);
        if (CfgSymIdSetIsContain(lList, symId->id) == 0)
            return 0;
    }
    return 1;
}

/*
 * 功能：判断符号id链表lList和rList是否相等。
 * lList: 左链表
 * rList: 右链表
 * 返回值：如果符号id链表lList和rList相等返回1，否则返回0。
 **/
int CfgSymIdListIsEqual(const struct list_head *lList, const struct list_head *rList)
{
    if (!lList || !rList)
        return -EINVAL;

    if (CfgSymIdListIsContainList(lList, rList) == 1
            && CfgSymIdListIsContainList(rList, lList) == 1)
        return 1;
    else
        return 0;
}

/*
 * 功能：向符号id链表添加符号id
 * list: 符号id链表
 * id：id
 * 返回值：成功时返回0，否则返回错误码。
 **/
int CfgSymIdListAddId(struct list_head *list, int id)
{
    SymId *symId;

    if (!list)
        return -EINVAL;

    symId = malloc(sizeof (*symId));
    if (!symId) {
        PrErr("ENOMEM");
        return -ENOMEM;
    }
    symId->id = id;
    list_add(&symId->node, list);
    return 0;
}

/*
 * 功能：向符号id链表的尾部添加id
 * list: 符号id链表
 * id：id
 * 返回值：成功时返回0，否则返回错误码。
 **/
int CfgSymIdListTailAddId(struct list_head *list, int id)
{
    SymId *symId;

    if (!list)
        return -EINVAL;

    symId = malloc(sizeof (*symId));
    if (!symId) {
        PrErr("ENOMEM");
        return -ENOMEM;
    }
    symId->id = id;
    list_add_tail(&symId->node, list);
    return 0;
}

/*
 * 功能：向符号id链表lList的尾部添加符号id链表rList。
 * lList: 左符号id链表
 * rList: 右符号id链表
 * 返回值：成功时返回0，否则返回错误码。
 **/
int CfgSymIdListTailAddList(struct list_head *lList, const struct list_head *rList)
{
    struct list_head *pos;
    SymId *symId;
    int error = 0;

    if (!lList || !rList)
        return -EINVAL;

    list_for_each(pos, rList) {
        symId = container_of(pos, SymId, node);
        error = CfgSymIdListTailAddId(lList, symId->id);
        if (error != -ENOERR)
            return error;
    }
    return error;
}

/*
 * 功能：拷贝rList链表中从pos开始的符号id到lList
 * lList：lList
 * rList：rList
 * pos：拷贝的起始节点
 * 返回值：成功时返回0，否则返回错误码。
 **/
int CfgSymIdListCopyFromPos(struct list_head *lList, const struct list_head *rList, const struct list_head *pos)
{
    SymId *symId;
    int error = 0;

    for (; pos != rList; pos = pos->next) {
        symId = container_of(pos, SymId, node);
        error = CfgSymIdListTailAddId(lList, symId->id);
        if (error != -ENOERR)
            return error;
    }
    return error;
}

/*
 * 功能：向符号id集合中添加新的符号id
 * list: 符号id集合
 * id: id
 * 返回值：成功时返回0，否则返回错误码。
 **/
int CfgSymIdSetAddId(struct list_head *list, int id)
{
    if (!list)
        return -EINVAL;
    if (CfgSymIdSetIsContain(list, id) == 1)
        return 0;
    return CfgSymIdListAddId(list, id);
}

/*
 * 功能：向符号集合lList中添加rList上的符号。
 * lList: 左链表
 * rList: 右链表
 * 返回值：成功时返回0，否则返回错误码。
 **/
int CfgSymIdSetAddList(struct list_head *lList, const struct list_head *rList)
{
    struct list_head *pos;
    SymId *symId;
    int error = 0;

    if (!lList || !rList)
        return -EINVAL;

    list_for_each(pos, rList) {
        symId = container_of(pos, SymId, node);
        if (0 == CfgSymIdSetIsContain(lList, symId->id)) {
            error = CfgSymIdListAddId(lList, symId->id);
            if (error != -ENOERR)
                return error;
        }
    }
    return error;
}

/*
 * 功能：向符号集合lList中添加rList中的符号，指定符号id除外。
 * lList: 左链表
 * rList: 右链表
 * id：id
 * 返回值：成功时返回0，否则返回错误码。
 **/
int CfgSymIdSetAddListExcludeId(struct list_head *lList, const struct list_head *rList, int id)
{
    struct list_head *pos;
    SymId *symId;
    int error = 0;

    if (!lList || !rList)
        return -EINVAL;

    list_for_each(pos, rList) {
        symId = container_of(pos, SymId, node);
        if (0 == CfgSymIdSetIsContain(lList, symId->id)
                && symId->id != id) {
            error = CfgSymIdListAddId(lList, symId->id);
            if (error != -ENOERR)
                return error;
        }
    }
    return error;
}

/*
 * 功能：向符合集合lList中添加rList中的符号，指定符号除外，如果添加了符号，updateFlag被置1。
 * lList: 左链表
 * rList: 右链表
 * id: id
 * updageFlag：如果有符号被加入则置1，否则置0。
 * 返回值：成功时返回0，否则返回错误码。
 **/
int CfgSymIdSetAddListUpdateFlag(struct list_head *lList, const struct list_head *rList, int id,
                                 char *updageFlag)
{
    struct list_head *pos;
    SymId *symId;
    int error = 0;

    if (!lList || !rList || !updageFlag)
        return -EINVAL;

    *updageFlag = 0;
    list_for_each(pos, rList) {
        symId = container_of(pos, SymId, node);
        if (0 == CfgSymIdSetIsContain(lList, symId->id)
                && symId->id != id) {
            *updageFlag = 1;
            error = CfgSymIdListAddId(lList, symId->id);
            if (error != -ENOERR)
                return error;
        }
    }
    return error;
}

int CfgSymIdListDelId(struct list_head *list, int id)
{
    struct list_head *pos;
    SymId *symId;

    if (!list)
        return -EINVAL;

    list_for_each(pos, list) {
        symId = container_of(pos, SymId, node);
        if (symId->id == id) {
            list_del(pos);
            free(symId);
            return 1;
        }
    }
    return 0;
}

/*
 * 功能：释放符号id链表资源
 * list: 符号id链表
 * 返回值：无
 **/
void CfgSymIdListFree(struct list_head *list)
{
    struct list_head *pos;
    SymId *symId;

    if (!list)
        return;

    for (pos = list->next; pos != list; ) {
        symId = container_of(pos, SymId, node);
        pos = pos->next;
        list_del(&symId->node);
        free(symId);
    }
}

/*
 * 功能：打印符号id链表
 * list: 符号id链表
 * symIdStr: 用符号id获取字符串的函数
 * 返回值：无
 **/
void CfgSymIdListPrint(const struct list_head *list, const char *(*symIdStr)(int, const void *), const void *arg)
{
    struct list_head *pos;
    SymId *symId;

    if (!list || !symIdStr)
        return;

    list_for_each(pos, list) {
        symId = container_of(pos, SymId, node);
        printf("%s ", symIdStr(symId->id, arg));
    }
}





