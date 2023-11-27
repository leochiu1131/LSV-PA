#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"

#include <string>
#include <bdd/cudd/cudd.h>
#include <fstream>
#include <vector>
#include <map>
#include <set>

/* for SAT */
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

/* disjoint set for lsv_sym_all command */
static std::vector<int> disjointSetFather;
static std::vector<int> disjointSetGroupSize;

static void disjointSetInit(int n) {
  disjointSetFather.resize(n);
  disjointSetGroupSize.resize(n);
  for (int i = 0; i < n; ++i) {
    disjointSetFather[i] = i;
    disjointSetGroupSize[i] = 1;
  }
}
static int disjointSetFind(int x) {
  assert(x < disjointSetFather.size());
  return (disjointSetFather[x] == x) ? x : (disjointSetFather[x] = (disjointSetFather[x]));
}

static void disjointSetMerge(int a, int b) {
  assert(a < disjointSetFather.size());
  assert(b < disjointSetFather.size());
  int fa = disjointSetFind(a);
  int fb = disjointSetFind(b);
  if (fa == fb)
    return;
  if (disjointSetGroupSize[fa] > disjointSetFather[fb]) 
    std::swap(fa, fb);
  // size(fa) < size(fb)
  disjointSetFather[fa] = fb;
  disjointSetGroupSize[fb] += disjointSetGroupSize[fa];
  disjointSetGroupSize[fa] = 0;
}

static bool disjointSetEqual(int a, int b) {
  assert(a < disjointSetFather.size());
  assert(b < disjointSetFather.size());
  return disjointSetFind(a) == disjointSetFind(b);
}



 
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

/* lsv_sim_bdd Command */
int Lsv_SimBdd_getPiValue(Abc_Ntk_t* pNtk, const std::string& pattern, const char* piName) {
  if (Abc_NtkPiNum(pNtk) != pattern.size()) {
    printf("Error: pattern width != Pi num\n");
    return -1;
  }  
  
  Abc_Obj_t* pPi;
  int i;
  Abc_NtkForEachPi(pNtk, pPi, i) {
    if (strcmp(Abc_ObjName(pPi), piName) == 0) {
      if (pattern[i] == '0')
        return 0;
      else if (pattern[i] == '1')
        return 1;
      else {
        printf("Error: unexpected input_pattern value \'%c\' in input_pattern \'%s\'\n", pattern[i], pattern.c_str());
        return -1;
      }
    }
  }
  printf("Error: no match Pi name with Fanin \'%s\'\n", piName);
  return -1;
}

void Lsv_SimBdd(Abc_Ntk_t* pNtk, const std::string& pattern) {
  if (!Abc_NtkHasBdd(pNtk)) {
    printf("Error: This command is only applicable to bdd networks.\n");
    return;
  }

  int i;
  Abc_Obj_t* pPo;
  Abc_NtkForEachPo(pNtk, pPo, i) {
    Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo); 
    assert( Abc_NtkIsBddLogic(pRoot->pNtk) );
    DdManager * dd = (DdManager *)pRoot->pNtk->pManFunc; 
    // printf("mgr: %p\n", dd); 
    // printf("pRoot: %p\n", pRoot); 
    // printf("pNtk: %p\n", pNtk);
    // printf("----\n");
    DdNode* bFunc = (DdNode *)pRoot->pData;
    char* pOName = Abc_ObjName(pPo);
    
    char** piNames = (char**) Abc_NodeGetFaninNames(pRoot)->pArray;
    int fainNum = Abc_ObjFaninNum(pRoot);
    for (int j = 0; j < fainNum; ++j) {
      // printf("inName: %s\n", piNames[j]);
      DdNode* bVar = Cudd_bddIthVar(dd, j);
      // printf("inId: %ld\n", bVar->Id);
      // printf("bVar: %p\n", bVar);
      // printf("---\n");
      int piValue = Lsv_SimBdd_getPiValue(pNtk, pattern, piNames[j]);
      if (piValue == -1)
        return;
      bFunc = Cudd_Cofactor( dd, bFunc, Cudd_NotCond(bVar, (piValue == 0))); //Cudd_Ref( bFunc );
      // printf("isconst: %d\n", Cudd_IsConstant(bFunc));
    }
    if (!Cudd_IsConstant(bFunc)) {
      printf("Error: simulation result is not deterministic!\n");
      return;
    }
    printf("%s: %d\n", pOName, (bFunc == DD_ONE(dd)));
  }

/*
  Abc_Ntk_t* pTemp = Abc_NtkIsStrash(pNtk) ? pNtk : Abc_NtkStrash(pNtk, 0, 0, 0);
  DdManager * dd = (DdManager *)Abc_NtkBuildGlobalBdds( pTemp, 10000000, 1, 0, 0, 0 );
  if (pattern.size() != Abc_NtkCiNum(pTemp)) {
    printf("Error: pattern(%d) doesn't match size of Ci(%d)\n", pattern.size(), Abc_NtkCiNum(pTemp));
    return;
  }
  int i, j;
  Abc_Obj_t* pCo;
  Abc_Obj_t* pCi;
  Abc_NtkForEachCo(pTemp, pCo, i) {
    // printf("i: %d\n", i);
    DdNode* bFunc = ((DdNode*) Abc_ObjGlobalBdd(pCo));
    char* pOName = Abc_ObjName(pCo);
    Abc_NtkForEachCi(pTemp, pCi, j) {
      // printf("j: %d\n", j);
      DdNode* bVar = Cudd_bddIthVar(dd, j);
      if (pattern[j] == '0') {
        bVar = Cudd_Not(bVar);
      }
      else if (pattern[j] != '1') {
        printf("Error: unexpected input_pattern value \'%c\' in input_pattern \'%s\'\n", pattern[j], pattern);
        return;
      }
      bFunc = Cudd_Cofactor( dd, bFunc, bVar); //Cudd_Ref( bFunc );
    }
    if (!Cudd_IsConstant(bFunc)) {
      printf("Error: simulation result is not deterministic!\n");
      return;
    }
    printf("%s: %d\n", pOName, (bFunc == DD_ONE(dd)));
  }

  
  Abc_NtkFreeGlobalBdds( pTemp, 0 );
  if (pTemp != pNtk) {
    Abc_NtkDelete( pTemp );
  }

  // Remember to Dereference Cudd_RecursiveDeref( dd, bFunc );
  return;
*/
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
  if (argc != 2)
    goto usage;
  Lsv_SimBdd(pNtk, argv[1]);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sim_bdd [-h]\n");
  Abc_Print(-2, "usage: lsv_sim_bdd <input_pattern>\n");
  Abc_Print(-2, "\t        do simulation for BDD with given input pattern \n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}
/* lsv_sim_bdd end */


/* lsv_sim_aig Command */
void Lsv_SimAigInt(Abc_Ntk_t* pNtk, int* patternPi, int width) {
  if (width == 0)
    return;
  Abc_Obj_t* pObj;
  int i;
  Abc_NtkForEachPi(pNtk, pObj, i) {
    pObj->iTemp = patternPi[i];
  }
  Abc_NtkForEachObj(pNtk, pObj, i) {
    if (Abc_ObjIsPi(pObj) || pObj->Type == Abc_ObjType_t::ABC_OBJ_CONST1)
      continue;
    if (!Abc_ObjIsNode(pObj))
      continue;
    Abc_Obj_t* f0 = Abc_ObjFanin0(pObj);
    Abc_Obj_t* f1 = Abc_ObjFanin1(pObj);
    bool c0 = Abc_ObjFaninC0(pObj);
    bool c1 = Abc_ObjFaninC1(pObj);
    int value0 = c0 ? ~(f0->iTemp) : (f0->iTemp);
    int value1 = c1 ? ~(f1->iTemp) : (f1->iTemp);
    pObj->iTemp = value0 & value1;
    // printf("Obj=> id: %d, type: %d, value: %d\n", pObj->Id, pObj->Type, pObj->iTemp);
    // printf("f0 => id: %d, type: %d, value: %d\n", f0->Id, f0->Type, f0->iTemp);
    // printf("f1 => id: %d, type: %d, value: %d\n", f1->Id, f1->Type, f1->iTemp);
    // printf("------\n");
  }

  Abc_NtkForEachPo(pNtk, pObj, i) {
    Abc_Obj_t* f0 = Abc_ObjFanin0(pObj);
    bool c0 = Abc_ObjFaninC0(pObj);
    int value0 = c0 ? ~(f0->iTemp) : (f0->iTemp);
    pObj->iTemp = value0;
    std::string outputPattern;
    for (int j = 0; j < width; ++j) {
      outputPattern = (((value0 >> j) & 1) ? "1" : "0") + outputPattern;
    }
    printf("%s: %s\n", Abc_ObjName(pObj), outputPattern.c_str());
  }
}

void Lsv_SimAig(Abc_Ntk_t* pNtk, const std::string& input_pattern_file) {
  if (!Abc_NtkIsStrash(pNtk)) {
    printf("Error: This command is only applicable to strashed networks.\n");
    return;
  }
  std::ifstream infile(input_pattern_file);
  if (!infile) {
    printf("Error: cannot open input_pattern_file %s\n", input_pattern_file.c_str());
    return;
  }
  std::string pattern;
  int width = 0;
  int CiNum = Abc_NtkCiNum(pNtk);
  int* patternPi = new int[CiNum];
  while (1) {
    if (!(infile >> pattern)) {
      // sim
      Lsv_SimAigInt(pNtk, patternPi, width);
      break;
    }
    if (pattern.size() != CiNum) {
      printf("Error: input pattern(%s) with width(%ld) does not match input size(%d)\n", pattern.c_str(), pattern.size(), CiNum);
      return;
    }
    for (int i = 0; i < CiNum; ++i) {
      if (pattern[i] != '0' && pattern[i] != '1') {
        printf("Error: unexpected input value '%c' in input pattern '%s'\n", pattern[i], pattern.c_str());
        delete[] patternPi;
        return;
      }
      patternPi[i] = (patternPi[i] << 1) | (pattern[i] == '1');
    }
    if (++width == 32) {
      // sim
      Lsv_SimAigInt(pNtk, patternPi, width);
      width = 0;
    }
  }
  delete[] patternPi;
}

int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv) {
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
  if (argc != 2)
    goto usage;
  Lsv_SimAig(pNtk, argv[1]);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sim_aig [-h]\n");
  Abc_Print(-2, "usage: lsv_sim_aig <input_pattern_file>\n");
  Abc_Print(-2, "\t        do parallel simulation for Aig with given input pattern file\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}
/* lsv_sim_aig end */

/* lsv_sym_bdd Command */
void Lsv_SymBddCubeString2PrintCex(Abc_Ntk_t* pNtk, const std::vector<char*>& bddPiNames, const char* cubeString, int i_th, int j_th) {
  int i;
  Abc_Obj_t* pPi;
  std::string cex;
  Abc_NtkForEachPi(pNtk, pPi, i) {
    char* piName = Abc_ObjName(pPi);
    cex.push_back('0'); // dc we assume to 0
    for (int j = 0; j < bddPiNames.size(); ++j) {
      if (strcmp(bddPiNames[j], piName) == 0) {
        cex.back() = (cubeString[j] ? '1' : '0');
        break;
      }
    }
  }
  cex[i_th] = '0';
  cex[j_th] = '1';
  printf("%s\n", cex.c_str());
  cex[i_th] = '1';
  cex[j_th] = '0';
  printf("%s\n", cex.c_str());
}

void Lsv_SymBdd(Abc_Ntk_t* pNtk, int k_th, int i_th, int j_th) {
  if (!Abc_NtkHasBdd(pNtk)) {
    printf("Error: This command is only applicable to bdd networks.\n");
    return;
  }
  if (Abc_NtkPoNum(pNtk) <= k_th) {
    printf("Error: k(%d) is larger than num of Po\n", k_th);
    return;
  }
  if (Abc_NtkPiNum(pNtk) <= i_th) {
    printf("Error: i(%d) is larger than num of Pi\n", i_th);
    return;
  }
  if (Abc_NtkPiNum(pNtk) <= j_th) {
    printf("Error: j(%d) is larger than num of Pi\n", j_th);
    return;
  }
  if (i_th == j_th) {
    printf("symmetric\n");
    return;
  }
  Abc_Obj_t* pPo = Abc_NtkPo(pNtk, k_th);
  Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo); 
  assert( Abc_NtkIsBddLogic(pRoot->pNtk) );
  DdManager * dd = (DdManager *)pRoot->pNtk->pManFunc;
  DdNode* bFunc = (DdNode *)pRoot->pData;   Cudd_Ref(bFunc);
  char** piNames = (char**) Abc_NodeGetFaninNames(pRoot)->pArray;
  int fainNum = Abc_ObjFaninNum(pRoot);

  DdNode* Cof_iBar_j;
  DdNode* Cof_jBar_i;
  int ithFanin = -1;
  int jthFanin = -1;
  for (int i = 0; i < fainNum; ++i) {
    if (strcmp(piNames[i], Abc_ObjName(Abc_NtkPi(pNtk, i_th))) == 0)
      ithFanin = i;
    if (strcmp(piNames[i], Abc_ObjName(Abc_NtkPi(pNtk, j_th))) == 0)
      jthFanin = i;
  }
  if (ithFanin == -1 && jthFanin == -1) {
    printf("symmetric\n");
    return;
  }
  else if (ithFanin == -1) { // i is don't care
    Cof_iBar_j = Cudd_Cofactor(dd, bFunc, Cudd_bddIthVar(dd, jthFanin));             Cudd_Ref(Cof_iBar_j);
    Cof_jBar_i = Cudd_Cofactor(dd, bFunc, Cudd_Not(Cudd_bddIthVar(dd, jthFanin)));   Cudd_Ref(Cof_jBar_i);
  }
  else if (jthFanin == -1) { // j is don't care
    Cof_iBar_j = Cudd_Cofactor(dd, bFunc, Cudd_Not(Cudd_bddIthVar(dd, ithFanin)));   Cudd_Ref(Cof_iBar_j);
    Cof_jBar_i = Cudd_Cofactor(dd, bFunc, Cudd_bddIthVar(dd, ithFanin));             Cudd_Ref(Cof_jBar_i);
  }
  else {
    // ithVar and jthVar are both in k's fanin, consider the cofactor
    DdNode* cube_iBar_j = Cudd_bddAnd(dd, Cudd_Not(Cudd_bddIthVar(dd, ithFanin)), Cudd_bddIthVar(dd, jthFanin));   Cudd_Ref(cube_iBar_j);
    DdNode* cube_jBar_i = Cudd_bddAnd(dd, Cudd_Not(Cudd_bddIthVar(dd, jthFanin)), Cudd_bddIthVar(dd, ithFanin));   Cudd_Ref(cube_jBar_i);
    Cof_iBar_j = Cudd_Cofactor(dd, bFunc, cube_iBar_j);   Cudd_Ref(Cof_iBar_j);
    Cof_jBar_i = Cudd_Cofactor(dd, bFunc, cube_jBar_i);   Cudd_Ref(Cof_jBar_i);
    Cudd_RecursiveDeref(dd, cube_iBar_j);
    Cudd_RecursiveDeref(dd, cube_jBar_i);
  }
  if (Cof_iBar_j == Cof_jBar_i) {
    printf("symmetric\n");
  }
  else {
    printf("asymmetric\n");
    // TODO: how to get cex
    DdNode* XOR = Cudd_bddXor(dd, Cof_iBar_j, Cof_jBar_i);   cuddRef(XOR);
    char* cubeString = new char[dd->size];
    Cudd_bddPickOneCube(dd, XOR, cubeString); // string is in the order of bddVar, use piName to map 
    std::vector<char*> bddPiNames = std::vector<char*>(piNames, piNames+fainNum);
    Lsv_SymBddCubeString2PrintCex(pNtk, bddPiNames, cubeString, i_th, j_th);

    delete[] cubeString;
    Cudd_RecursiveDeref(dd, XOR);
  }

  Cudd_RecursiveDeref(dd, Cof_jBar_i);
  Cudd_RecursiveDeref(dd, Cof_iBar_j);
  Cudd_RecursiveDeref(dd, bFunc);
  return;
}

int Lsv_CommandSymBdd(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c;
  int i_th, j_th, k_th;
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
  if (argc != 4)
    goto usage;
  k_th = atoi(argv[1]);
  i_th = atoi(argv[2]);
  j_th = atoi(argv[3]);
  Lsv_SymBdd(pNtk, k_th, i_th, j_th);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sym_bdd [-h]\n");
  Abc_Print(-2, "usage: lsv_sym_bdd <k> <i> <j>\n");
  Abc_Print(-2, "\t        whether k-th output is symmetric in i-th and j-th inputs by BDD method\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}
/* lsv_sym_bdd end */

/* lsv_sym_sat Command */
void Lsv_SymSat(Abc_Ntk_t* pNtk, int k_th, int i_th, int j_th) {
  if (!Abc_NtkIsStrash(pNtk)) {
    printf("Error: This command is only applicable to strashed networks.\n");
    return;
  }
  if (Abc_NtkPoNum(pNtk) <= k_th) {
    printf("Error: k(%d) is larger than num of Po\n", k_th);
    return;
  }
  if (Abc_NtkPiNum(pNtk) <= i_th) {
    printf("Error: i(%d) is larger than num of Pi\n", i_th);
    return;
  }
  if (Abc_NtkPiNum(pNtk) <= j_th) {
    printf("Error: j(%d) is larger than num of Pi\n", j_th);
    return;
  }
  if (i_th == j_th) {
    printf("symmetric\n");
    return;
  }
  Abc_Obj_t* pPo = Abc_NtkPo(pNtk, k_th);
  Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo); 
  char pNodeName[1000];
  sprintf(pNodeName, "%dth_PO_Cone", k_th);
  Abc_Ntk_t* pConeNtk = Abc_NtkCreateCone(pNtk, pRoot, pNodeName, 1);
  Aig_Man_t* pMan = Abc_NtkToDar(pConeNtk, 0, 0); // 0 -> fExors, 0 -> fRegisters
  Cnf_Dat_t* pCnf = Cnf_Derive(pMan, Aig_ManCoNum(pMan));
  sat_solver* pSat = sat_solver_new();
  pSat = (sat_solver *)Cnf_DataWriteIntoSolverInt( pSat, pCnf, 1, 0 ); // 1 -> nFrames, 0 -> fInit 
  int liftNum = Aig_ManObjNum(pMan);
  // printf("liftNum: %d\n", liftNum);
  Cnf_DataLift(pCnf, liftNum);
  pSat = (sat_solver *)Cnf_DataWriteIntoSolverInt( pSat, pCnf, 1, 0 ); // 1 -> nFrames, 0 -> fInit 
  
  Abc_Obj_t* pObj;
  int i;
  // (p == q) = (p v ~q)^(~p v q)
  lit clause[2];
  Abc_NtkForEachPi(pConeNtk, pObj, i) {
    if (i == i_th || i == j_th)
      continue;
    int var = pCnf->pVarNums[pObj->Id];
    clause[0] = toLitCond(var          , 0); // p
    clause[1] = toLitCond(var - liftNum, 1); // ~q
    sat_solver_addclause(pSat, clause, clause+2);

    clause[0] = toLitCond(var          , 1); // ~p
    clause[1] = toLitCond(var - liftNum, 0); // q
    sat_solver_addclause(pSat, clause, clause+2);
  }
  int var_i = pCnf->pVarNums[Abc_NtkPi(pConeNtk, i_th)->Id];
  int var_j = pCnf->pVarNums[Abc_NtkPi(pConeNtk, j_th)->Id];

  // printf("var_i: %d\n", var_i);
  // printf("var_j: %d\n", var_j);

  clause[0] = toLitCond(var_i, 0); // x_i = 1
  sat_solver_addclause(pSat, clause, clause+1);
  clause[0] = toLitCond(var_j, 1); // x_j = 0
  sat_solver_addclause(pSat, clause, clause+1);
  clause[0] = toLitCond(var_i - liftNum, 1); // x_i = 0
  sat_solver_addclause(pSat, clause, clause+1);
  clause[0] = toLitCond(var_j - liftNum, 0); // x_j = 1
  sat_solver_addclause(pSat, clause, clause+1);

  // (x_k != lift{x_k}) = (x_k v lift{x_k}) ^ (~x_k v ~lift{x_k})
  // printf("PoNum: %d\n", Abc_NtkPoNum(pConeNtk));
  pRoot = Abc_ObjFanin0(Abc_NtkPo(pConeNtk, 0));
  int var_k = pCnf->pVarNums[pRoot->Id];
  // printf("var_k: %d\n", var_k);
  clause[0] = toLitCond(var_k          , 0); // x_k
  clause[1] = toLitCond(var_k - liftNum, 0); // lift{x_k}
  sat_solver_addclause(pSat, clause, clause+2);
  clause[0] = toLitCond(var_k          , 1); // ~x_k
  clause[1] = toLitCond(var_k - liftNum, 1); // ~lift{x_k}
  sat_solver_addclause(pSat, clause, clause+2);

  bool result = (l_True == sat_solver_solve(pSat, NULL, NULL, 0, 0, 0, 0));
  // printf("result: %d\n", result);
  if (!result) {
    printf("symmetric\n");
  }
  else {
    printf("asymmetric\n");
    Abc_NtkForEachPi(pConeNtk, pObj, i) {
      int var = pCnf->pVarNums[pObj->Id];
      printf("%d", sat_solver_var_value(pSat, var));
    }
    printf("\n");
    Abc_NtkForEachPi(pConeNtk, pObj, i) {
      int var = pCnf->pVarNums[pObj->Id];
      printf("%d", sat_solver_var_value(pSat, var - liftNum));
    }
    printf("\n");
  }
  sat_solver_delete(pSat);
  Cnf_DataFree(pCnf);
  Aig_ManStop(pMan);
  Abc_NtkDelete(pConeNtk);
}
int Lsv_CommandSymSat(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c;
  int i_th, j_th, k_th;
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
  if (argc != 4)
    goto usage;
  k_th = atoi(argv[1]);
  i_th = atoi(argv[2]);
  j_th = atoi(argv[3]);
  Lsv_SymSat(pNtk, k_th, i_th, j_th);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sym_sat [-h]\n");
  Abc_Print(-2, "usage: lsv_sym_sat <k> <i> <j>\n");
  Abc_Print(-2, "\t        whether k-th output is symmetric in i-th and j-th inputs by SAT method\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}
/* lsv_sym_sat end */


/* lsv_sym_all */
void Lsv_SymAll(Abc_Ntk_t* pNtk, int k_th) {
  if (!Abc_NtkIsStrash(pNtk)) {
    printf("Error: This command is only applicable to strashed networks.\n");
    return;
  }
  if (Abc_NtkPoNum(pNtk) <= k_th) {
    printf("Error: k(%d) is larger than num of Po\n", k_th);
    return;
  }
  Abc_Obj_t* pPo = Abc_NtkPo(pNtk, k_th);
  Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo); 
  char pNodeName[1000];
  sprintf(pNodeName, "%dth_PO_Cone", k_th);
  Abc_Ntk_t* pConeNtk = Abc_NtkCreateCone(pNtk, pRoot, pNodeName, 1);
  Aig_Man_t* pMan = Abc_NtkToDar(pConeNtk, 0, 0); // 0 -> fExors, 0 -> fRegisters
  Cnf_Dat_t* pCnf = Cnf_Derive(pMan, Aig_ManCoNum(pMan));
  sat_solver* pSat = sat_solver_new();
  pSat = (sat_solver *)Cnf_DataWriteIntoSolverInt( pSat, pCnf, 1, 0 ); // 1 -> nFrames, 0 -> fInit 
  int liftNum = Aig_ManObjNum(pMan);
  // printf("liftNum: %d\n", liftNum);
  Cnf_DataLift(pCnf, liftNum);
  pSat = (sat_solver *)Cnf_DataWriteIntoSolverInt( pSat, pCnf, 1, 0 ); // 1 -> nFrames, 0 -> fInit 
  
  std::vector<int> enableSymVar(Abc_NtkPiNum(pConeNtk));
  Abc_Obj_t* pObj;
  Abc_Obj_t* pObj2;
  int i, j;
  lit* clause = new lit[Abc_NtkPiNum(pConeNtk)+1];
  Abc_NtkForEachPi(pConeNtk, pObj, i) {
    enableSymVar[i] = sat_solver_addvar(pSat);
  }
  // x_i == x_i
  Abc_NtkForEachPi(pConeNtk, pObj, i) {
    int var = pCnf->pVarNums[pObj->Id];
    clause[0] = toLitCond(var            , 0); // p
    clause[1] = toLitCond(var - liftNum  , 1); // ~q
    clause[2] = toLitCond(enableSymVar[i], 0); // enable
    sat_solver_addclause(pSat, clause, clause+3);

    clause[0] = toLitCond(var          , 1); // ~p
    clause[1] = toLitCond(var - liftNum, 0); // q
    clause[2] = toLitCond(enableSymVar[i], 0); // enable
    sat_solver_addclause(pSat, clause, clause+3);
  }

  // Pi symmetric constraints and according enable variables
  // i < j -> (x_i = 1, x_j = 0, lift{x_i} = 0, lift{x_j} = 1)
  // (en_k ^ ~en_1     ^ ~en_2     ^ ... ^ ~en_{k-1}) -> ((x_k = 1) ^ (lift{x_k} = 0))
  // CNF => (~en_k v en_1 v en_2 v ... v en_{k-1} v x_k) ^ (~en_k v en_1 v en_2 v ... v en_{k-1} v ~lift{x_k})
  // (en_k ^ ~en_{k+1} ^ ~en_{k+2} ^ ... ^ ~en_n)     -> ((x_k = 0) ^ (lift{x_k} = 1))
  // CNF => (~en_k v en_{k+1} v en_{k+2} v ... v en_n v ~x_k) ^ (~en_k v en_{k+1} v en_{k+2} v ... v en_n v lift{x_k})
  int k, l;
  Abc_NtkForEachPi(pConeNtk, pObj, k) {
    int lift_kVar = pCnf->pVarNums[pObj->Id];
    clause[0] = toLitCond(enableSymVar[k], 1); // ～en_k
    Abc_NtkForEachPi(pConeNtk, pObj2, l) {
      if (l == k) {
        clause[l+1] = toLitCond(lift_kVar - liftNum, 0); // x_k
        sat_solver_addclause(pSat, clause, clause + l+2);
        clause[l+1] = toLitCond(lift_kVar          , 1); // ~lift{x_k}
        sat_solver_addclause(pSat, clause, clause + l+2);
      }
      else if (l < k) {
        clause[l+1] = toLitCond(enableSymVar[l], 0); // en_1, en_2, ..., en_{k-1}
      }
      else { // l > k
        clause[l-k] = toLitCond(enableSymVar[l], 0); // en_{k+1}, en_{k+2}, ..., en_n
      }
    }
    assert(l == Abc_NtkPiNum(pConeNtk));
    clause[l-k] = toLitCond(lift_kVar - liftNum, 1); // ~x_k
    sat_solver_addclause(pSat, clause, clause + l-k + 1);
    clause[l-k] = toLitCond(lift_kVar          , 0); // lift{x_k}
    sat_solver_addclause(pSat, clause, clause + l-k + 1);
  }

  // Po constraint
  // (x_k != lift{x_k}) = (x_k v lift{x_k}) ^ (~x_k v ~lift{x_k})
  pRoot = Abc_ObjFanin0(Abc_NtkPo(pConeNtk, 0));
  int var_k = pCnf->pVarNums[pRoot->Id];
  // printf("var_k: %d\n", var_k);
  clause[0] = toLitCond(var_k          , 0); // x_k
  clause[1] = toLitCond(var_k - liftNum, 0); // lift{x_k}
  sat_solver_addclause(pSat, clause, clause+2);
  clause[0] = toLitCond(var_k          , 1); // ~x_k
  clause[1] = toLitCond(var_k - liftNum, 1); // ~lift{x_k}
  sat_solver_addclause(pSat, clause, clause+2);

  // for each (i,j) pairs, examine the symmetric relation
  disjointSetInit(Abc_NtkPiNum(pConeNtk));
  Abc_NtkForEachPi(pConeNtk, pObj, i) {
    clause[i] = toLitCond(enableSymVar[i], 1); // ~en_i
  }
  Abc_NtkForEachPi(pConeNtk, pObj, i) {
    Abc_NtkForEachPi(pConeNtk, pObj2, j) {
      if (i >= j)
        continue;
      // (i, j) pair that i < j
      if (disjointSetEqual(i, j)) {
        printf("%d %d\n", i, j);
        continue;
      }
      clause[i] = toLit(enableSymVar[i]);
      clause[j] = toLit(enableSymVar[j]);
      bool result = (l_True == sat_solver_solve(pSat, clause, clause+Abc_NtkPiNum(pConeNtk), 0, 0, 0, 0));
      if (!result) { // UNSAT
        printf("%d %d\n", i, j);
        disjointSetMerge(i, j);
      }
      clause[i] = toLitCond(enableSymVar[i], 1);
      clause[j] = toLitCond(enableSymVar[j], 1);
    }
  }
  delete[] clause;
  sat_solver_delete(pSat);
  Cnf_DataFree(pCnf);
  Aig_ManStop(pMan);
  Abc_NtkDelete(pConeNtk);
}

int Lsv_CommandSymAll(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c;
  int k_th;
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
  if (argc != 2)
    goto usage;
  k_th = atoi(argv[1]);
  Lsv_SymAll(pNtk, k_th);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sym_all [-h]\n");
  Abc_Print(-2, "usage: lsv_sym_all <k>\n");
  Abc_Print(-2, "\t        list all (i,j) pairs that i<j, k-th output is symmetric in i-th and j-th input by incremental SAT method\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}
/* lsv_sym_all end */