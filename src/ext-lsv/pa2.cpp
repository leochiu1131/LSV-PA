#include <iostream>
#include <map>
#include <fstream>
#include <vector>
#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/cnf/cnf.h"
extern "C"{
    Aig_Man_t* Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
}



// lsv_sym_bdd (Symmetry Checking with BDD)

static int Lsv_SymmetryCheckingwithBDD(Abc_Frame_t* pAbc, int argc, char** argv);
//static int Lsv_SymmetryCheckingwithSAT(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_bdd",Lsv_SymmetryCheckingwithBDD, 0);
  //Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_sat",Lsv_SymmetryCheckingwithSAT, 0);
}

void destroy(Abc_Frame_t* pAbc) {}
Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;



void lsv_sym_bdd(Abc_Ntk_t* pNtk, int ithPi, int jthPi, int kthPo) {
  
  int i;
  Abc_Obj_t* pPi;
  std::map<std::string,int> boolean;

  Abc_NtkForEachPi(pNtk, pPi, i) {
    boolean[Abc_ObjName(pPi)]= i;
  }


  Abc_Obj_t* kthPo_node = Abc_NtkPo( pNtk, kthPo );
  Abc_Obj_t* pRoot = Abc_ObjFanin0(kthPo_node); 
  assert( Abc_NtkIsBddLogic(pRoot->pNtk) );
  DdManager * dd = (DdManager *)pRoot->pNtk->pManFunc;  
  DdNode* ddnode1 = (DdNode *)pRoot->pData;
  DdNode* ddnode2 = (DdNode *)pRoot->pData;

  char** vNamesIn = (char**) Abc_NodeGetFaninNames(pRoot)->pArray;

  for ( i = 0; i < Abc_ObjFaninNum(pRoot) ; i++)
  {
    std::string iPi=(std::string)vNamesIn[i];
    if (boolean[iPi]==ithPi){
      ddnode1=Cudd_Cofactor(dd, ddnode1, Cudd_Not(Cudd_bddIthVar(dd, i)));
      ddnode2=Cudd_Cofactor(dd, ddnode2, Cudd_bddIthVar(dd, i));
    }

    else if (boolean[iPi]==jthPi){
      ddnode2=Cudd_Cofactor(dd, ddnode2, Cudd_Not(Cudd_bddIthVar(dd, i)));
      ddnode1=Cudd_Cofactor(dd, ddnode1, Cudd_bddIthVar(dd, i));
    }
      
  }

  if (ddnode1==ddnode2){

    printf("symmetric \n");
  
  }
  else{ 

    printf("asymmetric \n");
    std::vector<bool> bool_vector(Abc_NtkPiNum(pNtk));
    DdNode* Xor= Cudd_bddXor(dd, ddnode1, ddnode2);
    for ( i = 0; i < Abc_ObjFaninNum(pRoot) ; i++){
      
      if(Cudd_Cofactor(dd, Xor, Cudd_bddIthVar(dd, i)) == Cudd_Not(Cudd_ReadOne(dd))){
        std::string iPi=(std::string)vNamesIn[i];
        bool_vector[boolean[iPi]]=0;
        Xor = Cudd_Cofactor(dd, Xor, Cudd_Not(Cudd_bddIthVar(dd, i)));

      }
      else {
        std::string iPi=(std::string)vNamesIn[i];
        bool_vector[boolean[iPi]]=1; 
        Xor = Cudd_Cofactor(dd, Xor, Cudd_bddIthVar(dd, i));

      }  
  
    
    }
    for(int k = 0; k < bool_vector.size() ; k++ ){
      if(k == ithPi){
        printf("1");

      }
      else if(k == jthPi){
        printf("0");

      }
      else{
        printf("%d", (int) bool_vector[k] );

      }
      
    }
    printf("\n"); 

    for(int k = 0; k < bool_vector.size() ; k++ ){
      if(k == ithPi){
        printf("0");

      }
      else if(k == jthPi){
        printf("1");

      }
      else{
        printf("%d", (int) bool_vector[k]);

      }
      
    }
    printf("\n"); 

  }
}



int Lsv_SymmetryCheckingwithBDD(Abc_Frame_t* pAbc, int argc, char** argv) {
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
  lsv_sym_bdd(pNtk, std::stoi(argv[2]), std::stoi(argv[3]), std::stoi(argv[1]));
  return 0;


usage:
  Abc_Print(-2, "usage: lsv_print_nodes [-h]\n");
  Abc_Print(-2, "\t        prints the nodes in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;

}













// lsv_sym_sat (Symmetry Checking with SAT)

static int Lsv_SymmetryCheckingwithSAT(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  //Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_bdd",Lsv_SymmetryCheckingwithBDD, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_sat",Lsv_SymmetryCheckingwithSAT, 0);
}

void destroy(Abc_Frame_t* pAbc) {}
Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

void lsv_sym_sat(Abc_Ntk_t* pNtk, int ithPi, int jthPi, int kthPo) {
  int nVarsPlus;

  Abc_Obj_t* kthPo_node = Abc_NtkPo( pNtk, kthPo );
  Abc_Obj_t* pRoot = Abc_ObjFanin0(kthPo_node);
  
  Abc_Ntk_t * Cone = Abc_NtkCreateCone(pNtk, pRoot, Abc_ObjName(kthPo_node), 1);
  Aig_Man_t * AIG_circuit = Abc_NtkToDar( Cone, 0, 0 );
  sat_solver* satsolver = sat_solver_new();
  Cnf_Dat_t * corresponding_CNFA = Cnf_Derive( AIG_circuit, Aig_ManCoNum(AIG_circuit) );
  satsolver = (sat_solver*)Cnf_DataWriteIntoSolverInt( satsolver, corresponding_CNFA, 1, 0 );
  Cnf_DataLift( corresponding_CNFA, corresponding_CNFA->nVars );
  satsolver = (sat_solver*)Cnf_DataWriteIntoSolverInt( satsolver, corresponding_CNFA, 1, 0 );
  
  int t;
  Abc_Obj_t* pPi;
  lit clauses[2];

  Abc_NtkForEachPi(pNtk, pPi, t) {
    if( t != ithPi && t != jthPi){
      clauses[0] = toLitCond(corresponding_CNFA->pVarNums[pPi->Id]-corresponding_CNFA->nVars, 0);
      clauses[1] = toLitCond(corresponding_CNFA->pVarNums[pPi->Id], 1);
      
      //corresponding_CNFA->pVarNums[pPi->Id];//B
      //corresponding_CNFA->pVarNums[pPi->Id]-corresponding_CNFA->nVars;//A
      int RetValue = sat_solver_addclause( satsolver, clauses, clauses + 2 );
      if( RetValue == 0 ){
        printf("symmetric \n");
        Cnf_DataFree(corresponding_CNFA);
        sat_solver_delete(satsolver);
        return;
      }
      
      clauses[0] = toLitCond(corresponding_CNFA->pVarNums[pPi->Id]-corresponding_CNFA->nVars, 1);
      clauses[1] = toLitCond(corresponding_CNFA->pVarNums[pPi->Id], 0);
      RetValue = sat_solver_addclause( satsolver, clauses, clauses + 2 );
      if( RetValue == 0 ){
        printf("symmetric \n");
        Cnf_DataFree(corresponding_CNFA);
        sat_solver_delete(satsolver);
        return;
      }


    }
    
  }

  
  clauses[0] = toLitCond(corresponding_CNFA->pVarNums[Abc_NtkPi(pNtk, ithPi)->Id]-corresponding_CNFA->nVars, 1);
  clauses[1] = toLitCond(corresponding_CNFA->pVarNums[Abc_NtkPi(pNtk, jthPi)->Id], 0);
  int RetValue = sat_solver_addclause( satsolver, clauses, clauses + 2 );
  if( RetValue == 0 ){
    printf("symmetric \n");
    Cnf_DataFree(corresponding_CNFA);
    sat_solver_delete(satsolver);
    return;
  }

  clauses[0] = toLitCond(corresponding_CNFA->pVarNums[Abc_NtkPi(pNtk, ithPi)->Id]-corresponding_CNFA->nVars, 0);
  clauses[1] = toLitCond(corresponding_CNFA->pVarNums[Abc_NtkPi(pNtk, jthPi)->Id], 1);
  RetValue = sat_solver_addclause( satsolver, clauses, clauses + 2 );
  if( RetValue == 0 ){
    printf("symmetric \n");
    Cnf_DataFree(corresponding_CNFA);
    sat_solver_delete(satsolver);
    return;
  }

  clauses[0] = toLitCond(corresponding_CNFA->pVarNums[Abc_NtkPi(pNtk, jthPi)->Id]-corresponding_CNFA->nVars, 1);
  clauses[1] = toLitCond(corresponding_CNFA->pVarNums[Abc_NtkPi(pNtk, ithPi)->Id], 0);
  RetValue = sat_solver_addclause( satsolver, clauses, clauses + 2 );
  if( RetValue == 0 ){
    printf("symmetric \n");
    Cnf_DataFree(corresponding_CNFA);
    sat_solver_delete(satsolver);
    return;
  }

  clauses[0] = toLitCond(corresponding_CNFA->pVarNums[Abc_NtkPi(pNtk, jthPi)->Id]-corresponding_CNFA->nVars, 0);
  clauses[1] = toLitCond(corresponding_CNFA->pVarNums[Abc_NtkPi(pNtk, ithPi)->Id], 1);
  RetValue = sat_solver_addclause( satsolver, clauses, clauses + 2 );
  if( RetValue == 0 ){
    printf("symmetric \n");
    Cnf_DataFree(corresponding_CNFA);
    sat_solver_delete(satsolver);
    return;
  }
  
  
  clauses[0] = toLitCond(corresponding_CNFA->pVarNums[Aig_ManCo(AIG_circuit, 0)->Id]-corresponding_CNFA->nVars, 0);
  clauses[1] = toLitCond(corresponding_CNFA->pVarNums[Aig_ManCo(AIG_circuit, 0)->Id], 0);
  RetValue = sat_solver_addclause( satsolver, clauses, clauses + 2 );
  if( RetValue == 0 ){
    printf("symmetric \n");
    Cnf_DataFree(corresponding_CNFA);
    sat_solver_delete(satsolver);
    return;
  }

  clauses[0] = toLitCond(corresponding_CNFA->pVarNums[Aig_ManCo(AIG_circuit, 0)->Id]-corresponding_CNFA->nVars, 1);
  clauses[1] = toLitCond(corresponding_CNFA->pVarNums[Aig_ManCo(AIG_circuit, 0)->Id], 1);
  RetValue = sat_solver_addclause( satsolver, clauses, clauses + 2 );
  if( RetValue == 0 ){
    printf("symmetric \n");
    Cnf_DataFree(corresponding_CNFA);
    sat_solver_delete(satsolver);
    return;
  }

  RetValue = sat_solver_solve( satsolver, NULL, NULL, 0, 0, 0, 0);
  if( RetValue != 1  ){
    printf("symmetric \n");
    Cnf_DataFree(corresponding_CNFA);
    sat_solver_delete(satsolver);
    return;
  }

  else{
    printf("asymmetric \n");

    Abc_NtkForEachPi(pNtk, pPi, t) {
      printf("%d", sat_solver_var_value(satsolver, corresponding_CNFA->pVarNums[pPi->Id]-corresponding_CNFA->nVars));

    }
    printf("\n"); 
    Abc_NtkForEachPi(pNtk, pPi, t) {
      printf("%d", sat_solver_var_value(satsolver, corresponding_CNFA->pVarNums[pPi->Id]));
    
    }
    printf("\n");

    Cnf_DataFree(corresponding_CNFA);
    sat_solver_delete(satsolver);
    return;

    
  }
}





int Lsv_SymmetryCheckingwithSAT(Abc_Frame_t* pAbc, int argc, char** argv) {
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
  lsv_sym_sat(pNtk, std::stoi(argv[2]), std::stoi(argv[3]), std::stoi(argv[1]));
  return 0;



usage:
  Abc_Print(-2, "usage: lsv_print_nodes [-h]\n");
  Abc_Print(-2, "\t        prints the nodes in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
  
}









// lsv_sym_all (Symmetry Checking with Incremental SAT)

static int Lsv_SymmetryCheckingwithIncrementalSAT(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  //Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_bdd",Lsv_SymmetryCheckingwithBDD, 0);
  //Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_sat",Lsv_SymmetryCheckingwithSAT, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_all",Lsv_SymmetryCheckingwithIncrementalSAT, 0);
}

void destroy(Abc_Frame_t* pAbc) {}
Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

void lsv_sym_all(Abc_Ntk_t* pNtk, int kthPo) {
  //int nVarsPlus;

  Abc_Obj_t* kthPo_node = Abc_NtkPo( pNtk, kthPo );
  Abc_Obj_t* pRoot = Abc_ObjFanin0(kthPo_node);
  
  Abc_Ntk_t * Cone = Abc_NtkCreateCone(pNtk, pRoot, Abc_ObjName(kthPo_node), 1);
  Aig_Man_t * AIG_circuit = Abc_NtkToDar( Cone, 0, 0 );
  sat_solver* satsolver = sat_solver_new();
  Cnf_Dat_t * corresponding_CNFA = Cnf_Derive( AIG_circuit, Aig_ManCoNum(AIG_circuit) );
  satsolver = (sat_solver*)Cnf_DataWriteIntoSolverInt( satsolver, corresponding_CNFA, 1, 0 );
  Cnf_DataLift( corresponding_CNFA, corresponding_CNFA->nVars );
  satsolver = (sat_solver*)Cnf_DataWriteIntoSolverInt( satsolver, corresponding_CNFA, 1, 0 );
  Cnf_DataLift( corresponding_CNFA, corresponding_CNFA->nVars );
  satsolver = (sat_solver*)Cnf_DataWriteIntoSolverInt( satsolver, corresponding_CNFA, 1, 0 );

  int t;
  int m;
  int jthPi;
  int ithPi;
  Abc_Obj_t* pPi;
  lit clauses[2];

  //VA(t)=VB(t)
  Abc_NtkForEachPi(pNtk, pPi, t) {

    clauses[0] = toLitCond(corresponding_CNFA->pVarNums[pPi->Id]-corresponding_CNFA->nVars, 0);
    clauses[1] = toLitCond(corresponding_CNFA->pVarNums[pPi->Id], 1);

    //corresponding_CNFA->pVarNums[pPi->Id];//B
    //corresponding_CNFA->pVarNums[pPi->Id]-corresponding_CNFA->nVars;//A
    //corresponding_CNFA->pVarNums[pPi->Id]+corresponding_CNFA->nVars;//H

    int RetValue = sat_solver_addclause( satsolver, clauses, clauses + 2 );
    if( RetValue == 0 ){
      //printf("symmetric \n");
      Abc_NtkForEachPi(pNtk, pPi, t) {
        printf("%d", corresponding_CNFA->pVarNums[pPi->Id]-corresponding_CNFA->nVars);
        printf("%d", corresponding_CNFA->pVarNums[pPi->Id]);
      
      }
      printf("\n");
      Cnf_DataFree(corresponding_CNFA);
      sat_solver_delete(satsolver);
      return;
    }
    
    clauses[0] = toLitCond(corresponding_CNFA->pVarNums[pPi->Id]-corresponding_CNFA->nVars, 1);
    clauses[1] = toLitCond(corresponding_CNFA->pVarNums[pPi->Id], 0);
    
    RetValue = sat_solver_addclause( satsolver, clauses, clauses + 2 );
    if( RetValue == 0 ){
      //printf("symmetric \n");
      Abc_NtkForEachPi(pNtk, pPi, t) {
        printf("%d", corresponding_CNFA->pVarNums[pPi->Id]-corresponding_CNFA->nVars);
        printf("%d", corresponding_CNFA->pVarNums[pPi->Id]);
      
      }
      printf("\n");
      Cnf_DataFree(corresponding_CNFA);
      sat_solver_delete(satsolver);
      return;
    }


    clauses[0] = toLitCond(corresponding_CNFA->pVarNums[pPi->Id]+corresponding_CNFA->nVars, 0);
    RetValue = sat_solver_addclause( satsolver, clauses, clauses + 1 );
    if( RetValue == 0 ){
      //printf("symmetric \n");
      Abc_NtkForEachPi(pNtk, pPi, t) {
        printf("%d", corresponding_CNFA->pVarNums[pPi->Id]-corresponding_CNFA->nVars);
        printf("%d", corresponding_CNFA->pVarNums[pPi->Id]);
      
      }
      printf("\n");
      Cnf_DataFree(corresponding_CNFA);
      sat_solver_delete(satsolver);
      return;
    }
  


    //VA(i)=VB(j)
    clauses[0] = toLitCond(corresponding_CNFA->pVarNums[Abc_NtkPi(pNtk, ithPi)->Id]-corresponding_CNFA->nVars, 1);
    clauses[1] = toLitCond(corresponding_CNFA->pVarNums[Abc_NtkPi(pNtk, jthPi)->Id], 0);
    int RetValue = sat_solver_addclause( satsolver, clauses, clauses + 2 );
    if( RetValue == 0 ){
      //printf("symmetric \n");
      Abc_NtkForEachPi(pNtk, pPi, t) {
        printf("%d", corresponding_CNFA->pVarNums[pPi->Id]-corresponding_CNFA->nVars);
        printf("%d", corresponding_CNFA->pVarNums[pPi->Id]);
          
      }
      printf("\n");
      Cnf_DataFree(corresponding_CNFA);
      sat_solver_delete(satsolver);
      return;
    }

    clauses[0] = toLitCond(corresponding_CNFA->pVarNums[Abc_NtkPi(pNtk, ithPi)->Id]-corresponding_CNFA->nVars, 0);
    clauses[1] = toLitCond(corresponding_CNFA->pVarNums[Abc_NtkPi(pNtk, jthPi)->Id], 1);
    RetValue = sat_solver_addclause( satsolver, clauses, clauses + 2 );
    if( RetValue == 0 ){
      //printf("symmetric \n");
      Abc_NtkForEachPi(pNtk, pPi, t) {
        printf("%d", corresponding_CNFA->pVarNums[pPi->Id]-corresponding_CNFA->nVars);
        printf("%d", corresponding_CNFA->pVarNums[pPi->Id]);
          
      }
      printf("\n");
      Cnf_DataFree(corresponding_CNFA);
      sat_solver_delete(satsolver);
      return;
    }

    clauses[0] = toLitCond(corresponding_CNFA->pVarNums[pPi->Id]+corresponding_CNFA->nVars, 1);
    RetValue = sat_solver_addclause( satsolver, clauses, clauses + 1 );
    if( RetValue == 0 ){
      //printf("symmetric \n");
      Abc_NtkForEachPi(pNtk, pPi, t) {
        printf("%d", corresponding_CNFA->pVarNums[pPi->Id]-corresponding_CNFA->nVars);
        printf("%d", corresponding_CNFA->pVarNums[pPi->Id]);
      
      }
      printf("\n");
      Cnf_DataFree(corresponding_CNFA);
      sat_solver_delete(satsolver);
      return;
    }

    clauses[0] = toLitCond(corresponding_CNFA->pVarNums[pPi->Id]+corresponding_CNFA->nVars, 1);
    RetValue = sat_solver_addclause( satsolver, clauses, clauses + 1 );
    if( RetValue == 0 ){
      //printf("symmetric \n");
      Abc_NtkForEachPi(pNtk, pPi, t) {
        printf("%d", corresponding_CNFA->pVarNums[pPi->Id]-corresponding_CNFA->nVars);
        printf("%d", corresponding_CNFA->pVarNums[pPi->Id]);
      
      }
      printf("\n");
      Cnf_DataFree(corresponding_CNFA);
      sat_solver_delete(satsolver);
      return;
    }


    //VA(j)=VB(i)
    clauses[0] = toLitCond(corresponding_CNFA->pVarNums[Abc_NtkPi(pNtk, jthPi)->Id]-corresponding_CNFA->nVars, 1);
    clauses[1] = toLitCond(corresponding_CNFA->pVarNums[Abc_NtkPi(pNtk, ithPi)->Id], 0);
    RetValue = sat_solver_addclause( satsolver, clauses, clauses + 2 );
    if( RetValue == 0 ){
      //printf("symmetric \n");
      Abc_NtkForEachPi(pNtk, pPi, t) {
        printf("%d", corresponding_CNFA->pVarNums[pPi->Id]-corresponding_CNFA->nVars);
        printf("%d", corresponding_CNFA->pVarNums[pPi->Id]);
          
      }
      printf("\n");
      Cnf_DataFree(corresponding_CNFA);
      sat_solver_delete(satsolver);
      return;
    }

    clauses[0] = toLitCond(corresponding_CNFA->pVarNums[Abc_NtkPi(pNtk, jthPi)->Id]-corresponding_CNFA->nVars, 0);
    clauses[1] = toLitCond(corresponding_CNFA->pVarNums[Abc_NtkPi(pNtk, ithPi)->Id], 1);
    RetValue = sat_solver_addclause( satsolver, clauses, clauses + 2 );
    if( RetValue == 0 ){
      //printf("symmetric \n");
      Abc_NtkForEachPi(pNtk, pPi, t) {
        printf("%d", corresponding_CNFA->pVarNums[pPi->Id]-corresponding_CNFA->nVars);
        printf("%d", corresponding_CNFA->pVarNums[pPi->Id]);
          
      }
      printf("\n");
      Cnf_DataFree(corresponding_CNFA);
      sat_solver_delete(satsolver);
      return;
    }

    clauses[0] = toLitCond(corresponding_CNFA->pVarNums[pPi->Id]+corresponding_CNFA->nVars, 1);
    RetValue = sat_solver_addclause( satsolver, clauses, clauses + 1 );
    if( RetValue == 0 ){
      //printf("symmetric \n");
      Abc_NtkForEachPi(pNtk, pPi, t) {
        printf("%d", corresponding_CNFA->pVarNums[pPi->Id]-corresponding_CNFA->nVars);
        printf("%d", corresponding_CNFA->pVarNums[pPi->Id]);
      
      }
      printf("\n");
      Cnf_DataFree(corresponding_CNFA);
      sat_solver_delete(satsolver);
      return;
    }
    clauses[0] = toLitCond(corresponding_CNFA->pVarNums[pPi->Id]+corresponding_CNFA->nVars, 1);
    RetValue = sat_solver_addclause( satsolver, clauses, clauses + 1 );
    if( RetValue == 0 ){
      //printf("symmetric \n");
      Abc_NtkForEachPi(pNtk, pPi, t) {
        printf("%d", corresponding_CNFA->pVarNums[pPi->Id]-corresponding_CNFA->nVars);
        printf("%d", corresponding_CNFA->pVarNums[pPi->Id]);
      
      }
      printf("\n");
      Cnf_DataFree(corresponding_CNFA);
      sat_solver_delete(satsolver);
      return;
    }
    
    //VA(yk)=VB(yk)
    clauses[0] = toLitCond(corresponding_CNFA->pVarNums[Aig_ManCo(AIG_circuit, 0)->Id]-corresponding_CNFA->nVars, 0);
    clauses[1] = toLitCond(corresponding_CNFA->pVarNums[Aig_ManCo(AIG_circuit, 0)->Id], 0);
    RetValue = sat_solver_addclause( satsolver, clauses, clauses + 2 );
    if( RetValue == 0 ){
      //printf("symmetric \n");
      Abc_NtkForEachPi(pNtk, pPi, t) {
        printf("%d", corresponding_CNFA->pVarNums[pPi->Id]-corresponding_CNFA->nVars);
        printf("%d", corresponding_CNFA->pVarNums[pPi->Id]);
          
      }
      printf("\n");
      Cnf_DataFree(corresponding_CNFA);
      sat_solver_delete(satsolver);
      return;
    }

    clauses[0] = toLitCond(corresponding_CNFA->pVarNums[Aig_ManCo(AIG_circuit, 0)->Id]-corresponding_CNFA->nVars, 1);
    clauses[1] = toLitCond(corresponding_CNFA->pVarNums[Aig_ManCo(AIG_circuit, 0)->Id], 1);
    RetValue = sat_solver_addclause( satsolver, clauses, clauses + 2 );
    if( RetValue == 0 ){
      //printf("symmetric \n");
      Abc_NtkForEachPi(pNtk, pPi, t) {
        printf("%d", corresponding_CNFA->pVarNums[pPi->Id]-corresponding_CNFA->nVars);
        printf("%d", corresponding_CNFA->pVarNums[pPi->Id]);
          
      }
      printf("\n");
      Cnf_DataFree(corresponding_CNFA);
      sat_solver_delete(satsolver);
      return;
    }

    //
    if( t >= 0 || t < m ){
      if( t == ithPi || t == jthPi){
        //VH(i)=VH(j)=0


        RetValue = sat_solver_solve( satsolver, clauses, clauses + 2 , 0, 0, 0, 0);
        if( RetValue != 1  ){
          //printf("symmetric \n");
          Abc_NtkForEachPi(pNtk, pPi, t) {
            printf("%d", corresponding_CNFA->pVarNums[pPi->Id]-corresponding_CNFA->nVars);
            printf("%d", corresponding_CNFA->pVarNums[pPi->Id]);
              
          }
          printf("\n");
          Cnf_DataFree(corresponding_CNFA);
          sat_solver_delete(satsolver);
          return;
        }

        else{
          printf("asymmetric \n");
          Cnf_DataFree(corresponding_CNFA);
          sat_solver_delete(satsolver);
          return;

      
    }
  }  
}




int Lsv_SymmetryCheckingwithIncrementalSAT(Abc_Frame_t* pAbc, int argc, char** argv) {
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
  lsv_sym_all(pNtk, std::stoi(argv[1]));
  return 0;



usage:
  Abc_Print(-2, "usage: lsv_print_nodes [-h]\n");
  Abc_Print(-2, "\t        prints the nodes in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
  
}





