#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <vector>
#include <set>
#include <algorithm>

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
  Abc_Obj_t* pObj;
  std::vector<std::vector<std::set<int>>> CutList(Abc_NtkObjNumMax(pNtk));
  int i;

  CutList.resize(Abc_NtkObjNumMax(pNtk));

  Abc_NtkForEachPi(pNtk, pObj, i) {
    std::set<int> trivial;
    int ObjId = Abc_ObjId(pObj);
    trivial.insert(ObjId);
    CutList[ObjId].push_back(trivial);
  }

  Abc_NtkForEachNode(pNtk, pObj, i) {
    
    std::set<int> trivial;
    int ObjId = Abc_ObjId(pObj);
    trivial.insert(ObjId);
    CutList[ObjId].push_back(trivial);
    int FaninId0 = Abc_ObjFaninId0(pObj), FaninId1 = Abc_ObjFaninId1(pObj);
    for (int m = 0; m < CutList[FaninId0].size(); m++) {
      for (int n = 0; n < CutList[FaninId1].size(); n++){
        std::set<int> New_Cut;
        std::merge(CutList[FaninId0][m].begin(), CutList[FaninId0][m].end(), 
                    CutList[FaninId1][n].begin(), CutList[FaninId1][n].end(), std::inserter(New_Cut, New_Cut.begin()));
        if(New_Cut.size() <= k) 
          CutList[ObjId].push_back(New_Cut);
      }
      
    }
  }
  Abc_NtkForEachPo(pNtk, pObj, i) {
   
    std::set<int> trivial;
    int ObjId = Abc_ObjId(pObj);
    trivial.insert(ObjId);
    CutList[ObjId].push_back(trivial);
    int FaninId0 = Abc_ObjFaninId0(pObj);
    for (int m = 0; m < CutList[FaninId0].size(); m++) {
      std::set<int> New_Cut;
      New_Cut = CutList[FaninId0][m];
      if(New_Cut.size() <= k) 
        CutList[ObjId].push_back(New_Cut);
    
    }
  }
  for(int m = 0; m < Abc_NtkObjNumMax(pNtk); m++){
    for (int n = 0; n < CutList[m].size(); n++){
      for (int p = 0; p < CutList[m].size(); p++){
        if (n != p){
          if(std::includes(CutList[m][n].begin(), CutList[m][n].end(), CutList[m][p].begin(), CutList[m][p].end())){
              CutList[m].erase(CutList[m].begin()+n);
              n--;
              break;
          }
        }
      }            
    }
  }

  for(int m = 0; m < Abc_NtkObjNumMax(pNtk); m++){
    for (int n = 0; n < CutList[m].size(); n++){
        printf("%d: ", m);
        for (auto it = CutList[m][n].begin(); it != CutList[m][n].end(); ++it) {
          printf("%d ", *it);
        }
        printf("\n");     
    }
  }
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
    if ( argc != globalUtilOptind + 1 )
  {
      Abc_Print( -1, "Wrong number of auguments.\n" );
      goto usage;
  }
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  Lsv_NtkPrintCuts(pNtk, atoi(argv[1]));
  
  return 0;
  
usage:
  Abc_Print(-2, "usage: lsv_printcut [-h] <k>\n");
  Abc_Print(-2, "\t        prints the k-feasible cut enumeration of every node on an AIG\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}