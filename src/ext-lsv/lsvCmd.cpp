#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <cstring>
#include <stdio.h>
#include <vector>

#include "sat/cnf/cnf.h"
extern "C"{
  Aig_Man_t* Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
}

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimBdd(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSymBdd(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSymSat(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSymAll(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", Lsv_CommandSimBdd, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig", Lsv_CommandSimAig, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_bdd", Lsv_CommandSymBdd, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_sat", Lsv_CommandSymSat, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_all", Lsv_CommandSymAll, 0);
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

// The compilation is with CUDD.
void Lsv_SimBdd(Abc_Ntk_t* pNtk, char* input){
  assert( Abc_NtkIsBddLogic(pNtk) );
  // the input is in order od dd->vars[0], dd->vars[1] ... 

  int n = Abc_NtkCiNum(pNtk);
  int* array = (int*) malloc(n * sizeof(int));
  for (int i=0; i<n; i++){
    array[i] = input[i]-'0';
  }

  char** input_names = Abc_NtkCollectCioNames( pNtk, 0 );

#ifdef ABC_USE_CUDD
  Abc_Obj_t *pNode, *func;
  DdNode *temp;  //bdd node
  DdManager* dd = (DdManager*) pNtk->pManFunc; //bdd manager

  int i, j, k;
    
  Abc_NtkForEachPo( pNtk, pNode, i ){
    printf("%s: " ,Abc_ObjName(pNode));
    func = Abc_ObjFanin0(pNode); //the original function
    temp = (DdNode*) func->pData; //function to be recursively cofactored
    int FaninNum = Abc_ObjFaninNum(func);
    char** pNamesIn = (char**)Abc_NodeGetFaninNames(func)->pArray;

    for(j=0; j<FaninNum; j++){
      for(k=0;k<n;k++){
        if(strcmp(pNamesIn[j], input_names[k])==0 ){//equal
          // printf("input %d:  %s\n", j ,pNamesIn[j]);
          if(array[k]==0){
            temp = Cudd_Cofactor( dd, temp, Cudd_Not(dd->vars[j]) );
          }else{
            temp = Cudd_Cofactor( dd, temp, dd->vars[j] );
          }
        }
      }
    }

    DdNode* one = Cudd_ReadOne(dd);
    DdNode* zero = Cudd_Not(Cudd_ReadOne(dd));
    
    if(temp==zero){
      printf("0\n");
    }
    if(temp==one){
      printf("1\n");
    } 
  }

#endif
//free memory
free(array);
  
}

int Lsv_CommandSimBdd(Abc_Frame_t* pAbc, int argc, char** argv){
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
  Lsv_SimBdd(pNtk, argv[1]);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sim_bdd [-h] <input_pattern>\n");
  Abc_Print(-2, "\t        Simulates the function in BDD\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}



void Lsv_SimAig(Abc_Ntk_t* pNtk, char* input_file){
  Abc_Obj_t* pObj;
  int j;

  //open file
  FILE * fp = fopen(input_file, "r");
  if (fp == NULL){ printf("Error in opening input file \"%s\".\n", input_file ); return; }

  //num_row: linesize; num_col: number of lines, need to count first by reading file
  int num_row = Abc_NtkCiNum(pNtk); //first in data index
  int num_nodes = Abc_NtkNodeNum(pNtk); //excluding PI/PO, index starts from num_row + num_output + 1
  int num_output = Abc_NtkCoNum(pNtk); //second in data index
  int total_num_nodes = num_row + num_nodes + num_output + 1;// index 0 is not used
  int num_col = 0;//=0?

  char* buffer = (char*) malloc(num_row*sizeof(char)+1); // for the terminator
  while (fgets(buffer, num_row+2, fp)!=NULL){ //read at most count-1 chars from file
    if(strcmp(buffer, "\n") !=0){
      num_col++;
    }
  }
  fclose(fp);

  //allocate memory
  int** data = (int**)malloc(total_num_nodes* sizeof(int*));
  for(int i=0; i<total_num_nodes; i++){ data[i] = (int*) malloc(num_col*sizeof(int)); }

  //constant 1 node is indexed 0
  for(int i=0; i < num_col; i++){
    data[0][i] = 1;
  }

  /*transpose the input for input simulation pattern: 
  input_data[i] contains a parallel pattern for i-th PI, which is the first n elements of Foreachnode...*/
  num_col = 0;
  fp = fopen(input_file, "r");
  while (fgets(buffer, num_row+2, fp)!=NULL){
    if(strcmp(buffer, "\n") !=0){
      for(int i=1; i < num_row+1; i++){
        data[i][num_col] = buffer[i-1] - '0';
      }
      num_col++;
    }
  }
  // Abc_NtkForEachPi( pNtk, pObj, j ){
  //   int index = pObj->Id;
  //   printf("Internal index: %d, %s :",index,  Abc_ObjName(pObj));
  //   for(int i=0; i<num_col; i++){
  //     printf("%d",data[index][i]);
  //   }
  //   printf("\n");
  // }

  /* If the value is evaluated in this order, then when we want to 
  evaluate a new node, its input must already been finished.*/
  Vec_Ptr_t * order = Abc_AigDfsMap(pNtk);
  Vec_PtrForEachEntry( Abc_Obj_t *, order, pObj, j ){
    int index = pObj->Id;
    int in_0_index = Abc_ObjFanin0(pObj)->Id;
    int in_1_index = Abc_ObjFanin1(pObj)->Id;
    unsigned int compl_0 = pObj->fCompl0;
    unsigned int compl_1 = pObj->fCompl1;
    if(compl_0 && compl_1){
      for(int i=0; i<num_col; i++){
        data[index][i] = !data[in_0_index][i] && !data[in_1_index][i];
      }
    }else if(compl_0 && !compl_1){
      for(int i=0; i<num_col; i++){
        data[index][i] = !data[in_0_index][i] && data[in_1_index][i];
      }
    }else if(!compl_0 && compl_1){
      for(int i=0; i<num_col; i++){
        data[index][i] = data[in_0_index][i] && !data[in_1_index][i];
      }
    }else{
      for(int i=0; i<num_col; i++){
        data[index][i] = data[in_0_index][i] && data[in_1_index][i];
      }
    }
  }

  Abc_NtkForEachPo( pNtk, pObj, j ){
    // output should only have one fanin
    assert(Abc_ObjFaninNum(pObj) ==1);
    int index = pObj->Id;
    int in_0_index = Abc_ObjFanin0(pObj)->Id;
    unsigned int compl_0 = pObj->fCompl0;
    if(compl_0){
      for(int i=0; i<num_col; i++){
        data[index][i] = !data[in_0_index][i];
      }
    }else{
      for(int i=0; i<num_col; i++){
        data[index][i] = data[in_0_index][i];
      }
    }
    printf("%s: ", Abc_ObjName(pObj));
    for(int i=0; i<num_col; i++){
      printf("%d",data[index][i]);
    }
    printf("\n");
  }
  
  // free memory 
  for(int i=0; i<total_num_nodes; i++){ free(data[i]); }
  free(data);
  free(buffer);
  return;
}

int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv){
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
  Lsv_SimAig(pNtk, argv[1]);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sim_aig [-h] <input_pattern_file>\n");
  Abc_Print(-2, "\t        Simulates the function in AIG\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}


void Lsv_Bdd_Symmetric(Abc_Ntk_t* pNtk, char* yk, char* xi, char* xj){
  int y_k = atoi(yk);
  int x_i = atoi(xi);
  int x_j = atoi(xj);

#ifdef ABC_USE_CUDD
  Abc_Obj_t *pNode, *func;

  DdManager* dd = (DdManager*) pNtk->pManFunc; //bdd manager

  // int i, j, k;
  pNode = Abc_NtkPo(pNtk, y_k);
  func = Abc_ObjFanin0(pNode); //the original function

  DdNode *temp1 = (DdNode*) func->pData; //function to be recursively cofactored;
  Cudd_Ref(temp1);
  DdNode *temp2 = (DdNode*) func->pData; //function to be recursively cofactored;  
  Cudd_Ref(temp2);

  int i, j;

  int CinNum = Abc_NtkCiNum(pNtk);
  char** input_names = Abc_NtkCollectCioNames( pNtk, 0 ); // network input
  int* input_assignment = (int*) malloc(CinNum*sizeof(int)); // assignment for temp1

  // Cudd_SupportSize(dd, temp1)
  int FaninNum = Abc_ObjFaninNum(func);
  char** pNamesIn = (char**)Abc_NodeGetFaninNames(func)->pArray; //node input

  // for(j=0; j<FaninNum; j++){
  //   printf("%s, ", pNamesIn[j]);
  // }

  for(i=0; i<CinNum; i++){ // initialize value
    input_assignment[i] = 0;
  }
  int calc_unused = 0;

  for(j=0; j<FaninNum; j++){
    if(strcmp(pNamesIn[j], input_names[x_i])==0 ){//equal to first input 
      DdNode *temp1_new = Cudd_Cofactor( dd, temp1, Cudd_Not(dd->vars[j]) );
      Cudd_Ref(temp1_new);
      Cudd_RecursiveDeref(dd, temp1);
      temp1 = temp1_new;
      // temp1 = Cudd_Cofactor( dd, temp1, Cudd_Not(dd->vars[j]) );
      input_assignment[x_i] = 0;

      DdNode *temp2_new = Cudd_Cofactor( dd, temp2, dd->vars[j] );
      Cudd_Ref(temp2_new);
      Cudd_RecursiveDeref(dd, temp2);
      temp2 = temp2_new;
      // temp2 = Cudd_Cofactor( dd, temp2, dd->vars[j] );
      calc_unused += 1;
    }
    else if(strcmp(pNamesIn[j], input_names[x_j])==0 ){//equal to second input 
      DdNode *temp1_new = Cudd_Cofactor( dd, temp1, dd->vars[j] );
      Cudd_Ref(temp1_new);
      Cudd_RecursiveDeref(dd, temp1);
      temp1 = temp1_new;

      // temp1 = Cudd_Cofactor( dd, temp1, dd->vars[j] );
      input_assignment[x_j] = 1;

      DdNode *temp2_new = Cudd_Cofactor( dd, temp2, Cudd_Not(dd->vars[j]) );
      Cudd_Ref(temp2_new);
      Cudd_RecursiveDeref(dd, temp2);
      temp2 = temp2_new;
      // temp2 = Cudd_Cofactor( dd, temp2, Cudd_Not(dd->vars[j]) );
      calc_unused += 2;
    }
  }
  if(calc_unused == 1){//x_j is not set (since it may not be in support)
    input_assignment[x_j] = !input_assignment[x_i];
  }else if(calc_unused == 2){ //x_i is not set
    input_assignment[x_i] = !input_assignment[x_j];
  }
  
  if(temp1==temp2){
    printf("symmetric\n");
  }else{
    printf("asymmetric\n");
    for(i=0; i<CinNum; i++){
      if(i!=x_i && i!=x_j){
        for(j=0; j<FaninNum; j++){
          if(strcmp(pNamesIn[j], input_names[i])==0 ){
            DdNode *temp1_new = Cudd_Cofactor( dd, temp1, dd->vars[j] );
            Cudd_Ref(temp1_new);
            DdNode *temp2_new = Cudd_Cofactor( dd, temp2, dd->vars[j] );
            Cudd_Ref(temp2_new);
            if(temp1_new != temp2_new){
              Cudd_RecursiveDeref(dd, temp1);
              temp1 = temp1_new;
              Cudd_RecursiveDeref(dd, temp2);
              temp2 = temp2_new;
              input_assignment[i] = 1;
            }else{
              Cudd_RecursiveDeref(dd, temp1_new);
              Cudd_RecursiveDeref(dd, temp2_new);
              DdNode *temp1_new_2 = Cudd_Cofactor( dd, temp1,  Cudd_Not(dd->vars[j]) );
              Cudd_Ref(temp1_new_2);
              Cudd_RecursiveDeref(dd, temp1);
              temp1 = temp1_new_2;
              DdNode *temp2_new_2 = Cudd_Cofactor( dd, temp2,  Cudd_Not(dd->vars[j]) );
              Cudd_Ref(temp2_new_2);
              Cudd_RecursiveDeref(dd, temp2);
              temp2 = temp2_new_2;

              input_assignment[i] = 0;
            }


            // if(Cudd_Cofactor(dd, temp1, dd->vars[j]) != Cudd_Cofactor(dd, temp2, dd->vars[j])){
            //   DdNode *temp1_new = Cudd_Cofactor( dd, temp1, dd->vars[j] );
            //   Cudd_Ref(temp1_new);
            //   DdNode *temp2_new = Cudd_Cofactor( dd, temp2, dd->vars[j] );
            //   Cudd_Ref(temp2_new);

            //   if(temp1!= nullptr)Cudd_RecursiveDeref(dd, temp1);
            //   temp1 = temp1_new;

            //   if(temp2!= nullptr)Cudd_RecursiveDeref(dd, temp2);
            //   temp2 = temp2_new;

            //   // temp1 = Cudd_Cofactor(dd, temp1, dd->vars[j]);
            //   // temp2 = Cudd_Cofactor(dd, temp2, dd->vars[j]);
            //   input_assignment[i] = 1;
            // }else{
            //   DdNode *temp1_new = Cudd_Cofactor( dd, temp1,  Cudd_Not(dd->vars[j]) );
            //   Cudd_Ref(temp1_new);
            //   if(temp1!= nullptr)Cudd_RecursiveDeref(dd, temp1);
            //   temp1 = temp1_new;

            //   DdNode *temp2_new = Cudd_Cofactor( dd, temp2,  Cudd_Not(dd->vars[j]) );
            //   Cudd_Ref(temp2_new);
            //   if(temp2!= nullptr)Cudd_RecursiveDeref(dd, temp2);
            //   temp2 = temp2_new;
            //   // temp1 = Cudd_Cofactor(dd, temp1, Cudd_Not(dd->vars[j]));
            //   // temp2 = Cudd_Cofactor(dd, temp2, Cudd_Not(dd->vars[j]));
            //   input_assignment[i] = 0;
            // }
          }
        }
      }
    } 
    for(i=0 ;i<CinNum; i++){
      printf("%d", input_assignment[i]);
    }
    printf("\n");
    for(i=0;i<CinNum; i++){
      if(i==x_i || i==x_j){
        printf("%d", !input_assignment[i]);
      }else{
        printf("%d", input_assignment[i]);
      }
    }
    printf("\n");

    // check asymmetry
    // DdNode* one = Cudd_ReadOne(dd);
    // DdNode* zero = Cudd_Not(Cudd_ReadOne(dd));
    
    // if(temp1==zero){
    //   printf("temp1 = 0\n");
    // }
    // if(temp1==one){
    //   printf("temp1 = 1\n");
    // } 
    // if(temp2==zero){
    //   printf("temp2 = 0\n");
    // }
    // if(temp2==one){
    //   printf("temp2 = 1\n");
    // } 
  }


  Cudd_RecursiveDeref(dd, temp1);
  Cudd_RecursiveDeref(dd, temp2);

  free(input_assignment);
#endif
}

int Lsv_CommandSymBdd(Abc_Frame_t* pAbc, int argc, char** argv){
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
  Lsv_Bdd_Symmetric(pNtk, argv[1], argv[2], argv[3]);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sym_bdd [-h] <output_pin> <input_pin1> <input_pin2>\n");
  Abc_Print(-2, "\t        Check the symmetry of the output with respect to two inputs by BDD\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

void Lsv_Sat_Symmetric(Abc_Ntk_t* pNtk, char* yk, char* xi, char* xj){
  int y_k = atoi(yk);
  int x_i = atoi(xi);
  int x_j = atoi(xj);

  int CinNum = Abc_NtkCiNum(pNtk);
  int CoutNum = Abc_NtkCoNum(pNtk);

  if(x_i >= CinNum || x_j >= CinNum || y_k >= CoutNum){
    Abc_Print(-1, "Input/Output pin is not available.\n");
  }

  Abc_Obj_t *pNode =  Abc_ObjFanin0(Abc_NtkPo(pNtk, y_k)); //k-th output node
  Abc_Ntk_t *cone_k = Abc_NtkCreateCone(pNtk, pNode, "New_cone", 1);
  Aig_Man_t *pAig = Abc_NtkToDar(cone_k, 0, 1);//aig manager
  Cnf_Dat_t *pCnf = Cnf_Derive(pAig, 1);//cnf, nOutputs: number of outputs

  sat_solver *pSat = sat_solver_new(); //sat
  // nframes:  sat solver will setnvars to  p->nVars * nFrames 
  pSat = (sat_solver*)Cnf_DataWriteIntoSolverInt( pSat, pCnf, 1, 0 );
  // printf("sat size: %d\n", pSat -> size);
  Cnf_DataLift(pCnf, pCnf->nVars); //increments variables in a formula by the number
  pSat = (sat_solver*)Cnf_DataWriteIntoSolverInt( pSat, pCnf, 1, 0 );
  Cnf_DataLift(pCnf, -pCnf->nVars);
  // printf("sat size: %d\n", pSat -> size);

  int t;
  int status;
  Aig_Obj_t* pObj;
  int literals_arr[2]; // stores the literals in the clause

  Aig_ManForEachCi( pAig, pObj, t ){
    if(t==x_i){
      literals_arr[0] = toLitCond(pCnf->pVarNums[pObj->Id], 0); //xi in A
      literals_arr[1] = toLitCond(pCnf->pVarNums[Aig_ManCi(pAig , x_j)->Id] + (pCnf->nVars), 1); //xj in B
      status = sat_solver_addclause(pSat, literals_arr, literals_arr + 2);
      assert( status );
      literals_arr[0] = toLitCond(pCnf->pVarNums[pObj->Id], 1); //xi in A
      literals_arr[1] = toLitCond(pCnf->pVarNums[Aig_ManCi(pAig , x_j)->Id] + (pCnf->nVars), 0); //xj in B
      status = sat_solver_addclause(pSat, literals_arr, literals_arr + 2);
      assert( status );
    }else if(t==x_j){
      literals_arr[0] = toLitCond(pCnf->pVarNums[pObj->Id], 0); //xj in A
      literals_arr[1] = toLitCond(pCnf->pVarNums[Aig_ManCi(pAig , x_i)->Id] + (pCnf->nVars), 1); //xi in B
      status = sat_solver_addclause(pSat, literals_arr, literals_arr + 2);
      assert( status );
      literals_arr[0] = toLitCond(pCnf->pVarNums[pObj->Id], 1); //xj in A
      literals_arr[1] = toLitCond(pCnf->pVarNums[Aig_ManCi(pAig , x_i)->Id] + (pCnf->nVars), 0); //xi in B
      status = sat_solver_addclause(pSat, literals_arr, literals_arr + 2);
      assert( status );
    }
    else{
      /* vA(t) == vB (t)  = (vA(t) -> vB(t)) and (vB(t) -> vA(t))  */
      literals_arr[0] = toLitCond(pCnf->pVarNums[pObj->Id], 0); //vA
      literals_arr[1] = toLitCond(pCnf->pVarNums[pObj->Id] + (pCnf->nVars), 1); //vB
      status = sat_solver_addclause(pSat, literals_arr, literals_arr + 2);
      assert( status );
      literals_arr[0] = toLitCond(pCnf->pVarNums[pObj->Id], 1);
      literals_arr[1] = toLitCond(pCnf->pVarNums[pObj->Id] + (pCnf->nVars), 0);
      status = sat_solver_addclause(pSat, literals_arr, literals_arr + 2);
      assert( status );
    }
  }

  //add clause to XOR output
  Aig_ManForEachCo( pAig, pObj, t ){
    literals_arr[0] = toLitCond(pCnf->pVarNums[pObj->Id], 0); //vA
    literals_arr[1] = toLitCond(pCnf->pVarNums[pObj->Id] + (pCnf->nVars), 0); //vB
    status = sat_solver_addclause(pSat, literals_arr, literals_arr + 2);
    assert( status );
    literals_arr[0] = toLitCond(pCnf->pVarNums[pObj->Id], 1);
    literals_arr[1] = toLitCond(pCnf->pVarNums[pObj->Id] + (pCnf->nVars), 1);
    status = sat_solver_addclause(pSat, literals_arr, literals_arr + 2);
    assert( status );
  }

  status = sat_solver_solve(pSat, nullptr, nullptr, (ABC_INT64_T)0, (ABC_INT64_T)0, (ABC_INT64_T)0, (ABC_INT64_T)0 );
  if(status == l_False){ // unsat
    printf("symmetric\n");
  }
  else if(status == l_True){ //sat
    printf("asymmetric\n");
    // print the sat values
    Aig_ManForEachCi( pAig, pObj, t ){
      printf("%d", sat_solver_var_value(pSat, pCnf->pVarNums[pObj->Id]));
    }
    printf("\n");
    Aig_ManForEachCi( pAig, pObj, t ){
      printf("%d", sat_solver_var_value(pSat, pCnf->pVarNums[pObj->Id] + (pCnf->nVars) ));
    }
    printf("\n");
  }
  else { // undefined
    printf("undefined\n");
  }
}

int Lsv_CommandSymSat(Abc_Frame_t* pAbc, int argc, char** argv){
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
  if(!Abc_NtkIsStrash(pNtk)){
    Abc_Print(-1, "You should first 'strash' then call this function.\n");
  }
  Lsv_Sat_Symmetric(pNtk, argv[1], argv[2], argv[3]);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sym_sat [-h] <output_pin> <input_pin1> <input_pin2>\n");
  Abc_Print(-2, "\t        Check the symmetry of the output with respect to two inputs by SAT\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

void Lsv_Sat_All(Abc_Ntk_t* pNtk, char* yk){
  int y_k = atoi(yk);

  int CoutNum = Abc_NtkCoNum(pNtk);

  if(y_k >= CoutNum){
    Abc_Print(-1, "Output pin is not available.\n");
  }

  Abc_Obj_t *pNode =  Abc_ObjFanin0(Abc_NtkPo(pNtk, y_k)); //k-th output node
  Abc_Ntk_t *cone_k = Abc_NtkCreateCone(pNtk, pNode, "New_cone", 1);
  Aig_Man_t *pAig = Abc_NtkToDar(cone_k, 0, 1);//aig manager
  Cnf_Dat_t *pCnf = Cnf_Derive(pAig, 1);//cnf, nOutputs: number of outputs

  sat_solver *pSat = sat_solver_new(); //sat 
  
  // nframes:  sat solver will setnvars to  p->nVars * nFrames 
  pSat = (sat_solver*)Cnf_DataWriteIntoSolverInt( pSat, pCnf, 1, 0 );//vA
  // printf("sat size: %d\n", pSat -> size);
  // printf("cnf size: %d\n", pCnf -> nVars);
  Cnf_DataLift(pCnf, pCnf->nVars); //increments variables in a formula by the number

  // printf("sat size: %d", pSat -> size);
  pSat = (sat_solver*)Cnf_DataWriteIntoSolverInt( pSat, pCnf, 1, 0 );//vB

  for(int i=0; i<=Abc_NtkCiNum(pNtk); i++){
    sat_solver_addvar(pSat);
  }

  // Cnf_DataLift(pCnf, pCnf->nVars); 
  // pSat = (sat_solver*)Cnf_DataWriteIntoSolverInt( pSat, pCnf, 3, 0 );//vH
  Cnf_DataLift(pCnf, -pCnf->nVars);
  // printf("sat size: %d", pSat -> size);

  int t, t2, t3;
  int status;
  Aig_Obj_t* pObj;
  int literals_arr[4]; // stores the literals in the clause

  // vA(t)==vB(t) || vH(t)
  Aig_ManForEachCi( pAig, pObj, t ){
    literals_arr[0] = toLitCond(pCnf->pVarNums[pObj->Id], 0); //vA
    literals_arr[1] = toLitCond(pCnf->pVarNums[pObj->Id] + (pCnf->nVars), 1); //vB
    literals_arr[2] = toLitCond(pCnf->pVarNums[pObj->Id] + 2*(pCnf->nVars), 0); //vH
    status = sat_solver_addclause(pSat, literals_arr, literals_arr + 3);
    assert( status );
    literals_arr[0] = toLitCond(pCnf->pVarNums[pObj->Id], 1); //vA
    literals_arr[1] = toLitCond(pCnf->pVarNums[pObj->Id] + (pCnf->nVars), 0); //vB
    literals_arr[2] = toLitCond(pCnf->pVarNums[pObj->Id] + 2*(pCnf->nVars), 0); //vH
    status = sat_solver_addclause(pSat, literals_arr, literals_arr + 3);
    assert( status );
  }

  for(t=0; t<Aig_ManCiNum(pAig)-1; t++){
    for(t2=t+1;t2<Aig_ManCiNum(pAig); t2++){
      // vA(t1)==vB(t2) || vH(t1)' || vH(t2)'
      literals_arr[0] = toLitCond(pCnf->pVarNums[Aig_ManCi(pAig , t)->Id], 0);
      literals_arr[1] = toLitCond(pCnf->pVarNums[Aig_ManCi(pAig , t2)->Id] + (pCnf->nVars), 1);
      literals_arr[2] = toLitCond(pCnf->pVarNums[Aig_ManCi(pAig , t)->Id] + 2*(pCnf->nVars), 1);
      literals_arr[3] = toLitCond(pCnf->pVarNums[Aig_ManCi(pAig , t2)->Id] + 2*(pCnf->nVars), 1);
      status = sat_solver_addclause(pSat, literals_arr, literals_arr + 4);
      assert( status );
      literals_arr[0] = toLitCond(pCnf->pVarNums[Aig_ManCi(pAig , t)->Id], 1);
      literals_arr[1] = toLitCond(pCnf->pVarNums[Aig_ManCi(pAig , t2)->Id] + (pCnf->nVars), 0);
      literals_arr[2] = toLitCond(pCnf->pVarNums[Aig_ManCi(pAig , t)->Id] + 2*(pCnf->nVars), 1);
      literals_arr[3] = toLitCond(pCnf->pVarNums[Aig_ManCi(pAig , t2)->Id] + 2*(pCnf->nVars), 1);
      status = sat_solver_addclause(pSat, literals_arr, literals_arr + 4);
      assert( status );

       // vA(t2)==vB(t1) || vH(t1)' || vH(t2)'
      literals_arr[0] = toLitCond(pCnf->pVarNums[Aig_ManCi(pAig , t2)->Id], 0);
      literals_arr[1] = toLitCond(pCnf->pVarNums[Aig_ManCi(pAig , t)->Id] + (pCnf->nVars), 1);
      literals_arr[2] = toLitCond(pCnf->pVarNums[Aig_ManCi(pAig , t)->Id] + 2*(pCnf->nVars), 1);
      literals_arr[3] = toLitCond(pCnf->pVarNums[Aig_ManCi(pAig , t2)->Id] + 2*(pCnf->nVars), 1);
      status = sat_solver_addclause(pSat, literals_arr, literals_arr + 4);
      assert( status );
      literals_arr[0] = toLitCond(pCnf->pVarNums[Aig_ManCi(pAig , t2)->Id], 1);
      literals_arr[1] = toLitCond(pCnf->pVarNums[Aig_ManCi(pAig , t)->Id] + (pCnf->nVars), 0);
      literals_arr[2] = toLitCond(pCnf->pVarNums[Aig_ManCi(pAig , t)->Id] + 2*(pCnf->nVars), 1);
      literals_arr[3] = toLitCond(pCnf->pVarNums[Aig_ManCi(pAig , t2)->Id] + 2*(pCnf->nVars), 1);
      status = sat_solver_addclause(pSat, literals_arr, literals_arr + 4);
      assert( status );
    }
  }

  //output
  Aig_ManForEachCo( pAig, pObj, t ){
    literals_arr[0] = toLitCond(pCnf->pVarNums[pObj->Id], 0); //vA
    literals_arr[1] = toLitCond(pCnf->pVarNums[pObj->Id] + (pCnf->nVars), 0); //vB
    status = sat_solver_addclause(pSat, literals_arr, literals_arr + 2);
    assert( status );
    literals_arr[0] = toLitCond(pCnf->pVarNums[pObj->Id], 1);
    literals_arr[1] = toLitCond(pCnf->pVarNums[pObj->Id] + (pCnf->nVars), 1);
    status = sat_solver_addclause(pSat, literals_arr, literals_arr + 2);
    assert( status );
  }

  //checking symmetry between i,j by setting variable values of vH (unit assumptions)
  //also, solve the transitive relation

  //create array to store transitive relation, true: symmetric
  int n = Aig_ManCiNum(pAig);
  std::vector<std::vector<bool>> array(n, std::vector<bool>(n));
  // stack smashing detected ***: terminated
  for(int i=0; i<n; i++){
    array[i].resize(n);
  }

  for(t=0; t<n-1; t++){
    for(t2=t+1;t2<n; t2++){
      if(array[t][t2] == true){
        printf("%d %d\n", t, t2);
        continue;
      }
      //add literal assumptions and solve the sat
      literals_arr[0] = toLitCond(pCnf->pVarNums[Aig_ManCi(pAig , t) ->Id] + 2*(pCnf->nVars), 0); //vH
      literals_arr[1] = toLitCond(pCnf->pVarNums[Aig_ManCi(pAig , t2)->Id] + 2*(pCnf->nVars), 0); //vH
      status = sat_solver_solve(pSat, literals_arr, literals_arr + 2, (ABC_INT64_T)0, (ABC_INT64_T)0, (ABC_INT64_T)0, (ABC_INT64_T)0 );
      if(status == l_False){ // unsat(symmetric)
        array[t][t2] = true;
        printf("%d %d\n", t, t2);
      }
    }
    //transitive relation
    for(t2=t+1; t2<n-1; t2++){
      for(t3=t2+1; t3<n; t3++){
        if(array[t][t2]==true && array[t][t3]==true){
          array[t2][t3]=true;
        }
      }
    }
  }

  //free the allocated memory
  sat_solver_delete(pSat);
  Cnf_DataFree(pCnf);
  return;
}

int Lsv_CommandSymAll(Abc_Frame_t* pAbc, int argc, char** argv){
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
  if(!Abc_NtkIsStrash(pNtk)){
    Abc_Print(-1, "You should first 'strash' then call this function.\n");
  }
  Lsv_Sat_All(pNtk, argv[1]);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sym_all [-h] <output_pin> <input_pin1> <input_pin2>\n");
  Abc_Print(-2, "\t        Check the symmetry of the output with respect to two inputs by incremental SAT\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}