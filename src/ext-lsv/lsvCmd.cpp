#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <vector>
#include <fstream>
#include <string>

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimBdd(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", Lsv_CommandSimBdd, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig", Lsv_CommandSimAig, 0);
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

int Lsv_CommandSimBdd(Abc_Frame_t* pAbc, int argc, char** argv) {
    // for (int i = 0; i < argc; ++i) {
    //     printf("[%d] %s\n", i, argv[i]);
    // }
    // printf("Running Simulation on BDD\n");

    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    Abc_Obj_t* pPo;
    int i;
    DdManager * dd = (DdManager *)pNtk->pManFunc;
    Abc_NtkForEachPo(pNtk, pPo, i) {
        Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo);
        assert( Abc_NtkIsBddLogic(pRoot->pNtk) );
        DdNode* ddnode = (DdNode *)pRoot->pData;
        // printf("%d\n", i);
        // char** vNamesIn = (char**) Abc_NodeGetFaninNames(pRoot)->pArray;
        // printf("%d\n", i);
        // for (int j = 0; vNamesIn[j] != NULL; j++) {
        //     printf("[%d] %s\n", j, vNamesIn[j]);
        // }
        Abc_Obj_t* pFanin;
        int j;
        Abc_ObjForEachFanin(pRoot, pFanin, j) {
            // printf("  Fanin-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanin),
            //     Abc_ObjName(pFanin));
            
            DdNode* nownode = Cudd_bddIthVar(dd, j);
            // printf("%d\n", j);
            // printf("input pattern: %d\n", (argv[1][Abc_ObjId(pFanin)-1] - '0'));
            if ((argv[1][Abc_ObjId(pFanin)-1] - '0') == 1) {
                ddnode = Cudd_Cofactor(dd, ddnode, nownode);
            }
            else {
                ddnode = Cudd_Cofactor(dd, ddnode, Cudd_Not(nownode));
            }
        }
        
        if (ddnode == dd->one) {
            printf("%s: %d\n", Abc_ObjName(pPo), 1);
        }
        else {
            printf("%s: %d\n", Abc_ObjName(pPo), 0);
        }
    }

    return 0;
}

int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv) {
    // for (int i = 0; i < argc; ++i) {
    //     printf("[%d] %s\n", i, argv[i]);
    // }
    // printf("Running Simulation on AIG\n");

    std::fstream f(argv[1]);
    std::vector<std::string> in;
    std::string tmp;
    while (std::getline(f, tmp)) {
        in.push_back(tmp);
    }
    f.close();

    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    Abc_Obj_t* pPi;
    int i;
    std::vector<std::vector<int>> simPattern;
    std::vector<int> simtmp;
    Abc_NtkForEachPi(pNtk, pPi, i) {
        for (int j=0; j<in.size(); j++) {
            // printf("%d", (in[j][i] - '0'));
            simtmp.push_back(in[j][i] - '0');
        }
        // printf("\n");
        simPattern.push_back(simtmp);
        simtmp.clear();
    }
    // for (i=0; i<simPattern.size(); i++) {
    //     for (int j=0; j<simPattern[i].size(); j++) {
    //         printf("%d", simPattern[i][j]);
    //     }
    //     printf("\n");
    // }

    std::vector<std::vector<int>> Parallel;
    int currentInt = 0;
    int bitCount = 0;
    for (i=0; i<simPattern.size(); i++) {
        for (int j=0; j<simPattern[i].size(); j++) {
            int bit = simPattern[i][j];
            currentInt |= bit;
            ++bitCount;

            if (j == simPattern[i].size()-1) break;

            if (bitCount == 32) {
                simtmp.push_back(currentInt);
                // printf("%d\n", currentInt);
                currentInt = 0;
                bitCount = 0;
            } 
            else {
                currentInt <<= 1;
            }
        }  
        if (bitCount > 0) {
            simtmp.push_back(currentInt);
            // printf("%d\n", currentInt);
            currentInt = 0;
            bitCount = 0;
        }
        Parallel.push_back(simtmp);
        simtmp.clear();
    }

    // int obj_num = Abc_NtkObjNum(pNtk);
    // printf("# of obj = %d\n", obj_num);
    Abc_Obj_t* pObj;
    std::vector<std::vector<int>> result;
    Abc_NtkForEachObj(pNtk, pObj, i) {
        if (Abc_ObjIsPi(pObj)) {
            // printf("PI\n");
            // printf("  Obj-%d: Id = %d, name = %s\n", i, Abc_ObjId(pObj),
            //     Abc_ObjName(pObj));
            result.push_back(Parallel[i-1]);
            continue;
        }
        else if (pObj->Type == ABC_OBJ_CONST1) {
            // printf("CONST1\n");
            result.push_back({1});
            continue;
        }
        else if (Abc_ObjIsPo(pObj)) {
            // printf("PO\n");
            // printf("  Obj-%d: Id = %d, name = %s\n", i, Abc_ObjId(pObj),
            //     Abc_ObjName(pObj));
            result.push_back({0});
            continue;
        }
        else {
            // printf("Node\n");
            // printf("  Obj-%d: Id = %d, name = %s\n", i, Abc_ObjId(pObj),
            //     Abc_ObjName(pObj));
            std::vector<int> temp;
            int tmp_i;
            for (int j=0; j<Parallel[0].size(); j++) {
                if (Abc_ObjFaninC0(pObj)) {
                    if (Abc_ObjFaninC1(pObj)) {
                        tmp_i = (~result[Abc_ObjFanin0(pObj)->Id][j]) & (~result[Abc_ObjFanin1(pObj)->Id][j]);
                    }
                    else {
                        tmp_i = (~result[Abc_ObjFanin0(pObj)->Id][j]) & (result[Abc_ObjFanin1(pObj)->Id][j]);
                    }
                }
                else {
                    if (Abc_ObjFaninC1(pObj)) {
                        tmp_i = (result[Abc_ObjFanin0(pObj)->Id][j]) & (~result[Abc_ObjFanin1(pObj)->Id][j]);
                    }
                    else {
                        tmp_i = (result[Abc_ObjFanin0(pObj)->Id][j]) & (result[Abc_ObjFanin1(pObj)->Id][j]);
                    }
                }
                temp.push_back(tmp_i);
            }
            result.push_back(temp);
        }
    }
    Abc_Obj_t* pPo;
    Abc_NtkForEachPo(pNtk, pPo, i) {
        std::vector<int> temp;
        int tmp_i;
        for (int j=0; j<Parallel[0].size(); j++) {
            if (Abc_ObjFaninC0(pPo)) {
                tmp_i = ~result[Abc_ObjFanin0(pPo)->Id][j];
            }
            else {
                tmp_i = result[Abc_ObjFanin0(pPo)->Id][j];
            }
            temp.push_back(tmp_i);
        }
        result[Abc_ObjId(pPo)] = temp;

        printf("%s: ", Abc_ObjName(pPo));
        int cnt = 0;
        for (int j=0; j<temp.size(); j++) {
            int k = 31;
            if (simPattern[0].size() - cnt <= k)
                k = simPattern[0].size() - cnt -1;
            for (; k >= 0; k--) {
                if (temp[j] & (1 << k)) {
                    printf("1");
                } else {
                    printf("0");
                }
                cnt++;
                if (cnt == simPattern[0].size()) break;
            }
        }
        printf("\n");
    }

    return 0;
}
