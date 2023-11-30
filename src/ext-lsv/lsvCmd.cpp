#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "bdd/cudd/cudd.h"
#include "bdd/cudd/cuddInt.h"
#include "aig/aig/aig.h"

#include <iostream>
#include <cstring>
#include <vector>

#include "sat/cnf/cnf.h"
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


void Lsv_SymBdd(Abc_Ntk_t* pNtk, char* str_k, char* str_i, char* str_j) {
  Abc_Obj_t* pObj;
  Abc_Obj_t* pPo;

  int i = std::stoi(str_i);
  int j = std::stoi(str_j);
  int k = std::stoi(str_k);

  if(i == j){
    printf("symmetric\n");
    return;
  }
  
  pPo = Abc_NtkPo(pNtk, k);
  Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo);
  char** vNamesIn = (char**) Abc_NodeGetFaninNames(pRoot)->pArray;
  int vNamesIn_size = Abc_NodeGetFaninNames(pRoot)->nSize;

  if( !(Abc_NtkIsBddLogic(pRoot->pNtk)) ) {
    printf("This command is for BDD only!\nPlease use \"collapse\" first.\n");
    return;
  }

  // map the original input ordering to the ordering in BDD
  std::vector<int> orderMap2Bdd; // -1 for non-PI input
  // map the input ordering in BDD to the original ordering
  std::vector<int> orderMap2Origin(vNamesIn_size, -1);
  int i_pi;
  Abc_NtkForEachPi(pNtk, pObj, i_pi) {
    // linear search for PI names
    orderMap2Bdd.emplace_back(-1);
    for(int ithBddPi = 0; ithBddPi < vNamesIn_size; ++ithBddPi){
      if(strcmp(vNamesIn[ithBddPi], Abc_ObjName(pObj)) == 0){
        orderMap2Origin[ithBddPi] = i_pi;
        orderMap2Bdd.back() = ithBddPi;
        break;
      }
    }
  }

  // printf("\n");
  // printf("%s", Abc_ObjName(pPo));
  // printf("\n");
  // for(int i_pi : orderMap2Bdd){
  //   if(i_pi != -1)
  //     printf("%s, ", vNamesIn[i_pi]);
  // }
  // printf("\n");
  // for(int i_pi = 0; i_pi < vNamesIn_size; ++i_pi){
  //   printf("%s, ", vNamesIn[i_pi]);
  // }
  // printf("\n");

  DdManager * dd = (DdManager *)pRoot->pNtk->pManFunc;  
  DdNode* ddnode = (DdNode *)pRoot->pData;
  Cudd_Ref(ddnode);
  DdNode* ddnode_cof_i0;
  DdNode* ddnode_cof_i0j1;
  DdNode* ddnode_cof_i1;
  DdNode* ddnode_cof_i1j0;
  DdNode* ithVar = nullptr;
  DdNode* jthVar = nullptr;
  DdNode* ddnode_xor;
  if(orderMap2Bdd[i] != -1){
    ithVar = Cudd_bddIthVar(dd, orderMap2Bdd[i]);
    Cudd_Ref(ithVar);
  }
  if(orderMap2Bdd[j] != -1){
    jthVar = Cudd_bddIthVar(dd, orderMap2Bdd[j]);
    Cudd_Ref(jthVar);
  }

  // cofactor i: 0, j: 1
  if(ithVar != nullptr){
    ddnode_cof_i0 = Cudd_Cofactor(dd, ddnode, Cudd_Not(ithVar));
    Cudd_Ref(ddnode_cof_i0);
  }
  else{
    ddnode_cof_i0 = ddnode;
    Cudd_Ref(ddnode_cof_i0);
  }

  if(jthVar != nullptr){
    ddnode_cof_i0j1 = Cudd_Cofactor(dd, ddnode_cof_i0, jthVar);
    Cudd_Ref(ddnode_cof_i0j1);
    Cudd_RecursiveDeref(dd, ddnode_cof_i0);
  }
  else{
    ddnode_cof_i0j1 = ddnode_cof_i0;
    Cudd_Ref(ddnode_cof_i0j1);
    Cudd_RecursiveDeref(dd, ddnode_cof_i0);
  }

  // cofactor i: 1, j: 0
  if(ithVar != nullptr){
    ddnode_cof_i1 = Cudd_Cofactor(dd, ddnode, ithVar);
    Cudd_Ref(ddnode_cof_i1);
    Cudd_RecursiveDeref(dd, ithVar);
  }
  else{
    ddnode_cof_i1 = ddnode;
    Cudd_Ref(ddnode_cof_i1);
  }

  if(jthVar != nullptr){
    ddnode_cof_i1j0 = Cudd_Cofactor(dd, ddnode_cof_i1, Cudd_Not(jthVar));
    Cudd_Ref(ddnode_cof_i1j0);
    Cudd_RecursiveDeref(dd, jthVar);
    Cudd_RecursiveDeref(dd, ddnode_cof_i1);
  }
  else{
    ddnode_cof_i1j0 = ddnode_cof_i1;
    Cudd_Ref(ddnode_cof_i1j0);
    Cudd_RecursiveDeref(dd, ddnode_cof_i1);
  }

  // deref original Po
  Cudd_RecursiveDeref(dd, ddnode);

  // miter
  ddnode_xor = Cudd_bddXor(dd, ddnode_cof_i0j1, ddnode_cof_i1j0);
  Cudd_Ref(ddnode_xor);
  Cudd_RecursiveDeref(dd, ddnode_cof_i0j1);
  Cudd_RecursiveDeref(dd, ddnode_cof_i1j0);

  if(ddnode_xor == Cudd_Not(DD_ONE(dd)) || ddnode_xor == DD_ZERO(dd)){
    printf("symmetric\n");
  }
  else{
    printf("asymmetric\n");

    // find a path to const. 1
    char* assign = new char(dd->size);
    Cudd_bddPickOneCube(dd, ddnode_xor, assign);
    std::vector<int> counterex(Abc_NtkPiNum(pNtk), 0);
    // reorder and assign for DC
    for(int i_ch = 0; i_ch < vNamesIn_size; ++i_ch){
      if(assign[i_ch] != 2){
        if(orderMap2Origin[i_ch] != -1){
          // printf("%d, %d, %d\n", i_ch, orderMap2Origin[i_ch], assign[i_ch]);
          counterex[orderMap2Origin[i_ch]] = assign[i_ch];
        }
      }
    }
    // for(int i_assign = 0; i_assign < vNamesIn_size; ++i_assign){
    //   printf("%d", assign[i_assign]);
    // }
    // printf("\n");

    for(int i_assign = 0; i_assign < Abc_NtkPiNum(pNtk); ++i_assign){
      if(i_assign == i) printf("0");
      else if(i_assign == j) printf("1");
      else printf("%d", counterex[i_assign]);
    }
    printf("\n");
    for(int i_assign = 0; i_assign < Abc_NtkPiNum(pNtk); ++i_assign){
      if(i_assign == i) printf("1");
      else if(i_assign == j) printf("0");
      else printf("%d", counterex[i_assign]);
    }
    printf("\n");
  }

  Cudd_RecursiveDeref(dd, ddnode_xor);
  
}

int Lsv_CommandSymBdd(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c;
  int nArgc;
  char **nArgv;
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
    switch (c) {
      case 'h':
        goto usage;
      default:
        goto usage;
    }
  }

  nArgc = argc - globalUtilOptind;
  nArgv = argv + globalUtilOptind;

  // check for input format
  if( nArgc != 3 ) {
    printf("Wrong input format!\n");
    goto usage;
  }

  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  Lsv_SymBdd(pNtk, nArgv[0], nArgv[1], nArgv[2]);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sym_bdd [-h] <k> <i> <j>\n");
  Abc_Print(-2, "\t      : do symmetry checking with BDD\n");
  Abc_Print(-2, "\t      : k: index of output pin\n");
  Abc_Print(-2, "\t      : i, j: indeces of different input pins\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

// ----------------------------------------------------------

void Lsv_SymSat(Abc_Ntk_t* pNtk, char* str_k, char* str_i, char* str_j) {
  Aig_Obj_t* pAigObj;
  Abc_Obj_t* pPo;
  int i_pi;

  int i = std::stoi(str_i);
  int j = std::stoi(str_j);
  int k = std::stoi(str_k);

  if(i >= Abc_NtkPiNum(pNtk) || j >= Abc_NtkPiNum(pNtk)){
    printf("Exceed the maximum input index %d!!\n", Abc_NtkPiNum(pNtk)-1);
    return;
  }

  if(i == j){
    printf("i cannot be equal to j!!\n");
    return;
  }
  
  pPo = Abc_NtkPo(pNtk, k);
  Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo);

  if( !(Abc_NtkIsStrash(pNtk)) ) {
    printf("This command is for AIG only!\nPlease use \"strash\" first.\n");
    return;
  }

  // focus on one cone
  Abc_Ntk_t* pNtk_y_k = Abc_NtkCreateCone(pNtk, pRoot, "y_k", 1);
  pPo = Abc_NtkPo(pNtk_y_k, 0);
  pRoot = Abc_ObjFanin0(pPo); // its id is the same as its id in aig 

  // convert to aig
  Aig_Man_t* pMan_y_k = Abc_NtkToDar(pNtk_y_k, 0, 0);
  Cnf_Dat_t* pCnf = Cnf_Derive(pMan_y_k, 1);

  // printf("input ids: ");
  // Aig_ManForEachCi(pMan_y_k, pAigObj, i_pi){
  //   printf("%d, ", pCnf->pVarNums[pAigObj->Id]);
  // }
  // printf("\n");
  // printf("output id: %d , %d\n", pRoot->Id, pCnf->pVarNums[pRoot->Id]);
 
  sat_solver* pSolver = sat_solver_new();
  // C_A
  Cnf_DataWriteIntoSolverInt(pSolver, pCnf, 1, 0);
  // C_B
  Cnf_DataLift(pCnf, pCnf->nVars);
  Cnf_DataWriteIntoSolverInt(pSolver, pCnf, 1, 0);

  // input equivalence
  int CnfA_VarId_i = 0;
  int CnfB_VarId_i = 0;
  int CnfA_VarId_j = 0;
  int CnfB_VarId_j = 0;
  std::vector<lit> vLit(2);
  Aig_ManForEachCi(pMan_y_k, pAigObj, i_pi){

    int CnfB_varId = pCnf->pVarNums[pAigObj->Id];
    int CnfA_varId = CnfB_varId - pCnf->nVars;

    if(i_pi == i){
      CnfA_VarId_i = CnfA_varId;
      CnfB_VarId_i = CnfB_varId;
      continue;
    }

    if(i_pi == j){
      CnfA_VarId_j = CnfA_varId;
      CnfB_VarId_j = CnfB_varId;
      continue;
    }

    vLit[0] = lit_neg(toLit(CnfA_varId));
    vLit[1] = toLit(CnfB_varId);
    sat_solver_addclause(pSolver, &vLit[0], &vLit[1]+1);
    vLit[0] = toLit(CnfA_varId);
    vLit[1] = lit_neg(toLit(CnfB_varId));
    sat_solver_addclause(pSolver, &vLit[0], &vLit[1]+1);
  }
  
  // i j equivalence
  vLit[0] = lit_neg(toLit(CnfA_VarId_i));
  vLit[1] = toLit(CnfB_VarId_j);
  sat_solver_addclause(pSolver, &vLit[0], &vLit[1]+1);
  vLit[0] = toLit(CnfA_VarId_i);
  vLit[1] = lit_neg(toLit(CnfB_VarId_j));
  sat_solver_addclause(pSolver, &vLit[0], &vLit[1]+1);

  vLit[0] = lit_neg(toLit(CnfA_VarId_j));
  vLit[1] = toLit(CnfB_VarId_i);
  sat_solver_addclause(pSolver, &vLit[0], &vLit[1]+1);
  vLit[0] = toLit(CnfA_VarId_j);
  vLit[1] = lit_neg(toLit(CnfB_VarId_i));
  sat_solver_addclause(pSolver, &vLit[0], &vLit[1]+1);

  // xor for symmetry cheching
  int CnfB_VarId_yk = pCnf->pVarNums[pRoot->Id];
  int CnfA_VarId_yk = CnfB_VarId_yk - pCnf->nVars;
  vLit[0] = toLit(CnfA_VarId_yk);
  vLit[1] = toLit(CnfB_VarId_yk);
  sat_solver_addclause(pSolver, &vLit[0], &vLit[1]+1);
  vLit[0] = lit_neg(toLit(CnfA_VarId_yk));
  vLit[1] = lit_neg(toLit(CnfB_VarId_yk));
  sat_solver_addclause(pSolver, &vLit[0], &vLit[1]+1);

  lbool status = sat_solver_solve(pSolver, 0, 0, 0, 0, 0, 0);
  
  if(status == l_False){
    printf("symmetric\n");
  }
  else{
    printf("asymmetric\n");
    int* assign = new int[Aig_ManCiNum(pMan_y_k)];
    Aig_ManForEachCi(pMan_y_k, pAigObj, i_pi){
      int CnfB_varId = pCnf->pVarNums[pAigObj->Id];
      assign[i_pi] = sat_solver_var_value(pSolver, CnfB_varId);
    }
    assign[i] = 0;
    assign[j] = 1;
    for(i_pi = 0; i_pi < Aig_ManCiNum(pMan_y_k); ++i_pi){
      printf("%d", assign[i_pi]);
    }
    printf("\n");
    assign[i] = 1;
    assign[j] = 0;
    for(i_pi = 0; i_pi < Aig_ManCiNum(pMan_y_k); ++i_pi){
      printf("%d", assign[i_pi]);
    }
    printf("\n");
    delete assign;
  }

  sat_solver_delete(pSolver);

}

int Lsv_CommandSymSat(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c;
  int nArgc;
  char **nArgv;
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
    switch (c) {
      case 'h':
        goto usage;
      default:
        goto usage;
    }
  }

  nArgc = argc - globalUtilOptind;
  nArgv = argv + globalUtilOptind;

  // check for input format
  if( nArgc != 3 ) {
    printf("Wrong input format!\n");
    goto usage;
  }

  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }

  Lsv_SymSat(pNtk, nArgv[0], nArgv[1], nArgv[2]);

  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sym_sat [-h] <k> <i> <j>\n");
  Abc_Print(-2, "\t      : do symmetry checking with SAT\n");
  Abc_Print(-2, "\t      : k: index of output pin\n");
  Abc_Print(-2, "\t      : i, j: indeces of different input pins\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

// ----------------------------------------------------------

void Lsv_SymAll(Abc_Ntk_t* pNtk, char* str_k) {
  Aig_Obj_t* pAigObj_i, * pAigObj_j;
  Abc_Obj_t* pPo;
  int i_pi, j_pi;
  int i, j;
  int k = std::stoi(str_k);
  lbool status;
  
  pPo = Abc_NtkPo(pNtk, k);
  Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo);

  if( !(Abc_NtkIsStrash(pNtk)) ) {
    printf("This command is for AIG only!\nPlease use \"strash\" first.\n");
    return;
  }

  // sym map
  std::vector<std::vector<char>> sym_map(Abc_NtkPiNum(pNtk));
  for(i = 0; i < sym_map.size(); ++i){
    sym_map[i].resize(Abc_NtkPiNum(pNtk) - i - 1, 0);
  }

  // focus on one cone
  Abc_Ntk_t* pNtk_y_k = Abc_NtkCreateCone(pNtk, pRoot, "y_k", 1);
  pPo = Abc_NtkPo(pNtk_y_k, 0);
  pRoot = Abc_ObjFanin0(pPo); // its id is the same as its id in aig 

  // convert to aig
  Aig_Man_t* pMan_y_k = Abc_NtkToDar(pNtk_y_k, 0, 0);
  Cnf_Dat_t* pCnf = Cnf_Derive(pMan_y_k, 1);

  sat_solver* pSolver = sat_solver_new();
  // C_A
  Cnf_DataWriteIntoSolverInt(pSolver, pCnf, 1, 0);
  // C_B
  Cnf_DataLift(pCnf, pCnf->nVars);
  Cnf_DataWriteIntoSolverInt(pSolver, pCnf, 1, 0);

  std::vector<lit> vLit(Aig_ManCiNum(pMan_y_k));

  // xor for symmetry cheching
  int CnfB_VarId_yk = pCnf->pVarNums[pRoot->Id];
  int CnfA_VarId_yk = CnfB_VarId_yk - pCnf->nVars;
  vLit[0] = toLit(CnfA_VarId_yk);
  vLit[1] = toLit(CnfB_VarId_yk);
  sat_solver_addclause(pSolver, &vLit[0], &vLit[1]+1);
  vLit[0] = lit_neg(toLit(CnfA_VarId_yk));
  vLit[1] = lit_neg(toLit(CnfB_VarId_yk));
  sat_solver_addclause(pSolver, &vLit[0], &vLit[1]+1);

  int v_H_0_id = pSolver->size;
  sat_solver_setnvars(pSolver, Aig_ManCiNum(pMan_y_k));

  // input equivalence
  Aig_ManForEachCi(pMan_y_k, pAigObj_i, i_pi){

    int v_B_t1_id = pCnf->pVarNums[pAigObj_i->Id];
    int v_A_t1_id = v_B_t1_id - pCnf->nVars;
    int v_H_t1_id = v_H_0_id + i_pi;

    // (vA(t) = vB(t)) ∨ vH(t)
    // = (vA(t) ∨ ¬vB(t) ∨ vH(t)) (¬vA(t) ∨ vB(t) ∨ vH(t))
    vLit[0] = toLitCond(v_A_t1_id, 0);
    vLit[1] = toLitCond(v_B_t1_id, 1);
    vLit[2] = toLitCond(v_H_t1_id, 0);
    sat_solver_addclause(pSolver, &vLit[0], &vLit[2]+1);
    vLit[0] = toLitCond(v_A_t1_id, 1);
    vLit[1] = toLitCond(v_B_t1_id, 0);
    sat_solver_addclause(pSolver, &vLit[0], &vLit[2]+1);

    Aig_ManForEachCi(pMan_y_k, pAigObj_j, j_pi){
      if(j_pi <= i_pi) continue;
      int v_B_t2_id = pCnf->pVarNums[pAigObj_j->Id];
      int v_A_t2_id = v_B_t2_id - pCnf->nVars;
      int v_H_t2_id = v_H_0_id + j_pi;

      // (vA(t1) = vB(t2)) ∨ ¬vH(t1) ∨ ¬vH(t2)
      // = (vA(t1) ∨ ¬vB(t2) ∨ ¬vH(t1) ∨ ¬vH(t2)) (¬vA(t1) ∨ vB(t2) ∨ ¬vH(t1) ∨ ¬vH(t2)) 
      vLit[0] = toLitCond(v_A_t1_id, 0);
      vLit[1] = toLitCond(v_B_t2_id, 1);
      vLit[2] = toLitCond(v_H_t1_id, 1);
      vLit[3] = toLitCond(v_H_t2_id, 1);
      sat_solver_addclause(pSolver, &vLit[0], &vLit[3]+1);
      vLit[0] = toLitCond(v_A_t1_id, 1);
      vLit[1] = toLitCond(v_B_t2_id, 0);
      sat_solver_addclause(pSolver, &vLit[0], &vLit[3]+1);

      // (vA(t2) = vB(t1)) ∨ ¬vH(t1) ∨ ¬vH(t2)
      // = (vA(t2) ∨ ¬vB(t1) ∨ ¬vH(t1) ∨ ¬vH(t2)) (¬vA(t2) ∨ vB(t1) ∨ ¬vH(t1) ∨ ¬vH(t2)) 
      vLit[0] = toLitCond(v_A_t2_id, 0);
      vLit[1] = toLitCond(v_B_t1_id, 1);
      vLit[2] = toLitCond(v_H_t1_id, 1);
      vLit[3] = toLitCond(v_H_t2_id, 1);
      sat_solver_addclause(pSolver, &vLit[0], &vLit[3]+1);
      vLit[0] = toLitCond(v_A_t2_id, 1);
      vLit[1] = toLitCond(v_B_t1_id, 0);
      sat_solver_addclause(pSolver, &vLit[0], &vLit[3]+1);
      
    }

  }

  for(i = 0; i < Aig_ManCiNum(pMan_y_k); ++i){
    for(j = i + 1; j < Aig_ManCiNum(pMan_y_k); ++j){
      int row_i = i;
      int col_j = j - i - 1;

      // induced by transitivity
      if(sym_map[row_i][col_j] == 1){
        printf("%d %d\n", i, j);
        continue;
      }

      // set control variables
      for(int t = 0; t < Aig_ManCiNum(pMan_y_k); ++t){
        if(t == i || t == j){
          vLit[t] = toLitCond(v_H_0_id + t, 0);
          continue;
        }
        vLit[t] = toLitCond(v_H_0_id + t, 1);
      }

      status = sat_solver_solve(pSolver, &vLit[0], &vLit[0] + Aig_ManCiNum(pMan_y_k), 0, 0, 0, 0);

      if(status == l_False){
        printf("%d %d\n", i, j);
        sym_map[row_i][col_j] = 1;
      }
    }
    
    // transitivity
    for(int j1 = i + 1; j1 < Aig_ManCiNum(pMan_y_k); ++j1){
      for(int j2 = j1 + 1; j2 < Aig_ManCiNum(pMan_y_k); ++j2){
        if(sym_map[j1][j2 - j1 - 1] == 1){
          continue;
        }
        int row_i = i;
        int col_j1 = j1 - i - 1;
        int col_j2 = j2 - i - 1;
        if(sym_map[row_i][col_j1] == 1 && sym_map[row_i][col_j2] == 1){
          // printf("transitivity! %d %d\n", j1, j2);
          sym_map[j1][j2 - j1 - 1] = 1;
        }
      }
    }
  }

  sat_solver_delete(pSolver);
  
}

int Lsv_CommandSymAll(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c;
  int nArgc;
  char **nArgv;
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
    switch (c) {
      case 'h':
        goto usage;
      default:
        goto usage;
    }
  }

  nArgc = argc - globalUtilOptind;
  nArgv = argv + globalUtilOptind;

  // check for input format
  if( nArgc != 1 ) {
    printf("Wrong input format!\n");
    goto usage;
  }

  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }

  Lsv_SymAll(pNtk, nArgv[0]);

  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sym_all [-h] <k>\n");
  Abc_Print(-2, "\t      : do symmetry checking for all input variables with Incremental SAT\n");
  Abc_Print(-2, "\t      : k: index of output pin\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

// ----------------------------------------------------------

void Lsv_SimBdd(Abc_Ntk_t* pNtk, int num_input, char* inputChars) {
  Abc_Obj_t* pObj;
  Abc_Obj_t* pPo;
  int i;
  int ithPo;

  // construct the input pattern
  std::vector<int> inputPattern;
  for(int ithIn = 0; ithIn < num_input; ++ithIn){
    inputPattern.emplace_back(int(inputChars[ithIn] - '0'));
  }

  Abc_NtkForEachPo(pNtk, pPo, ithPo) {
    Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo);
    char** vNamesIn = (char**) Abc_NodeGetFaninNames(pRoot)->pArray;
    int vNamesIn_size = Abc_NodeGetFaninNames(pRoot)->nSize;

    if( !(Abc_NtkIsBddLogic(pRoot->pNtk)) ) {
      printf("This command is for BDD only!\nPlease use \"collapse\" first.\n");
      return;
    }

    // map the original input ordering to the ordering in BDD
    std::vector<int> orderMap; // -1 for non-PI input
    Abc_NtkForEachPi(pNtk, pObj, i) {
      // linear search for PI names
      orderMap.emplace_back(-1);
      for(int ithBddPi = 0; ithBddPi < vNamesIn_size; ++ithBddPi){
        if(strcmp(vNamesIn[ithBddPi], Abc_ObjName(pObj)) == 0){
          orderMap.back() = ithBddPi;
          break;
        }
      }
    }

    DdManager * dd = (DdManager *)pRoot->pNtk->pManFunc;
    DdNode* ddnode_cof = (DdNode *)pRoot->pData;
    Cudd_Ref(ddnode_cof);
    for(int ithInput = 0; ithInput < num_input; ++ithInput){
      int ithVar = orderMap[ithInput];
      if(ithVar != -1){
        DdNode* var = Cudd_bddIthVar(dd, ithVar);
        Cudd_Ref(var);
        DdNode* ddnode_temp = ddnode_cof;
        Cudd_Ref(ddnode_temp);
        Cudd_RecursiveDeref(dd, ddnode_cof);
        if(inputPattern[ithInput] == 1){
          ddnode_cof = Cudd_Cofactor(dd, ddnode_temp, var);
        }
        else{
          ddnode_cof = Cudd_Cofactor(dd, ddnode_temp, Cudd_Not(var));
        }
        Cudd_Ref(ddnode_cof);
        Cudd_RecursiveDeref(dd, ddnode_temp);
        Cudd_RecursiveDeref(dd, var);
      }
    }
    if (ddnode_cof == Cudd_Not(DD_ONE(dd)) || ddnode_cof == DD_ZERO(dd)) {
      printf("%s: %d\n", Abc_ObjName(pPo), 0);
    }
    else{
      printf("%s: %d\n", Abc_ObjName(pPo), 1);
    }
    Cudd_RecursiveDeref(dd, ddnode_cof);
  }
}

int Lsv_CommandSimBdd(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c;
  int nArgc;
  char **nArgv;
  int num_input = 0;
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
    switch (c) {
      case 'h':
        goto usage;
      default:
        goto usage;
    }
  }

  nArgc = argc - globalUtilOptind;
  nArgv = argv + globalUtilOptind;

  // check for input format
  if( nArgc != 1 ) {
    printf("Wrong input format!\n");
    goto usage;
  }
  for(int ithCh = 0; nArgv[0][ithCh] != '\0'; ++ithCh){
    if( nArgv[0][ithCh] != '0' && nArgv[0][ithCh] != '1'){
      printf("Wrong input format!\n");
      goto usage;
    }
    ++num_input;
  }
  if( num_input != Abc_NtkPiNum(pNtk) ) {
    printf("Wrong input number!\nPlease enter %d input(s).\n", Abc_NtkPiNum(pNtk));
    return 1;
  }

  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  Lsv_SimBdd(pNtk, num_input, nArgv[0]);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sim_bdd [-h] <input pattern>\n");
  Abc_Print(-2, "\t      : do simulations for a given BDD and an input pattern\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

// ----------------------------------------------------------

void Lsv_SimAig_32bit_parallel(Abc_Ntk_t* pNtk, int* inputPatterns, std::vector<std::vector<int>>& result){
  int i;
  Abc_Obj_t * pNode;
  int inputVal0, inputVal1;

  Abc_NtkForEachCi(pNtk, pNode, i){
    pNode->iTemp = inputPatterns[i];
  }
  Abc_NtkForEachNode(pNtk, pNode, i)
  {
    inputVal0 = (Abc_ObjFanin0(pNode)->iTemp) ^ (0xffffffff * Abc_ObjFaninC0(pNode)); // complement
    inputVal1 = (Abc_ObjFanin1(pNode)->iTemp) ^ (0xffffffff * Abc_ObjFaninC1(pNode));
    pNode->iTemp = (inputVal0 & inputVal1);
  }
  Abc_NtkForEachPo(pNtk, pNode, i){
    result[i].emplace_back(0);
    result[i].back() = (Abc_ObjFanin0(pNode)->iTemp) ^ (0xffffffff * Abc_ObjFaninC0(pNode));
  }

}

int Lsv_SimAig(Abc_Ntk_t* pNtk, char* fileName) {
  if( !(Abc_NtkIsStrash(pNtk)) ) {
    printf("This command is for AIG only!\nPlease use \"strash\" first.\n");
    return 1;
  }

  int* inputPatterns;
  int num_input = Abc_NtkPiNum(pNtk);

  // read input
  FILE *inputFile;
  inputFile = fopen(fileName, "r");
  if(inputFile == NULL)
  {
    printf("No input file!\n");   
    return 1;           
  }

  inputPatterns = (int *)calloc(num_input, sizeof(int));

  char *line;
  size_t len = 0;
  ssize_t read;
  int num_line = 0;
  std::vector<std::vector<int>> result;
  result.resize(num_input, {});

  while ((read = getline(&line, &len, inputFile)) != -1) {
    if( (num_line != 0) && (num_line % 32 == 0) ) {
      Lsv_SimAig_32bit_parallel(pNtk, inputPatterns, result);
      for(int ithIn = 0; ithIn < num_input; ++ithIn){
        inputPatterns[ithIn] = 0;
      }
    }
    for(int ithIn = 0; ithIn < num_input; ++ithIn){
      if(line[ithIn] == '\n' || line[ithIn] == '\0'){
        printf("Wrong input patterns!\nPlease check the input file.\n");
        return 1;
      }
      if(line[ithIn] == '1'){
        inputPatterns[ithIn] |= 1 << num_line;
      }
      else if(line[ithIn] != '0'){
        printf("Wrong input patterns!\nPlease check the input file.\n");
        return 1;
      }
    }
    ++num_line;
  }

  Lsv_SimAig_32bit_parallel(pNtk, inputPatterns, result);

  // print result
  int ithPo;
  Abc_Obj_t* pPo;
  Abc_NtkForEachPo(pNtk, pPo, ithPo){
    printf("%s: ", Abc_ObjName(pPo));
    int num_remainBits = num_line;
    int ithSim = 0;
    while(num_remainBits >= 32){
      for(int ithBit = 0; ithBit < 32; ++ithBit){
        printf("%d", ((result[ithPo][ithSim] & (1 << ithBit)) != 0));
      }
      num_remainBits -= 32;
      ++ithSim;
    }
    for(int ithBit = 0; ithBit < num_remainBits; ++ithBit){
      printf("%d", ((result[ithPo][ithSim] & (1 << ithBit)) != 0));
    }
    printf("\n");
  }
  

  return 0;

}

int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c;
  int nArgc;
  char **nArgv;
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
    switch (c) {
      case 'h':
        goto usage;
      default:
        goto usage;
    }
  }

  nArgc = argc - globalUtilOptind;
  nArgv = argv + globalUtilOptind;

  // check for input format
  if( nArgc != 1 ) {
    printf("Wrong input format!\n");
    goto usage;
  }

  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }

  if( Lsv_SimAig(pNtk, nArgv[0]) ) {
    return 1;
  }

  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sim_aig [-h] <input pattern file>\n");
  Abc_Print(-2, "\t      : do 32-bit parallel simulations for a given AIG and some input patterns\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

// ----------------------------------------------------------

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
