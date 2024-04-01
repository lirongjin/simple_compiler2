#include <stdio.h>
#include "bison.h"
#include "gen_code.h"

int main()
{
    BisonExec();
    GCGenCode(bison->record);
    return 0;
}
