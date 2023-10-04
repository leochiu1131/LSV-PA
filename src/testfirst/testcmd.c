#include "base/main/main.h"
#include "testfirst.h"

ABC_NAMESPACE_HEADER_START

extern int TestFirst_CommandTestFirst(Abc_Frame_t * pAbc, int argc, int ** argv);

void TestFirst_Init(Abc_Frame_t * pAbc) {
    Cmd_CommandAdd(pAbc, "Various", "firstcmd", TestFirst_CommandTestFirst, 0);
}

int TestFirst_CommandTestFirst(Abc_Frame_t * pAbc, int argc, int ** argv) {
    int fVerbose;
    int c, result;

    fVerbose = 0;

    Extra_UtilGetoptReset();
    while ((c = Extra_UtilGetopt(argc, argv, "vh")) != EOF) {
        switch (c)
        {
        case 'v':
            fVerbose ^= 1;
            break;
        case 'h':
            goto usage;
        default:
            goto usage;
        }
    }
    
    result = TestFirst_FirstFunctionAbc(pAbc);

    if(fVerbose) {
        Abc_Print(1, "\nVerbose mode is on.\n");
        if(result)
            Abc_Print(1, "The command finished successfully.\n");
        else
            Abc_Print(1, "The command failed.\n");
    }

    return 0;

usage:
    Abc_Print(-2, "usage: firstcmd [-vh] \n");
    Abc_Print(-2, "\t -v: verbose info\n");
    Abc_Print(-2, "\t -h: help\n");
    return 1;
}

ABC_NAMESPACE_HEADER_END