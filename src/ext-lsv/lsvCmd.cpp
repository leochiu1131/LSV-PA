#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <stdlib.h>
#include "sat/cnf/cnf.h"
#include "sat/bsat/satSolver.h"
extern "C"
{
	Aig_Man_t *Abc_NtkToDar(Abc_Ntk_t *pNtk, int fExors, int fRegisters);
}

const unsigned int ALL_ONES_32BIT = 4294967295;
static int Lsv_CommandPrintNodes(Abc_Frame_t *pAbc, int argc, char **argv);
static int Lsv_CommandSimBdd(Abc_Frame_t *pAbc, int argc, char **argv);
static int Lsv_CommandParaSimAig(Abc_Frame_t *pAbc, int argc, char **argv);
static int Lsv_CommandSymBdd(Abc_Frame_t *pAbc, int argc, char **argv);
static int Lsv_CommandSymSat(Abc_Frame_t *pAbc, int argc, char **argv);
static int Lsv_CommandSymAll(Abc_Frame_t *pAbc, int argc, char **argv);

void init(Abc_Frame_t *pAbc)
{
	Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
	Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", Lsv_CommandSimBdd, 0);
	Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig", Lsv_CommandParaSimAig, 0);
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

int Bdd_EvalNode(Abc_Obj_t *pNode, DdManager *dd, DdNode *node, int *varValues)
{
	if (!node)
	{
		Abc_Print(-1, "Something went wrong.\n");
		return -1;
	}
	if (node == Cudd_ReadLogicZero(dd))
	{
		return 0;
	}
	if (node == Cudd_ReadOne(dd))
	{
		return 1;
	}
	int vidx = Cudd_NodeReadIndex(node);
	DdNode *covar = Cudd_bddIthVar(dd, vidx);
	DdNode *nextNode = (varValues[Abc_ObjId(Abc_ObjFanin(pNode, vidx)) - 1] == 0) ? Cudd_Cofactor(dd, node, Cudd_Not(covar)) : Cudd_Cofactor(dd, node, covar);
	return Bdd_EvalNode(pNode, dd, nextNode, varValues);
}

void Lsv_NtkSimBdd(Abc_Ntk_t *pNtk, char *pPattern)
{
	int i;
	int nInputs = Abc_NtkCiNum(pNtk);
	int *varValue = ABC_ALLOC(int, nInputs);
	for (i = 0; i < nInputs; i++)
	{
		varValue[i] = pPattern[i] - '0';
	}

	DdManager *dd = (DdManager *)pNtk->pManFunc;
	Abc_Obj_t *pNode;
	Abc_NtkForEachNode(pNtk, pNode, i)
	{
		DdNode *node = (DdNode *)pNode->pData;
		int val = Bdd_EvalNode(pNode, dd, node, varValue);
		printf("%s: %d\n", Abc_ObjName(Abc_ObjFanout(pNode, 0)), val);
	}
	ABC_FREE(varValue);
}

int Lsv_CommandSimBdd(Abc_Frame_t *pAbc, int argc, char **argv)
{
	Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
	Extra_UtilGetoptReset();
	if (!pNtk)
	{
		Abc_Print(-1, "Empty network.\n");
		return 1;
	}
	if (argc != 2)
	{
		Abc_Print(-1, "Invalid number of arguments. Usage: lsv_sim_bdd <input_pattern>\n");
		return 1;
	}
	if (!Abc_NtkIsBddLogic(pNtk))
	{
		Abc_Print(-1, "Invalid current network type. Apply \"collapse\" before use.\n");
		return 1;
	}
	if (strlen(argv[1]) != Abc_NtkCiNum(pNtk))
	{
		Abc_Print(-1, "Invalid input pattern.\n");
		return 1;
	}
	Lsv_NtkSimBdd(pNtk, argv[1]);
	return 0;
}

void Aig_ParaSim(Abc_Ntk_t *pNtk, int *pPatterns, int numPatterns, char *outputs[])
{
	if (numPatterns > 32)
	{
		Abc_Print(-1, "Number of patterns exceeds 32.\n");
		return;
	}
	int nNodes = Abc_NtkObjNumMax(pNtk);
	unsigned int *pNodeValues = ABC_ALLOC(unsigned int, nNodes);
	int i, j;
	Abc_Obj_t *pNode;
	// printf("%d\n", nNodes);
	Abc_NtkForEachPi(pNtk, pNode, i)
	{ // i starts from 0, id starts from 1
		// printf("(%d) %s: %d\n", i, Abc_ObjName(pNode), Abc_ObjId(pNode));
		pNodeValues[Abc_ObjId(pNode) - 1] = pPatterns[i];
	}
	unsigned int fanin0, fanin1;
	Abc_NtkForEachNode(pNtk, pNode, i)
	{
		// printf("(%d) %s: %d\n", i, Abc_ObjName(pNode), Abc_ObjId(pNode));
		fanin0 = pNodeValues[Abc_ObjId(Abc_ObjFanin0(pNode)) - 1];
		fanin1 = pNodeValues[Abc_ObjId(Abc_ObjFanin1(pNode)) - 1];
		if (Abc_ObjFaninC0(pNode))
		{
			fanin0 ^= ALL_ONES_32BIT;
		}
		if (Abc_ObjFaninC1(pNode))
		{
			fanin1 ^= ALL_ONES_32BIT;
		}
		// printf("f0: %d/%u\nf1: %d/%u\n", Abc_ObjId(Abc_ObjFanin0(pNode)), fanin0, Abc_ObjId(Abc_ObjFanin1(pNode)), fanin1);
		pNodeValues[Abc_ObjId(pNode) - 1] = fanin0 & fanin1;
		// printf("(%d) %s: %d/%u\n", i, Abc_ObjName(pNode), Abc_ObjId(pNode), fanin0 & fanin1);
	}
	unsigned int val;
	char *bin_val = ABC_ALLOC(char, 33);
	Abc_NtkForEachPo(pNtk, pNode, i)
	{
		// printf("fanin of %s: %d\n", Abc_ObjName(pNode), Abc_ObjId(Abc_ObjFanin0(pNode)));
		if (Abc_ObjId(Abc_ObjFanin0(pNode)) == 0)
		{
			val = ALL_ONES_32BIT;
		}
		else
		{
			val = pNodeValues[Abc_ObjId(Abc_ObjFanin0(pNode)) - 1];
		}
		if (Abc_ObjFaninC0(pNode))
		{
			val ^= ALL_ONES_32BIT;
		}
		val >>= (32 - numPatterns);
		// printf("%s: %u\n", Abc_ObjName(pNode), val);
		bin_val[numPatterns] = '\0';
		for (j = numPatterns - 1; j >= 0; j--)
		{
			bin_val[j] = (val & 1) + '0';
			val >>= 1;
		}
		strcat(outputs[i], bin_val);
	}
	ABC_FREE(pNodeValues);
	ABC_FREE(bin_val);
	return;
}

void Lsv_NtkParaSimAig(Abc_Ntk_t *pNtk, char *inputPatternFile)
{

	FILE *inFile = fopen(inputPatternFile, "r");
	if (!inFile)
	{
		Abc_Print(-1, "Error: Could not open input pattern file '%s'.\n", inputPatternFile);
		return;
	}

	int total_pattern_num = 0;
	char line[1000000]; // Assume number of PI is at most 999999
	while (fgets(line, sizeof(line), inFile))
	{
		total_pattern_num++;
	}
	fclose(inFile);

	int i, j;
	Abc_Obj_t *pNode;
	int nInputs = Abc_NtkPiNum(pNtk);
	int *pPatterns = ABC_ALLOC(int, nInputs);
	for (i = 0; i < nInputs; i++)
	{
		pPatterns[i] = 0;
	}

	int nOutputs = Abc_NtkPoNum(pNtk);
	char *outputs[nOutputs];
	for (i = 0; i < nOutputs; i++)
	{
		outputs[i] = ABC_ALLOC(char, total_pattern_num + 2);
		outputs[i][0] = '\0';
	}

	int patCount = 0; // for each round, at most 32 patterns
	inFile = fopen(inputPatternFile, "r");
	while (fgets(line, sizeof(line), inFile))
	{
		for (j = 0; j < nInputs; j++)
		{
			pPatterns[j] |= (line[j] - '0') << (31 - patCount);
		}
		patCount++;
		if (patCount == 32)
		{
			Aig_ParaSim(pNtk, pPatterns, patCount, outputs);
			for (i = 0; i < nInputs; i++)
			{
				pPatterns[i] = 0;
			}
			patCount = 0;
		}
	}
	fclose(inFile);
	if (patCount > 0)
	{
		Aig_ParaSim(pNtk, pPatterns, patCount, outputs);
	}

	Abc_NtkForEachPo(pNtk, pNode, i)
	{
		printf("%s: %s\n", Abc_ObjName(pNode), outputs[i]);
	}
	ABC_FREE(pPatterns);
	for (i = 0; i < nOutputs; i++)
	{
		ABC_FREE(outputs[i]);
	}
}

int Lsv_CommandParaSimAig(Abc_Frame_t *pAbc, int argc, char **argv)
{
	Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
	Extra_UtilGetoptReset();
	if (!pNtk)
	{
		Abc_Print(-1, "Empty network.\n");
		return 1;
	}
	if (argc != 2)
	{
		Abc_Print(-1, "Invalid number of arguments. Usage: lsv_sim_bdd <input pattern>\n");
		return 1;
	}
	Lsv_NtkParaSimAig(pNtk, argv[1]);
	return 0;
}

// ==============================================================
DdNode *Bdd_getCofactor(DdManager *dd, DdNode *f, DdNode *ni, DdNode *nj)
{
	DdNode *xi = ni;
	DdNode *xj = Cudd_Not(nj);
	Cudd_Ref(xi);
	Cudd_Ref(xj);
	// printf("xi, xj ok\n");
	DdNode *f_xi_xjNeg = Cudd_Cofactor(dd, f, Cudd_bddAnd(dd, xi, xj));
	Cudd_Ref(f_xi_xjNeg);
	Cudd_RecursiveDeref(dd, xi);
	Cudd_RecursiveDeref(dd, xj);
	// if(f_xi_xjNeg == Cudd_ReadLogicZero(dd)) printf("0\n");
	// else if(f_xi_xjNeg == Cudd_ReadOne(dd)) printf("1\n");
	// printf("cofactor get\n");
	return f_xi_xjNeg;
}

void getCounterPattern(DdManager *dd, DdNode *node, int *pat, int nInputs)
{
	DdNode *root = node;
	Cudd_Ref(root);
	for (int i = 0; i < nInputs; i++)
	{
		if (root == Cudd_ReadOne(dd))
		{
			// printf("Pattern Found\n");
			return;
		}
		DdNode *covar = Cudd_bddIthVar(dd, i);
		Cudd_Ref(covar);
		DdNode *newNode = Cudd_Cofactor(dd, root, covar);
		if (newNode != Cudd_ReadLogicZero(dd))
		{
			pat[i] = 1;
		}
		else
		{
			newNode = Cudd_Cofactor(dd, root, Cudd_Not(covar));
		}
		Cudd_Ref(newNode);
		Cudd_RecursiveDeref(dd, root);
		Cudd_RecursiveDeref(dd, covar);
		root = newNode;
	}
	// if(root != Cudd_ReadOne(dd)){
	//     printf("ERROR!!!\n");
	//     return;
	//   }
}

void Lsv_NtkSymBdd(Abc_Ntk_t *pNtk, int k, int idx1, int idx2)
{
	if (idx1 == idx2)
	{
		printf("symmetric\n");
		return;
	}
	int idx;
	DdManager *dd = (DdManager *)pNtk->pManFunc;
	Abc_Obj_t *pNode;
	Abc_Obj_t *pFanin;
	DdNode *xi = NULL;
	DdNode *xj = NULL;
	pNode = Abc_ObjFanin0(Abc_NtkPo(pNtk, k));
	DdNode *node = (DdNode *)pNode->pData;

	Abc_ObjForEachFanin(pNode, pFanin, idx)
	{
		if (Abc_ObjId(pFanin) - 1 == idx1)
		{
			xi = Cudd_bddIthVar(dd, idx);
		}
		if (Abc_ObjId(pFanin) - 1 == idx2)
		{
			xj = Cudd_bddIthVar(dd, idx);
		}
	}
	if ((xi == NULL) && (xj == NULL))
	{
		printf("symmetric\n");
		return;
	}

	DdNode *node1 = NULL;
	DdNode *node2 = NULL;
	if ((xi != NULL) && (xj != NULL))
	{
		node1 = Bdd_getCofactor(dd, node, xi, xj);
		node2 = Bdd_getCofactor(dd, node, xj, xi);
		if (Cudd_bddLeq(dd, node1, node2) && Cudd_bddLeq(dd, node2, node1))
		{
			printf("symmetric\n");
			return;
		}
	}

	int i, j, val;
	int nInputs = Abc_NtkCiNum(pNtk);
	int *pPattemp = ABC_ALLOC(int, nInputs);
	for (i = 0; i < nInputs; i++)
	{
		pPattemp[i] = 0;
	}
	if ((xi == NULL))
	{
		// get node1, node2 = f_j, f_j'
		node1 = Cudd_Cofactor(dd, node, xj);
		node2 = Cudd_Cofactor(dd, node, Cudd_Not(xj));
	}
	if ((xj == NULL))
	{
		// get node1, node2 = f_i, f_i'
		node1 = Cudd_Cofactor(dd, node, xi);
		node2 = Cudd_Cofactor(dd, node, Cudd_Not(xi));
	}
	// get pattern
	DdNode *targNode = Cudd_bddXor(dd, node1, node2);
	Cudd_Ref(targNode);
	getCounterPattern(dd, targNode, pPattemp, nInputs);

	int *pPattern = ABC_ALLOC(int, nInputs);
	for (i = 0; i < nInputs; i++)
	{
		pPattern[i] = 0;
	}
	Abc_ObjForEachFanin(pNode, pFanin, idx)
	{
		pPattern[Abc_ObjId(pFanin) - 1] = pPattemp[idx];
	}
	printf("asymmetric\n");
	for (j = 0; j < 2; j++)
	{
		for (i = 0; i < nInputs; i++)
		{
			if (i == idx2)
				val = j;
			else if (i == idx1)
				val = 1 - j;
			else
				val = pPattern[i];
			printf("%d", val);
		}
		printf("\n");
	}
	Cudd_RecursiveDeref(dd, targNode);
	ABC_FREE(pPattemp);
	ABC_FREE(pPattern);
}

void Lsv_NtkSymSat(Abc_Ntk_t *pNtk, int idxo, int idx1, int idx2)
{
	if (idx1 == idx2)
	{
		printf("symmetric\n");
		return;
	}
	int i;
	Abc_Obj_t *pPO = Abc_NtkPo(pNtk, idxo);
	if (Abc_ObjId(Abc_ObjFanin0(pPO)) == 0)
	{
		printf("symmetric\n");
		return;
	}
	Abc_Ntk_t *pCone = Abc_NtkCreateCone(pNtk, Abc_ObjFanin0(pPO), Abc_ObjName(pPO), 1);
	Aig_Man_t *pAig = Abc_NtkToDar(pCone, 0, 0);
	Cnf_Dat_t *pCnf0 = Cnf_Derive(pAig, Aig_ManCoNum(pAig));
	int numCnfVar = pCnf0->nVars;
	Cnf_Dat_t *pCnf1 = Cnf_Derive(pAig, Aig_ManCoNum(pAig));
	Cnf_DataLift(pCnf1, numCnfVar);
	sat_solver *pSat = sat_solver_new();
	Cnf_DataWriteIntoSolverInt(pSat, pCnf0, 1, 0);
	Cnf_DataWriteIntoSolverInt(pSat, pCnf1, 1, 0);
	Abc_Obj_t *pObj;
	lit Lit[4];
	Abc_NtkForEachPi(pCone, pObj, i)
	{
		if (i == idx1 || i == idx2)
			continue;
		int id0 = pCnf0->pVarNums[i + 1];
		int id1 = pCnf1->pVarNums[i + 1];
		Lit[0] = toLitCond(id0, 0);
		Lit[1] = toLitCond(id1, 1);
		Lit[2] = toLitCond(id0, 1);
		Lit[3] = toLitCond(id1, 0);
		sat_solver_addclause(pSat, Lit, Lit + 2);
		sat_solver_addclause(pSat, Lit + 2, Lit + 4);
	}
	Lit[0] = toLitCond(pCnf0->pVarNums[idx1 + 1], 0); // x_i = 1
	Lit[1] = toLitCond(pCnf0->pVarNums[idx2 + 1], 1); // x_j = 0
	Lit[2] = toLitCond(pCnf1->pVarNums[idx1 + 1], 1); // y_i = 0
	Lit[3] = toLitCond(pCnf1->pVarNums[idx2 + 1], 0); // y_j = 1
	sat_solver_addclause(pSat, Lit, Lit + 1);
	sat_solver_addclause(pSat, Lit + 1, Lit + 2);
	sat_solver_addclause(pSat, Lit + 2, Lit + 3);
	sat_solver_addclause(pSat, Lit + 3, Lit + 4);
	Aig_Obj_t *pConeOut = Aig_ManCo(pAig, 0);
	int outId = Aig_ObjId(pConeOut);
	int o1 = pCnf0->pVarNums[outId], o2 = pCnf1->pVarNums[outId];
	Lit[0] = toLitCond(o1, 0);
	Lit[1] = toLitCond(o2, 0);
	Lit[2] = toLitCond(o1, 1);
	Lit[3] = toLitCond(o2, 1);
	sat_solver_addclause(pSat, Lit, Lit + 2);
	sat_solver_addclause(pSat, Lit + 2, Lit + 4);

	lbool status = sat_solver_solve(pSat, NULL, NULL, 0, 0, 0, 0);

	if (status == l_True)
	{
		printf("asymmetric\n");
		int val;
		Aig_Obj_t *pConeIn;
		Aig_ManForEachCi(pAig, pConeIn, i)
		{
			val = sat_solver_var_value(pSat, pCnf0->pVarNums[Aig_ObjId(pConeIn)]);
			printf("%d", val);
		}
		printf("\n");
		Aig_ManForEachCi(pAig, pConeIn, i)
		{
			val = sat_solver_var_value(pSat, pCnf1->pVarNums[Aig_ObjId(pConeIn)]);
			printf("%d", val);
		}
		printf("\n");
	}
	else
	{
		printf("symmetric\n");
	}
	// else if (status == l_False) {
	//     printf("symmetric\n");
	// } else {
	//     printf("SAT solver status is undefined or encountered an error.\n");
	// }

	// Clean up
	sat_solver_delete(pSat);
	Cnf_DataFree(pCnf0);
	Cnf_DataFree(pCnf1);
}

void Lsv_NtkSymAll(Abc_Ntk_t *pNtk, int idxo)
{
	int i, j;
	int nInputs = Abc_NtkCiNum(pNtk);
	Abc_Obj_t *pPO = Abc_NtkPo(pNtk, idxo);
	if (Abc_ObjId(Abc_ObjFanin0(pPO)) == 0)
	{
		for (i = 0; i < nInputs - 1; i++)
		{
			for (j = i + 1; j < nInputs; j++)
			{
				printf("%d %d\n", i, j);
			}
		}
		return;
	}
	Abc_Ntk_t *pCone = Abc_NtkCreateCone(pNtk, Abc_ObjFanin0(pPO), Abc_ObjName(pPO), 1);
	Aig_Man_t *pAig = Abc_NtkToDar(pCone, 0, 0);

	// create vector to record symmetric group, grouped: 1
	int *symGroup = ABC_ALLOC(int, nInputs);
	for (i = 0; i < nInputs; i++)
	{
		symGroup[i] = 0;
	}

	// add circuit CNF
	Cnf_Dat_t *pCnf0 = Cnf_Derive(pAig, Aig_ManCoNum(pAig));
	Cnf_Dat_t *pCnf1 = Cnf_Derive(pAig, Aig_ManCoNum(pAig));
	int numCnfVar = pCnf0->nVars;
	Cnf_DataLift(pCnf1, numCnfVar);
	sat_solver *pSat = sat_solver_new();
	Cnf_DataWriteIntoSolverInt(pSat, pCnf0, 1, 0);
	Cnf_DataWriteIntoSolverInt(pSat, pCnf1, 1, 0);

	// add miter CNF
	lit Lit[4];
	Aig_Obj_t *pConeOut = Aig_ManCo(pAig, 0);
	int outId = Aig_ObjId(pConeOut);
	int o1 = pCnf0->pVarNums[outId], o2 = pCnf1->pVarNums[outId];
	Lit[0] = toLitCond(o1, 0);
	Lit[1] = toLitCond(o2, 0);
	Lit[2] = toLitCond(o1, 1);
	Lit[3] = toLitCond(o2, 1);
	sat_solver_addclause(pSat, Lit, Lit + 2);
	sat_solver_addclause(pSat, Lit + 2, Lit + 4);

	// add control variables
	int *ctrl = ABC_ALLOC(int, nInputs);
	for (i = 0; i < nInputs; i++)
	{
		ctrl[i] = sat_solver_addvar(pSat);
	}

	int xi, yi, xj, yj;
	for (i = 0; i < nInputs; i++)
	{
		// add (xi = yi) + ci
		xi = pCnf0->pVarNums[i + 1];
		yi = pCnf1->pVarNums[i + 1];
		Lit[2] = toLitCond(ctrl[i], 0);
		Lit[0] = toLitCond(xi, 0);
		Lit[1] = toLitCond(yi, 1);
		sat_solver_addclause(pSat, Lit, Lit + 3); // (xi' -> yi) + ci
		Lit[0] = toLitCond(xi, 1);
		Lit[1] = toLitCond(yi, 0);
		sat_solver_addclause(pSat, Lit, Lit + 3); // (yi' -> xi) + ci
		// add (xi = yj) + ci' + cj'
		Lit[2] = toLitCond(ctrl[i], 1);
		for (j = 0; j < nInputs; j++)
		{
			if (i == j)
				continue;
			xj = pCnf0->pVarNums[j + 1];
			yj = pCnf1->pVarNums[j + 1];
			Lit[3] = toLitCond(ctrl[j], 1);
			Lit[0] = toLitCond(xi, 0);
			Lit[1] = toLitCond(yj, 1);
			sat_solver_addclause(pSat, Lit, Lit + 4);
			Lit[0] = toLitCond(xi, 1);
			Lit[1] = toLitCond(yj, 0);
			sat_solver_addclause(pSat, Lit, Lit + 4);
		}
	}

	lit inc[nInputs];
	for (i = 0; i < nInputs; i++)
	{
		inc[i] = toLitCond(ctrl[i], 1); // default: 0
	}

	int symNum;
	lbool status;
	int *curGroup = ABC_ALLOC(int, nInputs);
	for (i = 0; i < nInputs; i++)
	{
		curGroup[i] = -1;
	}
	// control variable assignment
	for (i = 0; i < nInputs - 1; i++)
	{
		if (symGroup[i] == 1)
			continue;
		symNum = 1;
		curGroup[0] = i;
		inc[i] = toLitCond(ctrl[i], 0);
		for (j = i + 1; j < nInputs; j++)
		{
			if (symGroup[j] == 1)
				continue;
			inc[j] = toLitCond(ctrl[j], 0);
			// printf("start %d, %d\n", i, j);
			status = sat_solver_solve(pSat, inc, inc + nInputs, 0, 0, 0, 0);
			inc[j] = toLitCond(ctrl[j], 1);
			if (status == l_False)
			{ // symmetric
				symGroup[j] = 1;
				curGroup[symNum] = j;
				symNum++;
			}
			// else if (status != l_True) {
			//   printf("Something went wrong!\n");
			// }
		}
		inc[i] = toLitCond(ctrl[i], 1);
		if (symNum > 1)
		{
			for (int v1 = 0; v1 < symNum - 1; v1++)
			{
				for (int v2 = v1 + 1; v2 < symNum; v2++)
				{
					printf("%d %d\n", curGroup[v1], curGroup[v2]);
				}
			}
		}
	}

	// Clean up
	sat_solver_delete(pSat);
	Cnf_DataFree(pCnf0);
	Cnf_DataFree(pCnf1);
	ABC_FREE(ctrl);
	ABC_FREE(symGroup);
	ABC_FREE(curGroup);
}

int Lsv_CommandSymBdd(Abc_Frame_t *pAbc, int argc, char **argv)
{
	Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
	Extra_UtilGetoptReset();
	if (!pNtk)
	{
		Abc_Print(-1, "Empty network.\n");
		return 1;
	}
	if (argc != 4)
	{
		Abc_Print(-1, "Invalid number of arguments. Usage: lsv_sym_bdd <k> <i> <j>\n");
		return 1;
	}
	if (!Abc_NtkIsBddLogic(pNtk))
	{
		Abc_Print(-1, "Invalid current network type. Apply \"collapse\" before use.\n");
		return 1;
	}

	Lsv_NtkSymBdd(pNtk, atoi(argv[1]), atoi(argv[2]), atoi(argv[3]));
	return 0;
}

int Lsv_CommandSymSat(Abc_Frame_t *pAbc, int argc, char **argv)
{
	Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
	Extra_UtilGetoptReset();
	if (!pNtk)
	{
		Abc_Print(-1, "Empty network.\n");
		return 1;
	}
	if (argc != 4)
	{
		Abc_Print(-1, "Invalid number of arguments. Usage: lsv_sym_sat <k> <i> <j>\n");
		return 1;
	}
	Lsv_NtkSymSat(pNtk, atoi(argv[1]), atoi(argv[2]), atoi(argv[3]));
	return 0;
}

int Lsv_CommandSymAll(Abc_Frame_t *pAbc, int argc, char **argv)
{
	Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
	Extra_UtilGetoptReset();
	if (!pNtk)
	{
		Abc_Print(-1, "Empty network.\n");
		return 1;
	}
	if (argc != 2)
	{
		Abc_Print(-1, "Invalid number of arguments. Usage: lsv_sym_all <k>\n");
		return 1;
	}
	// printf("lsv_sym_all, out = %d\n", atoi(argv[1]));
	Lsv_NtkSymAll(pNtk, atoi(argv[1]));
	return 0;
}
