#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "bdd/extrab/extraBdd.h"

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimBdd(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimAIG(Abc_Frame_t* pAbc, int argc, char** argv);


void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", Lsv_CommandSimBdd, 0);
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
