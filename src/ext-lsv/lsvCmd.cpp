#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <iostream>
using namespace std;

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimBdd(Abc_Frame_t* pAbc, int argc, char** argv);
// static int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", Lsv_CommandSimBdd, 0);
  // Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig", Lsv_CommandSimAig, 0);
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

void Lsv_NtkSimBdd(Abc_Ntk_t* pNtk, char* arg) {
  int inputNum = Abc_NtkCiNum(pNtk);
  bool inputValue[inputNum];
  for (int i = 0; i < inputNum; ++i) {
    inputValue[i] = (arg[i] == '1');
  }
  // int orderMap[inputNum];
  // // blif order
  // char** inputNameArray = Abc_NtkCollectCioNames(pNtk, 0);

  // int i;
  // Abc_Obj_t* pPi;
  // // blif order
  // Abc_NtkForEachPi(pNtk, pPi, i) {
  //   for (int j = 0; j < inputNum; ++j) {
  //     if (strcmp(inputNameArray[j], Abc_ObjName(pPi)) == 0) {
  //       // orderMap[j] = i;
  //     }
  //   }
  // }

  
  // cout << inputNameArray[0] << inputNameArray[1]<<inputNameArray[2]<<inputNameArray[3]<<endl;

  DdManager* dd = (DdManager*)pNtk->pManFunc; 
  int ithPo; Abc_Obj_t* pPo;
  Abc_NtkForEachPo(pNtk, pPo, ithPo) {
    cout << Abc_ObjName(pPo) << ": ";
    Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo);
    // cout << Abc_ObjName(pRoot) << endl;
    DdNode* ddnode = (DdNode *)pRoot->pData;
    // bdd order
    char** vNamesIn = (char**) Abc_NodeGetFaninNames(pRoot)->pArray;
    // cout << vNamesIn[0] << vNamesIn[1]<<vNamesIn[2]<<vNamesIn[3]<<endl;
    // cout << "poInputNum: " << poInputNum <<endl;
    for (int i = 0; i < Abc_NtkPiNum(pNtk); ++i) {
      if (!vNamesIn[i]) continue;
      if ( vNamesIn[i][0] == '\0') continue;
      Abc_Obj_t* pPi;
      int j;
      Abc_NtkForEachPi(pNtk, pPi, j) {
        // find matching input
        if (strcmp(vNamesIn[i], Abc_ObjName(pPi)) == 0) {
          if (inputValue[j]) {
            ddnode = Cudd_Cofactor(dd, ddnode, dd->vars[i]);
          } else {
            ddnode = Cudd_Cofactor(dd, ddnode, Cudd_Not(dd->vars[i]));
          }
        }
      }
    }
    // for (int i = 0; i < poInputNum; ++i) {
    //   if (inputValue[]) {
    //     ddnode = Cudd_Cofactor(dd, ddnode, dd->vars[orderMap[]]);
    //   } else {
    //     ddnode = Cudd_Cofactor(dd, ddnode, Cudd_Not(dd->vars[orderMap[j]]));
    //   }
    // }
    if (ddnode == Cudd_ReadOne(dd) || ddnode == Cudd_Not(Cudd_ReadZero(dd))) {
      cout << 1 << endl;
    }
    if (ddnode == Cudd_ReadZero(dd) || ddnode == Cudd_Not(Cudd_ReadOne(dd))) {
      cout << 0 << endl;
    }  //else    cout << 1 << endl;

    // cout << ddnode << endl;
  }
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
  if (!Abc_NtkIsBddLogic(pNtk)) {
    Abc_Print(-1, "Network is not logic BDD networks (run \"collapse\").\n");
    return 1;
  }
  // if () {
  //   Abc_Print(-1, "Network is not logic BDD networks (run \"collapse\").\n");
  //   return 1;
  // }
  Lsv_NtkSimBdd(pNtk, argv[1]);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sim_bdd [-h]\n");
  Abc_Print(-2, "\t        simulate the Ntk with bdd\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}