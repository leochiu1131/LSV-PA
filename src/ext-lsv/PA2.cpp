#include "base/abc/abc.h"
#include "sat/cnf/cnf.h"
#include "sat/bsat/satSolver.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"

extern "C" {
    Aig_Man_t* Abc_NtkToDar(Abc_Ntk_t* pNtk, int fExors, int fRegisters);
}

void Comparator(int careSet[4][2], int sol[1][2]) {
    for (int i = 0; i < 4; i++) {
        if (careSet[i][0] == sol[0][0] && careSet[i][1] == sol[0][1]) {
            careSet[i][0] = -1;
            careSet[i][1] = -1;
        }
    }
}

void check_assign(Abc_Ntk_t *pNtk, int nPI_Cone, int *patterns, int target_ID[1][2], int sol[1][2]) {
    Abc_Obj_t *pObj; 
    int i, fanin0_value, fanin1_value, ID0, ID1;
    int *nodeValues = (int*)malloc(Abc_NtkObjNumMax(pNtk) * sizeof(int));
    Abc_NtkForEachPi(pNtk, pObj, i) {
        nodeValues[Abc_ObjId(pObj)] = patterns[i];
    }
    Abc_NtkForEachNode(pNtk, pObj, i) {
        if (Abc_ObjFaninNum(pObj) == 2) {
            ID0 = Abc_ObjId(Abc_ObjFanin0(pObj));
            ID1 = Abc_ObjId(Abc_ObjFanin1(pObj));
            fanin0_value = nodeValues[ID0] ^ Abc_ObjFaninC0(pObj); // 考慮反相
            fanin1_value = nodeValues[ID1] ^ Abc_ObjFaninC1(pObj); // 考慮反相
            nodeValues[Abc_ObjId(pObj)] = fanin0_value && fanin1_value;
            if (ID0 == target_ID[0][0] && ID1 == target_ID[0][1]) {
                sol[0][0] = nodeValues[ID0];
                sol[0][1] = nodeValues[ID1];
                break;
            }
        } else if (Abc_ObjFaninNum(pObj) == 1) {
            ID0 = Abc_ObjId(Abc_ObjFanin0(pObj));
            fanin0_value = nodeValues[ID0] ^ Abc_ObjFaninC0(pObj);
            nodeValues[Abc_ObjId(pObj)] = fanin0_value;
            if (ID0 == target_ID[0][0]) {
                sol[0][0] = nodeValues[ID0];
                sol[0][1] = -1;  // 設定為 -1 以示該值無效
            }
        }
    }
    free(nodeValues);
}

int Lsv_CommandSDC(Abc_Frame_t* pAbc, int argc, char** argv) {
    srand(time(NULL));
    Abc_Obj_t *pObj; 
    int i, j, nFanin;
    int Sol[1][2];
    int Care_set[4][2];
    Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
    Abc_Obj_t *n = Abc_NtkObj(pNtk, atoi(argv[1]));
    int nSimulation = 10;
    nFanin = Abc_ObjFaninNum(n);
    if (nFanin < 1 || nFanin > 2){
        Abc_Print(1, "node with nfanin = %d, invalid node!!\n",nFanin);
        return 1;
    }
    if (nFanin == 1){
        Care_set[0][0] =  0; Care_set[0][1] = -1; 
        Care_set[1][0] =  1; Care_set[1][1] = -1;
        Care_set[2][0] = -1; Care_set[2][1] = -1; 
        Care_set[3][0] = -1; Care_set[3][1] = -1;
    } else {
        Care_set[0][0] =  0; Care_set[0][1] =  0; 
        Care_set[1][0] =  0; Care_set[1][1] =  1;
        Care_set[2][0] =  1; Care_set[2][1] =  0;
        Care_set[3][0] =  1; Care_set[3][1] =  1; 
    }
    Vec_Ptr_t *vFanins = Vec_PtrAlloc(2);
    Abc_ObjForEachFanin(n, pObj, i){
        Vec_PtrPush(vFanins, pObj);
    }
    Abc_Ntk_t *pCone = Abc_NtkCreateConeArray(pNtk, vFanins, 1 /* fUseAllCis */);
    int edge_array[1][2];
    int y_array[1][2];
    int target_ID[1][2];
    int nPI_Cone = Abc_NtkPiNum(pNtk);
    int *pattern = (int*)malloc(nPI_Cone * sizeof(int));
    Vec_PtrForEachEntry(Abc_Obj_t*, vFanins, pObj, i){
        target_ID[0][i] = Abc_ObjId(pObj);
    }
    int SDC_flag = 0;
    for (i = 0; i < nSimulation; i++) {
        for (j = 0; j < nPI_Cone; j++) {
            pattern[j] = rand() % 2;
        }
        check_assign(pNtk, nPI_Cone, pattern, target_ID, Sol);
        Comparator(Care_set, Sol);
    }
    for (i = 0; i < 4; i++){
        if (Care_set[i][0] == -1){
            continue;
        } else {
            int v0 = Care_set[i][0];
            int v1 = Care_set[i][1];
            Aig_Man_t* pAig = Abc_NtkToDar(pCone, 0, 0);
            Cnf_Dat_t* pCnf = Cnf_Derive(pAig, Aig_ManCoNum(pAig));
            sat_solver* pSatSolver = sat_solver_new();
            for (j = 0; j < pCnf->nClauses; j++) {
                sat_solver_addclause(pSatSolver, pCnf->pClauses[j], pCnf->pClauses[j + 1]);
            }
            Vec_PtrForEachEntry(Abc_Obj_t*, vFanins, pObj, j){
                edge_array[0][j] = Abc_ObjFaninC(n, j);
                y_array[0][j] = pCnf->pVarNums[Abc_ObjId(pObj)];

            }
            if (nFanin == 1){
                int assumptions[1] = {Abc_Var2Lit(y_array[0][0], v0==0)};
                int result = sat_solver_solve(pSatSolver, assumptions, assumptions + 1, 0, 0, 0, 0);
                if (result == l_False){
                    Abc_Print(1, "%d ", v0 != edge_array[0][0]);
                    SDC_flag = 1;
                }
            } else if (nFanin == 2){
                int assumptions[2] = {Abc_Var2Lit(y_array[0][0], v0==0), Abc_Var2Lit(y_array[0][1], v1==0)};
                int result = sat_solver_solve(pSatSolver, assumptions, assumptions + 2, 0, 0, 0, 0);
                if (result == l_False){
                    Abc_Print(1, "%d%d ", v0 != edge_array[0][0], v1 != edge_array[0][1]);
                    SDC_flag = 1;
                }
            sat_solver_delete(pSatSolver);
            }
        }
    }
    if (SDC_flag==0){
        Abc_Print(1, "no sdc\n");
    } else {
        Abc_Print(1, "\n");
    }
    Vec_PtrFree(vFanins);
    free(pattern);
    return 0;
}


//----------------------------------------------------------------------------------------------------------------------------------------------------------------
int Lsv_CommandODC(Abc_Frame_t* pAbc, int argc, char** argv) {
    Abc_Obj_t *pObj1, *pObj2; // Fanout and fanin nodes
    int i, j, size, iter, result;
    Vec_Ptr_t*  Cp_yArray = Vec_PtrAlloc(2);
    Vec_Ptr_t* SAT_yArray = Vec_PtrAlloc(2);
    int nodeId = atoi(argv[1]); // Read the target node ID
    // Step 1: create C
    Abc_Ntk_t* C = Abc_FrameReadNtk(pAbc);   // o for original
    Abc_Ntk_t* Cp = Abc_NtkDup(C);
    Abc_Obj_t* n_C = Abc_NtkObj(C, nodeId); // Get the target node
    Abc_Obj_t* n_Cp = Abc_NtkObj(Cp, nodeId); // Get the target node
    // Step 2: Do negation on c' (Cp) of node n (n_Cp)
    Abc_ObjForEachFanin(n_Cp, pObj1, i){
        Vec_PtrPush(Cp_yArray, pObj1);
    }
    size = Cp_yArray->nSize;
    int* edge_Array = (int*)malloc(size * sizeof(int));
    Abc_ObjForEachFanin(n_C, pObj1, i){
        edge_Array[i] = (Abc_ObjFaninC(n_C, i) == 1);
        //Abc_Print(1, "Fanin%d ID = %d Name = %s isinv_edge = %d\n", i, Abc_ObjId(pObj1), Abc_ObjName(pObj1), Abc_ObjFaninC(pObj1, i));
    }

    Abc_ObjForEachFanout(n_Cp, pObj1, i) {
        //Abc_Print(1, "Fanout of node ID = %d Name = %s is ID = %d Name %s\n", Abc_ObjId(n_Cp), Abc_ObjName(n_Cp), Abc_ObjId(pObj1), Abc_ObjName(pObj1));
        Abc_ObjForEachFanin(pObj1, pObj2, j){
            //Abc_Print(1, "Fanin%d Node ID = %d Name = %s isinv_edge = %d\n", j, Abc_ObjId(pObj2), Abc_ObjName(pObj2), Abc_ObjFaninC(pObj1, j));
            if (pObj2 == n_Cp){
                Abc_ObjXorFaninC(pObj1, j);
                //Abc_Print(1, "Fanin%d Node ID = %d Name = %s isinv_edge = %d\n", j, Abc_ObjId(pObj2), Abc_ObjName(pObj2), Abc_ObjFaninC(pObj1, j));
            }
        }
    }
    Abc_Ntk_t* pMiter = Abc_NtkMiter(C, Cp, 1, 0, 0, 0);
    Abc_Ntk_t* Cone = Abc_NtkCreateConeArray(Cp, Cp_yArray, 0);
    Abc_NtkAppend(pMiter, Cone, 1);
    //int numPIs = Abc_NtkPiNum(pMiter);
    Aig_Man_t* pAigMiter = Abc_NtkToDar(pMiter, 0, 0);
    Cnf_Dat_t* pCnfMiter = Cnf_Derive(pAigMiter, Aig_ManCoNum(pAigMiter));
    int cnf_var_count = pCnfMiter->nVars;
    //Abc_Print(1, "Mappping list of primary outputs with %d variables\n", cnf_var_count);
    Abc_NtkForEachPo(pMiter, pObj1, i){
        //Abc_Print(1, "ID = %d NAME = %s cnf_var = %d\n", Abc_ObjId(pObj1), Abc_ObjName(pObj1), pCnfMiter->pVarNums[Abc_ObjId(pObj1)]);
        if (i != 0){
            int cnfVarId = pCnfMiter->pVarNums[Abc_ObjId(pObj1)];
            if (cnfVarId >= 0 && cnfVarId <= cnf_var_count){
                //Abc_Print(1, "Need to solve cnf_var_ID = %d\n", cnfVarId);
                Vec_PtrPush(SAT_yArray, pObj1);
            } else if (cnfVarId > cnf_var_count) {
                //Abc_Print(1, "Skip cnf_var_ID = %d since don't care\n", cnfVarId);
            }
        }
    }
    size = SAT_yArray->nSize;
    //Abc_Print(1, "Have %d SAT condition to solve\n", size);
    if (size <= 0){
        Abc_Print(1, "no odc\n");
        return 0;
    }
    //Abc_Print(1, "n_SAT_variables = %d\n", size);
    sat_solver* pSatSolver = sat_solver_new();
    for (i = 0; i < pCnfMiter->nClauses; i++) {
        if (!sat_solver_addclause(pSatSolver, pCnfMiter->pClauses[i], pCnfMiter->pClauses[i + 1])) {
            //Abc_Print(1, "Failed to add clause %d to SAT solver.\n", i);
            return 1;
        }
    }
    int Sol[1][2];
    int Care_set[4][2];
    Care_set[0][0] =  0; Care_set[0][1] =  0; 
    Care_set[1][0] =  0; Care_set[1][1] =  1;
    Care_set[2][0] =  1; Care_set[2][1] =  0;
    Care_set[3][0] =  1; Care_set[3][1] =  1; 
    iter = 1 << size;
    int y0, y1, v0, v1, flag, x;
    flag = 1;
    for (i = 0; i < iter; i++) {
        result = sat_solver_solve(pSatSolver, NULL, NULL, 0, 0, 0, 0);
        if (result == l_True){ // True表示兩個不同
            Abc_NtkForEachPi(pMiter, pObj1, j) {
                x= pCnfMiter->pVarNums[Abc_ObjId(pObj1)];
                //Abc_Print(1, "SAT under cnf_var = %d value = %d\n", x, sat_solver_var_value(pSatSolver, x));
            }
            if (size == 1){
                y0 = pCnfMiter->pVarNums[Abc_ObjId((Abc_Obj_t*)Vec_PtrEntry(SAT_yArray, 0))];
                v0 = sat_solver_var_value(pSatSolver, y0);
                Sol[0][0] = (v0 == edge_Array[0]);
                //Abc_Print(1, "ODC exclude %d%d\n", Sol[0][0], 0);
                //Abc_Print(1, "ODC exclude %d%d\n", Sol[0][0], 1);
                Sol[0][1] = 0;
                Comparator(Care_set, Sol);
                Sol[0][1] = 1;
                Comparator(Care_set, Sol);
                int litBlock[1] = {Abc_Var2Lit(y0, v0 == flag)};
                sat_solver_addclause(pSatSolver, litBlock, litBlock + 1);
            } else {
                y0 = pCnfMiter->pVarNums[Abc_ObjId((Abc_Obj_t*)Vec_PtrEntry(SAT_yArray, 0))];
                y1 = pCnfMiter->pVarNums[Abc_ObjId((Abc_Obj_t*)Vec_PtrEntry(SAT_yArray, 1))];
                v0 = sat_solver_var_value(pSatSolver, y0);
                v1 = sat_solver_var_value(pSatSolver, y1);
                Sol[0][0] = (v0 == edge_Array[0]);
                Sol[0][1] = (v1 == edge_Array[1]);
                //Abc_Print(1, "SDC exclude%d%d\n", Sol[0][0], Sol[0][1]);
                Comparator(Care_set, Sol);
                int litBlock[2] = {Abc_Var2Lit(y0, v0 == flag), Abc_Var2Lit(y1, v1 == flag)};
                sat_solver_addclause(pSatSolver, litBlock, litBlock + 2);
            }
        }
        else if (result == l_False) {
            //Abc_Print(1, "No more sat condition\n");
            sat_solver_delete(pSatSolver);
            break;
        }else{
            //Abc_Print(1, "No more sat condition\n");
            sat_solver_delete(pSatSolver);
            break;
        }
    }
    //Abc_Print(1, "SDC union ODC\n");
    int no_ODC_flag = 1;
    for (int i = 0; i < 4; i++){
        if ((Care_set[i][0] != -1) && (Care_set[i][0] != -1)){
            //Abc_Print(1, "%d%d\n",Care_set[i][0], Care_set[i][1]);
            no_ODC_flag = 0;
        } else if ((Care_set[i][0] != -1) && (Care_set[i][0] == -1)){
            //Abc_Print(1, "%d\n",Care_set[i][0]);
            no_ODC_flag = 0;
        }
    }
    if (no_ODC_flag){
        Abc_Print(1, "no odc\n");
        return 0;
    }
    for (i = 0; i < 4; i++){
        if (Care_set[i][0] == -1){
            continue;
        } else {
            Vec_Ptr_t *vFanins_new = Vec_PtrAlloc(2);
            Abc_ObjForEachFanin(n_C, pObj1, j){
                Vec_PtrPush(vFanins_new, pObj1);
            }
            Abc_Ntk_t *pCone = Abc_NtkCreateConeArray(C, vFanins_new, 1 /* fUseAllCis */);
            int edge_array[1][2];
            int y_array[1][2];
            v0 = Care_set[i][0];
            v1 = Care_set[i][1];
            Aig_Man_t* pAig = Abc_NtkToDar(pCone, 0, 0);
            Cnf_Dat_t* pCnf = Cnf_Derive(pAig, Aig_ManCoNum(pAig));
            sat_solver* pSatSolver = sat_solver_new();
            for (j = 0; j < pCnf->nClauses; j++) {
                sat_solver_addclause(pSatSolver, pCnf->pClauses[j], pCnf->pClauses[j + 1]);
            }
            Vec_PtrForEachEntry(Abc_Obj_t*, vFanins_new, pObj1, j){
                edge_array[0][j] = Abc_ObjFaninC(n_C, j);
                y_array[0][j] = pCnf->pVarNums[Abc_ObjId(pObj1)];

            }
            int nFanin = Abc_ObjFaninNum(n_C);
            if (nFanin == 1){
                int assumptions[1] = {Abc_Var2Lit(y_array[0][0], v0==0)};
                int result = sat_solver_solve(pSatSolver, assumptions, assumptions + 1, 0, 0, 0, 0);
                if (result == l_False){
                    Sol[0][0] = (v0 != edge_Array[0]);
                    Sol[0][1] = 0;
                    Comparator(Care_set, Sol);
                    Sol[0][1] = 1;
                    Comparator(Care_set, Sol);
                }
            } else if (nFanin == 2){
                int assumptions[2] = {Abc_Var2Lit(y_array[0][0], v0==0), Abc_Var2Lit(y_array[0][1], v1==0)};
                int result = sat_solver_solve(pSatSolver, assumptions, assumptions + 2, 0, 0, 0, 0);
                    if (result == l_False){
                        Sol[0][0] = (v0 != edge_Array[0]);
                        Sol[0][1] = (v0 != edge_Array[0]);
                        Comparator(Care_set, Sol);
                    }
            }
            sat_solver_delete(pSatSolver);
            Vec_PtrFree(vFanins_new);
        }
    }
    no_ODC_flag = 1;
    for (int i = 0; i < 4; i++){
        if ((Care_set[i][0] != -1) && (Care_set[i][0] != -1)){
            Abc_Print(1, "%d%d ",Care_set[i][0], Care_set[i][1]);
            no_ODC_flag = 0;
        } else if ((Care_set[i][0] != -1) && (Care_set[i][0] == -1)){
            Abc_Print(1, "%d ",Care_set[i][0]);
            no_ODC_flag = 0;
        }
    }
    if (no_ODC_flag){
        Abc_Print(1, "no odc\n");
    } else {
        Abc_Print(1, "\n");
    }
    Vec_PtrFree( Cp_yArray);
    Vec_PtrFree(SAT_yArray);
    free(edge_Array);
    return 0;
}