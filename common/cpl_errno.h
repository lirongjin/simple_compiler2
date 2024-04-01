#ifndef __CPL_ERRNO_H__
#define __CPL_ERRNO_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <errno.h>

#define ENOERR          0   /*�޴���*/
#define ESYNTAX         256   /*�﷨����*/
#define ELAERR          257   /*��ǰ�����Ų�ƥ��*/
#define EILLCH          258   /**/
#define ENOLASTNODE     259   /**/
#define EMISC           260   /*��������*/
#define ENOSTATE        261   /*û�����״̬*/
#define ENOFINISHID     262   /*û�н���״̬*/
#define ENOPOSID        263   /*û�ҵ�λ��*/
#define ENONBODY        264   /*����ʽ��Ϊ��*/
#define ESYMTYPE        265   /*�ķ��������ʹ���*/
#define ENOSYM          266   /*û������ķ�����*/
#define ENMATCHTM       267   /*û���ҵ�ƥ����ս��*/
#define ENMATCHPB       268   /*û���ҵ�ƥ��Ĳ���ʽ�塣*/
#define ENOPDTB         269   /*û���ҵ�ƥ��Ĳ���ʽ��*/
#define ENOITEM         270   /*û�ҵ���е��*/
#define EEOF            271   /*�ļ�ĩβ*/

#ifdef __cplusplus
}
#endif

#endif /*__CPL_ERRNO_H__*/
