#include "cpl_mm.h"
#include "stdlib.h"

void *CplAlloc(size_t size)
{
    return malloc(size);
}

void CplFree(void *p)
{
    return free(p);
}
