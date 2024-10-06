#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <iostream>
#include <vector>
#include <set>
#include <algorithm>
#include <iterator>



using namespace std;

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);

static int Lsv_CommandPrintCut(Abc_Frame_t* pAbc, int argc, char** argv);



void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_CommandPrintCut, 0);
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



/////////////////////////////////////////////////////////////


int Lsv_CommandPrintCut(Abc_Frame_t* pAbc, int argc, char** argv) {
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    int  k = 3;  // 默認 k 值

    Extra_UtilGetoptReset();
	
	if(argv[1]!= NULL)
		k  = atoi(argv[1]);
	
	
	if (k< 3 || k > 6) {
           Abc_Print(-1, "無效的 k 值。必須在 3 到 6 之間。\n");
                 return 1;
    }
                
    

    if (!pNtk) {
        Abc_Print(-1, "空網絡。\n");
        return 1;
    }

    if (!Abc_NtkIsStrash(pNtk)) {
        Abc_Print(-1, "網絡不是 AIG 格式。請先運行 'strash'。\n");
        return 1;
    }

    int maxId = Abc_NtkObjNumMax(pNtk);
    vector<vector<vector<int>>> cuts(maxId+10);  //三d

    // 初始化 PI 的 cuts
    Abc_Obj_t* pObj;
    int i;
	
	//先將pi全都印出來
    Abc_NtkForEachPi(pNtk, pObj, i) {
        cuts[Abc_ObjId(pObj)] = {{Abc_ObjId(pObj)}};
        printf("%d: %d\n", Abc_ObjId(pObj), Abc_ObjId(pObj));
    }

    // 處理內部 AND 
    Abc_AigForEachAnd(pNtk, pObj, i) {
		cuts[Abc_ObjId(pObj)] = {{Abc_ObjId(pObj)}};
		printf("%d: %d\n", Abc_ObjId(pObj), Abc_ObjId(pObj));
		
        Abc_Obj_t* pFanin0 = Abc_ObjFanin0(pObj);
        Abc_Obj_t* pFanin1 = Abc_ObjFanin1(pObj);
        int fanin0Id = Abc_ObjId(pFanin0);
        int fanin1Id = Abc_ObjId(pFanin1);

        for (const auto& cut0 : cuts[fanin0Id]) {
            for (const auto& cut1 : cuts[fanin1Id]) {
                vector<int> combinedCut;
                set_union(cut0.begin(), cut0.end(), cut1.begin(), cut1.end(), back_inserter(combinedCut));
                if (combinedCut.size() <= k&& combinedCut!=cut0 && combinedCut!=cut0) {
                    cuts[Abc_ObjId(pObj)].push_back(combinedCut);
                    printf("%d: ", Abc_ObjId(pObj));
                    for (int node : combinedCut) {
                        printf("%d ", node);
                    }
                    printf("\n");
                }
            }
        }
    }

    // 處理 PO
    Abc_NtkForEachPo(pNtk, pObj, i) {
        Abc_Obj_t* pFanin = Abc_ObjFanin0(pObj);
		
		
        int faninId = Abc_ObjId(pFanin);
		cuts[Abc_ObjId(pObj)] = {{Abc_ObjId(pObj)}};
		printf("%d: %d\n", Abc_ObjId(pObj), Abc_ObjId(pObj));
		
        for (const auto& cut : cuts[faninId]) {
            printf("%d: ", Abc_ObjId(pObj));
            for (int node : cut) {
                printf("%d ", node);
            }
            printf("\n");
        }
    }

    return 0;
/*
usage:
    Abc_Print(-2, "用法: lsv_printcut [-k <num>] [-h]\n");
    Abc_Print(-2, "\t         列舉 k-可行 cuts\n");
    Abc_Print(-2, "\t-k num : cut 大小 (3 <= num <= 6) [默認 = 3]\n");
    Abc_Print(-2, "\t-h     : 打印命令用法\n");
    return 1;
*/
}


