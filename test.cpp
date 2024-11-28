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

void PrintConeStructure(Abc_Ntk_t* pCone) {
    printf("\nCone Network Structure:\n");
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
        printf("    Driver: %d%s\n", 
               Abc_ObjId(Abc_ObjFanin0(pObj)),
               Abc_ObjFaninC0(pObj) ? "'" : "");
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

// ODC Implementation
void PrintOdcStructure(Abc_Ntk_t* pNtk1, Abc_Ntk_t* pNtk2, Abc_Ntk_t* pMiter) {
    printf("\nNetwork 1 Statistics:\n");
    printf("  Nodes: %d\n", Abc_NtkNodeNum(pNtk1));
    printf("  PIs: %d\n", Abc_NtkPiNum(pNtk1));
    printf("  POs: %d\n", Abc_NtkPoNum(pNtk1));
    
    printf("\nNetwork 2 Statistics:\n");
    printf("  Nodes: %d\n", Abc_NtkNodeNum(pNtk2));
    printf("  PIs: %d\n", Abc_NtkPiNum(pNtk2));
    printf("  POs: %d\n", Abc_NtkPoNum(pNtk2));
    
    printf("\nMiter Statistics:\n");
    printf("  Nodes: %d\n", Abc_NtkNodeNum(pMiter));
    printf("  PIs: %d\n", Abc_NtkPiNum(pMiter));
    printf("  POs: %d\n", Abc_NtkPoNum(pMiter));
}

Abc_Ntk_t* CreateOdcMiter(Abc_Ntk_t* pNtk, Abc_Obj_t* pNode) {
    Abc_Ntk_t* pNtk1 = Abc_NtkDup(pNtk);
    Abc_Ntk_t* pNtk2 = Abc_NtkDup(pNtk);
    
    Abc_Obj_t* pNodeCopy = Abc_NtkObj(pNtk2, pNode->Id);
    Abc_ObjXorFaninC(pNodeCopy, 0);
    
    Abc_Ntk_t* pMiter = Abc_NtkMiter(pNtk1, pNtk2, 1, 0, 0, 0);
    
    Abc_NtkDelete(pNtk1);
    Abc_NtkDelete(pNtk2);
    
    return pMiter;
}

std::vector<std::pair<int, int>> ComputeOdc(Abc_Ntk_t* pNtk, Abc_Obj_t* pNode) {
    std::vector<std::pair<int, int>> odcPatterns;
    printf("\nStarting ODC computation for node %d\n", Abc_ObjId(pNode));
    
    // Find the fanout node
    Abc_Obj_t* pFanout = Abc_ObjFanout0(pNode);
    if (!pFanout || !Abc_ObjIsNode(pFanout)) {
        printf("Node does not have a valid fanout\n");
        return odcPatterns;
    }
    
    // Get the other input to the fanout node
    Abc_Obj_t* pOtherFanin;
    int isFaninComplemented = 0;
    if (Abc_ObjFanin0(pFanout) == pNode) {
        pOtherFanin = Abc_ObjFanin1(pFanout);
        isFaninComplemented = Abc_ObjFaninC0(pFanout);  // Get other node's edge complement
    } else {
        pOtherFanin = Abc_ObjFanin0(pFanout);
        isFaninComplemented = Abc_ObjFaninC1(pFanout);  // Get other node's edge complement
    }
    
    // Create cone for the other input
    Vec_Ptr_t* vNodes = Vec_PtrAlloc(1);
    Vec_PtrPush(vNodes, pOtherFanin);
    Abc_Ntk_t* pCone = Abc_NtkCreateConeArray(pNtk, vNodes, 1);
    Vec_PtrFree(vNodes);
    
    if (!pCone) {
        printf("Failed to create logic cone\n");
        return odcPatterns;
    }
    
    PrintConeStructure(pCone);
    
    printf("\nConverting to AIG...\n");
    Aig_Man_t* pAig = Abc_NtkToDar(pCone, 0, 0);
    Abc_NtkDelete(pCone);
    
    if (!pAig) {
        printf("Failed to convert to AIG\n");
        return odcPatterns;
    }
    
    printf("\nCreating SAT solver...\n");
    sat_solver* pSat = sat_solver_new();
    sat_solver_setnvars(pSat, 2 * Aig_ManObjNumMax(pAig));
    
    // Map nodes to variables
    std::map<int, int> nodeToVar;
    int varCounter = 1;
    
    // Map primary inputs
    Aig_Obj_t* pObj;
    int i;
    Aig_ManForEachCi(pAig, pObj, i) {
        nodeToVar[Aig_ObjId(pObj)] = varCounter++;
    }
    
    // Create clauses for AND gates
    std::vector<std::vector<lit>> allClauses;
    Aig_ManForEachNode(pAig, pObj, i) {
        int outVar = varCounter++;
        nodeToVar[Aig_ObjId(pObj)] = outVar;
        
        int in0Var = nodeToVar[Aig_ObjFaninId0(pObj)];
        int in1Var = nodeToVar[Aig_ObjFaninId1(pObj)];
        bool isIn0Compl = Aig_ObjFaninC0(pObj);
        bool isIn1Compl = Aig_ObjFaninC1(pObj);
        
        lit Lits[3];
        
        // First clause: (!out + in0)
        std::vector<lit> clause1 = {toLitCond(outVar, 1), toLitCond(in0Var, isIn0Compl)};
        allClauses.push_back(clause1);
        Lits[0] = clause1[0];
        Lits[1] = clause1[1];
        if (!sat_solver_addclause(pSat, Lits, Lits + 2)) {
            sat_solver_delete(pSat);
            Aig_ManStop(pAig);
            return odcPatterns;
        }
        
        // Second clause: (!out + in1)
        std::vector<lit> clause2 = {toLitCond(outVar, 1), toLitCond(in1Var, isIn1Compl)};
        allClauses.push_back(clause2);
        Lits[0] = clause2[0];
        Lits[1] = clause2[1];
        if (!sat_solver_addclause(pSat, Lits, Lits + 2)) {
            sat_solver_delete(pSat);
            Aig_ManStop(pAig);
            return odcPatterns;
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
            return odcPatterns;
        }
    }
    
    // Get the AIG node corresponding to the other fanin's output and its inputs
    Aig_Obj_t* pOutputDriverObj = NULL;
    std::vector<int> inputVars;

    Aig_ManForEachCo(pAig, pObj, i) {
        int driverId = Aig_ObjFaninId0(pObj);
        pOutputDriverObj = Aig_ManObj(pAig, driverId);
        
        if (!Aig_ObjIsNode(pOutputDriverObj)) {
            continue;
        }
        
        inputVars.push_back(nodeToVar[Aig_ObjFaninId0(pOutputDriverObj)]);
        inputVars.push_back(nodeToVar[Aig_ObjFaninId1(pOutputDriverObj)]);
        printf("Input variables for other node: %d, %d\n", inputVars[0], inputVars[1]);
        break;
    }

    if (inputVars.size() != 2) {
        printf("Failed to find input variables\n");
        sat_solver_delete(pSat);
        Aig_ManStop(pAig);
        return odcPatterns;
    }

    // Add output constraint
    Aig_ManForEachCo(pAig, pObj, i) {
        int driverId = Aig_ObjFaninId0(pObj);
        int isCompl = Aig_ObjFaninC0(pObj);
        int outVar = nodeToVar[driverId];

        printf("DriverID: %d, isFaninCompl: %d, outVar: %d\n", driverId, isFaninComplemented, outVar);
        
        lit Lits[1];
        Lits[0] = toLitCond(outVar, isFaninComplemented);
        
        std::vector<lit> clause = {Lits[0]};
        allClauses.push_back(clause);
        
        if (!sat_solver_addclause(pSat, Lits, Lits + 1)) {
            sat_solver_delete(pSat);
            Aig_ManStop(pAig);
            return odcPatterns;
        }
        break;
    }
    
    PrintCnfClauses(pSat, nodeToVar, allClauses);
    
    // Find all satisfying assignments
    printf("\nSolving SAT problem...\n");
    while (sat_solver_solve(pSat, NULL, NULL, 0, 0, 0, 0) == l_True) {
        int val0 = sat_solver_var_value(pSat, inputVars[0]);
        int val1 = sat_solver_var_value(pSat, inputVars[1]);
        printf("Found pattern for other node: %d%d\n", val0, val1);
        
        // Consider input edges to our target node
        // We need to adjust the pattern based on the edges that feed into our node
        val0 = val0 ^ Abc_ObjFaninC0(pNode);
        val1 = val1 ^ Abc_ObjFaninC1(pNode);
        
        printf("Translated to target node pattern: %d%d\n", val0, val1);
        odcPatterns.push_back({val0, val1});
        
        // Block the original pattern (before edge consideration)
        lit blockLits[2];
        blockLits[0] = toLitCond(inputVars[0], sat_solver_var_value(pSat, inputVars[0]));
        blockLits[1] = toLitCond(inputVars[1], sat_solver_var_value(pSat, inputVars[1]));
        if (!sat_solver_addclause(pSat, blockLits, blockLits + 2)) break;
    }

    sat_solver_delete(pSat);
    Aig_ManStop(pAig);
    return odcPatterns;
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
    
    std::vector<std::pair<int, int>> odcPatterns = ComputeOdc(pNtk, pNode);
    
    if (odcPatterns.empty()) {
        Abc_Print(1, "no odc\n");
    } else {
        for (const auto& pattern : odcPatterns) {
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