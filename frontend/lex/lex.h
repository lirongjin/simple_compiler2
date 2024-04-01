#ifndef __LEX_H__
#define __LEX_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "list.h"
#include "regex.h"
#include "lex_words.h"

typedef struct _Lex {
    struct list_head patterList;    /*节点类型：LexPattern*/
    RegExReader reader;
    LexWords words;
    Token eofToken;
    const Token *curToken;
} Lex;

Lex *LexAlloc(void);
void LexFree(Lex *lex);

int LexSetInFile(Lex *lex, const char *inFilename);
const Token *LexScan(Lex *lex);
const Token *LexCurrent(Lex *lex);

#ifdef __cplusplus
}
#endif

#endif /*__LEX_H__*/

