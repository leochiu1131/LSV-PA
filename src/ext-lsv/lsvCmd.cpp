#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <iostream>
#include <set>
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

int Lsv_CommandSimBdd(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c;
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
  // Lsv_NtkPrintNodes(pNtk);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sim_bdd [-h]\n");
  Abc_Print(-2, "\t        sim with bdd, please give the pattern\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}