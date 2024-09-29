#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
/*
#include "opt/cut/cut.h"
#include "opt/cut/cutInt.h"
#include "opt/cut/cutList.h"
*/
#include "aig/aig/aig.h"

static int Lsv_CommandPrintCut(Abc_Frame_t *pAbc, int argc, char **argv); // command function

void init(Abc_Frame_t *pAbc)
{
    Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_CommandPrintCut, 0);
} // register this new command

void destroy(Abc_Frame_t *pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager
{
    PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

void Lsv_NtkPrintCuts(Abc_Ntk_t *pNtk)
{

    /*Cut_Man_t *pCutMan;
    extern Cut_Man_t *Abc_NtkCuts(Abc_Ntk_t * pNtk, Cut_Params_t * pParams);*/

    Abc_Obj_t *pObj; // nodes object
    int i;
    int j;
    Abc_NtkForEachNode(pNtk, pObj, i)
    {
        /*Cut_Cut_t *pList;
        pList = (Cut_Cut_t *)Abc_NodeReadCuts((Cut_Man_t *)pCutMan, pObj);
        printf("n leaves (%d)", (int)pList->nLeaves);*/
        /*for (j = 0; j < (int))
        {
            printf(" %d", pCut->pLeaves[i] >> CUT_SHIFT);
            if (pCut->pLeaves[i] & CUT_MASK)
                printf("(%d)", pCut->pLeaves[i] & CUT_MASK);
        }*/
    }
}
int Lsv_CommandPrintCut(Abc_Frame_t *pAbc, int argc, char **argv)
{
    Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc); // top level network, storing nodes and connections
    int c;
    Extra_UtilGetoptReset();
    while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) // pase command line option (argv is an array of strings)
    {
        switch (c)
        {
        case 'h':
            goto usage;
        default:
            goto usage;
        }
    }
    if (!pNtk)
    {
        Abc_Print(-1, "Empty network.\n");
        return 1;
    }
    Lsv_NtkPrintCuts(pNtk);
    return 0;

usage:
    Abc_Print(-2, "usage: lsv_print_cuts [-h]\n");
    Abc_Print(-2, "\t        prints the cuts in the network\n");
    Abc_Print(-2, "\t-h    : print the command usage\n");
    return 1;
}
