#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <iostream>
#include <fstream>

using namespace std;

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimBdd(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
	Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
	Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", Lsv_CommandSimBdd, 0);
	Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig", Lsv_CommandSimAig, 0);
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