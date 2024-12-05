/*
This implementation focuses on computing the Satisfiability Don’t Cares (SDCs) for a specific node within an AIG network. 
To achieve this, the program extracts the transitive fanin cone of the target node using the function `Abc_NtkCreateConeArray`. 
This approach ensures that the fanins of the target node are treated as primary outputs in the derived network, which is critical for subsequent SAT-based analysis.

Random simulation is performed with 1000 iterations to identify which patterns (out of 00, 01, 10, 11) are observable on the fanin nodes of the target node. 
The observed patterns are tracked using a bitmask, which provides an efficient representation of the simulation results. 
This step aims to preemptively eliminate patterns that are naturally observable, reducing the load on the SAT solver.

The SAT solver is initialized with a CNF derived from the AIG representation of the transitive fanin cone. 
The CNF mapping associates each node in the cone with corresponding SAT variables. 
For each unobserved pattern, constraints are added to the SAT solver to determine whether the pattern is satisfiable. 
If the SAT solver returns UNSAT for a pattern, it is classified as an SDC.

Finally, the program outputs the detected SDCs in binary format. If no unobserved patterns are identified as SDCs, it outputs "no sdc." 
This ensures clarity and adherence to the requirements of the problem statement.

Despite these technical choices, the implementation consistently outputs "no sdc" for all tested nodes. 
This outcome suggests potential issues such as over-simulation leading to all patterns being observed, incorrect CNF variable mapping, 
or inherent properties of the test BLIF file that preclude the presence of SDCs. 
Further debugging and testing on simpler circuits or known cases are required to validate and refine the approach.


Result:
Despite the implemented logic, the code consistently outputs "no sdc" for all tested nodes, indicating no unobserved patterns or possible SDCs. This outcome might result from:
1. Over-simulation, causing all patterns to appear as observed.
2. Potential issues with CNF variable mapping or SAT constraints.

Further testing on simpler circuits or known cases is necessary to validate the approach.
*/

#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/cnf/cnf.h"
#include "sat/bsat/satSolver.h"

extern "C" {
Aig_Man_t* Abc_NtkToDar(Abc_Ntk_t* pNtk, int fExors, int fRegisters);
}

// Function prototype for the command handler
static int Lsv_CommandSDC(Abc_Frame_t* pAbc, int argc, char** argv);

// Function prototype for computing SDC
static void Lsv_ComputeSDC(Abc_Ntk_t* pNtk, Abc_Obj_t* pNode);

static void init(Abc_Frame_t* pAbc) {
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sdc", Lsv_CommandSDC, 0);
}

static void destroy(Abc_Frame_t* pAbc) {}

static Abc_FrameInitializer_t frame_initializer = {init, destroy};
struct PackageRegistrationManager {
    PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsv_SDC_PackageRegistrationManager;

// Core function to compute SDCs for a given node
static void Lsv_ComputeSDC(Abc_Ntk_t* pNtk, Abc_Obj_t* pNode) {
    if (!pNtk || !pNode) {
        Abc_Print(-1, "Invalid network or node.\n");
        return;
    }

    // Create transitive fanin cone for the target node's fanins
    Vec_Ptr_t* vNodes = Vec_PtrAlloc(2);
    Vec_PtrPush(vNodes, Abc_ObjFanin0(pNode)); // Add the first fanin
    Vec_PtrPush(vNodes, Abc_ObjFanin1(pNode)); // Add the second fanin

    Abc_Print(1, "Processing Node: %d\n", Abc_ObjId(pNode)); // Debug: Node ID
    Abc_Print(1, "Fanin Nodes: %d, %d\n", Abc_ObjId(Abc_ObjFanin0(pNode)), Abc_ObjId(Abc_ObjFanin1(pNode))); // Debug: Fanin IDs

    // Create a network cone with fanins as primary outputs
    Abc_Ntk_t* pConeNtk = Abc_NtkCreateConeArray(pNtk, vNodes, 1);
    if (!pConeNtk) {
        Abc_Print(-1, "Failed to create cone network.\n");
        Vec_PtrFree(vNodes);
        return;
    }

    // Convert the cone network into an AIG
    Aig_Man_t* pAig = Abc_NtkToDar(pConeNtk, 0, 0);
    if (!pAig) {
        Abc_Print(-1, "Failed to create AIG.\n");
        Abc_NtkDelete(pConeNtk);
        Vec_PtrFree(vNodes);
        return;
    }

    // Derive CNF from the AIG
    Cnf_Dat_t* pCnf = Cnf_Derive(pAig, Aig_ManCoNum(pAig));
    if (!pCnf) {
        Abc_Print(-1, "Failed to create CNF.\n");
        Aig_ManStop(pAig);
        Abc_NtkDelete(pConeNtk);
        Vec_PtrFree(vNodes);
        return;
    }

    // Print CNF variable mapping for the fanins
    Abc_Print(1, "CNF Mapping: y0=%d, y1=%d\n", 
        pCnf->pVarNums[Abc_ObjId(Abc_ObjFanin0(pNode))], 
        pCnf->pVarNums[Abc_ObjId(Abc_ObjFanin1(pNode))]);

    // Initialize SAT solver
    sat_solver* pSat = sat_solver_new();
    sat_solver_setnvars(pSat, pCnf->nVars);
    Cnf_DataWriteIntoSolver(pCnf, 1, 0);

    // Perform random simulation to observe patterns
    srand(time(NULL));
    int observedPatterns = 0; // Bitmask to track observed patterns

    for (int sim = 0; sim < 1000; ++sim) {
        int value0 = rand() & 1; // Random value for fanin0
        int value1 = rand() & 1; // Random value for fanin1

        observedPatterns |= (1 << (value0 * 2 + value1)); // Update observed patterns
    }

    // Debug: Print observed patterns
    Abc_Print(1, "Observed Patterns: %d%d%d%d\n", 
        (observedPatterns & 1) > 0, 
        (observedPatterns & 2) > 0, 
        (observedPatterns & 4) > 0, 
        (observedPatterns & 8) > 0);

    // Check unobserved patterns using SAT
    bool foundSDC = false;
    for (int pattern = 0; pattern < 4; ++pattern) {
        if (observedPatterns & (1 << pattern)) {
            continue; // Skip observed patterns
        }

        int value0 = (pattern >> 1) & 1; // MSB
        int value1 = pattern & 1;       // LSB

        int var0 = pCnf->pVarNums[Abc_ObjId(Abc_ObjFanin0(pNode))];
        int var1 = pCnf->pVarNums[Abc_ObjId(Abc_ObjFanin1(pNode))];

        lit Lits[2];
        Lits[0] = Abc_Var2Lit(var0, value0 == 0); // Constraint for fanin0
        Lits[1] = Abc_Var2Lit(var1, value1 == 0); // Constraint for fanin1

        // Debug: Print testing pattern
        Abc_Print(1, "Testing Pattern: %d%d\n", value0, value1);

        int status = sat_solver_solve(pSat, Lits, Lits + 2, 0, 0, 0, 0);
        if (status == l_False) { // If pattern is UNSAT, it is an SDC
            Abc_Print(1, "SDC: %d%d\n", value0, value1);
            foundSDC = true;
        }
    }

    // If no SDCs are found, print "no sdc"
    if (!foundSDC) {
        Abc_Print(1, "no sdc\n");
    }

    // Cleanup resources
    sat_solver_delete(pSat);
    Cnf_DataFree(pCnf);
    Aig_ManStop(pAig);
    Abc_NtkDelete(pConeNtk);
    Vec_PtrFree(vNodes);
}

// Command handler for `lsv_sdc`
static int Lsv_CommandSDC(Abc_Frame_t* pAbc, int argc, char** argv) {
    if (argc != 2) {
        Abc_Print(-1, "Usage: lsv_sdc <node_id>\n");
        return 1;
    }

    // Read the network and validate the node
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    if (!pNtk) {
        Abc_Print(-1, "No network available.\n");
        return 1;
    }

    int nodeId = atoi(argv[1]);
    Abc_Obj_t* pNode = Abc_NtkObj(pNtk, nodeId);
    if (!pNode) {
        Abc_Print(-1, "Error: Node ID %d does not exist.\n", nodeId);
        return 1;
    }
    if (Abc_ObjIsPi(pNode)) {
        Abc_Print(-1, "Error: Node ID %d is a Primary Input (PI).\n", nodeId);
        return 1;
    }
    if (Abc_ObjIsPo(pNode)) {
        Abc_Print(-1, "Error: Node ID %d is a Primary Output (PO).\n", nodeId);
        return 1;
    }

    // Compute SDCs for the node
    Lsv_ComputeSDC(pNtk, pNode);
    return 0;
}
