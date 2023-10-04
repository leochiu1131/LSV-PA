#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"

#include <string>
#include <bdd/cudd/cudd.h>
#include <fstream>

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

/* lsv_sim_bdd Command*/
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


/* lsv_sim_aig Command*/

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