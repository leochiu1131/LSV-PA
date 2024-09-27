#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <vector>
#include <set>
#include <iostream>
#include <ostream>


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

  // PA1 
  if(argc==2){
    int k = argv[1][0]-'0';
    Abc_Obj_t *pObj;
    int i;
    std::vector<std::vector<std::set<int>>> eachNodeSet;
    eachNodeSet.resize(Abc_NtkPiNum(pNtk)+Abc_NtkObjNum(pNtk));
    std::cout<<"eachNodeSet.size() = "<<eachNodeSet.size()<<std::endl;

    Abc_NtkForEachPi(pNtk, pObj, i){
      // PIs
      if(Abc_ObjIsPi(pObj)){
        std::vector<std::set<int>> mySet;
        std::set<int> subset = {pObj->Id};
        mySet.push_back(subset);
        eachNodeSet.at(pObj->Id-1) = mySet;
        if(1<=k){
          printf("%d: %d\n", pObj->Id, pObj->Id);
        }
      }
    }

    // TODO: 還沒修好
    Abc_NtkForEachNode(pNtk, pObj, i){
      std::vector<std::set<int>> mySet;
      mySet.push_back({pObj->Id});
      Abc_Obj_t *fanin0 = Abc_ObjFanin0(pObj); 
      Abc_Obj_t *fanin1 = Abc_ObjFanin1(pObj);
      std::vector<std::set<int>> f0Set = eachNodeSet.at(fanin0->Id-1);
      std::vector<std::set<int>> f1Set = eachNodeSet.at(fanin1->Id-1);

      for(const auto& s1: f0Set){
        for(const auto& s2: f1Set){
          std::set<int> unionS = setUnion(s1, s2);
          if(unionS.size()<=k){
            // Check whether the set has been in mySet.
            bool rep = false;
            for(const auto& my: mySet){
              if(unionS==my){
                rep = true;
                break;
              }
            }
            if(!rep){
              mySet.push_back(unionS);
            }
          }
        }
      }

      eachNodeSet.at(pObj->Id-1) = mySet;
      for(const auto& s: mySet){
          printf("%d:", pObj->Id);
          for(const auto& mem: s){
            printf(" %d", mem);
          }
          printf("\n");
      }
    }

    Abc_NtkForEachPo(pNtk, pObj, i){
      std::vector<std::set<int>> mySet;
      mySet.push_back({pObj->Id});
      Abc_Obj_t *fanin0 = Abc_ObjFanin0(pObj); 
      std::vector<std::set<int>> f0Set = eachNodeSet.at(fanin0->Id-1);

      if(1<=k){
        printf("%d: %d\n", pObj->Id, pObj->Id);
      }
      for(const auto& s: f0Set){
        if(s.size()<=k){
          printf("%d:", pObj->Id);
          for(const auto& mem: s){
            printf(" %d", mem);
          }
          printf("\n");
        }
      }
    }

    // Abc_NtkForEachObj(pNtk, pObj, i){
    //   std::cout<<i<<std::endl;
    //   if(Abc_ObjIsNone(pObj)){
    //     continue;
    //   }
    //   // PIs
    //   if(Abc_ObjIsPi(pObj)){
    //     std::cout<<"PI"<<std::endl;
    //     std::vector<std::set<int>> mySet;
    //     std::set<int> subset = {pObj->Id};
    //     mySet.push_back(subset);
    //     eachNodeSet.push_back(mySet);
    //     printf("%d: %d\n", pObj->Id, pObj->Id);
    //   }
    //   // Not PIs
    //   else{
    //     std::cout<<"NOT PI"<<std::endl;
    //     std::vector<std::set<int>> mySet;
    //     mySet.push_back({pObj->Id});

    //     Abc_Obj_t *fanin0 = Abc_ObjFanin0(pObj); 
    //     Abc_Obj_t *fanin1 = Abc_ObjFanin1(pObj);
    //     std::vector<std::set<int>> f0Set = eachNodeSet.at(fanin0->Id-1);
    //     std::vector<std::set<int>> f1Set = eachNodeSet.at(fanin1->Id-1);

    //     for(const auto& s1: f0Set){
    //       for(const auto& s2: f1Set){
    //         std::set<int> unionS = setUnion(s1, s2);
    //         mySet.push_back(unionS);
    //       }
    //     }

    //     eachNodeSet.push_back(mySet);
    //     for(const auto& s: mySet){
    //       if(s.size()<=k){
    //         printf("%d :", pObj->Id);
    //         for(const auto& mem: s){
    //           printf(" %d", mem);
    //         }
    //         printf("\n");
    //       }
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