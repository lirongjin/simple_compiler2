#ifndef __LEX_WORDS_H__
#define __LEX_WORDS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "list.h"
#include "cpl_string.h"

typedef struct _Lex Lex;

typedef enum {
    TT_EOF,         /*文件结束符*/

    /*关键字*/
    TT_BREAK,       /*break*/
    TT_DO,          /*do*/
    TT_ELSE,        /*else*/
    TT_FALSE,       /*false*/
    TT_IF,          /*if*/
    TT_TRUE,        /*true*/
    TT_FOR,         /*for*/
    TT_WHILE,       /*while*/
    TT_RETURN,      /*return*/
    TT_STRUCT,      /*struct*/
    TT_INT,
    TT_SHORT,
    TT_CHAR,
    TT_BOOL,
    TT_FLOAT,
    TT_SWITCH,
    TT_CASE,
    TT_DEFAULT,
    TT_CONTINUE,
    TT_VOID,       /*void*/

    TT_ID,          /*普通标识符*/
    TT_DIGIT,       /*整数*/
    TT_STR,         /*字符串*/

    TT_LBRACE,      /*{*/
    TT_RBRACE,      /*}*/
    TT_LBRACKET,    /*(*/
    TT_RBRACKET,    /*)*/
    TT_LSBRACKET,   /*[*/
    TT_RSBRACKET,   /*]*/

    TT_SEMI_COLON,  /*;*/
    TT_COLON,       /*:*/
    TT_SHARP,       /*#*/
    TT_DOT,         /*.*/
    TT_COMMA,       /*,*/

    TT_ARROW,       /*->*/

    TT_ADD,         /*+*/
    TT_SUB,         /*-*/
    TT_MUL,         /***/
    TT_DIV,         /*/*/
    TT_MOD,         /*%*/

    TT_EQ,          /*==*/
    TT_NE,          /*!=*/
    TT_LT,          /*<*/
    TT_LE,          /*<=*/
    TT_GT,          /*>*/
    TT_GE,          /*>=*/

    TT_ASSIGN,      /*=*/

    TT_AND,         /*&&*/
    TT_OR,          /*||*/
    TT_NOT,         /*!*/

    TT_BAND,        /*&*/
    TT_BOR,         /*|*/
    TT_BNOR,        /*^*/

    TT_SPACE,       /*空格*/
    TT_TAB,         /*水平制表符*/
    TT_CR,          /*回车*/
    TT_LN,          /*换行*/
} TokenTag;

typedef struct {
    TokenTag tag;
    union {
        const CplString *cplString;
        int digit;
        float fDigit;
    };
} Token;

typedef struct {
    struct list_head node;
    const CplString *key;
    Token token;
} LexWord;

typedef struct {
    struct list_head wordList;
} LexWords;

int LexWordsInit(Lex *lex, LexWords *words);
void LexWordsFree(LexWords *words);
Token *LexWordsGet(Lex *lex, const CplString *key, TokenTag tag);

#ifdef __cplusplus
}
#endif

#endif /*__LEX_WORDS_H__*/
