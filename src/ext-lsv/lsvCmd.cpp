#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/cnf/cnf.h"
#include <vector>
#include <set>
#include <algorithm>
using namespace std; 

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintCut  (Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSdc  (Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandOdc  (Abc_Frame_t* pAbc, int argc, char** argv);
extern "C"{
  Aig_Man_t* Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
}

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut",    Lsv_CommandPrintCut,   0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sdc",         Lsv_CommandSdc,   0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_odc",         Lsv_CommandOdc,   0);
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

///////////////////////////////////////////////////////////////////////////////////////

void Lsv_NtkPrintCut(Abc_Ntk_t* pNtk, int k = 0) {
  Abc_Obj_t* pObj;
  int i, id;
  vector<set<set<int>>> v;

  // Initialization

  Abc_NtkForEachObj(pNtk, pObj, i) {
    v.push_back(set<set<int>>());
  }

  Abc_NtkForEachObj(pNtk, pObj, i) {
    // extracting information from the AIG
    id = Abc_ObjId(pObj);
    if (id == 0)
      continue;
    Abc_Obj_t* pFanin;
    int j;
    int fanInId[2]; 
    Abc_ObjForEachFanin(pObj, pFanin, j)
      fanInId[j] = Abc_ObjId(pFanin);
    
    // Building vector v
    if (j != 1)
      v[id].insert(set<int>{id});
    if (j == 2){
      for (const auto &s1 : v[fanInId[0]]) for (const auto &s2 : v[fanInId[1]]){
        set<int> merged_set = s1;
        merged_set.insert(s2.begin(), s2.end());
        if (merged_set.size() <= k){
          // Todo: maybe sort the set(?) here
          v[id].insert(merged_set);
        }
      }
    }
  }

  // Check if there is including relation
  for (i = 0; i < v.size(); i++) {
    set<set<int>>::iterator itr1 = v[i].begin(), itr2 = v[i].begin();
    while (itr1 != v[i].end()){
      while (itr2 != v[i].end()){
        if(itr1 != itr2 && includes((*itr1).begin(), (*itr1).end(), (*itr2).begin(), (*itr2).end()))
          itr1 = v[i].erase(itr1);
        else
          itr2++;
      }
      itr2 = v[i].begin();
      itr1++;
    }
  }

  // Printing out the result
  for (i = 0; i < v.size(); i++) {
    // printf("%d: {", i);
    for (const auto &s : v[i]) {                // v[i] is a set<vector<int>>
      printf("%d: ", i);
      for (const auto &e : s){                 // s in a set<int>
        printf("%d ", e);
      }
      printf("\n");
    }
    // printf("}\n");
  }
}

int Lsv_CommandPrintCut(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c, k = 0;
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "kh")) != EOF) {
    switch (c) {
      case 'k': 
        if ( globalUtilOptind >= argc )
        {
            Abc_Print( -1, "Command line switch \"-k\" should be followed by an integer.\n" );
            goto usage;
        }
        k = atoi(argv[globalUtilOptind]);
        globalUtilOptind++;
        if ( k < 0 )
            goto usage;
        break;
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
  Lsv_NtkPrintCut(pNtk, k);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_print_nodes [-h]\n");
  Abc_Print(-2, "\t        prints the nodes in the network\n");
  Abc_Print(-2, "\t-k    : specifies k-feasible cuts\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

// PA2

int Lsv_NtkSdc(Abc_Ntk_t* pNtk, int n, int print_or_not) {
  Abc_Obj_t* pObj;
  Abc_Ntk_t * pNtk2;
  sat_solver * pSat;
  Cnf_Dat_t * pCnf;
  
  Vec_Ptr_t * vIn;
  int i;
  vIn = Vec_PtrAlloc(2);

  pObj = Abc_NtkObj(pNtk, n);
  unsigned fanin_comp[2] = {pObj->fCompl0, pObj->fCompl1};
  Abc_Obj_t* pFanin;
  Abc_ObjForEachFanin(pObj, pFanin, i) { 
    // printf("fanin: %s%d, ", fanin_comp[i] ? "!" : "", Abc_ObjId(pFanin));
    Vec_PtrPush(vIn, pFanin);
  }
  // printf("\n");

  // Create new network
  pNtk2 = Abc_NtkCreateConeArray(pNtk, vIn, 1);

  // Print new network & store output ID
  int out_ID[2];
  bool second = false;
  // printf("New network: \n");
  Abc_NtkForEachObj(pNtk2, pObj, i) {
    int ID = Abc_ObjId(pObj);
    // printf("%d ", ID);
    if (Abc_ObjIsPo(pObj)) {
      out_ID[second ? 1 : 0] = ID;
      second = true;
    }
    // Abc_Obj_t* pFanin;
    // int j;
    // Abc_ObjForEachFanin(pObj, pFanin, j)
      // printf("fanin: %d, ", Abc_ObjId(pFanin));
    // printf("\n");
  }
  // printf("PO: %d and %d\n", out_ID[0], out_ID[1]);

  // Create CNF
  pSat = sat_solver_new();
  pCnf = Cnf_Derive(Abc_NtkToDar(pNtk2, 0, 0), 2);
  // printf("%d vars and %d clauses in CNF\n", pCnf->nVars, pCnf->nClauses);
  // printf("\n");

  // Print Network node <-> CNF var (pCnf->pVarNums[pObj->ID])
  int out_CNF_var[2] = {-1, -1};
  // printf("Node ID <-> CNF var\n");
  // printf("-----------------------------\n");
  Abc_NtkForEachObj(pNtk2, pObj, i) {
    // printf("%d ", Abc_ObjId(pObj));
    // printf("<-> %d\n", pCnf->pVarNums[Abc_ObjId(pObj)]);
    if (Abc_ObjId(pObj) == out_ID[0])
      out_CNF_var[0] = pCnf->pVarNums[Abc_ObjId(pObj)];
    else if (Abc_ObjId(pObj) == out_ID[1])
      out_CNF_var[1] = pCnf->pVarNums[Abc_ObjId(pObj)];
  }
  // printf("-----------------------------\n");
  // printf("PO CNF var: %d and %d\n", out_CNF_var[0], out_CNF_var[1]);
  // printf("\n");

  // Print CNF
  // int *pBeg, *pEnd;
  // printf("CNF: \n");
  // Cnf_CnfForClause(pCnf, pBeg, pEnd, i){
    // int* j = pBeg;
    // printf("(");
    // while (j != pEnd){
      // printf("%s%d + ", Abc_LitIsCompl(*j) ? "!" : "", Abc_Lit2Var(*j));
      // j++;
    // }
    // printf("\b\b\b) * \n");
  // }
  // printf("\n");

  // Solve SAT
  int out_SAT_rlt [2];
  int rlt;
  bool pssbl [4] = {false};
  Cnf_DataWriteIntoSolverInt(pSat, pCnf, 1, 0);
  rlt = sat_solver_solve(pSat, NULL, NULL, (ABC_INT64_T)0, (ABC_INT64_T)0, (ABC_INT64_T)0, (ABC_INT64_T)0);
  while (rlt == 1){
    // printf("SAT\n");
    out_SAT_rlt[0] = sat_solver_var_value(pSat, out_CNF_var[0]);
    out_SAT_rlt[1] = sat_solver_var_value(pSat, out_CNF_var[1]);
    // printf("PO value: %d and %d\n", out_SAT_rlt[0], out_SAT_rlt[1]);
    pssbl[out_SAT_rlt[0] * 2 + out_SAT_rlt[1]] = true;

    // Create new clause
    int sat_clause[2] = {Abc_Var2Lit(out_CNF_var[0], out_SAT_rlt[0]), Abc_Var2Lit(out_CNF_var[1], out_SAT_rlt[1])};
    // printf("New clause: (");
    // printf("%s%d + ", Abc_LitIsCompl(sat_clause[0]) ? "!" : "", Abc_Lit2Var(sat_clause[0]));
    // printf("%s%d)\n", Abc_LitIsCompl(sat_clause[1]) ? "!" : "", Abc_Lit2Var(sat_clause[1]));
    // printf("\n");

    // Add cluase to SAT sover
    rlt = sat_solver_addclause(pSat, sat_clause, sat_clause + 2);
    if (rlt)
      rlt = sat_solver_solve(pSat, NULL, NULL, (ABC_INT64_T)0, (ABC_INT64_T)0, (ABC_INT64_T)0, (ABC_INT64_T)0);
  }
  // else {
    // UNSAT: // printf("UNSAT\n");
  bool tf = true;
  bool reorder_rslt[4] = {false};
  for (i = 0; i < 4; i++)
    if (!pssbl[i]) {
      // if (print_or_not)
        reorder_rslt[(fanin_comp[0] ? !(i / 2) : i / 2) * 2 + (fanin_comp[1] ? !(i % 2) : i % 2)] = !pssbl[i];
        // printf("%d%d\n", fanin_comp[0] ? !(i / 2) : i / 2, fanin_comp[1] ? !(i % 2) : i % 2);
      // tf = false;
    }
  for (i = 0; i < 4; i++)
    if (reorder_rslt[i]){
      if (!tf)
        if (print_or_not) printf(" ");
      if (print_or_not) printf("%d%d", i / 2, i % 2);
      tf = false;
    }
  if (print_or_not){
    if (tf)
      printf("no sdc\n");
    else
      printf("\n");
  }
  return (reorder_rslt[0] * 8 + reorder_rslt[1] * 4 + reorder_rslt[2] * 2 + reorder_rslt[3]);
  // }
}

int Lsv_CommandSdc(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int n = 0;
  n = atoi(argv[1]);
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  Lsv_NtkSdc(pNtk, n, 1);
  return 0;
}

void print_Ntk_WL(Abc_Ntk_t* pNtk) {
  int i;
  Abc_Obj_t* pObj;
  Abc_NtkForEachObj(pNtk, pObj, i) {
    int ID = Abc_ObjId(pObj);
    printf("%d: ", ID);
    Abc_Obj_t* pFanin;
    int j;
    Abc_ObjForEachFanin(pObj, pFanin, j) {
      printf("fanin: %s%d, ", (j == 0 ? pObj->fCompl0 : pObj->fCompl1) ? "!" : "", Abc_ObjId(pFanin));
    }
    printf("\n");
  }
}

void print_CNF_WL(Abc_Ntk_t* pNtk, Cnf_Dat_t * pCnf){
  int i;
  Abc_Obj_t* pObj;
  printf("%d vars and %d clauses in CNF\n", pCnf->nVars, pCnf->nClauses);
  printf("Node ID <-> CNF var\n");
  printf("-----------------------------\n");
  Abc_NtkForEachObj(pNtk, pObj, i) {
    printf("%d ", Abc_ObjId(pObj));
    printf("<-> %d\n", pCnf->pVarNums[Abc_ObjId(pObj)]);
  }
  printf("-----------------------------\n");
  int *pBeg, *pEnd;
  printf("CNF: \n");
  Cnf_CnfForClause(pCnf, pBeg, pEnd, i){
    int* j = pBeg;
    printf("(");
    while (j != pEnd){
      printf("%s%d + ", Abc_LitIsCompl(*j) ? "!" : "", Abc_Lit2Var(*j));
      j++;
    }
    printf("\b\b\b) * \n");
  }
  printf("\n");
}

void print_proof_WL(sat_solver *pSat){
  for (int i = 0; i < pSat->size; i++)
    printf("%d, ", sat_solver_var_value(pSat, i));
  printf("\n");
}

void Lsv_NtkOdc(Abc_Ntk_t* pNtk, int n = 0) {
  // printf("Yes, This is ODC with n = %d. \n", n);
  int i, out_ID = -1, out_fanin_ID = -1, out_fanin_compl = 0;
  int in_ID[2];
  // int count = 0;
  bool out_fanin_no_var = false;
  Abc_Obj_t* pObj;
  Abc_Ntk_t *pNtk1, *pNtk2, *pNtkMiter, *pNtkCone;
  Vec_Ptr_t * vOut;
  sat_solver * pSat;
  Cnf_Dat_t * pCnf;

  pObj = Abc_NtkObj(pNtk, n);
  unsigned fanin_comp[2] = {pObj->fCompl0, pObj->fCompl1};
  vOut = Vec_PtrAlloc(3);

  pNtk1 = Abc_NtkDup(pNtk);
  // printf("pNtk1: \n");
  Abc_NtkForEachObj(pNtk1, pObj, i) {
    if (Abc_ObjId(pObj) == n) {
      Abc_Obj_t* pFanin;
      int j;
      Abc_ObjForEachFanin(pObj, pFanin, j) {
        // printf("fanin: %d, ", Abc_ObjId(pFanin));
        in_ID[j] = Abc_ObjId(pFanin);
        Vec_PtrPush(vOut, pFanin);
        // if (pFanin->Type != ABC_OBJ_PI){
          // pFanin->Type = ABC_OBJ_PI;
          // Vec_PtrPush(pNtk1->vPis, pFanin);
          // Vec_PtrPush(pNtk1->vCis, pFanin);
          // Abc_ObjRemoveFanins(pFanin);
        // }
      }
    }
  }
  // printf("Original Ntk: \n");
  // print_Ntk_WL(pNtk1);
  // Aig_ManDumpBlif(Abc_NtkToDar(pNtk1, 0, 0), "odc_Ntk1.blif", NULL, NULL);
  // printf("\n");

  // Abc_NtkForEachPi(pNtk1, pObj, i) {
    // printf("PI: %d\n", Abc_ObjId(pObj));
    // if (pObj->vFanouts.nSize == 0) {
      // printf("0 fanout Pis: %d\n", Abc_ObjId(pObj));
      // Vec_PtrRemove(pNtk1->vPis, pObj);
      // Vec_PtrRemove(pNtk1->vCis, pObj);
      // i--;
      // count++;
    // }
  // }

  // pNtk1 = Abc_NtkStrash(pNtk1, 1, 1, 0);
  // print_Ntk_WL(pNtk1);
  // printf("\n");

  // pNtk1 = Abc_NtkDup(pNtk1);
  // printf("pNtk1: \n");
  // print_Ntk_WL(pNtk1);

  pNtk2 = Abc_NtkDup(pNtk1);
  // n -= count;
  Abc_NtkForEachObj(pNtk2, pObj, i) {
    Abc_Obj_t* pFanin;
    int j;
    Abc_ObjForEachFanin(pObj, pFanin, j)
      if (Abc_ObjId(pFanin) == n)
        Abc_ObjXorFaninC(pObj, j);
  }
  Aig_ManDumpBlif(Abc_NtkToDar(pNtk2, 0, 0), "odc_Ntk2.blif", NULL, NULL);
  // printf("pNtk2: \n");
  // print_Ntk_WL(pNtk2);
  // printf("\n");

  pNtkMiter = Abc_NtkMiter(pNtk1, pNtk2, 1, 0, 0, 0);
  Abc_NtkAppend(pNtk1, pNtkMiter, 1);
  Abc_NtkForEachObj(pNtk1, pObj, i) {
    int ID = Abc_ObjId(pObj);
    if (Abc_ObjIsPo(pObj)) {  // maybe change to: If not the previous outputs
      out_ID = ID;
      // printf("Update out_ID: %d\n", out_ID);
      out_fanin_ID = Abc_ObjFaninId0(pObj);
      out_fanin_compl = pObj->fCompl0;
    }
  }
  // printf("out_ID = %d\n", out_ID);
  // printf("\n");
  // Aig_ManDumpBlif(Abc_NtkToDar(pNtkMiter, 0, 0), "odc_miter.blif", NULL, NULL);
  // Aig_ManDumpBlif(Abc_NtkToDar(pNtk1, 0, 0), "odc_Ntk1_new.blif", NULL, NULL);

  // Make Cone
  Vec_PtrPush(vOut, Abc_NtkObj(pNtk1, out_ID));
  pNtkCone = Abc_NtkCreateConeArray(pNtk1, vOut, 1);
  // printf("New network: \n");
  // print_Ntk_WL(pNtkCone);
  // Aig_ManDumpBlif(Abc_NtkToDar(pNtkCone, 0, 0), "odc_cone.blif", NULL, NULL);

  // CNF and SAT
  int in_CNF_var[2] = {-1, -1};
  pSat = sat_solver_new();
  pCnf = Cnf_Derive(Abc_NtkToDar(pNtkCone, 0, 0), 3);
  // print_CNF_WL(pNtkCone, pCnf);

  Abc_NtkForEachPo(pNtkCone, pObj, i){
    if (i == 2)
      out_ID = Abc_ObjId(pObj);
    else
      in_ID[i] = Abc_ObjId(pObj);
  }
  // printf("%d, %d, %d\n", in_ID[0], in_ID[1], out_ID);
  int out_CNF_var = pCnf->pVarNums[out_ID];
  if (out_CNF_var < 0){ // doesn't exist
    out_CNF_var = pCnf->pVarNums[out_fanin_ID];
    out_fanin_no_var = true;
  }
  in_CNF_var[0] = pCnf->pVarNums[in_ID[0]];
  in_CNF_var[1] = pCnf->pVarNums[in_ID[1]];
  
  // printf("PO CNF var: %d, out_fanin_no_var = %d, out_fanin_compl = %d\n", out_CNF_var, out_fanin_no_var, out_fanin_compl);
  // printf("in CNF var: %d and %d\n", in_CNF_var[0], in_CNF_var[1]);

  int in_SAT_rlt [2];
  int rlt;
  bool pssbl [4] = {false};
  Cnf_DataWriteIntoSolverInt(pSat, pCnf, 1, 0);
  
  // Add (miter output = 1) clause
  int sat_clause_miter[1];
  if (!out_fanin_no_var){
    sat_clause_miter[0] = {Abc_Var2Lit(out_CNF_var, 0)};
  }
  else{
    sat_clause_miter[0] = {Abc_Var2Lit(out_CNF_var, out_fanin_compl)};
  }
  rlt = sat_solver_addclause(pSat, sat_clause_miter, sat_clause_miter + 1);
  // printf("rlt = %d\n", rlt);
  if (rlt)
    rlt = sat_solver_solve(pSat, NULL, NULL, (ABC_INT64_T)0, (ABC_INT64_T)0, (ABC_INT64_T)0, (ABC_INT64_T)0);
  // printf("rlt = %d\n", rlt);

  while(rlt == 1) {
    in_SAT_rlt[0] = sat_solver_var_value(pSat, in_CNF_var[0]);
    in_SAT_rlt[1] = sat_solver_var_value(pSat, in_CNF_var[1]);
    // printf("PI value: %d and %d\n", in_SAT_rlt[0], in_SAT_rlt[1]);
    // print_proof_WL(pSat);
    pssbl[in_SAT_rlt[0] * 2 + in_SAT_rlt[1]] = true;

    // Create new clause
    int sat_clause[2] = {Abc_Var2Lit(in_CNF_var[0], in_SAT_rlt[0]), Abc_Var2Lit(in_CNF_var[1], in_SAT_rlt[1])};
    // printf("New clause: (");
    // printf("%s%d + ", Abc_LitIsCompl(sat_clause[0]) ? "!" : "", Abc_Lit2Var(sat_clause[0]));
    // printf("%s%d)\n", Abc_LitIsCompl(sat_clause[1]) ? "!" : "", Abc_Lit2Var(sat_clause[1]));
    // printf("\n");

    // Add cluase to SAT sover
    rlt = sat_solver_addclause(pSat, sat_clause, sat_clause + 2);
    // printf("rlt = %d\n", rlt);
    if (rlt)
      rlt = sat_solver_solve(pSat, NULL, NULL, (ABC_INT64_T)0, (ABC_INT64_T)0, (ABC_INT64_T)0, (ABC_INT64_T)0);
    // printf("rlt = %d\n", rlt);
  }
  
  bool tf = true;
  bool reorder_rslt[4] = {false};
  for (i = 0; i < 4; i++)
    if (!pssbl[i]) // {
      reorder_rslt[(fanin_comp[0] ? !(i / 2) : i / 2) * 2 + (fanin_comp[1] ? !(i % 2) : i % 2)] = !pssbl[i];
      // printf("%d%d\n", fanin_comp[0] ? !(i / 2) : i / 2, fanin_comp[1] ? !(i % 2) : i % 2);
      // tf = false;
    // }
  int sdc_int = Lsv_NtkSdc(pNtk, n, 0);
  bool sdc[4] = {sdc_int / 8, (sdc_int % 8) / 4, (sdc_int % 4) / 2, sdc_int % 2};
  // printf("sdc: %d %d %d %d\n", sdc[0], sdc[1], sdc[2], sdc[3]);
  for (i = 0; i < 4; i++)
    if (reorder_rslt[i] && !sdc[i]){
      if (!tf)
        printf(" ");
      printf("%d%d", i / 2, i % 2);
      tf = false;
    }
  if (tf)
    printf("no odc\n");
  else
    printf("\n");
}

int Lsv_CommandOdc(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int n = 0;
  n = atoi(argv[1]);
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  Lsv_NtkOdc(pNtk, n);
  return 0;
}