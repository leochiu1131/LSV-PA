#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <vector>
#include <set>
#include <iostream>
#include <ostream>
#include <algorithm>
#include <iterator>


static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);

static int Lsv_PrintCut(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_PrintCut, 0);
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

std::set<int> setUnion(const std::set<int> s1, const std::set<int> s2){
  std::set<int> result = s1;
  result.insert(s2.begin(), s2.end());
  return result;
}

void dominateCheck(std::pair<std::set<int>, bool>& s1, std::pair<std::set<int>, bool>& s2){
  std::set<int> diff1to2, diff2to1;
  std::set_difference(s1.first.begin(), s1.first.end(), s2.first.begin(), s2.first.end(), std::inserter(diff1to2, diff1to2.begin()));
  std::set_difference(s2.first.begin(), s2.first.end(), s1.first.begin(), s1.first.end(), std::inserter(diff2to1, diff2to1.begin()));
  // S1 dominates S2
  if(diff1to2.size()==0 && diff2to1.size()>0){
    s2.second = false;
  }
  // S2 dominates S1
  else if(diff1to2.size()>0 && diff2to1.size()==0){
    s1.second = false;
  }
}

int Lsv_PrintCut(Abc_Frame_t* pAbc, int argc, char** argv){
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

  // PI //
  if(argc==2){
    int k = argv[1][0]-'0';
    Abc_Obj_t *pObj;
    int i;
    std::vector<std::vector<std::set<int>>> eachNodeSet;
    eachNodeSet.resize(2*(Abc_NtkPiNum(pNtk)+Abc_NtkObjNum(pNtk)+Abc_NtkPoNum(pNtk)));

    Abc_NtkForEachPi(pNtk, pObj, i){
      // PIs
      if(Abc_ObjIsPi(pObj)){
        std::vector<std::set<int>> mySet;
        std::set<int> subset = {Abc_ObjId(pObj)};
        mySet.push_back(subset);
        eachNodeSet.at(Abc_ObjId(pObj)) = mySet;
        if(1<=k){
          printf("%d: %d\n", Abc_ObjId(pObj), Abc_ObjId(pObj));
        }
      }
    }

    // AND NODE //
    Abc_NtkForEachNode(pNtk, pObj, i){
      std::vector<std::pair<std::set<int>, bool>> naiveSet;
      std::pair<std::set<int>, bool> tmpPair;
      tmpPair.first = {Abc_ObjId(pObj)};
      tmpPair.second = true;
      naiveSet.push_back(tmpPair);
      // std::cout<<"NODE"<<pObj->Id<<" has "<<Abc_ObjFaninNum(pObj)<<" fanins."<<std::endl;
      Abc_Obj_t *fanin0 = Abc_ObjFanin0(pObj); 
      Abc_Obj_t *fanin1 = Abc_ObjFanin1(pObj);
      std::vector<std::set<int>> f0Set = eachNodeSet.at(fanin0->Id);
      std::vector<std::set<int>> f1Set = eachNodeSet.at(fanin1->Id);

      // First, union two fanins' sets without checking dominance.
      for(const auto& s1: f0Set){
        for(const auto& s2: f1Set){
          std::set<int> unionS = setUnion(s1, s2);
          // Check whether the set has been in naiveSet.
          if(unionS.size()<=k){
            bool rep = false;
            for(const auto& nS: naiveSet){
              if(unionS==nS.first){
                rep = true;
                break;
              }
            }
            if(!rep){
              tmpPair.first = unionS;
              tmpPair.second = true;
              naiveSet.push_back(tmpPair);
            }
          }
        }
      }

      // Domainance checking
      for(size_t i=0; i<naiveSet.size(); ++i){
        if(naiveSet[i].second==true){
          for(size_t j=i; j<naiveSet.size(); ++j){
            if(naiveSet[j].second==true){
              dominateCheck(naiveSet[i], naiveSet[j]);
            }
          }
        }
      }

      // Result
      for(const auto& s: naiveSet){
        if(s.second==true){
          eachNodeSet.at(Abc_ObjId(pObj)).push_back(s.first);
        }
      }

      // Print
      for(const auto& s: eachNodeSet.at(Abc_ObjId(pObj))){
          printf("%d:", Abc_ObjId(pObj));
          for(const auto& mem: s){
            printf(" %d", mem);
          }
          printf("\n");
      }
    }

    // PO //
    // Abc_NtkForEachPo(pNtk, pObj, i){
    //   // std::cout<<"----------------------------------------"<<std::endl;
    //   // std::cout<<"NOW is NODE"<<Abc_ObjId(pObj)<<std::endl;
    //   std::vector<std::set<int>> mySet;
    //   mySet.push_back({Abc_ObjId(pObj)});
    //   // std::cout<<"NODE"<<Abc_ObjId(pObj)<<" has "<<Abc_ObjFaninNum(pObj)<<" fanins."<<std::endl;
    //   Abc_Obj_t *fanin0 = Abc_ObjFanin0(pObj); 
    //   // std::cout<<"NODE"<<Abc_ObjId(pObj)<<" 's fanin is NODE"<<Abc_ObjId(Abc_ObjFanin0(pObj))<<std::endl;
    //   std::vector<std::set<int>> f0Set = eachNodeSet.at(Abc_ObjId(fanin0));
    //   // std::cout<<"fanin0->Id"<<Abc_ObjId(fanin0)<<std::endl;

    //   if(1<=k){
    //     printf("%d: %d\n", Abc_ObjId(pObj), Abc_ObjId(pObj));
    //   }
    //   for(const auto& s: f0Set){
    //     if(s.size()<=k){
    //       printf("%d:", Abc_ObjId(pObj));
    //       for(const auto& mem: s){
    //         printf(" %d", mem);
    //       }
    //       printf("\n");
    //     }
    //   }
    // }
  }
  else{
    printf("Please give a number k.\n");
  }

  return 0;

usage:
  Abc_Print(-2, "usage: lsv_printcut [-h]\n");
  Abc_Print(-2, "\tk       prints k-feasible cuts in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}