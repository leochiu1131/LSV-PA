#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <cstdlib>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <vector>
#include <algorithm>
#include <queue>
#include <iostream>
#include <fstream>

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

  std::unordered_map<int, std::vector<std::vector<int>>> node_cuts;
  std::unordered_map<int, std::unordered_set<int>> node_fanins;
  std::unordered_map<int, int> node_fanin_count;
  // bottom up
  std::queue<int> Queue;
  Abc_NtkForEachNode(pNtk, pObj, i) {
    int fanin_count = Abc_ObjFaninNum(pObj);
    int id = Abc_ObjId(pObj);
    node_fanin_count[id] = fanin_count;
  }

  Abc_NtkForEachPo(pNtk, pObj, i) {
    int fanin_count = Abc_ObjFaninNum(pObj);
    int id = Abc_ObjId(pObj);
    node_fanin_count[id] = fanin_count;
  }

  Abc_NtkForEachPi( pNtk, pObj, i ){
    if ( Abc_ObjFanoutNum(pObj) > 0 ) {
      int id = Abc_ObjId(pObj);
      node_cuts[id].push_back(std::vector<int>{id});
      Queue.push(id);
    }
  }  
  

  while(!Queue.empty()){    
    int node_id = Queue.front();
    Queue.pop();

    Abc_Obj_t* pObj = Abc_NtkObj(pNtk, node_id);
    Abc_Obj_t* pFanout;
    int i;
    std::unordered_set<int> next_level_nodes;

    Abc_ObjForEachFanout(pObj, pFanout, i) {
      int fanout_id = Abc_ObjId(pFanout);
      node_fanin_count[fanout_id]--;            
      if(node_fanin_count[fanout_id] == 0){        
        next_level_nodes.insert(fanout_id);
        Queue.push(fanout_id);
      }                 
      node_fanins[fanout_id].insert(node_id);
    }
    
    // merge cuts from fanins
    for(auto node_id : next_level_nodes){         
      int fanin_id1 = -1, fanin_id2 = -1;   
      for(int fanin_id:node_fanins[node_id]){            
        if(fanin_id1 == -1){
          fanin_id1 = fanin_id;
        }else{ 
          fanin_id2 = fanin_id;
        }
      }
      node_cuts[node_id].push_back(std::vector<int>{node_id});
      if(fanin_id2 == -1){
        node_cuts[node_id].insert(node_cuts[node_id].end(),node_cuts[fanin_id1].begin(),node_cuts[fanin_id1].end());
      }else{                
        std::vector<std::vector<int>> merged_cuts;
        for(const auto &cut1:node_cuts[fanin_id1]){
          for(const auto &cut2:node_cuts[fanin_id2]){            
            std::vector<int> merged_cut;
            std::set_union(cut1.begin(),cut1.end(),cut2.begin(),cut2.end(),std::back_inserter(merged_cut));            
            if(merged_cut.size() <= K){
              merged_cuts.push_back(merged_cut);
            }
          }          
        }  
        
        std::sort(merged_cuts.begin(),merged_cuts.end(),[](const std::vector<int>& a, const std::vector<int>& b){
          return a.size() < b.size();
        });      
        std::vector<std::set<int>> check_list;
        for(auto cut:merged_cuts){
          std::set<int> cut_set(cut.begin(),cut.end());
          bool duplicate = false;
          for(auto it : check_list){
            if( cut_set.size() > it.size() && std::includes(cut_set.begin(),cut_set.end(),it.begin(),it.end())){
              duplicate = true;              
              break;
            }
          }
          if(!duplicate){
            node_cuts[node_id].push_back(cut);
            check_list.push_back(cut_set);
          }
        }        
      }      
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