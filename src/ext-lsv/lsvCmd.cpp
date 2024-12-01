#include <iostream>
#include <vector>
#include <string>
#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/cnf/cnf.h"

#include "base/abc/abcShow.c"
extern "C"{
    Aig_Man_t* Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
}
using namespace std;

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintSDC(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintODC(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sdc", Lsv_CommandPrintSDC, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_odc", Lsv_CommandPrintODC, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

void CreateCnfForFaninNodes(Abc_Ntk_t * pNtk, int n, Cnf_Dat_t ** ppCnf, sat_solver ** ppSolver, int * pVarY0, int * pVarY1, int * pComplY0, int * pComplY1) {
    // 獲取目標節點A及其fanin節點y0和y1
    Abc_Obj_t * pNodeA = Abc_NtkObj(pNtk, n);
    Abc_Obj_t * pFanin0 = Abc_ObjFanin0(pNodeA);
    Abc_Obj_t * pFanin1 = Abc_ObjFanin1(pNodeA);

    // 創建一個指針向量來存儲y0和y1
    Vec_Ptr_t * vNodes = Vec_PtrAlloc(2);
    Vec_PtrPush(vNodes, pFanin0);
    Vec_PtrPush(vNodes, pFanin1);
    // cout<<pNodeA->Id<<" : "<<pFanin0->Id<<" "<<pFanin1->Id<<endl;

    // 創建包含y0和y1的傳遞扇入錐
    Abc_Ntk_t * pConeNtk = Abc_NtkCreateConeArray(pNtk, vNodes, 1);
    Vec_PtrFree(vNodes);

    // 將傳遞扇入錐轉換為AIG
    Aig_Man_t * pAig = Abc_NtkToDar(pConeNtk, 0, 0);

    // 導出CNF，這裡假設只有一個輸出
    *ppCnf = Cnf_Derive(pAig, 2);

    // 初始化SAT求解器
    *ppSolver = sat_solver_new();

    // 將CNF寫入求解器
    // Cnf_DataWriteIntoSolver(*ppCnf, 1, 0);
    Cnf_DataWriteIntoSolverInt(*ppSolver, *ppCnf, 1, 0);

    // 獲取y0和y1的變量索引
    *pVarY0 = (*ppCnf)->pVarNums[Aig_ObjId(Aig_ManCo(pAig, 0))];
    *pVarY1 = (*ppCnf)->pVarNums[Aig_ObjId(Aig_ManCo(pAig, 1))];
    *pComplY0 = Abc_ObjFaninC0(pNodeA);
    *pComplY1 = Abc_ObjFaninC1(pNodeA);

    // 清理
    Aig_ManStop(pAig);
    Abc_NtkDelete(pConeNtk);
}

int CheckPattern(sat_solver * pSolver, Cnf_Dat_t * pCnf, int y0, int y1, int v0, int v1, int complY0, int complY1) {
    // 重設求解器
    sat_solver_restart(pSolver);

    // 將CNF重新寫入求解器
    // Cnf_DataWriteIntoSolver(pCnf, 1, 0);
    Cnf_DataWriteIntoSolverInt(pSolver, pCnf, 1, 0);

    // 提升CNF數據以處理假設條件
    // Cnf_Dat_t * pCnfLifted = Cnf_DataDup(pCnf);
    // Cnf_DataLift(pCnfLifted, sat_solver_nvars(pSolver));

    // 假設y0和y1的值分別為v0和v1
    int lit_y0 = Abc_Var2Lit(y0, !(v0 ^ complY0));
    int lit_y1 = Abc_Var2Lit(y1, !(v1 ^ complY1));

    // 添加假設條件
    sat_solver_addclause(pSolver, &lit_y0, &lit_y0 + 1);
    sat_solver_addclause(pSolver, &lit_y1, &lit_y1 + 1);

    // 求解CNF公式
    int status = sat_solver_solve(pSolver, NULL, NULL, 0, 0, 0, 0);
    return status;
}


Vec_Int_t * Lsv_NtkPrint_SDC(Abc_Ntk_t* pNtk, int n, bool print) {
  Abc_Obj_t * nodeN = Abc_NtkObj(pNtk, n);
  if(nodeN == NULL){
    cout<< "ERROR: NULL node\n";
    return NULL;
  }
  if(Abc_ObjFaninNum(nodeN) == 0){
    printf("no sdc\n");
    return NULL;
  }
    Cnf_Dat_t * pCnf;
    sat_solver * pSolver;
    int y0, y1, complY0, complY1;
    Vec_Int_t * vSDC = Vec_IntAlloc(4);

    // 創建包含y0和y1的傳遞扇入錐並導出CNF
    CreateCnfForFaninNodes(pNtk, n, &pCnf, &pSolver, &y0, &y1, &complY0, &complY1);
    // cout<<n <<": "<<y0<<", "<<y1<<"; "<<complY0<<", "<<complY1<<endl;


    // // 獲取y0和y1的變量索引
    // int y0 = pCnf->pVarNums[Abc_ObjId(Abc_ObjFanin0(Abc_NtkObj(pNtk, n)))];
    // int y1 = pCnf->pVarNums[Abc_ObjId(Abc_ObjFanin1(Abc_NtkObj(pNtk, n)))];

    // 檢查所有模式
    int patterns[4][2] = {{0, 0}, {0, 1}, {1, 0}, {1, 1}};
    for (int i = 0; i < 4; i++) {
        int v0 = patterns[i][0];
        int v1 = patterns[i][1];
        int status = CheckPattern(pSolver, pCnf, y0, y1, v0, v1, complY0, complY1);
        if (status == l_False) {
            // printf("Pattern (%d, %d) is an SDC.\n", v0, v1);
            Vec_IntPush(vSDC, (v0 << 1) | v1);
        } else {
            // printf("Pattern (%d, %d) is not an SDC.\n", v0, v1);
            // 檢查滿足條件的變量賦值
            // printf("Satisfying assignment for y0: %d\n", sat_solver_var_value(pSolver, y0));
            // printf("Satisfying assignment for y1: %d\n", sat_solver_var_value(pSolver, y1));
        }
    }

    if(print){
        // 打印所有SDC
      if (Vec_IntSize(vSDC) == 0) {
          printf("no sdc\n");
      } else {
          int Entry, i;
          Vec_IntForEachEntry(vSDC, Entry, i) {
              printf("%d%d ", (Entry >> 1) & 1, Entry & 1);
          }
          printf("\n");
      }
    }
    
    // 清理
    Cnf_DataFree(pCnf);
    sat_solver_delete(pSolver);

    return vSDC;

}

// 求解ALLSAT並獲取y0和y1上的所有滿足賦值
Vec_Int_t *  SolveAllSat(sat_solver * pSolver, int y0, int y1, Abc_Obj_t * nodeN) {
    Vec_Int_t * vCareSet = Vec_IntAlloc(4);
    // cout<<"Check ";
    int count=0;
    while (1) {
      count++;
        // 求解CNF公式
        int status = sat_solver_solve(pSolver, NULL, NULL, 0, 0, 0, 0);
        if (status != l_True) {
            break;
        }
        // cout<<"Check1 ";

        int v0 = sat_solver_var_value(pSolver, y0);
        int v1 = sat_solver_var_value(pSolver, y1);

        int lit_y0 = Abc_Var2Lit(y0, v0);
        int lit_y1 = Abc_Var2Lit(y1, v1);

        v0 = v0 ^ Abc_ObjFaninC0(nodeN);
        v1 = v1 ^ Abc_ObjFaninC1(nodeN);
        Vec_IntPush(vCareSet, (v0 << 1) | v1);
        // cout<< endl <<((v0 << 1) | v1)<<" \n";


        int clause[2] = {lit_y0, lit_y1};
        if(!sat_solver_addclause(pSolver, clause, clause + 2)) break;
        
        // cout<<sat_solver_addclause(pSolver, clause, clause + 2)<<endl;
        // sat_solver_addclause(pSolver, clause, clause + 1);
        // sat_solver_addclause(pSolver, clause +1, clause + 2);
    }

    // 打印所有滿足賦值
    // printf("Care set for y0 and y1: ");
    // int i, entry;
    // Vec_IntForEachEntry(vCareSet, entry, i) {
    //     printf("(%d, %d) ", (entry >> 1) & 1, entry & 1);
    // }
    // printf("\n");

    // 清理
    return vCareSet;
}


void Lsv_NtkPrint_ODC(Abc_Ntk_t* pNtk, int n) {
  // Abc_Obj_t * nodeN = Abc_NtkObj(pNtk, n);
  // Abc_Ntk_t * pNtkNeg = Abc_NtkDup(pNtk);
  // Abc_Obj_t * pNodeA = Abc_NtkObj(pNtkNeg, n);
  // Abc_Obj_t* pFanout;
  // int i;
  // Abc_ObjForEachFanout(pNodeA, pFanout, i) {
  //     Abc_ObjXorFaninC(pFanout, Abc_ObjFanoutFaninNum(pFanout, pNodeA));
  // }
  // Abc_Ntk_t * pMiter = Abc_NtkMiter(pNtk, pNtkNeg, 1, 0, 0, 0);

  Abc_Obj_t * nodeN = Abc_NtkObj(pNtk, n);
  if(nodeN == NULL){
    cout<< "ERROR: NULL node\n";
    return;
  }
  if(Abc_ObjFaninNum(nodeN) == 0 || Abc_ObjFaninNum(nodeN) == 1){
    printf("no odc\n");
    return;
  }
  if(Abc_ObjIsPo(nodeN)){
    nodeN = Abc_NtkObj(pNtk, n);
  }
  // if(Abc_ObjFanoutNum(nodeN) == 1 && Abc_ObjFanout0(nodeN) != NULL){
  //   if(Abc_ObjIsPo(Abc_ObjFanout0(nodeN))){
  //     printf("no odc\n");
  //     return;
  //   }
  // }
  // cout<< "n= "<<n <<endl;
  Vec_Int_t * vSDC = Lsv_NtkPrint_SDC(pNtk, n, false);
  // cout<< "n= "<<n_value <<endl;
  // 創建給定AIG的副本並否定節點n
    // Abc_Ntk_t * pNtkNeg = CreateNegatedAig(pNtk, n);
    Abc_Ntk_t * pNtkNeg = Abc_NtkDup(pNtk);
    Abc_Ntk_t * pBig = Abc_NtkDup(pNtk);
    Abc_Obj_t * pNodeA = Abc_NtkObj(pNtkNeg, n);

    Abc_Obj_t* pFanout;
    int i;
    Abc_ObjForEachFanout(pNodeA, pFanout, i) {
        // Invert the connection to the fanout
        Abc_ObjXorFaninC(pFanout, Abc_ObjFanoutFaninNum(pFanout, pNodeA));
    }

    // Abc_ObjReplace(pNodeA, negated_node);
    // Abc_ObjXorFaninC(pNodeA, 0);
    // Abc_ObjXorFaninC(pNodeA, 1);

    // 創建C和C'的miter
    // Abc_Ntk_t * pMiter = CreateMiter(pNtk, pNtkNeg);

    ///////////////////
    Abc_Ntk_t * pMiter = Abc_NtkMiter(pNtk, pNtkNeg, 1, 0, 0, 0);

    // Vec_Ptr_t * vNodes0 = Vec_PtrAlloc(3);
    // Vec_PtrPush(vNodes0, Abc_NtkPo(pNtk, 0));
    // Vec_PtrPush(vNodes0, Abc_ObjFanin0(Abc_NtkObj(pNtk, n)));
    // Vec_PtrPush(vNodes0, Abc_ObjFanin1(Abc_NtkObj(pNtk, n)));
    // Abc_Ntk_t * pCone0 = Abc_NtkCreateConeArray(pNtk, vNodes0, 1);

    // Vec_Ptr_t * vNodes1 = Vec_PtrAlloc(3);
    // Vec_PtrPush(vNodes1, Abc_NtkPo(pNtkNeg, 0));
    // Vec_PtrPush(vNodes1, Abc_ObjFanin0(Abc_NtkObj(pNtkNeg, n)));
    // Vec_PtrPush(vNodes1, Abc_ObjFanin1(Abc_NtkObj(pNtkNeg, n)));
    // Abc_Ntk_t * pCone1 = Abc_NtkCreateConeArray(pNtkNeg, vNodes1, 1);

    // Abc_Ntk_t * pMiter1 = Abc_NtkMiter(pCone0, pCone1, 1, 0, 0, 0);
    // /////////////////
    // // Vec_Ptr_t * vNodes = Vec_PtrAlloc(2);
    // // Vec_PtrPush(vNodes, NpMiter[0]);
    // // Vec_PtrPush(vNodes, NpMiter[1]);
    // // Abc_Ntk_t * pCone = Abc_NtkCreateConeArray(pMiter, vNodes, 1);
    ////////////////
    // pNtk = pMiter;
    // Abc_NtkShow(pNtk, 0, 0, 0, 0, 1);
    // Abc_NtkShow(pMiter, 0, 0, 0, 0, 1);
    Abc_NtkAppend(pBig, pMiter, 1);
    // Abc_NtkShow(pBig, 0, 0, 0, 0, 1);
    pMiter = pBig;

    
    // return;
    ////////////////

    // 將miter轉換為CNF並寫入SAT求解器
    // cout<<"finish construct\n";

    // vector<string> poNames;
    // Abc_Obj_t* pObj;
    // Abc_NtkForEachPo(pNtk, pObj, i) {
    //     poNames.push_back(Abc_ObjName(Abc_ObjFanin0(pObj)));
    // }
    // Abc_Obj_t* pPoInMiter = NULL;
    // for (const auto& poName : poNames) {
    //     Abc_NtkForEachPo(pMiter, pObj, i) {
    //       if (poName == Abc_ObjName(Abc_ObjFanin0(pObj))) {
    //           pPoInMiter = pObj;
    //       }
    //     }
    //     if (pPoInMiter) {
    //         printf("Found PO %s in miter network.\n", poName.c_str());
    //     } else {
    //         printf("PO %s not found in miter network.\n", poName.c_str());
    //     }
    // }

    // int y0, y1;
    int pVarY0, pVarY1;
    Abc_Obj_t * NpMiter[2] = {NULL, NULL};
    if(Abc_ObjFanin0(nodeN) == NULL || Abc_ObjFanin1(nodeN) == NULL){
      cout<<"ERROR_fanin!!!!!!\n";
      return;
    }
    // else cout<<"*** "<<Abc_ObjFanin0(nodeN)->Id<<" // "<<Abc_ObjFanin1(nodeN)->Id<<endl;
    NpMiter[0] = Abc_NtkObj(pMiter, Abc_ObjId(Abc_ObjFanin0(nodeN)));
    NpMiter[1] = Abc_NtkObj(pMiter, Abc_ObjId(Abc_ObjFanin1(nodeN)));
    if(NpMiter[0] == NULL || NpMiter[1] == NULL){
      cout<<"ERROR_fanin_NpMiter\n";
      return;
    }
    // else cout<<"pass_fanin_NpMiter\n";

    // ConvertMiterToCnfAndSolve(pMiter, &y0, &y1, pNodeN); 

    // Abc_Obj_t * pNodeNMiter = Abc_NtkObj(pMiter, Abc_ObjId(nodeN));
    Vec_Ptr_t * vNodes = Vec_PtrAlloc(3);
    // Vec_PtrPush(vNodes, Abc_NtkPo(pMiter, 0));
    Vec_PtrPush(vNodes, NpMiter[0]);
    Vec_PtrPush(vNodes, NpMiter[1]);
    Abc_Ntk_t * pConeIn = Abc_NtkCreateConeArray(pMiter, vNodes, 1);
    Abc_Obj_t * Conefanin[3] = {NULL, NULL, NULL};
    Conefanin[0] = Abc_ObjFanin0(Abc_NtkPo(pConeIn, 0));
    Conefanin[1] = Abc_ObjFanin0(Abc_NtkPo(pConeIn, 1));
    Vec_PtrPush(vNodes, Abc_NtkPo(pMiter, 1));
    // cout<<"=== New_pMiter_out: "<<Abc_NtkPo(pMiter, 0)->Id<<", "<<Abc_NtkPo(pMiter, 1)->Id<<endl;

    // Vec_PtrPush(vNodes, pNodeNMiter);
    // Vec_PtrPush(vNodes, pOutputNode);
    // Vec_PtrPush(vNodes, Abc_NtkObj(pMiter, Abc_ObjId(Abc_ObjFanin0(nodeN))));
    // Vec_PtrPush(vNodes, Abc_NtkObj(pMiter, Abc_ObjId(Abc_ObjFanin1(nodeN))));
    // Vec_PtrPush(vNodes, Abc_ObjFanin(nodeN, 0));
    // cout<<"Hi_4\n";
    // Vec_PtrPush(vNodes, Abc_ObjFanin(nodeN, 1));
    // cout<<"Hi_5\n";
    Abc_Ntk_t * pCone = Abc_NtkCreateConeArray(pMiter, vNodes, 1);
    Conefanin[2] = Abc_NtkPo(pCone, 2);
    // cout<<"==================== "<<Conefanin[0]->Id<<", "<<Conefanin[1]->Id<<", "<<Conefanin[2]->Id<<endl;

    // Abc_Obj_t * NpCone[3] = {NULL, NULL, NULL};
    // if(Abc_ObjId(NpMiter[0]) == -1 || Abc_ObjId(NpMiter[0]) == -1){
    //   cout<<"ERROR_fanin_0_NpCone!!!!!!\n";
    //   return;
    // }else cout<<"*** "<<NpMiter[0]->Id<<" // "<<NpMiter[1]->Id<<endl;
    // NpCone[0] = Abc_NtkObj(pCone, Abc_ObjId(NpMiter[0]));
    // NpCone[1] = Abc_NtkObj(pCone, Abc_ObjId(NpMiter[1]));
    // NpCone[2] = Abc_NtkObj(pCone, Abc_ObjId(Abc_NtkPo(pMiter, 0)));
    // if(NpCone[0] == NULL ||  NpCone[1] == NULL  ||  NpCone[2] == NULL){
    //   cout<<"ERROR_fanin_NpCone\n";
    //   return;
    // }else cout<<"pass_fanin_NpCone\n";

    // Abc_NtkShow(pCone, 0, 0, 0, 0, 1);
    // Abc_NtkShow(pMiter, 0, 0, 0, 0, 1);
    // Aig_Man_t * pAig1 = Abc_NtkToDar(pCone, 0, 0);
    // Aig_ManShow(pAig1, 0, NULL);
    // cout<<"*** PO=> "<<Abc_NtkPoNum(pMiter)<<endl;
    // return;

    // cout<<"Hi_6\n";
    Vec_PtrFree(vNodes);
    
    // 將miter轉換為AIG
    Aig_Man_t * pAig = Abc_NtkToDar(pCone, 0, 0);
    // cout<<"convert_1\n";

    Aig_Obj_t * NpAig[3] = {NULL, NULL, NULL};
    // if(Abc_ObjId(NpCone[0]) == -1 || Abc_ObjId(NpCone[1]) == -1){
    //   cout<<"ERROR_fanin_0_NpAig!!!!!!\n";
    //   return;
    // }else cout<<"*** "<<NpCone[0]->Id<<" // "<<NpCone[1]->Id<<endl;
    // NpAig[0] = Aig_ManObj(pAig, Abc_ObjId(NpCone[0]));
    // NpAig[1] = Aig_ManObj(pAig, Abc_ObjId(NpCone[1]));
    // NpAig[2] = Aig_ManObj(pAig, Abc_ObjId(NpCone[2]));
    if(Conefanin[0] == NULL ||Conefanin[1] == NULL ||Conefanin[2] == NULL){
      cout<<"ERROR: Conefanin NULL!!!\n";
    }
    // else cout<<"==== Conefanin = "<<Abc_ObjId(Conefanin[0])<<", "<<Abc_ObjId(Conefanin[1])<<", "<<Abc_ObjId(Conefanin[2])<<"\n";
    NpAig[0] = Aig_ManObj(pAig, Abc_ObjId(Conefanin[0]));
    NpAig[1] = Aig_ManObj(pAig, Abc_ObjId(Conefanin[1]));
    NpAig[2] = Aig_ManObj(pAig, Abc_ObjId(Conefanin[2]));
    if(NpAig[0] == NULL ||  NpAig[1] == NULL ||  NpAig[2] == NULL){
      cout<<"ERROR_fanin_NpAig\n";
      return;
    }
    // else cout<<"pass_fanin_NpAig\n";

    // Aig_ManShow(pAig, 0, NULL);

    // 導出CNF
    // Cnf_Dat_t * pCnf = Cnf_Derive(pAig, Aig_ManCoNum(pAig));
    Cnf_Dat_t * pCnf = Cnf_Derive(pAig, Aig_ManCoNum(pAig));
    // cout<<"***    Aig_ManCoNum of pCNF = "<<Aig_ManCoNum(pAig)<<endl;


    int NpCnf[3] = {-1, -1, -1};
    if(Aig_ObjId(NpAig[0]) == -1 || Aig_ObjId(NpAig[1]) == -1){
      cout<<"ERROR_fanin_0_NpCnf!!!!!!\n";
      return;
    }
    // else cout<<"*** "<<NpAig[0]->Id<<" // "<<NpAig[1]->Id<<" // "<<NpAig[2]->Id<<endl;
    NpCnf[0] = pCnf->pVarNums[ Aig_ObjId(NpAig[0]) ];
    NpCnf[1] = pCnf->pVarNums[ Aig_ObjId(NpAig[1]) ];
    NpCnf[2] = pCnf->pVarNums[ Aig_ObjId(NpAig[2]) ];
    if(NpCnf[0] == -1 ||  NpCnf[1] == -1 ||  NpCnf[2] == -1){
      cout<<"ERROR_fanin_NpCnf\n";
      return;
    }
    // else cout<<"pass_fanin_NpCnf\n";

    // cout<<"*** CNF === "<<NpCnf[0]<<" // "<<NpCnf[1]<<" // "<<NpCnf[2]<<endl;

    pVarY0 = NpCnf[0];
    pVarY1 = NpCnf[1];


    // cout<<"convert_2\n";
    // 初始化SAT求解器
    sat_solver * pSolver = sat_solver_new();
    // cout<<"convert_3\n";
    // 將CNF寫入求解器
    Cnf_DataWriteIntoSolverInt(pSolver, pCnf, 1, 0);
    // cout<<"convert_4\n";

    // 獲取y0和y1的變量索引
    // Abc_Obj_t * pFanin0 = Abc_ObjFanin(nodeN, 0);
    // Abc_Obj_t * pFanin1 = Abc_ObjFanin(nodeN, 1);
    // cout<<"* "<<pFanin0<<", "<<pFanin1<<endl;
    // pVarY0 = pCnf->pVarNums[Aig_ObjId(Aig_ManObj(pAig, Abc_ObjId(pFanin0)))];
    // pVarY1 = pCnf->pVarNums[Aig_ObjId(Aig_ManObj(pAig, Abc_ObjId(pFanin1)))];
    // cout<<"* "<<pVarY0<<", "<<pVarY1<<endl;
    // if (Abc_ObjFaninC0(nodeN)) {
    //     *pVarY0 = Abc_LitNot(*pVarY0);
    // }
    // if (Abc_ObjFaninC1(nodeN)) {
    //     *pVarY1 = Abc_LitNot(*pVarY1);
    // }

    // *pVarY0 = pCnf->pVarNums[Aig_ObjId(Aig_ManCi(pAig, 0))];
    // *pVarY1 = pCnf->pVarNums[Aig_ObjId(Aig_ManCi(pAig, 1))];
    // *pVarY0 = pCnf->pVarNums[Aig_ObjId(Abc_NtkObj(pMiter, 0))];
    // *pVarY1 = pCnf->pVarNums[Aig_ObjId(Aig_ManCo(pAig, 1))];
    // cout<<"convert_5\n";
    // cout<<"* "<<pVarY0<<", "<<pVarY1<<endl;
    if(pVarY0==-1 || pVarY1==-1){
      cout<<"NULL ERROR return\n";
      return;
    }

    // 假設miter輸出為1
    int lit_miter = Abc_Var2Lit(NpCnf[2], 0);
    sat_solver_addclause(pSolver, &lit_miter, &lit_miter + 1);
    // cout<<"solve_0\n";

    // // 求解CNF公式
    // int status = sat_solver_solve(pSolver, NULL, NULL, 0, 0, 0, 0);
    // if (status == l_True) {
    //     printf("The miter is satisfiable.\n");
    // } else if (status == l_False) {
    //     printf("The miter is unsatisfiable.\n");
    // } else {
    //     printf("The miter is undecided.\n");
    // }
    // cout<<"solve_1\n";
    // 求解ALLSAT並獲取y0和y1上的所有滿足賦值

    Vec_Int_t * vCareSet = SolveAllSat(pSolver, pVarY0, pVarY1, nodeN);
    // Vec_Int_t * vSDC = Lsv_NtkPrint_SDC(pNtk, n);
    // Vec_Int_t * vSDC 
    Vec_Int_t * vODC = Vec_IntAlloc(4); // 最多有 4 種模式

    // 四種模式
    int patterns[4] = {0, 1, 2, 3};

    // 找出不在 vCareSet 中的模式
    for (int i = 0; i < 4; i++) {
        if (Vec_IntFind(vCareSet, patterns[i]) == -1) {
            Vec_IntPush(vODC, patterns[i]);
        }
    }

    // 移除在 vSDC 中計算的 SDCs
    for (int i = 0; i < Vec_IntSize(vSDC); i++) {
        int sdc = Vec_IntEntry(vSDC, i);
        Vec_IntRemove(vODC, sdc);
    }


    // 打印所有SDC
    if (Vec_IntSize(vODC) == 0) {
        printf("no odc\n");
    } else {
        int Entry, i;
        Vec_IntForEachEntry(vODC, Entry, i) {
            printf("%d%d ", (Entry >> 1) & 1, Entry & 1);
        }
        printf("\n");
    }
    

    // cout<<"solve_all\n";

    // 清理
    Cnf_DataFree(pCnf);
    sat_solver_delete(pSolver);
    Aig_ManStop(pAig);






    // cout<<"finish solver\n";

    // 清理
    Abc_NtkDelete(pNtkNeg);
    Abc_NtkDelete(pMiter);


    // Vec_Ptr_t * vNodes = Vec_PtrAlloc(4);
    // Abc_Obj_t * nodeN = Abc_NtkObj(pNtk, n);
    // Vec_PtrPush(vNodes, 
    // Abc_NtkObj(pMiter, Abc_ObjId(Abc_ObjFanin0(nodeN))) );
    // /////////////////

    // Abc_Ntk_t * pCone = Abc_NtkCreateConeArray(pMiter, vNodes, 0);
    // Aig_Man_t * pAig = Abc_NtkToDar(pCone, 0, 0);
    // Cnf_Dat_t * pCnf = Cnf_Derive(pAig, 1);
    // /////////////////
    
    // Abc_Obj_t * newN = Abc_NtkObj(pMiter, Abc_ObjId(nodeN));
    // Abc_Obj_t * pFanin0 = Abc_ObjFanin(newN, 0);
    // int pVarY0 = pCnf->pVarNums[ Aig_ObjId(
    //     Aig_ManObj(pAig, Abc_ObjId(pFanin0)) )];
}

void Lsv_NtkPrintNodes(Abc_Ntk_t* pNtk) {
  Abc_Obj_t* pObj;
  int i;
  // Abc_NtkForEachNode(pNtk, pObj, i) {
  for ( i = 0; (i < Vec_PtrSize((pNtk)->vObjs)) && (((pObj) = Abc_NtkObj(pNtk, i)), 1); i++ )   \
        if ( (pObj) == NULL //  ||    !Abc_ObjIsNode(pObj)  
        ) {} else{
    
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

int Lsv_CommandPrintSDC(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c = std::stoi(argv[1]);
  Extra_UtilGetoptReset();
  // while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
  //   switch (c) {
  //     case 'h':
  //       goto usage;
  //     default:
  //       goto usage;
  //   }
  // }
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  Lsv_NtkPrint_SDC(pNtk, c, true);
  return 0;

// usage:
//   Abc_Print(-2, "usage: lsv_print_nodes [-h]\n");
//   Abc_Print(-2, "\t        prints the nodes in the network\n");
//   Abc_Print(-2, "\t-h    : print the command usage\n");
//   return 1;
}

int Lsv_CommandPrintODC(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c = std::stoi(argv[1]);
  Extra_UtilGetoptReset();
  // while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
  //   switch (c) {
  //     case 'h':
  //       goto usage;
  //     default:
  //       goto usage;
  //   }
  // }
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  Lsv_NtkPrint_ODC(pNtk, c);
  return 0;

// usage:
//   Abc_Print(-2, "usage: lsv_print_nodes [-h]\n");
//   Abc_Print(-2, "\t        prints the nodes in the network\n");
//   Abc_Print(-2, "\t-h    : print the command usage\n");
//   return 1;
}

