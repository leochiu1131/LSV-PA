#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <stdio.h>
#include <stdlib.h>
#include "misc/vec/vec.h"

// Function for the printcut command
static int Lsv_CommandPrintCut(Abc_Frame_t* pAbc, int argc, char** argv);

// Initialization function
void Lsv_init(Abc_Frame_t* pAbc){
    // Register the lsv_printcut command
    Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_CommandPrintCut, 0);
}

void destroy(Abc_Frame_t* pAbc){}

Abc_FrameInitializer_t frame_initializer = {Lsv_init, destroy};

struct PackageRegistrationManager{
    PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

// Function to compute k-feasible cuts for a given node
Vec_Ptr_t* ComputeCuts(Abc_Obj_t* pNode, int k) {
    // If the node is a primary input, the only cut is the node itself
    if (Abc_ObjIsCi(pNode)) {
        Vec_Ptr_t* vCuts = Vec_PtrAlloc(1);  // Allocate a vector to store the cuts
        Vec_Int_t* vCut = Vec_IntAlloc(1);   // Allocate a vector for a single cut
        Vec_IntPush(vCut, Abc_ObjId(pNode)); // Add the node's ID to the cut
        Vec_PtrPush(vCuts, vCut);            // Store the cut in the list of cuts
        return vCuts;
    }

    // Check if the node has fanins
    if (Abc_ObjFaninNum(pNode) < 2) {
        printf("Node %d has less than 2 fanins. Skipping.\n", Abc_ObjId(pNode));
        return NULL;
    }

    // Retrieve the fanin nodes of the current node
    Abc_Obj_t* pFanin0 = Abc_ObjFanin0(pNode);
    Abc_Obj_t* pFanin1 = Abc_ObjFanin1(pNode);

    // Get the cuts for each fanin node
    Vec_Ptr_t* vCuts0 = ComputeCuts(pFanin0, k);
    Vec_Ptr_t* vCuts1 = ComputeCuts(pFanin1, k);

    // Check if the fanins returned cuts
    if (!vCuts0 || !vCuts1) {
        return NULL;
    }

    // Vector to store the combined cuts
    Vec_Ptr_t* vCuts = Vec_PtrAlloc(100);  // Allocate space for the combined cuts

    // Add the node itself as an individual cut
    Vec_Int_t* vSelfCut = Vec_IntAlloc(1);
    Vec_IntPush(vSelfCut, Abc_ObjId(pNode));
    Vec_PtrPush(vCuts, vSelfCut);  // This cut will be displayed first

    // Combine the cuts of the fanin nodes with a Cartesian product
    int i, j;
    Vec_Int_t* vCut0, *vCut1;
    Vec_PtrForEachEntry(Vec_Int_t*, vCuts0, vCut0, i) {
        Vec_PtrForEachEntry(Vec_Int_t*, vCuts1, vCut1, j) {
            // Combine the two cuts
            Vec_Int_t* vNewCut = Vec_IntTwoMerge(vCut0, vCut1); // Merge and sort the two cuts

            // Ignore if the combined cut is larger than k
            if (Vec_IntSize(vNewCut) > k) {
                Vec_IntFree(vNewCut);
                continue;
            }

            // Add the current node to the cut if not already included
            if (!Vec_IntFind(vNewCut, Abc_ObjId(pNode)))
                Vec_IntPush(vNewCut, Abc_ObjId(pNode));

            // Sort the cut and add it to the final list of cuts
            Vec_IntSort(vNewCut, 0);
            Vec_PtrPush(vCuts, vNewCut);
        }
    }

    // Free the memory used for the fanin cuts
    Vec_PtrFree(vCuts0);
    Vec_PtrFree(vCuts1);

    return vCuts;
}

// Function to display all k-feasible cuts for a given node
void PrintCuts(Abc_Obj_t* pNode, Vec_Ptr_t* vCuts) {
    Vec_Int_t* vCut;
    int i, j;

    // Display the cut of the node itself first
    Vec_Int_t* vSelfCut = (Vec_Int_t*)Vec_PtrEntry(vCuts, 0);
    printf("%d: ", Abc_ObjId(pNode));
    for (j = 0; j < Vec_IntSize(vSelfCut); j++)
        printf("%d ", Vec_IntEntry(vSelfCut, j));
    printf("\n");

    // Then display all other combinations
    for (i = 1; i < Vec_PtrSize(vCuts); i++) {
        vCut = (Vec_Int_t*)Vec_PtrEntry(vCuts, i);
        printf("%d: ", Abc_ObjId(pNode));
        for (j = 0; j < Vec_IntSize(vCut); j++)
            printf("%d ", Vec_IntEntry(vCut, j));
        printf("\n");
    }
}

// Main function to enumerate and display k-feasible cuts for each node
void Lsv_EnumerateCuts(Abc_Ntk_t* pNtk, int k) {
    Abc_Obj_t* pNode;
    int i;

    // Iterate through each node in the network and compute k-feasible cuts
    Abc_NtkForEachObj(pNtk, pNode, i) { // Iterate through all nodes, including primary inputs
        if (Abc_ObjIsNode(pNode) || Abc_ObjIsCi(pNode)) {
            Vec_Ptr_t* vCuts = ComputeCuts(pNode, k);
            if (vCuts) {
                PrintCuts(pNode, vCuts);
                Vec_PtrFree(vCuts);
            }
        }
    }
}

// Command function for `lsv_printcut` 
int Lsv_CommandPrintCut(Abc_Frame_t* pAbc, int argc, char** argv) {
    int k;
    // Parse the command-line argument to get the value of k
    if (argc != 2 || (k = atoi(argv[1])) < 1) {
        printf("Usage: lsv_printcut <k>\n");
        return 1;
    }

    // Retrieve the current Boolean network in ABC
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    if (!pNtk) {
        printf("Error: Empty network.\n");
        return 1;
    }

    // Call the function to enumerate and display k-feasible cuts for each node in the network
    Lsv_EnumerateCuts(pNtk, k);
    return 0;
}
