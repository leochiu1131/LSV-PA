#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <vector>
#include "sat/cnf/cnf.h"
using namespace std;
extern "C"{
Aig_Man_t* Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
}

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimBdd(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSymBdd(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSymSat(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSymAll(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", Lsv_CommandSimBdd, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig", Lsv_CommandSimAig, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_bdd", Lsv_CommandSymBdd, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_sat", Lsv_CommandSymSat, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_all", Lsv_CommandSymAll, 0);
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

vector<int> AddPattern(char** argv, Abc_Ntk_t* pNtk) {
  vector<int> pat;
  for (int i = 0; i < pNtk->vPis->nSize; ++i) {
    int p = (argv[1][i] == '1')? 1:0;
    pat.push_back(p);
  }
  return pat;
}

void PrintData(Abc_Obj_t* pRoot, Abc_Obj_t* pPo) {
  cout << "pRoot->Id : " << pRoot->Id << endl;            // 9
  cout << "pPo Name : " << Abc_ObjName(pPo) << endl;      // y3
  for (int j = 0; j < pRoot->vFanins.nSize; ++j) {
    char** vNamesIn = (char**) Abc_NodeGetFaninNames(pRoot)->pArray;
    cout << "vNamesIn[" << j << "] : " << vNamesIn[j] << " (" << pRoot->vFanins.pArray[j]-1 << ")" << endl;    // a1 b0 a0 b1
  }
}

int Lsv_CommandSimBdd(Abc_Frame_t* pAbc, int argc, char** argv) {
  if (argc < 2) {
    cout << "missing pattern" << endl;
    return 1;
  }

  Abc_Ntk_t* pNtk;
  Abc_Obj_t* pPo;
  int ithPo = 0;
  pNtk = Abc_FrameReadNtk(pAbc);
  // add pattern into unordered_map
  vector<int> pattern = AddPattern(argv, pNtk);

  Abc_NtkForEachPo(pNtk, pPo, ithPo) {
    Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo);
    assert( Abc_NtkIsBddLogic(pRoot->pNtk) );
    DdManager * dd = (DdManager *)pRoot->pNtk->pManFunc;
    DdNode* ddnode = (DdNode *)pRoot->pData;
    // PrintData(pRoot, pPo);

    for (int j = 0; j < pRoot->vFanins.nSize; ++j) {
      int n = pRoot->vFanins.pArray[j] - 1;
      DdNode* d = (pattern[n] == 1)? dd->vars[j] : Cudd_Not(dd->vars[j]); 
      ddnode = Cudd_Cofactor(dd, ddnode, d);
    }
    if (ddnode == dd->one) cout << Abc_ObjName(pPo) << ": 1" << endl;
    else cout << Abc_ObjName(pPo) << ": 0" << endl; 
  }
  return 0;
}

vector<string> ReadFile(char* filename) {
  ifstream fin(filename);
  vector<string> pattern;
  string pat;
  while(getline(fin, pat)) {
    pattern.push_back(pat);
    for (int i = 0; i < pat.size(); ++i) {
    }
  }
  return pattern;
}

int** Create2dimArray (int f_dim, int s_dim) {
  int** arr = new int*[f_dim];
  for (int i = 0; i < f_dim; ++i) {
    arr[i] = new int[s_dim];
  }
  for (int i = 0; i < f_dim; ++i) {
    for (int j = 0; j < s_dim; ++j) {
      arr[i][j] =0;
    }
  }
  return arr;
}

void PrintBinary(int in, int lim) {
  int a = 1;
  for (int i = 0; i < lim; ++i) {
    if ((in & (a << i)) == 0) cout << '0';
    else                      cout << '1';
  }
}

void PrintPattern(int** ptn, int Pi_num, int ptn_size) {
  for (int i = 0; i < Pi_num; ++i) {
    for (int j = 0; j < ptn_size; ++j) {
      cout << "i = " << i << ", j = " << j << " : ";
      PrintBinary(ptn[i][j], 32);
    }
  }
}

int** StrToIntPtn(vector<string> ptn_str) {
  int Pi_num = ptn_str[0].size() - 1;
  int ptn_str_size = ptn_str.size();
  int ptn_size = (ptn_str_size % 32 == 0)? (ptn_str_size/32) : (ptn_str_size/32 + 1);
  int** ptn = Create2dimArray(Pi_num, ptn_size);
  for (int i = 0; i < ptn_str_size; ++i) {
    for (int j = 0; j < Pi_num; ++j) {
      int p = (ptn_str[i][j] == '1')? 1 : 0;
      ptn[j][i/32] = ptn[j][i/32] | (p << i);
    }
  }
  // PrintPattern(ptn, Pi_num, ptn_size);
  return ptn;
}

void DFS(vector<Abc_Obj_t*>& DFS_List, Abc_Obj_t* curNode) {
  Abc_Obj_t* Fanin;
  int i = 0;
  Abc_ObjForEachFanin(curNode, Fanin, i) {
    DFS(DFS_List, Fanin);
  }
  bool exist = false;
  for (int i = 0; i < DFS_List.size(); ++i) {
    if (DFS_List[i] == curNode) {
       exist = true;
       break;
    }
  }
  if(!exist)  {
    DFS_List.push_back(curNode);
  }
}

int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv) {
  if (argc < 2) {
    cout << "missing file" << endl;
    return 1;
  }
  // read file
  vector<string> ptn_str = ReadFile(argv[1]);
  // convert Intpt file into int pattern
  int** ptn = StrToIntPtn(ptn_str);
  int ptn_fdim = ptn_str[0].size() - 1;
  int ptn_sdim = (ptn_str.size() % 32 == 0)? (ptn_str.size()/32) : (ptn_str.size()/32 + 1);

  Abc_Ntk_t* pNtk;
  Abc_Obj_t* pPo;
  int ithPo;
  Abc_Obj_t* curNode;
  int ithNode;
  pNtk = Abc_FrameReadNtk(pAbc);
  Vec_Ptr_t * vNodes = Abc_NtkDfsIter(pNtk, 0);
  int** output = Create2dimArray(pNtk->vPos->nSize, ptn_sdim);
  for (int i = 0; i < ptn_sdim; ++i) {
    Abc_Obj_t* pPi;
    int ithPi = 0;
    Abc_NtkForEachPi(pNtk, pPi, ithPi) {
      pPi->iTemp = ptn[ithPi][i];
    }
    Vec_PtrForEachEntry( Abc_Obj_t *, vNodes, curNode, ithNode) {
      int fin0res = (Abc_ObjFaninC0(curNode))? ~Abc_ObjFanin0(curNode)->iTemp : Abc_ObjFanin0(curNode)->iTemp;
      int fin1res = (Abc_ObjFaninC1(curNode))? ~Abc_ObjFanin1(curNode)->iTemp : Abc_ObjFanin1(curNode)->iTemp;
      int res = fin0res & fin1res;
      curNode->iTemp = res;
      // cout << curNode->Id << ' ';
      // PrintBinary(curNode->iTemp, 32);
      // cout << endl;
    }
    cout << endl;
    Abc_Obj_t* pPo;
    int ithPo;
    
    Abc_NtkForEachPo(pNtk, pPo, ithPo) {
      Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo);
      int ans = (pRoot->Id == 0)? -1 : pRoot->iTemp;
      if (!Abc_ObjFaninC0(pPo)) output[ithPo][i] = ans;
      else                      output[ithPo][i] = ~ans; 
    }
  } 
  // print output
  // cout << "start print output" << endl;
  Abc_NtkForEachPo(pNtk, pPo, ithPo) {
    cout << Abc_ObjName(pPo) << " : ";
    for (int i = 0; i < ptn_sdim; ++i) {
      int lim = (i == ptn_sdim-1)? ptn_str.size()-32*i : 32;
      PrintBinary(output[ithPo][i], lim);
    }
    cout << endl;
  }
  for (int i = 0; i < ptn_fdim; i++) {
    delete[] ptn[i];
  }
  delete[] ptn;
  for (int i = 0; i < pNtk->vPos->nSize; ++i) {
    delete[] output[i];
  }
  delete[] output;

  return 0;
}

bool MissingInput(int argc, int lim) {
  if (argc < lim) {
    cout << "missing input" << endl;
    return 1;
  }
  return 0;
}

int Lsv_CommandSymBdd(Abc_Frame_t* pAbc, int argc, char** argv) {
  if (MissingInput(argc, 4)) return 1;

  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int K = stoi(argv[1]);
  int I = stoi(argv[2]);
  int J = stoi(argv[3]);
  Abc_Obj_t* pPoK = (Abc_Obj_t*) pNtk->vPos->pArray[K];
  Abc_Obj_t* pRootK = Abc_ObjFanin0(pPoK);
  DdManager * dd = (DdManager *) pNtk->pManFunc;
  DdNode* ddPiI;
  DdNode* ddPiI_;
  DdNode* ddPiJ;
  DdNode* ddPiJ_;
  bool I_valid = false;
  bool J_valid = false;

  for (int i = 0; i < pRootK->vFanins.nSize; ++i) {
    int n = pRootK->vFanins.pArray[i] - 1;
    if (n == I) {
      I_valid = true;
      ddPiI = dd->vars[i];            Cudd_Ref(ddPiI);
      ddPiI_ = Cudd_Not(ddPiI);       Cudd_Ref(ddPiI_);
    }
    if (n == J) {
      J_valid = true;
      ddPiJ = dd->vars[i];            Cudd_Ref(ddPiJ);
      ddPiJ_ = Cudd_Not(ddPiJ);       Cudd_Ref(ddPiJ_);
    }
  }
  DdNode* ddIJ_ = (DdNode*) pRootK->pData;  Cudd_Ref(ddIJ_);
  DdNode* ddI_J = (DdNode*) pRootK->pData;  Cudd_Ref(ddI_J);
  if (I_valid) {
    DdNode* New_ddIJ_ = Cudd_Cofactor(dd, ddIJ_, ddPiI);
    Cudd_Ref(New_ddIJ_);
    Cudd_RecursiveDeref(dd, ddIJ_);
    Cudd_RecursiveDeref(dd, ddPiI);
    ddIJ_ = New_ddIJ_;
    DdNode* New_ddI_J = Cudd_Cofactor(dd, ddI_J, ddPiI_);
    Cudd_Ref(New_ddI_J);
    Cudd_RecursiveDeref(dd, ddI_J);
    Cudd_RecursiveDeref(dd, ddPiI_);
    ddI_J = New_ddI_J;
  }
  if (J_valid) {
    DdNode* New_ddIJ_ = Cudd_Cofactor(dd, ddIJ_, ddPiJ_);
    Cudd_Ref(New_ddIJ_);
    Cudd_RecursiveDeref(dd, ddIJ_);
    Cudd_RecursiveDeref(dd, ddPiJ);
    ddIJ_ = New_ddIJ_;
    DdNode* New_ddI_J = Cudd_Cofactor(dd, ddI_J, ddPiJ);
    Cudd_Ref(New_ddI_J);
    Cudd_RecursiveDeref(dd, ddI_J);
    Cudd_RecursiveDeref(dd, ddPiJ_);
    ddI_J = New_ddI_J;
  }

  if (ddIJ_ == ddI_J) {
    cout << "symmetric" << endl;
  }
  else {
    cout << "asymmetric" << endl;
    string ptnIJ_ = "";
    string ptnI_J = "";
    for (int i = 0; i < Abc_NtkCiNum(pNtk); ++i) {
      if (i == I) {
        ptnIJ_ += '1';
        ptnI_J += '0';
      }
      else if (i == J) {
        ptnIJ_ += '0';
        ptnI_J += '1';
      }
      else {
        bool IsFin = false;
        for (int j = 0; j < pRootK->vFanins.nSize; ++j) {
          int n = pRootK->vFanins.pArray[j] - 1;
          if (i == n) {
            IsFin = true;
            DdNode* ddnode = dd->vars[j];                           Cudd_Ref(ddnode);
            DdNode* Pos_ddIJ_ = Cudd_Cofactor(dd, ddIJ_, ddnode);   Cudd_Ref(Pos_ddIJ_);
            DdNode* Pos_ddI_J = Cudd_Cofactor(dd, ddI_J, ddnode);   Cudd_Ref(Pos_ddI_J);
            if (Pos_ddIJ_ != Pos_ddI_J) {
              ptnIJ_ += '1';
              ptnI_J += '1';
              Cudd_RecursiveDeref(dd, ddIJ_);
              ddIJ_ = Pos_ddIJ_;
              Cudd_RecursiveDeref(dd, ddI_J);
              ddI_J = Pos_ddI_J;
            }
            else {
              Cudd_RecursiveDeref(dd, Pos_ddIJ_);
              Cudd_RecursiveDeref(dd, Pos_ddI_J);
              DdNode* Neg_ddIJ_ = Cudd_Cofactor(dd, ddIJ_, Cudd_Not(ddnode));
              Cudd_Ref(Neg_ddIJ_);
              Cudd_RecursiveDeref(dd, ddIJ_);
              ddIJ_ = Neg_ddIJ_;
              DdNode* Neg_ddI_J = Cudd_Cofactor(dd, ddI_J, Cudd_Not(ddnode));
              Cudd_Ref(Neg_ddI_J);
              Cudd_RecursiveDeref(dd, ddI_J);
              ddI_J = Neg_ddI_J;
              ptnIJ_ += '0';
              ptnI_J += '0';
            }
            Cudd_RecursiveDeref(dd, ddnode);
            break;
          }
        }
        if (!IsFin) {
          ptnIJ_ += '1';
          ptnI_J += '1';
        }
      }
    }
    cout << ptnIJ_ << endl;
    cout << ptnI_J << endl;
  }
  Cudd_RecursiveDeref(dd, ddIJ_);
  Cudd_RecursiveDeref(dd, ddI_J);
  return 0;
}

int VarNums(Cnf_Dat_t* pCnf, Aig_Obj_t* pIN) {
  return pCnf->pVarNums[pIN->Id];
}

void AddSymmetric(Aig_Man_t* pAig, sat_solver* pSat, Cnf_Dat_t* pCnfA, Cnf_Dat_t* pCnfB, int I, int J) {
  Aig_Obj_t* pPiI;
  Aig_Obj_t* pPiJ;
  int Lits[2];
  // vA(t) = vB(t)
  for (int i = 0; i < pAig->vCis->nSize; ++i) {
    Aig_Obj_t* pPi = (Aig_Obj_t*) pAig->vCis->pArray[i];
    int vA_t = VarNums(pCnfA, pPi);
    int vB_t = VarNums(pCnfB, pPi);
    if (i == I) {
      pPiI = pPi;
    }
    else if (i == J) {
      pPiJ = pPi;
    }
    else {
      // (vA(t) + vB(t)')
      Lits[0] = 2 * vA_t;
      Lits[1] = 2 * vB_t + 1;
      sat_solver_addclause(pSat, Lits, Lits + 2);
      // (vA(t)' + vB(t))
      Lits[0] = 2 * vA_t + 1;
      Lits[1] = 2 * vB_t;
      sat_solver_addclause(pSat, Lits, Lits + 2);
    }
  }
  int vA_i = VarNums(pCnfA, pPiI);
  int vA_j = VarNums(pCnfA, pPiJ);
  int vB_i = VarNums(pCnfB, pPiI);
  int vB_j = VarNums(pCnfB, pPiJ);
  // vA(i) = vB(j)
    // (vA(i) + vB(j)')
    Lits[0] = 2 * vA_i;
    Lits[1] = 2 * vB_j + 1;
    sat_solver_addclause(pSat, Lits, Lits + 2);
    // (vA(i)' + vB(j))
    Lits[0] = 2 * vA_i + 1;
    Lits[1] = 2 * vB_j;
    sat_solver_addclause(pSat, Lits, Lits + 2);
  // vA(j) = vB(i)
    // (vA(j) + vB(i)')
    Lits[0] = 2 * vA_j;
    Lits[1] = 2 * vB_i + 1;
    sat_solver_addclause(pSat, Lits, Lits + 2);
    // (vA(j)' + vB(i))
    Lits[0] = 2 * vA_j + 1;
    Lits[1] = 2 * vB_i;
    sat_solver_addclause(pSat, Lits, Lits + 2);
}

int RunSAT(sat_solver* pSat, Cnf_Dat_t* pCnfA, Cnf_Dat_t* pCnfB, Aig_Obj_t* pPoK) {
  int Lits[2];
  int vA_yk = VarNums(pCnfA, pPoK);
  int vB_yk = VarNums(pCnfB, pPoK);
  Lits[0] = 2 * vA_yk;
  Lits[1] = 2 * vB_yk + 1;
  int status = sat_solver_solve(pSat, Lits, Lits + 2, 0, 0, 0, 0);
  return status;
}

void PrintResult(Aig_Man_t* pAig, sat_solver* pSat, Cnf_Dat_t* pCnfA, Cnf_Dat_t* pCnfB, int I, int status) {
  if (status == l_False) {
    cout << "symmetric" << endl;
  }
  else {
    cout << "asymmetric" << endl;
    string ptnA = "";
    string ptnB = "";
    for (int i = 0; i < pAig->vCis->nSize; ++i) {
      Aig_Obj_t* pPi = (Aig_Obj_t*) pAig->vCis->pArray[i];
      int vA = VarNums(pCnfA, pPi);
      int vB = VarNums(pCnfB, pPi);
      if (sat_solver_var_value(pSat, vA)) ptnA += '1';
      else                                ptnA += '0';
      if (sat_solver_var_value(pSat, vB)) ptnB += '1';
      else                                ptnB += '0';
    }
    if (ptnA[I] == '1') {
      cout << ptnA << endl;
      cout << ptnB << endl;
    }
    else {
      cout << ptnB << endl;
      cout << ptnA << endl;
    }
  }
}

int Lsv_CommandSymSat(Abc_Frame_t* pAbc, int argc, char** argv) {
  if (MissingInput(argc, 4)) return 1;
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int K = stoi(argv[1]);
  int I = stoi(argv[2]);
  int J = stoi(argv[3]);
  Abc_Obj_t* pPoK_obj = (Abc_Obj_t*) pNtk->vPos->pArray[K];
  Abc_Obj_t* pRootK = Abc_ObjFanin0(pPoK_obj);

  // step 1: extract the cone of yk
  Abc_Ntk_t* pNtkConeK = Abc_NtkCreateCone(pNtk, pRootK, Abc_ObjName(pRootK), 1);
  // step 2: derive a corresponding AIG citcuit
  Aig_Man_t* pAig = Abc_NtkToDar(pNtkConeK, 0, 1);
  Aig_Obj_t* pPoK = (Aig_Obj_t*) pAig->vCos->pArray[0];
  // step 3: initialize a SAT solver
  sat_solver* pSat = sat_solver_new();
  // step 4: obtain the CNF from AIG
  Cnf_Dat_t* pCnfA = Cnf_Derive(pAig, Aig_ManCoNum(pAig));
  // step 5: add the CNF to the SAT solver 
  pSat = (sat_solver *) Cnf_DataWriteIntoSolverInt( pSat, pCnfA, 1, 1 );
  // step 6: create a copy and add to the SAT solver
  Cnf_Dat_t* pCnfB = Cnf_Derive(pAig, Aig_ManCoNum(pAig));
  Cnf_DataLift( pCnfB, pCnfA->nVars );
  pSat = (sat_solver *) Cnf_DataWriteIntoSolverInt( pSat, pCnfB, 1, 1 );
  // step 7: add symmetric infromation
  AddSymmetric(pAig, pSat, pCnfA, pCnfB, I, J);
  // step 8: solve the sat problem
  int status = RunSAT(pSat, pCnfA, pCnfB, pPoK);
  // step 9: print result
  PrintResult(pAig, pSat, pCnfA, pCnfB, I, status);
  return 0;
}

void AddSymmetricEnb(Aig_Man_t* pAig, sat_solver* pSat, Cnf_Dat_t* pCnfA, Cnf_Dat_t* pCnfB, int* enable) {
  Aig_Obj_t* pPoK = (Aig_Obj_t*) pAig->vCos->pArray[0];
  int Lits[4];
  int FinSize = pAig->vCis->nSize;
  // (a) (vA_yk xor vB_yk)
    int vA_yk = VarNums(pCnfA, pPoK);
    int vB_yk = VarNums(pCnfB, pPoK);
    // (vAyk + vByk)
    Lits[0] = 2 * vA_yk;
    Lits[1] = 2 * vB_yk;
    sat_solver_addclause(pSat, Lits, Lits + 2);
    // (¬vAyk + ¬vByk)
    Lits[0] = 2 * vA_yk + 1;
    Lits[1] = 2 * vB_yk + 1;
    sat_solver_addclause(pSat, Lits, Lits + 2);
  // (b) (vA_t == vB_t) + enable_t
  for (int i = 0; i < FinSize; ++i) {
    enable[i] = sat_solver_addvar(pSat);
    Lits[2] = 2 * enable[i];
    Aig_Obj_t* pPi = (Aig_Obj_t*) pAig->vCis->pArray[i];
    int vA_t = VarNums(pCnfA, pPi);
    int vB_t = VarNums(pCnfB, pPi);
    // (vA_t + ¬vB_t + enable_t)
    Lits[0] = 2 * vA_t;
    Lits[1] = 2 * vB_t + 1;
    sat_solver_addclause(pSat, Lits, Lits + 3);
    // (¬vA_t + vB_t + enable_t)
    Lits[0] = 2 * vA_t + 1;
    Lits[1] = 2 * vB_t;
    sat_solver_addclause(pSat, Lits, Lits + 3);
  }
  // (c) (d)
  for (int i = 0; i < FinSize-1; ++i) {
    Lits[2] = 2 * enable[i] + 1;
    Aig_Obj_t* pPiI = (Aig_Obj_t*) pAig->vCis->pArray[i];
    int vA_i = VarNums(pCnfA, pPiI);
    int vB_i = VarNums(pCnfB, pPiI);
    for (int j = i + 1; j < FinSize; ++j) {
      Lits[3] = 2 * enable[j] + 1;
      Aig_Obj_t* pPiJ = (Aig_Obj_t*) pAig->vCis->pArray[j];
      int vA_j = VarNums(pCnfA, pPiJ);
      int vB_j = VarNums(pCnfB, pPiJ);
    // (c) (vA_i == vB_j) + ¬enable_i + ¬enable_j
      // (vA_i + ¬vB_j + ¬enable_i + ¬enable_j)
      Lits[0] = 2 * vA_i;
      Lits[1] = 2 * vB_j + 1;
      sat_solver_addclause(pSat, Lits, Lits + 4);
      // (¬vA_i + vB_j + ¬enable_i + ¬enable_j)
      Lits[0] = 2 * vA_i + 1;
      Lits[1] = 2 * vB_j;
      sat_solver_addclause(pSat, Lits, Lits + 4);     
    // (d) (vA_j == vB_i) + ¬enable_i + ¬enable_j
       // (vA_j + ¬vB_i + ¬enable_i + ¬enable_j)
      Lits[0] = 2 * vA_j;
      Lits[1] = 2 * vB_i + 1;
      sat_solver_addclause(pSat, Lits, Lits + 4);
      // (¬vA_j + vB_i + ¬enable_i + ¬enable_j)
      Lits[0] = 2 * vA_j + 1;
      Lits[1] = 2 * vB_i;
      sat_solver_addclause(pSat, Lits, Lits + 4);     
    }
  }
}

void RunSATIncre(sat_solver* pSat, int* enable, int FinSize) {
  int* Lits = new int[FinSize];
  for (int i = 0; i < FinSize; ++i) {
    Lits[i] = 2 * enable[i] + 1;
  }

  int status = sat_solver_solve(pSat, Lits, Lits + FinSize, 0, 0, 0, 0);
  if (status == l_True) {
    cout << "something wrong!" << endl;
  }
  else {
    vector<vector<int>> sym_pair;
    for (int i = 0; i < FinSize-1; ++i) {
      Lits[i] = 2 * enable[i];
      for (int j = i + 1; j < FinSize; ++j) {
        Lits[j] = 2 * enable[j];
        status = sat_solver_solve(pSat, Lits, Lits + FinSize, 0, 0, 0, 0);
        if (status == l_False) {
          vector<int> pair;
          pair.push_back(i);
          pair.push_back(j);
          sym_pair.push_back(pair);
        }
        Lits[j] = 2 * enable[j] + 1;
      }
      Lits[i] = 2 * enable[i] + 1;
    }
    if (sym_pair.size()) {
      cout << "symmetric" << endl;
      for (int i = 0; i < sym_pair.size(); ++i) {
        cout << sym_pair[i][0] << ' ' << sym_pair[i][1] << endl;
      }
    }
    else {
      cout << "asymmetric" << endl;
    }
  }
  delete[] Lits;
}

int Lsv_CommandSymAll(Abc_Frame_t* pAbc, int argc, char** argv) {
  if (MissingInput(argc, 2)) return 1;
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int K = stoi(argv[1]);
  Abc_Obj_t* pPoK_obj = (Abc_Obj_t*) pNtk->vPos->pArray[K];
  Abc_Obj_t* pRootK = Abc_ObjFanin0(pPoK_obj);

  // step 1: extract the cone of yk
  Abc_Ntk_t* pNtkConeK = Abc_NtkCreateCone(pNtk, pRootK, Abc_ObjName(pRootK), 1);
  // step 2: derive a corresponding AIG citcuit
  Aig_Man_t* pAig = Abc_NtkToDar(pNtkConeK, 0, 1);
  // step 3: initialize a SAT solver
  sat_solver* pSat = sat_solver_new();
  // step 4: obtain the CNF from AIG
  Cnf_Dat_t* pCnfA = Cnf_Derive(pAig, Aig_ManCoNum(pAig));
  // step 5: add the CNF to the SAT solver 
  pSat = (sat_solver *) Cnf_DataWriteIntoSolverInt( pSat, pCnfA, 1, 1 );
  // step 6: create a copy and add to the SAT solver
  Cnf_Dat_t* pCnfB = Cnf_Derive(pAig, Aig_ManCoNum(pAig));
  Cnf_DataLift( pCnfB, pCnfA->nVars );
  pSat = (sat_solver *) Cnf_DataWriteIntoSolverInt( pSat, pCnfB, 1, 1 );
  // step 7: add constraint
  int* enable = new int[pAig->vCis->nSize];
  AddSymmetricEnb(pAig, pSat, pCnfA, pCnfB, enable);
  // step 8: run SAT
  RunSATIncre(pSat, enable, pAig->vCis->nSize);

  delete[] enable;
  return 0;
}