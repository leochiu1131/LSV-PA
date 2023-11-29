#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "bdd/cudd/cudd.h"
#include "bdd/cudd/cuddInt.h"
#include "sat/cnf/cnf.h"
extern "C"{
Aig_Man_t* Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
}
using namespace std;
static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimBdd(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSymBdd(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSymSat(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSymSatAll(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", Lsv_CommandSimBdd, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig", Lsv_CommandSimAig, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_bdd", Lsv_CommandSymBdd, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_sat", Lsv_CommandSymSat, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_all", Lsv_CommandSymSatAll, 0);
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


void Lsv_sim_bdd(Abc_Ntk_t* pNtk, char* pattern){
  
  // printf("Bdd Input: %s \n",pattern);

  if(!Abc_NtkIsBddLogic(pNtk)){
    fprintf(stderr, "The network isn't BDD.\n");
    return;
  }
  if(strlen(pattern)!=Abc_NtkPiNum(pNtk)){
    // fprintf(stderr,"%d",strlen(pattern));
    fprintf(stderr, "Length of pattern isn't valid.\nIt should be %d \n",Abc_NtkPiNum(pNtk));
    return;
  }
  int* p_array;
  p_array = (int*)malloc(strlen(pattern)*sizeof(int));
  if(p_array==NULL) {
    fprintf(stderr,"Malloc fails"); 
    return;
  }
  for(int i=0;i<strlen(pattern);i++){
    if(pattern[i]!='0'&&pattern[i]!='1'){
      fprintf(stderr, "Invalid pattern bit exists.\nOnly 0 and 1 are legal.\n");
      free(p_array);
      return;
    }
    p_array[i] = pattern[i] - '0';
  }
  // for(int i=0;i<strlen(pattern);i++){
  //   fprintf(stderr,"%d ",p_array[i]);
  // }
  Abc_Obj_t* pPo;
  int ithPo ;
  Abc_NtkForEachPo(pNtk, pPo, ithPo) {
    Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo); 
    assert( Abc_NtkIsBddLogic(pRoot->pNtk) );
    DdManager * dd = (DdManager *)pRoot->pNtk->pManFunc;  
    DdNode* ddnode = (DdNode *)pRoot->pData;

    DdNode* bdd_tmp = ddnode;
    int faninIdx=0;
    Abc_Obj_t* pFanin;
    Abc_ObjForEachFanin(pRoot, pFanin, faninIdx) {

      Abc_Obj_t* pPi;
      int ithPi;
      Abc_NtkForEachPi(pNtk, pPi, ithPi){
        if(Abc_ObjId(pPi) == Abc_ObjId(pFanin)) break;
      }
      DdNode* bdd_var = Cudd_bddIthVar(dd,faninIdx);
      if(p_array[ithPi]==1) bdd_tmp = Cudd_Cofactor(dd,bdd_tmp,bdd_var);
      else if(p_array[ithPi]==0) bdd_tmp = Cudd_Cofactor(dd,bdd_tmp,Cudd_Not(bdd_var));

    }
    fprintf(stdout,"%s: %d \n", Abc_ObjName(pPo), (bdd_tmp == Cudd_ReadOne(dd))? 1: 0);

  }
  free(p_array);

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

  Lsv_sim_bdd(pNtk,argv[1]);
  return 0;
  
  usage:
  Abc_Print(-2, "usage: lsv_print_nodes [-h]\n");
  Abc_Print(-2, "\t        prints the nodes in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}


void sim_aig(Abc_Ntk_t* pNtk,vector<int>& vInputs) {
  Abc_Obj_t *pObj, *pPi, *pPo, *pFanin;
  int ithObj, ithPi, ithPo, ithFanin;

  Abc_NtkForEachPi(pNtk, pPi, ithPi)
  {
    pPi->iTemp = vInputs[ithPi];
  }
  Abc_NtkForEachObj(pNtk, pObj, ithObj)
  {
    if (pObj->Type != 7) continue;
    vector<Abc_Obj_t *> fanin;
    Abc_ObjForEachFanin(pObj, pFanin, ithFanin)
    {
      fanin.push_back(pFanin);
    }
    int a,b;
    if(pObj->fCompl0) a =  ~(fanin[0]->iTemp);
    else a =  fanin[0]->iTemp;
    if(pObj->fCompl1) b =  ~(fanin[1]->iTemp);
    else b =  fanin[1]->iTemp;
    
    pObj->iTemp = a&b;
  }

  Abc_NtkForEachPo(pNtk, pPo, ithPo)
  {
    Abc_Obj_t *fanin;
    Abc_ObjForEachFanin(pPo, pFanin, ithFanin)
    {
      fanin = pFanin;
    }
    if(pPo->fCompl0) pPo->iTemp = ~fanin->iTemp;
    else pPo->iTemp = fanin->iTemp;
  }

  return;
}

void Lsv_parallel_sim_aig(Abc_Ntk_t* pNtk, char* input_file){
  
  ifstream inputFile;
  string line;
  inputFile.open(input_file);
  int cnt = 0, remained,ithPo;
  Abc_Obj_t* pPo;
  vector<int> vInputs;
  vector<string> vOutputs(Abc_NtkPoNum(pNtk));

  if(!inputFile.is_open()){
    fprintf(stderr,"Can't find or open the file.\n");
    return;
  }
  vInputs = vector<int>(Abc_NtkPiNum(pNtk));
  while(inputFile >> line){
    if(line.size()!=Abc_NtkPiNum(pNtk)){
      fprintf(stderr,"Bits of input pattern is not consistent with network.\n");
      return;
    }
    for (size_t i = 0; i < Abc_NtkPiNum(pNtk); ++i)
    {
      if (line[i] == '0')
        vInputs[i] = vInputs[i] << 1;
      else if (line[i] == '1')
      {
        vInputs[i] = vInputs[i] << 1;
        vInputs[i] += 1;
      }
      else
      {
        fprintf(stderr,"Only 0 and 1 are legal.\n");
        vInputs.clear();
        return;
      }
    }
    cnt++;
    if(cnt==32){
      cnt=0;
      sim_aig(pNtk,vInputs);
      vInputs.clear();
      vInputs = vector<int>(Abc_NtkPiNum(pNtk));
      Abc_NtkForEachPo(pNtk, pPo, ithPo)
      {
        for (int i = sizeof(int) * 8 - 1; i >= 0; i--) //byte*8
        {
          if(((pPo->iTemp >> i) & 1)==1){
            vOutputs[ithPo] += "1";
          }
          else{
            vOutputs[ithPo] += "0";
          }
        }
      }
    }
  }
  remained = cnt;
  while(cnt!=32){
    for (size_t i = 0; i < Abc_NtkPiNum(pNtk); ++i)
    {
      vInputs[i] = vInputs[i] << 1;
    }
    cnt++;
  }
  sim_aig(pNtk,vInputs);
  Abc_NtkForEachPo(pNtk, pPo, ithPo)
  {
    for (int i = sizeof(int) * 8 - 1; i >= sizeof(int) * 8 - remained; i--)
    {
      if (((pPo->iTemp >> i) & 1) == 1)
      {
        vOutputs[ithPo] += "1";
      }
      else
      {
        vOutputs[ithPo] += "0";
      }
    }
  }

  Abc_NtkForEachPo(pNtk, pPo, ithPo)
  {
    cout << Abc_ObjName(pPo) << ": " << vOutputs[ithPo] << endl;
    // fprintf(stdout,"%s: %s\n",Abc_ObjName(pPo),vOutputs[ithPo]);
  }
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

  Lsv_parallel_sim_aig(pNtk,argv[1]);
  return 0;

  usage:
  Abc_Print(-2, "usage: lsv_print_nodes [-h]\n");
  Abc_Print(-2, "\t        prints the nodes in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}


bool find_counter_ex(DdManager * dd, DdNode* ddnode,string counter_ex, int ithVar){
  if(ddnode == Cudd_ReadOne(dd) || ddnode == Cudd_Not(Cudd_ReadZero(dd))){
    return true;
  }
  if(ddnode == Cudd_ReadZero(dd) || ddnode == Cudd_Not(DD_ONE(dd))){
    return false;
  }

  DdNode* var = Cudd_bddIthVar(dd,ithVar);
  DdNode* node_co = Cudd_Cofactor(dd, ddnode, var);
  Cudd_Ref(node_co);
  if(find_counter_ex(dd,node_co,counter_ex,ithVar+1)){
    counter_ex[ithVar] = '1';
    Cudd_RecursiveDeref(dd,node_co);
    return true;
  }
  Cudd_RecursiveDeref(dd,node_co);
  node_co = Cudd_Cofactor(dd, ddnode, Cudd_Not(var));
  Cudd_Ref(node_co);
  // Cudd_Ref(Cudd_Not(var));
  if(find_counter_ex(dd,node_co,counter_ex,ithVar+1)){
    counter_ex[ithVar] = '0';
    Cudd_RecursiveDeref(dd,node_co);
    return true;
  }
  Cudd_RecursiveDeref(dd,node_co);
  return false;
  

}

void Lsv_sym_bdd(Abc_Ntk_t* pNtk, char* k, char* i, char* j){
  if(!Abc_NtkIsBddLogic(pNtk)){
    fprintf(stderr, "The network isn't BDD.\n");
    return;
  }
  int int_k = atoi(k);
  int int_i = atoi(i);
  int int_j = atoi(j);
  if (int_i >= Abc_NtkPiNum(pNtk) || int_j >= Abc_NtkPiNum(pNtk)){
    // fprintf(stderr, "i:%d  j:%d.\n",int_i, int_j);
    fprintf(stderr, "Invalid input index.\n");
    return;
  }
  if(int_k >= Abc_NtkPoNum(pNtk)){
    fprintf(stderr, "k:%d.\n",int_k);
    fprintf(stderr, "Invalid output index.\n");
    return;
  }
  Abc_Obj_t* pPo, pPi;
  int ithPo ,ithPi;
  int bdd_ith = -1,bdd_jth = -1;
  DdNode* xor_node,*new_cube = nullptr ,*new_cube2 = nullptr, *tmp, *var1, *var2;
  
  pPo = Abc_NtkPo(pNtk, int_k);
  Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo); 
  assert( Abc_NtkIsBddLogic(pRoot->pNtk) );
  DdManager * dd = (DdManager *)pRoot->pNtk->pManFunc;  
  DdNode* ddnode = (DdNode *)pRoot->pData;
  
  char** bdd_inputs = (char**)Abc_NodeGetFaninNames(pRoot)->pArray;
  
  for(int i = 0; i < Abc_ObjFaninNum(pRoot); i++){
    // cout << Abc_ObjName(Abc_NtkPi(pNtk, int_i)) << "       " << bdd_inputs[i]<<endl;
    if(strcmp(Abc_ObjName(Abc_NtkPi(pNtk, int_i)) , bdd_inputs[i])==0) bdd_ith = i;
    if(strcmp(Abc_ObjName(Abc_NtkPi(pNtk, int_j)) , bdd_inputs[i])==0) bdd_jth = i;
  }

  if (bdd_ith == -1 && bdd_jth == -1) {  // symmetric
    cout << "symmetric" <<endl;
    return;
  }
  else if ((bdd_ith == -1) ^ (bdd_jth == -1))  //asymmetric
  {
    if (bdd_ith == -1) {
      xor_node = Cudd_bddBooleanDiff(dd, ddnode, bdd_jth);
      Cudd_Ref(xor_node);
    } else {
      xor_node = Cudd_bddBooleanDiff(dd, ddnode, bdd_ith);
      Cudd_Ref(xor_node);
    }
  }
  else{
    var1 = Cudd_Not(Cudd_bddIthVar(dd,bdd_ith));
    var2 = Cudd_bddIthVar(dd,bdd_jth);

    Cudd_Ref(var1);
    Cudd_Ref(var2);
    // DdNode* new_cube = Cudd_bddAnd(dd,cube,var1);
    tmp = Cudd_Cofactor(dd,ddnode,var1);
    Cudd_Ref(tmp);
    new_cube = Cudd_Cofactor(dd,tmp,var2);
    Cudd_Ref(new_cube);
    Cudd_RecursiveDeref(dd, tmp);
    Cudd_RecursiveDeref(dd, var1);
    Cudd_RecursiveDeref(dd, var2);
    //10
    var1 = Cudd_bddIthVar(dd,bdd_ith);
    var2 = Cudd_Not(Cudd_bddIthVar(dd,bdd_jth));
    Cudd_Ref(var1);
    Cudd_Ref(var2);
    tmp = Cudd_Cofactor(dd,ddnode,var1);
    Cudd_Ref(tmp);
    new_cube2 = Cudd_Cofactor(dd,tmp,var2);
    Cudd_Ref(new_cube2);
    Cudd_RecursiveDeref(dd, tmp);
    Cudd_RecursiveDeref(dd, var1);
    Cudd_RecursiveDeref(dd, var2);

    //xor 10 and 01
    xor_node = Cudd_bddXor(dd,new_cube,new_cube2);
    Cudd_Ref(xor_node);
    if(xor_node == Cudd_Not(DD_ONE(dd))||xor_node == DD_ZERO(dd)){
      cout << "symmetric" << endl;
      Cudd_RecursiveDeref(dd, new_cube);
      Cudd_RecursiveDeref(dd, new_cube2);
      Cudd_RecursiveDeref(dd, xor_node);
      return;
    }
  }
  
  //asymmetric result
  cout << "asymmetric" << endl;
  string counter_ex = "";
  string abc_counter_ex = "";
  for (int i = 0; i < Abc_ObjFaninNum(pRoot); i++) counter_ex += "0";
  for (int i = 0; i < Abc_NtkPiNum(pNtk); i++) abc_counter_ex += "0";
  if(!find_counter_ex(dd,xor_node,counter_ex,0)){ 
    fprintf(stderr, "Can't find vector (should debug)\n");
    return;
  }
  for(int i = 0; i< Abc_ObjFaninNum(pRoot); i++){
    for(int j = 0; j < Abc_NtkPiNum(pNtk); j++){
      if(bdd_inputs[i]==Abc_ObjName(Abc_NtkPi(pNtk, j))){
        abc_counter_ex[j] = counter_ex[i];
      }
    }
  }
  abc_counter_ex[int_i] = '1';
  abc_counter_ex[int_j] = '0';
  cout << abc_counter_ex << endl;
  abc_counter_ex[int_i] = '0';
  abc_counter_ex[int_j] = '1';
  cout << abc_counter_ex << endl;
  if(new_cube) Cudd_RecursiveDeref(dd, new_cube);
  if(new_cube2) Cudd_RecursiveDeref(dd, new_cube2);
  Cudd_RecursiveDeref(dd, xor_node);
  
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
  // fprintf(stderr,"k:%s i:%s j:%s",argv[1],argv[2],argv[3]);
  Lsv_sym_bdd(pNtk,argv[1],argv[2],argv[3]);
  return 0;

  usage:
  Abc_Print(-2, "usage: lsv_print_nodes [-h]\n");
  Abc_Print(-2, "\t        prints the nodes in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}


void Lsv_sym_sat(Abc_Ntk_t* pNtk, char* k, char* i, char* j){
  if(!Abc_NtkIsStrash(pNtk)){
    fprintf(stderr, "The network isn't Aig.\n");
    return;
  }
  int int_k = atoi(k);
  int int_i = atoi(i);
  int int_j = atoi(j);
  if (int_i >= Abc_NtkPiNum(pNtk) || int_j >= Abc_NtkPiNum(pNtk)){
    fprintf(stderr, "Invalid input index.\n");
    return;
  }
  if(int_k >= Abc_NtkPoNum(pNtk)){
    fprintf(stderr, "Invalid output index.\n");
    return;
  }

  Aig_Obj_t* pObj;
  int ith;
  lit Lits[2];
  int sat_judge;

  Abc_Obj_t* kth_output = Abc_NtkPo(pNtk,int_k);
  //(1) Use Abc NtkCreateCone to extract the cone of yk
  Abc_Ntk_t* pFanin_cone_ntk = Abc_NtkCreateCone(pNtk,Abc_ObjFanin0(kth_output),Abc_ObjName(kth_output),1); 
  //(2) Use Abc NtkToDar to derive a corresponding AIG circuit
  Aig_Man_t* pAig_ntk = Abc_NtkToDar(pFanin_cone_ntk,0,1);
  //(3) Use sat solver new to initialize an SAT solver
  sat_solver* pSat_sovler = sat_solver_new();
  //(4) Use Cnf Derive to obtain the corresponding CNF formula CA, which
  //depends on variables v1, . . . , vn
  Cnf_Dat_t* pCnf = Cnf_Derive(pAig_ntk,1);
  //(5) Use Cnf DataWriteIntoSolverInt to add the CNF into the SAT
  //solver(
  Cnf_DataWriteIntoSolverInt(pSat_sovler,pCnf,1,0);
  //(6) Use Cnf DataLift to create another CNF formula CB that depends
  //on different input variables vn+1, . . . , v2n. Again, add the CNF into
  //the SAT solver.
  Cnf_DataLift(pCnf, pCnf -> nVars);
  Cnf_DataWriteIntoSolverInt(pSat_sovler,pCnf,2,0);
  Cnf_DataLift(pCnf, -pCnf -> nVars);

  //(7) For each input xt of the circuit, find its corresponding CNF variables
  //vA(t) in CA and vB (t) in CB . Set vA(t) = vB (t) ∀t ̸ ∈ {i, j}, and set
  //vA(i) = vB (j), vA(j) = vB (i). This step can be done by adding
  //corresponding clauses to the SAT solver
  Aig_ManForEachCi(pAig_ntk,pObj,ith){
    if(ith == int_i || ith == int_j){
      //i-th or j-th in c1 and c2 are different
      Lits[0] = toLitCond( pCnf->pVarNums[pObj->Id], 0 ); 
      Lits[1] = toLitCond( pCnf->pVarNums[pObj->Id] + pCnf -> nVars, 0 );
      sat_judge = sat_solver_addclause( pSat_sovler, Lits, Lits + 2 ) ;
      assert(sat_judge);

      Lits[0] = toLitCond( pCnf->pVarNums[pObj->Id], 1 ); //1 means complement 
      Lits[1] = toLitCond( pCnf->pVarNums[pObj->Id] + pCnf -> nVars, 1 );
      sat_judge = sat_solver_addclause( pSat_sovler, Lits, Lits + 2 ) ;
      assert(sat_judge);
    }
    else {
      //others (not i-th and j-th) should be the same for c1 and c2
      Lits[0] = toLitCond( pCnf->pVarNums[pObj->Id], 0 );
      Lits[1] = toLitCond( pCnf->pVarNums[pObj->Id] + pCnf -> nVars, 1 );
      sat_judge = sat_solver_addclause( pSat_sovler, Lits, Lits + 2 ) ;
      assert(sat_judge);

      Lits[0] = toLitCond( pCnf->pVarNums[pObj->Id], 1 );
      Lits[1] = toLitCond( pCnf->pVarNums[pObj->Id] + pCnf -> nVars, 0 );
      sat_judge = sat_solver_addclause( pSat_sovler, Lits, Lits + 2 ) ;
      assert(sat_judge);
    }
  }
  // {i-th,j-th} = {{10},{01}};
  Lits[0] = toLitCond( pCnf->pVarNums[Aig_ManCi(pAig_ntk,int_i)->Id], 0 );
  Lits[1] = toLitCond( pCnf->pVarNums[Aig_ManCi(pAig_ntk,int_j)->Id], 0 );
  sat_judge = sat_solver_addclause( pSat_sovler, Lits, Lits + 2 ) ;
  assert(sat_judge);

  Lits[0] = toLitCond( pCnf->pVarNums[Aig_ManCi(pAig_ntk,int_i)->Id], 1 );
  Lits[1] = toLitCond( pCnf->pVarNums[Aig_ManCi(pAig_ntk,int_j)->Id], 1 );
  sat_judge = sat_solver_addclause( pSat_sovler, Lits, Lits + 2 ) ;
  assert(sat_judge);
  //same for c2
  Lits[0] = toLitCond( pCnf->pVarNums[Aig_ManCi(pAig_ntk,int_i)->Id] + pCnf -> nVars, 0 );
  Lits[1] = toLitCond( pCnf->pVarNums[Aig_ManCi(pAig_ntk,int_j)->Id] + pCnf -> nVars, 0 );
  sat_judge = sat_solver_addclause( pSat_sovler, Lits, Lits + 2 ) ;
  assert(sat_judge);

  Lits[0] = toLitCond( pCnf->pVarNums[Aig_ManCi(pAig_ntk,int_i)->Id] + pCnf -> nVars, 1 );
  Lits[1] = toLitCond( pCnf->pVarNums[Aig_ManCi(pAig_ntk,int_j)->Id] + pCnf -> nVars, 1 );
  sat_judge = sat_solver_addclause( pSat_sovler, Lits, Lits + 2 ) ;
  assert(sat_judge);
  //(8) Use sat solver solve to solve the SAT problem. Note that yk is
  // symmetric in xi and xj if and only if vA(yk)⊕vB (yk) is unsatisfiable,
  // where vA(yk) and vB (yk) are the CNF variables in CA and CB that
  // corresponds to yk.

  // xor outputs, vA(yk)⊕vB (yk)  =>  10/01
  Aig_ManForEachCo(pAig_ntk,pObj,ith){
    Lits[0] = toLitCond( pCnf->pVarNums[pObj->Id] , 0 );
    Lits[1] = toLitCond( pCnf->pVarNums[pObj->Id] + pCnf -> nVars, 0 );
    sat_judge = sat_solver_addclause( pSat_sovler, Lits, Lits + 2 ) ;
    assert(sat_judge);

    Lits[0] = toLitCond( pCnf->pVarNums[pObj->Id] , 1 );
    Lits[1] = toLitCond( pCnf->pVarNums[pObj->Id] + pCnf -> nVars, 1 );
    sat_judge = sat_solver_addclause( pSat_sovler, Lits, Lits + 2 ) ;
    assert(sat_judge);
  }
  int sat_result = sat_solver_solve(pSat_sovler,NULL,NULL,0,0,0,0);
  if(sat_result == l_False){
    cout << "symmetric" << endl;
    return;
  }
  else if(sat_result == l_True){
    cout << "asymmetric" << endl;
    //(9) If yk is asymmetric in xi and xj , use sat solver var value to
    // obtain the satisfying assignment, which can be used to derive the
    // counterexample.
    Aig_ManForEachCi(pAig_ntk,pObj,ith){
      int val = sat_solver_var_value(pSat_sovler,pCnf->pVarNums[pObj->Id]);
      if(val){
        cout << "1";
      }
      else{
        cout << "0";
      }
    }
    cout << endl;
    Aig_ManForEachCi(pAig_ntk,pObj,ith){
      int val = sat_solver_var_value(pSat_sovler,pCnf->pVarNums[pObj->Id] + pCnf->nVars);
      if(val){
        cout << "1";
      }
      else{
        cout << "0";
      }
    }
    cout << endl;
    return;
  }
  else{
    cout << "wrong! " << endl;
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

  Lsv_sym_sat(pNtk,argv[1],argv[2],argv[3]);
  return 0;

  usage:
  Abc_Print(-2, "usage: lsv_print_nodes [-h]\n");
  Abc_Print(-2, "\t        prints the nodes in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}


void Lsv_sym_sat_all(Abc_Ntk_t* pNtk, char* k){
  if(!Abc_NtkIsStrash(pNtk)){
    fprintf(stderr, "The network isn't Aig.\n");
    return;
  }
  int int_k = atoi(k);

  if(int_k >= Abc_NtkPoNum(pNtk)){
    fprintf(stderr, "Invalid output index.\n");
    return;
  }

  Aig_Obj_t* pObj;
  int ith;
  lit Lits[4];
  int sat_judge;

  Abc_Obj_t* kth_output = Abc_NtkPo(pNtk,int_k);
  //(1) Use Abc NtkCreateCone to extract the cone of yk
  Abc_Ntk_t* pFanin_cone_ntk = Abc_NtkCreateCone(pNtk,Abc_ObjFanin0(kth_output),Abc_ObjName(kth_output),1); 
  //(2) Use Abc NtkToDar to derive a corresponding AIG circuit
  Aig_Man_t* pAig_ntk = Abc_NtkToDar(pFanin_cone_ntk,0,1);
  //(3) Use sat solver new to initialize an SAT solver
  sat_solver* pSat_sovler = sat_solver_new();
  //(4) Use Cnf Derive to obtain the corresponding CNF formula CA, which
  //depends on variables v1, . . . , vn
  Cnf_Dat_t* pCnf = Cnf_Derive(pAig_ntk,1);
  //(5) Use Cnf DataWriteIntoSolverInt to add the CNF into the SAT
  //solver(
  Cnf_DataWriteIntoSolverInt(pSat_sovler,pCnf,1,0);
  //(6) Use Cnf DataLift to create another CNF formula CB that depends
  //on different input variables vn+1, . . . , v2n. Again, add the CNF into
  //the SAT solver.
  Cnf_DataLift(pCnf, pCnf -> nVars);
  Cnf_DataWriteIntoSolverInt(pSat_sovler,pCnf,2,0);
  Cnf_DataLift(pCnf, pCnf -> nVars);
  Cnf_DataWriteIntoSolverInt(pSat_sovler,pCnf,3,0);
  Cnf_DataLift(pCnf, -2 * pCnf -> nVars);


  Aig_ManForEachCi(pAig_ntk,pObj,ith){
    //(b)(vA(t) = vB (t)) + vH (t) 
    Lits[0] = toLitCond( pCnf->pVarNums[pObj->Id], 0 ); 
    Lits[1] = toLitCond( pCnf->pVarNums[pObj->Id] + pCnf -> nVars, 1 );
    Lits[2] = toLitCond( pCnf->pVarNums[pObj->Id] + 2 * pCnf -> nVars, 0 );
    sat_judge = sat_solver_addclause( pSat_sovler, Lits, Lits + 3 ) ;
    assert(sat_judge);

    Lits[0] = toLitCond( pCnf->pVarNums[pObj->Id], 1 ); 
    Lits[1] = toLitCond( pCnf->pVarNums[pObj->Id] + pCnf -> nVars, 0 );
    Lits[2] = toLitCond( pCnf->pVarNums[pObj->Id] + 2 * pCnf -> nVars, 0 );
    sat_judge = sat_solver_addclause( pSat_sovler, Lits, Lits + 3 ) ;
    assert(sat_judge);

  }
  for (int i = 0; i< Aig_ManCiNum(pAig_ntk)-1; i++){
    for (int j = i+1; j<Aig_ManCiNum(pAig_ntk); j++){
      //(c) (vA(t1) = vB (t2)) + ¬vH (t1) + ¬vH (t2)  
      Lits[0] = toLitCond( pCnf->pVarNums[Aig_ManCi(pAig_ntk,i)->Id], 0 ); 
      Lits[1] = toLitCond( pCnf->pVarNums[Aig_ManCi(pAig_ntk,j)->Id] + pCnf -> nVars, 1 );
      Lits[2] = toLitCond( pCnf->pVarNums[Aig_ManCi(pAig_ntk,i)->Id] + 2 * pCnf -> nVars, 1 );
      Lits[3] = toLitCond( pCnf->pVarNums[Aig_ManCi(pAig_ntk,j)->Id] + 2 * pCnf -> nVars, 1 );
      sat_judge = sat_solver_addclause( pSat_sovler, Lits, Lits + 4 ) ;
      assert(sat_judge);
      Lits[0] = toLitCond( pCnf->pVarNums[Aig_ManCi(pAig_ntk,i)->Id], 1 ); 
      Lits[1] = toLitCond( pCnf->pVarNums[Aig_ManCi(pAig_ntk,j)->Id] + pCnf -> nVars, 0 );
      Lits[2] = toLitCond( pCnf->pVarNums[Aig_ManCi(pAig_ntk,i)->Id] + 2 * pCnf -> nVars, 1 );
      Lits[3] = toLitCond( pCnf->pVarNums[Aig_ManCi(pAig_ntk,j)->Id] + 2 * pCnf -> nVars, 1 );
      sat_judge = sat_solver_addclause( pSat_sovler, Lits, Lits + 4 ) ;
      assert(sat_judge);

      Lits[0] = toLitCond( pCnf->pVarNums[Aig_ManCi(pAig_ntk,j)->Id] , 0 ); 
      Lits[1] = toLitCond( pCnf->pVarNums[Aig_ManCi(pAig_ntk,i)->Id] + pCnf -> nVars, 1 );
      Lits[2] = toLitCond( pCnf->pVarNums[Aig_ManCi(pAig_ntk,i)->Id] + 2 * pCnf -> nVars, 1 );
      Lits[3] = toLitCond( pCnf->pVarNums[Aig_ManCi(pAig_ntk,j)->Id] + 2 * pCnf -> nVars, 1 );
      sat_judge = sat_solver_addclause( pSat_sovler, Lits, Lits + 4 ) ;
      assert(sat_judge);
      Lits[0] = toLitCond( pCnf->pVarNums[Aig_ManCi(pAig_ntk,j)->Id] , 1 ); 
      Lits[1] = toLitCond( pCnf->pVarNums[Aig_ManCi(pAig_ntk,i)->Id] + pCnf -> nVars, 0 );
      Lits[2] = toLitCond( pCnf->pVarNums[Aig_ManCi(pAig_ntk,i)->Id] + 2 * pCnf -> nVars, 1 );
      Lits[3] = toLitCond( pCnf->pVarNums[Aig_ManCi(pAig_ntk,j)->Id] + 2 * pCnf -> nVars, 1 );
      sat_judge = sat_solver_addclause( pSat_sovler, Lits, Lits + 4 ) ;
      assert(sat_judge);
      //vA(t1) != vA (t2)
      Lits[0] = toLitCond( pCnf->pVarNums[Aig_ManCi(pAig_ntk,i)->Id] , 0 ); 
      Lits[1] = toLitCond( pCnf->pVarNums[Aig_ManCi(pAig_ntk,j)->Id] , 0 );
      Lits[2] = toLitCond( pCnf->pVarNums[Aig_ManCi(pAig_ntk,i)->Id] + 2 * pCnf -> nVars, 1 );
      Lits[3] = toLitCond( pCnf->pVarNums[Aig_ManCi(pAig_ntk,j)->Id] + 2 * pCnf -> nVars, 1 );
      sat_judge = sat_solver_addclause( pSat_sovler, Lits, Lits + 4 ) ;
      assert(sat_judge);
      Lits[0] = toLitCond( pCnf->pVarNums[Aig_ManCi(pAig_ntk,i)->Id] , 1 ); 
      Lits[1] = toLitCond( pCnf->pVarNums[Aig_ManCi(pAig_ntk,j)->Id] , 1 );
      Lits[2] = toLitCond( pCnf->pVarNums[Aig_ManCi(pAig_ntk,i)->Id] + 2 * pCnf -> nVars, 1 );
      Lits[3] = toLitCond( pCnf->pVarNums[Aig_ManCi(pAig_ntk,j)->Id] + 2 * pCnf -> nVars, 1 );
      sat_judge = sat_solver_addclause( pSat_sovler, Lits, Lits + 4 ) ;
      assert(sat_judge);

    }
  }
  //(a) vA(yk) ⊕ vB (yk)
  Aig_ManForEachCo(pAig_ntk,pObj,ith){
    Lits[0] = toLitCond( pCnf->pVarNums[pObj->Id] , 0); 
    Lits[1] = toLitCond( pCnf->pVarNums[pObj->Id] + pCnf -> nVars, 0 );
    sat_judge = sat_solver_addclause( pSat_sovler, Lits, Lits + 2 ) ;
    assert(sat_judge);
    Lits[0] = toLitCond( pCnf->pVarNums[pObj->Id] , 1); 
    Lits[1] = toLitCond( pCnf->pVarNums[pObj->Id] + pCnf -> nVars, 1 );
    sat_judge = sat_solver_addclause( pSat_sovler, Lits, Lits + 2 ) ;
    assert(sat_judge);
  }

  
  bool flag_arr[Aig_ManCiNum(pAig_ntk)][Aig_ManCiNum(pAig_ntk)];
  for (int i = 0; i< Aig_ManCiNum(pAig_ntk); i++){
    for (int j = 0; j<Aig_ManCiNum(pAig_ntk); j++){
      flag_arr[i][j]=false;
    }
  }

  for (int i = 0; i< Aig_ManCiNum(pAig_ntk)-1; i++){
    for (int j = i+1; j<Aig_ManCiNum(pAig_ntk); j++){
      if(flag_arr[i][j]){
        // cout << "why";
        cout << i << " " << j << endl;
        continue;
      }
      Lits[0] = toLitCond( pCnf->pVarNums[Aig_ManCi(pAig_ntk,i)->Id] + 2 * pCnf -> nVars, 0 );
      Lits[1] = toLitCond( pCnf->pVarNums[Aig_ManCi(pAig_ntk,j)->Id] + 2 * pCnf -> nVars, 0 );
      int sat_result = sat_solver_solve(pSat_sovler,Lits, Lits + 2,0,0,0,0);
      if(sat_result == l_False){
        cout << i << " " << j << endl;
        flag_arr[i][j]=true;
      }
      // Lits[0] = toLitCond( pCnf->pVarNums[Aig_ManCi(pAig_ntk,i)->Id] + 2 * pCnf -> nVars, 1 );
      // Lits[1] = toLitCond( pCnf->pVarNums[Aig_ManCi(pAig_ntk,j)->Id] + 2 * pCnf -> nVars, 1 );
      
    }
    //Note that symmetry satisfies transitivity. In other words, given that yk is
    //symmetric in (xi, xj ) and (xj , xk), then yk is also symmetric in (xi, xk).
    for(int j = i+1; j<Aig_ManCiNum(pAig_ntk)-1; j++){
      for(int k = j+1; k<Aig_ManCiNum(pAig_ntk); k++){
        if(flag_arr[i][j] && flag_arr[i][k]){
          flag_arr[j][k] = true;
          flag_arr[k][j] = true;
        }
      }
    }
    
  }


}

int Lsv_CommandSymSatAll(Abc_Frame_t* pAbc, int argc, char** argv){
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

  Lsv_sym_sat_all(pNtk,argv[1]);
  return 0;

  usage:
  Abc_Print(-2, "usage: lsv_print_nodes [-h]\n");
  Abc_Print(-2, "\t        prints the nodes in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}
