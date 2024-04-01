#ifndef __BISON_H__
#define __BISON_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "lex_words.h"

typedef struct _Environ Environ;
typedef struct _Program Program;
typedef struct _Lex Lex;
typedef struct _QRRecord QRRecord;
typedef struct _FunType FunType;

typedef struct _Bison {
    Environ *env;
    Lex *lex;
    Program *program;
    QRRecord *record;
    const Token *mainToken;
    FunType *mainFunType;
} Bison;

extern Bison *bison;

void BisonExec(void);

#ifdef __cplusplus
}
#endif

#endif /*__BISON_H__*/

