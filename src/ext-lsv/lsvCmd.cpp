#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/cnf/cnf.h"
#include "sat/bsat/satSolver.h"
#include <vector>
#include <random>
#include <set>

extern "C" {
    Aig_Man_t* Abc_NtkToDar(Abc_Ntk_t* pNtk, int fExors, int fRegisters);
}

// Function to perform random simulation
std::set<std::pair<int, int>> performRandomSimulation(Abc_Obj_t* pNode, int numSimulations) {
    std::set<std::pair<int, int>> observedPatterns;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 1);
    
    Abc_Obj_t* pFanin0 = Abc_ObjFanin0(pNode);
    Abc_Obj_t* pFanin1 = Abc_ObjFanin1(pNode);
    
    // Store original values
    std::vector<int> originalPIValues;
    Abc_Ntk_t* pNtk = Abc_ObjNtk(pNode);
    Abc_Obj_t* pObj;
    int i;
    
    Abc_NtkForEachPi(pNtk, pObj, i) {
        originalPIValues.push_back(pObj->fMarkA);
    }
    
    // Perform random simulation
    for (int sim = 0; sim < numSimulations; sim++) {
        // Assign random values to PIs
        Abc_NtkForEachPi(pNtk, pObj, i) {
            int val = dis(gen);
            pObj->fMarkA = val;
        }
        
        // Simulate network
        Abc_NtkForEachNode(pNtk, pObj, i) {
            int val0 = Abc_ObjFanin0(pObj)->fMarkA;
            int val1 = Abc_ObjFanin1(pObj)->fMarkA;
            pObj->fMarkA = (val0 ^ Abc_ObjFaninC0(pObj)) & 
                          (val1 ^ Abc_ObjFaninC1(pObj));
        }
        
        // Record observed pattern
        int val0 = pFanin0->fMarkA;
        int val1 = pFanin1->fMarkA;
        observedPatterns.insert(std::make_pair(val0, val1));
    }
    
    // Restore original values
    i = 0;
    Abc_NtkForEachPi(pNtk, pObj, i) {
        pObj->fMarkA = originalPIValues[i++];
    }
    
    return observedPatterns;
}

// Function to check if a pattern is possible using SAT
bool isPatternPossible(Abc_Obj_t* pNode, int val0, int val1) {
    Abc_Ntk_t* pNtk = Abc_ObjNtk(pNode);
    
    // Create cone for the node
    Abc_Ntk_t* pCone = Abc_NtkCreateCone(pNtk, pNode, Abc_ObjName(pNode), 1);
    if (!pCone) return false;
    
    // Convert to AIG
    Aig_Man_t* pAig = Abc_NtkToDar(pCone, 0, 0);
    if (!pAig) {
        Abc_NtkDelete(pCone);
        return false;
    }
    
    // Create CNF
    Cnf_Dat_t* pCnf = Cnf_Derive(pAig, 0);
    if (!pCnf) {
        Aig_ManStop(pAig);
        Abc_NtkDelete(pCone);
        return false;
    }
    
    // Create SAT solver
    sat_solver* pSat = sat_solver_new();
    sat_solver_setnvars(pSat, pCnf->nVars);
    
    // Add clauses to SAT solver
    for (int i = 0; i < pCnf->nClauses; i++) {
        lit* begin = pCnf->pClauses[i];
        lit* end = pCnf->pClauses[i] + (pCnf->pClauses[i+1] - pCnf->pClauses[i]);
        if (!sat_solver_addclause(pSat, begin, end))
        {
            sat_solver_delete(pSat);
            Cnf_DataFree(pCnf);
            Aig_ManStop(pAig);
            Abc_NtkDelete(pCone);
            return false;
        }
    }
    
    // Add constraints for fanin values
    Abc_Obj_t* pFanin0 = Abc_ObjFanin0(pNode);
    Abc_Obj_t* pFanin1 = Abc_ObjFanin1(pNode);
    
    int Var0 = pCnf->pVarNums[Abc_ObjId(pFanin0)];
    int Var1 = pCnf->pVarNums[Abc_ObjId(pFanin1)];
    
    lit Lits[1];
    
    // Add clause for first fanin
    Lits[0] = toLitCond(Var0, val0 == 0);
    if (!sat_solver_addclause(pSat, Lits, Lits + 1))
    {
        sat_solver_delete(pSat);
        Cnf_DataFree(pCnf);
        Aig_ManStop(pAig);
        Abc_NtkDelete(pCone);
        return false;
    }
    
    // Add clause for second fanin
    Lits[0] = toLitCond(Var1, val1 == 0);
    if (!sat_solver_addclause(pSat, Lits, Lits + 1))
    {
        sat_solver_delete(pSat);
        Cnf_DataFree(pCnf);
        Aig_ManStop(pAig);
        Abc_NtkDelete(pCone);
        return false;
    }
    
    // Solve SAT problem
    int status = sat_solver_solve(pSat, NULL, NULL, 0, 0, 0, 0);
    
    // Clean up
    sat_solver_delete(pSat);
    Cnf_DataFree(pCnf);
    Aig_ManStop(pAig);
    Abc_NtkDelete(pCone);
    
    return status == l_True;
}

// Main command function
int Lsv_CommandSdc(Abc_Frame_t* pAbc, int argc, char** argv) {
    Abc_Ntk_t* pNtk;
    int c;
    
    pNtk = Abc_FrameReadNtk(pAbc);
    Extra_UtilGetoptReset();
    
    while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
        switch (c) {
            case 'h':
                Abc_Print(-2, "usage: lsv_sdc <node>\n");
                Abc_Print(-2, "\t        compute satisfiability don't cares for the specified node\n");
                Abc_Print(-2, "\t-h    : print the command usage\n");
                return 1;
            default:
                Abc_Print(-2, "usage: lsv_sdc <node>\n");
                return 1;
        }
    }
    
    if (argc != 2) {
        Abc_Print(-1, "Missing node index.\n");
        Abc_Print(-2, "usage: lsv_sdc <node>\n");
        return 1;
    }
    
    if (!pNtk) {
        Abc_Print(-1, "Empty network.\n");
        return 1;
    }
    
    // Get node index from argument
    int nodeIndex = atoi(argv[1]);
    Abc_Obj_t* pNode = Abc_NtkObj(pNtk, nodeIndex);
    
    if (!pNode || !Abc_ObjIsNode(pNode)) {
        Abc_Print(-1, "Invalid node index.\n");
        return 1;
    }
    
    // Perform random simulation
    std::set<std::pair<int, int>> observedPatterns = performRandomSimulation(pNode, 1000);
    
    // Check unobserved patterns using SAT
    std::vector<std::pair<int, int>> sdcPatterns;
    std::vector<std::pair<int, int>> allPatterns = {{0,0}, {0,1}, {1,0}, {1,1}};
    
    for (const auto& pattern : allPatterns) {
        if (observedPatterns.find(pattern) == observedPatterns.end()) {
            if (!isPatternPossible(pNode, pattern.first, pattern.second)) {
                sdcPatterns.push_back(pattern);
            }
        }
    }
    
    // Output results
    if (sdcPatterns.empty()) {
        Abc_Print(1, "no sdc\n");
    } else {
        for (const auto& pattern : sdcPatterns) {
            Abc_Print(1, "%d%d\n", pattern.first, pattern.second);
        }
    }
    
    return 0;
}

// Command registration
void init(Abc_Frame_t* pAbc) {
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sdc", Lsv_CommandSdc, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
    PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;