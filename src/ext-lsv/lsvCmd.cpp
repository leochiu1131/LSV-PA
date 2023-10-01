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

using namespace std;
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