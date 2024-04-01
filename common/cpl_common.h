#ifndef __CPL_COMMON_H__
#define __CPL_COMMON_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr)         (sizeof (arr) / sizeof((arr)[0]))
#endif

/*用于支持变参。*/
#define VA_ARRx(type, ...)      (type []){__VA_ARGS__}
#define VA_ARR(type, ...)       VA_ARRx(type, __VA_ARGS__), ARRAY_SIZE(VA_ARRx(type, __VA_ARGS__))

static inline size_t Align16Byte(size_t addr)
{
    return ((addr + 15) >> 4) << 4;
}

static inline size_t Align8Byte(size_t addr)
{
    return ((addr + 7) >> 3) << 3;
}

static inline size_t Align4Byte(size_t addr)
{
    return ((addr + 3) >> 2) << 2;
}

static inline size_t Align2Byte(size_t addr)
{
    return ((addr + 1) >> 1) << 1;
}


#ifdef __cplusplus
}
#endif

#endif  /*__CPL_COMMON_H__*/
