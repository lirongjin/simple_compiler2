#ifndef __VIRTUAL_MACHINE_H__
#define __VIRTUAL_MACHINE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"

typedef struct _VMachine VMachine;

VMachine *VMAllocMachine(void);
void VMFreeMachine(VMachine *vm);

int VMachineExec(VMachine *vm, uint32_t instArr[], uint32_t size);

#ifdef __cplusplus
}
#endif


#endif /*__VIRTUAL_MACHINE_H__*/

