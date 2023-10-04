#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <vector>
#include <string>
#include <unordered_map>
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


/*
#############################
###  lsv_sim_bdd Section  ###
#############################
*/
int Lsv_SimBdd(Abc_Ntk_t* pNtk, char* input_pattern) {
  Abc_Obj_t *pPi, *pPo;
  int i;
  unordered_map<string, bool> input_map;
  //build map for input name and value
  Abc_NtkForEachPi(pNtk, pPi, i) {
    input_map[Abc_ObjName(pPi)] = input_pattern[i] == '1';
  }
  //simulate for each bdd PO
  Abc_NtkForEachPo(pNtk, pPo, i) {
    Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo);
    assert( Abc_NtkIsBddLogic(pRoot->pNtk) );
    DdManager * dd = (DdManager *)pRoot->pNtk->pManFunc;  
    DdNode* ddnode = (DdNode *)pRoot->pData;   
    
    //get the variable order of this bdd PO
    Vec_Ptr_t *vName = Abc_NodeGetFaninNames(pRoot);

    //do cofactor for assigning value to variable
    DdNode * Cof = ddnode;
    for(int i = 0; i < vName->nSize; i++) {
      if(Cudd_IsConstant(Cof)) {
        break;
        cout<<"current node is constant, break at "<<i<<"\n";
      }
      if (input_map[(char*)vName->pArray[i]]) {
        Cof = Cudd_Cofactor(dd, Cof, Cudd_bddIthVar(dd, i)); 
      } else {
        Cof = Cudd_Cofactor(dd, Cof, Cudd_Not(Cudd_bddIthVar(dd, i))); 
      }
    }

    if(Cof == Cudd_ReadOne(dd)) {
      cout<<Abc_ObjName(pPo)<<": "<<"1\n";
    } else if(Cof == Cudd_ReadLogicZero(dd)) {
      cout<<Abc_ObjName(pPo)<<": "<<"0\n";
    } else {
      cout<<Abc_ObjName(pPo)<<": non-constant, something wrong with simulation\n";
    }

  } //end of foreach PO
  return 0;
}

int Lsv_CommandSimBdd(Abc_Frame_t* pAbc, int argc, char** argv) {
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

  if (pNtk == NULL) {
    cout<<"ERROR: No existing network.\n";
    return 1;
  }
  if (!Abc_NtkHasBdd(pNtk)) {
    cout<<"ERROR: No existing BDD network.\n";
    return 1;
  }
  if (argc < 2) {
    cout<<"ERROR: No input pattern specified\n";
    return 1;
  }
  if (Abc_NtkPiNum(pNtk) != strlen(argv[1])) {
    cout<<"ERROR: Mismatch between input pattern length and circuit PI number.\n";
    return 1;
  }

  Lsv_SimBdd(pNtk, argv[1]);

  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sim_bdd [-h] <input_pattern>\n");
  Abc_Print(-2, "\t do simulation on BDD with given input pattern\n");
  Abc_Print(-2, "\t-h              : print the command usage\n");
  Abc_Print(-2, "\t<input_pattern> : sequence of 0 and 1, specify input pin values\n");
  return 1;
}


/*
#############################
###  lsv_sim_aig Section  ###
#############################
*/
int Lsv_SimAig_ParallelSim(Abc_Ntk_t* pNtk, Vec_Ptr_t* vNode, vector<vector<int>> &result) {
  Abc_Obj_t *pObj;
  int i;

  for (i = 0; i < vNode->nSize; i++)
  {
    pObj = (Abc_Obj_t*)vNode->pArray[i];
    pObj->iData = (pObj->fCompl0 ? ~(Abc_ObjFanin0(pObj)->iData) : Abc_ObjFanin0(pObj)->iData) &
                  (pObj->fCompl1 ? ~(Abc_ObjFanin1(pObj)->iData) : Abc_ObjFanin1(pObj)->iData);
  }
  Abc_NtkForEachPo(pNtk, pObj, i)
  {
    pObj->iData = pObj->fCompl0 ? ~(Abc_ObjFanin0(pObj)->iData) : Abc_ObjFanin0(pObj)->iData;
    result[i].push_back(pObj->iData);
  }
  Abc_NtkForEachPi(pNtk, pObj, i) pObj->iData = 0;
  return 0;
}

void Lsv_SimAig_Output(Abc_Ntk_t* pNtk, vector<vector<int>> &result, int residual) {
  Abc_Obj_t *pObj;
  int iPo, jInt, kBit;

  Abc_NtkForEachPo(pNtk, pObj, iPo) {
    cout<<Abc_ObjName(pObj)<<": ";
    for (jInt = 0; jInt < result[iPo].size(); jInt++)
    {
      if (jInt == result[iPo].size()-1 && residual > 0)
      {
        for (kBit = 0; kBit < residual; kBit++) 
          cout<<( (result[iPo][jInt]>>kBit) & 1 );
      }
      else
      {
        for (kBit = 0; kBit < 32; kBit++) 
          cout<<( (result[iPo][jInt]>>kBit) & 1 );
      }      
    }
    cout<<'\n';
  }
  return;
}

int Lsv_SimAig(Abc_Ntk_t* pNtk, char* input_file) {
  int i;
  char pattCount;
  Abc_Obj_t *pObj;
  fstream inFile;
  string line;
  vector<vector<int>> result(Abc_NtkPoNum(pNtk));

  inFile.open( input_file, std::fstream::in );
  inFile >> line;
  pattCount = 0;
  //initialize data of CI node
  Abc_NtkForEachPi(pNtk, pObj, i) pObj->iData = 0;
  //get the DFS order of aig nodes( only internal nodes )
  Vec_Ptr_t *vNode = Abc_AigDfsMap( pNtk );

  while (!inFile.eof())
  {
    pattCount++; //valid pattern count
    Abc_NtkForEachPi(pNtk, pObj, i)
    {
      if (line[i] == '1'){
        pObj->iData |= (1 << (pattCount-1));
      }
    }
    inFile>>line;
    //simulate 32 patterns
    if (pattCount == 32)
    {
      Lsv_SimAig_ParallelSim(pNtk, vNode, result);
      pattCount = 0;
    }
  }
  inFile.close();

  //simulate residual patterns
  if (pattCount > 0) Lsv_SimAig_ParallelSim(pNtk, vNode, result);
  Lsv_SimAig_Output(pNtk, result, pattCount);

  return 0;
}


int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv) {
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

  if (pNtk == NULL) {
    cout<<"ERROR: No existing network.\n";
    return 1;
  }
  if (!Abc_NtkHasAig(pNtk)) {
    cout<<"ERROR: Current network doesn't have AIG.\n";
    return 1;
  }
  if (argc < 2) {
    cout<<"ERROR: No input pattern file specified.\n";
    return 1;
  }

  Lsv_SimAig(pNtk, argv[1]);
  
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sim_aig [-h] <input_file>\n");
  Abc_Print(-2, "\t do parallel simulation on AIG with given input pattern file.\n");
  Abc_Print(-2, "\t-h           : print the command usage\n");
  Abc_Print(-2, "\t<input_file> : the input file path of \n");
  return 1;
}