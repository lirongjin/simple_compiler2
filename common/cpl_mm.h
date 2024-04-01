#ifndef __CPL_MM_H__
#define __CPL_MM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"

void *CplAlloc(size_t size);
void CplFree(void *p);

#ifdef __cplusplus
}
#endif

#endif /*__CPL_MM_H__*/

