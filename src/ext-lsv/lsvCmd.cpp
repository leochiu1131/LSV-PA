#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <stdint.h>

#include "bdd/cudd/cudd.h"
#include "bdd/cudd/cuddInt.h"
#include "sat/cnf/cnf.h"

#include <vector>
#include <unordered_map>
#include <iostream>
extern "C"{
    Aig_Man_t* Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
 }
using namespace std;

static int Lsv_CommandPrintNodes (Abc_Frame_t* pAbc, int argc, char** argv); //argument count , argument value
static int Lsv_CommandSimulateBdd(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimulateAig(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSymBDD     (Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSymSAT     (Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_node_", Lsv_CommandPrintNodes , 0); 
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd"    , Lsv_CommandSimulateBdd, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig"    , Lsv_CommandSimulateAig, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_bdd"    , Lsv_CommandSymBDD     , 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_sat"    , Lsv_CommandSymSAT     , 0);
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

  int numPIs = Abc_NtkPiNum(pNtk); // Number of Primary Inputs
  int numPOs = Abc_NtkPoNum(pNtk); // Number of Primary Outputs

  printf("Number of Primary Inputs: %d\n", numPIs);
  printf("Number of Primary Outputs: %d\n", numPOs);
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

// ================= LSV Command SymBDD Start ===================================
// todo : will PO change order?
int Lsv_CommandSimulateBddChar(Abc_Frame_t* pAbc, char* input_pattern , int pOk) {
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    if (!input_pattern) {
        Abc_Print(-2, "Error: input_pattern is NULL\n");
        return 1;
    }
    if (pOk < 0 || pOk >= Abc_NtkPoNum(pNtk)) {
        Abc_Print(-2, "Error: pOk is out of range\n");
        return 1;
    }

    Abc_Obj_t* pPo;
    int ithPo;

    // Loop through each PO and simulate
    Abc_NtkForEachPo(pNtk, pPo, ithPo) {
        if (ithPo != pOk) {
            continue; // Skip all but the pOk-th PO
        }
        Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo);
        assert(Abc_NtkIsBddLogic(pRoot->pNtk));
        DdManager* dd = (DdManager*)pRoot->pNtk->pManFunc;
        DdNode* ddnode = (DdNode*)pRoot->pData;
        DdNode* temp = ddnode;

        // Cofactor for each input bit in pattern
        Abc_Obj_t* pCiObj;
        int FaninIdx;
        Abc_Obj_t * pFanin;
        Abc_ObjForEachFanin(pRoot, pFanin, FaninIdx){
            char* faninName = Abc_ObjName(pFanin);
            int varIndex = -1;
            Abc_NtkForEachCi(pNtk, pCiObj, varIndex) {
                if (strcmp(faninName, Abc_ObjName(pCiObj)) == 0) {
                    break;
                }
            }
            if (varIndex == -1) {
                Abc_Print(-2, "Error: Could not find CI with the name %s\n", faninName);
                return 1;
            }
            int bit = input_pattern[varIndex] - '0';

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

        return result; // return the pOk-th output result
        break;
    }
    return 0;
}

void iterateCombinations(Abc_Frame_t* pAbc, char *array, int n, int current, int k, int i, int j) {
    if (current == n) {
        array[i] = '0';
        array[j] = '1';
        // int result1 = Lsv_CommandSimulateBddChar(pAbc, array, k);
        array[i] = '1';
        array[j] = '0';
        // int result2 = Lsv_CommandSimulateBddChar(pAbc, array, k);
        printf("\n");
        // return ((result1 == result2) ? true : false) ;
        return ;
    }

    // Skip changing the value at indices i and j
    if (current == i || current == j) {
        iterateCombinations(pAbc, array, n, current + 1, k, i, j);
        return;
    }

    // Set current index to '0' and then to '1'
    array[current] = '0';
    iterateCombinations(pAbc, array, n, current + 1, k, i, j);
    array[current] = '1';
    iterateCombinations(pAbc, array, n, current + 1, k, i, j);
}

void printCubePattern(char* CubePattern, int numPIs){
  for (int i = 0; i < numPIs; ++i) {
      if (CubePattern[i] == 1) { printf("1"); }
      else if (CubePattern[i] == 2) { printf("0"); }
      else if(CubePattern[i] == 0){ printf("0"); }
      else { printf("?"); }
  }
  printf("\n"); 
}

int Lsv_CommandSymBDD(Abc_Frame_t* pAbc, int argc, char** argv) {
  if (argc != 4) { // Expecting four arguments: cmd, i, j, and k
      Abc_Print(-2, "usage: lsv_sym_bdd <k> <i> <j>\n");
      return 1;
  }
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  if( Abc_NtkIsBddLogic(pNtk) == false){  
    Abc_Print(-2, "Network is not BDD \n");
    return 1; 
  }

  int outk = stoi(argv[1]); // Convert k from string to integer
  int ini  = stoi(argv[2]); // Convert i from string to integer
  int iBdd = -1;
  int inj  = stoi(argv[3]); // Convert j from string to integer
  int jBdd = -1;
  int numPIs = Abc_NtkPiNum(pNtk); // Number of Primary Inputs
  int numPOs = Abc_NtkPoNum(pNtk); // Number of Primary Outputs
  if ( numPIs <= ini || numPIs <= inj || numPOs <= outk  ) { Abc_Print(-2, "index error \n"); return 1; }
  if (ini == inj){ printf("symmetric\n");  return 1;}
  if (ini > inj ){ ABC_SWAP(int, ini, inj); }
  assert(ini <= inj); 

  Abc_Obj_t* pPo = Abc_NtkPo(pNtk, outk);
  Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo); 
  DdManager* ddManager = (DdManager*)pRoot->pNtk->pManFunc;
  DdNode   * Func = (DdNode*)pRoot->pData;
  cuddRef(Func);
  // hash map of PI string name to index
  Abc_Obj_t *pPi;
  int idx;
  std::unordered_map< std::string, int > ABCPiName2Idx;
  Abc_NtkForEachPi( pNtk, pPi, idx ){
    ABCPiName2Idx[ Abc_ObjName(pPi)  ] = idx;
  }
  char** FinArray = (char**) Abc_NodeGetFaninNames(pRoot)->pArray;
  int FinNum = Abc_ObjFaninNum(pRoot);
  
  for (int i = 0; i < FinNum; i++) { //compare name
      if (strcmp(FinArray[i], Abc_ObjName(Abc_NtkPi(pNtk, ini))) == 0) { iBdd = i; }
      if (strcmp(FinArray[i], Abc_ObjName(Abc_NtkPi(pNtk, inj))) == 0) { jBdd = i; }
  }

  if( iBdd == -1 && jBdd == -1 ){ printf("symmetric\n");  return 1; }
  DdNode* Cofac_i0_j1;
  DdNode* Cofac_i1_j0;
  // iBdd is DC
  if( iBdd == -1 && jBdd != -1 ){  
    Cofac_i0_j1 = Cudd_Cofactor(ddManager, Func, Cudd_bddIthVar(ddManager, jBdd));          Cudd_Ref(Cofac_i0_j1);
    Cofac_i1_j0 = Cudd_Cofactor(ddManager, Func, Cudd_Not(Cudd_bddIthVar(ddManager, jBdd)));Cudd_Ref(Cofac_i1_j0);
  }
  // jBdd is DC
  else if( iBdd != -1 && jBdd == -1 ){  
    Cofac_i0_j1 = Cudd_Cofactor(ddManager, Func, Cudd_Not(Cudd_bddIthVar(ddManager, iBdd))); Cudd_Ref(Cofac_i0_j1);
    Cofac_i1_j0 = Cudd_Cofactor(ddManager, Func, Cudd_bddIthVar(ddManager, iBdd));           Cudd_Ref(Cofac_i1_j0);
  }
  // jBdd and iBdd both in BDD
  else{
    assert (iBdd != -1 && jBdd != -1 );

    DdNode* cube_i0    = Cudd_Not(Cudd_bddIthVar(ddManager, iBdd));   Cudd_Ref( cube_i0 );
    DdNode* cube_j1    = Cudd_bddIthVar(ddManager, jBdd);             Cudd_Ref( cube_j1 );
    DdNode* Cofac_i0   = Cudd_Cofactor(ddManager, Func, cube_i0);     Cudd_Ref( Cofac_i0);
    Cofac_i0_j1        = Cudd_Cofactor(ddManager, Cofac_i0, cube_j1); Cudd_Ref( Cofac_i0_j1);
    Cudd_RecursiveDeref(ddManager, cube_i0);
    Cudd_RecursiveDeref(ddManager, cube_j1);
    Cudd_RecursiveDeref(ddManager, Cofac_i0);


    DdNode* cube_i1    = Cudd_bddIthVar(ddManager, iBdd);             Cudd_Ref( cube_i1 );
    DdNode* cube_j0    = Cudd_Not(Cudd_bddIthVar(ddManager, jBdd));   Cudd_Ref( cube_j0 );
    DdNode* Cofac_i1   = Cudd_Cofactor(ddManager, Func, cube_i1);     Cudd_Ref( Cofac_i1);
    Cofac_i1_j0        = Cudd_Cofactor(ddManager, Cofac_i1, cube_j0); Cudd_Ref( Cofac_i1_j0);
    Cudd_RecursiveDeref(ddManager, cube_i1);
    Cudd_RecursiveDeref(ddManager, cube_j0);
    Cudd_RecursiveDeref(ddManager, Cofac_i1);
  }
  if (Cofac_i1_j0 == Cofac_i0_j1) {
    printf("symmetric\n");
  }
  else{
    printf("asymmetric\n");
    DdNode* XOR = Cudd_bddXor (ddManager, Cofac_i1_j0, Cofac_i0_j1); cuddRef(XOR);
    // char* CubePattern = new char [ddManager->size ];
    char* BDDCubePattern = (char*)malloc(numPIs * sizeof(char));
    char* ABCcounterExample = (char*)malloc(numPIs * sizeof(char));
    for (int i = 0; i < numPIs; ++i) { 
      BDDCubePattern[i] = 2;
      ABCcounterExample[i] = 2;
    }
    // printf("numPI:%d\n",numPIs);
    // printf("FanInNum:%d\n", FinNum);
    int success = Cudd_bddPickOneCube(ddManager, XOR, BDDCubePattern); //Picks one on-set cube randomly from the given DD
    if(success == 0){ printf("random pick failed\n"); assert(0); }
    // printCubePattern( BDDCubePattern, numPIs); //BDD Inputs
    for (int i = 0; i < FinNum; i++) { // here
        ABCcounterExample[ABCPiName2Idx[FinArray[i]]] = BDDCubePattern[i] ;
    }
    // printCubePattern( ABCcounterExample, numPIs); //ABC Inputs

    ABCcounterExample[ini] = 0; // cofactor 01
    ABCcounterExample[inj] = 1;
    printCubePattern(ABCcounterExample , numPIs);
    ABCcounterExample[ini] = 1; // cofactor 10
    ABCcounterExample[inj] = 0;
    printCubePattern(ABCcounterExample , numPIs);

    delete[] BDDCubePattern;
    delete[] ABCcounterExample;
    Cudd_RecursiveDeref(ddManager, XOR);
  }
  
  Cudd_RecursiveDeref(ddManager, Cofac_i0_j1);
  Cudd_RecursiveDeref(ddManager, Cofac_i1_j0);
  Cudd_RecursiveDeref(ddManager, Func);
  
  return 0;
}

// ================= LSV Command Try End ===================================

int Lsv_CommandSymSAT(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t * pNtk = Abc_FrameReadNtk(pAbc);
  if (!Abc_NtkIsStrash(pNtk)) {
    Abc_Print(-2, "Network is not AIG \n");      
    return 1;
  }

  int outk = stoi(argv[1]); // Convert k from string to integer
  int ini  = stoi(argv[2]); // Convert i from string to integer
  int inj  = stoi(argv[3]); // Convert j from string to integer
  int numPIs = Abc_NtkPiNum(pNtk); // Number of Primary Inputs
  int numPOs = Abc_NtkPoNum(pNtk); // Number of Primary Outputs
  if ( numPIs <= ini || numPIs <= inj || numPOs <= outk  ) { Abc_Print(-2, "index error \n"); return 1; }
  if (ini == inj){ printf("symmetric\n");  return 1;}
  if (ini > inj ){ ABC_SWAP(int, ini, inj); }
  assert(ini <= inj); 

  // Abc_Obj_t* PO = Abc_NtkPo(pNtk, outk);
  // 1. extract cone
  Abc_Ntk_t* ConeNtk = Abc_NtkCreateCone(pNtk, Abc_ObjFanin0(Abc_NtkPo(pNtk, outk)), Abc_ObjName(Abc_NtkPo(pNtk, outk)), 1);
  // 2. derive a corresponding AIG circuit.
  Aig_Man_t* AigNtk = Abc_NtkToDar(ConeNtk, 0, 0);
  // 3. sat solver
  sat_solver* satSolver = sat_solver_new();
  // 4. cnf data
  Cnf_Dat_t* CnfData = Cnf_Derive(AigNtk, 1);
  // 5. cnf write to SAT solver
  Cnf_DataWriteIntoSolverInt( satSolver, CnfData, 1, 0 );
  // 6. Use Cnf DataLift to create another CNF formula CB 
  Cnf_DataLift(CnfData, Aig_ManObjNum(AigNtk));
  Cnf_DataWriteIntoSolverInt( satSolver, CnfData, 1, 0 );
  // 7. For each input xt of the circuit, find its corresponding CNF variables


  printf("symmetric\n");  return 1;

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
    std::vector<std::vector<int> >  ans;
    // char* input_pattern;
    if (argc != 2) {
        Abc_Print(-2, "usage: lsv_sim_aig <input_pattern>\n");
        return 1;
    }
    FILE *fp = fopen(argv[1], "r");
    if (fp == NULL) {
        printf("File not found!\n");
        return 1;
    }
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        int len = 0;
        for (len = 0; line[len] != '\n' && line[len] != '\0'; ++len);

        int *pi_values = (int*)malloc(len * sizeof(int));
        std::vector<int> anstemp;

        for (int i = 0; i < len; ++i) {
            pi_values[i] = line[i] - '0';
        }

        // Do something with pattern here
        // printf("input:");
        // for (int i = 0; i < len; ++i) {
        //     printf("%d", pi_values[i]);
        // } printf("\n");

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
            // printf("  assign: Id = %d, name = %s val:%d\n", Abc_ObjId(pObj), Abc_ObjName(pObj), pi_values[i]);
        }

        // Evaluate internal nodes
        Abc_NtkForEachNode(pNtk, pObj, i) {
            Abc_Obj_t *pFanin0 = Abc_ObjFanin0(pObj);
            Abc_Obj_t *pFanin1 = Abc_ObjFanin1(pObj);
            
            int value1 = (int)((long)pFanin0->pData) ^ Abc_ObjFaninC0(pObj); //^ for the invert operation
            int value2 = (int)((long)pFanin1->pData) ^ Abc_ObjFaninC1(pObj);

            int result = value1 && value2;
            // printf("Fanout Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
            // printf("  Fanin-0: Id = %d, name = %s val:%d\n", Abc_ObjId(pFanin0), Abc_ObjName(pFanin0), value1);
            // printf("  Fanin-1: Id = %d, name = %s val:%d\n", Abc_ObjId(pFanin1), Abc_ObjName(pFanin1), value2);
            // printf("  result:%d\n", result);


            pObj->pData = (void*)((long)result);
        }

        // Evaluate and print primary output (PO) values
        // printf("Primary Output Values:\n");
        Abc_NtkForEachPo(pNtk, pObj, i) {
            int value = (int)((long)Abc_ObjFanin0(pObj)->pData ^ Abc_ObjFaninC0(pObj));
            // printf("%s: %d\n", Abc_ObjName(pObj), value);
            anstemp.push_back(value);
        }
        ans.push_back(anstemp);
        free(pi_values);
    }
    //output part
    int patternlen = ans.size();
    Abc_Obj_t* pObj;
    int POidx=0;
    Abc_NtkForEachPo(pNtk, pObj, POidx) {
      printf("%s: ", Abc_ObjName(pObj));
      for(int pat=0;pat<patternlen;pat++){
        printf("%d",ans[pat][POidx]);
      }
      printf("\n");
    }

    fclose(fp);
    return 0;

    /*
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
          
          int value1 = (int)((long)pFanin0->pData) ^ Abc_ObjFaninC0(pObj); //^ for the invert operation
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
      */


}
