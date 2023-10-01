#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <iostream>
#include <fstream>
#include <vector>
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

void Lsv_SimAig(Abc_Ntk_t* pNtk, vector<int>& inputVec, vector<string>& outputVec) {
  int ithPi;
  Abc_Obj_t* pPi;
  Abc_NtkForEachPi( pNtk, pPi, ithPi) {
    pPi->iTemp = inputVec[ithPi];  
  }

  int ithNode;
  Abc_Obj_t* pNode;
  Abc_NtkForEachNode(pNtk, pNode, ithNode) {
    // cout << pNode->Id << endl;
    int fin0vec = Abc_ObjFanin0(pNode)->iTemp;
    if (pNode->fCompl0) fin0vec = fin0vec ^ 0XFFFFFFFF;
    int fin1vec = Abc_ObjFanin1(pNode)->iTemp;
    if (pNode->fCompl1) fin1vec = fin1vec ^ 0XFFFFFFFF;
    pNode->iTemp = fin0vec & fin1vec;

    // Abc_ObjFaninC0(pNode)
  }

  int ithPo;
  Abc_Obj_t* pPo;
  Abc_NtkForEachPo(pNtk, pPo, ithPo) {
    Abc_Obj_t* poNode = Abc_ObjFanin0(pPo);
    int oPattern = poNode->iTemp; // this correct
    if (poNode->fCompl0) oPattern = oPattern ^ 0XFFFFFFFF;
    if (outputVec.size() == ithPo) {outputVec.push_back(""); }
    for (size_t i = 0; i < 32; ++i) {
      if (oPattern < 0) outputVec[ithPo].append("1");
      else outputVec[ithPo].append("0");
      oPattern = oPattern << 1;
    }
    // outputVec[ithPo].append(to_string(oPattern));
  }

  return;
}

int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c;
  vector<int> inputVec;
  vector<int> nodeVec;
  vector<string> outputVec;
  Extra_UtilGetoptReset();
  

  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  
  if (argc != 2) {
    Abc_Print(-1, "wrong argument.\n");
    return 1;
  }
  
  ifstream patternFile(argv[1]);
  string pattern;
  size_t num_pattern = 0;
  while (patternFile >> pattern) {
    if (num_pattern % 32 == 0 && !inputVec.empty()) {
      // do sim
      Lsv_SimAig(pNtk, inputVec, outputVec);
    }

    if (num_pattern % 32 == 0) {
      if (inputVec.empty()) {
        for (size_t ithInput = 0; ithInput < pattern.size(); ++ithInput) {
          // cout << ithInput << endl;
          inputVec.push_back(0);
        }
      } else {
        for (size_t ithInput = 0; ithInput < pattern.size(); ++ithInput) {
          inputVec[ithInput] = 0;
        }
      }
    }


    for (size_t ithInput = 0; ithInput < pattern.size(); ++ithInput) {
      inputVec[ithInput] = inputVec[ithInput] << 1;
      if (pattern[ithInput] == '1') ++inputVec[ithInput];
    }
    ++num_pattern;
  }

  if (!inputVec.empty()) {
    for (size_t ithInput = 0; ithInput < pattern.size(); ++ithInput) {
      inputVec[ithInput] = inputVec[ithInput] << (32- num_pattern % 32);
    }
    Lsv_SimAig(pNtk, inputVec, outputVec);
  }

  Abc_Obj_t* pPo;
  int ithPo;
  Abc_NtkForEachPo(pNtk, pPo, ithPo) {
    cout << Abc_ObjName(pPo) << ": " << outputVec[ithPo].substr(0, num_pattern) << endl; 
  }

  // Lsv_NtkPrintNodes(pNtk);
  return 0;
}