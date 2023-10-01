#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "bdd/cudd/cudd.h"

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimBDD(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", Lsv_CommandSimBDD, 0);
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

bool* Lsv_ParsePatternComplement(char* arg, int nPi) {
  bool* pattern = (bool*)malloc(sizeof(bool) * nPi);
  for (int i = 0; i < nPi; ++i) {
    pattern[i] = (arg[i] == '0');
  }
  return pattern;
}

void Lsv_FreePattern(bool* pattern) { free(pattern); }

void Lsv_NtkSimBDD(Abc_Ntk_t* pNtk, bool* pattern) {
  Abc_Obj_t* pPo;
  int iPo;
  Abc_NtkForEachPo(pNtk, pPo, iPo) {
    Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo); 
    assert( Abc_NtkIsBddLogic(pRoot->pNtk) );
    DdManager * dd = (DdManager *)pRoot->pNtk->pManFunc;  
    DdNode* ddnode = (DdNode *)pRoot->pData;

    char** vNamesIn = (char**) Abc_NodeGetFaninNames(pRoot)->pArray;

    Abc_Obj_t* pFi;
    int iFi;
    Abc_ObjForEachFanin(pRoot, pFi, iFi) {
      DdNode* idd = Cudd_bddIthVar(dd, iFi);
      idd = Cudd_NotCond(idd, pattern[iFi]);
      ddnode = Cudd_Cofactor(dd, ddnode, idd);
    }

    printf("%s: %d\n", Abc_ObjName(pPo), ddnode == Cudd_ReadOne(dd));
  }
}

int Lsv_CommandSimBDD(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c;
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
    switch (c) {
      case 'h':
      default:
        Abc_Print(-2, "usage: lsv_print_nodes [-h]\n");
        Abc_Print(-2, "\t        prints the nodes in the network\n");
        Abc_Print(-2, "\t-h    : print the command usage\n");
        return 1;
    }
  }
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  else if (argc != 2) {
    Abc_Print(-1, "Wrong number of arguments.\n");
    return 1;
  }
  bool* pattern = Lsv_ParsePatternComplement(argv[1], Abc_NtkPiNum(pNtk));
  Lsv_NtkSimBDD(pNtk, pattern);
  Lsv_FreePattern(pattern);
  return 0;
}