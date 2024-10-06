#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "misc/util/abc_global.h"
#include "misc/vec/vec.h"
#include "opt/cut/cut.h"
#include <vector>
#include <iostream>
#include <set>


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

//print node
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

//kcut

void PrintKFeasibleCuts(Abc_Ntk_t* pNtk, int k) {
    std::vector<std::vector<std::set<int>>> vecOfVecOfSets;
    int piCount = 0;
    int pocount = 0;
    piCount = Abc_NtkPiNum(pNtk);
    pocount = Abc_NtkPoNum(pNtk);
    for(int i = 0; i < piCount; i++){
      std::vector<std::set<int>> vecOfSets;
      std::set<int> mySet;
      mySet.insert(i+1);
      vecOfSets.push_back(mySet);
      vecOfVecOfSets.push_back(vecOfSets);
    }
    //printf("size of vecOfVecOfSets : %d\n", vecOfVecOfSets.size());
    Abc_Obj_t* pObj;
    int i;
    //printf("call success\n");
    Abc_NtkForEachNode(pNtk, pObj, i) {
      std::vector<std::set<int>> vecOfSets;
      int i = Abc_ObjId(Abc_ObjFanin0(pObj))-1;
      int j = Abc_ObjId(Abc_ObjFanin1(pObj))-1; 
      if(Abc_ObjIsPi(Abc_ObjFanin0(pObj)) == 0){
        i = i -pocount;
      }
      if(Abc_ObjIsPi(Abc_ObjFanin1(pObj)) == 0){
        j = j -pocount;
      }
      std::set<int> mySet;
      mySet.insert(Abc_ObjId(pObj));
      vecOfSets.push_back(mySet);
      //printf("set success value of i : %d , value of j : %d\n",i ,j );
      //printf("size of vecOfVecOfSets : %d\n", vecOfVecOfSets.size());    
      for(int a = 0; a < vecOfVecOfSets[i].size(); a++){
        for(int b = 0; b < vecOfVecOfSets[j].size(); b++){
              std::set<int> s(vecOfVecOfSets[i][a]);
              s.insert(vecOfVecOfSets[j][b].begin(), vecOfVecOfSets[j][b].end());
              //printf("merge success\n");
              vecOfSets.push_back(s);
        }
      }
      vecOfVecOfSets.push_back(vecOfSets);
      //printf("push success\n");
    }
    for(int i = 0; i < Abc_NtkPoNum(pNtk); i++){
      std::vector<std::set<int>> vecOfSets;
      std::set<int> mySet;
      mySet.insert(Abc_ObjId(Abc_NtkPo(pNtk, i)));
      vecOfSets.push_back(mySet);
      for(int k = 0; k < vecOfVecOfSets[Abc_ObjId(Abc_ObjFanin0(Abc_NtkPo(pNtk, i)))-piCount-1].size(); k++)
        vecOfSets.push_back(vecOfVecOfSets[Abc_ObjId(Abc_ObjFanin0(Abc_NtkPo(pNtk, i)))-piCount-1][k]);
      vecOfVecOfSets.insert(vecOfVecOfSets.begin() + piCount +i , vecOfSets);
    }
    
    // Abc_NtkForEachNode(pNtk, pObj, i) {
    //   if(Abc_ObjISPO(pObj) == 1){
    //     std::vector<std::set<int>> vecOfSets;
    //     Abc_Obj_t* pFanin;
    //     int j;
    //     Abc_ObjForEachFanin(pObj, pFanin, j) {
    //       std::set<int> mySet

    //   }
    // }
    for(int i = 0; i < vecOfVecOfSets.size(); i++){
      for(int j = 0; j < vecOfVecOfSets[i].size(); j++){
        if(vecOfVecOfSets[i][j].size() <= k){
          printf("Node %d : ", i+1);
          for(auto const &e: vecOfVecOfSets[i][j]){
            printf("%d ", e);
          }
          printf("\n");
        }
      }
    }
}

int Lsv_CommandPrintCut(Abc_Frame_t* pAbc, int argc, char** argv) {
    int k = 4; // Default value for k
    if (argc == 2) {
        k = atoi(argv[1]);
        if (k < 3 || k > 6) {
            printf("Error: k must be between 3 and 6.\n");
            return 1;
        }
    }
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    if (pNtk == NULL) {
        printf("Error: There is no current network.\n");
        return 1;
    }
    PrintKFeasibleCuts(pNtk, k);
    return 0;
}