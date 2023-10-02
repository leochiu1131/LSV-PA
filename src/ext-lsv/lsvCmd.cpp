#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <algorithm>
#include <bitset>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

static int Lsv_CommandSimBdd(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", Lsv_CommandSimBdd, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig", Lsv_CommandSimAig, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

void Lsv_NtkSimAig(Abc_Ntk_t* pNtk,const std::string input_pattern_file) {
    std::ifstream fin(input_pattern_file, std::ios_base::in);
    std::vector<std::string> input_patterns;
    std::string input_pattern("");
    while (fin >> input_pattern)
        input_patterns.push_back(input_pattern);
    fin.close();

    const int CiNum = Abc_NtkCiNum(pNtk);
    const int NodeNum = Abc_NtkCiNum(pNtk) + Abc_NtkNodeNum(pNtk) + Abc_NtkCoNum(pNtk);
    const int parallel_inputs_n = (input_patterns.size() % 32)
        ? (input_patterns.size() >> 5) + 1
        : (input_patterns.size() >> 5);
    std::vector<std::vector<unsigned int>> parallel_inputs(NodeNum, std::vector<unsigned int>(parallel_inputs_n));
    for (int i = 0; i < CiNum; ++i) 
        for (int j = 0; j < input_patterns.size(); ++j)
            if (input_patterns[j][i] == '1') 
                parallel_inputs[i][j >> 5] |= (1 << (31 - j % 32));

    Vec_Ptr_t* vVec = Abc_AigDfsMap(pNtk);
    Abc_Obj_t* pEntry;
    int k;
    Vec_PtrForEachEntry(Abc_Obj_t*, vVec, pEntry, k) {
        for (int t = 0; t < parallel_inputs_n; ++t) {
            unsigned int fin0 = pEntry->fCompl0
                ? ~parallel_inputs[Abc_ObjId(Abc_ObjFanin0(pEntry)) - 1][t]
                : parallel_inputs[Abc_ObjId(Abc_ObjFanin0(pEntry)) - 1][t];
            unsigned int fin1 = pEntry->fCompl1
                ? ~parallel_inputs[Abc_ObjId(Abc_ObjFanin1(pEntry)) - 1][t]
                : parallel_inputs[Abc_ObjId(Abc_ObjFanin1(pEntry)) - 1][t];
            parallel_inputs[Abc_ObjId(pEntry) - 1][t] = fin0 & fin1;
        }
    }

    Abc_Obj_t* pObj;
    Abc_NtkForEachPo(pNtk, pObj, k) {
        std::cout << Abc_ObjName(pObj) << ": ";
        for (int t = 0; t < parallel_inputs_n; ++t) {
            parallel_inputs[Abc_ObjId(pObj) - 1][t] = pObj->fCompl0 
                ? ~parallel_inputs[Abc_ObjId(Abc_ObjFanin0(pObj)) - 1][t]
                : parallel_inputs[Abc_ObjId(Abc_ObjFanin0(pObj)) - 1][t];
        }
        if (input_patterns.size() % 32 == 0)
            for (int t = 0; t < parallel_inputs_n; ++t)
                std::cout << std::bitset<32>(parallel_inputs[Abc_ObjId(pObj) - 1][t]);
        else {
            for (int t = 0; t < parallel_inputs_n - 1; ++t)
                std::cout << std::bitset<32>(parallel_inputs[Abc_ObjId(pObj) - 1][t]);
            std::cout << std::bitset<32>(parallel_inputs[Abc_ObjId(pObj) - 1][parallel_inputs_n - 1])
                .to_string().substr(0, input_patterns.size() % 32);
        }
        std::cout << '\n';
    }
}

int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv) {
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
  Lsv_NtkSimAig(pNtk, argv[1]);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sim_aig [-h]\n");
  Abc_Print(-2, "\t        prints the simulations for the given BDD and the input pattern\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

void Lsv_NtkSimBdd(Abc_Ntk_t* pNtk,const std::string input_pattern) {
    assert(Abc_NtkIsBddLogic(pNtk));
    int CiNum = Abc_NtkCiNum(pNtk);
    char** names = Abc_NtkCollectCioNames(pNtk, 0);
    std::vector<std::string> CiNames(names, names + CiNum);

    Abc_Obj_t* pPo;
    int i;
    Abc_NtkForEachPo(pNtk, pPo, i) {
        Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo); 
        DdManager* dd = static_cast<DdManager*>(pRoot->pNtk->pManFunc);
        DdNode* ddNode = static_cast<DdNode*>(pRoot->pData);
    
        Abc_Obj_t* pFanin;
        int j;
        Abc_ObjForEachFanin(pRoot, pFanin, j) {
            auto itFi = std::find(CiNames.begin(), CiNames.end(), Abc_ObjName(pFanin));
            if (itFi != CiNames.end()) {
                if (input_pattern[std::distance(CiNames.begin(), itFi)] == '1')
                    ddNode = Cudd_Cofactor(dd, ddNode, Cudd_bddIthVar(dd, j));
                else
                    ddNode = Cudd_Cofactor(dd, ddNode, Cudd_Not(Cudd_bddIthVar(dd, j)));
            }
        }

        std::cout << Abc_ObjName(pPo) << ": " << "10"[ddNode != Cudd_ReadOne(dd)] << '\n';
    }
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
  Abc_Print(-2, "\t        prints the simulations for the given BDD and the input pattern\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}
