#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <vector>
#include <set>
#include <algorithm>
#include <iterator>

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintCuts(Abc_Frame_t* pAbc, int argc, char** argv);
static void Lsv_CommandPrintNodes_Usage();
static void Lsv_CommandPrintCuts_Usage();

//-------------------------------------------------------------------------

void LSV_init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_CommandPrintCuts, 0);
}

void LSV_destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {LSV_init, LSV_destroy};

//-------------------------------------------------------------------------

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

//-------------------------------------------------------------------------

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
        Lsv_CommandPrintNodes_Usage();
        return 1;
      default:
        Lsv_CommandPrintNodes_Usage();
        return 1;
    }
  }
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  Lsv_NtkPrintNodes(pNtk);
  return 0;
}

void Lsv_CommandPrintNodes_Usage() {
  Abc_Print(-2, "usage: lsv_print_nodes [-h]\n");
  Abc_Print(-2, "\t        prints the nodes in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return;
}

//-------------------------------------------------------------------------

struct id_cuts_pair {
  int id;
  std::vector<std::set<Abc_Obj_t*>> cuts;
};

std::vector<std::set<Abc_Obj_t*>> ComputeKFeasibleCuts(std::vector<id_cuts_pair>& all_cuts, Abc_Obj_t* pObj, int k) {
  id_cuts_pair id_cuts;
  id_cuts.id = Abc_ObjId(pObj);
  std::vector<std::set<Abc_Obj_t*>> cuts;
  std::set<Abc_Obj_t*> cut;
  for (const auto& id_cut : all_cuts) {
    if (id_cut.id == Abc_ObjId(pObj)) {
      return id_cut.cuts;
    }
  }
  if (Abc_ObjIsPi(pObj)) {
    cut.insert(pObj);
    cuts.push_back(cut);
    id_cuts.cuts = cuts;
    all_cuts.push_back(id_cuts);
    return cuts;
  }

  Abc_Obj_t* pFanin;
  int i;
  std::vector<std::set<Abc_Obj_t*>> cuts1;
  std::vector<std::set<Abc_Obj_t*>> cuts2;
  Abc_ObjForEachFanin(pObj, pFanin, i) {
    if (i == 0) {
      cuts1 = ComputeKFeasibleCuts(all_cuts, pFanin, k);
      }
    if (i == 1) {
      cuts2 = ComputeKFeasibleCuts(all_cuts, pFanin, k);
      }
  }
  for (const auto& cut1 : cuts1) {
    for (const auto& cut2 : cuts2) {
      if (cut1.size() + cut2.size() <= k) {
        cut.insert(cut1.begin(), cut1.end());
        cut.insert(cut2.begin(), cut2.end());
        cuts.push_back(cut);
        cut.clear();
      }
    }
  }
  cut.insert(pObj);
  cuts.push_back(cut);
  id_cuts.cuts = cuts;
  all_cuts.push_back(id_cuts);
  return cuts;
}

int Lsv_CommandPrintCuts(Abc_Frame_t* pAbc, int argc, char** argv) {
  int c;
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
    switch (c) {
      case 'h':
        Lsv_CommandPrintCuts_Usage();
        return 1;
      default:
        Lsv_CommandPrintCuts_Usage();
        return 1;
    }
  }

  if (argc != 2) {
    Lsv_CommandPrintCuts_Usage();
    return 1;
  }
  int k = atoi(argv[1]);
  if (k < 3 || k > 6) {
    Lsv_CommandPrintCuts_Usage();
    return 1;
  }

  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }

  std::vector<std::set<Abc_Obj_t*>> cuts;
  std::vector<id_cuts_pair> all_cuts;  // all_cuts[i] is the cuts of node i
  all_cuts.reserve(Abc_NtkObjNumMax(pNtk));
  Abc_Obj_t* pObj;
  int i;
  Abc_NtkForEachObj(pNtk, pObj, i) {
    cuts = ComputeKFeasibleCuts(all_cuts, pObj, k);
    for (const auto& cut : cuts) {
      printf("%d:", Abc_ObjId(pObj));
        for (auto& c1 : cut) {
          printf(" %d", Abc_ObjId(c1));
        }
      printf("\n");
    }
  }

  return 0;
}

void Lsv_CommandPrintCuts_Usage(){
  Abc_Print(-2, "usage: lsv_printcut [-h] <k>\n");
  Abc_Print(-2, "\t        prints k-feasible cuts for each nodes in the network, where 3 <= k <= 6\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  Abc_Print(-2, "\t<k>   : the number of feasible cuts\n");
  return;
}