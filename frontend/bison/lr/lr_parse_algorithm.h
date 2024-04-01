#ifndef __LR_PARSE_ALGORITHM_H__
#define __LR_PARSE_ALGORITHM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "lalr_parse_table.h"

typedef struct {
    struct list_head node;
    int id;
    void *arg;
} StackStateNode;

typedef struct _LrReader {
    void *arg;
    int (*getCurrentSymId)(struct _LrReader *);
    int (*inputPosInc)(struct _LrReader *);
} LrReader;

typedef struct _LrErrorRecover LrErrorRecover;

typedef struct _LrParser {
    int (*shiftHandle)(void **, LrReader *);
    struct list_head configurationStack;
    LrErrorRecover *errorRecover;
    unsigned int syntaxErrorFlag;

} LrParser;

int LrParserAlloc(LrParser **pParser, int (shiftHandle)(void **, LrReader *), LrErrorRecover *errorRecover);
void LrParserFree(LrParser *parser);
int LrParse(LrParser *lrParser, Lalr *lalr, LrReader *);

int ConfigurationStackPushState(struct list_head *list, int id, void *arg);
StackStateNode *ConfigurationStackTopState(struct list_head *list, unsigned int idx);
int ConfigurationStackPopState(struct list_head *list, int len);

#if 0
int ConfigurationStackPush(struct list_head *list, int id);
int ConfigurationStackTop(struct list_head *list);
int ConfigurationStackPop(struct list_head *list, int len);
#endif

#ifdef __cplusplus
}
#endif

#endif /*__LR_PARSE_ALGORITHM_H__*/









