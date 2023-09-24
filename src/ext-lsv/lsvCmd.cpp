#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <cstring>
#include <stdio.h>

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
  int num_col = 1;//=0?

  char* buffer = (char*) malloc(num_row*sizeof(char));
  while (fgets(buffer, sizeof(buffer), fp)!=NULL){
    num_col++;
  }
  fclose(fp);

  //allocate memory
  int** data = (int**)malloc(total_num_nodes* sizeof(int*));
  for(int i=0; i<total_num_nodes; i++){ data[i] = (int*) malloc(num_col*sizeof(int)); }

  /*transpose the input for input simulation pattern: 
  input_data[i] contains a parallel pattern for i-th PI, which is the first n elements of Foreachnode...*/
  num_col = 0;
  fp = fopen(input_file, "r");
  while (fgets(buffer, sizeof(buffer), fp)!=NULL){
    for(int i=1; i < num_row+1; i++){
      data[i][num_col] = buffer[i-1] - '0';
    }
    num_col++;
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
    // printf("Internal index: %d, %s :",index,  Abc_ObjName(pObj));
    // for(int i=0; i<num_col; i++){
    //   printf("%d",data[index][i]);
    // }
    // printf("\n");
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

