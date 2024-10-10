#include <stdio.h>
#include <stdlib.h>

#include <algorithm>  // for std::find
#include <iostream>
#include <set>
#include <vector>

#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
using namespace std;

int max_node = 0;
vector<vector<vector<int>>> cut_dynamic;
int K = 3;
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

vector<int> merge(const vector<int>& vec1, const vector<int>& vec2) {
    // set can remove repeated ones and is ordered
    set<int> merged_set(vec1.begin(), vec1.end());
    merged_set.insert(vec2.begin(), vec2.end());

    vector<int> result(merged_set.begin(), merged_set.end());
    return result;
}
// check already exist or not
bool areVectorsEqual(const vector<int>& vec1, const vector<int>& vec2) {
    return vec1 == vec2;
}
void addUniqueCut(vector<vector<int>>& cuts, const vector<int>& newCut) {
    auto it = find_if(cuts.begin(), cuts.end(),
                      [&](const vector<int>& cut) {
                          return areVectorsEqual(cut, newCut);
                      });
    if (it == cuts.end()) {
        cuts.push_back(newCut);
    }
}

void Lsv_NtkPrint(Abc_Ntk_t* pNtk) {
    Abc_Obj_t* pObj;
    int i;
    // find max_node
    Abc_NtkForEachNode(pNtk, pObj, i) {
        int cur_node = Abc_ObjId(pObj);
        max_node = max_node > cur_node ? max_node : cur_node;
    }
    // Initialize
    for (int d = 0; d <= max_node; d++)
        cut_dynamic.push_back(vector<vector<int>>());

    // go through every input
    Abc_NtkForEachPi(pNtk, pObj, i) {
        int cur_node = Abc_ObjId(pObj);
        cut_dynamic[cur_node].push_back((vector<int>{cur_node}));
        // print
        for (int j = 0; j < cut_dynamic[cur_node].size(); j++) {
            if (cut_dynamic[cur_node][j].empty()) break;
            cout << cur_node << ":";
            for (vector<int>::iterator iter = cut_dynamic[cur_node][j].begin(); iter != cut_dynamic[cur_node][j].end(); iter++) {
                cout << " " << *iter;
            }
            cout << endl;
        }
    }
    // go through every logic node
    Abc_NtkForEachNode(pNtk, pObj, i) {
        int cur_node = Abc_ObjId(pObj);
        Abc_Obj_t* pFanin;
        int j;
        int left = 0, right = 0;
        Abc_ObjForEachFanin(pObj, pFanin, j) {
            if (!j)
                left = Abc_ObjId(pFanin);
            else
                right = Abc_ObjId(pFanin);
        }
        vector<int> temp;
        cut_dynamic[cur_node].push_back(vector<int>{cur_node});
        for (int i = 0; i < cut_dynamic[left].size(); i++) {
            for (int j = 0; j < cut_dynamic[right].size(); j++) {
                vector<int> temp = merge(cut_dynamic[left][i], cut_dynamic[right][j]);
                if (temp.size() > K) continue;
                // check exist
                addUniqueCut(cut_dynamic[cur_node], temp);  // 加入cut
                // cut_dynamic[cur_node].push_back(temp);
            }
        }
        // print
        for (int i = 0; i < cut_dynamic[cur_node].size(); i++) {
            cout << cur_node << ":";
            for (vector<int>::iterator iter = cut_dynamic[cur_node][i].begin(); iter != cut_dynamic[cur_node][i].end(); iter++) {
                cout << " " << *iter;
            }
            cout << endl;
        }
    }
    // go through every output
    // cuts of output node = cuts of it's (only) child + itself
    Abc_NtkForEachPo(pNtk, pObj, i) {
        int cur_node = Abc_ObjId(pObj);
        Abc_Obj_t* pFanin;
        int j;
        int left = 0, right = 0;
        Abc_ObjForEachFanin(pObj, pFanin, j) {
            if (!j)
                left = Abc_ObjId(pFanin);
            else
                right = Abc_ObjId(pFanin);
        }

        cut_dynamic[cur_node].push_back(vector<int>{cur_node});
        for (int i = 0; i < cut_dynamic[left].size(); i++) {
            cut_dynamic[cur_node].push_back(cut_dynamic[left][i]);
        }
        // print
        for (int i = 0; i < cut_dynamic[cur_node].size(); i++) {
            cout << cur_node << ":";
            for (vector<int>::iterator iter = cut_dynamic[cur_node][i].begin(); iter != cut_dynamic[cur_node][i].end(); iter++) {
                cout << " " << *iter;
            }
            cout << endl;
        }
    }

    cut_dynamic.clear();
    cut_dynamic.shrink_to_fit();
}
int Lsv_CommandPrintCut(Abc_Frame_t* pAbc, int argc, char** argv) {
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    int c;
    if (argv[1]) K = atoi(argv[1]);
    Extra_UtilGetoptReset();
    while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
        switch (c) {
            // case 'K':
            //     if (globalUtilOptind >= argc) {
            //         Abc_Print(-1, "Command line switch \"-K\" should be followed by an integer.\n");
            //         goto usage;
            //     }
            //     K = atoi(argv[globalUtilOptind]);
            //     globalUtilOptind++;
            //     // cout << K << endl;
            //     break;
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
    Lsv_NtkPrint(pNtk);
    K = 3;
    return 0;

usage:
    Abc_Print(-2, "usage: lsv_printcut [-h]\n");
    Abc_Print(-2, "\t        prints the k-feasible cuts in the network\n");
    Abc_Print(-2, "\t-h    : print the command usage\n");
    return 1;
}