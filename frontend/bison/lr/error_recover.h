#ifndef __ERROR_RECOVER_H__
#define __ERROR_RECOVER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "list.h"

typedef struct _LrErrorRecover LrErrorRecover;
typedef struct _Lalr Lalr;
typedef struct _LrReader LrReader;
typedef struct _LrParser LrParser;
typedef struct _LrErrorRecover LrErrorRecover;

typedef int (RecoverHandle)(LrErrorRecover *, Lalr *,
                                LrParser *, LrReader *);

typedef struct _LrErrorRecover {
    struct list_head recoverSymIdList;
    RecoverHandle *recoverHandle;
    void (*resourceFree)(struct _LrErrorRecover *);
} LrErrorRecover;

int LrErrorRecoverAlloc(LrErrorRecover **pLrErroeRecover, int recoverSymIdArr[], unsigned int len);
void LrErrorReocverFree(LrErrorRecover *lrErrorRecover);

#ifdef __cplusplus
}
#endif

#endif /*__ERROR_RECOVER_H__*/
