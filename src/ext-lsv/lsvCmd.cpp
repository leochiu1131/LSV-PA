#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "bdd/extrab/extraBdd.h"


static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimbdd(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimaig(Abc_Frame_t* pAbc, int argc, char** argv);
unsigned int simnode(Abc_Obj_t* anode, int* input);
int Lsv_Simbdd(Abc_Ntk_t* pNtk, char* input);
int Lsv_Simaig(Abc_Ntk_t* pNtk, int* input, int linenum);
void writedd(DdNode* dd, char* filename, DdManager* manager){
  FILE *outfile;
  outfile = fopen(filename, "w");
  DdNode **ddnodearray = (DdNode**)malloc(sizeof(DdNode*));
  ddnodearray[0] = dd;
  Cudd_DumpDot(manager, 1, ddnodearray, NULL, NULL, outfile);
  free(ddnodearray);
  fclose(outfile);
}

void printbinary(int num, int linenum){
  int output[32];
  for(int i=0;i<linenum;i++){
    if(num%2 == 1) output[i] = 1;
    else output[i] = 0;
    num /= 2;
  }
  for(int i=linenum-1;i>=0;i--){
    printf("%d", output[i]);
  }
}

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", Lsv_CommandSimbdd, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig", Lsv_CommandSimaig, 0);
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

int Lsv_CommandSimbdd(Abc_Frame_t* pAbc, int argc, char** argv){
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  // printf("argc = %d\n", argc);
  // printf("%s, %s", argv[0], argv[1]);
  if(argc!=2){
    printf("Wrong # of arguments\n");
    return 1;
  }
  else{
    Lsv_Simbdd(pNtk, argv[1]);
  }
  return 0;
}

int Lsv_Simbdd(Abc_Ntk_t* pNtk, char* input){
  Abc_Obj_t* pPo;
  int ithPo;
  Abc_NtkForEachPo(pNtk, pPo, ithPo){
    printf("%s: ", Abc_ObjName(pPo));
    Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo); 
    assert( Abc_NtkIsBddLogic(pRoot->pNtk) );
    DdManager * dd = (DdManager *)pRoot->pNtk->pManFunc;  
    DdNode* ddnode = (DdNode *)pRoot->pData;
    char** vNamesIn = (char**) Abc_NodeGetFaninNames(pRoot)->pArray;
    int n = Abc_NodeGetFaninNames(pRoot)->nSize;
    // printf("n: %d\n", n);
    int ithPi;
    Abc_Obj_t* pPi;
    Abc_NtkForEachPi(pNtk, pPi, ithPi){
      int ithvar = -1;
      for(int i=0;i < n;i++){
        if(strcmp(vNamesIn[i],Abc_ObjName(pPi)) == 0){
          ithvar = i;
          // printf("%d\n", ithvar);
        }
      }
      if(ithvar == -1){
        // printf("fail to find.\n");
        continue;
      }
      // printf("%s\n", Abc_ObjName(pPi));
      if(input[ithPi] == '0'){
        ddnode = Cudd_Cofactor(dd, ddnode, Cudd_Not(Cudd_bddIthVar(dd, ithvar)));
      }
      else if(input[ithPi] == '1'){
        ddnode = Cudd_Cofactor(dd, ddnode, Cudd_bddIthVar(dd, ithvar));
      }
    }
    // printf("%s\n", vNamesIn[0]);
    // printf("%s\n", vNamesIn[1]);
    // printf("%s\n", vNamesIn[2]);
    // printf("%s\n", vNamesIn[3]);

    // char* dotfile = "dd.dot";
    // char ddnum[100];
    // sprintf(ddnum, "%d", ithPo);
    // char* filename = strcat(ddnum, dotfile);
    // writedd(ddnode, filename, dd);
    if(ddnode == Cudd_ReadOne(dd)){
      // printf("%s: 1\n", vNamesIn[ithPo]);
      printf("1\n");
    }
    else if(ddnode == Cudd_Not(Cudd_ReadOne(dd))){
      // printf("%s: 1\n", vNamesIn[ithPo]);
      printf("0\n");
    }
    // else printf("fail\n");
  }
  return 0;
}


static int Lsv_CommandSimaig(Abc_Frame_t* pAbc, int argc, char** argv){
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  if(argc!=2){
    printf("Wrong # of arguments\n");
    return 1;
  }
  else{
    FILE * infile;
    char line[256];
    int value[256]={0};
    int linenum = 0;
    infile = fopen(argv[1], "r");
    if(infile == NULL){
      printf("Unable to open file.");
      return 1;
    }
    while(fgets(line, sizeof(line), infile)!=NULL){
      // printf("%s", line);
      linenum+=1;
      for(int i=0;line[i] == '0'||line[i] == '1';i++){
        value[i]*=2;
        if(line[i] == '1'){
          value[i]+=1;
        }
        else if(line[i] != '0') {
          // printf("Error");
          return 1;
        }
      }
      // printf("line done.");
    }
    // for(int i=0;value[i]!=0;i++){
    //   printf("%d\n", value[i]);
    // }
    Lsv_Simaig(pNtk, value, linenum);
  }
  return 0;
}

int Lsv_Simaig(Abc_Ntk_t* pNtk, int* input, int linenum){
  Abc_Obj_t* pPo;
  int ithPo;
  Abc_NtkForEachPo(pNtk, pPo, ithPo){
    printf("%s: ", Abc_ObjName(pPo));
    unsigned int povalue = simnode(Abc_ObjFanin0(pPo), input);
    if(Abc_ObjFaninC0(pPo)) povalue = ~povalue;
    printbinary(povalue, linenum);
    printf("\n");
  }
  return 0;
}

unsigned int simnode(Abc_Obj_t* anode, int* input){
  if(Abc_ObjIsPi(anode)){
    return input[Abc_ObjId(anode)-1];
  }
  else if(Abc_ObjIsNode(anode)){
    unsigned int fanin0 = simnode(Abc_ObjFanin0(anode), input);
    unsigned int fanin1 = simnode(Abc_ObjFanin1(anode), input);
    if(Abc_ObjFaninC0(anode)) fanin0 = ~fanin0;
    if(Abc_ObjFaninC1(anode)) fanin1 = ~fanin1;
    unsigned int nodevalue = fanin0 & fanin1;
    return nodevalue;
  }
}
