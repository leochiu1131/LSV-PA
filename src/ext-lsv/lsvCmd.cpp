#include <iostream>
#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/cnf/cnf.h"
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

void CreateCnfForFaninNodes(Abc_Ntk_t * pNtk, int n, Cnf_Dat_t ** ppCnf, sat_solver ** ppSolver, int * pVarY0, int * pVarY1) {
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
    Cnf_DataWriteIntoSolver(*ppCnf, 1, 0);

    // 獲取y0和y1的變量索引
    *pVarY0 = (*ppCnf)->pVarNums[Aig_ObjId(Aig_ManCo(pAig, 0))];
    *pVarY1 = (*ppCnf)->pVarNums[Aig_ObjId(Aig_ManCo(pAig, 1))];

    // 清理
    Aig_ManStop(pAig);
    Abc_NtkDelete(pConeNtk);
}

int CheckPattern(sat_solver * pSolver, Cnf_Dat_t * pCnf, int y0, int y1, int v0, int v1) {
    // 重設求解器
    sat_solver_restart(pSolver);

    // 將CNF重新寫入求解器
    Cnf_DataWriteIntoSolver(pCnf, 1, 0);

    // 假設y0和y1的值分別為v0和v1
    int lit_y0 = Abc_Var2Lit(y0, v0 == 0 ? 1 : 0);
    int lit_y1 = Abc_Var2Lit(y1, v1 == 0 ? 1 : 0);

    // 添加假設條件
    sat_solver_addclause(pSolver, &lit_y0, &lit_y0 + 1);
    sat_solver_addclause(pSolver, &lit_y1, &lit_y1 + 1);

    // 求解CNF公式
    int status = sat_solver_solve(pSolver, NULL, NULL, 0, 0, 0, 0);
    return status;
}


void Lsv_NtkPrint_SDC(Abc_Ntk_t* pNtk, int n) {

    Cnf_Dat_t * pCnf;
    sat_solver * pSolver;
    int y0, y1;

    // 創建包含y0和y1的傳遞扇入錐並導出CNF
    CreateCnfForFaninNodes(pNtk, n, &pCnf, &pSolver, &y0, &y1);
    cout<<n <<": "<<y0<<", "<<y1<<endl;

    // // 獲取y0和y1的變量索引
    // int y0 = pCnf->pVarNums[Abc_ObjId(Abc_ObjFanin0(Abc_NtkObj(pNtk, n)))];
    // int y1 = pCnf->pVarNums[Abc_ObjId(Abc_ObjFanin1(Abc_NtkObj(pNtk, n)))];

    // 檢查所有模式
    int patterns[4][2] = {{0, 0}, {0, 1}, {1, 0}, {1, 1}};
    for (int i = 0; i < 4; i++) {
        int v0 = patterns[i][0];
        int v1 = patterns[i][1];
        int status = CheckPattern(pSolver, pCnf, y0, y1, v0, v1);
        if (status == l_False) {
            printf("Pattern (%d, %d) is an SDC.\n", v0, v1);
        } else {
            printf("Pattern (%d, %d) is not an SDC.\n", v0, v1);
        }
    }

    // 清理
    Cnf_DataFree(pCnf);
    sat_solver_delete(pSolver);




    // Abc_Obj_t * pNodeA = Abc_NtkObj(pNtk, n);
    // Abc_Obj_t * pFanin0 = Abc_ObjFanin0(pNodeA);
    // Abc_Obj_t * pFanin1 = Abc_ObjFanin1(pNodeA);

    // // 創建一個指針向量來存儲這些節點
    // // Vec_Ptr_t * vNodes = Vec_PtrAlloc(3);
    // Vec_Ptr_t * vNodes = Vec_PtrAlloc(2);
    // // Vec_PtrPush(vNodes, pNodeA);
    // Vec_PtrPush(vNodes, pFanin0);
    // Vec_PtrPush(vNodes, pFanin1);

    // // 創建包含這些節點的傳遞扇入錐
    // Abc_Ntk_t * pConeNtk = Abc_NtkCreateConeArray(pNtk, vNodes, 1);
    // Vec_PtrFree(vNodes);

    // // 將傳遞扇入錐轉換為AIG
    // Aig_Man_t * pAig = Abc_NtkToDar(pConeNtk, 0, 0);

    // // 導出CNF，這裡假設只有一個輸出
    // Cnf_Dat_t * pCnf = Cnf_Derive(pAig, 2);

    // // 初始化SAT求解器
    // sat_solver * pSolver = sat_solver_new();

    // // 將CNF寫入求解器
    // Cnf_DataWriteIntoSolver(pCnf, 1, 0);

    // // 清理
    // Cnf_DataFree(pCnf);
    // Aig_ManStop(pAig);
    // Abc_NtkDelete(pConeNtk);

    // // 使用求解器進行求解
    // // 例如，檢查SAT問題是否可滿足
    // int status = sat_solver_solve(pSolver, NULL, NULL, 0, 0, 0, 0);
    // if (status == l_True) {
    //     printf("SAT problem is satisfiable.\n");
    // } else if (status == l_False) {
    //     printf("SAT problem is unsatisfiable.\n");
    // } else {
    //     printf("SAT problem is undecided.\n");
    // }

    // // 釋放求解器
    // sat_solver_delete(pSolver);

}

void Lsv_NtkPrint_ODC(Abc_Ntk_t* pNtk, int n_value) {
  cout<< "n= "<<n_value <<endl;
  
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
  ////////////////////////////////////////////////////////////////////////////////////////
  // printf("\n\n");
  // Abc_NtkForEachPi(pNtk, pObj, i) {
  //   printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
  //   Abc_Obj_t* pFanin;
  //   int j;
  //   Abc_ObjForEachFanin(pObj, pFanin, j) {
  //     printf("  Fanin-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanin),
  //            Abc_ObjName(pFanin));
  //   }
  //   if (Abc_NtkHasSop(pNtk)) {
  //     printf("The SOP of this node:\n%s", (char*)pObj->pData);
  //   }
  // }
  // printf("\n\n");
  // Abc_NtkForEachPo(pNtk, pObj, i) {
  //   printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
  //   Abc_Obj_t* pFanin;
  //   int j;
  //   Abc_ObjForEachFanin(pObj, pFanin, j) {
  //     printf("  Fanin-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanin),
  //            Abc_ObjName(pFanin));
  //   }
  //   if (Abc_NtkHasSop(pNtk)) {
  //     printf("The SOP of this node:\n%s", (char*)pObj->pData);
  //   }
  // }
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
  Lsv_NtkPrint_SDC(pNtk, c);
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