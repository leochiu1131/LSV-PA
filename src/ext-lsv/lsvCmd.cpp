#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <vector>
#include <set>
#include <algorithm>

using namespace std;

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

void print_cut(int id, vector< set<int> > cut_set){
  for (const auto &s : cut_set) {
    printf("%d:",id);
    for (const auto &i : s) {
        printf(" %d",i);
    }
    printf("\n");
  }
}

void Lsv_NtkPrintCut(Abc_Ntk_t* pNtk, int k) {
  Abc_Obj_t* pObj;

  int obj_num;
  int pi_num;
  obj_num = Abc_NtkObjNum(pNtk);
  pi_num = Abc_NtkPiNum(pNtk);
  //printf("Object num = %d\n", obj_num);
  //printf("Pi num = %d\n", pi_num);

  vector< vector< set<int> > > cut;
  for(int v=0; v<obj_num; v++){
    vector< set<int> > cut_set;
    cut.push_back(cut_set);
  }

  int i;
  Abc_NtkForEachPi(pNtk, pObj, i) {
    int id = Abc_ObjId(pObj);
    set<int> self_cut;
    self_cut.insert(id);
    cut[id].push_back(self_cut);
    print_cut(id, cut[id]);
  }
  Abc_NtkForEachNode(pNtk, pObj, i) {
    int id = Abc_ObjId(pObj);
    set<int> self_cut;
    self_cut.insert(id);
    cut[id].push_back(self_cut);

    Abc_Obj_t* pFanin;
    int j;
    int fanin[2];
    Abc_ObjForEachFanin(pObj, pFanin, j) {
      fanin[j] = Abc_ObjId(pFanin);
    }

    for(const auto &fanin1_cut : cut[fanin[0]]){
      for(const auto &fanin2_cut : cut[fanin[1]]){
        set<int> new_cut;
        set_union(fanin1_cut.begin(), fanin1_cut.end(),
                  fanin2_cut.begin(), fanin2_cut.end(),
                  inserter(new_cut, new_cut.begin()));


        vector<int> erase_id;
        for(const auto &node_id : new_cut){
          if(node_id <= 9) continue;

          for ( auto it = cut[node_id].rbegin() ; it != cut[node_id].rend() ; it++ ){
            set<int> node_cut = *it;
            if(node_cut.count(node_id)) continue;

            if(includes(new_cut.begin(), new_cut.end(), node_cut.begin(), node_cut.end())){
              erase_id.push_back(node_id);
              break;
            }
          }
        }
        for(const auto &node_id : erase_id){
          new_cut.erase(node_id);
        }

        if( (new_cut.size() <= k) && (find(cut[id].begin(), cut[id].end(), new_cut) == cut[id].end()) ){
          cut[id].push_back(new_cut);
        }
      }
    }


    print_cut(id, cut[id]);
    
  }
  // Abc_NtkForEachPo(pNtk, pObj, i) {
  //   printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
  // }
}

int Lsv_CommandPrintCut(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c;

  if(argc <= 1) return 1;

  int k;
  k = int(argv[1][0])-48;
  //printf("k = %d\n",argv[1][0]);
  //printf("k = %d\n",k);

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
  Lsv_NtkPrintCut(pNtk, k);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_printcut [-h]\n");
  Abc_Print(-2, "\t<k>     prints k-feasible cuts\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}