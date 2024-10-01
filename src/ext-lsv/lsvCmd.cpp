#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <vector>
#include <algorithm>

using std::vector;
using std::sort;

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintCut(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_CommandPrintCut, 0);
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

void Lsv_PrintCuts(Abc_Ntk_t* pNtk, vector<int> Cut_vec, Abc_Obj_t* root, Abc_Obj_t* now_node, int k,vector<vector<int> >& solutions) {
  int Id_root = Abc_ObjId(root);
  int ID = Abc_ObjId(now_node);
  int num_of_fanins = Abc_ObjFaninNum(now_node);
  /*
  if(num_of_fanins > k) {
    //printf("Too much : %d fanin_s in %d\n", num_of_fanins, ID);
    return;
  }
  */
  if(num_of_fanins == 0) {
    return;
  }
  
  int i = 0;
  while(i < Cut_vec.size()) {   //remove myself
    if(Cut_vec[i] == ID) {
      Cut_vec.erase(Cut_vec.begin() + i);
      break;
    }
    i++;
  }
  //printf("VECSIZE : %d\n", Cut_vec.size());
  Abc_Obj_t* pFanin;
  int t;
  Abc_ObjForEachFanin( now_node, pFanin, t ){
    int Id_fanin = Abc_ObjId(pFanin);
    bool Not_in_cut = true;
    for(int ttt = 0; ttt < Cut_vec.size(); ttt++) {
      if(Cut_vec[ttt] == Id_fanin) Not_in_cut = false;
    }
    if(Not_in_cut) Cut_vec.push_back(Id_fanin);
  }
  sort(Cut_vec.begin(), Cut_vec.end());
  //printf("Solution size : %d\n", solutions.size());
  for(int i = 0; i < solutions.size(); i++) {
    if(solutions[i].size() == Cut_vec.size()){
      bool all_same = true;
      for(int j = 0; j < Cut_vec.size(); j++) {
        if(Cut_vec[j] != solutions[i][j]) {
          all_same = false;
          break;
        }
      }
      if(all_same) return;
    }
  }
  solutions.push_back(Cut_vec);
  if(Cut_vec.size() <= k) {
    printf("%d:", Id_root);
    vector<int> B(Cut_vec.size());
    for(int i = 0; i < Cut_vec.size(); i++) {
      printf(" %d", Cut_vec[i]);
    }
    printf("\n");
  }
  for(int i = 0; i < Cut_vec.size(); i++) {
    Abc_Obj_t* pFanin_new = Abc_NtkObj(pNtk, Cut_vec[i]);
    if(Abc_ObjFaninNum(pFanin_new) > 0) Lsv_PrintCuts(pNtk, Cut_vec, root, pFanin_new, k, solutions);
    /*
    for(int ttt = 0; ttt < Cut_vec.size(); ttt++) {
      if(Cut_vec[ttt] == Id_fanin) Lsv_PrintCuts(pNtk, Cut_vec, root, pFanin_new, k);
    }
    */
  }
  return;
}
//Abc_NtkObj(pNtk, i)

int Lsv_CommandPrintCut(Abc_Frame_t* pAbc, int argc, char** argv) {
  if(argc != 2) {
    printf("Format should be \" lsv_printcut <k> \" where k is an integer between [3,6]\n");
    return 1;
  }
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int k = 0;
  for(int i = 0; argv[1][i] != '\0'; i++) {
    k = k * 10;
    k += argv[1][i] - '0';
  }
  if(k < 3 || k > 6) {
    printf("k should be an integer between [3,6]");
  }
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  Abc_Obj_t* pObj;
  int i;
  Abc_NtkForEachObj(pNtk, pObj, i) {
    vector<int> Cut_nodes;
    vector<vector<int> > solutions;
    Cut_nodes.push_back(Abc_ObjId(pObj));
    printf("%d: %d\n", Abc_ObjId(pObj), Abc_ObjId(pObj));
    Lsv_PrintCuts(pNtk, Cut_nodes, pObj, pObj, k, solutions);
  }
  return 0;
}