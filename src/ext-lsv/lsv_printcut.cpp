#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "aig/aig/aig.h"
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <algorithm>
#include <queue>
#include <unordered_set>

static int Lsv_CommandPrintCuts(Abc_Frame_t* pAbc, int argc, char** argv);
void enumerate_k_feasible_cuts_rec(Abc_Obj_t* pNode, int k, std::vector< std::vector<int> >& cuts, std::vector<int> current_cut);
bool is_valid_cut(Abc_Obj_t* pNode, const std::vector<int>& cut);

void init(Abc_Frame_t* pAbc) {
    Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_CommandPrintCuts, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
    PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

/*
 * Strategy for Enumerating k-Feasible Cuts:
 * ------------------------------------------
 * The program aims to find all k-feasible cuts for each node in a given network.
 * A cut is a set of nodes that can separate a given node from the primary inputs.
 * The approach is divided into two main parts:
 *
 * 1. **Enumeration of Cuts**: Using a recursive function, we first generate all possible cuts for each node,
 *    starting from the node itself and expanding to include its fanins. Each cut is recursively explored,
 *    ensuring that it does not exceed the specified size (k).
 *
 * 2. **Validation of Cuts**: Once a potential cut is found, it is validated to determine if it forms a valid
 *    separation for the node. The validation is done using a Breadth-First Search (BFS) approach to ensure
 *    that all paths from the node to primary inputs are "covered" by nodes in the cut.
 */

// Recursively enumerate k-feasible cuts
void enumerate_k_feasible_cuts_rec(Abc_Obj_t* pNode, int k, std::vector< std::vector<int> >& cuts, std::vector<int> current_cut) {
    // If the cut is too big, return
    if (current_cut.size() > (unsigned)k) {
        return;
    }
    // Sort the current cut for consistency
    std::sort(current_cut.begin(), current_cut.end());
    // If cut already exists, return
    if (std::find(cuts.begin(), cuts.end(), current_cut) != cuts.end()) {
        return;
    }
    // Check if the current cut is valid
    if (!is_valid_cut(pNode, current_cut)) {
        return;
    }
    // Add current cut to the list of feasible cuts
    cuts.push_back(current_cut);

    if (Abc_ObjIsCi(pNode)) {
        return;
    }

    Abc_Obj_t* pFanin0 = Abc_ObjFanin0(pNode);
    Abc_Obj_t* pFanin1 = Abc_ObjFanin1(pNode);

    // Recursively add cuts for each fanin
    if (pFanin0) {
        current_cut.push_back((int)Abc_ObjId(pFanin0));
        enumerate_k_feasible_cuts_rec(pFanin0, k, cuts, current_cut);
        current_cut.pop_back();
    }
    if (pFanin1) {
        current_cut.push_back((int)Abc_ObjId(pFanin1));
        enumerate_k_feasible_cuts_rec(pFanin1, k, cuts, current_cut);
        current_cut.pop_back();
    }

    // Consider the combination of both fanins
    if (pFanin0 && pFanin1) {
        current_cut.push_back((int)Abc_ObjId(pFanin0));
        current_cut.push_back((int)Abc_ObjId(pFanin1));
        enumerate_k_feasible_cuts_rec(pNode, k, cuts, current_cut);
        current_cut.pop_back();
        current_cut.pop_back();
    }
}

// Check if the current cut is a valid cut for the given node
bool is_valid_cut(Abc_Obj_t* pNode, const std::vector<int>& cut) {
    std::unordered_set<int> cut_set(cut.begin(), cut.end());
    std::queue<Abc_Obj_t*> queue;
    std::unordered_set<int> visited;
    
    // Start the BFS from the given node
    queue.push(pNode);
    visited.insert(Abc_ObjId(pNode));

    while (!queue.empty()) {
        Abc_Obj_t* current = queue.front();
        queue.pop();

        // To know if a cut is valide we ensure that all paths from the given node to primary inputs are "covered" by nodes in the cut.
        if (Abc_ObjIsCi(current) && cut_set.find(Abc_ObjId(current)) == cut_set.end()) {
            return true;
        }

        // Add fanins to the queue if they are not in the cut and not already visited
        if (!Abc_ObjIsCi(current)) {
            Abc_Obj_t* pFanin0 = Abc_ObjFanin0(current);
            Abc_Obj_t* pFanin1 = Abc_ObjFanin1(current);
            if (pFanin0 && visited.find(Abc_ObjId(pFanin0)) == visited.end() && cut_set.find(Abc_ObjId(pFanin0)) == cut_set.end()) {
                queue.push(pFanin0);
                visited.insert(Abc_ObjId(pFanin0));
            }
            if (pFanin1 && visited.find(Abc_ObjId(pFanin1)) == visited.end() && cut_set.find(Abc_ObjId(pFanin1)) == cut_set.end()) {
                queue.push(pFanin1);
                visited.insert(Abc_ObjId(pFanin1));
            }
        }
    }

    return false;
}

void enumerate_k_feasible_cuts(Abc_Ntk_t* pNtk, int k) {
    int i;
    Abc_Obj_t* pNode;
    // Iterate through the nodes one by one
    Abc_NtkForEachNode(pNtk, pNode, i) {
        if (Abc_ObjIsPo(pNode) || Abc_AigNodeIsConst(pNode)) {
            continue; // Skip primary outputs and constant nodes
        }
        // List that will contain the cuts of the current node
        std::vector< std::vector<int> > cuts;
        // Place the node itself as the initial cut
        std::vector<int> initial_cut = {(int)Abc_ObjId(pNode)};
        enumerate_k_feasible_cuts_rec(pNode, k, cuts, initial_cut);

        // Print the cuts
        for (const auto& cut : cuts) {
            printf("%d: ", Abc_ObjId(pNode));
            for (size_t j = 0; j < cut.size(); ++j) {
                printf("%d%c", cut[j], (j == cut.size() - 1) ? '\n' : ' ');
            }
        }
    }
}

// Command for integrating into ABC under the 'lsv_printcut' command
int Lsv_CommandPrintCuts(Abc_Frame_t* pAbc, int argc, char** argv) {
    // If the command is incorrectly entered, display the correct usage
    if (argc != 2) {
        Abc_Print(-1, "Usage: lsv_printcut <k>\n");
        return 1;
    }
    // Retrieve the entered k value, if it is invalid, remind the correct interval for k
    int k = atoi(argv[1]);
    if (k < 3 || k > 6) {
        Abc_Print(-1, "Error: k should be between 3 and 6.\n");
        return 1;
    }
    // Retrieve the network loaded in ABC, return an error if no network is loaded
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    if (!pNtk) {
        Abc_Print(-1, "Error: no network is loaded.\n");
        return 1;
    }

    enumerate_k_feasible_cuts(pNtk, k);
    return 0;
}