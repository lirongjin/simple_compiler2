#include "cpl_string.h"
#include "cpl_mm.h"
#include "cpl_errno.h"
#include "stdio.h"
#include "string.h"

CplString *CplStringAlloc(void)
{
    CplString *cplString;

    cplString = CplAlloc(sizeof (*cplString));
    if (!cplString)
        return NULL;
    cplString->str = NULL;
    cplString->idx = 0;
    cplString->size = 0;
    return cplString;
}

void CplStringFree(CplString *cplString)
{
    if (!cplString)
        return;
    if (cplString->str)
        CplFree(cplString->str);
    CplFree(cplString);
}

int CplStringAppendCh(CplString *cplString, const char ch)
{
    if (cplString->size == 0
            || cplString->idx >= cplString->size - 1) {
        /*分配内存*/
        if (cplString->size == 0) {
            char *str;

            str = CplAlloc(8);
            if (!str)
                return -ENOMEM;
            memset(str, 0, 8);
            cplString->str = str;
            cplString->size = 8;
        } else {
            char *str;
            unsigned int allocSize;

            if (cplString->size < 1024) {
                allocSize = cplString->size * 2;
            } else {
                allocSize = cplString->size + 1024;
            }
            str = CplAlloc(allocSize);
            if (!str)
                return -ENOMEM;
            memset(str, 0, allocSize);
            cplString->size = allocSize;
            strcpy(str, cplString->str);
            CplFree(cplString->str);
            cplString->str = str;
        }
    }
    cplString->str[cplString->idx++] = ch;
    return 0;
}

int CplStringAppendString(CplString *cplString, const char *str)
{
    int error = 0;

    while (*str) {
        error = CplStringAppendCh(cplString, *str);
        if (error != -ENOERR)
            return error;
        str++;
    }
    return 0;
}

void CplStringDel(CplString *cplString, unsigned int len)
{
    if (!cplString)
        return;

    while (len--) {
        cplString->idx--;
    }
    cplString->str[cplString->idx] = '\0';
}

