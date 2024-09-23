#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <cstdlib>
#include <map>
#include <set>
#include <vector>
#include <algorithm>
#include <queue>

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


void Lsv_NtkPrintCut(Abc_Ntk_t* pNtk, int K) {
  Abc_Obj_t* pObj;  
  int i;

  std::map<int, std::set<std::set<int>>> node_cuts;
  // bottom up
  std::queue<int> Queue;
  Abc_NtkForEachCi( pNtk, pObj, i ){
    if ( Abc_ObjFanoutNum(pObj) > 0 ) {
      int id = Abc_ObjId(pObj);
      node_cuts[id].insert(std::set<int>{id});
      Queue.push(id);
    }    
  }
  int level = 0;
  while(!Queue.empty()){
    int level_size = Queue.size();
    std::set<int> next_level_nodes;
    for(int i=0;i<level_size;i++){
      int node_id = Queue.front();
      Queue.pop();
      Abc_Obj_t* pObj = Abc_NtkObj(pNtk, node_id);
      Abc_Obj_t* pFanout;
      int j;      
      Abc_ObjForEachFanout(pObj, pFanout, j) {
        int fanout_id = Abc_ObjId(pFanout);
        if(next_level_nodes.find(fanout_id) == next_level_nodes.end()){
          next_level_nodes.insert(fanout_id);
          Queue.push(fanout_id);           
        }                
      }
    }
    level++;
    // merge cuts from fanins
    for(auto node_id:next_level_nodes){      
      Abc_Obj_t* pObj = Abc_NtkObj(pNtk, node_id);
      Abc_Obj_t* pFanin;
      int j;
      std::set<std::set<int>> cuts1,cuts2;
      int fanin_count = 0;
      Abc_ObjForEachFanin(pObj, pFanin, j) {
        int fanin_id = Abc_ObjId(pFanin);        
        if(fanin_count == 0){
          cuts1 = node_cuts[fanin_id];
        }else{ 
          cuts2 = node_cuts[fanin_id];
        }
        fanin_count++;
      }
      std::set<std::set<int>> merged_cuts;
      merged_cuts.insert(std::set<int>{node_id});

      if(fanin_count == 1){
        merged_cuts.insert(cuts1.begin(),cuts1.end());
      }else{
        for(auto cut1:cuts1){
          for(auto cut2:cuts2){
            std::vector<int> merged_cut;
            std::set_union(cut1.begin(),cut1.end(),cut2.begin(),cut2.end(),std::back_inserter(merged_cut));
            if(merged_cut.size() <= K){              
              merged_cuts.insert(std::set<int>(merged_cut.begin(),merged_cut.end()));
            }
          }
        }              
      }
      node_cuts[node_id] = merged_cuts;
    }
  }
  
  for (const auto &it : node_cuts) {
    int node_id = it.first;    
    for(auto cut:it.second){
      printf("%d: ",node_id);
      for(auto id:cut){
        printf("%d ",id);
      }
      printf("\n");
    }
  }
}

int Lsv_CommandPrintCut(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);

  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
      
  if (argc != 2) {
    Abc_Print(-1, "Missing cut size.\n");
    return 1;
  }  
  Lsv_NtkPrintCut(pNtk, atoi(argv[1]));
  return 0;
}