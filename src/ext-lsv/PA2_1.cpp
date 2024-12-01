#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "aig/aig/aig.h"
#include "sat/cnf/cnf.h"
#include "sat/bsat/satSolver.h"
#include <vector>
#include <iostream>
#include <set>
#include <algorithm>
#include <random>
#include <unordered_map>
#include <bitset>

// Command function declaration
static int Lsv_CommandSdc(Abc_Frame_t* pAbc, int argc, char** argv);

// Initialization and destruction functions
void init(Abc_Frame_t* pAbc) {
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sdc", Lsv_CommandSdc, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
    PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

static void ComputeSDC(Abc_Ntk_t* pNtk, int nodeId) ;
	
int Lsv_CommandSdc(Abc_Frame_t* pAbc, int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: lsv_sdc <node_id>\n";
        return 1;
    }

    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    if (!pNtk) {
        std::cerr << "Error: Network is empty.\n";
        return 1;
    }

    char* endptr;
    int nodeId = std::strtol(argv[1], &endptr, 10);
    if (*endptr != '\0') {
        std::cerr << "Error: Invalid node ID provided. Please provide a valid integer value.\n";
        return 1;
    }

    ComputeSDC(pNtk, nodeId);

    return 0;
}

// Core function: Compute SDC
static void ComputeSDC(Abc_Ntk_t* pNtk, int nodeId) {
    Abc_Obj_t* pNode = Abc_NtkObj(pNtk, nodeId);
    if (!pNode) {
        std::cerr << "Error: Node ID " << nodeId << " is invalid.\n";
        return;
    }

    // Check if the node has at least 2 fanins
    if (Abc_ObjFaninNum(pNode) < 2) {
        //std::cout << "Node " << nodeId << " has less than 2 fanins.\n";
        std::cout << "no sdc \n";
        return;
    }

    // Ensure the node is suitable for cone creation
    if (!Abc_ObjIsNode(pNode) && !Abc_ObjIsCi(pNode)) {
        std::cerr << "Error: Node ID " << nodeId << " is not a valid type for cone creation.\n";
        return;
    }

    // Create the transitive cone of the node's fanins
    Abc_Ntk_t* pCone = Abc_NtkCreateCone(pNtk, pNode, Abc_ObjName(pNode), 1);
    if (!pCone || Abc_NtkObjNumMax(pCone) == 0) {
        std::cerr << "Error: Failed to create transitive cone or cone is empty for node ID " << nodeId << ".\n";
        return;
    }

    // Manually create AIG representation from cone
    Aig_Man_t* pAigMan = Aig_ManStart(Abc_NtkObjNumMax(pCone));
    if (!pAigMan || Aig_ManObjNumMax(pAigMan) == 0) {
        std::cerr << "Error: Failed to create AIG manager or AIG manager is empty for node ID " << nodeId << ".\n";
        Abc_NtkDelete(pCone);
        return;
    }

    // Map to keep track of AIG objects
    std::unordered_map<int, Aig_Obj_t*> aigMap;

    // Step 1: Create CIs first
    Abc_Obj_t* pObj;
    int i;
    Abc_NtkForEachCi(pCone, pObj, i) {
        if (!pObj) {
            //std::cerr << "Error: Invalid CI pointer for node ID " << nodeId << ".\n";
            Aig_ManStop(pAigMan);
            Abc_NtkDelete(pCone);
            return;
        }
        Aig_Obj_t* pAigCi = Aig_ObjCreateCi(pAigMan);
        if (!pAigCi) {
            //std::cerr << "Error: Failed to create CI in AIG for node ID " << Abc_ObjId(pObj) << ".\n";
            Aig_ManStop(pAigMan);
            Abc_NtkDelete(pCone);
            return;
        }
        aigMap[Abc_ObjId(pObj)] = pAigCi;
    }

    // Step 2: Create internal nodes (AND gates)
    Abc_NtkForEachNode(pCone, pObj, i) {
        if (!pObj) {
            //std::cerr << "Error: Invalid node pointer in AIG creation for node ID " << nodeId << ".\n";
            Aig_ManStop(pAigMan);
            Abc_NtkDelete(pCone);
            return;
        }
        if (Abc_ObjFaninNum(pObj) == 2) {
            Abc_Obj_t* pFanin0 = Abc_ObjFanin0(pObj);
            Abc_Obj_t* pFanin1 = Abc_ObjFanin1(pObj);

            if (pFanin0 && pFanin1) {
                if (aigMap.find(Abc_ObjId(pFanin0)) == aigMap.end() || aigMap.find(Abc_ObjId(pFanin1)) == aigMap.end()) {
                    //std::cerr << "Error: Missing fanins in AIG map for node ID " << Abc_ObjId(pObj) << ".\n";
                    continue;
                }
                Aig_Obj_t* pAigFanin0 = aigMap[Abc_ObjId(pFanin0)];
                Aig_Obj_t* pAigFanin1 = aigMap[Abc_ObjId(pFanin1)];
                if (pAigFanin0 && pAigFanin1) {
                    Aig_Obj_t* pAndObj = Aig_And(pAigMan, pAigFanin0, pAigFanin1);
                    if (!pAndObj) {
                        //std::cerr << "Error: Failed to create AND gate for node ID " << Abc_ObjId(pObj) << ".\n";
                    } else {
                        aigMap[Abc_ObjId(pObj)] = pAndObj;
                    }
                } else {
                    //std::cerr << "Error: Missing or invalid fanins for node ID " << Abc_ObjId(pObj) << ".\n";
                }
            } else {
                //std::cerr << "Error: Fanin nodes are not properly set for node ID " << Abc_ObjId(pObj) << ".\n";
            }
        }
    }

    // Step 3: Create COs
    Abc_NtkForEachCo(pCone, pObj, i) {
        if (!pObj) {
            //std::cerr << "Error: Invalid CO pointer for node ID " << nodeId << ".\n";
            Aig_ManStop(pAigMan);
            Abc_NtkDelete(pCone);
            return;
        }
        Abc_Obj_t* pFanin0 = Abc_ObjFanin0(pObj);
        if (pFanin0) {
            if (aigMap.find(Abc_ObjId(pFanin0)) == aigMap.end()) {
                //std::cerr << "Error: Missing fanin in AIG map for CO node ID " << Abc_ObjId(pObj) << ".\n";
                continue;
            }
            Aig_Obj_t* pAigFanin = aigMap[Abc_ObjId(pFanin0)];
            if (pAigFanin) {
                Aig_ObjCreateCo(pAigMan, pAigFanin);
                aigMap[Abc_ObjId(pObj)] = pAigFanin;
            } else {
                //std::cerr << "Error: Failed to create CO for node ID " << Abc_ObjId(pObj) << " due to missing or invalid fanin.\n";
            }
        } else {
            //std::cerr << "Error: Fanin is not properly set for CO node ID " << Abc_ObjId(pObj) << ".\n";
        }
    }

    Abc_NtkDelete(pCone);

    if (Aig_ManCoNum(pAigMan) == 0) {
        std::cerr << "Error: AIG manager has no outputs after conversion.\n";
        Aig_ManStop(pAigMan);
        return;
    }

    // Generate CNF
    Cnf_Dat_t* pCnf = Cnf_Derive(pAigMan, 1);
    if (!pCnf || pCnf->nClauses == 0) {
        std::cerr << "Error: Failed to derive CNF or CNF is empty.\n";
        Aig_ManStop(pAigMan);
        return;
    }
    Aig_ManStop(pAigMan);

    // Initialize SAT solver
    int nVars = pCnf->nVars;
    sat_solver* pSat = sat_solver_new();
    if (!pSat) {
        std::cerr << "Error: Failed to initialize SAT solver.\n";
        Cnf_DataFree(pCnf);
        return;
    }
    sat_solver_setnvars(pSat, nVars);

    if (nVars <= 0) {
        std::cerr << "Error: Number of variables for SAT solver is non-positive.\n";
        sat_solver_delete(pSat);
        Cnf_DataFree(pCnf);
        return;
    }

    // Add CNF to SAT solver with improved safety checks
    for (int i = 0; i < pCnf->nClauses; ++i) {  // Use full clause range
        if (pCnf->pClauses[i] != NULL) {  // Replace nullptr with NULL for compatibility and consistency
            lit* pBegin = pCnf->pClauses[i];
            lit* pEnd = (i == pCnf->nClauses - 1) ? pCnf->pClauses[0] + pCnf->nLiterals : pCnf->pClauses[i + 1];  // Correctly set pEnd for the last clause

            // Ensure pEnd does not exceed the literal range
            if (pEnd > pCnf->pClauses[0] + pCnf->nLiterals) {
                pEnd = pCnf->pClauses[0] + pCnf->nLiterals;
            }

            if (pBegin < pEnd) {
                bool validClause = true;
                for (lit* pLit = pBegin; pLit < pEnd && *pLit != 0; ++pLit) {  // Corrected loop boundary to < pEnd and added check for clause termination
                    if (lit_var(*pLit) < 0 || lit_var(*pLit) >= nVars) {
                        std::cerr << "Error: Literal variable out of range for SAT solver. Skipping clause.\n";
                        validClause = false;
                        break;
                    }
                }
                if (validClause) {
                    if (!sat_solver_addclause(pSat, pBegin, pEnd)) {
                        std::cerr << "Error: Failed to add CNF clause to SAT solver.\n";
                        sat_solver_delete(pSat);
                        Cnf_DataFree(pCnf);
                        return;
                    }
                }
            } else {
                std::cerr << "Error: Invalid clause bounds while adding CNF to SAT solver for node " << nodeId << ".\n";
                sat_solver_delete(pSat);
                Cnf_DataFree(pCnf);
                return;
            }
        } else {
            std::cerr << "Error: Null clause pointer encountered during CNF addition to SAT solver for node " << nodeId << ".\n";
            sat_solver_delete(pSat);
            Cnf_DataFree(pCnf);
            return;
        }
    }

    // Systematically simulate all patterns
    bool hasSDC = false;
    int numFanins = Abc_ObjFaninNum(pNode);
    if (numFanins > 0) {
        int maxPattern = (1 << numFanins);
        for (int pattern = 0; pattern < maxPattern; ++pattern) {
            std::vector<int> assumptions;
            for (int j = 0; j < numFanins; ++j) {
                Abc_Obj_t* pFanin = Abc_ObjFanin(pNode, j);
                if (!pFanin || pCnf->pVarNums[Abc_ObjId(pFanin)] == -1) {
                    //std::cerr << "Warning: Fanin " << (pFanin ? Abc_ObjId(pFanin) : -1) << " of node " << nodeId << " is not mapped to a valid CNF variable. Skipping this pattern.\n";
                    //assumptions.clear();
                    break;
                }
                int varId = pCnf->pVarNums[Abc_ObjId(pFanin)];
                if (varId < 0 || varId >= nVars) {
                    //std::cerr << "Error: Variable ID " << varId << " out of range for SAT solver. Skipping this pattern.\n";
					//std::cerr << " no sdc\n";
                    //assumptions.clear();
                    break;
                }
                assumptions.push_back(Abc_Var2Lit(varId, (pattern >> j) & 1));
            }

            if (assumptions.size() != numFanins) {
                continue;
            }

            if (sat_solver_solve(pSat, assumptions.data(), assumptions.data() + assumptions.size(), 0, 0, 0, 0) == l_False) {
                hasSDC = true;
                //std::cout << "Node " << nodeId << " SDC pattern: " << std::bitset<32>(pattern).to_string().substr(32 - numFanins) << "\n";
                std::cout << std::bitset<32>(pattern).to_string().substr(32 - numFanins) << "\n";
			}
        }
    } else {
        //std::cerr << "Error: Node " << nodeId << " has no fanins to simulate.\n";
        sat_solver_delete(pSat);
        Cnf_DataFree(pCnf);
        return;
    }

    if (!hasSDC) {
        std::cout << " no sdc\n";
    }

    // Free resources
    sat_solver_delete(pSat);
    Cnf_DataFree(pCnf);
}

