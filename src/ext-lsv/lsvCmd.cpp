#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
//add library for problem 4.1 and 4.2
#include <string>
#include <iostream>
#include <fstream>
#include "sat/cnf/cnf.h"
extern "C" {
    Aig_Man_t* Abc_NtkToDar(Abc_Ntk_t* pNtk, int fExors, int fRegisters);
}

using namespace std;

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
//add function for problem 4.1 and 4.2
static int Lsv_CommandSymBDD(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSymSAT(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
    Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
    //add command for problem 4.1 and 4.2
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_bdd", Lsv_CommandSymBDD, 0);
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_sat", Lsv_CommandSymSAT, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

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

// problem 4.1 procedure part
void Lsv_NtkSymBDD(Abc_Ntk_t* pNtk, int k, int i, int j) {
    if (i == j) {
        printf("symmetric\n");
    }
    else {
        //Abc_Obj_t* pPi_i = Abc_NtkPi(pNtk, i);
        //Abc_Obj_t* pPi_j = Abc_NtkPi(pNtk, j);
        Abc_Obj_t* pPo_k = Abc_NtkPo(pNtk, k);;
        //printf("i: %s\n", Abc_ObjName(pPi_i));
        //printf("j: %s\n", Abc_ObjName(pPi_j));
        //printf("k: %s\n", Abc_ObjName(pPo_k));

        Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo_k);
        DdManager* root_dd = (DdManager*)pRoot->pNtk->pManFunc;
        DdNode* root_ddnode = (DdNode*)pRoot->pData;

        char** bdd_pi_name_list = (char**)Abc_NodeGetFaninNames(pRoot)->pArray;

        int pi_net2bdd[Abc_NtkPiNum(pNtk)];
        int pi_bdd2net[Abc_NtkPiNum(pNtk)];
        int ans1[Abc_NtkPiNum(pNtk)];
        int ans2[Abc_NtkPiNum(pNtk)];
        for (int m = 0; m < Abc_NtkPiNum(pNtk); m++) {
            pi_net2bdd[m] = -1;
            pi_bdd2net[m] = -1;
            ans1[m] = 0;
            ans2[m] = 0;
        }
        ans1[i] = 1;
        ans2[j] = 1;

        Abc_Obj_t* pPi;
        int i_Pi;
        for (int m = 0; m < Abc_NtkPiNum(pNtk); m++) {
            if (bdd_pi_name_list[m] != NULL) {
                if (bdd_pi_name_list[m][0] != '\0') {
                    Abc_NtkForEachPi(pNtk, pPi, i_Pi) {
                        string a = bdd_pi_name_list[m];
                        string b = Abc_ObjName(pPi);
                        if (a == b) {
                            pi_net2bdd[i_Pi] = m;
                            pi_bdd2net[m] = i_Pi;
                        }
                    }
                }
            }
        }
        if (pi_net2bdd[i] == -1) {
            if (pi_net2bdd[j] == -1) {
                printf("symmetric\n");
            }
            else {
                DdNode* var_j = Cudd_bddIthVar(root_dd, pi_net2bdd[j]);
                Cudd_Ref(var_j);
                DdNode* var_j_comp = Cudd_Not(var_j);
                Cudd_Ref(var_j_comp);

                DdNode* all_i1_j0 = Cudd_Cofactor(root_dd, root_ddnode, var_j_comp);
                Cudd_Ref(all_i1_j0);
                DdNode* all_i0_j1 = Cudd_Cofactor(root_dd, root_ddnode, var_j);
                Cudd_Ref(all_i0_j1);

                DdNode* all_delta = Cudd_bddXor(root_dd, all_i1_j0, all_i0_j1);
                Cudd_Ref(all_delta);

                DdNode* one = DD_ONE(root_dd);
                printf("asymmetric\n");
                for (int m = 0; m < Abc_NtkPiNum(pNtk); m++) {
                    if (pi_bdd2net[m] >= 0) {
                        if (pi_bdd2net[m] != i) {
                            if (pi_bdd2net[m] != j) {
                                DdNode* var_m = Cudd_bddIthVar(root_dd, m);
                                Cudd_Ref(var_m);
                                DdNode* var_m_comp = Cudd_Not(var_m);
                                Cudd_Ref(var_m_comp);
                                DdNode* all_d_m1 = Cudd_Cofactor(root_dd, all_delta, var_m);
                                Cudd_Ref(all_d_m1);
                                DdNode* all_d_m1_comp = Cudd_Not(all_d_m1);
                                Cudd_Ref(all_d_m1_comp);
                                if (all_d_m1_comp == one) {
                                    all_delta = Cudd_Cofactor(root_dd, all_delta, var_m_comp);
                                }
                                else {
                                    all_delta = all_d_m1;
                                    ans1[pi_bdd2net[m]] = 1;
                                    ans2[pi_bdd2net[m]] = 1;
                                }
                                Cudd_RecursiveDeref(root_dd, var_m);
                                Cudd_RecursiveDeref(root_dd, var_m_comp);
                                Cudd_RecursiveDeref(root_dd, all_d_m1);
                                Cudd_RecursiveDeref(root_dd, all_d_m1_comp);
                            }
                        }
                    }
                    else {
                        break;
                    }
                }

                for (int m = 0; m < Abc_NtkPiNum(pNtk); m++) {
                    cout << ans1[m];
                }
                cout << endl;
                for (int m = 0; m < Abc_NtkPiNum(pNtk); m++) {
                    cout << ans2[m];
                }
                cout << endl;

                Cudd_RecursiveDeref(root_dd, var_j);
                Cudd_RecursiveDeref(root_dd, var_j_comp);
                Cudd_RecursiveDeref(root_dd, all_i1_j0);
                Cudd_RecursiveDeref(root_dd, all_i0_j1);
                Cudd_RecursiveDeref(root_dd, all_delta);
            }
        }
        else {
            if (pi_net2bdd[j] == -1) {
                DdNode* var_i = Cudd_bddIthVar(root_dd, pi_net2bdd[i]);
                Cudd_Ref(var_i);
                DdNode* var_i_comp = Cudd_Not(var_i);
                Cudd_Ref(var_i_comp);

                DdNode* all_i1_j0 = Cudd_Cofactor(root_dd, root_ddnode, var_i);
                Cudd_Ref(all_i1_j0);
                DdNode* all_i0_j1 = Cudd_Cofactor(root_dd, root_ddnode, var_i_comp);
                Cudd_Ref(all_i0_j1);

                DdNode* all_delta = Cudd_bddXor(root_dd, all_i1_j0, all_i0_j1);
                Cudd_Ref(all_delta);

                DdNode* one = DD_ONE(root_dd);
                printf("asymmetric\n");
                for (int m = 0; m < Abc_NtkPiNum(pNtk); m++) {
                    if (pi_bdd2net[m] >= 0) {
                        if (pi_bdd2net[m] != i) {
                            if (pi_bdd2net[m] != j) {
                                DdNode* var_m = Cudd_bddIthVar(root_dd, m);
                                Cudd_Ref(var_m);
                                DdNode* var_m_comp = Cudd_Not(var_m);
                                Cudd_Ref(var_m_comp);
                                DdNode* all_d_m1 = Cudd_Cofactor(root_dd, all_delta, var_m);
                                Cudd_Ref(all_d_m1);
                                DdNode* all_d_m1_comp = Cudd_Not(all_d_m1);
                                Cudd_Ref(all_d_m1_comp);
                                if (all_d_m1_comp == one) {
                                    all_delta = Cudd_Cofactor(root_dd, all_delta, var_m_comp);
                                }
                                else {
                                    all_delta = all_d_m1;
                                    ans1[pi_bdd2net[m]] = 1;
                                    ans2[pi_bdd2net[m]] = 1;
                                }
                                Cudd_RecursiveDeref(root_dd, var_m);
                                Cudd_RecursiveDeref(root_dd, var_m_comp);
                                Cudd_RecursiveDeref(root_dd, all_d_m1);
                                Cudd_RecursiveDeref(root_dd, all_d_m1_comp);
                            }
                        }
                    }
                    else {
                        break;
                    }
                }

                for (int m = 0; m < Abc_NtkPiNum(pNtk); m++) {
                    cout << ans1[m];
                }
                cout << endl;
                for (int m = 0; m < Abc_NtkPiNum(pNtk); m++) {
                    cout << ans2[m];
                }
                cout << endl;

                Cudd_RecursiveDeref(root_dd, var_i);
                Cudd_RecursiveDeref(root_dd, var_i_comp);
                Cudd_RecursiveDeref(root_dd, all_i1_j0);
                Cudd_RecursiveDeref(root_dd, all_i0_j1);
                Cudd_RecursiveDeref(root_dd, all_delta);
            }
            else {
                DdNode* var_i = Cudd_bddIthVar(root_dd, pi_net2bdd[i]);
                Cudd_Ref(var_i);
                DdNode* var_i_comp = Cudd_Not(var_i);
                Cudd_Ref(var_i_comp);

                DdNode* var_j = Cudd_bddIthVar(root_dd, pi_net2bdd[j]);
                Cudd_Ref(var_j);
                DdNode* var_j_comp = Cudd_Not(var_j);
                Cudd_Ref(var_j_comp);

                DdNode* all_i1 = Cudd_Cofactor(root_dd, root_ddnode, var_i);
                Cudd_Ref(all_i1);
                DdNode* all_i0 = Cudd_Cofactor(root_dd, root_ddnode, var_i_comp);
                Cudd_Ref(all_i0);

                DdNode* all_i1_j0 = Cudd_Cofactor(root_dd, all_i1, var_j_comp);
                Cudd_Ref(all_i1_j0);
                DdNode* all_i0_j1 = Cudd_Cofactor(root_dd, all_i0, var_j);
                Cudd_Ref(all_i0_j1);

                DdNode* all_delta = Cudd_bddXor(root_dd, all_i1_j0, all_i0_j1);
                Cudd_Ref(all_delta);
                DdNode* all_delta_comp = Cudd_Not(all_delta);
                Cudd_Ref(all_delta_comp);

                DdNode* one = DD_ONE(root_dd);
                if (all_delta_comp == one) {
                    printf("symmetric\n");
                }
                else {
                    printf("asymmetric\n");
                    for (int m = 0; m < Abc_NtkPiNum(pNtk); m++) {
                        if (pi_bdd2net[m] >= 0) {
                            if (pi_bdd2net[m] != i) {
                                if (pi_bdd2net[m] != j) {
                                    DdNode* var_m = Cudd_bddIthVar(root_dd, m);
                                    Cudd_Ref(var_m);
                                    DdNode* var_m_comp = Cudd_Not(var_m);
                                    Cudd_Ref(var_m_comp);
                                    DdNode* all_d_m1 = Cudd_Cofactor(root_dd, all_delta, var_m);
                                    Cudd_Ref(all_d_m1);
                                    DdNode* all_d_m1_comp = Cudd_Not(all_d_m1);
                                    Cudd_Ref(all_d_m1_comp);
                                    if (all_d_m1_comp == one) {
                                        all_delta = Cudd_Cofactor(root_dd, all_delta, var_m_comp);
                                    }
                                    else {
                                        all_delta = all_d_m1;
                                        ans1[pi_bdd2net[m]] = 1;
                                        ans2[pi_bdd2net[m]] = 1;
                                    }
                                    Cudd_RecursiveDeref(root_dd, var_m);
                                    Cudd_RecursiveDeref(root_dd, var_m_comp);
                                    Cudd_RecursiveDeref(root_dd, all_d_m1);
                                    Cudd_RecursiveDeref(root_dd, all_d_m1_comp);
                                }
                            }
                        }
                        else {
                            break;
                        }
                    }

                    for (int m = 0; m < Abc_NtkPiNum(pNtk); m++) {
                        cout << ans1[m];
                    }
                    cout << endl;
                    for (int m = 0; m < Abc_NtkPiNum(pNtk); m++) {
                        cout << ans2[m];
                    }
                    cout << endl;
                }

                Cudd_RecursiveDeref(root_dd, var_i);
                Cudd_RecursiveDeref(root_dd, var_i_comp);
                Cudd_RecursiveDeref(root_dd, var_j);
                Cudd_RecursiveDeref(root_dd, var_j_comp);
                Cudd_RecursiveDeref(root_dd, all_i1);
                Cudd_RecursiveDeref(root_dd, all_i0);
                Cudd_RecursiveDeref(root_dd, all_i1_j0);
                Cudd_RecursiveDeref(root_dd, all_i0_j1);
                Cudd_RecursiveDeref(root_dd, all_delta);
                Cudd_RecursiveDeref(root_dd, all_delta_comp);
            }
        }
    }
}

int Lsv_CommandSymBDD(Abc_Frame_t* pAbc, int argc, char** argv) {
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    if (!pNtk) {
        Abc_Print(-1, "Empty network.\n");
        return 1;
    }

    int input_idx[3] = { 0,0,0 };
    bool legal_input = false;
    if (argc == 4) {
        legal_input = true;
        for (int i = 1; i < 4; i++) {
            std::string origin_input = argv[i];
            for (int j = 0; j < origin_input.size(); j++) {
                bool legal_input_temp = false;
                if (origin_input[j] >= '0') {
                    if (origin_input[j] <= '9') {
                        legal_input_temp = true;
                    }
                }
                legal_input = legal_input && legal_input_temp;
                if (legal_input_temp) {
                    input_idx[i - 1] = stoi(origin_input);
                }
            }
        }
        if (Abc_NtkPoNum(pNtk) <= input_idx[0]) {
            legal_input = false;
        }
        if (Abc_NtkPiNum(pNtk) <= input_idx[1]) {
            legal_input = false;
        }
        if (Abc_NtkPiNum(pNtk) <= input_idx[2]) {
            legal_input = false;
        }
    }

    if (Abc_NtkIsBddLogic(pNtk)) {
        if (legal_input) {
            Lsv_NtkSymBDD(pNtk, input_idx[0], input_idx[1], input_idx[2]);
        }
        else {
            printf("illegal input.\n");
        }
    }
    else {
        printf("Not bdd now.\n");
    }

    return 0;
}

// problem 4.2 procedure part
void Lsv_NtkSymSAT(Abc_Ntk_t* pNtk, int k, int i, int j) {
    if (i == j) {
        printf("symmetric\n");
    }
    else {
        //Abc_Obj_t* pPi_i = Abc_NtkPi(pNtk, i);
        //Abc_Obj_t* pPi_j = Abc_NtkPi(pNtk, j);
        //Abc_Obj_t* pPo_k = Abc_NtkPo(pNtk, k);;
        //printf("i: %s\n", Abc_ObjName(pPi_i));
        //printf("j: %s\n", Abc_ObjName(pPi_j));
        //printf("k: %s\n", Abc_ObjName(pPo_k));

        Abc_Obj_t* pObj_k = Abc_NtkPo(pNtk, k);

        Abc_Ntk_t* y_cone = Abc_NtkCreateCone(pNtk, Abc_ObjFanin0(pObj_k), Abc_ObjName(pObj_k), 1);
        Aig_Man_t* y_sat = Abc_NtkToDar(y_cone, 0, 0);

        sat_solver* sym_sat = sat_solver_new();
        Cnf_Dat_t* sym_sat_cnf_1 = Cnf_Derive(y_sat, 1);
        Cnf_DataWriteIntoSolverInt(sym_sat, sym_sat_cnf_1, 1, 0);
        Cnf_Dat_t* sym_sat_cnf_2 = Cnf_Derive(y_sat, 1);
        Cnf_DataLift(sym_sat_cnf_2, Abc_ObjFanin0(Abc_NtkPo(y_cone, 0))->Id);
        Cnf_DataWriteIntoSolverInt(sym_sat, sym_sat_cnf_2, 1, 0);

        Abc_Obj_t* pObj;
        int i_Pi;
        int idx_to_var1[Abc_NtkPiNum(pNtk)];
        int idx_to_var2[Abc_NtkPiNum(pNtk)];
        Abc_NtkForEachPi(pNtk, pObj, i_Pi) {
            idx_to_var1[i_Pi] = sym_sat_cnf_1->pVarNums[pObj->Id];
            idx_to_var2[i_Pi] = sym_sat_cnf_2->pVarNums[pObj->Id];
            //cout << "cnf1: " << sym_sat_cnf_1->pVarNums[pObj->Id] << " at " << pObj->Id << " when " << i_Pi << endl;
            //cout << "cnf2: " << sym_sat_cnf_2->pVarNums[pObj->Id] << " at " << pObj->Id << " when " << i_Pi << endl;
        }
        Abc_NtkForEachPi(pNtk, pObj, i_Pi) {
            if (i_Pi == i) {
                lit cls_1_0 = toLitCond(idx_to_var1[i], 0);
                lit cls_1_1 = toLitCond(idx_to_var2[j], 1);
                sat_solver_addclause(sym_sat, &cls_1_0, &cls_1_1 + 1);
                lit cls_2_0 = toLitCond(idx_to_var2[j], 0);
                lit cls_2_1 = toLitCond(idx_to_var1[i], 1);
                sat_solver_addclause(sym_sat, &cls_2_0, &cls_2_1 + 1);
            }
            else if (i_Pi == j) {
                lit cls_1_0 = toLitCond(idx_to_var1[j], 0);
                lit cls_1_1 = toLitCond(idx_to_var2[i], 1);
                sat_solver_addclause(sym_sat, &cls_1_0, &cls_1_1 + 1);
                lit cls_2_0 = toLitCond(idx_to_var2[i], 0);
                lit cls_2_1 = toLitCond(idx_to_var1[j], 1);
                sat_solver_addclause(sym_sat, &cls_2_0, &cls_2_1 + 1);
            }
            else {
                lit cls_1_0 = toLitCond(idx_to_var1[i_Pi], 0);
                lit cls_1_1 = toLitCond(idx_to_var2[i_Pi], 1);
                sat_solver_addclause(sym_sat, &cls_1_0, &cls_1_1 + 1);
                lit cls_2_0 = toLitCond(idx_to_var2[i_Pi], 0);
                lit cls_2_1 = toLitCond(idx_to_var1[i_Pi], 1);
                sat_solver_addclause(sym_sat, &cls_2_0, &cls_2_1 + 1);
            }
        }
        
        int po_cone_id = Abc_ObjFanin0(Abc_NtkPo(y_cone, 0))->Id;
        int y_var1 = sym_sat_cnf_1->pVarNums[po_cone_id];
        int y_var2 = sym_sat_cnf_2->pVarNums[po_cone_id];
        //cout << "cnf1: " << y_var1 << " at " << po_cone_id << " when " << k << endl;
        //cout << "cnf2: " << y_var2 << " at " << po_cone_id << " when " << k << endl;
        lit cls_3_0 = toLitCond(y_var1, 0);
        lit cls_3_1 = toLitCond(y_var2, 0);
        sat_solver_addclause(sym_sat, &cls_3_0, &cls_3_1 + 1);
        lit cls_4_0 = toLitCond(y_var1, 1);
        lit cls_4_1 = toLitCond(y_var2, 1);
        sat_solver_addclause(sym_sat, &cls_4_0, &cls_4_1 + 1);
        
        int is_sym = sat_solver_solve(sym_sat, &cls_4_0, &cls_4_0, 0, 0, 0, 0);
        //cout << "result: " << is_sym << endl;

        if (is_sym == -1) {
            printf("symmetric\n");
        }
        else {
            printf("asymmetric\n");
            int ans1[Abc_NtkPiNum(pNtk)];
            int ans2[Abc_NtkPiNum(pNtk)];
            for (int m = 0; m < Abc_NtkPiNum(pNtk); m++) {
                ans1[m] = sat_solver_var_value(sym_sat, idx_to_var1[m]);
                ans2[m] = sat_solver_var_value(sym_sat, idx_to_var2[m]);
            }

            for (int m = 0; m < Abc_NtkPiNum(pNtk); m++) {
                cout << ans1[m];
            }
            cout << endl;
            for (int m = 0; m < Abc_NtkPiNum(pNtk); m++) {
                cout << ans2[m];
            }
            cout << endl;
        }
    }
}

int Lsv_CommandSymSAT(Abc_Frame_t* pAbc, int argc, char** argv) {
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    if (!pNtk) {
        Abc_Print(-1, "Empty network.\n");
        return 1;
    }

    int input_idx[3] = { 0,0,0 };
    bool legal_input = false;
    if (argc == 4) {
        legal_input = true;
        for (int i = 1; i < 4; i++) {
            std::string origin_input = argv[i];
            for (int j = 0; j < origin_input.size(); j++) {
                bool legal_input_temp = false;
                if (origin_input[j] >= '0') {
                    if (origin_input[j] <= '9') {
                        legal_input_temp = true;
                    }
                }
                legal_input = legal_input && legal_input_temp;
                if (legal_input_temp) {
                    input_idx[i - 1] = stoi(origin_input);
                }
            }
        }
        if (Abc_NtkPoNum(pNtk) <= input_idx[0]) {
            legal_input = false;
        }
        if (Abc_NtkPiNum(pNtk) <= input_idx[1]) {
            legal_input = false;
        }
        if (Abc_NtkPiNum(pNtk) <= input_idx[2]) {
            legal_input = false;
        }
    }

    if (Abc_NtkIsStrash(pNtk)) {
        if (legal_input) {
            Lsv_NtkSymSAT(pNtk, input_idx[0], input_idx[1], input_idx[2]);
        }
        else {
            printf("illegal input.\n");
        }
    }
    else {
        printf("Not strash now.\n");
    }

    return 0;
}