#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <vector>
#include <fstream>
#include <string>
#include <iostream>
#include "sat/cnf/cnf.h"
#include "bdd/cudd/cudd.h"
#include "bdd/cudd/cuddInt.h"
extern "C"{
    Aig_Man_t* Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
}

using namespace std;

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
// PA1
static int Lsv_CommandSimBdd(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv);
// PA2
static int Lsv_CommandSymBdd(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSymSAT(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  // PA1
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", Lsv_CommandSimBdd, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig", Lsv_CommandSimAig, 0);
  // PA2
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_bdd", Lsv_CommandSymBdd, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_sat", Lsv_CommandSymSAT, 0);
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

int Lsv_CommandSimBdd(Abc_Frame_t* pAbc, int argc, char** argv) {
    // for (int i = 0; i < argc; ++i) {
    //     printf("[%d] %s\n", i, argv[i]);
    // }
    // printf("Running Simulation on BDD\n");

    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    Abc_Obj_t* pPo;
    int i;
    DdManager * dd = (DdManager *)pNtk->pManFunc;
    Abc_NtkForEachPo(pNtk, pPo, i) {
        Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo);
        assert( Abc_NtkIsBddLogic(pRoot->pNtk) );
        DdNode* ddnode = (DdNode *)pRoot->pData;
        // printf("%d\n", i);
        // char** vNamesIn = (char**) Abc_NodeGetFaninNames(pRoot)->pArray;
        // printf("%d\n", i);
        // for (int j = 0; vNamesIn[j] != NULL; j++) {
        //     printf("[%d] %s\n", j, vNamesIn[j]);
        // }
        Abc_Obj_t* pFanin;
        int j;
        Abc_ObjForEachFanin(pRoot, pFanin, j) {
            // printf("  Fanin-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanin),
            //     Abc_ObjName(pFanin));
            
            DdNode* nownode = Cudd_bddIthVar(dd, j);
            // printf("%d\n", j);
            // printf("input pattern: %d\n", (argv[1][Abc_ObjId(pFanin)-1] - '0'));
            if ((argv[1][Abc_ObjId(pFanin)-1] - '0') == 1) {
                ddnode = Cudd_Cofactor(dd, ddnode, nownode);
            }
            else {
                ddnode = Cudd_Cofactor(dd, ddnode, Cudd_Not(nownode));
            }
        }
        
        if (ddnode == dd->one) {
            printf("%s: %d\n", Abc_ObjName(pPo), 1);
        }
        else {
            printf("%s: %d\n", Abc_ObjName(pPo), 0);
        }
    }

    return 0;
}

int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv) {
    // for (int i = 0; i < argc; ++i) {
    //     printf("[%d] %s\n", i, argv[i]);
    // }
    // printf("Running Simulation on AIG\n");

    std::fstream f(argv[1]);
    std::vector<std::string> in;
    std::string tmp;
    while (std::getline(f, tmp)) {
        in.push_back(tmp);
    }
    f.close();

    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    Abc_Obj_t* pPi;
    int i;
    std::vector<std::vector<int>> simPattern;
    std::vector<int> simtmp;
    Abc_NtkForEachPi(pNtk, pPi, i) {
        for (int j=0; j<in.size(); j++) {
            // printf("%d", (in[j][i] - '0'));
            simtmp.push_back(in[j][i] - '0');
        }
        // printf("\n");
        simPattern.push_back(simtmp);
        simtmp.clear();
    }
    // for (i=0; i<simPattern.size(); i++) {
    //     for (int j=0; j<simPattern[i].size(); j++) {
    //         printf("%d", simPattern[i][j]);
    //     }
    //     printf("\n");
    // }

    std::vector<std::vector<int>> Parallel;
    int currentInt = 0;
    int bitCount = 0;
    for (i=0; i<simPattern.size(); i++) {
        for (int j=0; j<simPattern[i].size(); j++) {
            int bit = simPattern[i][j];
            currentInt |= bit;
            ++bitCount;

            if (j == simPattern[i].size()-1) break;

            if (bitCount == 32) {
                simtmp.push_back(currentInt);
                // printf("%d\n", currentInt);
                currentInt = 0;
                bitCount = 0;
            } 
            else {
                currentInt <<= 1;
            }
        }  
        if (bitCount > 0) {
            simtmp.push_back(currentInt);
            // printf("%d\n", currentInt);
            currentInt = 0;
            bitCount = 0;
        }
        Parallel.push_back(simtmp);
        simtmp.clear();
    }

    // int obj_num = Abc_NtkObjNum(pNtk);
    // printf("# of obj = %d\n", obj_num);
    Abc_Obj_t* pObj;
    std::vector<std::vector<int>> result;
    Abc_NtkForEachObj(pNtk, pObj, i) {
        if (Abc_ObjIsPi(pObj)) {
            // printf("PI\n");
            // printf("  Obj-%d: Id = %d, name = %s\n", i, Abc_ObjId(pObj),
            //     Abc_ObjName(pObj));
            result.push_back(Parallel[i-1]);
            continue;
        }
        else if (pObj->Type == ABC_OBJ_CONST1) {
            // printf("CONST1\n");
            result.push_back({1});
            continue;
        }
        else if (Abc_ObjIsPo(pObj)) {
            // printf("PO\n");
            // printf("  Obj-%d: Id = %d, name = %s\n", i, Abc_ObjId(pObj),
            //     Abc_ObjName(pObj));
            result.push_back({0});
            continue;
        }
        else {
            // printf("Node\n");
            // printf("  Obj-%d: Id = %d, name = %s\n", i, Abc_ObjId(pObj),
            //     Abc_ObjName(pObj));
            std::vector<int> temp;
            int tmp_i;
            for (int j=0; j<Parallel[0].size(); j++) {
                if (Abc_ObjFaninC0(pObj)) {
                    if (Abc_ObjFaninC1(pObj)) {
                        tmp_i = (~result[Abc_ObjFanin0(pObj)->Id][j]) & (~result[Abc_ObjFanin1(pObj)->Id][j]);
                    }
                    else {
                        tmp_i = (~result[Abc_ObjFanin0(pObj)->Id][j]) & (result[Abc_ObjFanin1(pObj)->Id][j]);
                    }
                }
                else {
                    if (Abc_ObjFaninC1(pObj)) {
                        tmp_i = (result[Abc_ObjFanin0(pObj)->Id][j]) & (~result[Abc_ObjFanin1(pObj)->Id][j]);
                    }
                    else {
                        tmp_i = (result[Abc_ObjFanin0(pObj)->Id][j]) & (result[Abc_ObjFanin1(pObj)->Id][j]);
                    }
                }
                temp.push_back(tmp_i);
            }
            result.push_back(temp);
        }
    }
    Abc_Obj_t* pPo;
    Abc_NtkForEachPo(pNtk, pPo, i) {
        std::vector<int> temp;
        int tmp_i;
        for (int j=0; j<Parallel[0].size(); j++) {
            if (Abc_ObjFaninC0(pPo)) {
                tmp_i = ~result[Abc_ObjFanin0(pPo)->Id][j];
            }
            else {
                tmp_i = result[Abc_ObjFanin0(pPo)->Id][j];
            }
            temp.push_back(tmp_i);
        }
        result[Abc_ObjId(pPo)] = temp;

        printf("%s: ", Abc_ObjName(pPo));
        int cnt = 0;
        for (int j=0; j<temp.size(); j++) {
            int k = 31;
            if (simPattern[0].size() - cnt <= k)
                k = simPattern[0].size() - cnt -1;
            for (; k >= 0; k--) {
                if (temp[j] & (1 << k)) {
                    printf("1");
                } else {
                    printf("0");
                }
                cnt++;
                if (cnt == simPattern[0].size()) break;
            }
        }
        printf("\n");
    }

    return 0;
}


int Lsv_CommandSymBdd(Abc_Frame_t* pAbc, int argc, char** argv) {
    // for (int i = 0; i < argc; ++i) {
    //     printf("[%d] %s\n", i, argv[i]);
    // }
    // printf("Running Symmetric checking with BDD\n");

    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    int k, i, j;
    k = stoi(argv[1]);
    i = stoi(argv[2]);
    j = stoi(argv[3]);

    DdManager * dd = (DdManager *)pNtk->pManFunc;
    Abc_Obj_t* pPo = Abc_NtkPo(pNtk, k);
    Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo);
    assert( Abc_NtkIsBddLogic(pRoot->pNtk) );
    
    DdNode* ddnode = (DdNode *)pRoot->pData;
    Cudd_Ref(ddnode);
	int num = Abc_ObjFaninNum(pRoot);

	char* name_i = Abc_ObjName(Abc_NtkPi(pNtk, i));
	char* name_j = Abc_ObjName(Abc_NtkPi(pNtk, j));
	char** vNamesIn = (char**) Abc_NodeGetFaninNames(pRoot)->pArray;
	DdNode* ddnode_10 = ddnode;     // for cofactor of i=1, j=0
    Cudd_Ref(ddnode_10);
    DdNode* ddnode_01 = ddnode;     // for cofactor of i=0, j=1
	Cudd_Ref(ddnode_01);
	for (int l=0; l<num; l++) {
		if (strcmp(name_i, vNamesIn[l]) == 0) {
            // printf("cofactor i\n");
			ddnode_10 = Cudd_Cofactor(dd, ddnode_10, dd->vars[l]);
			ddnode_01 = Cudd_Cofactor(dd, ddnode_01, Cudd_Not(dd->vars[l]));
		}
		if (strcmp(name_j, vNamesIn[l]) == 0) {
            // printf("cofactor j\n");
			ddnode_10 = Cudd_Cofactor(dd, ddnode_10, Cudd_Not(dd->vars[l]));
			ddnode_01 = Cudd_Cofactor(dd, ddnode_01, dd->vars[l]);
		}
	}

    // XOR two cofactor to check symmetry
    // DdNode* ddnode_xor = Cudd_bddXor(dd, ddnode_10, ddnode_01);
    // Cudd_Ref(ddnode_xor);
    // Cudd_RecursiveDeref(dd, ddnode_10);
    // Cudd_RecursiveDeref(dd, ddnode_01);

    if (ddnode_01 == ddnode_10) {
        printf("symmetric\n");
    }
    else {
        printf("asymmetric\n");

        // find cex
        string s1, s2;
        Abc_Obj_t* pObj;
        int l;
        Abc_NtkForEachPi(pNtk, pObj, l) {
            if (strcmp(Abc_ObjName(pObj), name_i) == 0) {
                // i
                s1 += '1';
                s2 += '0';
            }
            else if (strcmp(Abc_ObjName(pObj), name_j) == 0) {
                // j
                s1 += '0';
                s2 += '1';
            }
            else {
                int n = -1;
                for (int m=0; m<num; m++) {
                    if (strcmp(Abc_ObjName(pObj), vNamesIn[m]) == 0) {
                        n = m;
                        break;
                    }
                }

                if (n == -1) {
                    // not in Fanin of pRoot
                    s1 += '0';
				    s2 += '0';
                }
                else {
                    // in Fanin of pRoot
                    if (Cudd_Cofactor(dd, ddnode_10, dd->vars[n]) == Cudd_Cofactor(dd, ddnode_01, dd->vars[n])) {
                        s1 += '0';
					    s2 += '0';
                    }
                    else {
                        s1 += '1';
					    s2 += '1';
                    }
                }
            }
        }
        cout << s1 << endl;
		cout << s2 << endl;
    }
    
    // Cudd_RecursiveDeref(dd, ddnode_xor);
    Cudd_RecursiveDeref(dd, ddnode_10);
    Cudd_RecursiveDeref(dd, ddnode_01);
    Cudd_RecursiveDeref(dd, ddnode);
    
    return 0;
}

int Lsv_CommandSymSAT(Abc_Frame_t* pAbc, int argc, char** argv) {
    // for (int i = 0; i < argc; ++i) {
    //     printf("[%d] %s\n", i, argv[i]);
    // }
    // printf("Running Symmetric checking with SAT\n");

    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    int k, i, j;
    k = stoi(argv[1]);
    i = stoi(argv[2]);
    j = stoi(argv[3]);

    Abc_Obj_t* pPo = Abc_NtkPo(pNtk, k);
	Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo);
	Abc_Ntk_t* pCone = Abc_NtkCreateCone(pNtk, pRoot, Abc_ObjName(pRoot), 1);
	Aig_Man_t* cAig = Abc_NtkToDar(pCone, 0, 0);
	Cnf_Dat_t* pCnf_A = Cnf_Derive(cAig, 1);
	
    sat_solver* pSAT = sat_solver_new();
    Cnf_DataWriteIntoSolverInt(pSAT, pCnf_A, 1, 0);
    Cnf_Dat_t* pCnf_B = Cnf_Derive(cAig, 1);
	Cnf_DataLift(pCnf_B, pCnf_A->nVars);
	Cnf_DataWriteIntoSolverInt(pSAT, pCnf_B, 1, 0);

	// v_A(t) = v_B(t), t not i or j
    Aig_Obj_t* pObj;
	int t;
    lit Lits[2];
    int v_A, v_B;
	Aig_ManForEachCi(cAig, pObj, t) {
		if (t != i && t != j) {
            // xnor
            v_A = pCnf_A->pVarNums[pObj->Id];
            v_B = pCnf_B->pVarNums[pObj->Id];
			Lits[0] = toLitCond(v_A, 1); 
            Lits[1] = toLitCond(v_B, 0);
            sat_solver_addclause(pSAT, Lits, Lits + 2);
			Lits[0] = toLitCond(v_A, 0);
            Lits[1] = toLitCond(v_B, 1);
			sat_solver_addclause(pSAT, Lits, Lits + 2);
		}
	}

	// v_A(i) = v_B(j), v_A(j) = v_B(i)
	Aig_Obj_t* x_i = Aig_ManCi(cAig, i);
	Aig_Obj_t* x_j = Aig_ManCi(cAig, j);
    // v_A(i) = v_B(j)
    v_A = pCnf_A->pVarNums[x_i->Id];
    v_B = pCnf_B->pVarNums[x_j->Id];
	Lits[0] = toLitCond(v_A, 0);
    Lits[1] = toLitCond(v_B, 1);
    sat_solver_addclause(pSAT, Lits, Lits + 2);
    Lits[0] = toLitCond(v_A, 1);
    Lits[1] = toLitCond(v_B, 0);
    sat_solver_addclause(pSAT, Lits, Lits + 2);
    // v_A(j) = v_B(i)
    v_A = pCnf_A->pVarNums[x_j->Id];
    v_B = pCnf_B->pVarNums[x_i->Id];
    Lits[0] = toLitCond(v_A, 0);
    Lits[1] = toLitCond(v_B, 1);
    sat_solver_addclause(pSAT, Lits, Lits + 2);
    Lits[0] = toLitCond(v_A, 1);
    Lits[1] = toLitCond(v_B, 0);
    sat_solver_addclause(pSAT, Lits, Lits + 2);
	
	// v_A(y_k) xor v_B(y_k)  // should be UNSAT
	Aig_Obj_t* y_k = Aig_ManCo(cAig, 0);
    v_A = pCnf_A->pVarNums[y_k->Id];
    v_B = pCnf_B->pVarNums[y_k->Id];
	Lits[0] = toLitCond(v_A, 1);
    Lits[1] = toLitCond(v_B, 1);
    sat_solver_addclause(pSAT, Lits, Lits + 2);
	Lits[0] = toLitCond(v_A, 0);
    Lits[1] = toLitCond(v_B, 0);
    sat_solver_addclause(pSAT, Lits, Lits + 2);

	// solver solve
	int status = sat_solver_solve_internal(pSAT);
	if (status == -1) {
		printf("symmetric\n");
	} 
    else {
        printf("asymmetric\n");
        
        int num = Aig_ManCiNum(cAig);
		int assign1[num];
        int assign2[num];
        int l;
		Aig_ManForEachCi(cAig, pObj, l) {
            if (l == i) {
                assign1[l] = 1;
			    assign2[l] = 0;
            }
            else if (l == j) {
                assign1[l] = 0;
			    assign2[l] = 1;
            }
            else {
                assign1[l] = sat_solver_var_value(pSAT, pCnf_A->pVarNums[pObj->Id]);
			    assign2[l] = sat_solver_var_value(pSAT, pCnf_B->pVarNums[pObj->Id]);
            }
		}
		
		for (l=0; l<num; l++) {
            printf("%d", assign1[l]);
        }
        printf("\n");
        for (l=0; l<num; l++) {
            printf("%d", assign2[l]);
        }
        printf("\n");
	}

    return 0;
}
