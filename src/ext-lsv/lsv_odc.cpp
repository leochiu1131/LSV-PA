/*
 * 
 * The computation of Observability Don't Cares (ODCs) begins with extracting the logic cone of the target node. 
 * The logic cone includes all transitive fanins that directly or indirectly affect the node's output. 
 * By isolating this sub-network using the Abc_NtkCreateCone function, the computation focuses solely on the portion of the circuit relevant to the node, 
 * reducing complexity and ensuring precision. This step is essential for identifying the inputs that influence the behavior of the node.
 * 
 * Once the cone network is created, a miter network is constructed to compare the original logic of the node with its negated counterpart. 
 * This involves duplicating the cone network and negating its primary output using Abc_ObjXorFaninC. The miter network, formed using Abc_NtkMiter, 
 * outputs 1 whenever there is a discrepancy between the original and negated node outputs for the same input pattern. 
 * This approach effectively captures differences in observability, forming the basis for identifying ODC patterns.
 * 
 * To find ODC patterns, the miter network is converted into a SAT-compatible CNF representation using Cnf_Derive. 
 * A SAT solver is then employed to iteratively identify input patterns that create discrepancies between the original and negated outputs. 
 * For each discovered pattern, a blocking clause is added to the SAT solver, ensuring it is excluded in subsequent iterations. 
 * This process continues until no more patterns can be found or a predefined iteration limit is reached. Despite its logical framework,
 * challenges such as invalid CNF mappings and ineffective blocking clauses hinder the successful detection of valid ODCs in the current implementation.
 *
 * However, the implementation does not work as expected due to:
 * - Issues in CNF variable mapping, leading to invalid configurations.
 * - Ineffective blocking clauses, causing repeated patterns.
 * - Logical inconsistencies resulting in errors and failed ODC detection.
 *
 * The program terminates either when the maximum iteration count is reached or 
 * when all patterns are blocked.
 */

#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/cnf/cnf.h"
#include "sat/bsat/satSolver.h"

extern "C" {
Aig_Man_t* Abc_NtkToDar(Abc_Ntk_t* pNtk, int fExors, int fRegisters);
}

// Function prototype for the command and computation
static int Lsv_CommandODC(Abc_Frame_t* pAbc, int argc, char** argv);
static void Lsv_ComputeODC(Abc_Ntk_t* pNtk, Abc_Obj_t* pNode);

// Initialization of the command in ABC
void init(Abc_Frame_t* pAbc) {
    Cmd_CommandAdd(pAbc, "LSV", "lsv_odc", Lsv_CommandODC, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};
struct PackageRegistrationManager {
    PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

// Main function to compute ODCs for a given node
static void Lsv_ComputeODC(Abc_Ntk_t* pNtk, Abc_Obj_t* pNode) {
    if (!pNtk || !pNode) {
        Abc_Print(-1, "Invalid network or node.\n");
        return;
    }

    Abc_Print(1, "Processing Node for ODC: %d\n", Abc_ObjId(pNode));

    // Validate fanin nodes for the target node
    Abc_Obj_t* pFanin0 = Abc_ObjFanin0(pNode);
    Abc_Obj_t* pFanin1 = Abc_ObjFanin1(pNode);
    if (!pFanin0 || !pFanin1) {
        Abc_Print(-1, "Error: Node %d has invalid or missing fanins.\n", Abc_ObjId(pNode));
        return;
    }

    Abc_Print(1, "Fanin0 ID: %d, Fanin1 ID: %d\n", Abc_ObjId(pFanin0), Abc_ObjId(pFanin1));

    // Create the cone network for the node
    Abc_Ntk_t* pConeNtk = Abc_NtkCreateCone(pNtk, pNode, "cone", 1);
    if (!pConeNtk) {
        Abc_Print(-1, "Failed to create cone network.\n");
        return;
    }

    // Ensure the fanins are included in the cone network
    if (!Abc_NtkObj(pConeNtk, Abc_ObjId(pFanin0)) || !Abc_NtkObj(pConeNtk, Abc_ObjId(pFanin1))) {
        Abc_Print(-1, "Error: Fanins are not included in the cone network.\n");
        Abc_NtkDelete(pConeNtk);
        return;
    }

    // Duplicate the cone network and negate its primary output
    Abc_Ntk_t* pNegatedConeNtk = Abc_NtkDup(pConeNtk);
    if (!pNegatedConeNtk) {
        Abc_Print(-1, "Failed to duplicate cone network.\n");
        Abc_NtkDelete(pConeNtk);
        return;
    }

    // Negate the primary output of the duplicated cone
    Abc_Obj_t* pPo = Abc_NtkPo(pNegatedConeNtk, 0);
    Abc_ObjXorFaninC(pPo, 0); // Negate the fanin of the output

    // Create a miter network to compare the original and negated cones
    Abc_Ntk_t* pMiterNtk = Abc_NtkMiter(pConeNtk, pNegatedConeNtk, 1, 0, 0, 0);
    if (!pMiterNtk) {
        Abc_Print(-1, "Failed to create miter network.\n");
        Abc_NtkDelete(pConeNtk);
        Abc_NtkDelete(pNegatedConeNtk);
        return;
    }

    // Convert the miter network to an AIG
    Aig_Man_t* pAig = Abc_NtkToDar(pMiterNtk, 0, 0);
    if (!pAig) {
        Abc_Print(-1, "Failed to create AIG for miter network.\n");
        Abc_NtkDelete(pConeNtk);
        Abc_NtkDelete(pNegatedConeNtk);
        Abc_NtkDelete(pMiterNtk);
        return;
    }

    // Derive CNF from the AIG representation
    Cnf_Dat_t* pCnf = Cnf_Derive(pAig, Aig_ManCoNum(pAig));
    if (!pCnf) {
        Abc_Print(-1, "Failed to derive CNF for miter network.\n");
        Aig_ManStop(pAig);
        Abc_NtkDelete(pConeNtk);
        Abc_NtkDelete(pNegatedConeNtk);
        Abc_NtkDelete(pMiterNtk);
        return;
    }

    // Retrieve and validate CNF variable mappings for fanins
    int varY0 = pCnf->pVarNums[Abc_ObjId(pFanin0)];
    int varY1 = pCnf->pVarNums[Abc_ObjId(pFanin1)];
    Abc_Print(1, "Mapped variables: varY0=%d, varY1=%d\n", varY0, varY1);
    if (varY0 < 0 || varY0 >= pCnf->nVars || varY1 < 0 || varY1 >= pCnf->nVars) {
        Abc_Print(-1, "Error: Invalid CNF mapping for fanins: varY0=%d, varY1=%d\n", varY0, varY1);
        Cnf_DataFree(pCnf);
        Aig_ManStop(pAig);
        Abc_NtkDelete(pConeNtk);
        Abc_NtkDelete(pNegatedConeNtk);
        Abc_NtkDelete(pMiterNtk);
        return;
    }

    // SAT solver setup and ODC pattern finding
    sat_solver* pSat = sat_solver_new();
    sat_solver_setnvars(pSat, pCnf->nVars);
    Cnf_DataWriteIntoSolver(pCnf, 1, 0);

    // Iterative SAT solving to identify ODC patterns
    Abc_Print(1, "Solving for ODCs...\n");
    Vec_Int_t* vBlockingClause = Vec_IntAlloc(2);
    bool foundODC = false;
    int maxIterations = 1000, iterationCount = 0;

    while (sat_solver_solve(pSat, NULL, NULL, 0, 0, 0, 0) == l_True) {
        // Check iteration limit to avoid infinite loops
        if (++iterationCount > maxIterations) {
            Abc_Print(-1, "Error: Exceeded maximum iterations, terminating.\n");
            break;
        }

        // Retrieve the current solution from the SAT solver
        int y0 = sat_solver_var_value(pSat, varY0);
        int y1 = sat_solver_var_value(pSat, varY1);

        // Print the found ODC pattern
        Abc_Print(1, "ODC Pattern: %d%d\n", y0, y1);
        foundODC = true;

        // Construct and add a blocking clause to prevent the same pattern
        Vec_IntClear(vBlockingClause);
        Vec_IntPush(vBlockingClause, Abc_Var2Lit(varY0, !y0)); // Negate y0
        Vec_IntPush(vBlockingClause, Abc_Var2Lit(varY1, !y1)); // Negate y1

        // Add the blocking clause to the SAT solver
        if (!sat_solver_addclause(pSat, Vec_IntArray(vBlockingClause), Vec_IntArray(vBlockingClause) + 2)) {
            Abc_Print(-1, "Error: Failed to add blocking clause.\n");
            break;
        }
    }

    // Handle case when no ODCs are found
    if (!foundODC) {
        Abc_Print(1, "no odc\n");
    }

    // Clean up allocated resources
    Vec_IntFree(vBlockingClause);
    sat_solver_delete(pSat);
    Cnf_DataFree(pCnf);
    Aig_ManStop(pAig);
    Abc_NtkDelete(pConeNtk);
    Abc_NtkDelete(pNegatedConeNtk);
    Abc_NtkDelete(pMiterNtk);
}

// Command handler for ODC computation
static int Lsv_CommandODC(Abc_Frame_t* pAbc, int argc, char** argv) {
    if (argc != 2) {
        Abc_Print(-1, "Usage: lsv_odc <node_id>\n");
        return 1;
    }

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

    Lsv_ComputeODC(pNtk, pNode);
    return 0;
}
