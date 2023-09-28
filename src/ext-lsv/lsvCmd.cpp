#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "bdd/extrab/extraBdd.h"

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimBdd(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
    Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", Lsv_CommandSimBdd, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = { init, destroy };

struct PackageRegistrationManager {
    PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

void Lsv_NtkPrintNodes(Abc_Ntk_t* pNtk) {
    Abc_Obj_t* pObj;
    int i;
    Abc_NtkForEachNode(pNtk, pObj, i) {
        printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
        Abc_Obj_t* pFanin;
        int j;
        Abc_ObjForEachFanin(pObj, pFanin, j) {
            printf("  Fanin-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanin),
                Abc_ObjName(pFanin));
        }
        if (Abc_NtkHasSop(pNtk)) {
            printf("The SOP of this node:\n%s", (char*)pObj->pData);
        }
    }
}

void Lsv_NtkSimBdd(Abc_Ntk_t* pNtk, char* pattern) {
  
    if (!pNtk) {
        Abc_Print(-1, "Empty network.\n");
        return;
    }

    // Check if the network is a BDD network
    if (!Abc_NtkIsBddLogic(pNtk)) {
        Abc_Print(-1, "The current network is not a BDD network.\n");
        return;
    }

    // Iterate through each primary output
    Abc_Obj_t* pPo;
    int ithPo = 0;

    Abc_NtkForEachPo(pNtk, pPo, ithPo) 
    {
        char* poName = Abc_ObjName(pPo);

        // Get the associated BDD manager and node for the current PO
        Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo);
        assert(Abc_NtkIsBddLogic(pRoot->pNtk));
        DdManager* dd = (DdManager*)pRoot->pNtk->pManFunc;
        DdNode* poBdd = (DdNode*)pRoot->pData;

        // Find the variable order of the BDD
        char** vNamesIn = (char**)Abc_NodeGetFaninNames(pRoot)->pArray;

        // Parse the input pattern (e.g., "001" -> {0, 0, 1})
        int patternLength = strlen(pattern);
        if (patternLength != Abc_NtkPiNum(pNtk)) {
            Abc_Print(-1, "Input pattern length does not match the number of primary inputs.\n");
            return;
        }

        int* inputPattern = ABC_ALLOC(int, patternLength);
        for (int i = 0; i < patternLength; i++) {
            if (pattern[i] == '0') {
                inputPattern[i] = 0;
            }
            else if (pattern[i] == '1') {
                inputPattern[i] = 1;
            }
            else {
                Abc_Print(-1, "Invalid character in input pattern: %c\n", pattern[i]);
                ABC_FREE(inputPattern);
                return;
            }
        }

        // Initialize result BDD with PO BDD
        DdNode* resultBdd = poBdd;
        Cudd_Ref(resultBdd); // Reference the result BDD

        // Iterate through each input variable using Abc_NtkForEachCi
        Abc_Obj_t* pCi;
        int ciIdx = 0;

        Abc_NtkForEachCi(pNtk, pCi, ciIdx) 
        {
            // Get the corresponding input index based on variable name
            char* ciName = Abc_ObjName(pCi);

            // Find the matching input variable by name
            int inputIdx = -1;

            Abc_NtkForEachCi(pNtk, pCi, inputIdx) {
                if (strcmp(vNamesIn[ciIdx], ciName) == 0) {
                    inputIdx = ciIdx;
                    break;
                }
            }

            if (inputIdx == -1) {
                Abc_Print(-1, "Variable name %s not found in primary inputs.\n", ciName);
                ABC_FREE(inputPattern);
                Cudd_RecursiveDeref(dd, resultBdd);
                return;
            }

            // Create a BDD variable for the input
            DdNode* varBdd = Cudd_bddIthVar(dd, inputIdx);

            // Compute the cofactor based on the input pattern
            DdNode* cofactor = Cudd_Cofactor(dd, resultBdd, inputPattern[inputIdx] ? varBdd : Cudd_Not(varBdd));

            // Dereference the previous result BDD and replace it with the cofactor
            Cudd_RecursiveDeref(dd, resultBdd);
            resultBdd = cofactor;
            Cudd_Ref(resultBdd); // Reference the new result BDD
        }

        // Determine the result value
        int result = (resultBdd == Cudd_ReadOne(dd)) ? 1 : 0;

        // Print the PO name and result
        Abc_Print(1, "%s: %d\n", poName, result);

        // Cleanup the result BDD
        Cudd_RecursiveDeref(dd, resultBdd);
        ABC_FREE(inputPattern);
    }
}

int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv) {
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    int c;
    Extra_UtilGetoptReset();
    while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
        switch (c) {
        case 'h':
            goto usage;
        default:
            goto usage;
        }
    }
    if (!pNtk) {
        Abc_Print(-1, "Empty network.\n");
        return 1;
    }
    Lsv_NtkPrintNodes(pNtk);
    return 0;

usage:
    Abc_Print(-2, "usage: lsv_print_nodes [-h]\n");
    Abc_Print(-2, "\t        prints the nodes in the network\n");
    Abc_Print(-2, "\t-h    : print the command usage\n");
    return 1;
}

int Lsv_CommandSimBdd(Abc_Frame_t* pAbc, int argc, char** argv) {
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    int c;
    Extra_UtilGetoptReset();
    while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
        switch (c) {
        case 'h':
            goto usage;
        default:
            goto usage;
        }
    }
    if (!pNtk) {
        Abc_Print(-1, "Empty network.\n");
        return 1;
    }
    Lsv_NtkSimBdd(pNtk, argv[1]);
    return 0;

usage:
    Abc_Print(-2, "usage: lsv_sim_bdd [-h]\n");
    Abc_Print(-2, "\t        simulates for a given BDD and an input pattern\n");
    Abc_Print(-2, "\t-h    : print the command usage\n");
    return 1;
}
