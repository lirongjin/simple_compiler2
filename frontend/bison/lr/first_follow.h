#ifndef __FIRST_FOLLOW_H__
#define __FIRST_FOLLOW_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "list.h"
#include "sym_id.h"
#include "context_free_grammar.h"

int CfgGenFirstFollow(ContextFreeGrammar *grammar);

int CfgGetSymIdFirst(ContextFreeGrammar *grammar, int symId, struct list_head *list);
int CfgGetSymIdFollow(ContextFreeGrammar *grammar, int symId, struct list_head *list);

int CfgGetSymIdListFirst(ContextFreeGrammar *grammar, const struct list_head *symIdList, struct list_head *firstSet);

void CfgPrintFirstFollow(ContextFreeGrammar *grammar);

#ifdef __cplusplus
}
#endif

#endif
