#ifndef __CPL_ERRNO_H__
#define __CPL_ERRNO_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <errno.h>

#define ENOERR          0   /*无错误*/
#define ESYNTAX         256   /*语法错误*/
#define ELAERR          257   /*向前看符号不匹配*/
#define EILLCH          258   /**/
#define ENOLASTNODE     259   /**/
#define EMISC           260   /*其它错误*/
#define ENOSTATE        261   /*没有这个状态*/
#define ENOFINISHID     262   /*没有接受状态*/
#define ENOPOSID        263   /*没找到位置*/
#define ENONBODY        264   /*产生式体为空*/
#define ESYMTYPE        265   /*文法符号类型错误*/
#define ENOSYM          266   /*没有这个文法符号*/
#define ENMATCHTM       267   /*没有找到匹配的终结符*/
#define ENMATCHPB       268   /*没有找到匹配的产生式体。*/
#define ENOPDTB         269   /*没有找到匹配的产生式体*/
#define ENOITEM         270   /*没找到项集中的项。*/
#define EEOF            271   /*文件末尾*/

#ifdef __cplusplus
}
#endif

#endif /*__CPL_ERRNO_H__*/
