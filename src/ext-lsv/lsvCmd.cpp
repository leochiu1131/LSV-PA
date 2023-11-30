#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>

// For SAT
#include "sat/cnf/cnf.h"
extern "C"{
	Aig_Man_t* Abc_NtkToDar(Abc_Ntk_t* pNtk, int fExors, int fRegisters);
}

static int Lsv_CommandPrintNodes(Abc_Frame_t *pAbc, int argc, char **argv);
static int Lsv_CommandSimBdd(Abc_Frame_t *pAbc, int argc, char **argv);
static int Lsv_CommandSimAig(Abc_Frame_t *pAbc, int argc, char **argv);
static int Lsv_CommandSymBdd(Abc_Frame_t *pAbc, int argc, char **argv);
static int Lsv_CommandSymSat(Abc_Frame_t *pAbc, int argc, char **argv);
static int Lsv_CommandSymAll(Abc_Frame_t *pAbc, int argc, char **argv);

void init(Abc_Frame_t *pAbc)
{
	Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
	Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", Lsv_CommandSimBdd, 0);
	Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig", Lsv_CommandSimAig, 0);
	Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_bdd", Lsv_CommandSymBdd, 0);
	Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_sat", Lsv_CommandSymSat, 0);
	Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_all", Lsv_CommandSymAll, 0);
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

// PA2

void Lsv_SymBdd(Abc_Ntk_t *pNtk, char **argv)
{
	// Read the argv
	int i, j, k;
	i = std::stoi(argv[2]);
	j = std::stoi(argv[3]);
	k = std::stoi(argv[1]);
	// printf("i = %d, j = %d, k = %d\n", i, j, k);
	if (i == j)
	{
		std::cout << "symmetric" << std::endl;
		return;
	}

	// Get the PI variable order
	Vec_Ptr_t *vPiNodes;
	Abc_Obj_t *pPi;
	int ithPi;
	vPiNodes = Vec_PtrAlloc(100);
	Abc_NtkForEachPi(pNtk, pPi, ithPi)
	{
		Vec_PtrPush(vPiNodes, Abc_UtilStrsav(Abc_ObjName(pPi)));
	}
	char **vNamesPi = (char **)vPiNodes->pArray;

	// Find the target PO
	Abc_Obj_t *pPo;
	pPo = Abc_NtkPo(pNtk, k);

	// Build BDD
	Abc_Obj_t *pRoot = Abc_ObjFanin0(pPo);
	assert(Abc_NtkIsBddLogic(pRoot->pNtk));
	DdManager *dd = (DdManager *)pRoot->pNtk->pManFunc;
	DdNode *ddnode = (DdNode *)pRoot->pData;

	// Calculate two cofactors for symmetry checking
	char **vNamesIn = (char **)Abc_NodeGetFaninNames(pRoot)->pArray;
	DdNode *cof1 = ddnode;
	DdNode *cof2 = ddnode;
	Cudd_Ref(cof1);
	Cudd_Ref(cof2);
	DdNode *tmp;
	for (int p = 0; p < Abc_ObjFaninNum(pRoot); p++)
	{
		if (strcmp(vNamesIn[p], vNamesPi[i]) == 0)
		{
			// The p-th input of BDD is i-th PI
			tmp = Cudd_Cofactor(dd, cof1, Cudd_bddIthVar(dd, p));
			Cudd_Ref(tmp);
			Cudd_RecursiveDeref(dd, cof1);
			cof1 = tmp;
			Cudd_Ref(cof1);
			Cudd_RecursiveDeref(dd, tmp);

			tmp = Cudd_Cofactor(dd, cof2, Cudd_Not(Cudd_bddIthVar(dd, p)));
			Cudd_Ref(tmp);
			Cudd_RecursiveDeref(dd, cof2);
			cof2 = tmp;
			Cudd_Ref(cof2);
			Cudd_RecursiveDeref(dd, tmp);
		}

		if (strcmp(vNamesIn[p], vNamesPi[j]) == 0)
		{
			// The p-th input of BDD is j-th PI
			tmp = Cudd_Cofactor(dd, cof1, Cudd_Not(Cudd_bddIthVar(dd, p)));
			Cudd_Ref(tmp);
			Cudd_RecursiveDeref(dd, cof1);
			cof1 = tmp;
			Cudd_Ref(cof1);
			Cudd_RecursiveDeref(dd, tmp);

			tmp = Cudd_Cofactor(dd, cof2, Cudd_bddIthVar(dd, p));
			Cudd_Ref(tmp);
			Cudd_RecursiveDeref(dd, cof2);
			cof2 = tmp;
			Cudd_Ref(cof2);
			Cudd_RecursiveDeref(dd, tmp);
		}
	}
	// Check symmetry
	if (cof1 == cof2)
	{
		printf("symmetric\n");
		Cudd_RecursiveDeref(dd, cof1);
		Cudd_RecursiveDeref(dd, cof2);
	}
	else
	{
		printf("asymmetric\n");
		tmp = Cudd_bddXor(dd, cof1, cof2);
		Cudd_Ref(tmp);
		Cudd_RecursiveDeref(dd, cof1);
		Cudd_RecursiveDeref(dd, cof2);
		// Print the counterexample
		// Cudd_PrintMinterm(dd, tmp);
		char *cex = new char[Abc_NtkPiNum(pNtk)];
		char *cex_p = new char[Abc_NtkPiNum(pNtk)];
		for (int p = 0; p < Abc_NtkPiNum(pNtk);p++)
		{
			cex[p] = '2';
			cex_p[p] = '2';
			// std::cout << vNamesPi[p] << " " << vNamesIn[p] << std::endl;
		}
		DdNode *N, *T, *E;
		DdNode *node, *one, *bzero;
		one = DD_ONE(dd);
		bzero = Cudd_Not(one);
		node = tmp;
		while (true)
		{
			if (node == one) break;
			N = Cudd_Regular(node);
			T = cuddT(N);
			E = cuddE(N);
			if (Cudd_IsComplement(node))
			{
				T = Cudd_Not(T);
				E = Cudd_Not(E);
			}
			if (T == bzero)
			{
				cex_p[N->index] = '0';
				node = E;
			}
			else if (E == bzero)
			{
				cex_p[N->index] = '1';
				node = T;
			}
			else
			{
				// Go to then edge if the node has two children
				cex_p[N->index] = '1';
				node = T;
			}
		}
		for (int p = 0; p < Abc_NtkPiNum(pNtk);p++)
		{
			if (cex_p[p] == '2')
			{
				cex_p[p] = '0'; // Set the don't care bit to 0
			}
		}
		// std::cout << cex_p << std::endl;
		// Permute the cex_p to the PI order
		for (int p = 0; p < Abc_NtkPiNum(pNtk);p++)
		{
			for (int q = 0; q < Abc_NtkPiNum(pNtk);q++)
			{
				if (strcmp(vNamesIn[p], vNamesPi[q]) == 0)
				{
					cex[q] = cex_p[p]; // the q-th PI is the p-th input variable if BDD
				}
			}
		}
		// std::cout << cex << std::endl;
		cex[i] = '1';
		cex[j] = '0';
		std::cout << cex << std::endl;
		cex[i] = '0';
		cex[j] = '1';
		std::cout << cex << std::endl;
		delete[] cex;
		Cudd_RecursiveDeref(dd, tmp);
	}
}

int Lsv_CommandSymBdd(Abc_Frame_t *pAbc, int argc, char **argv)
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
	// Symmetric checking with BDD
	Lsv_SymBdd(pNtk, argv);
	return 0;

usage:
	Abc_Print(-2, "This is the command lsv_sym_bdd.\n");
	return 1;
}

void Lsv_SymSat(Abc_Ntk_t *pNtk, char **argv)
{
	// Read the argv
	int i, j, k;
	i = std::stoi(argv[2]);
	j = std::stoi(argv[3]);
	k = std::stoi(argv[1]);

	if (i == j)
	{
		std::cout << "symmetric" << std::endl;
		return;
	}

	// Get the PI variable order
	Vec_Ptr_t *vPiNodes;
	Abc_Obj_t *pPi;
	int ithPi;
	vPiNodes = Vec_PtrAlloc(100);
	Abc_NtkForEachPi(pNtk, pPi, ithPi)
	{
		Vec_PtrPush(vPiNodes, Abc_UtilStrsav(Abc_ObjName(pPi)));
	}
	char **vNamesPi = (char **)vPiNodes->pArray;

	// Find the target PO
	Abc_Obj_t *pPo;
	pPo = Abc_NtkPo(pNtk, k);
	if (Abc_ObjId(Abc_ObjFanin0(pPo)) == 0)
	{
		std::cout << "symmetric" << std::endl;
		return;
	}

	// Build the cone
	Abc_Obj_t *pRoot = Abc_ObjFanin0(pPo);
	Abc_Ntk_t *pCone = Abc_NtkCreateCone(pNtk, pRoot, Abc_ObjName(pRoot), 1);

	// Derive a corresponding AIG circuit
	Aig_Man_t *pAig = Abc_NtkToDar(pCone, 0, 0);
	// std::cout << Aig_ManCiNum(pAig) << std::endl;
	// std::cout << Aig_ManCoNum(pAig) << std::endl;

	// Obtain the corresponding CNF C_A
	Cnf_Dat_t *pCnfA;
	pCnfA = Cnf_Derive(pAig, Aig_ManCoNum(pAig));

	// Initialize a SAT solver
	sat_solver *pSat;
	pSat = sat_solver_new();

	// Add the CNF C_A into the sat solver
	Cnf_DataWriteIntoSolverInt(pSat, pCnfA, 1, 0);

	// Create another CNF C_B and add it into the sat solver
	Cnf_Dat_t *pCnfB;
	pCnfB = Cnf_Derive(pAig, Aig_ManCoNum(pAig));
	Cnf_DataLift(pCnfB, pCnfA->nVars);
	Cnf_DataWriteIntoSolverInt(pSat, pCnfB, 1, 0);

	// Add equivalence clause for symmetry checking
	Vec_Ptr_t *vInNodes;
	Abc_Obj_t *pIn;
	int ithIn;
	vInNodes = Vec_PtrAlloc(100);
	Abc_NtkForEachPi(pCone, pIn, ithIn)
	{
		Vec_PtrPush(vInNodes, Abc_UtilStrsav(Abc_ObjName(pIn)));
	}
	char **vNamesIn = (char **)vInNodes->pArray;

	int p;
	lit Lits[2];
	int p_i, p_j, p_tA, p_tB;
	// Determine i,j and add equivalent clause
	for (p = 0;p < Abc_NtkPiNum(pCone);p++)
	{
		if(strcmp(vNamesIn[p],vNamesPi[i]) == 0)
		{
			p_i = p;
			continue;
		}
		else if(strcmp(vNamesIn[p],vNamesPi[j]) == 0)
		{
			p_j = p;
			continue;
		}
		else
		{
			p_tA = pCnfA->pVarNums[Aig_ManCi(pAig,p)->Id];
			p_tB = pCnfB->pVarNums[Aig_ManCi(pAig,p)->Id];
			Lits[0] = toLitCond( p_tA, 0 );
			Lits[1] = toLitCond( p_tB, 1 );
			sat_solver_addclause( pSat, Lits, Lits + 2 );
			Lits[0] = toLitCond( p_tA, 1 );
			Lits[1] = toLitCond( p_tB, 0 );
			sat_solver_addclause( pSat, Lits, Lits + 2 );
		}
	}

	// Add symmetry clause for i,j
	p_tA = pCnfA->pVarNums[Aig_ManCi(pAig,p_i)->Id];
	p_tB = pCnfB->pVarNums[Aig_ManCi(pAig,p_j)->Id];
	Lits[0] = toLitCond( p_tA, 0 );
	Lits[1] = toLitCond( p_tB, 1 );
	sat_solver_addclause( pSat, Lits, Lits + 2 );
	Lits[0] = toLitCond( p_tA, 1 );
	Lits[1] = toLitCond( p_tB, 0 );
	sat_solver_addclause( pSat, Lits, Lits + 2 );

	p_tA = pCnfA->pVarNums[Aig_ManCi(pAig,p_j)->Id];
	p_tB = pCnfB->pVarNums[Aig_ManCi(pAig,p_i)->Id];
	Lits[0] = toLitCond( p_tA, 0 );
	Lits[1] = toLitCond( p_tB, 1 );
	sat_solver_addclause( pSat, Lits, Lits + 2 );
	Lits[0] = toLitCond( p_tA, 1 );
	Lits[1] = toLitCond( p_tB, 0 );
	sat_solver_addclause( pSat, Lits, Lits + 2 );

	// Add XOR clause
	p_tA = pCnfA->pVarNums[Aig_ManCo(pAig,0)->Id];
	p_tB = pCnfB->pVarNums[Aig_ManCo(pAig,0)->Id];
	Lits[0] = toLitCond( p_tA, 0 );
	Lits[1] = toLitCond( p_tB, 0 );
	sat_solver_addclause( pSat, Lits, Lits + 2 );
	Lits[0] = toLitCond( p_tA, 1 );
	Lits[1] = toLitCond( p_tB, 1 );
	sat_solver_addclause( pSat, Lits, Lits + 2 );

	// Solve the CNF with the sat solver
	int status;
	status = sat_solver_solve( pSat, 0, 0, (ABC_INT64_T)0, (ABC_INT64_T)0, (ABC_INT64_T)0, (ABC_INT64_T)0 );
	if (status == l_False)
	{
		std::cout << "symmetric" << std::endl;
	}
	else if (status == l_True)
	{
		std::cout << "asymmetric" << std::endl;
		// Find counterexample
		std::string cexA, cexB;
		for (int q=0;q<Abc_NtkPiNum(pNtk);q++)
		{
			for (p = 0;p < Abc_NtkPiNum(pCone);p++)
			{
				if (strcmp(vNamesIn[p],vNamesPi[q]) == 0)
				{
					if (sat_solver_var_value(pSat, pCnfA->pVarNums[Aig_ManCi(pAig,p)->Id]) != 0)
						cexA += '1';
					else
						cexA += '0';
					if (sat_solver_var_value(pSat, pCnfB->pVarNums[Aig_ManCi(pAig,p)->Id]) != 0)
						cexB += '1';
					else
						cexB += '0';
				}
			}
		}
		std::cout << cexA << std::endl;
		std::cout << cexB << std::endl;
	}
}

int Lsv_CommandSymSat(Abc_Frame_t *pAbc, int argc, char **argv)
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
	// Symmetric checking with SAT
	Lsv_SymSat(pNtk, argv);
	return 0;

usage:
	Abc_Print(-2, "This is the command lsv_sym_sat.\n");
	return 1;
}

void Lsv_SymAll(Abc_Ntk_t *pNtk, char **argv)
{
	// Read the argv
	int k;
	k = std::stoi(argv[1]);

	// Get the PI variable order
	Vec_Ptr_t *vPiNodes;
	Abc_Obj_t *pPi;
	int ithPi;
	vPiNodes = Vec_PtrAlloc(100);
	Abc_NtkForEachPi(pNtk, pPi, ithPi)
	{
		Vec_PtrPush(vPiNodes, Abc_UtilStrsav(Abc_ObjName(pPi)));
	}
	char **vNamesPi = (char **)vPiNodes->pArray;

	// Find the target PO
	Abc_Obj_t *pPo;
	pPo = Abc_NtkPo(pNtk, k);

	if (Abc_ObjId(Abc_ObjFanin0(pPo)) == 0)
	{
		for (int u=0;u < Abc_NtkPiNum(pNtk)-1;u++)
		{
			for (int v=u+1;v < Abc_NtkPiNum(pNtk);v++)
			{
				std::cout << u << " " << v << std::endl;
			}
		}
		return;
	}

	// Build the cone
	Abc_Obj_t *pRoot = Abc_ObjFanin0(pPo);
	Abc_Ntk_t *pCone = Abc_NtkCreateCone(pNtk, pRoot, Abc_ObjName(pRoot), 1);

	// Derive a corresponding AIG circuit
	Aig_Man_t *pAig = Abc_NtkToDar(pCone, 0, 0);
	// std::cout << Aig_ManCiNum(pAig) << std::endl;
	// std::cout << Aig_ManCoNum(pAig) << std::endl;

	// Obtain the corresponding CNF C_A
	Cnf_Dat_t *pCnfA;
	pCnfA = Cnf_Derive(pAig, Aig_ManCoNum(pAig));

	// Initialize a SAT solver
	sat_solver *pSat;
	pSat = sat_solver_new();

	// Add the CNF C_A into the sat solver
	Cnf_DataWriteIntoSolverInt(pSat, pCnfA, 1, 0);

	// Create another CNF C_B and add it into the sat solver
	Cnf_Dat_t *pCnfB;
	pCnfB = Cnf_Derive(pAig, Aig_ManCoNum(pAig));
	Cnf_DataLift(pCnfB, pCnfA->nVars);
	Cnf_DataWriteIntoSolverInt(pSat, pCnfB, 1, 0);

	// Add equivalence clause with enable for symmetry checking
	Vec_Ptr_t *vInNodes;
	Abc_Obj_t *pIn;
	int ithIn;
	vInNodes = Vec_PtrAlloc(100);
	Abc_NtkForEachPi(pCone, pIn, ithIn)
	{
		Vec_PtrPush(vInNodes, Abc_UtilStrsav(Abc_ObjName(pIn)));
	}
	char **vNamesIn = (char **)vInNodes->pArray;

	int i,j;
	int p,q;
	lit Lits[4];
	int p_tA, p_tB, p_E1, p_E2;

	// Add equivalent clause with enable
	for (i=0;i < Abc_NtkPiNum(pNtk);i++)
	{
		for (p = 0;p < Abc_NtkPiNum(pCone);p++)
		{
			if(strcmp(vNamesIn[p],vNamesPi[i]) == 0)
			{
				p_tA = pCnfA->pVarNums[Aig_ManCi(pAig,p)->Id];
				p_tB = pCnfB->pVarNums[Aig_ManCi(pAig,p)->Id];
				p_E1 = pCnfA->nVars * 2 + i;
				Lits[0] = toLitCond( p_tA, 0 );
				Lits[1] = toLitCond( p_tB, 1 );
				Lits[2] = toLitCond( p_E1, 0 );
				sat_solver_addclause( pSat, Lits, Lits + 3 );
				Lits[0] = toLitCond( p_tA, 1 );
				Lits[1] = toLitCond( p_tB, 0 );
				Lits[2] = toLitCond( p_E1, 0 );
				sat_solver_addclause( pSat, Lits, Lits + 3 );
				break;
			}
		}
	}
	for (i=0;i < Abc_NtkPiNum(pNtk)-1;i++)
	{
		for (j=i+1;j < Abc_NtkPiNum(pNtk);j++)
		{
			for (p = 0;p < Abc_NtkPiNum(pCone);p++)
			{
				if(strcmp(vNamesIn[p],vNamesPi[i]) == 0)
				{
					for (q = 0;q < Abc_NtkPiNum(pCone);q++)
					{
						if(strcmp(vNamesIn[q],vNamesPi[j]) == 0)
						{
							p_tA = pCnfA->pVarNums[Aig_ManCi(pAig,p)->Id];
							p_tB = pCnfB->pVarNums[Aig_ManCi(pAig,q)->Id];
							p_E1 = pCnfA->nVars * 2 + i;
							p_E2 = pCnfA->nVars * 2 + j;
							Lits[0] = toLitCond( p_tA, 0 );
							Lits[1] = toLitCond( p_tB, 1 );
							Lits[2] = toLitCond( p_E1, 1 );
							Lits[3] = toLitCond( p_E2, 1 );
							sat_solver_addclause( pSat, Lits, Lits + 4 );
							Lits[0] = toLitCond( p_tA, 1 );
							Lits[1] = toLitCond( p_tB, 0 );
							Lits[2] = toLitCond( p_E1, 1 );
							Lits[3] = toLitCond( p_E2, 1 );
							sat_solver_addclause( pSat, Lits, Lits + 4 );
							p_tA = pCnfA->pVarNums[Aig_ManCi(pAig,q)->Id];
							p_tB = pCnfB->pVarNums[Aig_ManCi(pAig,p)->Id];
							p_E1 = pCnfA->nVars * 2 + i;
							p_E2 = pCnfA->nVars * 2 + j;
							Lits[0] = toLitCond( p_tA, 0 );
							Lits[1] = toLitCond( p_tB, 1 );
							Lits[2] = toLitCond( p_E1, 1 );
							Lits[3] = toLitCond( p_E2, 1 );
							sat_solver_addclause( pSat, Lits, Lits + 4 );
							Lits[0] = toLitCond( p_tA, 1 );
							Lits[1] = toLitCond( p_tB, 0 );
							Lits[2] = toLitCond( p_E1, 1 );
							Lits[3] = toLitCond( p_E2, 1 );
							sat_solver_addclause( pSat, Lits, Lits + 4 );
							break;
						}
					}
					break;
				}
			}
		}
	}

	// Add XOR clause
	p_tA = pCnfA->pVarNums[Aig_ManCo(pAig,0)->Id];
	p_tB = pCnfB->pVarNums[Aig_ManCo(pAig,0)->Id];
	Lits[0] = toLitCond( p_tA, 0 );
	Lits[1] = toLitCond( p_tB, 0 );
	sat_solver_addclause( pSat, Lits, Lits + 2 );
	Lits[0] = toLitCond( p_tA, 1 );
	Lits[1] = toLitCond( p_tB, 1 );
	sat_solver_addclause( pSat, Lits, Lits + 2 );

	// Solve the CNF with the incremental sat solving
	int status;
	int sym_num = 0;
	int sym_num_tmp;
	std::vector<std::vector<int>> ijstatus(Abc_NtkPiNum(pCone),std::vector<int>(Abc_NtkPiNum(pCone),2));
	for (i = 0;i<Abc_NtkPiNum(pCone)-1;i++)
	{
		for (j = i+1;j<Abc_NtkPiNum(pCone);j++)
		{
			if (ijstatus[i][j] != 2)
				continue;
			else
			{
				Lits[0] = toLitCond(pCnfA->nVars * 2 + i, 0);
				Lits[1] = toLitCond(pCnfA->nVars * 2 + j, 0);
				status = sat_solver_solve( pSat, Lits, Lits+2, (ABC_INT64_T)0, (ABC_INT64_T)0, (ABC_INT64_T)0, (ABC_INT64_T)0 );
				if (status == l_False)
				{
					ijstatus[i][j] = 1;
					std::cout << i << " " << j << std::endl;
				}
				else if (status == l_True)
				{
					ijstatus[i][j] = 0;
				}
				while(true)
				{
					// Apply transitivity
					sym_num_tmp = sym_num;
					for (int x=0;x < Abc_NtkPiNum(pCone)-2;x++)
					{
						for (int y=x+1;y < Abc_NtkPiNum(pCone)-1;y++)
						{
							for (int z=y+1;z < Abc_NtkPiNum(pCone);z++)
							{
								if (ijstatus[x][y] == 1 && ijstatus[y][z] == 1)
								{
									if (ijstatus[x][z] == 2)
									{
										ijstatus[x][z] = 1;
										std::cout << x << " " << z << std::endl;
										sym_num_tmp++;
										continue;
									}
								}
								if (ijstatus[x][y] == 1 && ijstatus[x][z] == 1)
								{
									if (ijstatus[y][z] == 2)
									{
										ijstatus[y][z] = 1;
										std::cout << y << " " << z << std::endl;
										sym_num_tmp++;
										continue;
									}
								}
								if (ijstatus[x][z] == 1 && ijstatus[y][z] == 1)
								{
									if (ijstatus[x][y] == 2)
									{
										ijstatus[x][y] = 1;
										std::cout << x << " " << y << std::endl;
										sym_num_tmp++;
										continue;
									}
								}
							}
						}
					}
					if (sym_num_tmp == sym_num)
						break;
					else
						sym_num = sym_num_tmp;
				}
			}
		}
	}
}

int Lsv_CommandSymAll(Abc_Frame_t *pAbc, int argc, char **argv)
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
	// Symmetric checking with incremental SAT solving
	Lsv_SymAll(pNtk, argv);
	return 0;

usage:
	Abc_Print(-2, "This is the command lsv_sym_all.\n");
	return 1;
}