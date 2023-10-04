#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>

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
		// printf("  Fcompl0-%d, Fcompl1-%d\n", pObj->fCompl0,
		//			 pObj->fCompl1);
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

void Lsv_SimAig(Abc_Ntk_t *pNtk, char **argv)
{
	// Read input pattern
	std::ifstream infile;
	infile.open(argv[1]);
	std::string inputStr;
	std::vector<std::string> inputPattern;
	while (infile >> inputStr)
	{
		inputPattern.push_back(inputStr);
	}

	int count = 0;
	int p;
	std::vector<std::string> outputValue(Abc_NtkPoNum(pNtk), "");
	while (true)
	{
		// Create parallel input pattern
		std::vector<int> parallelPattern(Abc_NtkPiNum(pNtk), 0);
		for (p = 0; (p < 32 && count + p < inputPattern.size()); p++)
		{
			for (int k = 0; k < Abc_NtkPiNum(pNtk); k++)
			{
				parallelPattern[k] = parallelPattern[k] << 1;
				if (inputPattern[count + p][k] == '1')
				{
					parallelPattern[k] += 1;
				}
			}
		}
		// for (int k = 0; k < Abc_NtkPiNum(pNtk); k++)
		// {
		// 	std::cout << parallelPattern[k] << std::endl;
		// }
		if (p == 32)
			count += 32;
		else
			count += p;

		int i, j;
		// Add input pattern to PI
		std::vector<std::string> nodeName;
		std::vector<int> nodeValue(parallelPattern.begin(), parallelPattern.end());
		Abc_Obj_t *pPi;
		Abc_NtkForEachPi(pNtk, pPi, i)
		{
			nodeName.push_back(Abc_ObjName(pPi));
		}
		// for (int k = 0; k < Abc_NtkPiNum(pNtk); k++)
		// {
		// 	std::cout << nodeValue[k] << std::endl;
		// }

		// Simulate the pattern
		Abc_Obj_t *pObj;
		Abc_NtkForEachNode(pNtk, pObj, i)
		{
			Abc_Obj_t *pFanin0, *pFanin1;
			pFanin0 = Abc_ObjFanin0(pObj);
			pFanin1 = Abc_ObjFanin1(pObj);

			int tmp0 = 0, tmp1 = 0;
			for (j = 0; j < nodeName.size(); j++)
			{
				if (strcmp(Abc_ObjName(pFanin0), nodeName[j].c_str()) == 0)
				{
					tmp0 = nodeValue[j];
					// std::cout << j << " " << Abc_ObjName(pFanin0) << " " <<tmp0 << std::endl;
					if (pObj->fCompl0)
					{
						tmp0 = ~tmp0;
					}
					break;
				}
			}
			for (j = 0; j < nodeName.size(); j++)
			{
				if (strcmp(Abc_ObjName(pFanin1), nodeName[j].c_str()) == 0)
				{
					tmp1 = nodeValue[j];
					// std::cout << j << " " << Abc_ObjName(pFanin1) << " " << tmp1 << std::endl;
					if (pObj->fCompl1)
					{
						tmp1 = ~tmp1;
					}
					break;
				}
			}
			nodeName.push_back(Abc_ObjName(pObj));
			nodeValue.push_back(tmp0 & tmp1);
		}

		// Save value at PO
		Abc_Obj_t *pPo;
		Abc_NtkForEachPo(pNtk, pPo, i)
		{
			std::string bitStr = "";
			for (j = 0; j < nodeName.size(); j++)
			{
				if (strcmp(Abc_ObjName(Abc_ObjFanin0(pPo)), nodeName[j].c_str()) == 0)
				{
					// std::cout << nodeValue[j] << std::endl;
					if (pPo->fCompl0)
					{
						nodeValue[j] = ~nodeValue[j];
					}
					for (int k = 0; k < p; k++)
					{
						if (nodeValue[j] % 2 == 0)
							bitStr = "0" + bitStr;
						else
							bitStr = "1" + bitStr;
						nodeValue[j] = nodeValue[j] >> 1;
					}
					break;
				}
			}
			// std::cout << bitStr << std::endl;
			outputValue[i] += bitStr;
		}
		if (count >= inputPattern.size())
		{
			Abc_NtkForEachPo(pNtk, pPo, i)
			{
				printf("%s : %s\n", Abc_ObjName(pPo), outputValue[i].c_str());
			}
			break;
		}
	}
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
	Lsv_SimAig(pNtk, argv);
	return 0;

usage:
	Abc_Print(-2, "This is the command lsv_sim_aig.\n");
	return 1;
}