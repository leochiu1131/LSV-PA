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
//static int Lsv_k_feasibleCutEnumeration(Abc_Frame_t* pAbc, int argc, char** argv);
// Satisfiability Don’t Cares Computation
static int Satisfiability_Dont_Cares_Computation(Abc_Frame_t* pAbc, int argc, char** argv);
// Observability Don’t Cares Computation
static int Observability_Dont_Cares_Computation(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  //Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut",Lsv_k_feasibleCutEnumeration, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sdc",Satisfiability_Dont_Cares_Computation, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_odc",Observability_Dont_Cares_Computation, 0);
}

void destroy(Abc_Frame_t* pAbc) {}
Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

void lsv_sdc(Abc_Ntk_t* pNtk, int k) {
  printf("no sdc");
  printf("\n");
}

void lsv_odc(Abc_Ntk_t* pNtk, int k) {
  printf("no odc");
  printf("\n");
}

/*
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
}   
*/
/*
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
*/
int Satisfiability_Dont_Cares_Computation(Abc_Frame_t* pAbc, int argc, char** argv) {
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
  lsv_sdc(pNtk, std::stoi(argv[1]));
  return 0;
usage:
  Abc_Print(-2, "usage: lsv_print_nodes [-h]\n");
  Abc_Print(-2, "\t        prints the nodes in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

int Observability_Dont_Cares_Computation(Abc_Frame_t* pAbc, int argc, char** argv) {
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
  lsv_odc(pNtk, std::stoi(argv[1]));
  return 0;
usage:
  Abc_Print(-2, "usage: lsv_print_nodes [-h]\n");
  Abc_Print(-2, "\t        prints the nodes in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}
