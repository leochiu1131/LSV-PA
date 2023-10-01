#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "bdd/extrab/extraBdd.h"

// For file parser in 4-2
#include <fstream>
#include <iostream>
#include <vector>
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
int* AIGInputFileParser(const char* intputfile, int ntkInput_num){
  
  ifstream infile(intputfile);
  std::string pattern;
  int* patternPI = new int[ntkInput_num+1];
  int width = 0;

  while (infile >> pattern) {
    
    if (pattern.size() != ntkInput_num) {
      Abc_Print(-1, "Dismatch number of input pattern and network input.\n");
      return 0;
    }
    for (int i = 0; i < ntkInput_num; ++i) {
      if (pattern[i] != '0' && pattern[i] != '1') {
        Abc_Print(-1, "Invalid input pattern!.\n");
        return 0;
      }
      patternPI[i] = (patternPI[i] << 1) | (pattern[i] == '1');
    }
    width++;
    if (width>32){
      Abc_Print(-1, "Do not support parallel simulation more than 32 bits.");
      return 0;
    }
  }
  patternPI[ntkInput_num+1] = width;
  // printf("Width: %i\n", width);
  return patternPI;
}

void Lsv_print_result(Abc_Ntk_t* pNtk, int width){

  int ithPo;
  Abc_Obj_t* pObj;
  Abc_NtkForEachPo(pNtk, pObj, ithPo) {
    Abc_Obj_t* f0 = Abc_ObjFanin0(pObj);
    int v0 = Abc_ObjFaninC0(pObj) ? ~(f0->iTemp) : (f0->iTemp);
    pObj->iTemp = v0;
    
    string output;
    for (int j=0; j<width; ++j) {
      // Abc_Print(1, "%i", (((v0 >> j) & 1) ? "1" : "0"));
      output = (((v0 >> j) & 1) ? "1" : "0") + output;
    }
    Abc_Print(1, "%s: %s\n", Abc_ObjName(pObj), output.c_str());
  }
}


void Lsv_Sim_Aig(Abc_Ntk_t* pNtk, int* patterns, int ntkInput_num){
  
  int width = patterns[ntkInput_num+1];
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

  Lsv_print_result(pNtk, width);

  return;
}

int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv) {
  
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  char* input_file = argv[1];
  int ntkInput_num = Abc_NtkPiNum(pNtk);
  int* input_pattern = AIGInputFileParser(input_file, ntkInput_num);

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
  if (!input_pattern) return 1;

  Lsv_Sim_Aig(pNtk, input_pattern, ntkInput_num);

  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sim_aig [-h]\n");
  Abc_Print(-2, "       Nothing to say. [-h]\n");
  return 1;
}