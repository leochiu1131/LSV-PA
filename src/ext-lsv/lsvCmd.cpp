#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <set>

using namespace std;

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_CommandPrintCut, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;



class Cut {
  public:
    vector<int> leaves;
    
    Cut() {}
    Cut(int leaf) : leaves{leaf} {}

    bool operator<(const Cut& other) const {
      return leaves < other.leaves;
    }
};



void print_cut(const Cut &cut) {
  for(int leaf : cut.leaves) cout << leaf << " ";

  cout << endl;
}

Cut merge(const Cut &cut1, const Cut &cut2, int k) {
  Cut result;

  set_union(cut1.leaves.begin(), cut1.leaves.end(), cut2.leaves.begin(), cut2.leaves.end(), back_inserter(result.leaves));

  if(result.leaves.size() > k) result.leaves.resize(k);

  return result;
}

vector<Cut> enumerate_cuts(Abc_Obj_t* pObj, int k) {
  vector<Cut> cuts;
  
  if(Abc_ObjIsCi(pObj)){
    cuts.push_back(Abc_ObjId(pObj));

    return cuts;
  }

  Abc_Obj_t *pFanin0 = Abc_ObjFanin0(pObj);
  Abc_Obj_t *pFanin1 = Abc_ObjFanin1(pObj);

  vector<Cut> cuts0 = enumerate_cuts(pFanin0, k);
  vector<Cut> cuts1 = enumerate_cuts(pFanin1, k);

  for (const auto& cut0 : cuts0) {
    for (const auto& cut1 : cuts1) {
        Cut newCut = merge(cut0, cut1, k);
        
        if (newCut.leaves.size() <= k) cuts.push_back(newCut);
    }
  }

  cuts.emplace_back(Abc_ObjId(pObj));

  sort(cuts.begin(), cuts.end());
  cuts.erase(unique(cuts.begin(), cuts.end()), cuts.end());

  return cuts;
}



void Lsv_NtkPrintCut(Abc_Ntk_t* pNtk, int k) {
  Abc_Obj_t *pObj;
  int i;
  
  Abc_NtkForEachNode(pNtk, pObj, i) {
    vector<Cut> cuts = enumerate_cuts(pObj, k);
    for(const auto& cut : cuts) {
      cout << Abc_ObjId(pObj) << ": ";
      print_cut(cut);
    }
  }
}

int Lsv_CommandPrintCut(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c, k = 0;

  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
    switch (c) {
      case 'h':
        goto usage;
      default:
        goto usage;
    }
  }

  if(argc != 2) goto usage;

  k = atoi(argv[1]);
  if(k < 3 || k > 6) {
    Abc_Print(-1, "Invalid k value. Must be between 3 and 6.\n");
    return 1;
  }

  if(!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }

  if(!Abc_NtkIsStrash(pNtk)) {
    Abc_Print(-1, "Network not in AIG. Run 'strash' first.\n");
    return 1;
  }

  Lsv_NtkPrintCut(pNtk, k);
  return 0;

usage:
    Abc_Print(-2, "usage: lsv_printcut <k>\n");
    Abc_Print(-2, "\t        enumerates all k-feasible cuts for 3 <= k <= 6\n");
    Abc_Print(-2, "\t-h    : print the command usage\n");
    return 1;
}

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