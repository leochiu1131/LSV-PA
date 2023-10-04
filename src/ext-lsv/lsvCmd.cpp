#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <iostream>
#include <vector>
#include <string>
#include <fstream> 

using namespace std;

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_simulation_bdd(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_simulation_aig(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd"    , Lsv_simulation_bdd, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig"    , Lsv_simulation_aig, 0);
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

int Lsv_simulation_bdd(Abc_Frame_t* pAbc, int argc, char** argv) {
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    int i, j, output;
    Abc_Obj_t * pPo, * pPi;

    assert(strlen(argv[1]) == Abc_NtkPiNum(pNtk));
    int cube[strlen(argv[1])], input[strlen(argv[1])];
    for(int i = 0; i < strlen(argv[1]); i++){
        input[i] = argv[1][i] - '0';
        cube[i] = -1;
    }

    Abc_NtkForEachPo(pNtk, pPo, i){
        Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo); 
        assert( Abc_NtkIsBddLogic(pRoot->pNtk) );
        DdManager * dd = (DdManager *)pRoot->pNtk->pManFunc;  
        DdNode* f = (DdNode *)pRoot->pData;
        Abc_ObjForEachFanin( pRoot, pPi, j ){
            cube[j] = input[pPi->Id - 1];
        }
        
        DdNode* g = Cudd_CubeArrayToBdd(dd, cube);
        f = Cudd_Cofactor(dd, f, g);
        if(f == dd->one){
            output = 1;
        }
        else{
            output = 0;
        }

        cout<<Abc_ObjName(pPo)<<": "<<output<<endl;
    }
    return 0;
}

int Lsv_simulation_aig(Abc_Frame_t* pAbc, int argc, char** argv) {
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    ifstream in(argv[1]);
    int i, j, k;
    Abc_Obj_t * pObj, * pPi, * pPo;
    string bit_pattern, output[Abc_NtkPoNum(pNtk)];
    vector<string> bit_patterns;
    vector<unsigned> patterns;
    unsigned fanin0, fanin1;

    while (getline(in, bit_pattern)){
        bit_patterns.push_back(bit_pattern);
    }
    int remainder = 32;
    for(int i = 0; i < (bit_patterns.size() - 1) / 32 + 1; i++){
        if(i == (bit_patterns.size() - 1) / 32){
            remainder = (bit_patterns.size() - 1) % 32 + 1;
        }
        Abc_NtkForEachPi(pNtk, pPi, k){
            unsigned pattern = 0;
            for(int j = 0; j < remainder; j++){
                pattern = pattern | (unsigned((bit_patterns[i*32+j][k] - '0')) << j);
            }
            pPi->iTemp = pattern; 
        }

        Abc_NtkForEachObj( pNtk, pObj, j ){
            if(pObj->Type != ABC_OBJ_CONST1 && pObj->Type != ABC_OBJ_PI && pObj->Type != ABC_OBJ_PO){
                fanin0 = pObj->fCompl0? ~Abc_ObjFanin0(pObj)->iTemp : Abc_ObjFanin0(pObj)->iTemp;
                fanin1 = pObj->fCompl1? ~Abc_ObjFanin1(pObj)->iTemp : Abc_ObjFanin1(pObj)->iTemp;
                pObj->iTemp = fanin0 & fanin1;
            }
        }

        Abc_NtkForEachPo(pNtk, pPo, k){
            fanin0 = pPo->fCompl0? ~Abc_ObjFanin0(pPo)->iTemp : Abc_ObjFanin0(pPo)->iTemp;
            for(int j = 0; j < remainder; j++){
                output[k] += to_string((fanin0>>j) & 1);
            }
        }
    }
    
    Abc_NtkForEachPo(pNtk, pPo, k){
        cout<<Abc_ObjName(pPo)<<": "<<output[k]<<endl;
    }

    return 0;
}