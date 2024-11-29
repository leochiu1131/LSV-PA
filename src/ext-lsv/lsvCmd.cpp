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
#include <random>
#include "sat/cnf/cnf.h"

extern "C"{
 Aig_Man_t* Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
}

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandComputeSDC(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintCut(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandComputeODC(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_CommandPrintCut, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sdc", Lsv_CommandComputeSDC, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_odc", Lsv_CommandComputeODC, 0);
  
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



//Compute SDC
std::vector<std::vector<int>> simulation_filter(Abc_Ntk_t *Ntk, int node_idx, int pattern_num){
    
  int pattern_size = Abc_NtkPiNum(Ntk);  
  std::vector<std::vector<int>> patterns;  
  // random simulation
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, 1);
  for (int i = 0; i < pattern_num; ++i) {
    std::vector<int> pattern;
    for (int j = 0; j < pattern_size; ++j) {
      pattern.push_back(dis(gen));
    }
    patterns.push_back(pattern);
  }  

  Abc_Obj_t* node = Abc_NtkObj(Ntk, node_idx);  
  Abc_Obj_t* node_fanin0 = Abc_ObjFanin0(node);
  Abc_Obj_t* node_fanin1 = Abc_ObjFanin1(node);  
  bool fanin0_inv = Abc_ObjFaninC0(node);
  bool fanin1_inv = Abc_ObjFaninC1(node);
  std::unordered_set<int> remain_fanin_combinations({0,1,2,3});

  for(int i = 0; i < pattern_num; i++){
    Abc_Obj_t* obj;
    int j;        
    Abc_NtkForEachPi(Ntk, obj, j){
      obj -> pData = (void*) (intptr_t) patterns[i][j];
    }

    Abc_NtkForEachNode(Ntk, obj, j){
      int v0 = (int) (intptr_t) Abc_ObjFanin0(obj) -> pData;
      int v1 = (int) (intptr_t) Abc_ObjFanin1(obj) -> pData;
      
      if(Abc_ObjFaninC0(obj)){
        v0 = 1 - v0;
      }
      if(Abc_ObjFaninC1(obj)){
        v1 = 1 - v1;
      }

      int value = v0 & v1;
      obj -> pData = (void*) (intptr_t) value;
    }

    int y0 = (int) (intptr_t) node_fanin0 -> pData;
    int y1 = (int) (intptr_t) node_fanin1 -> pData;
    int fanin_combination = 2 * y0 + y1;
    remain_fanin_combinations.erase(fanin_combination);    
  }

  std::vector<std::vector<int>> rets;
  for(int i : remain_fanin_combinations){
    int v0_in = i / 2;
    int v1_in = i % 2;
    int v0 = (fanin0_inv) ? 1 - v0_in : v0_in;    
    int v1 = (fanin1_inv) ? 1 - v1_in : v1_in;
    int y = v0 & v1;
    rets.push_back({v0_in, v1_in, y});
  }
  return rets;
}


bool SAT_check_fanin_combination(Abc_Ntk_t* Ntk, int node_id, const std::vector<int> &fanin_combination){    
  sat_solver* pSat = sat_solver_new();
  int i;
  Abc_Obj_t* pObj;
  std::unordered_map<int, int> aig_var_map;
  Abc_NtkForEachNode(Ntk, pObj, i){
    int id = Abc_ObjId(pObj);
    if(node_id == id){
      break;
    }
    int iVar, iVar0, iVar1;
    if(aig_var_map.find(id) == aig_var_map.end()){
      int idx = aig_var_map.size();
      iVar = aig_var_map[id] = idx;      
    }else{
      iVar = aig_var_map[id];
    }

    int fanin0_id = Abc_ObjId(Abc_ObjFanin0(pObj));
    if(aig_var_map.find(fanin0_id) == aig_var_map.end()){
      int idx = aig_var_map.size();
      aig_var_map[fanin0_id] = idx;
      iVar0 = idx;
    }else{
      iVar0 = aig_var_map[fanin0_id];
    }
  
    int fanin1_id = Abc_ObjId(Abc_ObjFanin1(pObj));
    if(aig_var_map.find(fanin1_id) == aig_var_map.end()){
      int idx = aig_var_map.size();
      aig_var_map[fanin1_id] = idx;
      iVar1 = idx;
    }else{
      iVar1 = aig_var_map[fanin1_id];
    }

    bool fCompl0 = Abc_ObjFaninC0(pObj);
    bool fCompl1 = Abc_ObjFaninC1(pObj);

    // create clause1
    lit Lits_1[2];
    // ~A or B
    Lits_1[0] = toLitCond(iVar, 0);
    ////std::cout<<-iVar<<" ";
    if(fCompl0){
      Lits_1[1] = toLitCond(iVar0, 0);
      ////std::cout<<-iVar0<<" 0"<<std::endl;
    }else{
      Lits_1[1] = toLitCond(iVar0, 1);
      ////std::cout<<iVar0<<" 0"<<std::endl;
    }
    int Cid = sat_solver_addclause(pSat, Lits_1, Lits_1 + 2);

    // create clause2
    lit Lits_2[2];
    // ~A or C
    Lits_2[0] = toLitCond(iVar, 0);
    ////std::cout<<-iVar<<" ";
    if(fCompl1){
      Lits_2[1] = toLitCond(iVar1, 0);
      ////std::cout<<-iVar1<<" 0"<<std::endl;
    }else{
      Lits_2[1] = toLitCond(iVar1, 1);
      ////std::cout<<iVar1<<" 0"<<std::endl;
    }
    Cid = sat_solver_addclause(pSat, Lits_2, Lits_2 + 2);

    // create clause3
    // A or ~B or ~C
    lit Lits_3[3];
    Lits_3[0] = toLitCond(iVar, 1);
    ////std::cout<<iVar<<" ";
    if(fCompl0){
      Lits_3[1] = toLitCond(iVar0, 1);
      ////std::cout<<iVar0<<" ";
    }else{
      Lits_3[1] = toLitCond(iVar0, 0);
      ////std::cout<<-iVar0<<" ";
    }
    if(fCompl1){
      Lits_3[2] = toLitCond(iVar1, 1);
      ////std::cout<<iVar1<<" 0"<<std::endl;
    }else{
      Lits_3[2] = toLitCond(iVar1, 0);
      ////std::cout<<-iVar1<<" 0"<<std::endl;
    }
    Cid = sat_solver_addclause(pSat, Lits_3, Lits_3 + 3);
  }
  
  Abc_Obj_t* node = Abc_NtkObj(Ntk, node_id);
  Abc_Obj_t* fanin0 = Abc_ObjFanin0(node);
  Abc_Obj_t* fanin1 = Abc_ObjFanin1(node);
  
  int iVar0 = aig_var_map[Abc_ObjId(fanin0)];
  int iVar1 = aig_var_map[Abc_ObjId(fanin1)];  

  int v0 = fanin_combination[0];
  int v1 = fanin_combination[1];  
  int Cid;  
  //add clasue constraints
  
  lit v0_Lits[1];
  v0_Lits[0] = toLitCond(iVar0, v0);
  if(v0 == 0){
    ////std::cout<<-iVar0<<" 0\n";
  }else{
    ////std::cout<<iVar0<<" 0\n";
  }
  Cid = sat_solver_addclause(pSat, v0_Lits, v0_Lits + 1);

  lit v1_Lits[1];
  v1_Lits[0] = toLitCond(iVar1, v1);
  if(v1 == 0){
    ////std::cout<<-iVar1<<" 0\n";
  }else{
    ////std::cout<<iVar1<<" 0\n";
  }
  Cid = sat_solver_addclause(pSat, v1_Lits, v1_Lits + 1);
             
  // solve sat
  int status = sat_solver_solve(pSat, NULL, NULL, 0, 0, 0, 0);
  bool unsat = (status == l_False);

  sat_solver_delete(pSat);
  return unsat;
}


std::set<int> Lsv_NtkComputeSDC(Abc_Ntk_t* pNtk, int node_id){
  std::set<int> sdc_set;
  const int PATTERN_NUM = 0;  
  std::vector<std::vector<int>> remain_fanin_combinations;
  Abc_Obj_t* node_in_ntk = Abc_NtkObj(pNtk,node_id);
  Abc_Ntk_t* ConeNtk = Abc_NtkCreateCone(pNtk, node_in_ntk, "c", 0);
  Abc_Obj_t* node_fanout = Abc_NtkPo(ConeNtk,0);  
  Abc_Obj_t* node = Abc_ObjFanin0(node_fanout);
  int new_node_id = Abc_ObjId(node);
  remain_fanin_combinations = simulation_filter(ConeNtk, new_node_id, PATTERN_NUM);
  
  bool flag = true;
  for(const auto &fanin_combination : remain_fanin_combinations){
    bool is_fanin_combination_sdc = SAT_check_fanin_combination(ConeNtk, new_node_id, fanin_combination);
    if(is_fanin_combination_sdc){
      flag = false;
      bool fCompl0 = Abc_ObjFaninC0(node);
      bool fCompl1 = Abc_ObjFaninC1(node);
      int y0 = (fCompl0)? 1-fanin_combination[0] : fanin_combination[0];
      int y1 = (fCompl1)? 1-fanin_combination[1] : fanin_combination[1];
      sdc_set.insert(2*y0+y1);
    }
  }
  return sdc_set;
}

int Lsv_CommandComputeSDC(Abc_Frame_t* pAbc, int argc, char** argv){
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  char* c = argv[1];
  int node_id = std::atoi(c);

  if(!pNtk){
    Abc_Print(-1, "Empty network.\n");
    return 0; 
  }

  Abc_Obj_t* pObj;  
  pObj = Abc_NtkObj(pNtk, node_id);
  bool isNode = Abc_ObjIsNode(pObj);
  if(!isNode){
    Abc_Print(-1, "Not a node.\n");
    return 0; 
  }
  

  std::set<int> sdc_set = Lsv_NtkComputeSDC(pNtk,node_id);
  if(sdc_set.size() == 0){
    std::cout<<"no sdc\n";
  }else{
    for(auto num : sdc_set){
      char c0 = (num & 2) ? '1' : '0';
      char c1 = (num & 1) ? '1' : '0';
      std::cout<<c0<<c1<<" ";
    }
    std::cout<<std::endl;
  }
  return 0;
}
//

//Compute ODC

std::set<int> Lsv_NtkComputeODC(Abc_Ntk_t* pNtk, int node_id){
  std::set<int> odc_set;
  // step1 duplicate the network
  Abc_Ntk_t* pNtk_neg_node = Abc_NtkDup(pNtk);
  Abc_Obj_t* pObj;
  // step2 
  // find all node_id's fanouts
  // add inv node between node_id and its fanouts  
  Abc_Obj_t* neg_node = Abc_NtkObj(pNtk_neg_node, node_id);
  Abc_Obj_t* neg_node_fanout;
  int i;
  int Cid;
  Abc_ObjForEachFanout(neg_node, neg_node_fanout, i){
    int fanout_id = Abc_ObjId(neg_node_fanout);
    //std::cout<<"fanout_id: "<<fanout_id<<std::endl;
    if(Abc_ObjFanin0(neg_node_fanout) == neg_node){
      //std::cout<<"fanin0"<<std::endl;
      neg_node_fanout->fCompl0 = ~neg_node_fanout->fCompl0;
    }else{
      //std::cout<<"fanin1"<<std::endl;
      neg_node_fanout->fCompl1 = ~neg_node_fanout->fCompl1;
    }
  }

  // step3 miter
  Abc_Ntk_t* miter = Abc_NtkMiter(pNtk, pNtk_neg_node, 1, 0, 0, 0);  
  
  

  // step4 init SAT 
  sat_solver* pSat = sat_solver_new();
  // step5 convert miter to CNF
  // for each node in miter, add clause
  //std::cout<<"add node clause\n";
  int j;
  Abc_NtkForEachNode(miter,pObj,j){
    int iVar = Abc_ObjId(pObj);
    int iVar0 = Abc_ObjId(Abc_ObjFanin0(pObj));
    int iVar1 = Abc_ObjId(Abc_ObjFanin1(pObj));
    bool fCompl0 = Abc_ObjFaninC0(pObj);
    bool fCompl1 = Abc_ObjFaninC1(pObj);
    // create clause1
    lit Lits_1[2];
    // ~A or B
    Lits_1[0] = toLitCond(iVar, 0);
    if(fCompl0){
      Lits_1[1] = toLitCond(iVar0, 0);
    }else{
      Lits_1[1] = toLitCond(iVar0, 1);
    }
    Cid = sat_solver_addclause(pSat, Lits_1, Lits_1 + 2);

    // create clause2
    lit Lits_2[2];
    // ~A or C
    Lits_2[0] = toLitCond(iVar, 0);
    if(fCompl1){
      Lits_2[1] = toLitCond(iVar1, 0);
    }else{
      Lits_2[1] = toLitCond(iVar1, 1);
    }
    Cid = sat_solver_addclause(pSat, Lits_2, Lits_2 + 2);

    // create clause3
    // A or ~B or ~C
    lit Lits_3[3];
    Lits_3[0] = toLitCond(iVar, 1);
    if(fCompl0){
      Lits_3[1] = toLitCond(iVar0, 1);
    }else{
      Lits_3[1] = toLitCond(iVar0, 0);
    }
    if(fCompl1){
      Lits_3[2] = toLitCond(iVar1, 1);
    }else{
      Lits_3[2] = toLitCond(iVar1, 0);
    }
    Cid = sat_solver_addclause(pSat, Lits_3, Lits_3 + 3);
  }
  
  //std::cout<<"add output clause\n";
  // add output
  Abc_NtkForEachPo(miter,pObj,j){
    int iVar = Abc_ObjId(pObj);
    int fanin_num = Abc_ObjFaninNum(pObj);
    if(fanin_num == 1){
      int iVar0 = Abc_ObjId(Abc_ObjFanin0(pObj));
      bool fCompl0 = Abc_ObjFaninC0(pObj);
      // A = B 
      // clause1 ~A or B
      lit Lits_1[2];
      Lits_1[0] = toLitCond(iVar, 0);
      if(fCompl0){        
        Lits_1[1] = toLitCond(iVar0, 0);
      }else{        
        Lits_1[1] = toLitCond(iVar0, 1);
      }
      Cid = sat_solver_addclause(pSat, Lits_1, Lits_1 + 2);
      // clause2 A or ~B
      lit Lits_2[2];
      Lits_2[0] = toLitCond(iVar, 1);
      if(fCompl0){
        Lits_2[1] = toLitCond(iVar0, 1);
      }else{
        Lits_2[1] = toLitCond(iVar0, 0);
      }
      Cid = sat_solver_addclause(pSat, Lits_2, Lits_2 + 2);

    }else{
      // noraml and gate
      int iVar0 = Abc_ObjId(Abc_ObjFanin0(pObj));
      int iVar1 = Abc_ObjId(Abc_ObjFanin1(pObj));
      bool fCompl0 = Abc_ObjFaninC0(pObj);
      bool fCompl1 = Abc_ObjFaninC1(pObj);
      // clause1 ~A or B
      lit Lits_1[2];
      if(fCompl0){
        Lits_1[0] = toLitCond(iVar, 0);
        Lits_1[1] = toLitCond(iVar0, 0);
      }else{
        Lits_1[0] = toLitCond(iVar, 0);
        Lits_1[1] = toLitCond(iVar0, 1);
      }
      Cid = sat_solver_addclause(pSat, Lits_1, Lits_1 + 2);
      // clause2 ~A or C
      lit Lits_2[2];
      if(fCompl1){
        Lits_2[0] = toLitCond(iVar, 0);
        Lits_2[1] = toLitCond(iVar1, 0);
      }else{
        Lits_2[0] = toLitCond(iVar, 0);
        Lits_2[1] = toLitCond(iVar1, 1);
      }
      Cid = sat_solver_addclause(pSat, Lits_2, Lits_2 + 2);
      // clause3 A or ~B or ~C
      lit Lits_3[3];
      Lits_3[0] = toLitCond(iVar, 1);
      if(fCompl0){
        Lits_3[1] = toLitCond(iVar0, 1);
      }else{
        Lits_3[1] = toLitCond(iVar0, 0);
      }
      if(fCompl1){
        Lits_3[2] = toLitCond(iVar1, 1);
      }else{
        Lits_3[2] = toLitCond(iVar1, 0);
      }
      Cid = sat_solver_addclause(pSat, Lits_3, Lits_3 + 3);
    }
  }

  //std::cout<<"add miter output == 1 clause\n";
  // step6 add constraints output = 1
  Abc_NtkForEachPo(miter,pObj,j){
    int iVar = Abc_ObjId(pObj);
    lit Lits[1];
    Lits[0] = toLitCond(iVar, 1);
    Cid = sat_solver_addclause(pSat, Lits, Lits + 1);
    //std::cout << "Cid: " << Cid << std::endl;
  }
  
  //std::cout<<"what's node_id in miter?: "<<std::endl;

  //Abc_FrameReplaceCurrentNetwork( Abc_FrameReadGlobalFrame(), miter );

  // what's node_id in miter?
  // step7 solve sat
  Abc_NtkForEachNode(miter,pObj,j){
    int id = Abc_ObjId(pObj);
    int fanin0_id = Abc_ObjId(Abc_ObjFanin0(pObj));
    int fanin1_id = Abc_ObjId(Abc_ObjFanin1(pObj));
    bool fCompl0 = Abc_ObjFaninC0(pObj);
    bool fCompl1 = Abc_ObjFaninC1(pObj);
    //std::cout << id << " " << fanin0_id << " " << fanin1_id << " " << fCompl0 << " " << fCompl1 << std::endl;
  }
  //std::cout << std::endl;

  Abc_Obj_t* node = Abc_NtkObj(miter, node_id); 
  if(node == NULL){
    //std::cout << "node is NULL" << std::endl;
    return odc_set;
  }
  Abc_Obj_t* node_fanin0 = Abc_ObjFanin0(node);
  Abc_Obj_t* node_fanin1 = Abc_ObjFanin1(node);
  if(node_fanin0 == NULL || node_fanin1 == NULL){
    //std::cout << "one fanin miss" << std::endl;
    return odc_set;
  }
  //std::cout<<"Solver: "<<std::endl;

  int node_fanin0_id = Abc_ObjId(node_fanin0);
  int node_fanin1_id = Abc_ObjId(node_fanin1);  
  int cnt = 4;
  while(cnt--){
    int status = sat_solver_solve(pSat, NULL, NULL, 0, 0, 0, 0);
    if(status == 0){
      //std::cout << "UNSAT" << std::endl;
      break;
    }

    // status = l_True, find a solution --> ODC
    int iVar0_value = sat_solver_var_value(pSat, node_fanin0_id);
    int iVar1_value = sat_solver_var_value(pSat, node_fanin1_id);
    //std::cout << iVar0_value << " " << iVar1_value << std::endl;

    odc_set.insert(2*iVar0_value+iVar1_value);
    
    // add constraint to avoid the same solution
    // (y0 ^ iVar0_value  || y1 ^ iVar1_value)
    // (iVar0_value,iVar1_value) == (0,0) (0,1) (1,0) (1,1)
    // (1,1) --> not y0 or not y1
    // (1,0) --> not y0 or y1
    // (0,1) --> y0 or not y1
    // (0,0) --> y0 or y1
    lit Lits[2];
    if(iVar0_value == 1 && iVar1_value == 1){
      Lits[0] = toLitCond(node_fanin0_id, 0);
      Lits[1] = toLitCond(node_fanin1_id, 0);
      //std::cout<<"not y0 or not y1"<<std::endl;
    }else if(iVar0_value == 1 && iVar1_value == 0){
      Lits[0] = toLitCond(node_fanin0_id, 0);
      Lits[1] = toLitCond(node_fanin1_id, 1);
      //std::cout<<"not y0 or y1"<<std::endl;
    }else if(iVar0_value == 0 && iVar1_value == 1){
      Lits[0] = toLitCond(node_fanin0_id, 1);
      Lits[1] = toLitCond(node_fanin1_id, 0);
      //std::cout<<"y0 or not y1"<<std::endl;
    }else{
      Lits[0] = toLitCond(node_fanin0_id, 1);
      Lits[1] = toLitCond(node_fanin1_id, 1);
      //std::cout<<"y0 or y1"<<std::endl;
    }
    Cid = sat_solver_addclause(pSat, Lits, Lits + 2);
    //std::cout << "Cid: " << Cid << std::endl;        
  }

  return odc_set;  
}

int Lsv_CommandComputeODC(Abc_Frame_t* pAbc, int argc, char** argv){
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);  
  int node_id = std::atoi(argv[1]);

  if(!pNtk){
    Abc_Print(-1, "Empty network.\n");
    return 0; 
  }

  Abc_Obj_t* pObj;  
  pObj = Abc_NtkObj(pNtk, node_id);
  bool isNode = Abc_ObjIsNode(pObj);
  if(!isNode){
    Abc_Print(-1, "Not a node.\n");
    return 0; 
  }

  std::set<int> odcs = Lsv_NtkComputeODC(pNtk,node_id);
  //std::cout << "ODC finished\n";
  std::set<int> sdcs = Lsv_NtkComputeSDC(pNtk,node_id);
  //std::cout << "SDC finished\n";

  for(int sdc : sdcs){
    odcs.erase(sdc);
  }
  if(odcs.empty()){
    std::cout << "no odc\n";
  }else{
    for(int odc : odcs){
      // odc 0:00, 1:01, 2:10, 3:11
      char c0 = (odc & 2) ? '1' : '0';
      char c1 = (odc & 1) ? '1' : '0';
      std::cout << c0 << c1 << " ";
    }
    std::cout << std::endl;
  }
  return 0;
}