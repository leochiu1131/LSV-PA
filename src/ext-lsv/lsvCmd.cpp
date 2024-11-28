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

// void PrintConeStructure(Abc_Ntk_t* pCone) {
//     printf("\nCone Network Structure:\n");
//     printf("------------------------\n");
    
//     printf("Network Statistics:\n");
//     printf("  Nodes: %d\n", Abc_NtkNodeNum(pCone));
//     printf("  Primary Inputs: %d\n", Abc_NtkPiNum(pCone));
//     printf("  Primary Outputs: %d\n", Abc_NtkPoNum(pCone));
    
//     Abc_Obj_t* pObj;
//     int i;
    
//     printf("\nPrimary Inputs:\n");
//     Abc_NtkForEachPi(pCone, pObj, i) {
//         printf("  PI %d: Name = %s, ID = %d\n", 
//                i, Abc_ObjName(pObj), Abc_ObjId(pObj));
//     }
    
//     printf("\nNodes:\n");
//     Abc_NtkForEachNode(pCone, pObj, i) {
//         printf("  Node %d: Name = %s, ID = %d\n", 
//                i, Abc_ObjName(pObj), Abc_ObjId(pObj));
//         printf("    Fanins: ");
//         Abc_Obj_t* pFanin;
//         int j;
//         Abc_ObjForEachFanin(pObj, pFanin, j) {
//             printf("(%d%s) ", Abc_ObjId(pFanin), 
//                    Abc_ObjFaninC(pObj, j) ? "'" : "");
//         }
//         printf("\n");
//     }
    
//     printf("\nPrimary Outputs:\n");
//     Abc_NtkForEachPo(pCone, pObj, i) {
//         printf("  PO %d: Name = %s, ID = %d\n", 
//                i, Abc_ObjName(pObj), Abc_ObjId(pObj));
//         printf("    Driver: %d%s\n", 
//                Abc_ObjId(Abc_ObjFanin0(pObj)),
//                Abc_ObjFaninC0(pObj) ? "'" : "");
//     }
// }

// void PrintAigStructure(Aig_Man_t* pAig) {
//     printf("\nAIG Structure:\n");
//     printf("---------------\n");
//     printf("Statistics:\n");
//     printf("  Total nodes: %d\n", Aig_ManNodeNum(pAig));
//     printf("  Total objects: %d\n", Aig_ManObjNum(pAig));
//     printf("  Constant nodes: %d\n", Aig_ManConst1(pAig) != NULL);
    
//     Aig_Obj_t* pObj;
//     int i;
    
//     printf("\nObjects by type:\n");
//     Aig_ManForEachObj(pAig, pObj, i) {
//         if (Aig_ObjIsConst1(pObj))
//             printf("  Const1 node: ID = %d\n", Aig_ObjId(pObj));
//         else if (Aig_ObjIsCi(pObj))
//             printf("  CI: ID = %d\n", Aig_ObjId(pObj));
//         else if (Aig_ObjIsCo(pObj))
//             printf("  CO: ID = %d, Driver = %d%s\n", 
//                    Aig_ObjId(pObj), Aig_ObjFaninId0(pObj),
//                    Aig_ObjFaninC0(pObj) ? "'" : "");
//         else if (Aig_ObjIsNode(pObj)) {
//             printf("  AND: ID = %d\n", Aig_ObjId(pObj));
//             printf("    Fanin0: %d%s\n", 
//                    Aig_ObjFaninId0(pObj),
//                    Aig_ObjFaninC0(pObj) ? "'" : "");
//             printf("    Fanin1: %d%s\n", 
//                    Aig_ObjFaninId1(pObj),
//                    Aig_ObjFaninC1(pObj) ? "'" : "");
//         }
//     }
// }

    void PrintCnfClauses(sat_solver* pSat, const std::map<int, int>& nodeToVar, const std::vector<std::vector<lit>>& allClauses) {
        printf("\nCNF Structure:\n");
        printf("--------------\n");
        printf("Total Variables: %d\n", sat_solver_nvars(pSat));
        
        printf("\nVariable Mapping (AIG Node -> CNF Variable):\n");
        for (const auto& pair : nodeToVar) {
            printf("  AIG Node %d -> CNF Var %d\n", pair.first, pair.second);
        }
        
        printf("\nClauses:\n");
        for (size_t i = 0; i < allClauses.size(); i++) {
            printf("Clause %zu: ", i);
            for (lit literal : allClauses[i]) {
                int var = lit_var(literal);
                int sign = lit_sign(literal);
                
                // Find corresponding AIG node if it exists
                int aigNode = -1;
                for (const auto& pair : nodeToVar) {
                    if (pair.second == var) {
                        aigNode = pair.first;
                        break;
                    }
                }
                
                if (aigNode != -1) {
                    printf("%s%d(AIG:%d) ", sign ? "¬" : "", var, aigNode);
                } else {
                    printf("%s%d ", sign ? "¬" : "", var);
                }
            }
            printf("\n");
        }
    }

bool isPatternPossible(Abc_Obj_t* pNode, int val0, int val1) {
    //printf("\nChecking pattern (%d,%d) for node %d\n", val0, val1, Abc_ObjId(pNode));
    
    Abc_Ntk_t* pNtk = Abc_ObjNtk(pNode);
    Abc_Obj_t* pFanin0 = Abc_ObjFanin0(pNode);
    Abc_Obj_t* pFanin1 = Abc_ObjFanin1(pNode);
    
    Vec_Ptr_t* vNodes = Vec_PtrAlloc(2);
    Vec_PtrPush(vNodes, pFanin0);
    Vec_PtrPush(vNodes, pFanin1);
    
    Abc_Ntk_t* pCone = Abc_NtkCreateConeArray(pNtk, vNodes, 1);
    //PrintConeStructure(pCone);
    Vec_PtrFree(vNodes);
    if (!pCone) return true;
    
    Aig_Man_t* pAig = Abc_NtkToDar(pCone, 0, 0);
    //PrintAigStructure(pAig);
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

        //std::cout << "DriverID: " << driverId << ", isCompl: " << isCompl << ", outVar: " << outVar << ", fanin0Compl: " << fanin0Compl << ", fanin1Compl: " << fanin1Compl << std::endl;
        
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
    
    //PrintCnfClauses(pSat, nodeToVar, allClauses);
    
    int status = sat_solver_solve(pSat, NULL, NULL, 0, 0, 0, 0);
    //printf("SAT solver status: %s\n", status == l_True ? "SAT" : "UNSAT");
    
    sat_solver_delete(pSat);
    Aig_ManStop(pAig);
    
    return status == l_True;
}

struct OdcResult {
    std::vector<std::pair<int, int>> odcPatterns;
    bool success;
};

void PrintMiterStructure(Abc_Ntk_t* pMiter) {
    printf("\nMiter Network Structure:\n");
    printf("------------------------\n");
    printf("Network Statistics:\n");
    printf("  Nodes: %d\n", Abc_NtkNodeNum(pMiter));
    printf("  Primary Inputs: %d\n", Abc_NtkPiNum(pMiter));
    printf("  Primary Outputs: %d\n", Abc_NtkPoNum(pMiter));
    
    Abc_Obj_t* pObj;
    int i;
    
    printf("\nPrimary Inputs:\n");
    Abc_NtkForEachPi(pMiter, pObj, i) {
        printf("  PI %d: Name = %s, ID = %d\n", 
               i, Abc_ObjName(pObj), Abc_ObjId(pObj));
    }
    
    printf("\nNodes:\n");
    Abc_NtkForEachNode(pMiter, pObj, i) {
        printf("  Node %d: Name = %s, ID = %d\n", 
               i, Abc_ObjName(pObj), Abc_ObjId(pObj));
        printf("    Fanins: ");
        Abc_Obj_t* pFanin;
        int j;
        Abc_ObjForEachFanin(pObj, pFanin, j) {
            printf("(%d%s) ", Abc_ObjId(pFanin), 
                   Abc_ObjFaninC(pObj, j) ? "'" : "");
        }
        printf("\n");
    }
}

void PrintAigStructure(Aig_Man_t* pAig) {
    printf("\nAIG Structure:\n");
    printf("---------------\n");
    printf("Statistics:\n");
    printf("  Total nodes: %d\n", Aig_ManNodeNum(pAig));
    printf("  Total objects: %d\n", Aig_ManObjNum(pAig));
    printf("  Constant nodes: %d\n", Aig_ManConst1(pAig) != NULL);
    
    Aig_Obj_t* pObj;
    int i;
    
    printf("\nObjects by type:\n");
    Aig_ManForEachObj(pAig, pObj, i) {
        if (Aig_ObjIsConst1(pObj))
            printf("  Const1 node: ID = %d\n", Aig_ObjId(pObj));
        else if (Aig_ObjIsCi(pObj))
            printf("  CI: ID = %d\n", Aig_ObjId(pObj));
        else if (Aig_ObjIsCo(pObj))
            printf("  CO: ID = %d, Driver = %d%s\n", 
                   Aig_ObjId(pObj), Aig_ObjFaninId0(pObj),
                   Aig_ObjFaninC0(pObj) ? "'" : "");
        else if (Aig_ObjIsNode(pObj)) {
            printf("  AND: ID = %d\n", Aig_ObjId(pObj));
            printf("    Fanin0: %d%s\n", 
                   Aig_ObjFaninId0(pObj),
                   Aig_ObjFaninC0(pObj) ? "'" : "");
            printf("    Fanin1: %d%s\n", 
                   Aig_ObjFaninId1(pObj),
                   Aig_ObjFaninC1(pObj) ? "'" : "");
        }
    }
}

void PrintCnfInfo(Cnf_Dat_t* pCnf) {
    printf("\nCNF Structure:\n");
    printf("--------------\n");
    printf("Number of variables: %d\n", pCnf->nVars);
    printf("Number of clauses: %d\n", pCnf->nClauses);
    printf("\nVariable mapping (AIG Node -> CNF Variable):\n");
    for (int i = 0; i < pCnf->nVars; i++) {
        if (pCnf->pVarNums[i] != -1) {
            printf("  AIG Node %d -> CNF Var %d\n", i, pCnf->pVarNums[i]);
        }
    }
    
    printf("\nFirst few clauses:\n");
    for (int i = 0; i < std::min(5, pCnf->nClauses); i++) {
        printf("Clause %d: ", i);
        int* pClause = pCnf->pClauses[i];
        int j = 0;
        while (pClause[j]) {
            printf("%s%d ", (pClause[j] & 1) ? "¬" : "", pClause[j]/2);
            j++;
        }
        printf("\n");
    }
}

void PrintConeStructure(Abc_Ntk_t* pCone, const char* title) {
    printf("\n%s Network Structure:\n", title);
    printf("------------------------\n");
    printf("Network Statistics:\n");
    printf("  Nodes: %d\n", Abc_NtkNodeNum(pCone));
    printf("  Primary Inputs: %d\n", Abc_NtkPiNum(pCone));
    printf("  Primary Outputs: %d\n", Abc_NtkPoNum(pCone));
    
    Abc_Obj_t* pObj;
    int i;
    
    printf("\nPrimary Inputs:\n");
    Abc_NtkForEachPi(pCone, pObj, i) {
        printf("  PI %d: Name = %s, ID = %d\n", 
               i, Abc_ObjName(pObj), Abc_ObjId(pObj));
    }
    
    printf("\nNodes:\n");
    Abc_NtkForEachNode(pCone, pObj, i) {
        printf("  Node %d: Name = %s, ID = %d\n", 
               i, Abc_ObjName(pObj), Abc_ObjId(pObj));
        printf("    Fanins: ");
        Abc_Obj_t* pFanin;
        int j;
        Abc_ObjForEachFanin(pObj, pFanin, j) {
            printf("(%d%s) ", Abc_ObjId(pFanin), 
                   Abc_ObjFaninC(pObj, j) ? "'" : "");
        }
        printf("\n");
    }
    
    printf("\nPrimary Outputs:\n");
    Abc_NtkForEachPo(pCone, pObj, i) {
        printf("  PO %d: Name = %s, ID = %d\n", 
               i, Abc_ObjName(pObj), Abc_ObjId(pObj));
        Abc_Obj_t* pDriver = Abc_ObjFanin0(pObj);
        printf("    Driver: %d%s\n", 
               Abc_ObjId(pDriver),
               Abc_ObjFaninC0(pObj) ? "'" : "");
    }
}

// Node mapping structure and helper functions
typedef struct {
    Vec_Ptr_t* pOrigToAigMap;  // Maps original node IDs to AIG node IDs
    Vec_Ptr_t* pAigToOrigMap;  // Maps AIG node IDs to original node IDs
} NodeMapping_t;

// Initialize mapping structure
NodeMapping_t* CreateNodeMapping() {
    NodeMapping_t* pMapping = ABC_ALLOC(NodeMapping_t, 1);
    pMapping->pOrigToAigMap = Vec_PtrStart(1000);
    pMapping->pAigToOrigMap = Vec_PtrStart(1000);
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
    NodeMapping_t* pMapping = CreateNodeMapping();
    
    //printf("\n=== Starting ODC Calculation for Node %d ===\n", Abc_ObjId(pNode));

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
    
    //printf("\nSecond Network Before Complementation:\n");
    //PrintConeStructure(pNtk2, "Second");
    
    // Complement the fanout of our target node
    Abc_ObjForEachFanout(pNode2, pFanout, i) {
        //printf("Complementing fanout %d of node %d\n", 
               //Abc_ObjId(pFanout), Abc_ObjId(pNode2));
        Abc_ObjXorFaninC(pFanout, Abc_ObjFanoutFaninNum(pFanout, pNode2));
    }
    
    //printf("\nSecond Network After Complementation:\n");
    //PrintConeStructure(pNtk2, "Second");
    
    // Create miter network
    //printf("\nCreating miter network\n");
    Abc_Ntk_t* pMiter = Abc_NtkMiter(pNtk1, pNtk2, 1, 0, 0, 0);
    if (!pMiter) {
        Abc_NtkDelete(pNtk1);
        Abc_NtkDelete(pNtk2);
        DeleteNodeMapping(pMapping);
        return result;
    }
    //PrintMiterStructure(pMiter);
    
    // Convert to AIG with mapping
    Aig_Man_t* pAig = Abc_NtkToDarWithMapping(pMiter, pMapping);
    if (!pAig) {
        Abc_NtkDelete(pMiter);
        Abc_NtkDelete(pNtk1);
        Abc_NtkDelete(pNtk2);
        DeleteNodeMapping(pMapping);
        return result;
    }
    //PrintAigStructure(pAig);

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

    // Step 6: Print CNF clauses for debugging
    //PrintCnfClauses(pSat, nodeToVar, allClauses);

    // Step 7: Find all care patterns using ALLSAT
    //printf("\nFinding non-ODC patterns\n");
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
        //printf("Only constant nodes in SAT instance - no ODC patterns possible\n");
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
            
            //printf("Mapping verification:\n");
            //printf("Original fanin0 (ID: %d) -> AIG node (ID: %d)\n", fanin0Id, aigFanin0Id);
            //printf("Original fanin1 (ID: %d) -> AIG node (ID: %d)\n", fanin1Id, aigFanin1Id);
            
            if (aigFanin0Id == -1 || aigFanin1Id == -1) {
                //printf("Error: Failed to map original nodes to AIG nodes\n");
                break;
            }
            
            // Get and verify SAT variables
            int var0 = nodeToVar[aigFanin0Id];
            int var1 = nodeToVar[aigFanin1Id];
            
            //printf("AIG to SAT variable mapping:\n");
            //printf("AIG node %d -> SAT var %d\n", aigFanin0Id, var0);
            //printf("AIG node %d -> SAT var %d\n", aigFanin1Id, var1);
            
            if (var0 == 0 || var1 == 0) {
                printf("Error: Invalid SAT variable mapping\n");
                continue;
            }
            
            // Get current assignment
            int val0 = sat_solver_var_value(pSat, var0);
            int val1 = sat_solver_var_value(pSat, var1);
            
            //printf("Current assignment before complement adjustment:\n");
            //printf("var%d = %d, var%d = %d\n", var0, val0, var1, val1);
            
            // Apply complemented edges adjustment only for output display
            int displayVal0 = isCompl0 ? !val0 : val0;
            int displayVal1 = isCompl1 ? !val1 : val1;

            // Store pattern using original SAT values
            std::pair<int, int> pattern = {displayVal0, displayVal1};
            
            //printf("Final values after complement adjustment (for display):\n");
            //printf("val0 = %d, val1 = %d\n", displayVal0, displayVal1);
            
            if (foundPatterns.find(pattern) == foundPatterns.end()) {
                foundPatterns.insert(pattern);
                result.odcPatterns.push_back(pattern);
                
                // Create blocking clause using original SAT values
                Vec_Int_t* vLits = Vec_IntAlloc(2);
                
                //printf("Adding blocking clause:\n");
                //printf("Literal 1: var %d = %d\n", var0, val0);
                //printf("Literal 2: var %d = %d\n", var1, val1);
                
                Vec_IntPush(vLits, Abc_Var2Lit(var0, val0));
                Vec_IntPush(vLits, Abc_Var2Lit(var1, val1));
                
                if (!sat_solver_addclause(pSat, Vec_IntArray(vLits), 
                    Vec_IntArray(vLits) + Vec_IntSize(vLits))) {
                    //printf("Failed to add blocking clause\n");
                    Vec_IntFree(vLits);
                    break;
                }
                
                //printf("Successfully added blocking clause\n");
                Vec_IntFree(vLits);
            } else {
                //printf("Pattern already found, terminating search\n");
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