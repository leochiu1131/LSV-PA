#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <vector>
#include <iostream>
#include <algorithm>

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

std::set<int> mergeSets(const std::set<int>& s1, const std::set<int>& s2) {
    std::set<int> result;
    std::set_union(s1.begin(), s1.end(), s2.begin(), s2.end(), std::inserter(result, result.begin()));
    return result;
}

bool isBetter(const std::set<int>& s1, const std::set<int>& s2) {
    return std::includes(s1.begin(), s1.end(), s2.begin(), s2.end());
}

int Lsv_PrintCut(Abc_Frame_t* pAbc, int argc, char** argv) {
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

    if (argc == 2) {
        int k = argv[1][0] - '0';
        Abc_Obj_t* pObj;
        int i;
        std::vector<std::vector<std::set<int>>> nodeSets(Abc_NtkPiNum(pNtk) + Abc_NtkObjNum(pNtk));

        Abc_NtkForEachPi(pNtk, pObj, i) {
            if (Abc_ObjIsPi(pObj)) {
                std::vector<std::set<int>> setsForPi;
                std::set<int> singleSet = {Abc_ObjId(pObj)};
                setsForPi.push_back(singleSet);
                nodeSets[Abc_ObjId(pObj)] = setsForPi;
                if (1 <= k) {
                    printf("%d: %d\n", Abc_ObjId(pObj), Abc_ObjId(pObj));
                }
            }
        }

        Abc_NtkForEachNode(pNtk, pObj, i) {
            std::vector<std::pair<std::set<int>, bool>> setPairs;
            setPairs.push_back({{Abc_ObjId(pObj)}, true});

            Abc_Obj_t* fanin0 = Abc_ObjFanin0(pObj);
            Abc_Obj_t* fanin1 = Abc_ObjFanin1(pObj);

            std::vector<std::set<int>> setsFanin0 = nodeSets[Abc_ObjId(fanin0)];
            std::vector<std::set<int>> setsFanin1 = nodeSets[Abc_ObjId(fanin1)];

            for (const auto& set0 : setsFanin0) {
                for (const auto& set1 : setsFanin1) {
                    std::set<int> combinedSet;
                    std::set_union(set0.begin(), set0.end(), set1.begin(), set1.end(), std::inserter(combinedSet, combinedSet.begin()));
                    if (combinedSet.size() <= k) {
                        bool exists = false;
                        for (const auto& setPair : setPairs) {
                            if (setPair.first == combinedSet) {
                                exists = true;
                                break;
                            }
                        }
                        if (!exists) {
                            setPairs.push_back({combinedSet, true});
                        }
                    }
                }
            }

            for (size_t i = 0; i < setPairs.size(); ++i) {
                if (setPairs[i].second) {
                    for (size_t j = i + 1; j < setPairs.size(); ++j) {
                        if (setPairs[j].second) {
                            if (isBetter(setPairs[i].first, setPairs[j].first)) {
                                setPairs[j].second = false;
                            } else if (isBetter(setPairs[j].first, setPairs[i].first)) {
                                setPairs[i].second = false;
                            }
                        }
                    }
                }
            }

            for (const auto& setPair : setPairs) {
                if (setPair.second) {
                    nodeSets[Abc_ObjId(pObj)].push_back(setPair.first);
                }
            }

            for (const auto& resultSet : nodeSets[Abc_ObjId(pObj)]) {
                printf("%d:", Abc_ObjId(pObj));
                for (const auto& element : resultSet) {
                    printf(" %d", element);
                }
                printf("\n");
            }
        }
    } else {
        printf("Please provide a number k.\n");
    }

    return 0;

usage:
    Abc_Print(-2, "usage: lsv_printcut [-h]\n");
    Abc_Print(-2, "\tk       prints k-feasible cuts in the network\n");
    Abc_Print(-2, "\t-h    : print the command usage\n");
    return 1;
}










