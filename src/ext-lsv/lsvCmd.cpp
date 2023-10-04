#include <iostream>
#include <map>
#include <fstream>
#include <vector>
#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"

// Lsv_CommandSimulateBDD

static int Lsv_CommandSimulateBDD(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimulateAIG(Abc_Frame_t* pAbc, int argc, char** argv);


void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd",Lsv_CommandSimulateBDD, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig",Lsv_CommandSimulateAIG, 0);
}

void destroy(Abc_Frame_t* pAbc) {}
Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

void lsv_sim_bdd(Abc_Ntk_t* pNtk, std::string argv) {
  // Abc_Obj_t* pObj;
  int ithPo;
  int ithPi;
  Abc_Obj_t* pPi;
  Abc_Obj_t* pPo;
  std::map<std::string,bool> boolean;

  Abc_NtkForEachPi(pNtk, pPi, ithPi) {
    boolean[Abc_ObjName(pPi)]= (argv[ithPi]=='1');
  }

  Abc_NtkForEachPo(pNtk, pPo, ithPo) {
    Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo); 
    assert( Abc_NtkIsBddLogic(pRoot->pNtk) );
    DdManager * dd = (DdManager *)pRoot->pNtk->pManFunc;  
    DdNode* ddnode = (DdNode *)pRoot->pData;

    char** vNamesIn = (char**) Abc_NodeGetFaninNames(pRoot)->pArray;

    for ( ithPi = 0; ithPi < Abc_ObjFaninNum(pRoot) ; ithPi++)
    {
      std::string iPi=(std::string)vNamesIn[ithPi];
      if (boolean[iPi]==0){
      
        ddnode=Cudd_Cofactor(dd, ddnode, Cudd_Not(Cudd_bddIthVar(dd, ithPi)));
      }

      else{
        ddnode=Cudd_Cofactor(dd, ddnode, Cudd_bddIthVar(dd, ithPi));
      }
      
    }
    if (ddnode==Cudd_ReadOne(dd)){
    
      if (pPo->fCompl0){
        printf("%s: 0 \n",Abc_ObjName(pPo));
      }  
      else{
        printf("%s: 1 \n",Abc_ObjName(pPo));
      }
    }
    else{
      
      if (pPo->fCompl0){
        printf("%s: 1 \n",Abc_ObjName(pPo));
      }  
      else{
        printf("%s: 0 \n",Abc_ObjName(pPo));
      }
    }
  
  }
}

int Lsv_CommandSimulateBDD(Abc_Frame_t* pAbc, int argc, char** argv) {
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
  lsv_sim_bdd(pNtk, argv[1]);
  return 0;
  
usage:
  Abc_Print(-2, "usage: lsv_print_nodes [-h]\n");
  Abc_Print(-2, "\t        prints the nodes in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}









// Lsv_CommandSimulateAIG





void lsv_sim_aig(Abc_Ntk_t* pNtk, std::string argv) {

  std::ifstream inFile;
  inFile.open(argv);
  std::string input;
  std::vector<std::vector<bool>> bool_vector(Abc_NtkPiNum(pNtk));
  std::vector<std::vector<bool>> bool_vector_po(Abc_NtkPoNum(pNtk));
  while(getline(inFile, input)){
    for ( int i = 0; i < Abc_NtkPiNum(pNtk); i++)
    {
      bool_vector[i].push_back(input[i]=='1');

    }
  }
  

  std::map<Abc_Obj_t *,unsigned> value;
    Abc_Obj_t* pObj;
    int ithObj;

    Abc_NtkForEachObj(pNtk, pObj, ithObj) {
      value[pObj]= 0;
    }

  int c=0;
  while(true){
    Abc_Obj_t* pPi;
    int ithPi;
    Abc_NtkForEachPi(pNtk, pPi, ithPi) {
      value[pPi]=2*value[pPi]+bool_vector[ithPi][c];
    }

    c++;
    if (c%32==0 || c==bool_vector[0].size()){
      Abc_NtkForEachObj(pNtk, pObj, ithObj) {
        if (Abc_ObjIsPi(pObj)){
          continue;
        }

        if (Abc_ObjIsPo(pObj)){
          continue;
        }

        if (pObj -> Type == ABC_OBJ_CONST1){
          value[pObj] = 0xffffffff;
          continue;

        }
        if (pObj->fCompl0){

          if (pObj->fCompl1){
           value[pObj] = (~value[Abc_ObjFanin(pObj,0)]) & (~value[Abc_ObjFanin(pObj,1)]);
          }

          else {
            value[pObj] = (~value[Abc_ObjFanin(pObj,0)]) & value[Abc_ObjFanin(pObj,1)];
            
          }
        }
         else {

          if (pObj->fCompl1){
          value[pObj] = value[Abc_ObjFanin(pObj,0)] & (~value[Abc_ObjFanin(pObj,1)]);
          }

          else {
            value[pObj] = value[Abc_ObjFanin(pObj,0)] & value[Abc_ObjFanin(pObj,1)];
            
          }
        }
      }


      Abc_Obj_t* pPo;
      int ithPo;
      Abc_NtkForEachPo(pNtk, pPo, ithPo) {
        unsigned po_value;  
        if (pPo->fCompl0){
          po_value = ~value[Abc_ObjFanin(pPo,0)];
        }  
        else{
          po_value = value[Abc_ObjFanin(pPo,0)];
        }
        std::vector<bool> bool_vector_temporary_po;
        for ( int i = 0; i < (c%32); i++){
          if ((po_value%2)==1){
            bool_vector_temporary_po.push_back(1);
          }

          else {
            bool_vector_temporary_po.push_back(0); 

          }

          po_value=po_value/2;
        } 
        for ( int j =((c%32)-1) ; j >= 0; j--){
        bool_vector_po[ithPo].push_back( bool_vector_temporary_po[j]);
        }
          


      }

      if (c==bool_vector[0].size()){
        break;
      }

      Abc_NtkForEachObj(pNtk, pObj, ithObj) {
        value[pObj]= 0;
      }
    }

  }
  Abc_Obj_t* pPo;
  int ithPo;
  Abc_NtkForEachPo(pNtk, pPo, ithPo) {
    
    printf("%s: ",Abc_ObjName(pPo));
    for(int k = 0; k < bool_vector[0].size() ; k++ ){
      printf("%d", (int) bool_vector_po[ithPo][k] );
  
    }
    printf("\n");
      
  }
}


int Lsv_CommandSimulateAIG(Abc_Frame_t* pAbc, int argc, char** argv) {
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
  lsv_sim_aig(pNtk, argv[1]);
  return 0;
  
usage:
  Abc_Print(-2, "usage: lsv_print_nodes [-h]\n");
  Abc_Print(-2, "\t        prints the nodes in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}















/*
static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
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
*/