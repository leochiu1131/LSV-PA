#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <stdint.h>

#include "bdd/cudd/cudd.h"
#include "bdd/cudd/cuddInt.h"

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv); //argument count , argument value
static int Lsv_CommandSimulateBdd(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimulateAig(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_node", Lsv_CommandPrintNodes, 0); 
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", Lsv_CommandSimulateBdd, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig", Lsv_CommandSimulateAig, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

void Lsv_NtkPrintNodes(Abc_Ntk_t* pNtk) {
  Abc_Obj_t* pObj;
  int i;
  Abc_NtkForEachNode(pNtk, pObj, i) { //for each fanout
    printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
    Abc_Obj_t* pFanin;
    int j;
    Abc_ObjForEachFanin(pObj, pFanin, j) { // for each fanin of this fanout
      printf("  Fanin-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanin), Abc_ObjName(pFanin));
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


int Lsv_CommandSimulateBdd(Abc_Frame_t* pAbc, int argc, char** argv) {
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    char* input_pattern;
    if (argc != 2) {
        Abc_Print(-2, "usage: lsv_sim_bdd <input_pattern>\n");
        return 1;
    }

    input_pattern = argv[1];
    Abc_Obj_t* pPo;
    int ithPo;
    
    // Loop through each PO and simulate
    Abc_NtkForEachPo(pNtk, pPo, ithPo) { // iterate all fanouts
        Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo);
        assert(Abc_NtkIsBddLogic(pRoot->pNtk));
        DdManager* dd = (DdManager*)pRoot->pNtk->pManFunc;
        DdNode* ddnode = (DdNode*)pRoot->pData;
        DdNode* temp = ddnode;

        
        // Cofactor for each input bit in pattern
        Abc_Obj_t* pCiObj;
        
        int FaninIdx;
        Abc_Obj_t * pFanin; //iterate all fanin names
        // printf( "\nnew pRoot  \n" );
        Abc_ObjForEachFanin( pRoot, pFanin, FaninIdx ){ //do cofactor for each fanin
          char* faninName =  Abc_ObjName(pFanin);
          // printf("%s,%d\n", faninName, FaninIdx);
          // printf("%s ", faninName);
          int varIndex = -1;
          Abc_NtkForEachCi(pNtk, pCiObj, varIndex) { //search for fanin name
              if (strcmp(faninName, Abc_ObjName(pCiObj)) == 0) {
                  break;
              }
          }
          if (varIndex == -1) {
              Abc_Print(-2, "Error: Could not find CI with the name %s\n", faninName);
              return 1;
          }
          // printf("varindex: %d ", varIndex);
          int bit = input_pattern[varIndex] - '0';
          // printf("bit: %d\n", bit);

          DdNode* var = Cudd_bddIthVar(dd, FaninIdx);
          temp = Cudd_Cofactor(dd, temp, bit ? var : Cudd_Not(var));
          Cudd_Ref(temp);  // Keep the BDD node
        }
        // Check if the result equals to constant 1
        int result = (temp == Cudd_ReadOne(dd));
        
        // Print the result
        Abc_Print(1, "%s: %d\n", Abc_ObjName(pPo), result);

        // Dereference all temporary BDD nodes
        Cudd_RecursiveDeref(dd, temp);

    }
    return 0;
}

/*
1. To find the BDD node associated with each PO, use the codes below.

Abc_NtkForEachPo(pNtk, pPo, ithPo) {
    Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo); 
    assert( Abc_NtkIsBddLogic(pRoot->pNtk) );
    DdManager * dd = (DdManager *)pRoot->pNtk->pManFunc;  
    DdNode* ddnode = (DdNode *)pRoot->pData;
}


2. To find the variable order of the BDD, you may use the following codes to find the variable name array.

char** vNamesIn = (char**) Abc_NodeGetFaninNames(pRoot)->pArray;
*/


int Lsv_CommandSimulateAig(Abc_Frame_t* pAbc, int argc, char** argv) {
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    char* input_pattern;
    if (argc != 2) {
        Abc_Print(-2, "usage: lsv_sim_aig <input_pattern>\n");
        return 1;
    }

    // Convert char* input_pattern to int* pi_values
    input_pattern = argv[1];
    size_t length = strlen(input_pattern);
    int* pi_values = (int*) malloc(length * sizeof(int));
    for(int i = 0; i < length; i++) {
      pi_values[i] = (input_pattern[i] == '0') ? 0 : 1;    
    }
    for(int i = 0; i < length; i++) {
      printf("i:%d,val:%d\n",i,pi_values[i]);
    }

    Abc_Obj_t* pObj;
    int i;

    // Ensure the network is an AIG
    if (!Abc_NtkIsStrash(pNtk)) {
        printf("Not a strashed AIG\n");
        return 0;
    }

    // Assign primary input (PI) values
    i = 0;
    Abc_NtkForEachPi(pNtk, pObj, i) {
        pObj->pData = (void*)((long)pi_values[i]);
        printf("  assign: Id = %d, name = %s val:%d\n", Abc_ObjId(pObj), Abc_ObjName(pObj), pi_values[i]);
    }

    // Evaluate internal nodes
    Abc_NtkForEachNode(pNtk, pObj, i) {
        Abc_Obj_t *pFanin0 = Abc_ObjFanin0(pObj);
        Abc_Obj_t *pFanin1 = Abc_ObjFanin1(pObj);
        
        int value1 = (int)((long)pFanin0->pData) ^ Abc_ObjFaninC0(pObj);
        int value2 = (int)((long)pFanin1->pData) ^ Abc_ObjFaninC1(pObj);

        int result = value1 && value2;
        printf("Fanout Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
        printf("  Fanin-0: Id = %d, name = %s val:%d\n", Abc_ObjId(pFanin0), Abc_ObjName(pFanin0), value1);
        printf("  Fanin-1: Id = %d, name = %s val:%d\n", Abc_ObjId(pFanin1), Abc_ObjName(pFanin1), value2);
        printf("  result:%d\n", result);


        pObj->pData = (void*)((long)result);
    }

    // Evaluate and print primary output (PO) values
    printf("Primary Output Values:\n");
    Abc_NtkForEachPo(pNtk, pObj, i) {
        int value = (int)((long)Abc_ObjFanin0(pObj)->pData ^ Abc_ObjFaninC0(pObj));
        printf("%s: %d\n", Abc_ObjName(pObj), value);
    }

    free(pi_values);
    return 0;
}
