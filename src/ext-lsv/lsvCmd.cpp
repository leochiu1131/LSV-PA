#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/cnf/cnf.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <set>
using namespace std;

extern "C"{
  Aig_Man_t* Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
}

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimBdd(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSymSAT(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSymBdd(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", Lsv_CommandSimBdd, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig", Lsv_CommandSimAig, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_sat", Lsv_CommandSymSAT, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_bdd", Lsv_CommandSymBdd, 0);
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
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  set<string> negPI; 
  Extra_UtilGetoptReset();
  

  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  if (!Abc_NtkIsBddLogic(pNtk)) {
    Abc_Print(-1, "not bdd network yet.\n");
    return 1;
  }
  if (argc != 2) {
    Abc_Print(-1, "wrong argument.\n");
    return 1;
  }
  
  int ithPi;
  Abc_Obj_t* pPi;
  Abc_NtkForEachPi( pNtk, pPi, ithPi) {
    char* PiName = Abc_ObjName(pPi);
    if (argv[1][ithPi] == '0') negPI.insert(string(PiName));
  }


  int ithPo;
  Abc_Obj_t* pPo;
  Abc_NtkForEachPo(pNtk, pPo, ithPo) { // change pRoot-> change pNext, Id
    Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo);  
    assert( Abc_NtkIsBddLogic(pRoot->pNtk) );
    DdManager* dd = (DdManager *)pRoot->pNtk->pManFunc;  
    DdNode* ddnode = (DdNode *)pRoot->pData;  // all change
    DdNode* simNode = ddnode;

    int ithFi;
    Abc_Obj_t* pFi;
    char** vNamesIn = (char**) Abc_NodeGetFaninNames(pRoot)->pArray;
    Abc_ObjForEachFanin( pRoot, pFi, ithFi) {
      DdNode* ddFi = Cudd_bddIthVar(dd, ithFi);
      if (negPI.count(string(vNamesIn[ithFi])) == 1) {
        ddFi = Cudd_Not(ddFi);
      }
      simNode = Cudd_Cofactor(dd, simNode, ddFi);
      assert(simNode);
    }
    cout << Abc_ObjName(pPo) << ": " << (simNode == DD_ONE(dd)) << endl;  
  }
  return 0;
}

void Lsv_SimAig(Abc_Ntk_t* pNtk, vector<int>& inputVec, vector<string>& outputVec) {
  int ithPi;
  Abc_Obj_t* pPi;

  Abc_Obj_t* pConst = Abc_NtkObj(pNtk, 0);
  pConst->iTemp = 0XFFFFFFFF;

  Abc_NtkForEachPi( pNtk, pPi, ithPi) {
    pPi->iTemp = inputVec[ithPi];  
  }

  int ithNode;
  Abc_Obj_t* pNode;
  Abc_NtkForEachNode(pNtk, pNode, ithNode) {
    // cout << pNode->Id << endl;
    int fin0vec = Abc_ObjFanin0(pNode)->iTemp;
    if (pNode->fCompl0) fin0vec = fin0vec ^ 0XFFFFFFFF;
    int fin1vec = Abc_ObjFanin1(pNode)->iTemp;
    if (pNode->fCompl1) fin1vec = fin1vec ^ 0XFFFFFFFF;
    pNode->iTemp = fin0vec & fin1vec;

    // Abc_ObjFaninC0(pNode)
  }

  int ithPo;
  Abc_Obj_t* pPo;
  Abc_NtkForEachPo(pNtk, pPo, ithPo) {
    Abc_Obj_t* poNode = Abc_ObjFanin0(pPo);
    int oPattern = poNode->iTemp; // this correct
    if (poNode->fCompl0) oPattern = oPattern ^ 0XFFFFFFFF;
    if (outputVec.size() == ithPo) {outputVec.push_back(""); }
    for (size_t i = 0; i < 32; ++i) {
      if (oPattern < 0) outputVec[ithPo].append("1");
      else outputVec[ithPo].append("0");
      oPattern = oPattern << 1;
    }
    // outputVec[ithPo].append(to_string(oPattern));
  }

  return;
}

int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  vector<int> inputVec;
  vector<int> nodeVec;
  vector<string> outputVec;
  Extra_UtilGetoptReset();
  

  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  
  if (argc != 2) {
    Abc_Print(-1, "wrong argument.\n");
    return 1;
  }
  
  ifstream patternFile(argv[1]);
  string pattern;
  size_t num_pattern = 0;
  while (patternFile >> pattern) {
    if (num_pattern % 32 == 0 && !inputVec.empty()) {
      // do sim
      Lsv_SimAig(pNtk, inputVec, outputVec);
    }

    if (num_pattern % 32 == 0) {
      if (inputVec.empty()) {
        for (size_t ithInput = 0; ithInput < pattern.size(); ++ithInput) {
          // cout << ithInput << endl;
          inputVec.push_back(0);
        }
      } else {
        for (size_t ithInput = 0; ithInput < pattern.size(); ++ithInput) {
          inputVec[ithInput] = 0;
        }
      }
    }


    for (size_t ithInput = 0; ithInput < pattern.size(); ++ithInput) {
      inputVec[ithInput] = inputVec[ithInput] << 1;
      if (pattern[ithInput] == '1') ++inputVec[ithInput];
    }
    ++num_pattern;
  }

  if (!inputVec.empty()) {
    for (size_t ithInput = 0; ithInput < pattern.size(); ++ithInput) {
      inputVec[ithInput] = inputVec[ithInput] << (32- num_pattern % 32);
    }
    Lsv_SimAig(pNtk, inputVec, outputVec);
  }

  Abc_Obj_t* pPo;
  int ithPo;
  Abc_NtkForEachPo(pNtk, pPo, ithPo) {
    cout << Abc_ObjName(pPo) << ": " << outputVec[ithPo].substr(0, num_pattern) << endl; 
  }

  // Lsv_NtkPrintNodes(pNtk);
  return 0;
}

int str_to_int(char* str) {
  char* ptr = str;
  int i = 0;
  while (*ptr != '\0') {
    if ('0' <= *ptr && *ptr <= '9') {
      i *= 10;
      i += (*ptr - '0');
    } else {
      cerr << "wrong command" << endl;
      return -1;
    }
    ptr++;
  }
  return i;
}

void add_equal_clause(sat_solver* solver, int i, int j, int fcompl = 0) {
  // cout << "add_clause!" << endl;
  lit s[2];
  s[0] = toLitCond(i, fcompl);
  s[1] = toLitCond(j, 1);
  // cout << s[0] << " " << s[1] << endl;
  sat_solver_addclause(solver, s, s+2);

  s[0] = toLitCond(i, 1-fcompl);
  // cout << "is comp?" << s[0]%2 << endl;
  s[1] = toLitCond(j, 0);
  // cout << s[0] << " " << s[1] << endl;
  sat_solver_addclause(solver, s, s+2);
}

int Lsv_CommandSymSAT(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  Extra_UtilGetoptReset();
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  
  if (argc != 4) {
    Abc_Print(-1, "wrong argument.\n");
    return 1;
  }
  int opin_k = str_to_int(argv[1]);
  int ipin_i = str_to_int(argv[2]);
  int ipin_j = str_to_int(argv[3]);

  Abc_Obj_t* po = Abc_NtkPo(pNtk, opin_k);
  Abc_Obj_t* p_k = Abc_ObjFanin0(po);
  
  Abc_Ntk_t* cone_k = Abc_NtkCreateCone(pNtk, p_k, Abc_ObjName(po), 1);
  p_k = Abc_NtkPo(cone_k, 0);
  p_k = Abc_ObjFanin0(p_k);

  Aig_Man_t* aig_k = Abc_NtkToDar(cone_k, 0, 1);
  
  // Aig_Man_t* aig_k_dup = Abc_NtkToDar(cone_k, 0, 0);
  
  sat_solver* solver = sat_solver_new();
  // solver->verbosity = 1;

  Cnf_Dat_t* cnf_k = Cnf_Derive(aig_k, 1);
  // int** c_ptr = cnf_k->pClauses;
  // for (int i = 0; i<cnf_k->nClauses ; i++) {
  //   int* lit_ptr = *c_ptr;
  //   c_ptr++;
  //   for (;lit_ptr != *(c_ptr);lit_ptr++) {
  //     cout << *lit_ptr << " ";
  //   }
  //   cout << endl;
  // }
  // for (int i = 0; i < cnf_k->pVarNums[p_k->Id]; i++) {
  //   cout << pVarNums[p_k->Id]
  // }
  
  solver = (sat_solver*)Cnf_DataWriteIntoSolverInt(solver, cnf_k, 1, 0); //finit?
  
  // Cnf_Dat_t* cnf_k_dup = Cnf_DataDup(cnf_k);
  Cnf_Dat_t* cnf_k_dup = Cnf_Derive(aig_k, 1);
  
  Cnf_DataLift(cnf_k_dup, cnf_k->nVars);
  // cout << cnf_k->nVars << endl;
  // c_ptr = cnf_k_dup->pClauses;
  // for (int i = 0; i<cnf_k_dup->nClauses ; i++) {
  //   int* lit_ptr = *c_ptr;
  //   c_ptr++;
  //   for (;lit_ptr != *(c_ptr);lit_ptr++) {
  //     cout << *lit_ptr << " ";
  //   }
  //   cout << endl;
  // }

  solver = (sat_solver*)Cnf_DataWriteIntoSolverInt(solver, cnf_k_dup, 2, 0);
  

  Abc_Obj_t* pPi_i = Abc_NtkPi(cone_k, ipin_i);
  Abc_Obj_t* pPi_j = Abc_NtkPi(cone_k, ipin_j);
  int i_in_k = cnf_k->pVarNums[pPi_i->Id];
  int i_in_kdup = cnf_k_dup->pVarNums[pPi_i->Id];
  int j_in_k = cnf_k->pVarNums[pPi_j->Id];
  int j_in_kdup = cnf_k_dup->pVarNums[pPi_j->Id];

  add_equal_clause(solver, i_in_k, j_in_kdup);
  add_equal_clause(solver, i_in_kdup, j_in_k);

  int ithPi;
  Abc_Obj_t* pPi;
  Abc_NtkForEachCi(cone_k, pPi, ithPi) {
    int cnf_var_k = cnf_k->pVarNums[pPi->Id];
    int cnf_var_kdup = cnf_k_dup->pVarNums[pPi->Id];  // does it need??
    // cout << cnf_var_k << " " << cnf_var_kdup << endl;
    if (ithPi != ipin_i && ithPi != ipin_j) {
      add_equal_clause(solver, cnf_var_k, cnf_var_kdup);
    }
  }

  int o_in_k = cnf_k->pVarNums[p_k->Id];
  int o_in_kdup = cnf_k_dup->pVarNums[p_k->Id];
  // cout << o_in_k << " " << o_in_kdup << endl;
  add_equal_clause(solver, o_in_k, o_in_kdup, 1);

  int result = sat_solver_solve_internal(solver);
  if (result == l_False) {
    cout << "symmetric" << endl;
  } else if (result == l_True) {
    cout << "asymmetric" << endl;
    Abc_NtkForEachCi(cone_k, pPi, ithPi) {
      int cnf_var_k = cnf_k->pVarNums[pPi->Id];
      if (sat_solver_var_value(solver, cnf_var_k)) {
        cerr << '1';
      } else {
        cerr << '0';
      }
    }
    cout << endl;
    Abc_NtkForEachCi(cone_k, pPi, ithPi) {
      int cnf_var_kdup = cnf_k_dup->pVarNums[pPi->Id];
      if (sat_solver_var_value(solver, cnf_var_kdup)) {
        cerr << '1';
      } else {
        cerr << '0';
      }
    }
    cout << endl;
  }


  // cout << opin_k << ipin_i << ipin_j << endl;
  // Abc_NtkCreateCone(pNtk,)
  return 0;
}

DdNode* bdd_cof_input(DdManager* dd, DdNode* po, int ithFi, bool is_comp) {
  DdNode* ddFi = Cudd_bddIthVar(dd, ithFi); Cudd_Ref(ddFi);
  if (is_comp) {
    DdNode* comp_buf = Cudd_Not(ddFi); Cudd_Ref(comp_buf);
    Cudd_RecursiveDeref(dd, ddFi);
    ddFi = comp_buf;
  }
  DdNode* po_cof = Cudd_Cofactor(dd, po, ddFi);
  // Cudd_Ref(po_cof);
  Cudd_RecursiveDeref(dd, ddFi);
  return po_cof;
}

int Lsv_CommandSymBdd(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  Extra_UtilGetoptReset();
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  
  if (argc != 4) {
    Abc_Print(-1, "wrong argument.\n");
    return 1;
  }
  int opin_k = str_to_int(argv[1]);
  int ipin_i_ntk = str_to_int(argv[2]);
  int ipin_j_ntk = str_to_int(argv[3]);

  Abc_Obj_t* p_k = Abc_NtkPo(pNtk, opin_k);
  p_k = Abc_ObjFanin0(p_k);
  DdManager* dd = (DdManager *)p_k->pNtk->pManFunc;
  DdNode* d_k = (DdNode *)p_k->pData;

  vector<int> ntk_to_bddid;
  int ithPi;
  Abc_Obj_t* pPi;
  char** vNamesIn = (char**) Abc_NodeGetFaninNames(p_k)->pArray;
  Abc_NtkForEachPi( pNtk, pPi, ithPi) {
    char* PiName = Abc_ObjName(pPi);
    bool found = false;
    for (int i = 0; i < Abc_ObjFaninNum(p_k); ++i) {
      if (string(PiName) == string(vNamesIn[i])) {
        ntk_to_bddid.push_back(i);
        found = true;
        break;
      }
    }
    if (!found) ntk_to_bddid.push_back(-1);
  }
  // for (auto v: ntk_to_bddid) {cout << v << " ";}
  // cout << endl;

  int ipin_i = ntk_to_bddid[ipin_i_ntk];
  int ipin_j = ntk_to_bddid[ipin_j_ntk];
  DdNode *d_k_01, *d_k_10;

  if (ipin_i == ipin_j) {
    cout << "symmetric" << endl;
    return 0;
  }
  if (ipin_i == -1) {
    ipin_i = ipin_j;
    d_k_01 = bdd_cof_input(dd, d_k, ipin_j, false);
    Cudd_Ref(d_k_01);
    d_k_10 = bdd_cof_input(dd, d_k, ipin_j, true);
    Cudd_Ref(d_k_10);
  } else if (ipin_j == -1) {
    ipin_j = ipin_i;
    d_k_01 = bdd_cof_input(dd, d_k, ipin_i, true);
    Cudd_Ref(d_k_01);
    d_k_10 = bdd_cof_input(dd, d_k, ipin_i, false);
    Cudd_Ref(d_k_10);
  } else {
    DdNode* d_k_0 = bdd_cof_input(dd, d_k, ipin_i, true);
    Cudd_Ref(d_k_0);
    d_k_01 = bdd_cof_input(dd, d_k_0, ipin_j, false);
    Cudd_Ref(d_k_01);
    Cudd_RecursiveDeref(dd, d_k_0);

    DdNode* d_k_1 = bdd_cof_input(dd, d_k, ipin_i, false);
    Cudd_Ref(d_k_1);
    d_k_10 = bdd_cof_input(dd, d_k_1, ipin_j, true);
    Cudd_Ref(d_k_10);
    Cudd_RecursiveDeref(dd, d_k_1);

    if (d_k_01 == d_k_10) {
      cout << "symmetric" << endl;
      return 0;
    }
  }

  // DdNode* d_k_0 = bdd_cof_input(dd, d_k, ipin_i, false);
  // Cudd_Ref(d_k_0);
  // DdNode* d_k_01 = bdd_cof_input(dd, d_k_0, ipin_j, true);
  // Cudd_Ref(d_k_01);
  // Cudd_RecursiveDeref(dd, d_k_0);
  // // cout << 1 << endl;
// 
  // // DdNode* bCube1 = Cudd_bddAnd( dd, Cudd_Not( dd->vars[ipin_j] ), dd->vars[ipin_i] );   Cudd_Ref( bCube1 );
  // // DdNode* bCof01 = Cudd_Cofactor( dd, d_k, bCube1 );  Cudd_Ref( bCof01 );
  // // cout << bCof01 << " " << d_k_01 << endl;
  // // Cudd_RecursiveDeref(dd, d_k_0);
// 
  // DdNode* d_k_1 = bdd_cof_input(dd, d_k, ipin_i, true);
  // Cudd_Ref(d_k_1);
  // DdNode* d_k_10 = bdd_cof_input(dd, d_k_1, ipin_j, false);
  // Cudd_Ref(d_k_10);
  // Cudd_RecursiveDeref(dd, d_k_1);
  // 
  // // cout << 2 << endl;
  // // DdNode* bCube2 = Cudd_bddAnd( dd, Cudd_Not( dd->vars[ipin_i] ), dd->vars[ipin_j] );   Cudd_Ref( bCube2 );
  // // DdNode* bCof10 = Cudd_Cofactor( dd, d_k, bCube2 );  Cudd_Ref( bCof10 );
  // // cout << bCof10 << " " << d_k_10 << endl;
  // // Cudd_RecursiveDeref(dd, d_k_1);
// 
  // // cout << d_k_01 << " " << d_k_10 << endl;
  // // cout << Extra_bddCheckVarsSymmetricNaive(dd, d_k, ipin_i, ipin_j) << endl;
  // // cout << Extra_bddCheckVarsSymmetric(dd, d_k, ipin_i, ipin_j) << endl;
  
  // if (d_k_01 == d_k_10) {
  //   cout << "symmetric" << endl;
  // } else {
  int nPi = Abc_NtkPiNum(pNtk);
  char a[nPi + 1];
  a[nPi] = '\0';
  for (int i_ntk = 0; i_ntk < nPi; i_ntk++) {
    int i = ntk_to_bddid[i_ntk];
    if (i == ipin_i || i == ipin_j) {a[i_ntk] = '-'; continue;}
    else if (i == -1) {a[i_ntk] = '0'; continue;}
    DdNode* buff_k_01 = bdd_cof_input(dd, d_k_01, i, false); Cudd_Ref(buff_k_01);
    DdNode* buff_k_10 = bdd_cof_input(dd, d_k_10, i, false); Cudd_Ref(buff_k_10);
    if (buff_k_01 == buff_k_10) {
      Cudd_RecursiveDeref(dd, buff_k_01);
      Cudd_RecursiveDeref(dd, buff_k_10);
      buff_k_01 = bdd_cof_input(dd, d_k_01, i, true);
      buff_k_10 = bdd_cof_input(dd, d_k_10, i, true);
      Cudd_Ref(buff_k_01);
      Cudd_Ref(buff_k_10);
      if (buff_k_01 == buff_k_10) cout << "sth_wrong" << endl;
      a[i_ntk] = '0';
    } else {
      a[i_ntk] = '1';
    }
    Cudd_RecursiveDeref(dd, d_k_01);
    Cudd_RecursiveDeref(dd, d_k_10);
    d_k_01 = buff_k_01;
    d_k_10 = buff_k_10;
  }
  cout << "asymmetric" << endl;
  a[ipin_i_ntk] = '0'; a[ipin_j_ntk] = '1';
  cout << a << endl;
  a[ipin_j_ntk] = '0'; a[ipin_i_ntk] = '1';
  cout << a << endl;
  // }

  return 0;
}