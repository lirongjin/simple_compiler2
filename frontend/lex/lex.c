#include "lex.h"
#include "regex.h"
#include "stdio.h"
#include "cpl_errno.h"
#include "cpl_debug.h"
#include "cpl_mm.h"
#include "stdlib.h"


#define PrErr(fmt, ...)  Pr(__FILE__, __LINE__, __FUNCTION__, "error", fmt, ##__VA_ARGS__)

//#define LEX_DEBUG
#ifdef LEX_DEBUG
#define PrDbg(fmt, ...)  Pr(__FILE__, __LINE__, __FUNCTION__, "debug", fmt, ##__VA_ARGS__)
#else
#define PrDbg(fmt, ...)
#endif

/*词法单元模式匹配器*/
typedef struct {
    struct list_head node;
    RegEx *re;      /*正则表达式分析器*/
    TokenTag tag;   /*词法单元tag*/
} LexPattern;

/*
 * 功能：分配词法单元模式匹配器
 * 返回值：
 **/
static LexPattern *LexPatternAlloc(void)
{
    LexPattern *pattern;

    pattern = CplAlloc(sizeof (*pattern));
    return pattern;
}

/*
 * 功能：释放词法单元模式匹配器。
 * 返回值：
 **/
static void LexPatternFree(LexPattern *pattern)
{
    CplFree(pattern);
}

/*
 * 功能：向词法分析器添加一个词法单元模式匹配器。
 * lex: 词法分析器
 * patterStr: 模式字符串
 * tag: 令牌标签
 * 返回值：
 **/
static int LexAddPattern(Lex *lex, const char *patterStr, TokenTag tag)
{
    int error = 0;
    LexPattern *lexPattern;

    if (!lex || !patterStr)
        return -EINVAL;

    lexPattern = LexPatternAlloc();
    if (!lexPattern) {
        PrErr("no memory\n");
        exit(-1);
    }
    lexPattern->tag = tag;
    error = RegExGenerate(patterStr, &lexPattern->re);
    if (error != -ENOERR) {
        PrErr("failure %s regex do not generate, error: %d\n", patterStr, error);
        exit(-1);
    }
    list_add_tail(&lexPattern->node, &lex->patterList);
    return 0;
}

/*
 * 功能：词法分析器初始化
 * 返回值：
 **/
static inline void LexInit(Lex *lex)
{
#if 0
    const char *idPattern = "(_|a|b|c|d|e|f|g|h|i|j|k|l|m|n|o|p|q|r|s|t|u|v|w|x|y|z"
                            "|A|B|C|D|E|F|G|H|I|J|K|L|M|N|O|P|Q|R|S|T|U|V|W|X|Y|Z)"
                            "(_|a|b|c|d|e|f|g|h|i|j|k|l|m|n|o|p|q|r|s|t|u|v|w|x|y|z"
                            "|A|B|C|D|E|F|G|H|I|J|K|L|M|N|O|P|Q|R|S|T|U|V|W|X|Y|Z"
                            "|0|1|2|3|4|5|6|7|8|9)*";
    const char *strPattern = "\"(_|a|b|c|d|e|f|g|h|i|j|k|l|m|n|o|p|q|r|s|t|u|v|w|x|y|z"
                            ")"
                            "(_|a|b|c|d|e|f|g|h|i|j|k|l|m|n|o|p|q|r|s|t|u|v|w|x|y|z"
                            ""
                            ")*\"";
#else
    const char *idPattern = "(_|a|b|c|d|e|f|g|h|i|j|k|l|m|n|o|p|q|r|s|t|u|v|w|x|y|z"
                            ")"
                            "(_|a|b|c|d|e|f|g|h|i|j|k|l|m|n|o|p|q|r|s|t|u|v|w|x|y|z"
                            ""
                            "|0|1|2|3|4|5|6|7|8|9)*";

    const char *strPattern = "\"abc\"";
#endif

    const char *digitPattern = "(0|1|2|3|4|5|6|7|8|9)(0|1|2|3|4|5|6|7|8|9)*";

    INIT_LIST_HEAD(&lex->patterList);
    lex->reader.arg = NULL;
    lex->reader.getChar = NULL;
    lex->eofToken.tag = TT_EOF;
    lex->curToken = NULL;
    LexWordsInit(lex, &lex->words);

    LexAddPattern(lex, "break", TT_BREAK);
    LexAddPattern(lex, "do", TT_DO);
    LexAddPattern(lex, "else", TT_ELSE);
    LexAddPattern(lex, "false", TT_FALSE);
    LexAddPattern(lex, "if", TT_IF);
    LexAddPattern(lex, "true", TT_TRUE);
    LexAddPattern(lex, "for", TT_FOR);
    LexAddPattern(lex, "while", TT_WHILE);
    LexAddPattern(lex, "return", TT_RETURN);
    LexAddPattern(lex, "struct", TT_STRUCT);
    LexAddPattern(lex, "int", TT_INT);
    LexAddPattern(lex, "short", TT_SHORT);
    LexAddPattern(lex, "char", TT_CHAR);
    LexAddPattern(lex, "bool", TT_BOOL);
    LexAddPattern(lex, "float", TT_FLOAT);
    LexAddPattern(lex, "switch", TT_SWITCH);
    LexAddPattern(lex, "case", TT_CASE);
    LexAddPattern(lex, "default", TT_DEFAULT);
    LexAddPattern(lex, "continue", TT_CONTINUE);
    LexAddPattern(lex, "void", TT_VOID);

    LexAddPattern(lex, idPattern, TT_ID);
    LexAddPattern(lex, digitPattern, TT_DIGIT);
    LexAddPattern(lex, strPattern, TT_STR);

    LexAddPattern(lex, "{", TT_LBRACE);
    LexAddPattern(lex, "}", TT_RBRACE);
    LexAddPattern(lex, "\\(", TT_LBRACKET);
    LexAddPattern(lex, "\\)", TT_RBRACKET);
    LexAddPattern(lex, "\\[", TT_LSBRACKET);
    LexAddPattern(lex, "\\]", TT_RSBRACKET);

    LexAddPattern(lex, ";", TT_SEMI_COLON);
    LexAddPattern(lex, ":", TT_COLON);
    LexAddPattern(lex, "#", TT_SHARP);
    LexAddPattern(lex, "\\.", TT_DOT);
    LexAddPattern(lex, ",", TT_COMMA);

    LexAddPattern(lex, "\\->", TT_ARROW);

    LexAddPattern(lex, "\\+", TT_ADD);
    LexAddPattern(lex, "\\-", TT_SUB);
    LexAddPattern(lex, "\\*", TT_MUL);
    LexAddPattern(lex, "/", TT_DIV);
    LexAddPattern(lex, "%", TT_MOD);

    LexAddPattern(lex, "==", TT_EQ);
    LexAddPattern(lex, "!=", TT_NE);
    LexAddPattern(lex, "<=", TT_LE);
    LexAddPattern(lex, "<", TT_LT);
    LexAddPattern(lex, ">=", TT_GE);
    LexAddPattern(lex, ">", TT_GT);

    LexAddPattern(lex, "=", TT_ASSIGN);

    LexAddPattern(lex, "&&", TT_AND);
    LexAddPattern(lex, "\\|\\|", TT_OR);
    LexAddPattern(lex, "!", TT_NOT);

    LexAddPattern(lex, "&", TT_BAND);
    LexAddPattern(lex, "\\|", TT_BOR);
    LexAddPattern(lex, "^", TT_BNOR);

    LexAddPattern(lex, " ", TT_SPACE);
    LexAddPattern(lex, "\\t", TT_TAB);
    LexAddPattern(lex, "\\r", TT_CR);
    LexAddPattern(lex, "\\n", TT_LN);
}

/*
 * 功能：分配词法分析器。
 * 返回值：
 **/
Lex *LexAlloc(void)
{
    Lex *lex;

    lex = CplAlloc(sizeof (*lex));
    if (lex) {
        LexInit(lex);
    } else {
        PrErr("lex alloc fail\n");
        exit(-1);
    }
    return lex;
}

/*
 * 功能：释放词法分析器
 * 返回值：
 **/
void LexFree(Lex *lex)
{
    if (!lex)
        return;

    if (lex->reader.arg) {
        fclose(lex->reader.arg);
        lex->reader.arg = NULL;
    }

    struct list_head *pos;
    LexPattern *pattern;

    for (pos = lex->patterList.next; pos != &lex->patterList; ) {
        pattern = container_of(pos, LexPattern, node);
        pos = pos->next;
        list_del(&pattern->node);
        LexPatternFree(pattern);
    }
}

/*
 * 功能：获取读取器中的当前字符。
 * 返回值：
 **/
static int ReaderCurrentChar(struct _RegExReader *reader)
{
    int ch;

    if (reader->arg) {
        ch = getc(reader->arg);
        if (ch >= 0) {
            ungetc(ch, reader->arg);
            return ch;
        } else
            return -EEOF;
    } else {
        return -EEOF;
    }
}

/*
 * 功能：读取器位置前进一步。
 * 返回值：
 **/
static int ReaderIncPos(struct _RegExReader *reader)
{
    int ch;

    if (reader->arg) {
        ch = getc(reader->arg);
        if (ch >= 0)
            return ch;
        else
            return -EEOF;
    } else {
        return -EEOF;
    }
}

/*
 * 功能：获取读取器当前字符并且位置前进一步
 * 返回值：
 **/
static int ReaderGetChar(struct _RegExReader *reader)
{
    int ch;

    if (reader->arg) {
        ch = getc(reader->arg);
        if (ch >= 0)
            return ch;
        else
            return -EEOF;
    } else {
        return -EEOF;
    }
}

/*
 * 功能：设置词法分析器的读取文件
 * 返回值：
 **/
int LexSetInFile(Lex *lex, const char *inFilename)
{
    FILE *fp;

    if (!lex || !inFilename)
        return -EINVAL;

    if (lex->reader.arg) {
        fclose(lex->reader.arg);
    }
    fp = fopen(inFilename, "rb");
    if (fp) {
        lex->reader.arg = fp;
        lex->reader.getChar = ReaderGetChar;
        lex->reader.currentChar = ReaderCurrentChar;
        lex->reader.incPos = ReaderIncPos;
        return 0;
    } else {
        PrErr("lex set in file fail: %d\n", -EIO);
        exit(-1);
    }
}

/*
 * 功能：寻找跟模式匹配的输入
 * 返回值：
 **/
static int _LexScan(Lex *lex, LexPattern *pattern, const Token **pToken)
{
    int error = 0;
    CplString *matchString;
    const Token *token;

    error = RegExMostScanReader(pattern->re, &lex->reader, &matchString);
    if (error == 0) {
        token = LexWordsGet(lex, matchString, pattern->tag);
        *pToken = token;
        return 0;
    } else if (error == -ENOSTATE
               || error == -EEOF) {
        return error;
    } else {
        PrErr("lex scan error\n");
        if (lex->reader.arg) {
            fclose(lex->reader.arg);
            lex->reader.arg = NULL;
        }
        exit(-1);
    }
}

/*
 * 功能：词法分析器扫描输入，并返回词法单元对应的令牌
 * 返回值：
 **/
const Token *LexScan(Lex *lex)
{
    int error = 0;
    const Token *token;
    struct list_head *pos;
    LexPattern *pattern;
    long seek = 0;

    if (!lex)
        return NULL;

    list_for_each(pos, &lex->patterList) {
        pattern = container_of(pos, LexPattern, node);
        seek = ftell(lex->reader.arg);
        error = _LexScan(lex, pattern, &token);
        if (error == 0) {
            lex->curToken = token;
            return token;
        } else {
            fseek(lex->reader.arg, seek, SEEK_SET);
        }
    }

    seek = ftell(lex->reader.arg);
    if (lex->reader.getChar(&lex->reader) == -EEOF) {
        if (lex->reader.arg) {
            fclose(lex->reader.arg);
            lex->reader.arg = NULL;
        }
        PrDbg("end of file\n");
        lex->curToken = &lex->eofToken;
        return &lex->eofToken;
    } else {
        int ch;

        fseek(lex->reader.arg, seek, SEEK_SET);
        ch = lex->reader.getChar(&lex->reader);
        PrErr("unscan code [%d], %c\n", ch, ch);
    }

    exit(-1);
    return NULL;
}

/*
 * 功能：词法分析器当前的令牌
 * 返回值：
 **/
const Token *LexCurrent(Lex *lex)
{
    if (lex && lex->curToken) {
        PrDbg("(%d)\n", lex->curToken->tag);
        if (lex->curToken->tag != TT_DIGIT
                && lex->curToken->tag != TT_EOF) {
            PrDbg("%s\n", lex->curToken->cplString->str);
        } else if (lex->curToken->tag == TT_DIGIT) {
            PrDbg("%d\n", lex->curToken->digit);
        } else {
            PrDbg("eof\n");
        }
        return lex->curToken;
    }
    PrErr("lex token null\n");
    exit(-1);
}

/*
 * 功能：simple test
 * 返回值：
 **/
void LexTest(void)
{
    Lex *lex;
    const char *filename = "test.txt";

    lex = LexAlloc();
    LexSetInFile(lex, filename);

    const Token *token;

    while (1) {
        token = LexScan(lex);
        if (token) {
            if (token->tag == TT_EOF) {
                break;
            }
            switch (token->tag) {
            case TT_SPACE:
            case TT_TAB:
            case TT_CR:
            case TT_LN:
                continue;
            default:;
            }
            if (token->tag != TT_DIGIT)
                printf("match %d, %s\n", token->tag, token->cplString->str);
            else
                printf("match %d, %d\n", token->tag, token->digit);
        }
    }

    LexFree(lex);
}

