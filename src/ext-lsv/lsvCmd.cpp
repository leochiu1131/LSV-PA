#include <iostream>
#include <map>
#include <set>
#include <fstream>
#include <vector>
#include <algorithm>
#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
using namespace std;

// k-feasible Cut Enumeration
static int Lsv_k_feasibleCutEnumeration(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut",Lsv_k_feasibleCutEnumeration, 0);
}

void destroy(Abc_Frame_t* pAbc) {}
Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

void lsv_printcut(Abc_Ntk_t* pNtk, int k) {
  int ithNode;
  Abc_Obj_t* pNode;
  map<string,int> boolean;
  set<int> s; 
  set<int> newset;
  vector<set<set<int>>> allcutvector(Abc_NtkObjNum(pNtk)); // a vector contains Node_numbers sets (Abc_NtkObjNum)
  Abc_NtkForEachObj(pNtk, pNode, ithNode) {
    boolean[Abc_ObjName(pNode)]= ithNode;

    s.insert(ithNode);
    allcutvector[ithNode].insert(s);
    if(s.size() <= k){
      printf("%d:", ithNode);
      for ( auto it : s ){
        printf(" %d", it );
      }
        printf("\n");     
    }
    s.clear();

    if (Abc_ObjFaninNum(pNode) > 1){
      for ( auto &cut0 : allcutvector[Abc_ObjFaninId0(pNode)] ){
        for ( auto &cut1 : allcutvector[Abc_ObjFaninId1(pNode)] ){
          set_union(cut0.begin(), cut0.end(),
                         cut1.begin(), cut1.end(),
                         inserter(newset, newset.end()));
          allcutvector[ithNode].insert(newset);
          if(newset.size() <= k){
            printf("%d:", ithNode);
            for ( auto it : newset ){
              printf(" %d", it );
            }
            printf("\n");     
          }
          newset.clear();               
        }
      }
    }
  }
  //for ( set<set<int> >::iterator it = set1.begin(); set1 != set1.end(); set1 ++ )
/*
  for (int i = 1; i < Abc_NtkObjNum(pNtk); i++) {
    for (set<set<int>>::iterator it = allcutvector[i].begin(); it != allcutvector[i].end(); it++){
      for (set<int>::iterator s = it->begin(); s != it->end(); s++) { 
        //if(s.size() <= k){
          cout << i-1 + 1 << " :";                                                     
          cout << *s << ' ';
        //}
        cout << endl;
      }
    }
  }
*/
/*
void lsv_printcut(Abc_Ntk_t* pNtk, int k) {
  int ithPi;
  std::set<int> cutset;
  std::set<int> cutset0;
  std::set<int> cutset1;
  std::set<int> cutset00;
  std::set<int> cutset01;
  std::set<int> cutset10;
  std::set<int> cutset11;
  std::map<std::string,int> boolean;
  Abc_Obj_t* pPi;
  // Abc_Obj_t* pPo;
 
  Abc_NtkForEachNode(pNtk, pPi, ithPi) {
    // Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo);
    boolean[Abc_ObjName(pPi)]= ithPi;
    Abc_Obj_t* child0_obj = Abc_ObjFanin0(pPi);
    Abc_Obj_t* child00_obj = Abc_ObjFanin0(child0_obj);
    Abc_Obj_t* child01_obj = Abc_ObjFanin1(child0_obj);
    int child0 = Abc_ObjFaninId0(pPi);
    int child00 = Abc_ObjFaninId0(child0_obj);
    int child01 = Abc_ObjFaninId1(child0_obj);
    Abc_Obj_t* child1_obj = Abc_ObjFanin1(pPi);
    Abc_Obj_t* child10_obj = Abc_ObjFanin0(child1_obj);
    Abc_Obj_t* child11_obj = Abc_ObjFanin1(child1_obj);
    int child1 = Abc_ObjFaninId1(pPi);
    int child10 = Abc_ObjFaninId0(child1_obj);
    int child11 = Abc_ObjFaninId1(child1_obj);
    printf("%d: %d\n", ithPi, ithPi);
    printf("%d: %d %d\n", ithPi, child0, child1);
    
    if (!Abc_ObjIsPi(child0_obj)){
      cutset0.insert(Abc_ObjFaninId0(child0_obj));
      cutset0.insert(Abc_ObjFaninId1(child0_obj));
      cutset0.insert(child1);
      //printf("%d: %d %d %d\n", ithPi, Abc_ObjFaninId0(child0_obj), Abc_ObjFaninId1(child0_obj), child1);
      if(cutset0.size() <= k){
        printf("%d:", ithPi);
        for ( auto it : cutset0 ){
          printf(" %d", it );
        }
        printf("\n");   
      }
      cutset0.clear();
    }
    if (!Abc_ObjIsPi(child1_obj)){
      cutset1.insert(child0);
      cutset1.insert(Abc_ObjFaninId0(child1_obj));
      cutset1.insert(Abc_ObjFaninId1(child1_obj));
      //printf("%d: %d %d %d\n", ithPi, child0, Abc_ObjFaninId0(child1_obj), Abc_ObjFaninId1(child1_obj));
      if(cutset1.size() <= k){
        printf("%d:", ithPi);
        for ( auto it : cutset1 ){
          printf(" %d", it );
        }
        printf("\n");     
      }
      cutset1.clear();
    }
    //std::cout<<"out"<<std::endl;
    if (!Abc_ObjIsPi(child0_obj) & !Abc_ObjIsPi(child1_obj)){
      cutset.insert(Abc_ObjFaninId0(child0_obj));
      cutset.insert(Abc_ObjFaninId1(child0_obj));
      cutset.insert(Abc_ObjFaninId0(child1_obj));
      cutset.insert(Abc_ObjFaninId1(child1_obj));
      //printf("%d: %d %d %d\n", ithPi, Abc_ObjFaninId0(child0_obj), Abc_ObjFaninId1(child0_obj), Abc_ObjFaninId0(child1_obj), Abc_ObjFaninId1(child1_obj));
      if(cutset.size() <= k){
        printf("%d:", ithPi);
        for ( auto it : cutset ){
          printf(" %d", it );
        }
        printf("\n");     
      }
      cutset.clear();
    }
    
  }
  */
}   

int Lsv_k_feasibleCutEnumeration(Abc_Frame_t* pAbc, int argc, char** argv) {
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
  lsv_printcut(pNtk, std::stoi(argv[1]));
  return 0;
usage:
  Abc_Print(-2, "usage: lsv_print_nodes [-h]\n");
  Abc_Print(-2, "\t        prints the nodes in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}
