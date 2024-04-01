#ifndef __REGEX_H__
#define __REGEX_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "dfa.h"
#include "cpl_string.h"

typedef struct _RegExReader {
    int (*currentChar)(struct _RegExReader *);
    int (*incPos)(struct _RegExReader *);
    int (*getChar)(struct _RegExReader *);
    void *arg;
} RegExReader;

typedef Dfa RegEx;

int RegExGenerate(const char *regExStr, RegEx **regEx);
void RegExFree(RegEx *regEx);

int RegExIsMatch(RegEx *re, const char *str);
int RegExMostScanReader(RegEx *re, RegExReader *reader, CplString **pCplString);
const char *RegExMostScan(RegEx *re, const char *str);
const char *RegExLeastScan(RegEx *re, const char *str);

#ifdef __cplusplus
}
#endif

#endif /*__REGEX_H__*/

