#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "bdd/extrab/extraBdd.h"
#include <vector>
// #include <iostream>
#include "sat/cnf/cnf.h"
extern "C"{
  Aig_Man_t* Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
}


static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimBDD(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSymBDD(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSymSAT(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", Lsv_CommandSimBDD, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig", Lsv_CommandSimAig, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_bdd", Lsv_CommandSymBDD, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_sat", Lsv_CommandSymSAT, 0);
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

bool isPartOf(char* w1, char* w2){
    int i = 0;
    int j = 0;
    while(w1[i] != '\0'){
        if(w1[i] == w2[j]){
            int init = i;
            while (w1[i] == w2[j] && w2[j] != '\0'){
                j++;
                i++;
            }

            if(w2[j] == '\0') return true;
            j = 0;
        }
        i++;
    }
    return false;
}

void Lsv_SimBDD(Abc_Ntk_t* pNtk, Abc_Obj_t* pRoot, char* in_pat){
  //
  Abc_Ntk_t* ddNtk = pRoot->pNtk;
  DdManager * temp_dd = (DdManager *)pRoot->pNtk->pManFunc;  
  DdManager * dd = temp_dd;
  DdNode* temp_ddnode = (DdNode *)pRoot->pData;
  DdNode* ddnode = temp_ddnode;
  //DdNode _ddnode = *ddnode;
  //ddnode = &_ddnode;
  Abc_Obj_t * pPi;
  int i;
  char** vNamesIn = (char**) Abc_NodeGetFaninNames(pRoot)->pArray;

  Abc_NtkForEachPi(pNtk, pPi, i){
    // cofactor in_pat
    //printf("%s - %s\n", Abc_ObjName(pRoot), Abc_ObjName(pPi));
    for(int ddi=0;ddi<Abc_NodeGetFaninNames(pRoot)->nSize;ddi++){
      if(isPartOf(Abc_ObjName(pPi), vNamesIn[ddi])){
        if(in_pat[i]=='1'){
          //printf("%d\n", 1);
          ddnode = Cudd_Cofactor(dd, ddnode, dd->vars[ddi]);
        }else{
          //printf("%d\n", 0);
          ddnode = Cudd_Cofactor(dd, ddnode, Cudd_Not(dd->vars[ddi]));
        }
        break;
        //printf("%d / %d : %s \n", ddi, Abc_NtkPiNum(ddNtk), vNamesIn[ddi]);
      }
      //else printf("%s\n", vNamesIn[ddi]);
    }
    
  }
  // print result
  int ans;
  if(!Cudd_IsConstant(ddnode)) Abc_Print(-1, "Something Wrong.\n");
  if(Cudd_IsComplement(ddnode))
    ans = 0;
  else
    ans = 1;
  //printf("%s: %d\n\n", Abc_ObjName(pRoot), ans);
  printf("%d\n", ans);
}

int Lsv_CommandSimBDD(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  //Abc_Ntk_t* ppNtk = Abc_FrameReadNtk(pAbc);
  //Abc_Ntk_t* pNtk = Abc_NtkDup(ppNtk);
  int c;
  char* in_pat;

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

  in_pat = argv[1];
  int ithPo;
  Abc_Obj_t * pPo;
  Abc_NtkForEachPo(pNtk, pPo, ithPo) {
    printf("%s: ", Abc_ObjName(pPo));
    Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo); 
    assert( Abc_NtkIsBddLogic(pRoot->pNtk) );
    //DdManager * dd = (DdManager *)pRoot->pNtk->pManFunc;  
    //DdNode* ddnode = (DdNode *)pRoot->pData;
    Lsv_SimBDD(pNtk, pRoot, in_pat);
  }

  //Abc_NtkDelete(ppNtk);
  
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sim_bdd [-h]\n");
  Abc_Print(-2, "\t        simulate BDD\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

unsigned long bi2int(char* tar){
  int i=0, temp;
  unsigned long ans = 0;
  while(tar[i]!='\0'){
    ans<<=1;
    temp = tar[i] - '0';
    if (temp) ans+=1;
    i+=1;
  }
  return ans;
}

char* int2bi(unsigned long tar, int num){
  char *ans = (char*)malloc(num + 1);
  for (int i = num - 1; i >= 0; i--) {
    ans[i] = (tar & 1) + '0';
    tar >>= 1;
  }
  ans[num] = '\0';
  return ans;
}

int pat_len(char* tar){
  int i=0;
  while(tar[i]!='\0'){
    i+=1;
  }
  return i;
}


int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c;
  char ** in_pat;
  char* fname;

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

  fname = argv[1];
  FILE* fptr;
  if ((fptr = fopen(fname,"r")) == NULL){
    Abc_Print( -1, "Can not open file.\n" );
  }
  in_pat = (char**)malloc(sizeof(char*)*Abc_NtkPiNum(pNtk));
  for(int i=0;i<Abc_NtkPiNum(pNtk);i++){
    in_pat[i] = (char*)malloc(sizeof(char)*32);
    fscanf(fptr, "%s\n", in_pat[i]);
    //printf("%s\n", in_pat[i]);
  }
  fclose(fptr);

  int ithPo, ithNode, ithPi, p_len;
  p_len = pat_len(in_pat[0]);
  unsigned long temp_c0, temp_c1, Value0, Value1;
  Abc_Obj_t * pPo;
  Abc_Obj_t * pPi;
  Abc_Obj_t * pNode;
  Abc_NtkForEachPi(pNtk, pPi, ithPi){
    //printf("%s\n",Abc_ObjName(pPi));
    //int temp = in_pat[ithPi][0] - '0';
    pPi->pCopy = (Abc_Obj_t *)bi2int(in_pat[ithPi]);
  }
  Abc_NtkForEachPo(pNtk, pPo, ithPo) {
    Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo); 
    printf("%s: ", Abc_ObjName(pPo));
    Abc_NtkForEachNode( pRoot->pNtk, pNode, ithNode ){
      temp_c0 = ((int)Abc_ObjFaninC0(pNode))? (1<<p_len)-1:0;
      Value0 = ((unsigned long)(ABC_PTRINT_T)Abc_ObjFanin0(pNode)->pCopy) ^ temp_c0;
      temp_c1 = ((int)Abc_ObjFaninC1(pNode))? (1<<p_len)-1:0;
      Value1 = ((unsigned long)(ABC_PTRINT_T)Abc_ObjFanin1(pNode)->pCopy) ^ temp_c1;
      pNode->pCopy = (Abc_Obj_t *)(Value0 & Value1);
    }
    printf("%s\n", int2bi((unsigned long)(ABC_PTRINT_T)pRoot->pCopy, p_len));
  }
  
  return 0;


usage:
  Abc_Print(-2, "usage: lsv_sim_aig [-h]\n");
  Abc_Print(-2, "\t        simulate Aig\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}



void Lsv_SymBDD(Abc_Ntk_t* pNtk, Abc_Obj_t* pRoot, int in_i1, int in_i2){
  //
  //Abc_Ntk_t* ddNtk = pRoot->pNtk;
  DdManager * dd = (DdManager *)pRoot->pNtk->pManFunc;  
  DdNode* ddnode = (DdNode *)pRoot->pData;
  Cudd_Ref(ddnode);
  //DdNode _ddnode = *ddnode;
  //ddnode = &_ddnode;
  Abc_Obj_t * pPi;
  if (in_i1 == in_i2) {
    printf("symmetric\n");
    return;
  }

  char** vNamesIn = (char**) Abc_NodeGetFaninNames(pRoot)->pArray;
  int in_1 = -1, in_2 = -1;
  for (int i = 0; i < Abc_ObjFaninNum(pRoot); i++) {
    if (strcmp(vNamesIn[i], Abc_ObjName(Abc_NtkPi(pNtk, in_i1))) == 0){
      in_1 = i;
    }
    else if (strcmp(vNamesIn[i], Abc_ObjName(Abc_NtkPi(pNtk, in_i2))) == 0){
      in_2 = i;
    }
  }
  DdNode* node_1_n2, * node_n1_2;
  DdNode* node_temp;

  if (in_1 == -1 && in_2 == -1) {
    printf("symmetric\n");
    return;
  }
  else if (in_2 == -1) {
    node_1_n2 = Cudd_Cofactor(dd, ddnode, Cudd_bddIthVar(dd, in_1));
    Cudd_Ref(node_1_n2);
    node_n1_2 = Cudd_Cofactor(dd, ddnode, Cudd_Not(Cudd_bddIthVar(dd, in_1)));
    Cudd_Ref(node_n1_2);
  }
  else if (in_1 == -1) {
    node_1_n2 = Cudd_Cofactor(dd, ddnode, Cudd_Not(Cudd_bddIthVar(dd, in_2)));
    Cudd_Ref(node_1_n2);
    node_n1_2 = Cudd_Cofactor(dd, ddnode, Cudd_bddIthVar(dd, in_2));
    Cudd_Ref(node_n1_2);
  }
  else {
    node_temp = Cudd_Cofactor(dd, ddnode, Cudd_Not(Cudd_bddIthVar(dd, in_1)));
    Cudd_Ref(node_temp);
    node_n1_2 = Cudd_Cofactor(dd, node_temp, Cudd_bddIthVar(dd, in_2));
    Cudd_Ref(node_n1_2);
    Cudd_RecursiveDeref(dd, node_temp);
    node_temp = Cudd_Cofactor(dd, ddnode, Cudd_Not(Cudd_bddIthVar(dd, in_2)));
    Cudd_Ref(node_temp);
    node_1_n2 = Cudd_Cofactor(dd, node_temp, Cudd_bddIthVar(dd, in_1));
    Cudd_Ref(node_1_n2);
    Cudd_RecursiveDeref(dd, node_temp);
  }
  Cudd_RecursiveDeref(dd, ddnode);
  
  if (node_n1_2 == node_1_n2) {
    printf("symmetric\n");
  }
  else {
    DdNode* node_xor = Cudd_bddXor(dd, node_n1_2, node_1_n2);
    Cudd_Ref(node_xor);
    char* cube = new char[dd->size];
    Cudd_bddPickOneCube(dd, node_xor, cube);
    Cudd_RecursiveDeref(dd, node_xor);
    int i;
    Abc_Obj_t* pPi;
    std::vector<int> out(Abc_NtkPiNum(pNtk), 0);
    //printf("%s\n", cube);
    Abc_NtkForEachPi(pNtk, pPi, i) {
      char* piName = Abc_ObjName(pPi);
      //printf("%d %s\n", i, piName);
      for (int j = 0; j < Abc_ObjFaninNum(pRoot); j++) {
        if (strcmp(vNamesIn[j], piName) == 0) {
          out[i] = (cube[j]);
          //printf("cube %d, %d\n", j, (cube[j]));
          break;
        }
      }
    }
    delete cube;

    printf("asymmetric\n");
    out[in_i1] = 0;
    out[in_i2] = 1;
    for(int j=0;j<Abc_NtkPiNum(pNtk);j++){
      printf("%d", out[j]);
    }
    printf("\n");
    out[in_i1] = 1;
    out[in_i2] = 0;
    for(int j=0;j<Abc_NtkPiNum(pNtk);j++){
      printf("%d", out[j]);
    }
    printf("\n");

  }
  Cudd_RecursiveDeref(dd, node_1_n2);
  Cudd_RecursiveDeref(dd, node_n1_2);


  return;
}


int Lsv_CommandSymBDD(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  //Abc_Ntk_t* ppNtk = Abc_FrameReadNtk(pAbc);
  //Abc_Ntk_t* pNtk = Abc_NtkDup(ppNtk);
  int c;
  //char* in_pat;

  // Extra_UtilGetoptReset();
  // while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
  //   switch (c) {
  //     case 'h':
  //       goto usage;
  //     default:
  //       goto usage;
  //   }
  // }

  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }

  //in_pat = argv[1]; ///
  int out_i = atoi(argv[1]);
  int in_i1 = atoi(argv[2]);
  int in_i2 = atoi(argv[3]);
  // int ithPo;
  // Abc_NtkForEachPo(pNtk, pPo, ithPo) {
  //   printf("%s: ", Abc_ObjName(pPo));
  //   assert( Abc_NtkIsBddLogic(pRoot->pNtk) );
  //   //DdManager * dd = (DdManager *)pRoot->pNtk->pManFunc;  
  //   //DdNode* ddnode = (DdNode *)pRoot->pData;
  // }
  Abc_Obj_t * pPo = Abc_NtkPo(pNtk, out_i);
  Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo); 
  Lsv_SymBDD(pNtk, pRoot, in_i1, in_i2);

  //Abc_NtkDelete(ppNtk);
  
  return 0;



usage:
  Abc_Print(-2, "usage: lsv_sym_bdd [-h]\n");
  Abc_Print(-2, "\t        symmetric BDD\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}


void Lsv_SymSAT(Abc_Ntk_t* pNtk, Abc_Obj_t* pRoot, int in_i1, int in_i2) {
  //printf("hi0\n");
  Abc_Ntk_t* pNtkAig = Abc_NtkCreateCone(pNtk, pRoot, "wut", 1);
  Aig_Man_t* pAig = Abc_NtkToDar(pNtkAig, 0, 0);
  Cnf_Dat_t* pCnf = Cnf_Derive(pAig, 1);
  sat_solver* pSat = sat_solver_new();
  Cnf_DataWriteIntoSolverInt(pSat, pCnf, 1, 0);
  Cnf_DataLift(pCnf, pCnf->nVars);
  Cnf_DataWriteIntoSolverInt(pSat, pCnf, 1, 0);
  lit clause[2];
  int var_num = pCnf->nVars;

  Aig_Obj_t* pObj;
  int i_pi, va, vb, va_i1, va_i2;
  Aig_ManForEachCi(pAig, pObj, i_pi){
    va = pCnf->pVarNums[pObj->Id];
    vb = va - var_num;
    //printf("va vb :%d %d\n", va, vb);
    if(i_pi==in_i1){
      va_i1 = va;
      continue;
    }else if(i_pi==in_i2){
      va_i2 = va;
      continue;
    }else{
      clause[0] = toLitCond(va, 1);
      clause[1] = toLitCond(vb, 0);
      sat_solver_addclause(pSat, &clause[0], &clause[1]+1);
      clause[0] = toLitCond(va, 0);
      clause[1] = toLitCond(vb, 1);
      sat_solver_addclause(pSat, &clause[0], &clause[1]+1);
    }
  }
  clause[0] = toLitCond(va_i1, 1); // a_i1 = 0
  //clause[1] = toLitCond(va_i1, 1);
  sat_solver_addclause(pSat, &clause[0], &clause[1]);
  clause[0] = toLitCond(va_i2, 0); // a_i2 = 1
  //clause[1] = toLitCond(va_i2, 0);
  sat_solver_addclause(pSat, &clause[0], &clause[1]);

  clause[0] = toLitCond(va_i1 - var_num, 0); // b_i1 = 1
  //clause[1] = toLitCond(va_i1 - var_num, 0);
  sat_solver_addclause(pSat, &clause[0], &clause[1]);
  clause[0] = toLitCond(va_i2 - var_num, 1); // b_i2 = 0
  //clause[1] = toLitCond(va_i2 - var_num, 1);
  sat_solver_addclause(pSat, &clause[0], &clause[1]);

  pRoot = Abc_ObjFanin0(Abc_NtkPo(pNtkAig, 0));
  int va_k = pCnf->pVarNums[pRoot->Id];
  int vb_k = va_k - var_num;
  clause[0] = toLitCond(va_k, 0);
  clause[1] = toLitCond(vb_k, 0);
  sat_solver_addclause(pSat, &clause[0], &clause[1]+1);
  clause[0] = toLitCond(va_k, 1);
  clause[1] = toLitCond(vb_k, 1);
  sat_solver_addclause(pSat, &clause[0], &clause[1]+1);

  int status = sat_solver_solve(pSat, 0, 0, 0, 0, 0, 0);
  //printf("%d\n", status);

  if(status == l_False){
    printf("symmetric\n");
  }
  else{
    printf("asymmetric\n");
    Aig_ManForEachCi(pAig, pObj, i_pi){
      printf("%d", sat_solver_var_value(pSat, pCnf->pVarNums[pObj->Id]));
    }
    printf("\n");
    Aig_ManForEachCi(pAig, pObj, i_pi){
      printf("%d", sat_solver_var_value(pSat, pCnf->pVarNums[pObj->Id] - var_num));
    }
    printf("\n");
  }
  sat_solver_delete(pSat);
}

int Lsv_CommandSymSAT(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  //Abc_Ntk_t* ppNtk = Abc_FrameReadNtk(pAbc);
  //Abc_Ntk_t* pNtk = Abc_NtkDup(ppNtk);
  int c;
  //char* in_pat;

  // Extra_UtilGetoptReset();
  // while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
  //   switch (c) {
  //     case 'h':
  //       goto usage;
  //     default:
  //       goto usage;
  //   }
  // }
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }

  //in_pat = argv[1]; ///
  int out_i = atoi(argv[1]);
  int in_i1 = atoi(argv[2]);
  int in_i2 = atoi(argv[3]);
  // int ithPo;
  // Abc_NtkForEachPo(pNtk, pPo, ithPo) {
  //   printf("%s: ", Abc_ObjName(pPo));
  //   assert( Abc_NtkIsBddLogic(pRoot->pNtk) );
  //   //DdManager * dd = (DdManager *)pRoot->pNtk->pManFunc;  
  //   //DdNode* ddnode = (DdNode *)pRoot->pData;
  // }
  Abc_Obj_t * pPo = Abc_NtkPo(pNtk, out_i);
  Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo); 
  Lsv_SymSAT(pNtk, pRoot, in_i1, in_i2);

  //Abc_NtkDelete(ppNtk);
  
  return 0;



usage:
  Abc_Print(-2, "usage: lsv_sym_aig [-h]\n");
  Abc_Print(-2, "\t        symmetric Aig\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}


