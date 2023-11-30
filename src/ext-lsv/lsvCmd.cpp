#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "bdd/extrab/extraBdd.h"
#include "sat/cnf/cnf.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <string>
#include <unordered_map>

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimBdd(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSymBdd(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSymSAT(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSymAll(Abc_Frame_t* pAbc, int argc, char** argv);

extern "C" {
    Aig_Man_t* Abc_NtkToDar(Abc_Ntk_t* pNtk, int fExors, int fRegisters);
}

void init(Abc_Frame_t* pAbc) {
    Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", Lsv_CommandSimBdd, 0);
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig", Lsv_CommandSimAig, 0);
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_bdd", Lsv_CommandSymBdd, 0);
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_sat", Lsv_CommandSymSAT, 0);
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_all", Lsv_CommandSymAll, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = { init, destroy };

struct PackageRegistrationManager {
    PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

std::vector<std::string> Lsv_ReadInputPatterns(const char *filename, Abc_Ntk_t *pNtk)
{
    std::ifstream inFile(filename);
    std::vector<std::string> lines;

    if (!inFile) {
        std::cerr << "Error: Unable to open file " << filename << std::endl;
    }

    std::string line;
    while (std::getline(inFile, line)) {
        char* cstr = new char[line.length() + 1];
        std::strcpy(cstr, line.c_str());
        lines.push_back(cstr);
    }

    return lines;
}

void Lsv_NtkPrintNodes(Abc_Ntk_t* pNtk) {
    Abc_Obj_t* pObj;
    int i;
    Abc_NtkForEachNode(pNtk, pObj, i) {
        printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
        Abc_Obj_t* pFanin;
        int j;
        Abc_ObjForEachFanin(pObj, pFanin, j) {
            printf("  Fanin-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanin),
                Abc_ObjName(pFanin));
        }
        if (Abc_NtkHasSop(pNtk)) {
            printf("The SOP of this node:\n%s", (char*)pObj->pData);
        }
    }
}

int Lsv_SimulateAigNode(Abc_Obj_t* pNode, int* inputValues) {

    if (Abc_ObjIsPi(pNode)) {
        // printf("aig leaf %s: %d: %d\n", Abc_ObjName(pNode), Abc_ObjId(pNode)-1, inputValues[Abc_ObjId(pNode)-1]);
        return inputValues[Abc_ObjId(pNode)-1];
    } 
    else if (Abc_ObjIsNode(pNode)) {
        Abc_Obj_t* pFanin0 = Abc_ObjFanin0(pNode);
        Abc_Obj_t* pFanin1 = Abc_ObjFanin1(pNode);
        int value0 = Lsv_SimulateAigNode(Abc_ObjRegular(pFanin0), inputValues);
        int value1 = Lsv_SimulateAigNode(Abc_ObjRegular(pFanin1), inputValues);
        assert(value0 == 1 || value0 == 0);
        assert(value1 == 1 || value1 == 0);
        if (Abc_ObjFaninC0(pNode)) {
            // printf("node %s fanin 0 %s is complement!!\n", Abc_ObjName(pNode), Abc_ObjName(Abc_ObjRegular(pFanin0)));
            value0 ^= 1;
        }
        if (Abc_ObjFaninC1(pNode)) {
            // printf("node %s fanin 1 %s is complement!!\n", Abc_ObjName(pNode), Abc_ObjName(Abc_ObjRegular(pFanin1)));
            value1 ^= 1;
        }
        // printf("Node %s: %d & %d is %d\n", Abc_ObjName(pNode), value0, value1, value0 & value1);
        return value0 & value1;
    } 
    else {
        // Handle constant nodes (0 and 1)
        // printf("Node %s: %d\n", Abc_ObjName(pNode), Abc_ObjFanin0(pNode) == Abc_ObjNot(pNode));
        return Abc_ObjFanin0(pNode) == Abc_ObjNot(pNode);
    }
}

void Lsv_NtkSimBdd(Abc_Ntk_t* pNtk, char* pattern) {
  
    if (!pNtk) {
        Abc_Print(-1, "Empty network.\n");
        return;
    }

    // Check if the network is a BDD network
    if (!Abc_NtkIsBddLogic(pNtk)) {
        Abc_Print(-1, "The current network is not a BDD network.\n");
        return;
    }

    // Parse the input pattern (e.g., "001" -> {0, 0, 1})
    int patternLength = strlen(pattern);
    if (patternLength != Abc_NtkPiNum(pNtk)) {
        Abc_Print(-1, "Input pattern length does not match the number of primary inputs.\n");
        return;
    }

    int* inputPattern = ABC_ALLOC(int, patternLength);
    for (int i = 0; i < patternLength; i++) {
        if (pattern[i] == '0') {
            inputPattern[i] = 0;
        }
        else if (pattern[i] == '1') {
            inputPattern[i] = 1;
        }
        else {
            Abc_Print(-1, "Invalid character in input pattern: %c\n", pattern[i]);
            ABC_FREE(inputPattern);
            return;
        }
    }

    // Iterate through each primary output
    Abc_Obj_t* pPo;
    int ithPo = -1;

    Abc_NtkForEachPo(pNtk, pPo, ithPo) 
    {
        char* poName = Abc_ObjName(pPo);

        // Get the associated BDD manager and node for the current PO
        Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo); 
        assert( Abc_NtkIsBddLogic(pRoot->pNtk) );
        DdManager * dd = (DdManager *)pRoot->pNtk->pManFunc;  
        DdNode* ddnode = (DdNode *)pRoot->pData;

        // Initialize result BDD with PO BDD
        DdNode* resultBdd = ddnode;

        // Iterate through each input variable using Abc_NtkForEachFanin
        Abc_Obj_t* pFanin;
        int faninIdx = 0;

        Abc_ObjForEachFanin(pRoot, pFanin, faninIdx) 
        {
            // Get the corresponding input index based on variable name
            char* faninName = Abc_ObjName(pFanin);

            // Find the matching input variable by name
            int inputIdx = -1;
            Abc_Obj_t* pCi;

            Abc_NtkForEachCi(pNtk, pCi, inputIdx) {
                if (strcmp(faninName, Abc_ObjName(pCi)) == 0) {
                    break;
                }
            }

            if (inputIdx == -1) {
                Abc_Print(-1, "Variable name %s not found in primary inputs.\n", faninName);
                return;
            }

            // Create a BDD variable for the input
            DdNode* varBdd = Cudd_bddIthVar(dd, faninIdx);

            // Compute the cofactor based on the input pattern
            resultBdd = Cudd_Cofactor(dd, resultBdd, inputPattern[inputIdx] ? varBdd : Cudd_Not(varBdd));
            Cudd_Ref(resultBdd); // Reference the new result BDD
        }

        // Determine the result value
        int result = (resultBdd == Cudd_ReadOne(dd)) ? 1 : 0;

        // Print the PO name and result
        Abc_Print(1, "%s: %d\n", poName, result);

        // Cleanup the result BDD
        Cudd_RecursiveDeref(dd, resultBdd);
    }
}

void Lsv_NtkSimAig(Abc_Ntk_t* pNtk, std::vector<std::string> patterns) {
    
    int nPatterns = patterns.size();

    if (!pNtk) {
        Abc_Print(-1, "Empty network.\n");
        return;
    }

    // Check if the network is a BDD network
    if (!Abc_NtkIsStrash(pNtk)) {
        Abc_Print(-1, "The current network is not a AIG network.\n");
        return;
    }

    Abc_Obj_t* pPo;
    int ithPo = -1;

    // Loop through the patterns
    Abc_NtkForEachPo(pNtk, pPo, ithPo) {
        
        printf("%6s: ", Abc_ObjName(pPo));

        for (int i = 0; i < nPatterns; i++) {
            int inputValues[Abc_NtkPiNum(pNtk)];

            // Set input values based on the pattern
            for (int j = 0; j < Abc_NtkPiNum(pNtk); j++) {
                inputValues[j] = patterns[i][j] - '0';
            }

            // Simulate the value of PO for given pattern
            // printf("\npatterns: ");
            // for (int j = 0; j < Abc_NtkPiNum(pNtk); j++) {
            //     printf("%d", inputValues[j]);
            // }
            // printf("\n");
            int poValue = Lsv_SimulateAigNode(Abc_ObjFanin0(pPo), inputValues);
            if (Abc_ObjFaninC0(pPo)) {
                poValue ^= 1;
            }
            printf("%d", poValue);
        }

        printf("\n");
    }
}

void Lsv_NtkSymBdd(Abc_Ntk_t* pNtk, int out_idx, int in0_idx, int in1_idx) {

    Abc_Obj_t *pPo, *pPi, *pFanin;

    int in0_idx_bdd = -1, in1_idx_bdd = -1;
    int ithPi = -1, ithFanin = -1;

    // find po
    pPo = Abc_NtkPo(pNtk, out_idx);
    Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo);
    assert(Abc_NtkIsBddLogic(pRoot->pNtk));

    DdManager* dd = (DdManager*)pRoot->pNtk->pManFunc;
    DdNode* ddnode = (DdNode*)pRoot->pData;

    // find the bdd order of in0_idx in1_idx
    char* in0_name = Abc_ObjName(Abc_NtkPi(pNtk, in0_idx));
    char* in1_name = Abc_ObjName(Abc_NtkPi(pNtk, in1_idx));

    Abc_ObjForEachFanin(pRoot, pFanin, ithFanin) {
        if (Abc_ObjName(pFanin) == in0_name)
            in0_idx_bdd = ithFanin;
        else if (Abc_ObjName(pFanin) == in1_name)
            in1_idx_bdd = ithFanin;
    }

    // if two inputs are not the support of output, BDD is symmetric
    if (in0_idx_bdd == -1 && in1_idx_bdd == -1) {
        std::cout << "symmetric\n";
        return;
    }
    
    DdNode *in0_tmp = NULL, *in1_tmp = NULL;
    DdNode *f_01 = NULL, *f_01_tmp = NULL;
    DdNode *f_10 = NULL, *f_10_tmp = NULL;
    DdNode *f_xor = NULL;

    // if one of input is not the support of the output
    if ((in0_idx_bdd == -1) ^ (in1_idx_bdd == -1))
    {
        if (in0_idx_bdd == -1) {
            f_xor = Cudd_bddBooleanDiff(dd, ddnode, in1_idx_bdd);
            Cudd_Ref(f_xor);
        } else {
            f_xor = Cudd_bddBooleanDiff(dd, ddnode, in0_idx_bdd);
            Cudd_Ref(f_xor);
        }
    } 
    else 
    {
        // assign in0 = 0, in1 = 1
        in0_tmp = Cudd_Not(Cudd_bddIthVar(dd, in0_idx_bdd));
        Cudd_Ref(in0_tmp);
        in1_tmp = Cudd_bddIthVar(dd, in1_idx_bdd);
        Cudd_Ref(in1_tmp);
        f_01_tmp = Cudd_Cofactor(dd, ddnode, in0_tmp);
        Cudd_Ref(f_01_tmp);
        f_01 = Cudd_Cofactor(dd, f_01_tmp, in1_tmp);
        Cudd_Ref(f_01);

        // Derefernce all temp nodes
        Cudd_RecursiveDeref(dd, in0_tmp);
        Cudd_RecursiveDeref(dd, in1_tmp);
        Cudd_RecursiveDeref(dd, f_01_tmp);

        // assign in0 = 1, in1 = 0
        in0_tmp = Cudd_bddIthVar(dd, in0_idx_bdd);
        Cudd_Ref(in0_tmp);
        in1_tmp = Cudd_Not(Cudd_bddIthVar(dd, in1_idx_bdd));
        Cudd_Ref(in1_tmp);
        f_10_tmp = Cudd_Cofactor(dd, ddnode, in0_tmp);
        Cudd_Ref(f_10_tmp);
        f_10 = Cudd_Cofactor(dd, f_10_tmp, in1_tmp);
        Cudd_Ref(f_10);

        // Derefernce all temp nodes
        Cudd_RecursiveDeref(dd, in0_tmp);
        Cudd_RecursiveDeref(dd, in1_tmp);
        Cudd_RecursiveDeref(dd, f_10_tmp);

        // xor f(0,1) and f(1,0)
        f_xor = Cudd_bddXor(dd, f_01, f_10);
        Cudd_Ref(f_xor);
    }

    // if the reults of XOR is 0, then BDD is symmetric
    if (f_xor == Cudd_Not(DD_ONE(dd)) || f_xor == DD_ZERO(dd)) 
    {
        std::cout << "symmetric\n";
    } 
    else
    {
        int* cube;
        std::string ex = "";
        DdGen* gen;
        CUDD_VALUE_TYPE value;

        // counter example initialize
        cube = ABC_ALLOC(int, Abc_ObjFaninNum(pRoot));
        for (int i = 0; i < Abc_NtkPiNum(pNtk); i++) {
            ex += "0";
        }

        // find the first cube of the BDD.
        gen = Cudd_FirstCube(dd, f_xor, &cube, &value);
        Cudd_RecursiveDeref(dd, f_xor);

        Abc_NtkForEachPi(pNtk, pPi, ithPi) {
            Abc_ObjForEachFanin(pRoot, pFanin, ithFanin) {
                if (Abc_ObjName(pPi) == Abc_ObjName(pFanin)) {
                    ex[ithPi] = ((cube[ithFanin] % 2) == 0) ? '0' : '1';
                } 
            }
        }
        
        // output result
        std::cout << "asymmetric\n";
        // output pattern f_01
        ex[in0_idx] = '0';
        ex[in1_idx] = '1';
        std::cout << ex << std::endl;
        // output pattern f_10
        ex[in0_idx] = '1';
        ex[in1_idx] = '0';
        std::cout << ex << std::endl;
    }

    Cudd_RecursiveDeref(dd, f_xor);
    if (f_01 != NULL) Cudd_RecursiveDeref(dd, f_01);
    if (f_10 != NULL) Cudd_RecursiveDeref(dd, f_10);
    return;
}

void Lsv_NtkSymSAT(Abc_Ntk_t* pNtk, int out_idx, int in0_idx, int in1_idx) {

    sat_solver* pSat;
    Abc_Ntk_t *outCone;
    Aig_Man_t *outCone_aig;
    Cnf_Dat_t *pCnfA, *pCnfB;
    int Lits[2];
    
    // use Abc_NtkCreateCone to extract the cone of yk
    outCone = Abc_NtkCreateCone(pNtk, Abc_ObjFanin0(Abc_NtkPo(pNtk, out_idx)), Abc_ObjName(Abc_NtkPo(pNtk, out_idx)), 1);
    // use Abc_NtkToDar to derive a corresponding AIG circuit
    outCone_aig = Abc_NtkToDar(outCone, 0, 1);
    // use sat solver new to initialize an SAT solver
    pSat = sat_solver_new();
    // use Cnf_Derive to obtain the corresponding CNF formula CA, which depends on variables v1 to vn
    pCnfA = Cnf_Derive(outCone_aig, 1);
    // use Cnf_DataWriteIntoSolverInt to add the CNF into the SAT solver
    Cnf_DataWriteIntoSolverInt(pSat, pCnfA, 1, 0);
    // use Cnf_DataLift to create another CNF formula CB that depends on different input variables vn+1 to v2n
    pCnfB = Cnf_Derive(outCone_aig, 1);
    Cnf_DataLift(pCnfB, pCnfA->nVars);
    // add the other CNF into the SAT solver.
    Cnf_DataWriteIntoSolverInt(pSat, pCnfB, 1, 0);


    Aig_Obj_t* pObj;
    int ithObj = -1;

    Aig_ManForEachCi(outCone_aig, pObj, ithObj) {
        if (in0_idx == ithObj) {
            Lits[0] = toLitCond(pCnfA->pVarNums[pObj->Id], 0);
            assert(sat_solver_addclause(pSat, Lits, Lits + 1));
            Lits[0] = toLitCond(pCnfB->pVarNums[pObj->Id], 1);
            assert(sat_solver_addclause(pSat, Lits, Lits + 1));
        }
        else if (in1_idx == ithObj) {
            Lits[0] = toLitCond(pCnfA->pVarNums[pObj->Id], 1);
            assert(sat_solver_addclause(pSat, Lits, Lits + 1));
            Lits[0] = toLitCond(pCnfB->pVarNums[pObj->Id], 0);
            assert(sat_solver_addclause(pSat, Lits, Lits + 1));
        }
        else {
            Lits[0] = toLitCond(pCnfA->pVarNums[pObj->Id], 0);
            Lits[1] = toLitCond(pCnfB->pVarNums[pObj->Id], 1);
            assert(sat_solver_addclause(pSat, Lits, Lits + 2));
            Lits[0] = toLitCond(pCnfA->pVarNums[pObj->Id], 1);
            Lits[1] = toLitCond(pCnfB->pVarNums[pObj->Id], 0);
            assert(sat_solver_addclause(pSat, Lits, Lits + 2));
        }
    }

    ithObj = -1;
    Aig_ManForEachCo(outCone_aig, pObj, ithObj) 
    {
        Lits[0] = toLitCond(pCnfA->pVarNums[pObj->Id], 0);
        Lits[1] = toLitCond(pCnfB->pVarNums[pObj->Id], 0);
        assert(sat_solver_addclause(pSat, Lits, Lits + 2));
        Lits[0] = toLitCond(pCnfA->pVarNums[pObj->Id], 1);
        Lits[1] = toLitCond(pCnfB->pVarNums[pObj->Id], 1);
        assert(sat_solver_addclause(pSat, Lits, Lits + 2));
    }

    int status = sat_solver_solve(pSat, NULL, NULL, 0, 0, 0, 0);

    if (status == l_False) {
        std::cout << "symmetric" << std::endl;
    }
    else if (status == l_True) 
    {
        std::cout << "asymmetric" << std::endl;

        // pattern 0
        ithObj = -1;
        Aig_ManForEachCi(outCone_aig, pObj, ithObj) {
            printf("%d", sat_solver_var_value(pSat, pCnfA->pVarNums[pObj->Id]));
        }
        std::cout << std::endl;

        // pattern 1
        ithObj = -1;
        Aig_ManForEachCi(outCone_aig, pObj, ithObj) {
            printf("%d", sat_solver_var_value(pSat, pCnfB->pVarNums[pObj->Id]));
        }
        std::cout << std::endl;
    } 
    else 
    {
        std::cerr << "solver error!!!" << std::endl;
    }
}

void Lsv_NtkSymAll(Abc_Ntk_t* pNtk, int out_idx) {

    sat_solver* pSat;
    Abc_Ntk_t *outCone;
    Aig_Man_t *outCone_aig;
    Cnf_Dat_t *pCnfA, *pCnfB, *pCnfH;
    int Lits[4];

    outCone = Abc_NtkCreateCone(pNtk, Abc_ObjFanin0(Abc_NtkPo(pNtk, out_idx)), Abc_ObjName(Abc_NtkPo(pNtk, out_idx)), 1);
    outCone_aig = Abc_NtkToDar(outCone, 0, 1);
    pSat = sat_solver_new();

    pCnfA = Cnf_Derive(outCone_aig, 1);
    Cnf_DataWriteIntoSolverInt(pSat, pCnfA, 1, 0);
    
    pCnfB = Cnf_Derive(outCone_aig, 1);
    Cnf_DataLift(pCnfB, pCnfA->nVars);
    Cnf_DataWriteIntoSolverInt(pSat, pCnfB, 1, 0);

    pCnfH = Cnf_Derive(outCone_aig, 1);
    Cnf_DataLift(pCnfH, pCnfB->nVars * 2);
    Cnf_DataWriteIntoSolverInt(pSat, pCnfH, 1, 0);

    Aig_Obj_t* pObj;
    int ithObj = -1;

    // constraint (a)
    Aig_ManForEachCo(outCone_aig, pObj, ithObj) 
    {
        Lits[0] = toLitCond(pCnfA->pVarNums[pObj->Id], 0);
        Lits[1] = toLitCond(pCnfB->pVarNums[pObj->Id], 0);
        if (!sat_solver_addclause(pSat, Lits, Lits + 2)) 
            Abc_Print(-1, "SAT solver add error!");
        Lits[0] = toLitCond(pCnfA->pVarNums[pObj->Id], 1);
        Lits[1] = toLitCond(pCnfB->pVarNums[pObj->Id], 1);
        if (!sat_solver_addclause(pSat, Lits, Lits + 2)) 
            Abc_Print(-1, "SAT solver add error!");
    }

    ithObj = -1;

    // constraint (b)
    Aig_ManForEachCi(outCone_aig, pObj, ithObj) {
        Lits[0] = toLitCond(pCnfA->pVarNums[pObj->Id], 0);
        Lits[1] = toLitCond(pCnfB->pVarNums[pObj->Id], 1);
        Lits[2] = toLitCond(pCnfH->pVarNums[pObj->Id], 0);
        if (!sat_solver_addclause(pSat, Lits, Lits + 3)) 
            Abc_Print(-1, "SAT solver add error!");
        assert(sat_solver_addclause(pSat, Lits, Lits + 3));
        Lits[0] = toLitCond(pCnfA->pVarNums[pObj->Id], 1);
        Lits[1] = toLitCond(pCnfB->pVarNums[pObj->Id], 0);
        Lits[2] = toLitCond(pCnfH->pVarNums[pObj->Id], 0);
        if (!sat_solver_addclause(pSat, Lits, Lits + 3)) 
            Abc_Print(-1, "SAT solver add error!");
    }

    // constraint (c), (d)
    for (int i = 0; i < Aig_ManCiNum(outCone_aig) - 1; i++) {
        for (int j = i + 1; j < Aig_ManCiNum(outCone_aig); j++) {
            Lits[0] = toLitCond(pCnfA->pVarNums[Aig_ManCi(outCone_aig, i)->Id], 0);
            Lits[1] = toLitCond(pCnfA->pVarNums[Aig_ManCi(outCone_aig, j)->Id], 0);
            Lits[2] = toLitCond(pCnfH->pVarNums[Aig_ManCi(outCone_aig, i)->Id], 1);
            Lits[3] = toLitCond(pCnfH->pVarNums[Aig_ManCi(outCone_aig, j)->Id], 1);
            if (!sat_solver_addclause(pSat, Lits, Lits + 4)) 
                Abc_Print(-1, "SAT solver add error!");

            Lits[0] = toLitCond(pCnfA->pVarNums[Aig_ManCi(outCone_aig, i)->Id], 1);
            Lits[1] = toLitCond(pCnfA->pVarNums[Aig_ManCi(outCone_aig, j)->Id], 1);
            Lits[2] = toLitCond(pCnfH->pVarNums[Aig_ManCi(outCone_aig, i)->Id], 1);
            Lits[3] = toLitCond(pCnfH->pVarNums[Aig_ManCi(outCone_aig, j)->Id], 1);
            if (!sat_solver_addclause(pSat, Lits, Lits + 4)) 
                Abc_Print(-1, "SAT solver add error!");

            Lits[0] = toLitCond(pCnfB->pVarNums[Aig_ManCi(outCone_aig, i)->Id], 0);
            Lits[1] = toLitCond(pCnfB->pVarNums[Aig_ManCi(outCone_aig, j)->Id], 0);
            Lits[2] = toLitCond(pCnfH->pVarNums[Aig_ManCi(outCone_aig, i)->Id], 1);
            Lits[3] = toLitCond(pCnfH->pVarNums[Aig_ManCi(outCone_aig, j)->Id], 1);
            if (!sat_solver_addclause(pSat, Lits, Lits + 4)) 
                Abc_Print(-1, "SAT solver add error!");

            Lits[0] = toLitCond(pCnfB->pVarNums[Aig_ManCi(outCone_aig, i)->Id], 1);
            Lits[1] = toLitCond(pCnfB->pVarNums[Aig_ManCi(outCone_aig, j)->Id], 1);
            Lits[2] = toLitCond(pCnfH->pVarNums[Aig_ManCi(outCone_aig, i)->Id], 1);
            Lits[3] = toLitCond(pCnfH->pVarNums[Aig_ManCi(outCone_aig, j)->Id], 1);
            if (!sat_solver_addclause(pSat, Lits, Lits + 4)) 
                Abc_Print(-1, "SAT solver add error!");
        }
    }

    std::vector< std::vector< bool > > symTable(Aig_ManCiNum(outCone_aig));

    for (int i = 0; i < Aig_ManCiNum(outCone_aig); i++) {
        symTable[i].resize(Aig_ManCiNum(outCone_aig));
        for (int j = 0; j < Aig_ManCiNum(outCone_aig); j++) {
            symTable[i][j] = false;
        }
    }
    
    for (int i = 0; i < Aig_ManCiNum(outCone_aig) - 1; i++) {
        for (int j = i + 1; j < Aig_ManCiNum(outCone_aig); j++) {
            if (symTable[i][j]) {
                std::cout << i << " " << j << std::endl;
                continue;
            }

            Lits[0] = toLitCond(pCnfH->pVarNums[Aig_ManCi(outCone_aig, i)->Id], 0);
            Lits[1] = toLitCond(pCnfH->pVarNums[Aig_ManCi(outCone_aig, j)->Id], 0);

            int status = sat_solver_solve(pSat, Lits, Lits + 2, 0, 0, 0, 0);
            if (status == l_False) {
                std::cout << i << " " << j << std::endl;
                symTable[i][j] = true;
            }
        }
        for (int j = 0; j < Aig_ManCiNum(outCone_aig) - 1; j++) {
            for (int k = j + 1; k < Aig_ManCiNum(outCone_aig); k++) {
                if (symTable[i][j] && symTable[i][k]) {
                    symTable[j][k] = true;
                }
            }
        }              
    }
}

int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv) {
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    int c;
    Extra_UtilGetoptReset();
    while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
        switch (c) {
        case 'h':
            goto usage;
        default:
            goto usage;
        }
    }
    if (!pNtk) {
        Abc_Print(-1, "Empty network.\n");
        return 1;
    }
    Lsv_NtkPrintNodes(pNtk);
    return 0;

usage:
    Abc_Print(-2, "usage: lsv_print_nodes [-h]\n");
    Abc_Print(-2, "\t        prints the nodes in the network\n");
    Abc_Print(-2, "\t-h    : print the command usage\n");
    return 1;
}

int Lsv_CommandSimBdd(Abc_Frame_t* pAbc, int argc, char** argv) {
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    int c;
    Extra_UtilGetoptReset();
    while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
        switch (c) {
        case 'h':
            goto usage;
        default:
            goto usage;
        }
    }

    if (!pNtk) {
        Abc_Print(-1, "Empty network.\n");
        return 1;
    }
    Lsv_NtkSimBdd(pNtk, argv[1]);
    return 0;

usage:
    Abc_Print(-2, "usage: lsv_sim_bdd [-h]\n");
    Abc_Print(-2, "\t        simulates for a given BDD and an input pattern\n");
    Abc_Print(-2, "\t-h    : print the command usage\n");
    return 1;
}

int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv) {

    Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
    std::vector<std::string> patterns;
    char *patternFile = NULL;

    int c;
    Extra_UtilGetoptReset();
    while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
        switch (c) {
        case 'h':
            goto usage;
        default:
            goto usage;
        }
    }

    patternFile = argv[1];

    // Read input patterns from the file
    patterns = Lsv_ReadInputPatterns(patternFile, pNtk);

    // Check if pattern reading was successful
    if (patterns.size() == 0) {
        Abc_Print(-1, "Error: Failed to read input patterns from file %s\n", patternFile);
        return 1;
    }

    // Call the simulation function
    Lsv_NtkSimAig(pNtk, patterns);

    return 0;

usage:
    Abc_Print(-2, "usage: lsv_sim_aig [-h]\n");
    Abc_Print(-2, "\t        simulates for a given AIG and input patterns\n");
    Abc_Print(-2, "\t-h    : print the command usage\n");
    return 1;
}

int Lsv_CommandSymBdd(Abc_Frame_t* pAbc, int argc, char** argv) {

    Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
    int out_idx = -1, in0_idx = -1, in1_idx = -1;
    int c;
    Extra_UtilGetoptReset();

    if (argc != 4) {
        goto usage;
    }

    // check if Network exist
    if (pNtk == NULL) {
        Abc_Print(-1, "Empty network.\n");
        return 1;
    }
    if (!Abc_NtkIsBddLogic(pNtk)) {
        Abc_Print(-1, "Simulating BDDs can only be done for BDD networks (run \"collapse\").\n");
        return 1;
    }

    while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
        switch (c) {
        case 'h':
            goto usage;
        default:
            goto usage;
        }
    }

    // read output pin index starting from 0
    out_idx = std::stoi(argv[1]);
    // read input pin indices starting from 0
    in0_idx = std::stoi(argv[2]);
    in1_idx = std::stoi(argv[3]);

    // check if the indices is valid
    if (in0_idx > in1_idx) {
        int temp = in0_idx;
        in0_idx = in1_idx;
        in1_idx = temp;
    }
    if ((in0_idx >= Abc_NtkPiNum(pNtk)) || (in1_idx >= Abc_NtkPiNum(pNtk))) {
        Abc_Print(-2, "Input indices i and j has to be less than the number of primary inputs.\n");
        goto usage;
    }
    if (out_idx >= Abc_NtkPoNum(pNtk)) {
        Abc_Print(-2, "Output index k has to be less than the number of primary outputs.\n");
        goto usage;
    }
    if (in0_idx == in1_idx) {
        Abc_Print(-2, "Input indices i and j has to be different.\n");
        goto usage;
    }

    // Call the simulation function
    Lsv_NtkSymBdd(pNtk, out_idx, in0_idx, in1_idx);

    return 0;

usage:
    Abc_Print(-2, "usage: lsv_sym_bdd <k> <i> <j> [-h]\n");
    Abc_Print(-2, "\t        Symmetry Checking with BDD, \n");
    Abc_Print(-2, "\t        where k is the output pin index starting from 0, \n");
    Abc_Print(-2, "\t        and i and j are input variable indexes starting from 0.\n");
    Abc_Print(-2, "\t-h    : print the command usage\n");
    return 1;
}

int Lsv_CommandSymSAT(Abc_Frame_t* pAbc, int argc, char** argv) {

    Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
    int out_idx = -1, in0_idx = -1, in1_idx = -1;
    int c;
    Extra_UtilGetoptReset();

    if (argc != 4) {
        goto usage;
    }

    // check if Network exist
    if (pNtk == NULL) {
        Abc_Print(-1, "Empty network.\n");
        return 1;
    }
    if (!Abc_NtkIsStrash(pNtk)) {
        Abc_Print(-1, "Simulating AIGs can only be done for AIG networks (run \"strash\").\n");
        return 1;
    }

    while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
        switch (c) {
        case 'h':
            goto usage;
        default:
            goto usage;
        }
    }

    // read output pin index starting from 0
    out_idx = std::stoi(argv[1]);
    // read input pin indices starting from 0
    in0_idx = std::stoi(argv[2]);
    in1_idx = std::stoi(argv[3]);

    // check if the indices is valid
    if (in0_idx > in1_idx) {
        int temp = in0_idx;
        in0_idx = in1_idx;
        in1_idx = temp;
    }
    if ((in0_idx >= Abc_NtkPiNum(pNtk)) || (in1_idx >= Abc_NtkPiNum(pNtk))) {
        Abc_Print(-2, "Input indices i and j has to be less than the number of primary inputs.\n");
        goto usage;
    }
    if (out_idx >= Abc_NtkPoNum(pNtk)) {
        Abc_Print(-2, "Output index k has to be less than the number of primary outputs.\n");
        goto usage;
    }
    if (in0_idx == in1_idx) {
        Abc_Print(-2, "Input indices i and j has to be different.\n");
        goto usage;
    }

    // Call the simulation function
    Lsv_NtkSymSAT(pNtk, out_idx, in0_idx, in1_idx);

    return 0;

usage:
    Abc_Print(-2, "usage: lsv_sym_sat <k> <i> <j> [-h]\n");
    Abc_Print(-2, "\t        Symmetry Checking with SAT, \n");
    Abc_Print(-2, "\t        where k is the output pin index starting from 0, \n");
    Abc_Print(-2, "\t        and i and j are input variable indexes starting from 0.\n");
    Abc_Print(-2, "\t-h    : print the command usage\n");
    return 1;
}

int Lsv_CommandSymAll(Abc_Frame_t* pAbc, int argc, char** argv) {

    Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
    int out_idx = -1;
    int c;
    Extra_UtilGetoptReset();

    if (argc != 2) {
        goto usage;
    }

    // check if Network exist
    if (pNtk == NULL) {
        Abc_Print(-1, "Empty network.\n");
        return 1;
    }
    if (!Abc_NtkIsStrash(pNtk)) {
        Abc_Print(-1, "Simulating AIGs can only be done for AIG networks (run \"strash\").\n");
        return 1;
    }

    while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
        switch (c) {
        case 'h':
            goto usage;
        default:
            goto usage;
        }
    }

    if (out_idx >= Abc_NtkPoNum(pNtk)) {
        Abc_Print(-2, "Output index k has to be less than the number of primary outputs.\n");
        goto usage;
    }

    // read output pin index starting from 0
    out_idx = std::stoi(argv[1]);

    // Call the simulation function
    Lsv_NtkSymAll(pNtk, out_idx);

    return 0;

usage:
    Abc_Print(-2, "usage: lsv_sym_all <k> [-h]\n");
    Abc_Print(-2, "\t        Symmetry Checking with Incremental SAT, \n");
    Abc_Print(-2, "\t        where k is the output pin index starting from 0, \n");
    Abc_Print(-2, "\t-h    : print the command usage\n");
    return 1;
}