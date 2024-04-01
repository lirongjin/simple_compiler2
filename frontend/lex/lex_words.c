#include "lex_words.h"
#include "string.h"
#include "cpl_mm.h"
#include "cpl_errno.h"
#include "cpl_debug.h"
#include "lex.h"
#include "stdlib.h"

#define PrErr(fmt, ...)  Pr(__FILE__, __LINE__, __FUNCTION__, "error", fmt, ##__VA_ARGS__)

//#define LEX_DEBUG
#ifdef LEX_DEBUG
#define PrDbg(fmt, ...)  Pr(__FILE__, __LINE__, __FUNCTION__, "debug", fmt, ##__VA_ARGS__)
#else
#define PrDbg(fmt, ...)
#endif

/*
 * 功能：分配单词映射单元
 * 返回值：
 **/
static LexWord *LexWordAlloc(TokenTag tag)
{
    LexWord *word;

    word = CplAlloc(sizeof (*word));
    if (word) {
        word->token.tag = tag;
        word->key = NULL;
    }
    return word;
}

/*
 * 功能：释放单词存储单词映射单元
 * 返回值：
 **/
static void LexWordFree(LexWord *word)
{
    if (!word)
        return;
    if (word->key) {
        CplFree((CplString *)word->key);
    }
    free(word);
}

/*
 * 功能：释放字典中的所有单词映射单元
 * 返回值：
 **/
void LexWordsFree(LexWords *words)
{
    struct list_head *pos;
    LexWord *word;

    for (pos = words->wordList.next; pos != &words->wordList; ) {
        word = container_of(pos, LexWord, node);
        pos = pos->next;
        list_del(&word->node);
        LexWordFree(word);
    }
}

/*
 * 功能：向字典中放置一个单词映射单元
 * 返回值：
 **/
static LexWord *LexWordsPut(Lex *lex, const CplString *key, TokenTag tag)
{
    LexWord *word;

    if (!lex || !key)
        return NULL;

    word = LexWordAlloc(tag);
    if (!word) {
        PrErr("no memory\n");
        exit(-1);
    }
    word->key = key;
    word->token.cplString = key;
    list_add_tail(&word->node, &lex->words.wordList);
    return word;
}

/*
 * 功能：向字典中放置一个单词映射单元，key用普通字符串表示。
 * 返回值：
 **/
static LexWord *LexWordsPutString(Lex *lex, const char *keyStr, TokenTag tag)
{
    LexWord *word;
    CplString *kcString;
    int error = 0;

    if (!lex || !keyStr)
        return NULL;

    kcString = CplStringAlloc();
    if (!kcString) {
        PrErr("no memory\n");
        exit(-1);
    }
    error = CplStringAppendString(kcString, keyStr);
    if (error != -ENOERR) {
        printf("words put error: %d\n", error);
        exit(-1);
    }
    word = LexWordAlloc(tag);
    if (!word) {
        PrErr("no memory\n");
        exit(-1);
    }
    word->key = kcString;
    word->token.cplString = kcString;
    list_add_tail(&word->node, &lex->words.wordList);
    return word;
}

/*
 * 功能：向字典用放置一个数字。
 * 返回值：
 **/
static LexWord *LexWordsPutDigit(Lex *lex, const CplString *key, TokenTag tag, int digit)
{
    LexWord *word;

    if (!lex)
        return NULL;

    word = LexWordAlloc(tag);
    if (!word) {
        PrErr("no memory\n");
        exit(-1);
    }
    word->key = key;
    word->token.digit = digit;
    list_add_tail(&word->node, &lex->words.wordList);
    return word;
}

/*
 * 功能：根据key从字典中获取一个token。
 * 返回值：
 **/
static Token *_LexWordsGet(Lex *lex, const CplString *key)
{
    struct list_head *pos;
    LexWord *word;

    list_for_each(pos, &lex->words.wordList) {
        word = container_of(pos, LexWord, node);
        if (strcmp(key->str, word->key->str) == 0) {
            return &word->token;
        }
    }
    return NULL;
}

/*
 * 功能：根据key从字典中获取一个token，如果不存在对应的token，则
 *      向字典中新增一个单词映射。
 * 返回值：
 **/
Token *LexWordsGet(Lex *lex, const CplString *key, TokenTag tag)
{
    Token *token;
    LexWord *word;

    token = _LexWordsGet(lex, key);
    if (!token) {
        if (tag != TT_DIGIT) {
            word = LexWordsPut(lex, key, tag);
            return &word->token;
        } else {
            word = LexWordsPutDigit(lex, key, tag, atoi(key->str));
            return &word->token;
        }
    }
    return token;
}

/*
 * 功能：初始化字典。把编程语言中的关键字和特殊符号先放入字典。
 * 返回值：
 **/
int LexWordsInit(Lex *lex, LexWords *words)
{
    if (!lex || !words)
        return -EINVAL;

    INIT_LIST_HEAD(&words->wordList);
    LexWordsPutString(lex, "break", TT_BREAK);
    LexWordsPutString(lex, "do", TT_DO);
    LexWordsPutString(lex, "else", TT_ELSE);
    LexWordsPutString(lex, "false", TT_FALSE);
    LexWordsPutString(lex, "if", TT_IF);
    LexWordsPutString(lex, "true", TT_TRUE);
    LexWordsPutString(lex, "for", TT_FOR);
    LexWordsPutString(lex, "while", TT_WHILE);
    LexWordsPutString(lex, "return", TT_RETURN);
    LexWordsPutString(lex, "struct", TT_STRUCT);
    LexWordsPutString(lex, "int", TT_INT);
    LexWordsPutString(lex, "short", TT_SHORT);
    LexWordsPutString(lex, "char", TT_CHAR);
    LexWordsPutString(lex, "bool", TT_BOOL);
    LexWordsPutString(lex, "float", TT_FLOAT);
    LexWordsPutString(lex, "switch", TT_SWITCH);
    LexWordsPutString(lex, "case", TT_CASE);
    LexWordsPutString(lex, "default", TT_DEFAULT);
    LexWordsPutString(lex, "continue", TT_CONTINUE);
    LexWordsPutString(lex, "void", TT_VOID);

    LexWordsPutString(lex, "{", TT_LBRACE);
    LexWordsPutString(lex, "}", TT_RBRACE);
    LexWordsPutString(lex, "(", TT_LBRACKET);
    LexWordsPutString(lex, ")", TT_RBRACKET);
    LexWordsPutString(lex, "[", TT_LSBRACKET);
    LexWordsPutString(lex, "]", TT_RSBRACKET);

    LexWordsPutString(lex, ";", TT_SEMI_COLON);
    LexWordsPutString(lex, ":", TT_COLON);
    LexWordsPutString(lex, "#", TT_SHARP);
    LexWordsPutString(lex, ".", TT_DOT);
    LexWordsPutString(lex, ",", TT_COMMA);

    LexWordsPutString(lex, "+", TT_ADD);
    LexWordsPutString(lex, "-", TT_SUB);
    LexWordsPutString(lex, "*", TT_MUL);
    LexWordsPutString(lex, "/", TT_DIV);
    LexWordsPutString(lex, "%", TT_MOD);

    LexWordsPutString(lex, "=", TT_ASSIGN);

    LexWordsPutString(lex, "==", TT_EQ);
    LexWordsPutString(lex, "!=", TT_NE);
    LexWordsPutString(lex, "<", TT_LT);
    LexWordsPutString(lex, "<=", TT_LE);
    LexWordsPutString(lex, ">", TT_GT);
    LexWordsPutString(lex, ">=", TT_GE);

    LexWordsPutString(lex, "&&", TT_AND);
    LexWordsPutString(lex, "||", TT_OR);
    LexWordsPutString(lex, "!", TT_NOT);

    LexWordsPutString(lex, "&", TT_BAND);
    LexWordsPutString(lex, "|", TT_BOR);
    LexWordsPutString(lex, "^", TT_BNOR);

    LexWordsPutString(lex, " ", TT_SPACE);
    LexWordsPutString(lex, "\t", TT_TAB);
    LexWordsPutString(lex, "\r", TT_CR);
    LexWordsPutString(lex, "\n", TT_LN);

    LexWordsPutString(lex, "->", TT_ARROW);

    return 0;
}

