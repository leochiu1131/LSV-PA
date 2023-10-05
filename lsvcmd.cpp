#include "base/main/main.h"
#include "bdd/extrab/extraBdd.h"
#include "bdd/cudd/cudd.h"

static int Lsv_CommandSimulateBdd(Abc_Frame_t *pAbc, int argc, char **argv);
static int Lsv_CommandSimulateAig(Abc_Frame_t *pAbc, int argc, char **argv);

void Lsv_InitSimulateBdd(Abc_Frame_t *pAbc)
{
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", Lsv_CommandSimulateBdd, 0);
}

void Lsv_InitSimulateAig(Abc_Frame_t *pAbc)
{
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig", Lsv_CommandSimulateBdd, 0);
}

void Lsv_EndSimulateBdd(Abc_Frame_t *pAbc) {}

int Lsv_CommandSimulateBdd(Abc_Frame_t *pAbc, int argc, char **argv)
{
    if (argc != 2)
    {
        Abc_Print(-1, "Usage: lsv_sim_bdd <input_pattern>\n");
        return 0;
    }

    Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
    if (!pNtk)
    {
        Abc_Print(-1, "No network loaded.\n");
        return 0;
    }

    char *inputPattern = argv[1];
    int inputPatternLength = strlen(inputPattern);

    // Ensure that the network has been transformed into a BDD
    if (!Abc_NtkIsBddLogic(pNtk))
    {
        Abc_Print(-1, "Network has not been transformed into a BDD.\n");
        return 0;
    }

    // Get the variable order from the BDD manager
    DdManager *dd = (DdManager *)pNtk->pManFunc;
    int *vPermIn = Cudd_ReadInvPerm(dd, 0);

    // Create a BDD for the input pattern
    DdNode *inputBdd = Cudd_ReadLogicZero(dd);
    for (int i = 0; i < inputPatternLength; i++)
    {
        if (inputPattern[i] == '1')
        {
            DdNode *varBdd = Cudd_bddIthVar(dd, vPermIn[i]);
            inputBdd = Cudd_bddAnd(dd, inputBdd, varBdd);
        }
        else if (inputPattern[i] == '0')
        {
            DdNode *varBdd = Cudd_bddIthVar(dd, vPermIn[i]);
            inputBdd = Cudd_bddAnd(dd, inputBdd, Cudd_Not(varBdd));
        }
    } 

    // Iterate through primary outputs and simulate
    Abc_Obj_t *pPo;
    int i = 0;

    Abc_NtkForEachPo(pNtk, pPo, i)
    {
        // Get the BDD for the PO
        DdNode *poBdd = (DdNode *)pPo->pData;

        // Simulate by taking the cofactor and comparing to 1
        DdNode *cofactorBdd = Cudd_Cofactor(dd, poBdd, inputBdd);
        int result = Cudd_bddLeq(dd, cofactorBdd, Cudd_ReadOne(dd));

        // Print the PO name and result
        Abc_Print(1, "%s: %d\n", Abc_ObjName(pPo), result);

        // Clean up cofactorBdd
        Cudd_RecursiveDeref(dd, cofactorBdd);
    }

    // Clean up inputBdd
    Cudd_RecursiveDeref(dd, inputBdd);

    return 0; // Add a return statement at the end
}

int Lsv_CommandSimulateAig(Abc_Frame_t *pAbc, int argc, char **argv)