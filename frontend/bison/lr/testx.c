#include "context_free_grammar.h"
#include "lalr_parse_table.h"
#include "error_recover.h"
#include "lr_parse_algorithm.h"
#include "list.h"
#include "cpl_common.h"
#include "cpl_debug.h"
#include "cpl_errno.h"
#include "stdlib.h"
#include "string.h"
#include "lex.h"

#define SYMBOL_ID_EMPTY         0      /*empty string*/
#define SYMBOL_ID_END           1      /*'$'*/
#define SYMBOL_ID_STARTx        2      /*增广文法开始符号*/

#define SYMBOL_ID_S             3       /*语句*/
#define SYMBOL_ID_C             4       /*条件表达式*/

#define SYMBOL_ID_while         5       /*while*/
#define SYMBOL_ID_c             6       /*条件表达式中的条件*/
#define SYMBOL_ID_SEMI          7       /*;*/
#define SYMBOL_ID_LBRAC         8       /*(*/
#define SYMBOL_ID_RBRAC         9       /*)*/
#define SYMBOL_ID_id            10      /*id*/

#define SYMBOL_ID_M             11      /**/
#define SYMBOL_ID_N             12      /**/
#define SYMBOL_ID_PROGRAM       13      /*program*/
#define SYMBOL_ID_M2            14      /**/

#define SYMBOL_ID_E             15      /*E*/
#define SYMBOL_ID_T             16      /*T*/
#define SYMBOL_ID_F             17      /*F*/

#define SYMBOL_ID_ADD           18      /*+*/
#define SYMBOL_ID_SUB           19      /*-*/
#define SYMBOL_ID_MUL           20      /***/
#define SYMBOL_ID_DIV           21      /*/*/
#define SYMBOL_ID_MOD           22      /*%*/

#define SYMBOL_ID_digit         23     /*digit*/

#define SYMBOL_ID_assign        24     /*assign*/
#define SYMBOL_ID_assign_Sym    25     /*=*/

#define SYMBOL_ID_START         SYMBOL_ID_STARTx   /*上下文无关文法第一个符号。*/

typedef struct _Node Node;

typedef struct {
    Node *e;
} NodeL;

typedef struct {
    Node *e;
    Node *t;
} NodeE;

typedef struct {
    Node *t;
    Node *f;
} NodeT;

typedef struct {
    Node *assign;
    Node *e;
} NodeAssign;

typedef struct {
    int val;
} NodeDigit;

typedef struct {
    const CplString *str;
} NodeId;

typedef enum {
    NT_L,
    NT_ADD,
    NT_SUB,
    NT_MUL,
    NT_DIV,
    NT_MOD,
    NT_DIGIT,
    NT_ID,
    NT_ASSIGN,
} NodeType;

typedef struct _Node {
    NodeType type;
    union {
        NodeL l;
        NodeE e;
        NodeT t;
        NodeDigit digit;
        NodeId id;
        NodeAssign assign;
    };
} Node;

Node *AllocNode(NodeType type)
{
    Node *node;

    node = malloc(sizeof (*node));
    if (node) {
        node->type = type;
    } else {
        printf("alloc node error, no memory\n");
        exit(-1);
    }
    return node;
}

typedef struct {
    const char *next;
} M2Record;

typedef struct {
    const char *trueLabel;
    const char *falseLabel;
    const char *l1;
    const char *l2;
} MRecord;

typedef struct {
    const char *next;
} NRecord;

typedef struct {
    char ch;
} cRecord;

typedef struct {
    const CplString *str;
} idRecord;

typedef struct {
    int digit;
} digitRecord;

typedef struct {
    const char *code;
} CRecord;

typedef struct {
    const char *code;
} SRecord;

static int LrShiftHandle(void **pNode, LrReader *reader)
{
    int curSymId;

    *pNode = NULL;

    curSymId = reader->getCurrentSymId(reader);
    if (curSymId == SYMBOL_ID_id) {
        Node *idNode;
        const Token *token;

        idNode = AllocNode(NT_ID);
        token = LexCurrent(reader->arg);
        idNode->id.str = token->cplString;
        *pNode = idNode;

        printf("%s", token->cplString->str);
    } else if (curSymId == SYMBOL_ID_digit) {
        Node *digitNode;
        const Token *token;

        digitNode = AllocNode(NT_DIGIT);
        token = LexCurrent(reader->arg);
        digitNode->digit.val = token->digit;
        *pNode = digitNode;

        printf("%d", token->digit);
    } else if (curSymId == SYMBOL_ID_c) {

    } else {
        *pNode = NULL;
    }
    return 0;
}

static void PreorderTraversal(Node *node);

static void PreorderTraversalL(NodeL *l)
{
    PreorderTraversal(l->e);
}

static void PreorderTraversalAdd(NodeE *e)
{
    PreorderTraversal(e->e);
    PreorderTraversal(e->t);
    printf("+ ");
}

static void PreorderTraversalSub(NodeE *e)
{
    PreorderTraversal(e->e);
    PreorderTraversal(e->t);
    printf("- ");
}

static void PreorderTraversalMul(NodeT *t)
{
    PreorderTraversal(t->t);
    PreorderTraversal(t->f);
    printf("* ");
}

static void PreorderTraversalDiv(NodeT *t)
{
    PreorderTraversal(t->t);
    PreorderTraversal(t->f);
    printf("/ ");
}

static void PreorderTraversalMod(NodeT *t)
{
    PreorderTraversal(t->t);
    PreorderTraversal(t->f);
    printf("%s ", "%");
}

static void PreorderTraversalDigit(NodeDigit *digit)
{
    printf("%d ", digit->val);
}

static void PreorderTraversalId(NodeId *id)
{
    printf("%s ", id->str->str);
}

static void PreorderTraversalAssign(NodeAssign *assign)
{
    PreorderTraversal(assign->assign);
    PreorderTraversal(assign->e);
    printf("= ");
}

static void PreorderTraversal(Node *node)
{
    switch (node->type) {
    case NT_L:
        return PreorderTraversalL(&node->l);
    case NT_ADD:
        return PreorderTraversalAdd(&node->e);
    case NT_SUB:
        return PreorderTraversalSub(&node->e);
    case NT_MUL:
        return PreorderTraversalMul(&node->t);
    case NT_DIV:
        return PreorderTraversalDiv(&node->t);
    case NT_MOD:
        return PreorderTraversalMod(&node->t);
    case NT_DIGIT:
        return PreorderTraversalDigit(&node->digit);
    case NT_ID:
        return PreorderTraversalId(&node->id);
    case NT_ASSIGN:
        return PreorderTraversalAssign(&node->assign);
    }
}

static int TestCurrentSymId(LrReader *reader)
{
    int symId = -1;
    const Token *token;
    Lex *lex = reader->arg;

    token = LexCurrent(lex);
    switch (token->tag) {
    case TT_ID:
        symId = SYMBOL_ID_id;
        break;
    /*case TT_ID:
        symId = SYMBOL_ID_c;
        break;*/
    case TT_SEMI_COLON:
        symId = SYMBOL_ID_SEMI;
        break;
    case TT_LBRACKET:
        symId = SYMBOL_ID_LBRAC;
        break;
    case TT_RBRACKET:
        symId = SYMBOL_ID_RBRAC;
        break;
    case TT_ADD:
        symId = SYMBOL_ID_ADD;
        break;
    case TT_SUB:
        symId = SYMBOL_ID_SUB;
        break;
    case TT_MUL:
        symId = SYMBOL_ID_MUL;
        break;
    case TT_DIV:
        symId = SYMBOL_ID_DIV;
        break;
    case TT_MOD:
        symId = SYMBOL_ID_MOD;
        break;
    case TT_ASSIGN:
        symId = SYMBOL_ID_assign_Sym;
        break;
    case TT_EOF:
        symId = SYMBOL_ID_END;
        /*printf("-------end----------\n");
        fflush(stdout);
        exit(-1);*/
        break;
    default:
        break;
    }

    return symId;
}

static int TestIncInputPos(LrReader *reader)
{
    Lex *lex = reader->arg;

    LexScan(lex);
    return 0;
}

static Node *root = NULL;

/*Sx -> S*/
static int PbSxsHandle(void **headArg, void *bodyPrevArg, void *bodyArg[])
{
    (void)headArg, (void)bodyArg, (void)bodyPrevArg;
    root = bodyArg[0];
    printf("---------ROOT-----------\n");
    PreorderTraversal(root);
    return 0;
}

/*S -> assign ;*/
static int PbSasHandle(void **headArg, void *bodyPrevArg, void *bodyArg[])
{
    (void)bodyPrevArg;
    *headArg = bodyArg[0];

    printf(";");
    return 0;
}

/*assign -> assign = E */
static int PbAassHandle(void **headArg, void *bodyPrevArg, void *bodyArg[])
{
    Node *node;

    (void)bodyPrevArg;
    node = AllocNode(NT_ASSIGN);
    node->assign.assign = bodyArg[0];
    node->assign.e = bodyArg[2];
    *headArg = node;

    printf("=");
    return 0;
}

/*assign -> E*/
static int PbAeHandle(void **headArg, void *bodyPrevArg, void *bodyArg[])
{
    (void)bodyPrevArg;
    *headArg = bodyArg[0];
    return 0;
}

/*E -> E + T */
static int PbEeatHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Node *node;

    (void)prevArg;
    node = AllocNode(NT_ADD);
    node->e.e = bodyArg[0];
    node->e.t = bodyArg[2];
    *headArg = node;

    printf("+");
    return 0;
}

/*E -> E - T*/
static int PbEestHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Node *node;

    (void)prevArg;
    node = AllocNode(NT_SUB);
    node->e.e = bodyArg[0];
    node->e.t = bodyArg[2];
    *headArg = node;

    printf("-");
    return 0;
}

/*E -> T*/
static int PbEtHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    (void)prevArg;
    *headArg = bodyArg[0];
    return 0;
}

/*T -> T * F*/
static int PbTtmfHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Node *node;

    (void)prevArg;
    node = AllocNode(NT_MUL);
    node->t.t = bodyArg[0];
    node->t.f = bodyArg[2];
    *headArg = node;

    printf("*");
    return 0;
}

/*T -> T / F*/
static int PbTtdfHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Node *node;

    (void)prevArg;
    node = AllocNode(NT_DIV);
    node->t.t = bodyArg[0];
    node->t.f = bodyArg[2];
    *headArg = node;

    printf("/");
    return 0;
}

/*T -> T % F*/
static int PbTtofHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    Node *node;

    (void)prevArg;
    node = AllocNode(NT_MOD);
    node->t.t = bodyArg[0];
    node->t.f = bodyArg[2];
    *headArg = node;

    printf("%%");
    return 0;
}

/*T -> F*/
static int PbTfHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    (void)prevArg;
    *headArg = bodyArg[0];
    return 0;
}

/*F -> ( E ) */
static int PbFlerHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    (void)prevArg;
    *headArg = bodyArg[1];
    return 0;
}

/*F -> id */
static int PbFidHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    (void)prevArg;
    *headArg = bodyArg[0];
    return 0;
}

/*F -> digit*/
static int PbFdigitHandle(void **headArg, void *prevArg, void *bodyArg[])
{
    (void)prevArg;
    *headArg = bodyArg[0];
    return 0;
}

static int DesignGrammar(ContextFreeGrammar **pGrammar)
{
    int error = 0;
    ContextFreeGrammar *grammar;

    error = ContextFreeGrammarAlloc(&grammar, SYMBOL_ID_EMPTY, SYMBOL_ID_END, SYMBOL_ID_START);
    if (error != -ENOERR) {
        return error;
    }
    *pGrammar = grammar;
    error = CfgAddSymbol(grammar, SYMBOL_ID_STARTx, "S'", SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SYMBOL_ID_EMPTY, "empty", SYMBOL_TYPE_EMPTY);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SYMBOL_ID_END, "$",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SYMBOL_ID_PROGRAM, "P",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SYMBOL_ID_S, "S",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SYMBOL_ID_C, "C",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SYMBOL_ID_while, "while",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SYMBOL_ID_c, "c",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SYMBOL_ID_SEMI, ";",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SYMBOL_ID_LBRAC, "(",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SYMBOL_ID_RBRAC, ")",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SYMBOL_ID_id, "id",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SYMBOL_ID_M, "M",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SYMBOL_ID_N, "N",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SYMBOL_ID_M2, "M2",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SYMBOL_ID_E, "E",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SYMBOL_ID_T, "T",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SYMBOL_ID_F, "F",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SYMBOL_ID_ADD, "+",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SYMBOL_ID_SUB, "-",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SYMBOL_ID_MUL, "*",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SYMBOL_ID_DIV, "/",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SYMBOL_ID_MOD, "%",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SYMBOL_ID_digit, "digit",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SYMBOL_ID_assign, "assign",  SYMBOL_TYPE_NONTERMINAL);
    if (error != -ENOERR)
        goto err;
    error = CfgAddSymbol(grammar, SYMBOL_ID_assign_Sym, "=",  SYMBOL_TYPE_TERMINAL);
    if (error != -ENOERR)
        goto err;

    error = CfgAddProductH(grammar, RHT_SELF, PbSxsHandle, SYMID_ARR(SYMBOL_ID_STARTx, SYMBOL_ID_S));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, PbSasHandle, SYMID_ARR(SYMBOL_ID_S, SYMBOL_ID_assign, SYMBOL_ID_SEMI));
    if (error != -ENOERR)
        goto err;

    error = CfgAddProductH(grammar, RHT_SELF, PbAassHandle, SYMID_ARR(SYMBOL_ID_assign, SYMBOL_ID_assign, SYMBOL_ID_assign_Sym, SYMBOL_ID_E));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, PbAeHandle, SYMID_ARR(SYMBOL_ID_assign, SYMBOL_ID_E));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, PbEeatHandle, SYMID_ARR(SYMBOL_ID_E, SYMBOL_ID_E, SYMBOL_ID_ADD, SYMBOL_ID_T));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, PbEestHandle, SYMID_ARR(SYMBOL_ID_E, SYMBOL_ID_E, SYMBOL_ID_SUB, SYMBOL_ID_T));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, PbEtHandle, SYMID_ARR(SYMBOL_ID_E, SYMBOL_ID_T));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, PbTtmfHandle, SYMID_ARR(SYMBOL_ID_T, SYMBOL_ID_T, SYMBOL_ID_MUL, SYMBOL_ID_F));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, PbTtdfHandle, SYMID_ARR(SYMBOL_ID_T, SYMBOL_ID_T, SYMBOL_ID_DIV, SYMBOL_ID_F));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, PbTtofHandle, SYMID_ARR(SYMBOL_ID_T, SYMBOL_ID_T, SYMBOL_ID_MOD, SYMBOL_ID_F));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, PbTfHandle, SYMID_ARR(SYMBOL_ID_T, SYMBOL_ID_F));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, PbFlerHandle, SYMID_ARR(SYMBOL_ID_F, SYMBOL_ID_LBRAC, SYMBOL_ID_E, SYMBOL_ID_RBRAC));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, PbFidHandle, SYMID_ARR(SYMBOL_ID_F, SYMBOL_ID_id));
    if (error != -ENOERR)
        goto err;
    error = CfgAddProductH(grammar, RHT_SELF, PbFdigitHandle, SYMID_ARR(SYMBOL_ID_F, SYMBOL_ID_digit));
    if (error != -ENOERR)
        goto err;

    return 0;
err:
    printf("Design grammar fail.\n");
    ContextFreeGrammarFree(grammar);
    *pGrammar = NULL;
    return error;
}

static int GenLrParseTable(Lalr *lalr)
{
    int error = 0;

    error = LalrGenItemSet(lalr);
    if (error != -ENOERR)
        return error;
    return LalrGenParseTable(lalr);
}

static int GrammarParse(LrParser *lrParser, Lalr *lalr, Lex *lex)
{
    LrReader lrReader;

    lrReader.getCurrentSymId = TestCurrentSymId;
    lrReader.inputPosInc = TestIncInputPos;
    lrReader.arg = lex;

    return LrParse(lrParser, lalr, &lrReader);
}

static int BuildRegExTree(Lex *lex)
{
    int error = 0;
    ContextFreeGrammar *grammar = NULL;
    Lalr *lalr;
    LrErrorRecover *lrErrorRecover;
    LrParser *lrParser;

    error = DesignGrammar(&grammar);
    if (error != -ENOERR)
        goto err0;
    CfgPrintProduct(grammar);
    fflush(stdout);

    error = LalrAlloc(&lalr, grammar, NULL, NULL);
    if (error != -ENOERR)
        goto err1;
    error = GenLrParseTable(lalr);
    if (error != -ENOERR)
        goto err2;
    LalrPrintItemSet(lalr);
    LalrPrintParseTable(lalr);

    error = LrErrorRecoverAlloc(&lrErrorRecover, SYMID_ARR(SYMBOL_ID_S, SYMBOL_ID_C));
    if (error != -ENOERR)
        goto err2;
    error = LrParserAlloc(&lrParser, LrShiftHandle, lrErrorRecover);
    if (error != -ENOERR)
        goto err3;
    error = GrammarParse(lrParser, lalr, lex);
    if (error != -ENOERR)
        goto err4;
    if (lrParser->syntaxErrorFlag == 1)
        error = -ESYNTAX;

    LrParserFree(lrParser);
    LrErrorReocverFree(lrErrorRecover);
    LalrFree(lalr);
    ContextFreeGrammarFree(grammar);
    return error;

err4:
    LrParserFree(lrParser);
err3:
    LrErrorReocverFree(lrErrorRecover);
err2:
    LalrFree(lalr);
err1:
    ContextFreeGrammarFree(grammar);
err0:
    printf("error: %d\n", error);
    return error;
}

void LrLSddSelf(void)
{
    int error = 0;
    Lex *lex;

    lex = LexAlloc();
    LexSetInFile(lex, "test.txt");
    LexScan(lex);

    error = BuildRegExTree(lex);
    if (error == -ENOERR) {
        printf("\n");
    }
}














