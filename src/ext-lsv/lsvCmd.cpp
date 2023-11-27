#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <iostream>
#include <fstream>
#include "sat/cnf/cnf.h"
#include "bdd/cudd/cudd.h"
#include "bdd/cudd/cuddInt.h"
using namespace std;

extern "C" {
	Aig_Man_t* Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
}

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimBdd(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSymBdd(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSymSat(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
	Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
	Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", Lsv_CommandSimBdd, 0);
	Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig", Lsv_CommandSimAig, 0);
	Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_bdd", Lsv_CommandSymBdd, 0);
	Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_sat", Lsv_CommandSymSat, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

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

////////////////////////////////////////////////////////////////////////
///                        PA1                                       ///
////////////////////////////////////////////////////////////////////////

void Lsv_NtkSimBdd(Abc_Ntk_t* pNtk, char* arg) {
	int inputNum = Abc_NtkCiNum(pNtk);
	bool inputValue[inputNum];
	for (int i = 0; i < inputNum; ++i) {
		inputValue[i] = (arg[i] == '1');
	}

	DdManager* dd = (DdManager*)pNtk->pManFunc; 
	int ithPo; Abc_Obj_t* pPo;
	Abc_NtkForEachPo(pNtk, pPo, ithPo) {
		cout << Abc_ObjName(pPo) << ": ";
		Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo);
		DdNode* ddnode = (DdNode *)pRoot->pData;
		// bdd order
		char** vNamesIn = (char**) Abc_NodeGetFaninNames(pRoot)->pArray;
		for (int i = 0; i < Abc_NtkPiNum(pNtk); ++i) {
			if (!vNamesIn[i]) continue;
			if ( vNamesIn[i][0] == '\0') continue;
			Abc_Obj_t* pPi;
			int j;
			Abc_NtkForEachPi(pNtk, pPi, j) {
				// find matching input
				if (strcmp(vNamesIn[i], Abc_ObjName(pPi)) == 0) {
					if (inputValue[j]) {
						ddnode = Cudd_Cofactor(dd, ddnode, dd->vars[i]);
					} else {
						ddnode = Cudd_Cofactor(dd, ddnode, Cudd_Not(dd->vars[i]));
					}
				}
			}
		}
		if (ddnode == Cudd_ReadOne(dd) || ddnode == Cudd_Not(Cudd_ReadZero(dd))) {
			cout << 1 << endl;
		}
		if (ddnode == Cudd_ReadZero(dd) || ddnode == Cudd_Not(Cudd_ReadOne(dd))) {
			cout << 0 << endl;
		}  
	}
}


void Lsv_NtkSimAig(Abc_Ntk_t* pNtk, fstream& inFile) {
	string sss; 
	int row = 0;
	int column = Abc_NtkCiNum(pNtk);
	while(inFile >> sss) {
		++row;
	}
	inFile.clear();
	inFile.seekg(0, std::ios::beg);
	bool** inputArr = new bool*[row];
	for (int i = 0; i < row; ++i) {
		inputArr[i] = new bool[column];
	}
	Abc_Obj_t* pNode; int i;


	for(int r = 0; inFile >> sss; ++r) {
		const char* str = sss.c_str();
		for (int c = 0; c < column; ++c) {
			inputArr[r][c] = (str[c] == '1');
		}
	}

	// handle Pi
	Abc_NtkForEachPi(pNtk, pNode, i) {
		pNode->pTemp = new bool[row];
		for (int j = 0; j < row; ++j) {
			((bool*)pNode->pTemp)[j] = inputArr[j][i];
		}
	}

	// handle Nodes
	Abc_NtkForEachNode(pNtk, pNode, i) {
		pNode->pTemp = new bool[row];
		bool* fi0Arr = (bool*)(Abc_ObjFanin0(pNode)->pTemp);
		bool* fi1Arr = (bool*)(Abc_ObjFanin1(pNode)->pTemp);
		bool c0 = Abc_ObjFaninC0(pNode);
		bool c1 = Abc_ObjFaninC1(pNode);
		for (int j = 0; j < row; ++j) {
			((bool*)pNode->pTemp)[j] =   (c0 ? !(fi0Arr[j]) : fi0Arr[j])
									  && (c1 ? !(fi1Arr[j]) : fi1Arr[j]);	
		}
	}

	// handle Po
	Abc_NtkForEachPo(pNtk, pNode, i) {
		pNode->pTemp = new bool[row];
		bool* fi0Arr = (bool*)(Abc_ObjFanin0(pNode)->pTemp);
		bool c0 = Abc_ObjFaninC0(pNode);
		for (int j = 0; j < row; ++j) {
			((bool*)pNode->pTemp)[j] = c0 ? !(fi0Arr[j]) : fi0Arr[j];
		}
		cout << Abc_ObjName(pNode) <<": ";
		for (int j = 0; j < row; ++j) {
			cout << ((bool*)pNode->pTemp)[j];
		}
		cout<< endl;
	}
	
	/*----------------delete part-------------------*/
	Abc_NtkForEachPi(pNtk, pNode, i) {
		delete[] (bool*)pNode->pTemp;
	}
	Abc_NtkForEachPo(pNtk, pNode, i) {
		delete[] (bool*)pNode->pTemp;
	}
	Abc_NtkForEachNode(pNtk, pNode, i) {
		delete[] (bool*)pNode->pTemp;
	}
	for (int i = 0; i < row; ++i) {
		delete[] inputArr[i];
	}	
	delete[] inputArr;
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
	if (!Abc_NtkIsBddLogic(pNtk)) {
		Abc_Print(-1, "Network is not logic BDD networks (run \"collapse\").\n");
		return 1;
	}
	Lsv_NtkSimBdd(pNtk, argv[1]);
	return 0;

usage:
	Abc_Print(-2, "usage: lsv_sim_bdd [-h]\n");
	Abc_Print(-2, "\t        simulate the Ntk with bdd\n");
	Abc_Print(-2, "\t-h    : print the command usage\n");
	return 1;
}

int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv) {
	Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
	fstream inFile;
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
	if (!Abc_NtkIsStrash(pNtk)) {
		Abc_Print(-1, "Network is not logic AIG networks (run \"strash\").\n");
		return 1;
	}
	inFile.open(argv[1], ios::in);
	if (!inFile) {
		Abc_Print(-1, "Cannot open input file!\n");
		return 1;
	}
	Lsv_NtkSimAig(pNtk, inFile);
	return 0;

usage:
	Abc_Print(-2, "usage: lsv_sim_aig [-h]\n");
	Abc_Print(-2, "\t        simulate the Ntk with aig\n");
	Abc_Print(-2, "\t-h    : print the command usage\n");
	return 1;
}

////////////////////////////////////////////////////////////////////////
///                        PA2                                       ///
////////////////////////////////////////////////////////////////////////

void Lsv_NtkSymBdd(Abc_Ntk_t* pNtk, int k, int i, int j) {
	Abc_Obj_t* pPo = Abc_NtkPo(pNtk, k);
	Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo);
	// total Fin number of pNtk
	int CiNum = Abc_NtkCiNum(pNtk);
	// Fin number of pPo (may be less)
	int FaninNum = Abc_ObjFaninNum(pRoot);

	DdManager* dd = (DdManager*)pNtk->pManFunc;
	char* inputName1 = Abc_ObjName(Abc_NtkPi(pNtk, i));
	char* inputName2 = Abc_ObjName(Abc_NtkPi(pNtk, j));
	char** vNamesIn = (char**) Abc_NodeGetFaninNames(pRoot)->pArray;
	DdNode* d1 = (DdNode*)pRoot->pData;
	DdNode* d2 = (DdNode*)pRoot->pData;
	Cudd_Ref(d1);
	Cudd_Ref(d2);
	for (int l = 0; l < FaninNum; ++l) {
		if (strcmp(inputName1, vNamesIn[l]) == 0) {
			DdNode* temp1 = Cudd_Cofactor(dd, d1, dd->vars[l]);
			DdNode* temp2 = Cudd_Cofactor(dd, d2, Cudd_Not(dd->vars[l]));
			Cudd_RecursiveDeref(dd, d1);
			Cudd_RecursiveDeref(dd, d2);
			d1 = temp1;
			d2 = temp2;
			Cudd_Ref(d1);
			Cudd_Ref(d2);
		}
		if (strcmp(inputName2, vNamesIn[l]) == 0) {
			DdNode* temp1 = Cudd_Cofactor(dd, d1, Cudd_Not(dd->vars[l]));
			DdNode* temp2 = Cudd_Cofactor(dd, d2, dd->vars[l]);
			Cudd_RecursiveDeref(dd, d1);
			Cudd_RecursiveDeref(dd, d2);
			d1 = temp1;
			d2 = temp2;
			Cudd_Ref(d1);
			Cudd_Ref(d2);
		}
	}

	// check if d1 == d2
	if (d1 == d2) {
		cout << "symmetric" << endl;
	} else {
		string s1, s2;
		for (int l = 0; l < CiNum; ++l) {

			// check if l is i, j
			if (strcmp(Abc_ObjName(Abc_NtkPi(pNtk, l)), inputName1) == 0) {
				s1 += '0';
				s2 += '1';
				continue;
			} else if (strcmp(Abc_ObjName(Abc_NtkPi(pNtk, l)), inputName2) == 0) {
				s1 += '1';
				s2 += '0';
				continue;
			}

			int ddvar = -1;
			for (int m = 0; m < FaninNum; ++m) {
				if (strcmp(Abc_ObjName(Abc_NtkPi(pNtk, l)), vNamesIn[m]) == 0) {
					ddvar = m;
					continue;
				}
			}
			// not appear in pPo
			if (ddvar < 0) {
				s1 += '0';
				s2 += '0';
				continue;
			} else {
				DdNode* temp1 = Cudd_Cofactor(dd, d1, dd->vars[ddvar]);
				DdNode* temp2 = Cudd_Cofactor(dd, d2, dd->vars[ddvar]);
				Cudd_Ref(temp1);
				Cudd_Ref(temp2);
				if (temp1 != temp2) {
					Cudd_RecursiveDeref(dd, d1);
					Cudd_RecursiveDeref(dd, d2);
					d1 = temp1;
					d2 = temp2;
					s1 += '1';
					s2 += '1';
				} else {
					Cudd_RecursiveDeref(dd, temp1);
					Cudd_RecursiveDeref(dd, temp2);
					temp1 = Cudd_Cofactor(dd, d1, Cudd_Not(dd->vars[ddvar]));
					temp2 = Cudd_Cofactor(dd, d2, Cudd_Not(dd->vars[ddvar]));
					Cudd_RecursiveDeref(dd, d1);
					Cudd_RecursiveDeref(dd, d2);
					d1 = temp1;
					d2 = temp2;
					Cudd_Ref(d1);
					Cudd_Ref(d2);
					s1 += '0';
					s2 += '0';
				}
			}
		}
		cout << "asymmetric" << endl;
		cout << s1 << endl;
		cout << s2 << endl;
	}

}

void Lsv_NtkSymSat(Abc_Ntk_t* pNtk, int k, int i, int j) {

}


int Lsv_CommandSymBdd(Abc_Frame_t* pAbc, int argc, char** argv) {
	Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
	int c;
	int k, i, j;
	int CiNum = Abc_NtkCiNum(pNtk);
	int CoNum = Abc_NtkCoNum(pNtk);
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
	if (!Abc_NtkIsBddLogic(pNtk)) {
		Abc_Print(-1, "Network is not logic BDD networks (run \"collapse\").\n");
		return 1;
	}
	if (argc != 4) {
		Abc_Print(-1, "Wrong argument amount.\n");
		return 1;
	}
	try {
		k = stoi(argv[1]);
		i = stoi(argv[2]);
		j = stoi(argv[3]);
	}
	catch (const std::exception& e) {
		Abc_Print(-1, "Connot convert to int !!!\n");
		return 1;
	}
	if (k < 0 || k >= CoNum) {
		Abc_Print(-1, "k exceeds range.\n");
		return 1;
	}
	if (i < 0 || i >= CiNum) {
		Abc_Print(-1, "i exceeds range.\n");
		return 1;
	}
	if (j < 0 || j >= CiNum) {
		Abc_Print(-1, "j exceeds range.\n");
		return 1;
	}
	if (i == j) {
		Abc_Print(-1, "i, j are the same.\n");
		return 1;
	}
	
	Lsv_NtkSymBdd(pNtk, k, i, j);
	return 0;

usage:
	Abc_Print(-2, "usage: lsv_sym_bdd [-h]\n");
	Abc_Print(-2, "\t        check if i j symmetric to k with bdd\n");
	Abc_Print(-2, "\t-h    : print the command usage\n");
	return 1;
}

int Lsv_CommandSymSat(Abc_Frame_t* pAbc, int argc, char** argv) {
	Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
	int c;
	int k, i, j;
	int CiNum = Abc_NtkCiNum(pNtk);
	int CoNum = Abc_NtkCoNum(pNtk);
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
	if (!Abc_NtkIsStrash(pNtk)) {
		Abc_Print(-1, "Network is not logic AIG networks (run \"strash\").\n");
		return 1;
	}
	if (argc != 4) {
		Abc_Print(-1, "Wrong input!!!\n");
		return 1;
	}
	try {
		k = stoi(argv[1]);
		i = stoi(argv[2]);
		j = stoi(argv[3]);
	}
	catch (const std::exception& e) {
		Abc_Print(-1, "Connot convert to int !!!\n");
		return 1;
	}
	if (k < 0 || k >= CoNum) {
		Abc_Print(-1, "k exceeds range.\n");
		return 1;
	}
	if (i < 0 || i >= CiNum) {
		Abc_Print(-1, "i exceeds range.\n");
		return 1;
	}
	if (j < 0 || j >= CiNum) {
		Abc_Print(-1, "j exceeds range.\n");
		return 1;
	}
	if (i == j) {
		Abc_Print(-1, "i, j are the same.\n");
		return 1;
	}
	
	Lsv_NtkSymSat(pNtk, k, i, j);
	return 0;

usage:
	Abc_Print(-2, "usage: lsv_sym_sat [-h]\n");
	Abc_Print(-2, "\t        check if i j symmetric to k with sat\n");
	Abc_Print(-2, "\t-h    : print the command usage\n");
	return 1;
}

