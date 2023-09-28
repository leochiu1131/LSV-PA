#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <iostream>
#include <fstream>
#include <unordered_map>
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

vector<int> AddPattern(char** argv, Abc_Ntk_t* pNtk) {
  vector<int> pat;
  for (int i = 0; i < pNtk->vPis->nSize; ++i) {
    int p = (argv[1][i] == '1')? 1:0;
    pat.push_back(p);
  }
  return pat;
}

void PrintData(Abc_Obj_t* pRoot, Abc_Obj_t* pPo) {
  cout << "pRoot->Id : " << pRoot->Id << endl;            // 9
  cout << "pPo Name : " << Abc_ObjName(pPo) << endl;      // y3
  for (int j = 0; j < pRoot->vFanins.nSize; ++j) {
    char** vNamesIn = (char**) Abc_NodeGetFaninNames(pRoot)->pArray;
    cout << "vNamesIn[" << j << "] : " << vNamesIn[j] << " (" << pRoot->vFanins.pArray[j]-1 << ")" << endl;    // a1 b0 a0 b1
  }
}

int Lsv_CommandSimBdd(Abc_Frame_t* pAbc, int argc, char** argv) {
  if (argc < 2) {
    cout << "missing pattern" << endl;
    return 1;
  }

  Abc_Ntk_t* pNtk;
  Abc_Obj_t* pPo;
  int ithPo = 0;
  pNtk = Abc_FrameReadNtk(pAbc);
  // add pattern into unordered_map
  vector<int> pattern = AddPattern(argv, pNtk);

  Abc_NtkForEachPo(pNtk, pPo, ithPo) {
    Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo);
    assert( Abc_NtkIsBddLogic(pRoot->pNtk) );
    DdManager * dd = (DdManager *)pRoot->pNtk->pManFunc;
    DdNode* ddnode = (DdNode *)pRoot->pData;
    // PrintData(pRoot, pPo);

    for (int j = 0; j < pRoot->vFanins.nSize; ++j) {
      int n = pRoot->vFanins.pArray[j] - 1;
      DdNode* d = (pattern[n] == 1)? dd->vars[j] : Cudd_Not(dd->vars[j]); 
      ddnode = Cudd_Cofactor(dd, ddnode, d);
    }
    if (ddnode == dd->one) cout << Abc_ObjName(pPo) << ": 1" << endl;
    else cout << Abc_ObjName(pPo) << ": 0" << endl; 
  }
  return 0;
}

vector<string> ReadFile(char* filename) {
  ifstream fin(filename);
  vector<string> pattern;
  string pat;
  while(getline(fin, pat)) {
    pattern.push_back(pat);
  }
  return pattern;
}

int** Create2dimArray (int f_dim, int s_dim) {
  int** arr = new int*[f_dim];
  for (int i = 0; i < f_dim; ++i) {
    arr[i] = new int[s_dim];
  }
  for (int i = 0; i < f_dim; ++i) {
    for (int j = 0; j < s_dim; ++j) {
      arr[i][j] =0;
    }
  }
  return arr;
}

void PrintBinary(int in, int lim) {
  int a = 1;
  for (int i = 0; i < lim; ++i) {
    if ((in & (a << i)) == 0) cout << '0';
    else                      cout << '1';
  }
}

void PrintPattern(int** ptn, int Pi_num, int ptn_size) {
  for (int i = 0; i < Pi_num; ++i) {
    for (int j = 0; j < ptn_size; ++j) {
      cout << "i = " << i << ", j = " << j << " : ";
      PrintBinary(ptn[i][j], 32);
    }
  }
}

int** StrToIntPtn(vector<string> ptn_str) {
  int Pi_num = ptn_str[0].size() - 1;
  int ptn_str_size = ptn_str.size();
  int ptn_size = (ptn_str_size % 32 == 0)? (ptn_str_size/32) : (ptn_str_size/32 + 1);
  int** ptn = Create2dimArray(Pi_num, ptn_size);
  for (int i = 0; i < ptn_str_size; ++i) {
    for (int j = 0; j < Pi_num; ++j) {
      int p = (ptn_str[i][j] == '1')? 1 : 0;
      ptn[j][i/32] = ptn[j][i/32] | (p << i);
    }
  }
  // PrintPattern(ptn, Pi_num, ptn_size);
  return ptn;
}

void DFS(vector<Abc_Obj_t*>& DFS_List, Abc_Obj_t* curNode) {
  Abc_Obj_t* Fanin;
  int i = 0;
  Abc_ObjForEachFanin(curNode, Fanin, i) {
    DFS(DFS_List, Fanin);
  }
  bool exist = false;
  for (int i = 0; i < DFS_List.size(); ++i) {
    if (DFS_List[i] == curNode) {
       exist = true;
       break;
    }
  }
  if(!exist)  DFS_List.push_back(curNode);
}

int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv) {
  if (argc < 2) {
    cout << "missing file" << endl;
    return 1;
  }
  // read file
  vector<string> ptn_str = ReadFile(argv[1]);
  // convert Intpt file into int pattern
  int** ptn = StrToIntPtn(ptn_str);
  int ptn_fdim = ptn_str[0].size() - 1;
  int ptn_sdim = (ptn_str.size() % 32 == 0)? (ptn_str.size()/32) : (ptn_str.size()/32 + 1);

  // construct DFS List
  Abc_Ntk_t* pNtk;
  Abc_Obj_t* pPo;
  int ithPo = 0;
  pNtk = Abc_FrameReadNtk(pAbc);
  vector<Abc_Obj_t*> DFS_List;
  Abc_NtkForEachPo(pNtk, pPo, ithPo) {
    Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo);
    DFS(DFS_List, pRoot);
  }

  int** output = Create2dimArray(ptn_fdim, ptn_sdim);

  // simulation
  for (int i = 0; i < ptn_sdim; ++i) {
    unordered_map<Abc_Obj_t*, int> result;
    Abc_Obj_t* pPi;
    int ithPi = 0;
    Abc_NtkForEachPi(pNtk, pPi, ithPi) {
      result.insert(pair<Abc_Obj_t*, int>(pPi, ptn[ithPi][i]));
    }
    for (int j = 0; j < DFS_List.size(); ++j) {
      Abc_Obj_t* curNode = DFS_List[j];
      if (result.find(curNode) == result.end()) {
        int fin0res = (Abc_ObjFaninC0(curNode))? ~result[Abc_ObjFanin0(curNode)] : result[Abc_ObjFanin0(curNode)];
        int fin1res = (Abc_ObjFaninC1(curNode))? ~result[Abc_ObjFanin1(curNode)] : result[Abc_ObjFanin1(curNode)];
        int res = fin0res & fin1res;
        result.insert(pair<Abc_Obj_t*, int>(curNode, res));
      }
    }
    Abc_NtkForEachPo(pNtk, pPo, ithPo) {
      Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo);
      output[ithPo][i] = Abc_ObjFaninC0(pPo)? ~result[pRoot] : result[pRoot];
    }
  }
  
  // print output
  Abc_NtkForEachPo(pNtk, pPo, ithPo) {
    cout << Abc_ObjName(pPo) << " : ";
    for (int i = 0; i < ptn_sdim; ++i) {
      int lim = (i == ptn_sdim-1)? ptn_str.size()-32*i : 32;
      PrintBinary(output[ithPo][i], lim);
    }
    cout << endl;
  }

  return 0;
}