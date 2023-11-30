#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "bdd/extrab/extraBdd.h"
#include <cstring>
#include "sat/cnf/cnf.h"
extern "C"{
  Aig_Man_t* Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
}

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimbdd(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimaig(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSymbdd(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSymsat(Abc_Frame_t* pAbc, int argc, char** argv);
unsigned long long int simnode(Abc_Obj_t* anode, unsigned long long int* input, unsigned long long int const1,
unsigned long long int* nodevalue, bool* nodedone);
int Lsv_Symbdd(Abc_Ntk_t* pNtk, char** argv);
int Lsv_Symsat(Abc_Ntk_t* pNtk, char** argv);
int Lsv_Simbdd(Abc_Ntk_t* pNtk, char* input);
int Lsv_Simaig(Abc_Ntk_t* pNtk, unsigned long long int* input, int linenum, unsigned long long int const1);
void writedd(DdNode* dd, char* filename, DdManager* manager){
  FILE *outfile;
  outfile = fopen(filename, "w");
  DdNode **ddnodearray = (DdNode**)malloc(sizeof(DdNode*));
  ddnodearray[0] = dd;
  Cudd_DumpDot(manager, 1, ddnodearray, NULL, NULL, outfile);
  free(ddnodearray);
  fclose(outfile);
}

void printbinary(unsigned long long int num, int linenum){
  int output[100];
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
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_bdd", Lsv_CommandSymbdd, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_sat", Lsv_CommandSymsat, 0);
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
  Lsv_Symbdd(pNtk, argv);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sym_bdd [-h]\n");
  Abc_Print(-2, "\t        check symmetry by bdd\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

int Lsv_CommandSymbdd(Abc_Frame_t* pAbc, int argc, char** argv){
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
  Lsv_Symbdd(pNtk, argv);
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

int Lsv_Symbdd(Abc_Ntk_t* pNtk, char** argv){
  int k = atoi(argv[1]);
  // printf("%d\n", k);
  Abc_Obj_t* pRoot = Abc_ObjFanin0(Abc_NtkPo(pNtk, k)); 
  assert( Abc_NtkIsBddLogic(pRoot->pNtk) );
  DdManager * dd = (DdManager *)pRoot->pNtk->pManFunc;  
  DdNode* ddnode = (DdNode *)pRoot->pData;
  Cudd_Ref(ddnode);
  int xi, xj;
  xi = atoi(argv[2]);
  xj = atoi(argv[3]);
  if(xi == xj){
    printf("symmetric\n");
    return 0;
  }
  Abc_Obj_t* Pi1 = Abc_NtkPi(pNtk, xi);
  Abc_Obj_t* Pi2 = Abc_NtkPi(pNtk, xj);
  char** vNamesIn = (char**) Abc_NodeGetFaninNames(pRoot)->pArray;
  int n = Abc_NodeGetFaninNames(pRoot)->nSize;
  // printf("%d\n", n);
  int index1=-1, index2=-1;
  for(int i=0;i<n;i++){
    if(strcmp(vNamesIn[i],Abc_ObjName(Pi1)) == 0){
          index1 = i;
    }
    else if(strcmp(vNamesIn[i],Abc_ObjName(Pi2)) == 0){
          index2 = i;
    }
  }
  if(index1 == -1 && index2 == -1){
    printf("symmetric\n");
  }
  else if(index2 == -1){
    DdNode* var1 = Cudd_bddIthVar(dd, index1);
    Cudd_Ref(var1);
    DdNode* temp1 = Cudd_Cofactor(dd, ddnode, var1);
    Cudd_Ref(temp1);
    DdNode* temp2 = Cudd_Cofactor(dd, ddnode, Cudd_Not(var1));
    Cudd_Ref(temp2);
    if(temp1 == temp2) printf("symmetric\n");
    else{
      printf("asymmetric\n");
      int pinum = Abc_NtkPiNum(pNtk);
      int* pattern1 = new int[pinum];
      int* pattern2 = new int[pinum];
      int ithPi;
      Abc_Obj_t* pPi;
      Abc_NtkForEachPi(pNtk, pPi, ithPi){
        int ithvar = -1;
        for(int i=0;i < n;i++){
          if(strcmp(vNamesIn[i],Abc_ObjName(pPi)) == 0){
            ithvar = i;
          }
        }
        if(ithvar == -1){
          pattern1[ithPi] = 0;
          pattern2[ithPi] = 0;
          continue;
        }
        else if(ithvar == index1){
          pattern1[ithPi] = 1;
          pattern2[ithPi] = 0;
        }
        else{
          DdNode* poscof1 = Cudd_Cofactor(dd, temp1, Cudd_bddIthVar(dd, ithvar));
          Cudd_Ref(poscof1);
          DdNode* poscof2 = Cudd_Cofactor(dd, temp2, Cudd_bddIthVar(dd, ithvar));
          Cudd_Ref(poscof2);
          if(poscof1 != poscof2){
            pattern1[ithPi] = 1;
            pattern2[ithPi] = 1;
            temp1 = poscof1;
            temp2 = poscof2;
          }
          else{
            pattern1[ithPi] = 0;
            pattern2[ithPi] = 0;
            temp1 = Cudd_Cofactor(dd, temp1, Cudd_Not(Cudd_bddIthVar(dd, ithvar)));
            temp2 = Cudd_Cofactor(dd, temp2, Cudd_Not(Cudd_bddIthVar(dd, ithvar)));
          }
          Cudd_RecursiveDeref(dd, poscof1);
          Cudd_RecursiveDeref(dd, poscof2);
        }
      }
      pattern1[xj] = 0;
      pattern2[xj] = 1;
      for(int i=0;i<pinum;i++){
        printf("%d", pattern1[i]);
      }
      printf("\n");
      for(int i=0;i<pinum;i++){
        printf("%d", pattern2[i]);
      }
      printf("\n");
      Cudd_RecursiveDeref(dd, var1);
      Cudd_RecursiveDeref(dd, temp1);
      Cudd_RecursiveDeref(dd, temp2);
    }
  }
  else if(index1 == -1){
    DdNode* var2 = Cudd_bddIthVar(dd, index2);
    Cudd_Ref(var2);
    DdNode* temp1 = Cudd_Cofactor(dd, ddnode, var2);
    Cudd_Ref(temp1);
    DdNode* temp2 = Cudd_Cofactor(dd, ddnode, Cudd_Not(var2));
    Cudd_Ref(temp2);
    if(temp1 == temp2) printf("symmetric\n");
    else{
      printf("asymmetric\n");
      int pinum = Abc_NtkPiNum(pNtk);
      int* pattern1 = new int[pinum];
      int* pattern2 = new int[pinum];
      int ithPi;
      Abc_Obj_t* pPi;
      Abc_NtkForEachPi(pNtk, pPi, ithPi){
        int ithvar = -1;
        for(int i=0;i < n;i++){
          if(strcmp(vNamesIn[i],Abc_ObjName(pPi)) == 0){
            ithvar = i;
          }
        }
        if(ithvar == -1){
          pattern1[ithPi] = 0;
          pattern2[ithPi] = 0;
          continue;
        }
        else if(ithvar == index2){
          pattern1[ithPi] = 1;
          pattern2[ithPi] = 0;
        }
        else{
          DdNode* poscof1 = Cudd_Cofactor(dd, temp1, Cudd_bddIthVar(dd, ithvar));
          Cudd_Ref(poscof1);
          DdNode* poscof2 = Cudd_Cofactor(dd, temp2, Cudd_bddIthVar(dd, ithvar));
          Cudd_Ref(poscof2);
          if(poscof1 != poscof2){
            pattern1[ithPi] = 1;
            pattern2[ithPi] = 1;
            temp1 = poscof1;
            temp2 = poscof2;
          }
          else{
            pattern1[ithPi] = 0;
            pattern2[ithPi] = 0;
            temp1 = Cudd_Cofactor(dd, temp1, Cudd_Not(Cudd_bddIthVar(dd, ithvar)));
            temp2 = Cudd_Cofactor(dd, temp2, Cudd_Not(Cudd_bddIthVar(dd, ithvar)));
          }
          Cudd_RecursiveDeref(dd, poscof1);
          Cudd_RecursiveDeref(dd, poscof2);
        }
      }
      pattern1[xi] = 0;
      pattern2[xi] = 1;
      for(int i=0;i<pinum;i++){
        printf("%d", pattern1[i]);
      }
      printf("\n");
      for(int i=0;i<pinum;i++){
        printf("%d", pattern2[i]);
      }
      printf("\n");
      Cudd_RecursiveDeref(dd, var2);
      Cudd_RecursiveDeref(dd, temp1);
      Cudd_RecursiveDeref(dd, temp2);
    }
  }
  else{
    DdNode* var1 = Cudd_bddIthVar(dd, index1);
    Cudd_Ref(var1);
    DdNode* var2 = Cudd_bddIthVar(dd, index2);
    Cudd_Ref(var2);
    DdNode* temp1 = Cudd_Cofactor(dd, ddnode, var1);
    Cudd_Ref(temp1);
    DdNode* temp2 = Cudd_Cofactor(dd, ddnode, var2);
    Cudd_Ref(temp2);
    temp1 = Cudd_Cofactor(dd, temp1, Cudd_Not(var2));
    Cudd_Ref(temp1);
    temp2 = Cudd_Cofactor(dd, temp2, Cudd_Not(var1));
    Cudd_Ref(temp2);
    if(temp1 == temp2) printf("symmetric\n");
    else{
      printf("asymmetric\n");
      int pinum = Abc_NtkPiNum(pNtk);
      int* pattern1 = new int[pinum];
      int* pattern2 = new int[pinum];
      int ithPi;
      Abc_Obj_t* pPi;
      Abc_NtkForEachPi(pNtk, pPi, ithPi){
        int ithvar = -1;
        for(int i=0;i < n;i++){
          if(strcmp(vNamesIn[i],Abc_ObjName(pPi)) == 0){
            ithvar = i;
          }
        }
        if(ithvar == -1){
          pattern1[ithPi] = 0;
          pattern2[ithPi] = 0;
          continue;
        }
        else if(ithvar == index1){
          pattern1[ithPi] = 1;
          pattern2[ithPi] = 0;
        }
        else if(ithvar == index2){
          pattern1[ithPi] = 0;
          pattern2[ithPi] = 1;
        }
        else{
          DdNode* poscof1 = Cudd_Cofactor(dd, temp1, Cudd_bddIthVar(dd, ithvar));
          Cudd_Ref(poscof1);
          DdNode* poscof2 = Cudd_Cofactor(dd, temp2, Cudd_bddIthVar(dd, ithvar));
          Cudd_Ref(poscof2);
          if(poscof1 != poscof2){
            pattern1[ithPi] = 1;
            pattern2[ithPi] = 1;
            temp1 = poscof1;
            temp2 = poscof2;
          }
          else{
            pattern1[ithPi] = 0;
            pattern2[ithPi] = 0;
            temp1 = Cudd_Cofactor(dd, temp1, Cudd_Not(Cudd_bddIthVar(dd, ithvar)));
            temp2 = Cudd_Cofactor(dd, temp2, Cudd_Not(Cudd_bddIthVar(dd, ithvar)));
          }
          Cudd_RecursiveDeref(dd, poscof1);
          Cudd_RecursiveDeref(dd, poscof2);
        }
      }
      for(int i=0;i<pinum;i++){
        printf("%d", pattern1[i]);
      }
      printf("\n");
      for(int i=0;i<pinum;i++){
        printf("%d", pattern2[i]);
      }
      printf("\n");
      Cudd_RecursiveDeref(dd, var1);
      Cudd_RecursiveDeref(dd, var2);
      Cudd_RecursiveDeref(dd, temp1);
      Cudd_RecursiveDeref(dd, temp2);
    }
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
    unsigned long long int value[256]={0};
    unsigned long long int const1 = 0;
    int linenum = 0;
    infile = fopen(argv[1], "r");
    if(infile == NULL){
      printf("Unable to open file.");
      return 1;
    }
    while(fgets(line, sizeof(line), infile)!=NULL){
      // printf("%s", line);
      linenum+=1;
      const1 = const1*2+1;
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
    Lsv_Simaig(pNtk, value, linenum, const1);
  }
  return 0;
}

int Lsv_Simaig(Abc_Ntk_t* pNtk, unsigned long long int* input, int linenum, unsigned long long int const1){
  Abc_Obj_t* pPo;
  int ithPo;
  unsigned long long int* nodevalue;
  nodevalue = new unsigned long long int[Abc_NtkObjNum(pNtk)];
  bool* nodedone;
  nodedone = new bool[Abc_NtkObjNum(pNtk)];
  for(int i=0;i<Abc_NtkObjNum(pNtk); i++){
    nodedone[i] = false;
  }
  Abc_NtkForEachPo(pNtk, pPo, ithPo){
    // printf("success\n");
    printf("%s: ", Abc_ObjName(pPo));
    unsigned long long int povalue = simnode(Abc_ObjFanin0(pPo), input, const1, nodevalue, nodedone);
    // printf("simdone.\n");
    if(Abc_ObjFaninC0(pPo)) povalue = ~povalue;
    printbinary(povalue, linenum);
    printf("\n");
  }
  return 0;
}

unsigned long long int simnode(Abc_Obj_t* anode, unsigned long long int* input, unsigned long long int const1, 
unsigned long long int* nodevalue, bool* nodedone){
  if(nodedone[Abc_ObjId(anode)]){
    return nodevalue[Abc_ObjId(anode)];
  }
  if(Abc_ObjIsPi(anode)){
    nodevalue[Abc_ObjId(anode)] = input[Abc_ObjId(anode)-1];
    nodedone[Abc_ObjId(anode)] = true;
    return input[Abc_ObjId(anode)-1];
  }
  else if(Abc_ObjIsNode(anode)){
    if(Abc_AigNodeIsConst(Abc_ObjFanin0(anode))){
      unsigned long long int fanin1 = simnode(Abc_ObjFanin1(anode), input, const1, nodevalue, nodedone);
      nodevalue[Abc_ObjId(anode)] = fanin1;
      nodedone[Abc_ObjId(anode)] = true;
      return fanin1;
    }
    else if(Abc_AigNodeIsConst(Abc_ObjFanin1(anode))){
      unsigned long long int fanin0 = simnode(Abc_ObjFanin0(anode), input, const1, nodevalue, nodedone);
      nodevalue[Abc_ObjId(anode)] = fanin0;
      nodedone[Abc_ObjId(anode)] = true;
      return fanin0;
    }
    unsigned long long int fanin0 = simnode(Abc_ObjFanin0(anode), input, const1, nodevalue, nodedone);
    unsigned long long int fanin1 = simnode(Abc_ObjFanin1(anode), input, const1, nodevalue, nodedone);
    if(Abc_ObjFaninC0(anode)) fanin0 = ~fanin0;
    if(Abc_ObjFaninC1(anode)) fanin1 = ~fanin1;
    unsigned long long int nodevaluex = fanin0 & fanin1;
    nodevalue[Abc_ObjId(anode)] = nodevaluex;
    nodedone[Abc_ObjId(anode)] = true;
    return nodevaluex;
  }
  return const1;
}

int Lsv_CommandSymsat(Abc_Frame_t* pAbc, int argc, char** argv){
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
  Lsv_Symsat(pNtk, argv);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_print_nodes [-h]\n");
  Abc_Print(-2, "\t        prints the nodes in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

int Lsv_Symsat(Abc_Ntk_t* pNtk, char** argv){
  int k = atoi(argv[1]);
  int i = atoi(argv[2]);
  int j = atoi(argv[3]);
  Abc_Obj_t* pRoot = Abc_ObjFanin0(Abc_NtkPo(pNtk, k));
  Abc_Ntk_t* newRoot = Abc_NtkCreateCone(pNtk, pRoot, Abc_ObjName(Abc_NtkPo(pNtk, k)), 1);
  Aig_Man_t* aig_circuit = Abc_NtkToDar(newRoot, 0, 1);
  sat_solver* solver = sat_solver_new();
  Cnf_Dat_t* cnf_1 = Cnf_Derive(aig_circuit, 1);
  Cnf_Dat_t* cnf_2 = Cnf_Derive(aig_circuit, 1);
  Cnf_DataWriteIntoSolverInt(solver, cnf_1, 1, 0);
  Cnf_DataLift(cnf_2, Abc_ObjFanin0(Abc_NtkPo(newRoot, 0))->Id);
  Cnf_DataWriteIntoSolverInt(solver, cnf_2, 1, 0);

  Abc_Obj_t* pObj;
  int ithPi;
  lit* var2lit_1 = new lit[Abc_NtkPiNum(pNtk)];
  lit* var2lit_2 = new lit[Abc_NtkPiNum(pNtk)];
  Abc_NtkForEachPi(pNtk, pObj, ithPi){
    var2lit_1[ithPi] = cnf_1->pVarNums[pObj->Id];
    var2lit_2[ithPi] = cnf_2->pVarNums[pObj->Id];
  }
  lit cls[2];
  cls[0] = toLitCond(var2lit_1[i], 0);
  cls[1] = toLitCond(var2lit_2[j], 1);
  sat_solver_addclause(solver, cls, cls+2);
  cls[0] = toLitCond(var2lit_1[i], 1);
  cls[1] = toLitCond(var2lit_2[j], 0);
  sat_solver_addclause(solver, cls, cls+2);
  cls[0] = toLitCond(var2lit_1[j], 0);
  cls[1] = toLitCond(var2lit_2[i], 1);
  sat_solver_addclause(solver, cls, cls+2);
  cls[0] = toLitCond(var2lit_1[j], 1);
  cls[1] = toLitCond(var2lit_2[i], 0);
  sat_solver_addclause(solver, cls, cls+2);
  Abc_NtkForEachPi(pNtk, pObj, ithPi){
    if(ithPi != i && ithPi != j){
      cls[0] = toLitCond(var2lit_1[ithPi], 0);
      cls[1] = toLitCond(var2lit_2[ithPi], 1);
      sat_solver_addclause(solver, cls, cls+2);
      cls[0] = toLitCond(var2lit_1[ithPi], 1);
      cls[1] = toLitCond(var2lit_2[ithPi], 0);
      sat_solver_addclause(solver, cls, cls+2);
    }
  }
  cls[0] = toLitCond(cnf_1->pVarNums[Abc_ObjFanin0(Abc_NtkPo(newRoot, 0))->Id], 0);
  cls[1] = toLitCond(cnf_2->pVarNums[Abc_ObjFanin0(Abc_NtkPo(newRoot, 0))->Id], 0);
  sat_solver_addclause(solver, cls, cls+2);
  cls[0] = toLitCond(cnf_1->pVarNums[Abc_ObjFanin0(Abc_NtkPo(newRoot, 0))->Id], 1);
  cls[1] = toLitCond(cnf_2->pVarNums[Abc_ObjFanin0(Abc_NtkPo(newRoot, 0))->Id], 1);
  sat_solver_addclause(solver, cls, cls+2);

  int sat_result = sat_solver_solve(solver, cls, cls, 0, 0, 0, 0);
  if(sat_result == -1) printf("symmetric\n");
  else{
    printf("asymmetric\n");
    int n = Abc_NtkPiNum(pNtk);
    int* pattern1 = new int[n];
    int* pattern2 = new int[n];
    for(int it=0;it<n;it++){
      pattern1[it] = sat_solver_var_value(solver, var2lit_1[it]);
      pattern2[it] = sat_solver_var_value(solver, var2lit_2[it]);
    }
    for(int it=0;it<n;it++){
      printf("%d", pattern1[it]);
    }
    printf("\n");
    for(int it=0;it<n;it++){
      printf("%d", pattern2[it]);
    }
    printf("\n");
  }
  return 0;
}