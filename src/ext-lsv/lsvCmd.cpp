#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_Sim_BDD(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_Sim_AIG(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", Lsv_Sim_BDD, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig", Lsv_Sim_AIG, 0);
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
void Lsv_Print_Sim_BDD_Results(Abc_Ntk_t* pNtk, char* input_pattern) {
  //check network type
  //printf("Hello world! The type of the network is %d.\n", pNtk->ntkType);
  //Read input Pattern
  int input_num = Abc_NtkPiNum(pNtk);
  int* input_arr = (int*) malloc(input_num * sizeof(int));
  size_t input_length = strlen(input_pattern);
  for (int i=0; i<input_num; i++){
    if (i >= input_length) {
      input_arr[i] = 0;
    }
    else {
      input_arr[i] = input_pattern[i] - '0';
    }
  }
  char** PI_names = Abc_NtkCollectCioNames( pNtk, 0 );
  // Manipulate DdNode
  DdNode* cofactor;  
  DdManager* dd = (DdManager*) pNtk->pManFunc;
  Abc_Obj_t *pPo, *pFi;
  int i,j;
  //iterate each PO
  Abc_NtkForEachPo( pNtk, pPo, i )
  {
    printf("PO[%d] %s: " ,i,Abc_ObjName(pPo));
    Abc_Obj_t* PoFunc = Abc_ObjFanin0(pPo);
    cofactor = (DdNode *)PoFunc->pData;
    char** FaninNames = (char**) Abc_NodeGetFaninNames(PoFunc)->pArray;
    
    // iterate each FI
    Abc_ObjForEachFanin( PoFunc, pFi, j)
    {
      //printf("Fi[%d]%s: " ,j,Abc_ObjName(pFi));
      DdNode* FiDdNode = Cudd_bddIthVar(dd,j);
      for (int k=0 ; k<input_num; k++){
        if(strcmp(FaninNames[j], PI_names[k]) == 0){
          if (input_arr[k]==0){
            cofactor = Cudd_Cofactor(dd, cofactor, Cudd_Not(FiDdNode));
          }
          else{
            cofactor = Cudd_Cofactor(dd, cofactor, FiDdNode);
          }
        }
      }
    }
    DdNode* one = Cudd_ReadOne(dd);
    if(cofactor==one){
      printf("1\n");
    }
    else {
      printf("0\n");
    }
  }
  free(input_arr);

}
int Lsv_Sim_BDD(Abc_Frame_t* pAbc, int argc, char** argv) {
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
  Lsv_Print_Sim_BDD_Results(pNtk, argv[1]);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_print_nodes [-h] <input_pattern>\n");
  Abc_Print(-2, "\t        Simulate specific pattern in BDD\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

void Lsv_Print_Sim_AIG_Results(Abc_Ntk_t* pNtk, char* input_file) {
  //check network type
  //printf("Hello world! The type of the network is %d.\n", pNtk->ntkType);

  //Initialize
  int PI_num      = Abc_NtkPiNum(pNtk);
  int node_count  = Abc_NtkPiNum(pNtk)+Abc_NtkNodeNum(pNtk)+Abc_NtkPoNum(pNtk)+1;
  int num_inputlines = 0;
  char* buffer = (char*) malloc(PI_num*sizeof(char));

  // Read inputlines from the file
  FILE *inputFile;
  inputFile = fopen(input_file, "r"); // Replace with your input file name
  while (fgets(buffer, sizeof(buffer), inputFile)!=NULL) num_inputlines++;
  fclose(inputFile);
  //printf("Finsih read input lines!Total row = %d. Total node = %d.\n",num_inputlines,node_count);

  //allocate memory
  int row = 0;
  int** sim_pat_array = (int**)malloc(node_count * sizeof(int*));
  for (int i = 0; i < node_count; i++) sim_pat_array[i] = (int*)malloc((num_inputlines+1)*sizeof(int));

  // Read inputlines into sim_pat_array
  inputFile = fopen(input_file, "r");
  while (fgets(buffer, sizeof(buffer), inputFile)!=NULL){
    for(int i=1; i <= PI_num; i++) sim_pat_array[i][row] = buffer[i-1] - '0';
    row++;}
  fclose(inputFile);
    //printf("Finsih read sim pattern array!Total row = %d.\n",row);
    // Display the read input values
    //printf("Read input values:\n");
    /*for (int i = 1; i <= PI_num; i++) {
      for (int j = 0; j < num_inputlines; j++) {
        printf("Input PI %d, ith-bit %d: %d\n", i, j, sim_pat_array[i][j]);
      }
    }*/
    
    Abc_Obj_t* pObj;
    int j;
    Abc_NtkForEachObj( pNtk, pObj, j) //compute the value of each and node in AIG
    {
      if((Abc_ObjIsPi(pObj) || pObj->Type == ABC_OBJ_CONST1 || Abc_ObjIsPo(pObj)))
        continue;
      else{
        int cur_index = (pObj->Id);
        Abc_Obj_t* fanin0;
        Abc_Obj_t* fanin1;
        fanin0 = Abc_ObjFanin0(pObj);
        fanin1 = Abc_ObjFanin1(pObj);
        int fanin0_ID = (fanin0->Id);
        int fanin1_ID = (fanin1->Id);
        //printf("Index: %d, Fanin0 Index: %d,Fanin1 Index: %d.\n",cur_index,fanin0_ID,fanin1_ID);
        unsigned inv_fanin0 = pObj->fCompl0;
        unsigned inv_fanin1 = pObj->fCompl1;
        unsigned int fanin0_value;
        unsigned int fanin1_value;
        for(int i=0; i<num_inputlines; i++){
          if(inv_fanin0)
            fanin0_value = !sim_pat_array[fanin0_ID][i];
          else
            fanin0_value = sim_pat_array[fanin0_ID][i];
          if(inv_fanin1)
            fanin1_value = !sim_pat_array[fanin1_ID][i];
          else
            fanin1_value = sim_pat_array[fanin1_ID][i];
          sim_pat_array[cur_index][i] = fanin0_value && fanin1_value;
        }
      } 
    }
  Abc_NtkForEachPo( pNtk, pObj, j ){
    // output should only have one fanin
    int PO_index = (pObj->Id);
    Abc_Obj_t* fanin0;
    fanin0 = Abc_ObjFanin0(pObj);
    int fanin0_ID = (fanin0->Id);
    //printf("Index: %d, Fanin0 Index: %d.\n",PO_index,fanin0_ID);
    unsigned inv_fanin0 = pObj->fCompl0;
    unsigned int fanin0_value;
    for(int i=0; i<=num_inputlines; i++){
      if(inv_fanin0)
        fanin0_value = !sim_pat_array[fanin0_ID][i];
      else
        fanin0_value = sim_pat_array[fanin0_ID][i];
      sim_pat_array[PO_index][i] = fanin0_value;
    }
    printf("%s: ", Abc_ObjName(pObj));
    for(int i=0; i<num_inputlines; i++){
      printf("%d",sim_pat_array[PO_index][i]);
    }
    printf("\n");
  }
  // Free allocated memory
  for(int i=0; i<node_count; i++){
    free(sim_pat_array[i]); 
  }
  free(sim_pat_array);
  free(buffer);
}
int Lsv_Sim_AIG(Abc_Frame_t* pAbc, int argc, char** argv) {
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
  Lsv_Print_Sim_AIG_Results(pNtk, argv[1]);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_print_nodes [-h] <input_file>\n");
  Abc_Print(-2, "\t        Simulate specific pattern in AIG\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}