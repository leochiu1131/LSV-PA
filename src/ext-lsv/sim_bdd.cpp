#include "base/abc/abc.h"
#include "bdd/cudd/cudd.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <vector>
#include <iostream>
#include <algorithm>
#include <string>
#include <fstream>
using namespace std;

static int Lsv_CommandSimBdd(Abc_Frame_t* pAbc, int argc, char** argv);

void init_bdd(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", Lsv_CommandSimBdd, 0);
}

void destroy_bdd(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer_bdd = {init_bdd, destroy_bdd};

struct PackageRegistrationManager_bdd {
  PackageRegistrationManager_bdd() { Abc_FrameAddInitializer(&frame_initializer_bdd); }
} lsvPackageRegistrationManager_bdd;

void Lsv_NtkSimBdd(Abc_Ntk_t* pNtk, string input) {
  if (! Abc_NtkIsBddLogic(pNtk)){
    std::cout<<"Please use this function when the circuit is at BDD form!\n";
  }
  else{
    //std::cout<<"Start simulate!!!\n";

    // there are many primary outputs, do the simulation from each primary inputs.
    // refer to Abc_NtkShowBdd
/*
    DdManager * dd;
    dd = (DdManager *)Abc_NtkBuildGlobalBdds( pNtk, 10000000, 1, 0, 0, 0 );
    Vec_Ptr_t * vFuncsGlob;
    Abc_Obj_t * pObj; int i;
    char ** ppNamesIn, ** ppNamesOut;

    vFuncsGlob = Vec_PtrAlloc( Abc_NtkCoNum(pNtk) );
    Abc_NtkForEachCo( pNtk, pObj, i )
        Vec_PtrPush( vFuncsGlob, Abc_ObjGlobalBdd(pObj) );

    ppNamesIn = Abc_NtkCollectCioNames( pNtk, 0 );
    ppNamesOut = Abc_NtkCollectCioNames( pNtk, 1 );
*/
/*
    Abc_NtkForEachPo(pNtk, pPo, ithPo) {
    Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo); 
    assert( Abc_NtkIsBddLogic(pRoot->pNtk) );
    DdManager * dd = (DdManager *)pRoot->pNtk->pManFunc;  
    DdNode* ddnode = (DdNode *)pRoot->pData;
    }

    char** vNamesIn = (char**) Abc_NodeGetFaninNames(pRoot)->pArray;
*/

//-------------------------------------------------------------------------------------------------------------------------------------------------------
    // find the BDD node associated with each PO
    Abc_Obj_t* pPo;
    int ithPo;
    vector<bool> final_result;
/*
    for (int i=0; i < sizeof(ppNamesIn)-4; i++){
      std::cout << ppNamesIn[i] << std::endl;
    }
    std::cout << "\n\n";
*/
    Abc_NtkForEachPo(pNtk, pPo, ithPo) {
      Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo); 
      assert( Abc_NtkIsBddLogic(pRoot->pNtk) );
      DdManager * dd = (DdManager *)pRoot->pNtk->pManFunc;  
      DdNode* ddnode_ori = (DdNode *)pRoot->pData;
      DdNode* ddnode = ddnode_ori;
      Cudd_Ref(ddnode);

      // find the variable order of the BDD
      char** vNamesIn = (char**) Abc_NodeGetFaninNames(pRoot)->pArray;
      char **ppNamesIn = Abc_NtkCollectCioNames( pNtk, 0 );
      
      int ddnode_size = Cudd_SupportSize(dd, ddnode);

/*
      for (int i=0; i < sizeof(vNamesIn)-4; i++){
        std::cout << vNamesIn[i] << std::endl;
      }
      std::cout << "\n\n";
*/
      int counter = 0;
      for (counter; counter < ddnode_size; counter++){
        DdNode *temp_ddnode;

        char* temp_input = vNamesIn[counter];
        int true_pos = 0;
        for (true_pos; true_pos < sizeof(ppNamesIn)/2; true_pos++){
          if (strcmp(temp_input, ppNamesIn[true_pos])==0){
            break;
          }
        }
        //std::cout << "true_pos: " << true_pos << std::endl;


        if (input[true_pos]=='1'){
          temp_ddnode = Cudd_Cofactor(dd, ddnode, Cudd_bddIthVar(dd, counter));
          Cudd_Ref(temp_ddnode);
        }
        else{
          temp_ddnode = Cudd_Cofactor(dd, ddnode, Cudd_Not(Cudd_bddIthVar(dd, counter)));
          Cudd_Ref(temp_ddnode);
        }
        Cudd_RecursiveDeref(dd, ddnode);
        ddnode = temp_ddnode;
        Cudd_RecursiveDeref(dd, temp_ddnode);
      }

      DdNode *temp_ddnode = Cudd_ReadOne(dd);
      bool result;
      if (ddnode == temp_ddnode){result = true;}
      else {result = false;}

      if (result){final_result.push_back(true);}
      else {final_result.push_back(false);}
    }
    //std::cout << "Done!!!!!!!!" << std::endl;
    Abc_Obj_t* ppo;
    int po_idx;
    Abc_NtkForEachPo(pNtk, ppo, po_idx){
      char * ouput_name = Abc_ObjName(ppo);
      std::cout << ouput_name << ": ";
      std:cout << final_result[po_idx];
      std::cout << "\n";
    }


/*
    DdNode *copy_ddnode = Abc_NtkGlobalBdd(pNtk);
    Cudd_Ref(temp_ddnode);
    int counter = 0;
    for (counter; counter < input.size(); counter++){
      DdNode *temp_ddnode;
      if (input[counter]=='1'){
        temp_ddnode = Cudd_Cofactor(Abc_NtkGlobalBddMan(pNtk), copy_ddnode, Cudd_bddIthVar(Abc_NtkGlobalBddMan(pNtk), counter));
        Cudd_Ref(temp_ddnode);
      }
      else{
        temp_ddnode = Cudd_Cofactor(Abc_NtkGlobalBddMan(pNtk), copy_ddnode, Cudd_Not(Cudd_bddIthVar(Abc_NtkGlobalBddMan(pNtk), counter)));
        Cudd_Ref(temp_ddnode);
      }
      copy_ddnode = temp_ddnode;
      Cudd_RecursiveDeref(Abc_NtkGlobalBddMan(pNtk), temp_ddnode);
    }
    DdNode *temp_ddnode = Cudd_ReadOne(Abc_NtkGlobalBddMan(pNtk));
    bool result;
    if (copy_ddnode == temp_ddnode){result = true;}
    else {result = false;}
*/


  }
}






int Lsv_CommandSimBdd(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  /*
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
  */
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }


  string input = argv[1];
  //std::cout << "input string: " << input << std::endl;

  Lsv_NtkSimBdd(pNtk, input);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sim_bdd [-h]\n");
  Abc_Print(-2, "\t        given input file, simulate the bdd circuit.\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}
