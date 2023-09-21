#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "bdd/cudd/cudd.h"

#include <string>
#include <fstream>
#include <unordered_map>

#define S_ONE  (~(0))
#define S_ZERO (0)

static int Lsv_CommandPrintNodes(Abc_Frame_t *pAbc, int argc, char **argv);
static int Lsv_CommandSimBdd(Abc_Frame_t *pAbc, int argc, char **argv);
static int Lsv_CommandSimAig(Abc_Frame_t *pAbc, int argc, char **argv);

void init(Abc_Frame_t *pAbc) {
    Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", Lsv_CommandSimBdd, 0);
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig", Lsv_CommandSimAig, 0);
}

void destroy(Abc_Frame_t *pAbc) {
}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
    PackageRegistrationManager() {
        Abc_FrameAddInitializer(&frame_initializer);
    }
} lsvPackageRegistrationManager;

void Lsv_NtkSimAig(Abc_Ntk_t *pNtk, Vec_Ptr_t *pModel_bit) {
    assert(Abc_NtkIsStrash(pNtk));
    assert(Abc_NtkPiNum(pNtk) == Vec_PtrSize(pModel_bit));
    int i, j, v0, v1, nBits, batchSize;
    Abc_Obj_t *pObj;

    int *pModel = ABC_ALLOC(int, Abc_NtkPiNum(pNtk));
    Vec_Ptr_t *pValue = Vec_PtrAlloc(Abc_NtkPoNum(pNtk));
    for (int i = 0; i < Abc_NtkPoNum(pNtk); ++i) {
        Vec_PtrPush(pValue, Vec_IntAlloc(0));
    }

    nBits = Vec_BitSize((Vec_Bit_t *)Vec_PtrEntry(pModel_bit, 0));
    batchSize = (nBits - 1) / 32 + 1;
    for (int b = 0; b < batchSize; ++b) {
        for (i = 0; i < Abc_NtkPiNum(pNtk); ++i) {
            pModel[i] = ((Vec_Bit_t *)Vec_PtrEntry(pModel_bit, i))->pArray[b];
        }

        Abc_AigConst1(pNtk)->iTemp = S_ONE;
        Abc_NtkForEachCi(pNtk, pObj, i) {
            pObj->iTemp = pModel[i];
        }
        Abc_NtkForEachNode(pNtk, pObj, i) {
            v0 = Abc_ObjFanin0(pObj)->iTemp ^ ((Abc_ObjFaninC0(pObj)) ? S_ONE : S_ZERO);
            v1 = Abc_ObjFanin1(pObj)->iTemp ^ ((Abc_ObjFaninC1(pObj)) ? S_ONE : S_ZERO);
            pObj->iTemp = v0 & v1;
        }
        Abc_NtkForEachPo(pNtk, pObj, i) {
            Vec_IntPush((Vec_Int_t *)Vec_PtrEntry(pValue, i), Abc_ObjFanin0(pObj)->iTemp ^ ((Abc_ObjFaninC0(pObj)) ? S_ONE : S_ZERO));
        }
    }
    Abc_NtkForEachPo(pNtk, pObj, i) {
        printf("%s: ", Abc_ObjName(pObj));
        for (int b = 0; b < batchSize; ++b) {
            int res = Vec_IntEntry((Vec_Int_t *)Vec_PtrEntry(pValue, i), b);
            for (j = 0; j < nBits - 32 * b; ++j) {
                printf("%d", (res >> j) & 1);
            }
        }
        printf("\n");
    }
    ABC_FREE(pModel);
    for (int i = 0; i < Abc_NtkPoNum(pNtk); ++i) {
        Vec_IntFree((Vec_Int_t *)Vec_PtrEntry(pValue, i));
    }
    Vec_PtrFree(pValue);
}

Vec_Ptr_t *Lsv_GetPattern(const char *filepath, int nInputs) {
    std::fstream fin(filepath);
    if (fin.fail()) {
        return NULL;
    }

    Vec_Ptr_t *pModel = Vec_PtrAlloc(nInputs);
    for (int i = 0; i < nInputs; ++i) {
        Vec_PtrPush(pModel, Vec_BitAlloc(0));
    }

    std::string line;
    while (std::getline(fin, line)) {
		if (line.size() != nInputs) {
			printf("Inconsistent input size %d vs %d.\n", line.size(), nInputs);
			printf("Stop at bit %d.\n", Vec_BitSize((Vec_Bit_t *)Vec_PtrEntry(pModel, 0)));
			fin.close();
			return pModel;
		}
        for (int i = 0; i < nInputs; ++i) {
            Vec_BitPush((Vec_Bit_t *)Vec_PtrEntry(pModel, i), line[i] == '1');
        }
    }

    fin.close();

    return pModel;
}

int Lsv_CommandSimAig(Abc_Frame_t *pAbc, int argc, char **argv) {
    Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
    Vec_Ptr_t *pModel = NULL;
    int c, fGlobal = 0, fVerbose = 0, nArgc;
    char **nArgv;
    // set defaults
    Extra_UtilGetoptReset();
    while ((c = Extra_UtilGetopt(argc, argv, "gvh")) != EOF) {
        switch (c) {
        case 'g':
            fGlobal ^= 1;
            break;
        case 'v':
            fVerbose ^= 1;
            break;
        case 'h':
            goto usage;
        default:
            goto usage;
        }
    }

    nArgc = argc - globalUtilOptind;
    nArgv = argv + globalUtilOptind;

    if (nArgc != 1) {
        goto usage;
    }

    if (pNtk == NULL) {
        Abc_Print(-1, "Empty network.\n");
        return 1;
    }

    if (!Abc_NtkIsStrash(pNtk)) {
        Abc_Print(-1, "Cannot lsv_sim_aig a network that is not an AIG (run \"strash\").\n");
        return 1;
    }

    pModel = Lsv_GetPattern(nArgv[0], Abc_NtkPiNum(pNtk));

    if (!pModel) {
        Abc_Print(-1, "Cannot open the input pattern file \"%s\"\n", nArgv[0]);
        return 1;
    }

    Lsv_NtkSimAig(pNtk, pModel);

final:
    for (c = 0; c < Abc_NtkPiNum(pNtk); ++c) {
        Vec_BitFree((Vec_Bit_t *)Vec_PtrEntry(pModel, c));
    }
    Vec_PtrFree(pModel);
    return 0;
usage:
    Abc_Print(-2, "usage: lsv_sim_aig [-gh] <input pattern file>\n");
    Abc_Print(-2, "\t        parallel simulation using aig\n");
    Abc_Print(-2, "\t-g    : parallel simulation of all primary outputs [default = %s].\n", fGlobal ? "yes" : "no");
    Abc_Print(-2, "\t-v    : verbose [default = %s].\n", fVerbose ? "yes" : "no");
    Abc_Print(-2, "\t-h    : print the command usage\n");
    return 1;
}

void Lsv_NtkSimBdd(Abc_Ntk_t *pNtk, const char *pattern) {
	int i;
    Abc_Obj_t *pNode;
    Vec_Ptr_t * vNamesIn;
    char * pNameOut;
    int comp;
    DdNode *ptr;

    std::unordered_map<std::string, int> map;
    Abc_NtkForEachPi(pNtk, pNode, i) {
        map[Abc_ObjName(pNode)] = pattern[i] == '1';
    }

    Abc_NtkForEachPo(pNtk, pNode, i) {
        printf("%s: ", Abc_ObjName(pNode));
        pNode = Abc_ObjFanin0(pNode);
        DdManager * dd = (DdManager *)pNode->pNtk->pManFunc;
        vNamesIn = Abc_NodeGetFaninNames( pNode );
        pNameOut = Abc_ObjName(pNode);

        int *model = ABC_ALLOC(int, Vec_PtrSize(vNamesIn));
        for (int i = 0; i < Vec_PtrSize(vNamesIn); ++i) {
            model[i] = map[(char *)Vec_PtrEntry(vNamesIn, i)];
        }

        // root node
        DdNode *root = (DdNode *)pNode->pData;
        comp = Cudd_IsComplement(root);
        ptr = Cudd_Regular(root);
        for (int i = 0; i < Vec_PtrSize(vNamesIn); ++i) {
            if (Cudd_IsConstant(ptr)) break;
            if (model[ptr->index] == 1) {
                ptr = cuddT(ptr);
            } else {
                comp ^= Cudd_IsComplement(cuddE(ptr));
                ptr = Cudd_Regular(cuddE(ptr));
            }
        }
        DdNode *target = Cudd_NotCond(ptr, comp);
        int constant = Cudd_IsConstant(target);
        printf("%d\n", constant ^ comp);

        ABC_FREE(model);
        Abc_NodeFreeNames( vNamesIn );
    }
}

int Lsv_CommandSimBdd(Abc_Frame_t *pAbc, int argc, char **argv) {
    Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
    Abc_Obj_t *pObj;
    int c;
    // set defaults
    Extra_UtilGetoptReset();
    while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
        switch (c) {
        case 'h':
            goto usage;
        default:
            goto usage;
        }
    }

    if (pNtk == NULL) {
        Abc_Print(-1, "Empty network.\n");
        return 1;
    }

	if (!Abc_NtkIsBddLogic(pNtk)) {
		Abc_Print(-1, "Cannot lsv_sim_bdd a network that is not an BDD (run \"collapse\").\n");
        return 1;
	}

    Lsv_NtkSimBdd(pNtk, argv[1]);

	return 0;
usage:
    Abc_Print(-2, "usage: lsv_sim_bdd [-gh] <input pattern>\n");
    Abc_Print(-2, "\t        simulation using bdd\n");
    Abc_Print(-2, "\t-h    : print the command usage\n");
    return 1;
}

void Lsv_NtkPrintNodes(Abc_Ntk_t *pNtk) {
    Abc_Obj_t *pObj;
    int i;
    Abc_NtkForEachNode(pNtk, pObj, i) {
        printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
        Abc_Obj_t *pFanin;
        int j;
        Abc_ObjForEachFanin(pObj, pFanin, j) {
            printf("  Fanin-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanin),
                   Abc_ObjName(pFanin));
        }
        if (Abc_NtkHasSop(pNtk)) {
            printf("The SOP of this node:\n%s", (char *)pObj->pData);
        }
    }
}

int Lsv_CommandPrintNodes(Abc_Frame_t *pAbc, int argc, char **argv) {
    Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
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