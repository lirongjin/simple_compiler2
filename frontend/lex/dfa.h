#ifndef __DFA_H__
#define __DFA_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _Dfa Dfa;
typedef struct _DfaState DfaState;

int DfaGenerate(const char *regExStr, Dfa **pDfa);
void DfaFree(Dfa *dfa);

DfaState *DfaGetState(const Dfa *dfa, int id);
DfaState *DfaMoveState(Dfa *dfa, DfaState *state, char ch);
int DfaIsFinishState(Dfa *dfa, DfaState *state);

#ifdef __cplusplus
}
#endif

#endif /*__DFA_H__*/
