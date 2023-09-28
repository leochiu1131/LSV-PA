#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"

static int Lsv_CommandPrintNodes(Abc_Frame_t *pAbc, int argc, char **argv);
static int Lsv_CommandSimBdd(Abc_Frame_t *pAbc, int argc, char **argv);
static int Lsv_CommandSimAig(Abc_Frame_t *pAbc, int argc, char **argv);

void init(Abc_Frame_t *pAbc)
{
	Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
	Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", Lsv_CommandSimBdd, 0);
	Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig", Lsv_CommandSimAig, 0);
}

void destroy(Abc_Frame_t *pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager
{
	PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

void Lsv_NtkPrintNodes(Abc_Ntk_t *pNtk)
{
	Abc_Obj_t *pObj;
	int i;
	Abc_NtkForEachNode(pNtk, pObj, i)
	{
		printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
		Abc_Obj_t *pFanin;
		int j;
		Abc_ObjForEachFanin(pObj, pFanin, j)
		{
			printf("  Fanin-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanin),
						 Abc_ObjName(pFanin));
		}
		if (Abc_NtkHasSop(pNtk))
		{
			printf("The SOP of this node:\n%s", (char *)pObj->pData);
		}
	}
}

int Lsv_CommandPrintNodes(Abc_Frame_t *pAbc, int argc, char **argv)
{
	Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
	int c;
	Extra_UtilGetoptReset();
	while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF)
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
	Lsv_NtkPrintNodes(pNtk);
	return 0;

usage:
	Abc_Print(-2, "usage: lsv_print_nodes [-h]\n");
	Abc_Print(-2, "\t        prints the nodes in the network\n");
	Abc_Print(-2, "\t-h    : print the command usage\n");
	return 1;
}

// Hint
/*
Abc_NtkForEachPo(pNtk, pPo, ithPo)
{
	Abc_Obj_t *pRoot = Abc_ObjFanin0(pPo);
	assert(Abc_NtkIsBddLogic(pRoot->pNtk));
	DdManager *dd = (DdManager *)pRoot->pNtk->pManFunc;
	DdNode *ddnode = (DdNode *)pRoot->pData;
}

char **vNamesIn = (char **)Abc_NodeGetFaninNames(pRoot)->pArray;
*/
// lsv_sim_bdd command
void Lsv_SimBdd(Abc_Ntk_t *pNtk, char **argv)
{
	// Find the input pattern variable order and read the input pattern
	Vec_Ptr_t *vPiNodes;
	Abc_Obj_t *pPi;
	int ithPi;
	vPiNodes = Vec_PtrAlloc(100);
	bool *truthV = new bool[Abc_NtkPiNum(pNtk)];
	Abc_NtkForEachPi(pNtk, pPi, ithPi)
	{
		Vec_PtrPush(vPiNodes, Abc_UtilStrsav(Abc_ObjName(pPi)));
		truthV[ithPi] = (argv[1][ithPi] == '1') ? true : false;
	}
	char **vNamesPi = (char **)vPiNodes->pArray;
	// Debuging
	// for (int i = 0; i < Abc_NtkPiNum(pNtk); i++)
	// {
	// 	printf("%s : %d\n", vNamesPi[i], truthV[i]);
	// }
	// Traverse BDD for every PO
	Abc_Obj_t *pPo;
	int ithPo;
	Abc_NtkForEachPo(pNtk, pPo, ithPo)
	{
		// printf("%s\n", Abc_ObjName(pPo));
		Abc_Obj_t *pRoot = Abc_ObjFanin0(pPo);
		assert(Abc_NtkIsBddLogic(pRoot->pNtk));
		DdManager *dd = (DdManager *)pRoot->pNtk->pManFunc;
		DdNode *ddnode = (DdNode *)pRoot->pData;

		char **vNamesIn = (char **)Abc_NodeGetFaninNames(pRoot)->pArray;
		DdNode *tmp;
		int j;
		bool isInput1 = true;
		for (int i = 0; i < Abc_ObjFaninNum(pRoot); i++)
		{
			// printf("%s ", vNamesIn[i]);
			// Find the truth value of i-th variable in BDD
			for (j = 0; j < Abc_NtkPiNum(pNtk); j++) // remember to compare all PI names
			{
				if (strcmp(vNamesIn[i], vNamesPi[j]) == 0)
				{
					// The truth value is the j-th input value
					isInput1 = truthV[j];
					break;
				}
			}
			// printf("%d\n", isInput1);
			if (isInput1)
			{
				tmp = Cudd_Cofactor(dd, ddnode, Cudd_bddIthVar(dd, i));
			}
			else
			{
				tmp = Cudd_Cofactor(dd, ddnode, Cudd_Not(Cudd_bddIthVar(dd, i)));
			}
			ddnode = tmp;
			Cudd_RecursiveDeref(dd, tmp);
		}
		// Output sim result
		bool isOutput1 = (ddnode == Cudd_ReadOne(dd));
		printf("%s : %d\n", Abc_ObjName(pPo), isOutput1);
	}
	delete[] truthV;
}

int Lsv_CommandSimBdd(Abc_Frame_t *pAbc, int argc, char **argv)
{
	Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
	int c;
	Extra_UtilGetoptReset();
	while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF)
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
	// Simulate the BDD
	Lsv_SimBdd(pNtk, argv);
	return 0;

usage:
	Abc_Print(-2, "This is the command lsv_sim_bdd.\n");
	return 1;
}

// lsv_sim_aig
int Lsv_CommandSimAig(Abc_Frame_t *pAbc, int argc, char **argv)
{
	Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
	int c;
	Extra_UtilGetoptReset();
	while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF)
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
	// Simulate the AIG

	return 0;

usage:
	Abc_Print(-2, "This is the command lsv_sim_aig.\n");
	return 1;
}