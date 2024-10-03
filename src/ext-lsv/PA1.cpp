#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"

// 合併兩個 Vec_Int_t 並進行排序，且不重複元素
Vec_Int_t* Lsv_TwoMerge(Vec_Int_t* v1, Vec_Int_t* v2) {
    Vec_Int_t* vMerged = Vec_IntAlloc(Vec_IntSize(v1) + Vec_IntSize(v2));
    int i1 = 0, i2 = 0, val1, val2;
    // 將 v1 和 v2 合併並排序
    while (i1 < Vec_IntSize(v1) && i2 < Vec_IntSize(v2)) {
        val1 = Vec_IntEntry(v1, i1);
        val2 = Vec_IntEntry(v2, i2);
        if (val1 < val2) {
            Vec_IntPush(vMerged, val1);
            i1++;
        } else if (val2 < val1) {
            Vec_IntPush(vMerged, val2);
            i2++;
        } else {
            Vec_IntPush(vMerged, val1); // 去除重複元素
            i1++;
            i2++;
        }
    }
    // 如果其中一個集合還有剩餘，將其加入 vMerged
    while (i1 < Vec_IntSize(v1)) {
        Vec_IntPush(vMerged, Vec_IntEntry(v1, i1));
        i1++;
    }
    while (i2 < Vec_IntSize(v2)) {
        Vec_IntPush(vMerged, Vec_IntEntry(v2, i2));
        i2++;
    }
    return vMerged;
}

// 列印 Vec_Int_t
void PrintCut(Vec_Int_t* v) {
    for (int i = 0; i < Vec_IntSize(v); i++) {
        printf("%d", Vec_IntEntry(v, i));
        if (i < Vec_IntSize(v) - 1) {
            printf(" ");
        }
    }
}

// 列印 Vec_Ptr_t (包含多個 cuts)
void PrintCutList(Vec_Ptr_t* vList) {
    Vec_Int_t* vCut;
    int i;
    Vec_PtrForEachEntry(Vec_Int_t*, vList, vCut, i) {
        PrintCut(vCut);
    }
}

void ProcessNode(Abc_Obj_t* pObj, Vec_Vec_t* T, Vec_Ptr_t* Q, int k) {
    Abc_Obj_t* pFanin0 = Abc_ObjFanin0(pObj);
    Abc_Obj_t* pFanin1 = Abc_ObjFanin1(pObj);
    // 如果 T 中找不到 fanin0 或 fanin1，將當前節點放入 Q 等待處理
    if (Vec_VecEntry(T, Abc_ObjId(pFanin0)) == NULL || Vec_VecEntry(T, Abc_ObjId(pFanin1)) == NULL) {
        Vec_PtrPush(Q, pObj);
    }
    // 初始化當前節點的 cuts
    Vec_Int_t* cut = Vec_IntAlloc(1);
    Vec_IntPush(cut, Abc_ObjId(pObj));
    Vec_VecPush(T, Abc_ObjId(pObj), cut);
    // 遍歷 fanin0 和 fanin1 的 cuts 並進行 union
    Vec_Ptr_t* cutsFanin0 = Vec_VecEntry(T, Abc_ObjId(pFanin0));
    Vec_Ptr_t* cutsFanin1 = Vec_VecEntry(T, Abc_ObjId(pFanin1));
    Vec_Int_t* cut0, * cut1;
    int j, l;
    Vec_PtrForEachEntry(Vec_Int_t*, cutsFanin0, cut0, j) {
        Vec_PtrForEachEntry(Vec_Int_t*, cutsFanin1, cut1, l) {
            Vec_Int_t* mergedCut = Vec_IntTwoMerge(cut0, cut1);
            // 如果 union 後的 cut 大小 <= k，將其加入當前節點的 cuts
            if (Vec_IntSize(mergedCut) <= k) {
                Vec_VecPush(T, Abc_ObjId(pObj), mergedCut);
            } else {
                Vec_IntFree(mergedCut);
            }
        }
    }
    // 列印當前節點的 ID 和對應的 cuts
    Vec_PtrForEachEntry(Vec_Int_t*, Vec_VecEntry(T, Abc_ObjId(pObj)), cut, j) {
        printf("%d: ", Abc_ObjId(pObj));
        PrintCut(cut);
        printf("\n");
    }
}

int Lsv_PrintCut(Abc_Frame_t* pAbc, int argc, char** argv) {
    // Basic variables
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc); // Get the current AIG network
    Abc_Obj_t* pObj;
    int i;
    // 偵錯
    if (argc != 2) {
        Abc_Print(-1, "Usage: lsv_printcut <k>\n");
        return 1;
    }
    int k = atoi(argv[1]);
    if (k < 3 || k > 6) {
        Abc_Print(-1, "k should be between 3 and 6.\n");
        return 1;
    }
    if (!pNtk || !Abc_NtkIsStrash(pNtk)) {
        Abc_Print(-1, "The network is not a strashed AIG or is empty.\n");
        return 1;
    }
    // 初始化隊列 Q 和查找表 T
    Vec_Ptr_t* Q = Vec_PtrAlloc(100);
    Vec_Vec_t* T = Vec_VecAlloc(100);
    // 處理所有的 primary inputs
    Abc_NtkForEachPi(pNtk, pObj, i) {
        Vec_Int_t* singletonSet = Vec_IntAlloc(1);
        Vec_IntPush(singletonSet, Abc_ObjId(pObj));
        Vec_VecPush(T, Abc_ObjId(pObj), singletonSet);
        // 列印 Primary Input 的 ID 和對應的 cuts
        printf("%d: ", Abc_ObjId(pObj));
        PrintCut(singletonSet);
        printf("\n");
    }
    // 遍歷所有的內部節點
    Abc_NtkForEachNode(pNtk, pObj, i) {
        ProcessNode(pObj, T, Q, k);
    }
    // 處理隊列中的節點
    while (Vec_PtrSize(Q) > 0) {
        pObj = (Abc_Obj_t *)Vec_PtrPop(Q);  // 進行類型轉換
        ProcessNode(pObj, T, Q, k);
    }
    Abc_NtkForEachPi(pNtk, pObj, i) {
        ProcessNode(pObj, T, Q, k);
    }
    // 釋放資源
    Vec_VecFree(T);
    Vec_PtrFree(Q);
    return 0;
}
// Function to register the hello command
void registercutCommand(Abc_Frame_t* pAbc) {
    Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_PrintCut, 0);
}
