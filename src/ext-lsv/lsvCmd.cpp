#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "opt/cut/cut.h"
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <iostream>

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


void Lsv_NtkPrintCut(Abc_Ntk_t* pNtk, int k) {
  Abc_Obj_t* pObj;
  std::vector<int> now_cuts;
  int i;

  std::unordered_map<int, std::vector<std::vector<int>>> Node_Cuts;

  assert( Abc_NtkIsStrash(pNtk) );

  Abc_NtkForEachObj(pNtk, pObj, i) {
    
    int oID = Abc_ObjId(pObj);
    int cnum = 1;


    if ( Abc_ObjIsCo(pObj) || Abc_ObjIsNone(pObj)){
      continue;
    }

    // save the node itself ( 1-cut )
    now_cuts.push_back(oID);
    Node_Cuts[oID].push_back(now_cuts);
    now_cuts.clear();

    // PIs only have 1-cut
    if ( Abc_ObjFaninNum(pObj)==0 ){
      std::cout << oID << ": " << oID << std::endl;
      continue;
    } else if ( Abc_ObjFaninNum(pObj)==1 ){
      std::cout << oID << ": " << oID << std::endl;
      int child_ID = Abc_ObjFaninId0(pObj);
      for (int j = 0; j < Node_Cuts[child_ID].size(); j++){
        now_cuts.assign(Node_Cuts[child_ID][j].begin(), Node_Cuts[child_ID][j].end());
        Node_Cuts[oID].push_back(now_cuts);
        now_cuts.clear();
        std::cout << oID << ": ";
        for (auto& ele : Node_Cuts[child_ID][j]) {
            std::cout << ele << " ";
        }
        std::cout << "\n";
      }
      continue;
    }

    int lchild_ID = Abc_ObjFaninId0(pObj);
    int rchild_ID = Abc_ObjFaninId1(pObj);
    int lchild_cnum = Node_Cuts[lchild_ID].size();
    int rchild_cnum = Node_Cuts[rchild_ID].size();

    // merge cuts of two childs
    for (int j = 0; j < lchild_cnum; j++){
      now_cuts.clear();
      if( Node_Cuts[lchild_ID][j].size() >= k)
        continue; // too large

      for (int l = 0; l < rchild_cnum; l++){
        now_cuts.clear();
        if( Node_Cuts[rchild_ID][l].size() >= k)
          continue; // too large      

        now_cuts.assign(Node_Cuts[lchild_ID][j].begin(), Node_Cuts[lchild_ID][j].end());
        now_cuts.insert( now_cuts.end(), Node_Cuts[rchild_ID][l].begin(), Node_Cuts[rchild_ID][l].end() );
        // sort the cut in assending order and remove duplicated nodes
        std::sort( now_cuts.begin(), now_cuts.end() );
        now_cuts.erase( std::unique( now_cuts.begin(), now_cuts.end() ), now_cuts.end() );
        if( now_cuts.size() > k)
          continue; // too large         
         
        Node_Cuts[oID].push_back(now_cuts);
        cnum++;
      }
    }

    now_cuts.clear();
    
    // sort cuts with number of cuts
    std::sort(Node_Cuts[oID].begin(), Node_Cuts[oID].end(),
          [](const std::vector<int>& a, const std::vector<int>& b) {
      return a.size() < b.size();
    });

    for (int j = 0; j < cnum; j++){
      std::cout << oID << ": ";
      for (auto& ele : Node_Cuts[oID][j]) {
          std::cout << ele << " ";
      }
      std::cout << "\n";
    }
  }
}


int Lsv_CommandPrintCut(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c, k;

  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
    switch (c) {
      case 'h':
        goto usage;
      default:
        goto usage;
    }
  }
  if ( globalUtilOptind >= argc )
  {
      Abc_Print( -1, "Command line lsv_printcut should be followed by an integer.\n" );
      goto usage;
  }
  k = atoi(argv[globalUtilOptind]);

  if (k < 3 || k > 6) {
    Abc_Print(-1, "Cut size k must be between 3 and 6.\n");
    return 1;
  }

  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  Lsv_NtkPrintCut(pNtk, k);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_printcut [-h] <cut>\n");
  Abc_Print(-2, "\t        prints <cut> cut of the network\n");
  Abc_Print(-2, "\t<cut> : the number of cuts we want\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}
