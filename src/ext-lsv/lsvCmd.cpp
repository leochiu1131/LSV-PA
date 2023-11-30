#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "bdd/extrab/extraBdd.h"
#include "bdd/cudd/cudd.h"
#include "sat/cnf/cnf.h"

// For file parser in 4-2
#include <fstream>
#include <iostream>
#include <vector>
#include <map>
using namespace std;

extern "C" {
Aig_Man_t* Abc_NtkToDar(Abc_Ntk_t* pNtk, int fExors, int fRegisters);
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


// Exercise 4-1 main code

int* BddPatternParser(char* input_pattern, int Ntk_input_num){
    if (!input_pattern) return 0;
    int len = strlen(input_pattern);
    if (len != Ntk_input_num){
      Abc_Print(-1, "Dismatch number of input pattern and network input.\n");
      return 0;
    }
    int* bdd_pattern = ABC_ALLOC(int, len);
    for (int i=0; i<len; i++) {
      if (input_pattern[i] == '0') bdd_pattern[i] = 0;
      else if (input_pattern[i] == '1') bdd_pattern[i] = 1;
      else return 0;
    }
    // for(int p=0;p<4; p++) printf("%i\n", bdd_pattern[p]);
    return bdd_pattern;
}

// Find Idx match the variable name
int matchPI(Abc_Ntk_t* pNtk, char* faninName){
  int inputIdx = -1;
  Abc_Obj_t* pCi;

  Abc_NtkForEachPi(pNtk, pCi, inputIdx) {
    if (strcmp(faninName, Abc_ObjName(pCi)) == 0) break;
  }
  // printf("%s: %i\n", faninName, inputIdx);
  return inputIdx;
}

void Lsv_Sim_Bdd(Abc_Ntk_t* pNtk, int* pattern){

  Abc_Obj_t* pPo;
  int ithPo = -1;

  Abc_NtkForEachPo(pNtk, pPo, ithPo) {
    // Demo code given by TA
    char* poName = Abc_ObjName(pPo);
    Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo);

    assert( Abc_NtkIsBddLogic(pRoot->pNtk) );
    DdManager * dd = (DdManager *)pRoot->pNtk->pManFunc;  
    DdNode* ddnode = (DdNode *)pRoot->pData;
    // char** piNames = (char**) Abc_NodeGetFaninNames(pRoot)->pArray;

    // Check each FI
    Abc_Obj_t* pFanin;
    int FI_index = 0;

    Abc_ObjForEachFanin(pRoot, pFanin, FI_index){
      char* faninName = Abc_ObjName(pFanin);
      int inputIdx = matchPI(pNtk, faninName);
      // printf("before: %s, %i  ", faninName, FI_index);
      // printf("after: %s, %i   value=%i\n", faninName, inputIdx, pattern[inputIdx]);
      DdNode* bddVar = Cudd_bddIthVar(dd, FI_index);
      ddnode = Cudd_Cofactor( dd, ddnode, (pattern[inputIdx]==1)? bddVar : Cudd_Not(bddVar));
    }
    Abc_Print(1, "%s: %d\n", poName, (ddnode == DD_ONE(dd)));
  }
  return;
}

int Lsv_CommandSimBdd(Abc_Frame_t* pAbc, int argc, char** argv) {
  
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int* bdd_pattern = BddPatternParser(argv[1], Abc_NtkPiNum(pNtk));

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
    Abc_Print(-1, "The network is not a BDD network.\n");
    return 1;
  }

  if (!bdd_pattern) {
    Abc_Print(-1, "Invalid input pattern!.\n");
    return 1;
  }

  Lsv_Sim_Bdd(pNtk, bdd_pattern);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sim_bdd [-h]\n");
  Abc_Print(-2, "       Nothing to say. [-h]\n");
  return 1;
}


// Exercise 4-2 main code

// the last bit store the width
vector<int*> AIGInputFileParser(const char* intputfile, int ntkInput_num){
  
  ifstream infile(intputfile);
  std::string pattern;
  int* patternPI = new int[ntkInput_num+1];
  vector<int*> Result;
  int width = 0;

  while (infile >> pattern) {
    
    if (pattern.size() != ntkInput_num) {
      Abc_Print(-1, "Dismatch number of input pattern and network input.\n");
      return {};
    }
    for (int i = 0; i < ntkInput_num; ++i) {
      if (pattern[i] != '0' && pattern[i] != '1') {
        Abc_Print(-1, "Invalid input pattern!.\n");
        return {};
      }
      patternPI[i] = (patternPI[i] << 1) | (pattern[i] == '1');
    }
    width++;
    if (width>=32){
      patternPI[ntkInput_num+1] = 32;
      Result.push_back(patternPI);
      patternPI = new int[ntkInput_num+1];
      width=0;
    }
  }
  patternPI[ntkInput_num+1] = width;
  Result.push_back(patternPI);
  // printf("Width: %i\n", width);
  return Result;
}

void Lsv_print_result(Abc_Ntk_t* pNtk, map<char*, string> answer){
  
  for (const auto& n : answer) {
    Abc_Print(1, "%s: %s\n", n.first, n.second.c_str());
  }
}


void Lsv_Sim_Aig(Abc_Ntk_t* pNtk, vector<int*> pattern_set, int ntkInput_num){
  
  // int total_width = 0;
  map<char*, string> answer;

  for(auto& patterns: pattern_set){
    int width = patterns[ntkInput_num+1];
    // total_width += width;
    // printf("width: %i", width);
    if (!width) return;

    Abc_Obj_t* pObj;
    
    // Set input value
    int ithPi;
    Abc_NtkForEachPi(pNtk, pObj, ithPi) {
      pObj->iTemp = patterns[ithPi];
    }

    // Start the simulation with topological order
    int ithOb;
    Abc_NtkForEachObj(pNtk, pObj, ithOb) {
      
      if (Abc_ObjIsPi(pObj) || pObj->Type == Abc_ObjType_t::ABC_OBJ_CONST1 || !Abc_ObjIsNode(pObj)) continue;
      
      // Verify values by child nodes and completement
      Abc_Obj_t* f0 = Abc_ObjFanin0(pObj);
      bool c0 = Abc_ObjFaninC0(pObj);
      int v0 = c0 ? ~(f0->iTemp) : (f0->iTemp);

      Abc_Obj_t* f1 = Abc_ObjFanin1(pObj);
      bool c1 = Abc_ObjFaninC1(pObj);
      int v1 = c1 ? ~(f1->iTemp) : (f1->iTemp);

      // And
      pObj->iTemp = v0 & v1;
    }
    // Store the answer
    int ithPo;
    Abc_NtkForEachPo(pNtk, pObj, ithPo) {
      Abc_Obj_t* f0 = Abc_ObjFanin0(pObj);
      int v0 = Abc_ObjFaninC0(pObj) ? ~(f0->iTemp) : (f0->iTemp);
      pObj->iTemp = v0;
      
      string output;
      for (int j=0; j<width; ++j) {
        // Abc_Print(1, "%i", (((v0 >> j) & 1) ? "1" : "0"));
        output = (((v0 >> j) & 1) ? "1" : "0") + output;
      }

      // Abc_Print(1, "%s: %s\n", Abc_ObjName(pObj), output.c_str());
      if(answer.find(Abc_ObjName(pObj))== answer.end()) answer[Abc_ObjName(pObj)] = output;
      else answer[Abc_ObjName(pObj)] += output;
    }
  }
  
  Lsv_print_result(pNtk, answer);

  return;
}

int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv) {
  
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  char* input_file = argv[1];
  int ntkInput_num = Abc_NtkPiNum(pNtk);
  vector<int*> input_pattern = AIGInputFileParser(input_file, ntkInput_num);

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
    Abc_Print(-1, "The network is not a AIG network.\n");
    return 1;
  }

  if (!input_file) {
    Abc_Print(-1, "Input file missing.\n");
    return 1;
  }

  // Error while parsing
  if (!input_pattern.size()) return 1;

  Lsv_Sim_Aig(pNtk, input_pattern, ntkInput_num);

  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sym_aig [-h]\n");
  Abc_Print(-2, "       Nothing to say. [-h]\n");
  return 1;
}

// PA2

// 2-1
int Lsv_CommandSymBdd(Abc_Frame_t* pAbc, int argc, char** argv) {
  
  // k = Pin of output, i, j = Pins of input variable
  int k, i, j;
  int i_bdd = -1, j_bdd = -1;
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  Abc_Obj_t* kth_Po;
  Abc_Obj_t* pRoot;
  DdManager* dd;
  DdNode* ddnode, *cube_i, *cube_j, *cof_dd01, *cof_dd10, *XOR;
  // vector<int> *Id2Order, *Order2Id;

  Abc_Obj_t* pObj;
  int i_Pi;

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
    Abc_Print(-1, "The network is not a BDD network.\n");
    return 1;
  }

  if (argc != 4){
    Abc_Print(-1, "Wrong input patterns.\n");
    return 1;
  }
  else{
    k = stoi(argv[1]);
    i = stoi(argv[2]);
    j = stoi(argv[3]);
  }

  // Ensure i < j and check if the patterns is valid.
  if (i > j) swap(i, j);
  else if (i == j){
    Abc_Print(-1, "The two input variable should be different.\n");
    return 1;
  }

  if (k >= Abc_NtkPoNum(pNtk)){
    Abc_Print(-1, "Pin of output out of range %i.\n", Abc_NtkPoNum(pNtk));
    return 1;
  }

  if (j >= Abc_NtkPiNum(pNtk)){
    Abc_Print(-1, "Pin of input out of range %i.\n", Abc_NtkPiNum(pNtk));
    return 1;
  }

  // Main process

  kth_Po = Abc_NtkPo(pNtk, k);
  pRoot = Abc_ObjFanin0(kth_Po);
  dd = (DdManager*)pRoot->pNtk->pManFunc;
  ddnode = (DdNode*)pRoot->pData;

  Abc_ObjForEachFanin( pRoot, pObj, i_Pi){
    if((pObj->Id-1) == i) i_bdd = i_Pi;
    else if((pObj->Id-1) == j) j_bdd = i_Pi;
  }

  if((i_bdd==-1) && (j_bdd==-1)){ 
    // Both i, j are not support of k
    printf("symmetry\n");
    return 0;
  }else if((i_bdd==-1)){
    // Only j is support of k
    XOR = Cudd_bddBooleanDiff(dd, ddnode, j_bdd);
    Cudd_Ref(XOR);
  }else if((j_bdd==-1)){
    // Only i is support of k
    XOR = Cudd_bddBooleanDiff(dd, ddnode, i_bdd);
    Cudd_Ref(XOR);
  }else{
    // Both of i, j are support of k
    cube_i = Cudd_bddAnd( dd, Cudd_Not( dd->vars[j_bdd] ), dd->vars[i_bdd] );
    cube_j = Cudd_bddAnd( dd, Cudd_Not( dd->vars[i_bdd] ), dd->vars[j_bdd] );

    cof_dd01 = Cudd_Cofactor(dd, ddnode, cube_i);
    cof_dd10 = Cudd_Cofactor(dd, ddnode, cube_j);

    XOR = Cudd_bddXor(dd, cof_dd01, cof_dd10);
  }

  if(XOR == DD_ZERO(dd) || XOR == Cudd_Not(DD_ONE(dd))){
    printf("symmetry\n");
    return 0;
  }else{
    // Asymmetry, find counter example
    // Reference: cudd_firstCube 
    // http://eddiehung.github.io/dox-abc/de/dc1/cudd_8h.html#a139d8dd64955ecf3e2fd4c5d6ceeebb4
    int counter_example[Abc_NtkPiNum(pNtk)];
    for(int _i=0; _i<Abc_NtkPiNum(pNtk); _i++) counter_example[_i] = 0;
    int** cube = new(int*);
    CUDD_VALUE_TYPE* value = new(CUDD_VALUE_TYPE);

    Cudd_FirstCube(dd, XOR, cube, value);
    // For cube:
    // 0: complemented literal, 1: uncomplemented literal, 2:r don't care. 

    // for(int _i=0; _i< Abc_NtkPiNum(pNtk); _i++) printf("%i ", cube[0][_i]);
    // printf("\n");
    
    Abc_ObjForEachFanin( pRoot, pObj, i_Pi){
        if(cube[0][i_Pi] == 2){
          counter_example[pObj->Id-1] = 1;
        }else{
          counter_example[pObj->Id-1] = cube[0][i_Pi];
          // printf("cube[0][i_Pi]: %i", cube[0][i_Pi]);
        }
      }
    
    printf("asymmetry\n");
    //Pattern 0 
    for(int _i = 0; _i < Abc_NtkPiNum(pNtk); _i++){
      if(_i == i) printf("0");
      else if(_i == j) printf("1");
      else printf("%i", counter_example[_i]);
    }
    printf("\n");
    for(int _i = 0; _i < Abc_NtkPiNum(pNtk); _i++){
      if(_i == i) printf("1");
      else if(_i == j) printf("0");
      else printf("%i", counter_example[_i]);
    }
    printf("\n");
  }

  


  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sym_bdd <k> <i> <j> [-h]\n");
  Abc_Print(-2, "       Check if the circuit is symmetry on variables.\n");
  Abc_Print(-2, "       <k> : the output pin index [-h]\n");
  Abc_Print(-2, "       <i> <j> : the intput pin indexes [-h]\n");
  return 1;
}




// 2-2
int Lsv_CommandSymSat(Abc_Frame_t* pAbc, int argc, char** argv) {
  
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  Abc_Ntk_t* pNtk_cone;
  Aig_Man_t* aig_Man;
  sat_solver* solver;
  Cnf_Dat_t* Cnf,* Cnf_2;
  Aig_Obj_t* pObj,* pObj_2;
  int i_Pi = -1, i_Pi_2 = -1;
  lit lits[2];

  // k = Pin of output, i, j = Pins of input variable
  int k, i, j;
  int n;
  int Is_asym;
  int c;
  // for debug
  // int count = 0;

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
    Abc_Print(-1, "The network is not a AIG network.\n");
    return 1;
  }

  // Parse input patterns
  if (argc != 4){
    Abc_Print(-1, "Wrong input patterns.\n");
    return 1;
  }
  else{
    k = stoi(argv[1]);
    i = stoi(argv[2]);
    j = stoi(argv[3]);
  }

  // Ensure i < j and check if the patterns is valid.
  if (i > j) swap(i, j);
  else if (i == j){
    Abc_Print(-1, "The two input variable should be different.\n");
    return 1;
  }

  if (k >= Abc_NtkPoNum(pNtk)){
    Abc_Print(-1, "Pin of output out of range %i.\n", Abc_NtkPoNum(pNtk));
    return 1;
  }

  if (j >= Abc_NtkPiNum(pNtk)){
    Abc_Print(-1, "Pin of input out of range %i.\n", Abc_NtkPiNum(pNtk));
    return 1;
  }

  // Main process (follow the Hint given by TA)
  // (1)  
  pNtk_cone = Abc_NtkCreateCone(pNtk, Abc_ObjFanin0(Abc_NtkPo(pNtk, k)), Abc_ObjName(Abc_NtkPo(pNtk, k)), 1);;
  // (2)
  aig_Man = Abc_NtkToDar(pNtk_cone, 0, 1);
  // (3)
  solver = sat_solver_new();
  // (4)
  Cnf = Cnf_Derive(aig_Man, 1);
  n = Cnf->nVars;
  // (5)
  Cnf_DataWriteIntoSolverInt(solver, Cnf, 1, 0);
  // (6)  create another CNF formula C_B
  Cnf_2 = Cnf_Derive(aig_Man, 1);
  Cnf_DataLift(Cnf_2, n);
  Cnf_DataWriteIntoSolverInt(solver, Cnf_2, 1, 0);
  // printf("Before: %i\n",sat_solver_nclauses(solver));
  // (7) Add corresponding clauses to the SAT solver (V_i != V_(i+n), V_j != V_(j+n), ow V_m == V_(m+n))
  // printf("n is %i\n", n);
  Aig_ManForEachCi(aig_Man, pObj, i_Pi){
    if(i_Pi == i || i_Pi == j){
      // i xor i+n, j xor j+n
      lits[0] = toLitCond(Cnf->pVarNums[pObj->Id], 0);
      lits[1] = toLitCond(Cnf_2->pVarNums[pObj->Id], 0);
      sat_solver_addclause(solver, lits, lits+2);
      lits[0] = toLitCond(Cnf->pVarNums[pObj->Id], 1);
      lits[1] = toLitCond(Cnf_2->pVarNums[pObj->Id], 1);
      sat_solver_addclause(solver, lits, lits+2);
      Aig_ManForEachCi(aig_Man, pObj_2, i_Pi_2){
        // i <-> j+n, j <-> i+n
        if((i_Pi_2 == i || i_Pi_2 == j) && i_Pi_2 != i_Pi){
          // printf("Pi = %i Pi_2 = %i.\n", i_Pi, i_Pi_2);
          lits[0] = toLitCond(Cnf->pVarNums[pObj->Id], 0);
          lits[1] = toLitCond(Cnf_2->pVarNums[pObj_2->Id], 1);
          // printf("Pi_var = %i Pi_2_var = %i.\n", Cnf->pVarNums[pObj->Id], Cnf_2->pVarNums[pObj_2->Id]);
          sat_solver_addclause(solver, lits, lits+2);
          lits[0] = toLitCond(Cnf->pVarNums[pObj->Id], 1);
          lits[1] = toLitCond(Cnf_2->pVarNums[pObj_2->Id], 0);
          sat_solver_addclause(solver, lits, lits+2);
        }
      }
    }
    else{
      // a <-> a+n
      lits[0] = toLitCond(Cnf->pVarNums[pObj->Id], 0);
      lits[1] = toLitCond(Cnf_2->pVarNums[pObj->Id], 1);
      // printf("%i and %i.\n", Cnf->pVarNums[pObj->Id] + n, Cnf_2->pVarNums[pObj->Id]);
      sat_solver_addclause(solver, lits, lits+2);
      lits[0] = toLitCond(Cnf->pVarNums[pObj->Id], 1);
      lits[1] = toLitCond(Cnf_2->pVarNums[pObj->Id], 0);
      sat_solver_addclause(solver, lits, lits+2);
    }
  }

  // (8)
  Aig_ManForEachCo(aig_Man, pObj, i_Pi) {
    // out1 xor out2, if sat then it is asymm
    lits[0] = toLitCond(Cnf->pVarNums[pObj->Id], 0);
    lits[1] = toLitCond(Cnf_2->pVarNums[pObj->Id], 0);
    sat_solver_addclause(solver, lits, lits + 2);
    lits[0] = toLitCond(Cnf->pVarNums[pObj->Id], 1);
    lits[1] = toLitCond(Cnf_2->pVarNums[pObj->Id], 1);
    sat_solver_addclause(solver, lits, lits + 2);
  }

  Is_asym = sat_solver_solve(solver, NULL, NULL, 0, 0, 0, 0);
  // printf("After: %i  \ncount is %i\n",sat_solver_nclauses(solver), count);

  // (9)
  
  if(Is_asym != l_True) printf("symmetry\n");
  else {
    printf("asymmetry\n");
    Aig_ManForEachCi(aig_Man, pObj, i) {
      // Print pattern 0
      printf("%i", sat_solver_var_value(solver, Cnf->pVarNums[pObj->Id]));
    }
    printf("\n");
    Aig_ManForEachCi(aig_Man, pObj, i) {
      // Print pattern 1
      printf("%i", sat_solver_var_value(solver, Cnf_2->pVarNums[pObj->Id]));
    }
    printf("\n");
  }

  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sym_sat <k> <i> <j> [-h]\n");
  Abc_Print(-2, "       <k> : the output pin index [-h]\n");
  Abc_Print(-2, "       <i> <j> : the intput pin indexes [-h]\n");
  return 1;
}



