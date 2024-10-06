#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <stdlib.h>
#include <map>
#include <vector>
#include <algorithm>
#include <iostream>
#include <set>

using Cut = std::vector<unsigned int>;
using CutSet = std::set<Cut>;

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

// TODO PA1 Q4
void Lsv_NtkCrossProduct(const std::map<unsigned int, CutSet> &mp, 
                                const std::vector<unsigned int> &fanins, CutSet &res, const int& k) {
  // Check if fanins is empty
  if (fanins.empty()) {
    return;  // Nothing to do
  }

  // Initialize result with the vectors from the first element in fanins
  int first_fanin = fanins[0];
  if (mp.find(first_fanin) != mp.end())
    res = mp.at(first_fanin);  // Start with the first set of vectors
   else 
    return;  // First element not in the map, nothing to process
  
  for (size_t i = 1; i < fanins.size(); ++i) {
    int fanin = fanins[i];

    // Check if the fanin is in the map
    if (mp.find(fanin) == mp.end()) {
        continue;  // Skip if the fanin is not in the map
    }

    const CutSet &next_set = mp.at(fanin);
    CutSet new_res;

    // Perform cross-product between current result and the next set of cuts
    for (const auto &current_cut : res) {
      for (const auto &next_cut : next_set) {
        Cut combined_cut = current_cut;
        combined_cut.insert(combined_cut.end(), next_cut.begin(), next_cut.end());

        // Remove duplicates within the cut
        std::sort(combined_cut.begin(), combined_cut.end());
        combined_cut.erase(
            std::unique(combined_cut.begin(), combined_cut.end()),
            combined_cut.end());

        // Check the size after removing duplicates
        if (combined_cut.size() > static_cast<size_t>(k))
          continue;

        new_res.insert(combined_cut);  // Add to the new result set
      }
    }

    res = std::move(new_res);  // Update result
  }

}

void Lsv_NtkPrintCuts(Abc_Ntk_t* pNtk, int k) {
  std::map<unsigned int, CutSet> mp;
  Abc_Obj_t* pObj;
  int i;

  Abc_NtkForEachPi(pNtk, pObj, i) {
    unsigned int obj_id = Abc_ObjId(pObj);
    mp[obj_id] = {{obj_id}};
    printf("%d: %d\n", obj_id, obj_id);
  }

  Abc_NtkForEachNode(pNtk, pObj, i) {
    unsigned int obj_id = Abc_ObjId(pObj);
    Abc_Obj_t* pFanin;
    int j;
    std::vector<unsigned int> fanins;
    // For each node, find their fanins
    Abc_ObjForEachFanin(pObj, pFanin, j) {
      fanins.push_back(Abc_ObjId(pFanin));
    }

    CutSet res;
    Lsv_NtkCrossProduct(mp, fanins, res, k);

    // Include the node itself as a cut
    res.insert({obj_id});

    // Store the cuts for future use
    mp[obj_id] = res;
    
    // Output the cuts
    for (const auto &cut : res) {
      printf("%d:", obj_id);
      for (size_t idx = 0; idx < cut.size(); ++idx) {
        printf(" %d", cut[idx]);
      }
      printf("\n");
    }
  }
}

int Lsv_CommandPrintCut(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  if (argc != 2){
    Abc_Print(-2, "usage: lsv_printcut k\n");
    Abc_Print(-2, "\t        prints the k-feasible cuts of primary inputs and AND nodes\n");
    // Abc_Print(-2, "\t-h    : print the command usage\n");
    return 1;
  }
  // printf("argc: %d, argv[1]: %s\n", argc, argv[1]);
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  int k = atoi(argv[1]);
  Lsv_NtkPrintCuts(pNtk, k);
  return 0;

// usage:
//   Abc_Print(-2, "usage: lsv_printcut k\n");
//   Abc_Print(-2, "\t        prints the nodes in the network\n");
//   Abc_Print(-2, "\t-h    : print the command usage\n");
//   return 1;
}