#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/cnf/cnf.h"
#include "sat/bsat/satSolver.h"
#include <vector>
#include <set>
#include <map>
#include <iostream>

extern "C" {
    Aig_Man_t* Abc_NtkToDar(Abc_Ntk_t* pNtk, int fExors, int fRegisters);
}

bool isPatternPossible(Abc_Obj_t* pNode, int val0, int val1) { 
    Abc_Ntk_t* pNtk = Abc_ObjNtk(pNode);
    Abc_Obj_t* pFanin0 = Abc_ObjFanin0(pNode);
    Abc_Obj_t* pFanin1 = Abc_ObjFanin1(pNode);
    
    Vec_Ptr_t* vNodes = Vec_PtrAlloc(2);
    Vec_PtrPush(vNodes, pFanin0);
    Vec_PtrPush(vNodes, pFanin1);
    
    Abc_Ntk_t* pCone = Abc_NtkCreateConeArray(pNtk, vNodes, 1);
    Vec_PtrFree(vNodes);
    if (!pCone) return true;
    
    Aig_Man_t* pAig = Abc_NtkToDar(pCone, 0, 0);
    Abc_NtkDelete(pCone);
    if (!pAig) return true;
    
    sat_solver* pSat = sat_solver_new();
    int nVars = 2 * Aig_ManObjNumMax(pAig);
    sat_solver_setnvars(pSat, nVars);
    
    std::map<int, int> nodeToVar;
    int varCounter = 1;
    
    // Map combinational inputs to variables
    Aig_Obj_t* pObj;
    int i;
    Aig_ManForEachCi(pAig, pObj, i) {
        nodeToVar[Aig_ObjId(pObj)] = varCounter++;
    }
    
    std::vector<std::vector<lit>> allClauses;
    
    // When creating clauses for AND gates, store them in allClauses
    Aig_ManForEachNode(pAig, pObj, i) {
        int outVar = varCounter++;
        nodeToVar[Aig_ObjId(pObj)] = outVar;
        
        int in0Var = nodeToVar[Aig_ObjFaninId0(pObj)];
        int in1Var = nodeToVar[Aig_ObjFaninId1(pObj)];
        bool isIn0Compl = Aig_ObjFaninC0(pObj);
        bool isIn1Compl = Aig_ObjFaninC1(pObj);
        
        // Create clauses for AND gate
        lit Lits[3];
        
        // First clause: (!out + in0)
        std::vector<lit> clause1 = {toLitCond(outVar, 1), toLitCond(in0Var, isIn0Compl)};
        allClauses.push_back(clause1);
        Lits[0] = clause1[0];
        Lits[1] = clause1[1];
        if (!sat_solver_addclause(pSat, Lits, Lits + 2)) {
            sat_solver_delete(pSat);
            Aig_ManStop(pAig);
            return false;
        }
        
        // Second clause: (!out + in1)
        std::vector<lit> clause2 = {toLitCond(outVar, 1), toLitCond(in1Var, isIn1Compl)};
        allClauses.push_back(clause2);
        Lits[0] = clause2[0];
        Lits[1] = clause2[1];
        if (!sat_solver_addclause(pSat, Lits, Lits + 2)) {
            sat_solver_delete(pSat);
            Aig_ManStop(pAig);
            return false;
        }
        
        // Third clause: (out + !in0 + !in1)
        std::vector<lit> clause3 = {toLitCond(outVar, 0), 
                                   toLitCond(in0Var, !isIn0Compl),
                                   toLitCond(in1Var, !isIn1Compl)};
        allClauses.push_back(clause3);
        Lits[0] = clause3[0];
        Lits[1] = clause3[1];
        Lits[2] = clause3[2];
        if (!sat_solver_addclause(pSat, Lits, Lits + 3)) {
            sat_solver_delete(pSat);
            Aig_ManStop(pAig);
            return false;
        }
    }
    
    // Add output constraints
    int fanin0Compl = Abc_ObjFaninC0(pNode);  // Get complemented status of first fanin
    int fanin1Compl = Abc_ObjFaninC1(pNode);  // Get complemented status of second fanin

    Aig_ManForEachCo(pAig, pObj, i) {
        int driverId = Aig_ObjFaninId0(pObj);
        int isCompl = Aig_ObjFaninC0(pObj);
        int outVar = nodeToVar[driverId];
        
        lit Lits[1];
        if (i == 0) {
            // For first fanin: consider both the original fanin complement and AIG complement
            Lits[0] = toLitCond(outVar, !(val0 ^ fanin0Compl ^ isCompl));
        } else {
            // For second fanin
            Lits[0] = toLitCond(outVar, !(val1 ^ fanin1Compl ^ isCompl));
        }
        
        std::vector<lit> clause = {Lits[0]};
        allClauses.push_back(clause);
        
        if (!sat_solver_addclause(pSat, Lits, Lits + 1)) {
            sat_solver_delete(pSat);
            Aig_ManStop(pAig);
            return false;
        }
    }
    
    int status = sat_solver_solve(pSat, NULL, NULL, 0, 0, 0, 0);
    
    sat_solver_delete(pSat);
    Aig_ManStop(pAig);
    
    return status == l_True;
}

struct OdcResult {
    std::vector<std::pair<int, int>> odcPatterns;
    bool success;
};

// Node mapping structure and helper functions
typedef struct {
    Vec_Ptr_t* pOrigToAigMap;  // Maps original node IDs to AIG node IDs
    Vec_Ptr_t* pAigToOrigMap;  // Maps AIG node IDs to original node IDs
} NodeMapping_t;

// Initialize mapping structure
NodeMapping_t* CreateNodeMapping(Abc_Ntk_t* pNtk) {
    NodeMapping_t* pMapping = ABC_ALLOC(NodeMapping_t, 1);
    
    // Add some buffer space
    int maxId = Abc_NtkObjNumMax(pNtk) * 3; 
    pMapping->pOrigToAigMap = Vec_PtrStart(maxId);
    pMapping->pAigToOrigMap = Vec_PtrStart(maxId);
    return pMapping;
}

// Free mapping structure
void DeleteNodeMapping(NodeMapping_t* pMapping) {
    Vec_PtrFree(pMapping->pOrigToAigMap);
    Vec_PtrFree(pMapping->pAigToOrigMap);
    ABC_FREE(pMapping);
}

// Convert network to AIG while maintaining index mapping
Aig_Man_t* Abc_NtkToDarWithMapping(Abc_Ntk_t* pNtk, NodeMapping_t* pMapping) {
    Aig_Man_t* pAig;
    Abc_Obj_t* pObj;
    int i;
    
    pAig = Abc_NtkToDar(pNtk, 0, 0);
    if (!pAig) return NULL;
    
    Abc_NtkForEachObj(pNtk, pObj, i) {
        if (pObj->pCopy) {
            Vec_PtrWriteEntry(pMapping->pOrigToAigMap, i, pObj->pCopy);
            Vec_PtrWriteEntry(pMapping->pAigToOrigMap, 
                            Aig_ObjId((Aig_Obj_t*)pObj->pCopy), pObj);
        }
    }
    
    return pAig;
}

// Get AIG node ID from original node ID
int GetAigNodeId(NodeMapping_t* pMapping, int origId) {
    Aig_Obj_t* pAigObj = (Aig_Obj_t*)Vec_PtrEntry(pMapping->pOrigToAigMap, origId);
    return pAigObj ? Aig_ObjId(pAigObj) : -1;
}

// Main ODC calculation function
OdcResult calculateOdc(Abc_Ntk_t* pNtk, Abc_Obj_t* pNode) {
    OdcResult result;
    result.success = false;
    NodeMapping_t* pMapping = CreateNodeMapping(pNtk);

    // Store original fanin information
    Abc_Obj_t* pFanin0 = Abc_ObjFanin0(pNode);
    Abc_Obj_t* pFanin1 = Abc_ObjFanin1(pNode);
    int fanin0Id = Abc_ObjId(pFanin0);
    int fanin1Id = Abc_ObjId(pFanin1);
    bool isCompl0 = Abc_ObjFaninC0(pNode);
    bool isCompl1 = Abc_ObjFaninC1(pNode);
    
    // Create duplicate networks
    Abc_Ntk_t* pNtk1 = Abc_NtkDup(pNtk);
    if (!pNtk1) {
        DeleteNodeMapping(pMapping);
        return result;
    }
    
    Abc_Ntk_t* pNtk2 = Abc_NtkDup(pNtk);
    if (!pNtk2) {
        Abc_NtkDelete(pNtk1);
        DeleteNodeMapping(pMapping);
        return result;
    }
    
    // Get and complement target node in second network
    Abc_Obj_t* pNode2 = Abc_NtkObj(pNtk2, Abc_ObjId(pNode));
    Abc_Obj_t* pFanout;
    int i;
    
    // Complement the fanout of our target node
    Abc_ObjForEachFanout(pNode2, pFanout, i) {
        Abc_ObjXorFaninC(pFanout, Abc_ObjFanoutFaninNum(pFanout, pNode2));
    }
    
    // Create miter network
    Abc_Ntk_t* pMiter = Abc_NtkMiter(pNtk1, pNtk2, 1, 0, 0, 0);
    if (!pMiter) {
        Abc_NtkDelete(pNtk1);
        Abc_NtkDelete(pNtk2);
        DeleteNodeMapping(pMapping);
        return result;
    }
    
    // Convert to AIG with mapping
    Aig_Man_t* pAig = Abc_NtkToDarWithMapping(pMiter, pMapping);
    if (!pAig) {
        Abc_NtkDelete(pMiter);
        Abc_NtkDelete(pNtk1);
        Abc_NtkDelete(pNtk2);
        DeleteNodeMapping(pMapping);
        return result;
    }

    // Set up SAT solver
    sat_solver* pSat = sat_solver_new();
    int nVars = 2 * Aig_ManObjNumMax(pAig);
    sat_solver_setnvars(pSat, nVars);

    std::map<int, int> nodeToVar;
    int varCounter = 1;

    // Map AIG inputs to variables
    Aig_Obj_t* pObj;
    Aig_ManForEachCi(pAig, pObj, i) {
        nodeToVar[Aig_ObjId(pObj)] = varCounter++;
    }

    // Create clauses for AIG nodes
    std::vector<std::vector<lit>> allClauses;
    Aig_ManForEachNode(pAig, pObj, i) {
        int outVar = varCounter++;
        nodeToVar[Aig_ObjId(pObj)] = outVar;

        int in0Var = nodeToVar[Aig_ObjFaninId0(pObj)];
        int in1Var = nodeToVar[Aig_ObjFaninId1(pObj)];
        bool isIn0Compl = Aig_ObjFaninC0(pObj);
        bool isIn1Compl = Aig_ObjFaninC1(pObj);

        // Add clauses for AND gate
        lit Lits[3];
        
        // Clause 1: (!out + in0)
        std::vector<lit> clause1 = {toLitCond(outVar, 1), toLitCond(in0Var, isIn0Compl)};
        allClauses.push_back(clause1);
        Lits[0] = clause1[0];
        Lits[1] = clause1[1];
        if (!sat_solver_addclause(pSat, Lits, Lits + 2)) {
            sat_solver_delete(pSat);
            Aig_ManStop(pAig);
            Abc_NtkDelete(pMiter);
            Abc_NtkDelete(pNtk1);
            Abc_NtkDelete(pNtk2);
            DeleteNodeMapping(pMapping);
            return result;
        }

        // Second clause: (!out + in1)
        std::vector<lit> clause2 = {toLitCond(outVar, 1), toLitCond(in1Var, isIn1Compl)};
        allClauses.push_back(clause2);
        Lits[0] = clause2[0];
        Lits[1] = clause2[1];
        if (!sat_solver_addclause(pSat, Lits, Lits + 2)) {
            sat_solver_delete(pSat);
            Aig_ManStop(pAig);
            Abc_NtkDelete(pMiter);
            Abc_NtkDelete(pNtk1);
            Abc_NtkDelete(pNtk2);
            DeleteNodeMapping(pMapping);
            return result;
        }

        // Third clause: (out + !in0 + !in1)
        std::vector<lit> clause3 = {toLitCond(outVar, 0),
                                   toLitCond(in0Var, !isIn0Compl),
                                   toLitCond(in1Var, !isIn1Compl)};
        allClauses.push_back(clause3);
        Lits[0] = clause3[0];
        Lits[1] = clause3[1];
        Lits[2] = clause3[2];
        if (!sat_solver_addclause(pSat, Lits, Lits + 3)) {
            sat_solver_delete(pSat);
            Aig_ManStop(pAig);
            Abc_NtkDelete(pMiter);
            Abc_NtkDelete(pNtk1);
            Abc_NtkDelete(pNtk2);
            DeleteNodeMapping(pMapping);
            return result;
        }
    }

    Aig_ManForEachCo(pAig, pObj, i) {
        int driverId = Aig_ObjFaninId0(pObj);
        int outVar = nodeToVar[driverId];

        lit Lits[1];
        Lits[0] = toLitCond(outVar, 0);

        std::vector<lit> clause = {Lits[0]};
        allClauses.push_back(clause);

        if (!sat_solver_addclause(pSat, Lits, Lits + 1)) {
            sat_solver_delete(pSat);
            Aig_ManStop(pAig);
            Abc_NtkDelete(pMiter);
            Abc_NtkDelete(pNtk1);
            Abc_NtkDelete(pNtk2);
            DeleteNodeMapping(pMapping);
            return result;
        }
    }

    std::set<std::pair<int, int>> foundPatterns;

    // Check if we only have constant nodes or invalid clauses
    bool hasOnlyConstantNodes = true;
    for (const auto& clause : allClauses) {
        for (const auto& literal : clause) {
            if (Abc_Lit2Var(literal) > 0) {
                hasOnlyConstantNodes = false;
                break;
            }
        }
        if (!hasOnlyConstantNodes) break;
    }
    
    if (hasOnlyConstantNodes) {
        // Cleanup
        sat_solver_delete(pSat);
        Aig_ManStop(pAig);
        Abc_NtkDelete(pMiter);
        Abc_NtkDelete(pNtk1);
        Abc_NtkDelete(pNtk2);
        
        result.success = true;
        return result;
    }
    
    while (true) {
        int status = sat_solver_solve(pSat, NULL, NULL, 0, 0, 0, 0);
        if (status == l_False) break;
        
        if (status == l_True) {
            // Map original fanin IDs to AIG IDs with verification
            int aigFanin0Id = GetAigNodeId(pMapping, fanin0Id);
            int aigFanin1Id = GetAigNodeId(pMapping, fanin1Id);
            
            if (aigFanin0Id == -1 || aigFanin1Id == -1) {
                break;
            }
            
            // Get and verify SAT variables
            int var0 = nodeToVar[aigFanin0Id];
            int var1 = nodeToVar[aigFanin1Id];
            
            if (var0 == 0 || var1 == 0) {
                continue;
            }
            
            // Get current assignment
            int val0 = sat_solver_var_value(pSat, var0);
            int val1 = sat_solver_var_value(pSat, var1);
            
            // Apply complemented edges adjustment only for output display
            int displayVal0 = isCompl0 ? !val0 : val0;
            int displayVal1 = isCompl1 ? !val1 : val1;

            // Store pattern using original SAT values
            std::pair<int, int> pattern = {displayVal0, displayVal1};
            
            if (foundPatterns.find(pattern) == foundPatterns.end()) {
                foundPatterns.insert(pattern);
                result.odcPatterns.push_back(pattern);
                
                // Create blocking clause using original SAT values
                Vec_Int_t* vLits = Vec_IntAlloc(2);
                
                Vec_IntPush(vLits, Abc_Var2Lit(var0, val0));
                Vec_IntPush(vLits, Abc_Var2Lit(var1, val1));
                
                if (!sat_solver_addclause(pSat, Vec_IntArray(vLits), 
                    Vec_IntArray(vLits) + Vec_IntSize(vLits))) {
                    Vec_IntFree(vLits);
                    break;
                }
                Vec_IntFree(vLits);
            } else {
                break;
            }
        }
    }

    // Cleanup
    sat_solver_delete(pSat);
    Aig_ManStop(pAig);
    Abc_NtkDelete(pMiter);
    Abc_NtkDelete(pNtk1);
    Abc_NtkDelete(pNtk2);

    result.success = true;
    return result;
}

int Lsv_CommandSdc(Abc_Frame_t* pAbc, int argc, char** argv) {
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    int c;
    
    Extra_UtilGetoptReset();
    while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
        switch (c) {
            case 'h':
                Abc_Print(-2, "usage: lsv_sdc <node>\n");
                Abc_Print(-2, "\t        compute satisfiability don't cares for the specified node\n");
                Abc_Print(-2, "\t-h    : print the command usage\n");
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
    
    int nodeIndex = atoi(argv[1]);
    Abc_Obj_t* pNode = Abc_NtkObj(pNtk, nodeIndex);
    
    if (!pNode || !Abc_ObjIsNode(pNode)) {
        Abc_Print(-1, "Invalid node index.\n");
        return 1;
    }
    
    std::vector<std::pair<int, int>> sdcPatterns;
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            if (!isPatternPossible(pNode, i, j)) {
                sdcPatterns.push_back({i, j});
            }
        }
    }
    
    if (sdcPatterns.empty()) {
        Abc_Print(1, "no sdc\n");
    } else {
        for (const auto& pattern : sdcPatterns) {
            Abc_Print(1, "%d%d\n", pattern.first, pattern.second);
        }
    }
    
    return 0;
}

int Lsv_CommandOdc(Abc_Frame_t* pAbc, int argc, char** argv) {
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    int c;
    
    Extra_UtilGetoptReset();
    while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
        switch (c) {
            case 'h':
                Abc_Print(-2, "usage: lsv_odc <node>\n");
                Abc_Print(-2, "\t        compute observability don't cares for the specified node\n");
                Abc_Print(-2, "\t-h    : print the command usage\n");
                return 1;
        }
    }
    
    if (argc != 2) {
        Abc_Print(-1, "Missing node index.\n");
        Abc_Print(-2, "usage: lsv_odc <node>\n");
        return 1;
    }
    
    if (!pNtk) {
        Abc_Print(-1, "Empty network.\n");
        return 1;
    }
    
    int nodeIndex = atoi(argv[1]);
    Abc_Obj_t* pNode = Abc_NtkObj(pNtk, nodeIndex);
    
    if (!pNode || !Abc_ObjIsNode(pNode)) {
        Abc_Print(-1, "Invalid node index.\n");
        return 1;
    }
    
    // Calculate ODCs
    OdcResult result = calculateOdc(pNtk, pNode);
    
    if (!result.success) {
        Abc_Print(-1, "Failed to compute ODCs.\n");
        return 1;
    }
    
    // Output results
    if (result.odcPatterns.empty()) {
        Abc_Print(1, "no odc\n");
    } else {
        for (const auto& pattern : result.odcPatterns) {
            Abc_Print(1, "%d%d\n", pattern.first, pattern.second);
        }
    }
    
    return 0;
}

void init(Abc_Frame_t* pAbc) {
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sdc", Lsv_CommandSdc, 0);
    Cmd_CommandAdd(pAbc, "LSV", "lsv_odc", Lsv_CommandOdc, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
    PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;