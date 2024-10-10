#include <algorithm>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"

using namespace std;

int K = 3;
unordered_map<int, vector<vector<int>>> cuts;
int max_depth = 0;
int max_node = 0;

static int Lsv_CommandPrintKcut(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
    
    Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_CommandPrintKcut, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
    PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

vector<int> mergeCuts(const vector<int>& cut1, const vector<int>& cut2) {
    unordered_set<int> merged_set(cut1.begin(), cut1.end());
    merged_set.insert(cut2.begin(), cut2.end());

    vector<int> merged_vector(merged_set.begin(), merged_set.end());
    sort(merged_vector.begin(), merged_vector.end());
    return merged_vector;
}

void processNode(Abc_Obj_t* pObj) {
    if (!pObj) return;

    int cur_node = Abc_ObjId(pObj);

    if (cuts.find(cur_node) != cuts.end()) return;

    cuts[cur_node].resize(1);
    cuts[cur_node][0].push_back(cur_node);

    Abc_Obj_t* pFanin;
    int left = -1, right = -1;
    int j;

    Abc_ObjForEachFanin(pObj, pFanin, j) {
        if (j == 0) {
            left = Abc_ObjId(pFanin);
        } else if (j == 1) {
            right = Abc_ObjId(pFanin);
        }
    }

    // 將 left 和 right 的 cuts 進行 merge
    for (const auto& cutL : cuts[left]) {
        for (const auto& cutR : cuts[right]) {
            vector<int> temp = mergeCuts(cutL, cutR);
            if (temp.size() <= K && 
                find(cuts[cur_node].begin(), cuts[cur_node].end(), temp) == cuts[cur_node].end()) {
                cuts[cur_node].push_back(temp);
            }
        }
    }

    // 輸出 cuts 結果
    for (int i = 0; i < cuts[cur_node].size(); i++) {
        cout << cur_node << ":";
        for (auto iter = cuts[cur_node][i].begin(); iter != cuts[cur_node][i].end(); ++iter) {
            cout << " " << *iter;
        }
        cout << endl;
    }

    max_depth = max(max_depth, (int)cuts[cur_node].size());
    max_node = max(max_node, cur_node);
}

void Lsv_NtkPrint(Abc_Ntk_t* pNtk) {
    Abc_Obj_t* pObj;
    int i;

    cuts.clear();
    max_depth = 0;
    max_node = 0;

    Abc_NtkForEachPi(pNtk, pObj, i) {
        processNode(pObj);
    }

    Abc_NtkForEachNode(pNtk, pObj, i) {
        processNode(pObj);
    }

    // 修正處理輸出節點的邏輯
    Abc_NtkForEachPo(pNtk, pObj, i) {
        Abc_Obj_t* pFanin = Abc_ObjFanin0(pObj);
        if (pFanin) {
            processNode(pFanin); // 處理輸出節點的 fanin
        }
    }

    
    vector<pair<int, vector<vector<int>>>> sorted_cuts(cuts.begin(), cuts.end());
    sort(sorted_cuts.begin(), sorted_cuts.end(), [](const auto& a, const auto& b) {
        return a.first < b.first;
    });
}

int Lsv_CommandPrintKcut(Abc_Frame_t* pAbc, int argc, char** argv) {
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    int c;
    if (argv[1]) K = atoi(argv[1]);
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
    Lsv_NtkPrint(pNtk);
    K = 3;
    return 0;

usage:
    Abc_Print(-2, "usage: lsv_printKcut [-h] [-K <integer>]\n");
    Abc_Print(-2, "\t        prints the k-feasible cuts in the network\n");
    Abc_Print(-2, "\t-K <integer> : specify the K value (between 3 and 6)\n");
    Abc_Print(-2, "\t-h           : print the command usage\n");
    return 1;
}