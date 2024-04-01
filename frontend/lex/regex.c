/*
 * 文件：regex.c
 * 描述：用于实现正则表达式匹配功能。
 * 操作符: ab: 表示字符a, b连接
 *        a|b: 表示字符a或b
 *        a*: 表示字符a的Kleene闭包
 * 操作符优先级由高到低依次为：
 *              ()
 *              *
 *              连接
 *              |
 * 作者：Li Rongjin
 * 日期：2023-11-06
 **/

#include "regex.h"
#include "dfa.h"
#include "cpl_errno.h"
#include "cpl_mm.h"
#include "stdio.h"
#include "string.h"
#include "stdlib.h"

/*
 * 功能：判断字符串str跟正则表达式re是否匹配。
 * 返回值：匹配返回1，不匹配返回0；出错返回错误码。
 **/
int RegExIsMatch(RegEx *re, const char *str)
{
    char ch;
    DfaState *state;

    if (!re || !str) {
        return -EINVAL;
    }

    state = DfaGetState(re, 0);
    if (!state)
        return -ENOSTATE;
    while (*str) {
        ch = *str;
        state = DfaMoveState(re, state, ch);
        if (!state)
            return 0;
        str++;
    }

    return DfaIsFinishState(re, state);
}

/*
 * 功能：找到字符串str跟正则表达式re的最大匹配
 * re：正则表达式
 * str：待匹配字符串
 * 返回值：找到时返回指向最后一个字符的指针，否则返回NULL
 **/
const char *RegExMostScan(RegEx *re, const char *str)
{
    char ch;
    DfaState *state;
    const char *endPos = NULL;

    if (!re || !str) {
        return NULL;
    }

    state = DfaGetState(re, 0);
    if (!state)
        return NULL;
    while (*str) {
        ch = *str;
        state = DfaMoveState(re, state, ch);
        if (!state)
            break;
        if (DfaIsFinishState(re, state))
            endPos = str;
        str++;
    }

    return endPos;
}

/*
 * 功能：返回正则表达式的最大匹配
 * re：正则表达式
 * reader：读取器
 * pCplString：匹配到的字符串
 * 返回值：匹配到字符串返回0，未匹配到字符串返回-ENOSTATE，读到文件末尾返回-EEOF
 **/
int RegExMostScanReader(RegEx *re, RegExReader *reader, CplString **pCplString)
{
    int ch;
    DfaState *state;
    int pos = 0, lastPos = -1;
    CplString *cplString;

    if (!re || !reader || !pCplString) {
        return -EINVAL;
    }
    cplString = CplStringAlloc();
    if (!cplString)
        return -ENOMEM;

    state = DfaGetState(re, 0);
    if (!state) {
        CplStringFree(cplString);
        return -EMISC;
    }

    while (ch = reader->currentChar(reader), ch >= 0) {
        CplStringAppendCh(cplString, ch);
        state = DfaMoveState(re, state, ch);
        if (!state) {
            if (lastPos != -1) {
                CplStringDel(cplString, pos-lastPos);
                *pCplString = cplString;
                return 0;
            } else {
                CplStringFree(cplString);
                return -ENOSTATE;
            }
        }
        if (DfaIsFinishState(re, state)) {
            lastPos = pos;
        }
        reader->incPos(reader);
        pos++;
    }

    if (ch == -EEOF) {
        if (lastPos != -1) {
            CplStringDel(cplString, pos-1-lastPos);
            *pCplString = cplString;
            return 0;
        } else {
            CplStringFree(cplString);
            return -EEOF;
        }
    } else {
        CplStringFree(cplString);
        return -EMISC;
    }
}

/*
 * 功能：找到字符串str跟正则表达式re的最小匹配
 * re：正则表达式
 * str：待匹配字符串
 * 返回值：找到时返回指向最后一个字符的指针，否则返回NULL
 **/
const char *RegExLeastScan(RegEx *re, const char *str)
{
    char ch;
    DfaState *state;

    if (!re || !str) {
        return NULL;
    }

    state = DfaGetState(re, 0);
    if (!state)
        return NULL;
    while (*str) {
        ch = *str;
        state = DfaMoveState(re, state, ch);
        if (!state)
            break;
        if (DfaIsFinishState(re, state))
            return str;
        str++;
    }

    return NULL;
}

/*
 * 功能：生成用于字符串匹配的正则表达式对象。
 * regExStr：正则表达式字符串。
 * regEx：输出型指针，传出指向RegEx的指针。
 * 返回值：成功时返回0，否则返回错误码。
 **/
int RegExGenerate(const char *regExStr, RegEx **regEx)
{
    return DfaGenerate(regExStr, regEx);
}

/*
 * 功能：释放RegEx资源
 * 返回值：无。
 **/
void RegExFree(RegEx *regEx)
{
    DfaFree(regEx);
}
