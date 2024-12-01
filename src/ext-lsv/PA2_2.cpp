#include "base/main/mainInt.h"
#include "base/main/main.h"
#include "base/abc/abc.h"
#include "aig/aig/aig.h"
#include "sat/bsat/satSolver.h"
#include "sat/cnf/cnf.h"

#include <vector>
#include <set>
#include <algorithm>
#include <bitset>

// 指令函數聲明
static int Lsv_CommandOdc(Abc_Frame_t* pAbc, int argc, char** argv);

// 初始化和銷毀函數
static void init(Abc_Frame_t* pAbc) {
    Cmd_CommandAdd(pAbc, "LSV", "lsv_odc", Lsv_CommandOdc, 0);
}

static void destroy(Abc_Frame_t* pAbc) {}

static Abc_FrameInitializer_t frame_initializer = {init, destroy};

static struct PackageRegistrationManager {
    PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

extern "C" {
    Aig_Man_t* Abc_NtkToDar(Abc_Ntk_t * pNtk, int fExors, int fRegisters);
}

int Lsv_CommandOdc(Abc_Frame_t* pAbc, int argc, char** argv) {
    if (argc != 2) {
        Abc_Print(-1, "Usage: lsv_odc <node_id>\n");
        return 1;
    }

    int nodeId = atoi(argv[1]);
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    if (!pNtk) {
        Abc_Print(1, "no odc\n");
        return 1;
    }

    // 將網路轉換為 AIG
    Aig_Man_t* pAig = Abc_NtkToDar(pNtk, 0, 0);
    if (!pAig) {
        Abc_Print(1, "no odc\n");
        return 1;
    }

    // 檢查節點 ID 是否有效
    if (nodeId < 0 || nodeId >= Aig_ManObjNumMax(pAig)) {
        Abc_Print(1, "Invalid node ID: %d\n", nodeId);
        Aig_ManStop(pAig);
        return 1;
    }

    // 獲取 AIG 中的節點
    Aig_Obj_t* pNode = Aig_ManObj(pAig, nodeId);
    if (!pNode || !Aig_ObjIsNode(pNode) || !Aig_ObjFanin0(pNode) || !Aig_ObjFanin1(pNode)) {
        Abc_Print(1, "no odc\n");
        Aig_ManStop(pAig);
        return 1;
    }

    // 創建 C'：與 C 相同但節點 n 取反
    Aig_Man_t* pMiter = Aig_ManStart(Aig_ManObjNumMax(pAig) + 10);
    pMiter->pName = Abc_UtilStrsav("Miter");

    // 複製 AIG 到 miter 電路中
    Aig_Obj_t* pObj;
    int i;
    Aig_ManForEachObj(pAig, pObj, i) {
        if (Aig_ObjIsConst1(pObj)) {
            Aig_ObjSetCopy(pObj, Aig_ManConst1(pMiter));
        } else if (Aig_ObjIsCi(pObj)) {
            Aig_Obj_t* pCi = Aig_ObjCreateCi(pMiter);
            Aig_ObjSetCopy(pObj, pCi);
        } else if (Aig_ObjIsNode(pObj)) {
            Aig_Obj_t* pFanin0Copy = Aig_ObjChild0Copy(pObj);
            Aig_Obj_t* pFanin1Copy = Aig_ObjChild1Copy(pObj);
            if (!pFanin0Copy || !pFanin1Copy) {
                continue;  // 確保 fanin 複製成
            }
            Aig_Obj_t* pAndNode = Aig_And(pMiter, pFanin0Copy, pFanin1Copy);
            Aig_ObjSetCopy(pObj, pAndNode);
        }
    }

    // 創建 C' 節點 n 的取反版本
    Aig_Obj_t* pNodeCopy = Aig_ObjCopy(pNode);
    if (!pNodeCopy) {
        Abc_Print(1, "Node copy is invalid.\n");
        Aig_ManStop(pMiter);
        Aig_ManStop(pAig);
        return 1;
    }
    Aig_Obj_t* pNodeNeg = Aig_Not(pNodeCopy);

    // 創建 miter 輸出：將原節點和取反節點進行 XOR 操作
    Aig_Obj_t* pMiterOutput = Aig_Exor(pMiter, Aig_ObjCopy(pNode), pNodeNeg);
    if (!pMiterOutput) {
        Abc_Print(1, "no odc\n");
        Aig_ManStop(pMiter);
        Aig_ManStop(pAig);
        return 1;
    }
    Aig_ObjCreateCo(pMiter, pMiterOutput);

    // 將 miter 電路轉換為 CNF 表達式
    Cnf_Dat_t* pCnf = Cnf_Derive(pMiter, 0);
    if (!pCnf) {
        Abc_Print(1, "no odc\n");
        Aig_ManStop(pMiter);
        Aig_ManStop(pAig);
        return 1;
    }

    // 初始化 SAT 求解器
    sat_solver* pSat = sat_solver_new();
    if (!pSat) {
        Abc_Print(1, "no odc\n");
        Cnf_DataFree(pCnf);
        Aig_ManStop(pMiter);
        Aig_ManStop(pAig);
        return 1;
    }
    Cnf_DataWriteIntoSolverInt(pSat, pCnf, 1, 0);

    // 設定最大求解次數以防止無限循環
    int maxSolveAttempts = 10000;  // 增加求解次數，增加找到解的機率
    int solveCount = 0;

    // 解決 SAT 問題以找到所有滿足的賦值
    Vec_Int_t* vSatisfyingAssignments = Vec_IntAlloc(100);
    int hasValidAssignment = 0;
    std::set<std::vector<int>> uniqueAssignments;

    while (sat_solver_solve(pSat, NULL, NULL, 0, 0, 0, 0) == l_True) {
        if (solveCount++ >= maxSolveAttempts) {
            Abc_Print(1, "Maximum solve attempts reached, stopping.\n");
            break;
        }

        // 提取滿足的賦值
        Vec_IntClear(vSatisfyingAssignments);
        std::vector<int> currentAssignment;

        for (int i = 0; i < pCnf->nVars; i++) {
            if (sat_solver_var_value(pSat, i)) {
                // 過濾掉 PI 節點，確保只處理內部節點
                Aig_Obj_t* pCurrentObj = Aig_ManObj(pAig, i);
                if (!pCurrentObj || Aig_ObjIsCi(pCurrentObj)) {
                    continue;
                }
                Vec_IntPush(vSatisfyingAssignments, i);
                currentAssignment.push_back(i);
            }
        }

        if (uniqueAssignments.find(currentAssignment) != uniqueAssignments.end()) {
            continue;  // 如果找到相同的賦值，跳過該解
        }
        uniqueAssignments.insert(currentAssignment);

        // 確保輸出所有可能的組合
        Abc_Print(1, "Minterm: ");
        for (int val : currentAssignment) {
            Abc_Print(1, "%d ", val);
        }
        Abc_Print(1, "\n");

        hasValidAssignment = 1;

        // 阻止當前的解繼續
        Vec_Int_t* vBlockingClause = Vec_IntAlloc(100);
        for (int i = 0; i < Vec_IntSize(vSatisfyingAssignments); i++) {
            int lit = Vec_IntEntry(vSatisfyingAssignments, i);
            Vec_IntPush(vBlockingClause, Abc_Var2Lit(lit, 1)); // 使用正確的阻止子句格式，阻止相同解
        }
        if (!sat_solver_addclause(pSat, Vec_IntArray(vBlockingClause), Vec_IntArray(vBlockingClause) + Vec_IntSize(vBlockingClause))) {
            Vec_IntFree(vBlockingClause);
            break;
        }
        Vec_IntFree(vBlockingClause);
    }

    if (!hasValidAssignment) {
        Abc_Print(1, "no odc\n");
    }

    // 清理
    Vec_IntFree(vSatisfyingAssignments);
    sat_solver_delete(pSat);
    Cnf_DataFree(pCnf);
    Aig_ManStop(pMiter);
    Aig_ManStop(pAig);

    return 0;
}

