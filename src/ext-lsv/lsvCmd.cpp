#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "bdd/cudd/cudd.h"

#include <fstream>
#include <string>
#include <vector>

using namespace std;

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimBDD(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimAIG(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", Lsv_CommandSimBDD, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig", Lsv_CommandSimAIG, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

void Lsv_NtkPrintNodes(Abc_Ntk_t* pNtk) {
  Abc_Obj_t* pObj;
  int i;
  printf("Pi:\n");
  Abc_NtkForEachPi(pNtk, pObj, i) {
    printf("Object Id = %d, name = %s, idata = %d\n", Abc_ObjId(pObj), Abc_ObjName(pObj), pObj->iData);
  }
  printf("Node:\n");
  Abc_NtkForEachNode(pNtk, pObj, i) {
    printf("Object Id = %d, name = %s, idata = %d\n", Abc_ObjId(pObj), Abc_ObjName(pObj), pObj->iData);
    Abc_Obj_t* pFanin;
    int j;
    Abc_ObjForEachFanin(pObj, pFanin, j) {
      printf("  Fanin-%d: Id = %d, name = %s, comp0 = %d, comp1 = %d\n", j, Abc_ObjId(pFanin),
             Abc_ObjName(pFanin), pFanin->fCompl0, pFanin->fCompl1);
    }
    if (Abc_NtkHasSop(pNtk)) {
      printf("The SOP of this node:\n%s", (char*)pObj->pData);
    }
  }
  printf("Po:\n");
  Abc_NtkForEachPo(pNtk, pObj, i) {
    printf("Object Id = %d, name = %s, idata = %d\n", Abc_ObjId(pObj), Abc_ObjName(pObj), pObj->iData);
    Abc_Obj_t* pFanin;
    int j;
    Abc_ObjForEachFanin(pObj, pFanin, j) {
      printf("  Fanin-%d: Id = %d, name = %s, comp0 = %d, comp1 = %d\n", j, Abc_ObjId(pFanin),
             Abc_ObjName(pFanin), pFanin->fCompl0, pFanin->fCompl1);
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

bool* Lsv_ParsePattern(char* arg, int nPi) {
  bool* pattern = (bool*)malloc(sizeof(bool) * nPi);
  for (int i = 0; i < nPi; ++i) {
    pattern[i] = (arg[i] == '1');
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

    Abc_Obj_t* pFi;
    int iFi;
    Abc_ObjForEachFanin(pRoot, pFi, iFi) {
      DdNode* idd = Cudd_bddIthVar(dd, iFi);
      idd = Cudd_NotCond(idd, !pattern[iFi]);
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
  bool* pattern = Lsv_ParsePattern(argv[1], Abc_NtkPiNum(pNtk));
  Lsv_NtkSimBDD(pNtk, pattern);
  Lsv_FreePattern(pattern);
  return 0;
}

void Lsv_NtkSimAIGBatch(Abc_Ntk_t* pNtk, vector<bool *>::iterator begin, vector<bool *>::iterator end) {
  Abc_Obj_t* pObj;
  int i;
  Abc_NtkForEachPi(pNtk, pObj, i) {
    pObj->iData = 0;
    for (auto it = begin; it < end; ++it) {
      pObj->iData <<= 1;
      pObj->iData |= (*it)[i];
    }
  }
  Abc_NtkForEachNode(pNtk, pObj, i) {
    Abc_Obj_t* pFanin0 = Abc_ObjFanin0(pObj);
    Abc_Obj_t* pFanin1 = Abc_ObjFanin1(pObj);

    int iFanin0 = Abc_ObjFaninC0(pObj) ? ~pFanin0->iData : pFanin0->iData;
    int iFanin1 = Abc_ObjFaninC1(pObj) ? ~pFanin1->iData : pFanin1->iData;

    pObj->iData = iFanin0 & iFanin1;
  }
  Abc_NtkForEachPo(pNtk, pObj, i) {
    Abc_Obj_t* pFanin = Abc_ObjFanin0(pObj);
    pObj->iData = Abc_ObjFaninC0(pObj) ? ~pFanin->iData : pFanin->iData;
    for (auto it = begin; it < end; ++it) {
      *(string*)pObj->pTemp += to_string((pObj->iData >> (end - it - 1)) & 1);
    }
  }
}

int Lsv_CommandSimAIG(Abc_Frame_t* pAbc, int argc, char** argv) {
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

  ifstream fin;
  fin.open(argv[1]);
  vector<string> inputs;
  string buffer;
  while (fin >> buffer)
      inputs.push_back(buffer);
  fin.close();

  vector<bool*> pattern;
  for (auto input : inputs)
      pattern.push_back(Lsv_ParsePattern((char*)input.c_str(), Abc_NtkPiNum(pNtk)));
  
  Abc_Obj_t* pObj;
  int i;
  Abc_NtkForEachPo(pNtk, pObj, i) {
    pObj->pTemp = new string("");
  }
  
  for (auto begin = pattern.begin(); begin < pattern.end(); begin += 32) {
    begin + 32 < pattern.end() ? Lsv_NtkSimAIGBatch(pNtk, begin, begin + 32) : Lsv_NtkSimAIGBatch(pNtk, begin, pattern.end());
  }
  
  for (auto p : pattern)
      Lsv_FreePattern(p);  
  
  Abc_NtkForEachPo(pNtk, pObj, i) {
    printf("%s: %s\n", Abc_ObjName(pObj), ((string*)pObj->pTemp)->c_str());
    delete (string*)pObj->pTemp;
  }

  return 0;
}