// lsvCmd.cpp

#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "proof/fraig/fraig.h"
#include "sat/cnf/cnf.h"
#include "sat/bsat/satSolver.h"
#include <iostream>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <cstdlib>
#include <ctime>

extern "C" {
Aig_Man_t* Abc_NtkToDar(Abc_Ntk_t* pNtk, int fExors, int fRegisters);
}

static int Lsv_CommandSDC(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandODC(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sdc", Lsv_CommandSDC, 0);
    Cmd_CommandAdd(pAbc, "LSV", "lsv_odc", Lsv_CommandODC, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct lsvPackageRegistrationManager {
    lsvPackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager_;

std::vector<int> get_sdc_patterns(Abc_Ntk_t* pNtk, Abc_Obj_t* pNode) {
    std::vector<int> sdc_patterns;

    std::unordered_set<int> observed_patterns;
    const int SIMULATION_ROUNDS = 16;
    std::unordered_map<int, int> sim_values;
    Abc_Obj_t* y0 = Abc_ObjFanin0(pNode);
    Abc_Obj_t* y1 = Abc_ObjFanin1(pNode);

    std::srand(std::time(nullptr));

    for (int round = 0; round < SIMULATION_ROUNDS; ++round) {
        sim_values.clear();
        Abc_Obj_t* pObj;
        int i;
        Abc_NtkForEachPi(pNtk, pObj, i) {
            int value = rand() % 2;
            sim_values[Abc_ObjId(pObj)] = value;
        }
        Abc_NtkForEachNode(pNtk, pObj, i) {
            Abc_Obj_t* pFanin0 = Abc_ObjFanin0(pObj);
            Abc_Obj_t* pFanin1 = Abc_ObjFanin1(pObj);
            int value0 = sim_values[Abc_ObjId(pFanin0)];
            int value1 = sim_values[Abc_ObjId(pFanin1)];
            if (Abc_ObjFaninC0(pObj)) value0 = !value0;
            if (Abc_ObjFaninC1(pObj)) value1 = !value1;
            int value = value0 & value1;
            sim_values[Abc_ObjId(pObj)] = value;
        }
        int value_y0 = sim_values[Abc_ObjId(y0)];
        int value_y1 = sim_values[Abc_ObjId(y1)];
        if (Abc_ObjFaninC0(pNode)) value_y0 = !value_y0;
        if (Abc_ObjFaninC1(pNode)) value_y1 = !value_y1;
        int pattern = (value_y0 << 1) | value_y1;
        observed_patterns.insert(pattern);
        if (observed_patterns.size() == 4) break;
    }

    std::vector<int> missing_patterns;
    for (int p = 0; p < 4; ++p) {
        if (observed_patterns.find(p) == observed_patterns.end()) {
            missing_patterns.push_back(p);
        }
    }

    if (missing_patterns.empty()) {
        return sdc_patterns;
    }

    // Build the cone of y0 and y1
    Vec_Ptr_t* vNodes = Vec_PtrAlloc(2);
    Vec_PtrPush(vNodes, y0);
    Vec_PtrPush(vNodes, y1);
    Abc_Ntk_t* pConeNtk = Abc_NtkCreateConeArray(pNtk, vNodes, 1);
    Vec_PtrFree(vNodes);

    Aig_Man_t* pAig = Abc_NtkToDar(pConeNtk, 0, 0);
    // Corrected here: Pass the number of outputs
    Cnf_Dat_t* pCnf = Cnf_Derive(pAig, Aig_ManCoNum(pAig));

    Aig_Obj_t* pAigObjY0 = Aig_ObjFanin0(Aig_ManCo(pAig, 0));
    Aig_Obj_t* pAigObjY1 = Aig_ObjFanin0(Aig_ManCo(pAig, 1));

    int varY0 = pCnf->pVarNums[Aig_ObjId(pAigObjY0)];
    int varY1 = pCnf->pVarNums[Aig_ObjId(pAigObjY1)];

    for (int pattern : missing_patterns) {
        int v0 = (pattern >> 1) & 1;
        int v1 = pattern & 1;
        int v0_adj = v0;
        int v1_adj = v1;
        if (Abc_ObjFaninC0(pNode)) v0_adj = !v0;
        if (Abc_ObjFaninC1(pNode)) v1_adj = !v1;

        sat_solver* pSat = (sat_solver*)Cnf_DataWriteIntoSolver(pCnf, 1, 0);
        lit assumptions[2];
        assumptions[0] = toLitCond(varY0, v0_adj == 0);
        assumptions[1] = toLitCond(varY1, v1_adj == 0);

        // Corrected here: pass assumptions, assumptions+2
        int status = sat_solver_solve(pSat, assumptions, assumptions + 2, 0, 0, 0, 0);
        if (status == l_False) {
            sdc_patterns.push_back(pattern);
        }
        sat_solver_delete(pSat);
    }

    Cnf_DataFree(pCnf);
    Aig_ManStop(pAig);
    Abc_NtkDelete(pConeNtk);

    return sdc_patterns;
}

int Lsv_CommandSDC(Abc_Frame_t* pAbc, int argc, char** argv) {
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    if (pNtk == nullptr) {
        Abc_Print(-1, "Empty network.\n");
        return 1;
    }

    // Check if network is strashed
    if (!Abc_NtkIsStrash(pNtk)) {
        // Strash the network
        pNtk = Abc_NtkStrash(pNtk, 0, 1, 0);
        if (pNtk == nullptr) {
            Abc_Print(-1, "Strashing the network failed.\n");
            return 1;
        }
        Abc_FrameReplaceCurrentNetwork(pAbc, pNtk);
    }

    if (argc != 2) {
        Abc_Print(-2, "usage: lsv_sdc <n>\n");
        Abc_Print(-2, "\t<n>   : node with id n\n");
        return 1;
    }
    int n = atoi(argv[1]);
    if (n < 0 || n >= Abc_NtkObjNumMax(pNtk)) {
        Abc_Print(-1, "Invalid node id %d.\n", n);
        return 1;
    }
    Abc_Obj_t* pNode = Abc_NtkObj(pNtk, n);
    if (pNode == nullptr || !Abc_ObjIsNode(pNode)) {
        Abc_Print(-1, "Node %d is not an internal node.\n", n);
        return 1;
    }

    std::vector<int> sdc_patterns = get_sdc_patterns(pNtk, pNode);

    if (sdc_patterns.empty()) {
        printf("no sdc\n");
    } else {
        std::sort(sdc_patterns.begin(), sdc_patterns.end());
        for (int pattern : sdc_patterns) {
            int v0 = (pattern >> 1) & 1;
            int v1 = pattern & 1;
            printf("%d%d\n", v0, v1);
        }
    }
    return 0;
}

int Lsv_CommandODC(Abc_Frame_t* pAbc, int argc, char** argv) {
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    if (pNtk == nullptr) {
        Abc_Print(-1, "Empty network.\n");
        return 1;
    }

    // Check if network is strashed
    if (!Abc_NtkIsStrash(pNtk)) {
        // Strash the network
        pNtk = Abc_NtkStrash(pNtk, 0, 1, 0);
        if (pNtk == nullptr) {
            Abc_Print(-1, "Strashing the network failed.\n");
            return 1;
        }
        Abc_FrameReplaceCurrentNetwork(pAbc, pNtk);
    }

    if (argc != 2) {
        Abc_Print(-2, "usage: lsv_odc <n>\n");
        Abc_Print(-2, "\t<n>   : node with id n\n");
        return 1;
    }
    int n = atoi(argv[1]);
    if (n < 0 || n >= Abc_NtkObjNumMax(pNtk)) {
        Abc_Print(-1, "Invalid node id %d.\n", n);
        return 1;
    }
    Abc_Obj_t* pNode = Abc_NtkObj(pNtk, n);
    if (pNode == nullptr || !Abc_ObjIsNode(pNode)) {
        Abc_Print(-1, "Node %d is not an internal node.\n", n);
        return 1;
    }

    // Duplicate the network and invert node n
    Abc_Ntk_t* pNtkDup = Abc_NtkDup(pNtk);
    Abc_Obj_t* pNodeDup = Abc_NtkObj(pNtkDup, n);

    if (pNodeDup == nullptr) {
        Abc_Print(-1, "Failed to find node %d in duplicated network.\n", n);
        Abc_NtkDelete(pNtkDup);
        return 1;
    }

    // Invert the fanouts of pNodeDup
    Abc_Obj_t* pFanout;
    int i;
    Abc_ObjForEachFanout(pNodeDup, pFanout, i) {
        int j;
        Abc_Obj_t* pFanin;
        Abc_ObjForEachFanin(pFanout, pFanin, j) {
            if (pFanin == pNodeDup) {
                Abc_ObjXorFaninC(pFanout, j);
            }
        }
    }
    Abc_NtkForEachCo(pNtkDup, pFanout, i) {
        if (Abc_ObjFanin0(pFanout) == pNodeDup) {
            Abc_ObjXorFaninC(pFanout, 0);
        }
    }

    // Convert original network to AIG
    Aig_Man_t* pAigA = Abc_NtkToDar(pNtk, 1, 0); // Set fAddStrash = 1
    // Convert modified network to AIG
    Aig_Man_t* pAigB = Abc_NtkToDar(pNtkDup, 1, 0);
    // Do NOT delete pNtkDup yet; it's still needed

    if (pAigA == nullptr || pAigB == nullptr) {
        Abc_Print(-1, "Failed to convert networks to AIG.\n");
        if (pAigA) Aig_ManStop(pAigA);
        if (pAigB) Aig_ManStop(pAigB);
        Abc_NtkDelete(pNtkDup);
        return 1;
    }

    // Derive CNFs
    Cnf_Dat_t* pCnfA = Cnf_Derive(pAigA, Aig_ManCoNum(pAigA));
    Cnf_Dat_t* pCnfB = Cnf_Derive(pAigB, Aig_ManCoNum(pAigB));

    if (pCnfA == nullptr || pCnfB == nullptr) {
        Abc_Print(-1, "Failed to derive CNF formulas.\n");
        Aig_ManStop(pAigA);
        Aig_ManStop(pAigB);
        if (pCnfA) Cnf_DataFree(pCnfA);
        if (pCnfB) Cnf_DataFree(pCnfB);
        Abc_NtkDelete(pNtkDup);
        return 1;
    }

    // Lift variables in pCnfB
    int nVarShift = pCnfA->nVars;
    Cnf_DataLift(pCnfB, nVarShift);

    // Create SAT solver
    sat_solver* pSat = sat_solver_new();

    // Add clauses from pCnfA and pCnfB into the SAT solver
    int* pVarMapA = (int*)Cnf_DataWriteIntoSolverInt((void*)pSat, pCnfA, 1, 1); // fInit = 1
    int* pVarMapB = (int*)Cnf_DataWriteIntoSolverInt((void*)pSat, pCnfB, 1, 0); // fInit = 0

    // Map PIs by their names to ensure correct correspondence
    std::unordered_map<std::string, int> piNameToVarA;
    std::unordered_map<std::string, int> piNameToVarB;

    Abc_Obj_t* pPi;
    int idx;

    // Map PIs in pNtk to variables in pCnfA (using mapped variables)
    Abc_NtkForEachPi(pNtk, pPi, idx) {
        std::string name = Abc_ObjName(pPi);
        Aig_Obj_t* pAigPi = (Aig_Obj_t*)(pPi->pCopy); // Use pCopy pointers
        int cnfVarA = pCnfA->pVarNums[Aig_ObjId(pAigPi)];
        int varA = pVarMapA[cnfVarA];
        piNameToVarA[name] = varA;
    }

    // Map PIs in pNtkDup to variables in pCnfB (using mapped variables)
    Abc_NtkForEachPi(pNtkDup, pPi, idx) {
        std::string name = Abc_ObjName(pPi);
        Aig_Obj_t* pAigPi = (Aig_Obj_t*)(pPi->pCopy); // Use pCopy pointers
        int cnfVarB = pCnfB->pVarNums[Aig_ObjId(pAigPi)];
        int varB = pVarMapB[cnfVarB];
        piNameToVarB[name] = varB;
    }

    // For each PI, enforce x_i in pCnfA equals x_i in pCnfB
    for (const auto& entry : piNameToVarA) {
        const std::string& name = entry.first;
        int varA = entry.second;
        int varB = piNameToVarB[name];

        lit lits[2];
        lits[0] = toLitCond(varA, 0); // xA_i
        lits[1] = toLitCond(varB, 1); // ¬xB_i
        sat_solver_addclause(pSat, lits, lits + 2);

        lits[0] = toLitCond(varA, 1); // ¬xA_i
        lits[1] = toLitCond(varB, 0); // xB_i
        sat_solver_addclause(pSat, lits, lits + 2);
    }

    // For each PO, create XOR variables and clauses
    int nPos = Aig_ManCoNum(pAigA);
    std::vector<int> vXorVars;
    for (idx = 0; idx < nPos; ++idx) {
        Aig_Obj_t* pCoA = Aig_ManCo(pAigA, idx);
        Aig_Obj_t* pCoB = Aig_ManCo(pAigB, idx);

        int cnfVarA = pCnfA->pVarNums[Aig_ObjId(pCoA)];
        int varA = pVarMapA[cnfVarA];

        int cnfVarB = pCnfB->pVarNums[Aig_ObjId(pCoB)];
        int varB = pVarMapB[cnfVarB];

        int varXor = sat_solver_addvar(pSat);
        vXorVars.push_back(varXor);

        // Build clauses for XOR: varXor = (varA ⊕ varB)
        lit lits[3];

        lits[0] = toLitCond(varXor, 1); // ¬varXor
        lits[1] = toLitCond(varA, 0);
        lits[2] = toLitCond(varB, 1);
        sat_solver_addclause(pSat, lits, lits + 3);

        lits[0] = toLitCond(varXor, 1); // ¬varXor
        lits[1] = toLitCond(varA, 1);
        lits[2] = toLitCond(varB, 0);
        sat_solver_addclause(pSat, lits, lits + 3);

        lits[0] = toLitCond(varXor, 0); // varXor
        lits[1] = toLitCond(varA, 0);
        lits[2] = toLitCond(varB, 0);
        sat_solver_addclause(pSat, lits, lits + 3);

        lits[0] = toLitCond(varXor, 0); // varXor
        lits[1] = toLitCond(varA, 1);
        lits[2] = toLitCond(varB, 1);
        sat_solver_addclause(pSat, lits, lits + 3);
    }

    // Create OR gate for miter output
    int varMiterOutput = sat_solver_addvar(pSat);

    // Add clauses for miter output being OR of vXorVars
    for (int varXor : vXorVars) {
        lit lits[2];
        lits[0] = toLitCond(varMiterOutput, 1); // ¬varMiterOutput
        lits[1] = toLitCond(varXor, 0);         // varXor_i
        sat_solver_addclause(pSat, lits, lits + 2);
    }

    // Add clause (varMiterOutput ∨ ¬varXor_1 ∨ ¬varXor_2 ∨ ...)
    std::vector<lit> lits_miter;
    lits_miter.push_back(toLitCond(varMiterOutput, 0)); // varMiterOutput
    for (int varXor : vXorVars) {
        lits_miter.push_back(toLitCond(varXor, 1)); // ¬varXor_i
    }
    sat_solver_addclause(pSat, &lits_miter[0], &lits_miter[0] + lits_miter.size());

    // Get variables corresponding to y0 and y1 in pCnfA
    Abc_Obj_t* y0 = Abc_ObjFanin0(pNode);
    Abc_Obj_t* y1 = Abc_ObjFanin1(pNode);

    Aig_Obj_t* pAigObjY0 = (Aig_Obj_t*)(y0->pCopy);
    Aig_Obj_t* pAigObjY1 = (Aig_Obj_t*)(y1->pCopy);

    // Check if pAigObjY0 and pAigObjY1 are not null
    if (pAigObjY0 == nullptr || pAigObjY1 == nullptr) {
        Abc_Print(-1, "Failed to find corresponding AIG nodes for y0 or y1.\n");
        sat_solver_delete(pSat);
        Cnf_DataFree(pCnfA);
        Cnf_DataFree(pCnfB);
        Aig_ManStop(pAigA);
        Aig_ManStop(pAigB);
        Abc_NtkDelete(pNtkDup);
        return 1;
    }

    int cnfVarY0 = pCnfA->pVarNums[Aig_ObjId(pAigObjY0)];
    int varY0 = pVarMapA[cnfVarY0];

    int cnfVarY1 = pCnfA->pVarNums[Aig_ObjId(pAigObjY1)];
    int varY1 = pVarMapA[cnfVarY1];

    std::unordered_set<int> care_patterns;

    // Collect care patterns by checking all combinations
    for (int v0 = 0; v0 <= 1; ++v0) {
        for (int v1 = 0; v1 <= 1; ++v1) {

            lit assumptions[3];
            // Adjust for inversion when setting the literals
            assumptions[0] = toLitCond(varY0, (v0 ^ Abc_ObjFaninC0(pNode)) == 0);
            assumptions[1] = toLitCond(varY1, (v1 ^ Abc_ObjFaninC1(pNode)) == 0);
            assumptions[2] = toLitCond(varMiterOutput, 0); // varMiterOutput = 1

            int status = sat_solver_solve(pSat, assumptions, assumptions + 3, 0, 0, 0, 0);

            if (status == l_True) {
                int pattern = (v0 << 1) | v1;
                care_patterns.insert(pattern);
            }
        }
    }

    // Clean up
    sat_solver_delete(pSat);
    Cnf_DataFree(pCnfA);
    Cnf_DataFree(pCnfB);
    Aig_ManStop(pAigA);
    Aig_ManStop(pAigB);
    Abc_NtkDelete(pNtkDup); // Now it's safe to delete pNtkDup

    // Get SDC patterns
    std::vector<int> sdc_patterns = get_sdc_patterns(pNtk, pNode);

    // Compute ODC patterns
    std::vector<int> odc_patterns;
    for (int p = 0; p < 4; ++p) {
        if (care_patterns.find(p) == care_patterns.end()) {
            if (std::find(sdc_patterns.begin(), sdc_patterns.end(), p) == sdc_patterns.end()) {
                odc_patterns.push_back(p);
            }
        }
    }

    if (odc_patterns.empty()) {
        printf("no odc\n");
    } else {
        std::sort(odc_patterns.begin(), odc_patterns.end());
        for (int pattern : odc_patterns) {
            int v0 = (pattern >> 1) & 1;
            int v1 = pattern & 1;
            printf("%d%d\n", v0, v1);
        }
    }
    return 0;
}

