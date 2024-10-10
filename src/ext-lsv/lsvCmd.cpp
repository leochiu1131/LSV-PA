#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <map>
#include <set>
#include <algorithm>

void findcuts(Abc_Obj_t* pObj, std::map<int, std::set<std::set<int>>> &cutMap, int k, bool isPi, bool isPo);
int dominate(std::set<int> a, std::set<int> b);
static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintCuts(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_CommandPrintCuts, 0);
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


void Lsv_NtkPrintCuts(Abc_Ntk_t* pNtk, int k) {
  // printf("k = %i\n", k);
  Abc_Obj_t* pObj;
  int i;
  std::map<int, std::set<std::set<int>>> cutMap;
  Abc_NtkForEachPi(pNtk, pObj, i) {
    // printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
    findcuts(pObj, cutMap, k, true, false);
  }
  Abc_NtkForEachNode(pNtk, pObj, i) {
    // printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
    findcuts(pObj, cutMap, k, false, false);
    
  }
  // Abc_NtkForEachPo(pNtk, pObj, i) {
  //   // printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
  //   findcuts(pObj, cutMap, k, false, true);
  // }
}

int Lsv_CommandPrintCuts(Abc_Frame_t* pAbc, int argc, char** argv) {
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
  if (argc!=2) {
    goto usage;
  }
  Lsv_NtkPrintCuts(pNtk, std::atoi(argv[1]));
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_print_nodes [-h]\n");
  Abc_Print(-2, "\t        prints the nodes in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

void findcuts(Abc_Obj_t* pObj, std::map<int, std::set<std::set<int>>> &cutMap, int k, bool isPi, bool isPo){
    std::set<std::set<int>> cuts;    

    if(isPo){
      int Fanin0 = Abc_ObjId(Abc_ObjFanin0(pObj));
      cuts = cutMap[Fanin0];
    }

    //itself
    std::set<int> self;
    self.insert(Abc_ObjId(pObj));
    cuts.insert(self);


    //cross product
    if(!isPi&&!isPo){
      int Fanin0 = Abc_ObjId(Abc_ObjFanin0(pObj));
      int Fanin1 = Abc_ObjId(Abc_ObjFanin1(pObj));
      if(cutMap.find(Fanin0)==cutMap.end()){
        printf("cut of %d not found \n", Fanin0); 
      }
      if(cutMap.find(Fanin1)==cutMap.end()){
        printf("cut of %d not found \n", Fanin1);
      }
      for (const auto &s : cutMap[Fanin0]) {      
        for (const auto &t : cutMap[Fanin1]){
          std::set<int> cut = s;
          for (const auto &r : t){
            cut.insert(r);
          } 
          if(cut.size()>k) continue;


          // check dominate;
          bool dom = false;
          for (const auto &r : cuts)
          {
            switch (dominate(r, cut))
            {
            case 0:
              continue;
              break;
            case 1:
              dom = true;
              break;
            case 2:
              cuts.erase(r);
              break;
            case 3:
              dom = true;
              break;
            default:
              break;
            }
          }
          if(!dom) cuts.insert(cut);
        }      
      }
    }
    //print cuts

    for (const auto &s : cuts) {
      if(s.size()>k) continue;
      printf("%d: ", Abc_ObjId(pObj));
      for (const auto &t : s)
        printf("%d ", t);
      printf("\n");
    }
    cutMap[Abc_ObjId(pObj)] = cuts;
}

int dominate(std::set<int> a, std::set<int> b){
  if (a == b)
  {
    return 1;
  }
  else if(std::includes(a.begin(), a.end(), b.begin(), b.end()))
  {
    return 2;
  }
  else if (std::includes(b.begin(), b.end(), a.begin(), a.end()))
  {
    return 3;
  }
  
  return 0;
  
}