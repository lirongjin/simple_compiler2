#ifndef __CPL_STRING_H__
#define __CPL_STRING_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char *str;
    unsigned int idx;
    unsigned int size;
} CplString;

CplString *CplStringAlloc(void);
void CplStringFree(CplString *cplString);
int CplStringAppendCh(CplString *reString, const char ch);
int CplStringAppendString(CplString *cplString, const char *str);
void CplStringDel(CplString *reString, unsigned int len);


#ifdef __cplusplus
}
#endif

#endif /*__CPL_STRING_H__*/
