#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <stdlib.h>
#include <map>
#include <vector>
#include <algorithm>

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
void Lsv_NtkCrossProduct(const std::map<unsigned int, std::vector<std::vector<unsigned int>>> &mp, 
                                const std::vector<unsigned int> &fanins, std::vector<std::vector<unsigned int>> &res, const int& k) {
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

    const std::vector<std::vector<unsigned int>> &next_set = mp.at(fanin);
    std::vector<std::vector<unsigned int>> new_res;

    // Perform cross-product between current result and the next set of vectors
    for (const auto &current_vec : res) {
        for (const auto &next_vec : next_set) {
            // if current cut is > k
            if (current_vec.size() > k || next_vec.size() > k)
              continue;
            std::vector<unsigned int> combined_vec = current_vec;
            combined_vec.insert(combined_vec.end(), next_vec.begin(), next_vec.end());
            new_res.push_back(combined_vec);
        }
    }

    res = new_res;  // Update result
  }

  // remove duplicate
  for (size_t i = 0; i < res.size(); i++) {
    std::sort(res[i].begin(), res[i].end());
    res[i].erase(std::unique(res[i].begin(), res[i].end()), res[i].end());
  }
}

void Lsv_NtkPrintCuts(Abc_Ntk_t* pNtk, int k) {
  std::map<unsigned int, std::vector<std::vector<unsigned int>>> mp;
  Abc_Obj_t* pObj;
  int i;
  Abc_NtkForEachPi(pNtk, pObj, i) {
    mp[Abc_ObjId(pObj)] = std::vector<std::vector<unsigned int>>{std::vector<unsigned int>{Abc_ObjId(pObj)}};
    printf("%d: %d\n", Abc_ObjId(pObj), Abc_ObjId(pObj));
  }
  Abc_NtkForEachNode(pNtk, pObj, i) {
    // printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
    Abc_Obj_t* pFanin;
    int j;
    std::vector<unsigned int> fanins;
    // For each node, find their fanins
    Abc_ObjForEachFanin(pObj, pFanin, j) {
      // printf("  Fanin-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanin),
      //        Abc_ObjName(pFanin));
      fanins.push_back(Abc_ObjId(pFanin));
      // if (mp.find(Abc_ObjId(pFanin)) == mp.end())
      //   mp[Abc_ObjId(pFanin)] = std::vector<std::vector<unsigned int>>{std::vector<unsigned int>{Abc_ObjId(pFanin)}};
    }

    // the pObj itself is a cut too
    std::vector<std::vector<unsigned int>> res;
    // mp[Abc_ObjId(pObj)] = res;
    // find cuts with respect to the pObj
    Lsv_NtkCrossProduct(mp, fanins, res, k);
    res.insert(std::begin(res), std::vector<unsigned int>{Abc_ObjId(pObj)});
    for (int i = 0; i < res.size(); i++) {
      // if the size of cut > k, ignore
      if (res[i].size() > k)
        continue;
      printf("%d: ", Abc_ObjId(pObj));
      for (int j = 0; j < res[i].size(); j++) {
        printf("%d", res[i][j]);
        if (j == res[i].size()-1)
          printf("\n");
        else
          printf(" ");
      }
    }
    mp[Abc_ObjId(pObj)] = res;
  }
  Abc_NtkForEachPo(pNtk, pObj, i) {
    // printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
    Abc_Obj_t* pFanin;
    int j;
    std::vector<unsigned int> fanins;
    // For each node, find their fanins
    Abc_ObjForEachFanin(pObj, pFanin, j) {
      fanins.push_back(Abc_ObjId(pFanin));
    }

    // the pObj itself is a cut too
    std::vector<std::vector<unsigned int>> res;
    // find cuts with respect to the pObj
    Lsv_NtkCrossProduct(mp, fanins, res, k);
    res.insert(std::begin(res), std::vector<unsigned int>{Abc_ObjId(pObj)});
    for (int i = 0; i < res.size(); i++) {
      // if the size of cut > k, ignore
      if (res[i].size() > k)
        continue;
      printf("%d: ", Abc_ObjId(pObj));
      for (int j = 0; j < res[i].size(); j++) {
        printf("%d", res[i][j]);
        if (j == res[i].size()-1)
          printf("\n");
        else
          printf(" ");
      }
    }
  }
}

int Lsv_CommandPrintCut(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  // if (argc != 2)
  //   goto usage;
  printf("argc: %d, argv[1]: %s\n", argc, argv[1]);
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