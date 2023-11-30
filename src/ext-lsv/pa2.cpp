#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"

#include <unordered_map>
#include <vector>
#include <iostream>
#include <fstream>
#include <map>
#include <string>
#include "sat/cnf/cnf.h"
extern "C"{
    Aig_Man_t* Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
}

using namespace std;

static int Lsv_CommandSymBdd(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSymSat(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSymAll(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_bdd", Lsv_CommandSymBdd, 0);
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_sat", Lsv_CommandSymSat, 0);
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_all", Lsv_CommandSymAll, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

void Lsv_NtkSymBdd(Abc_Ntk_t* pNtk, int out_k, int in_i, int in_j) {
  // std::cout<<"out_k: "<<out_k<<", in_i: "<<in_i<<", in_j: "<<in_j<<std::endl;
  if ( in_i == in_j ) {
    printf( "symmetric\n" );
    return;
  }

  Abc_Obj_t* pPo = Abc_NtkPo( pNtk, out_k );
  Abc_Obj_t* pRoot = Abc_ObjFanin0( pPo );
  DdManager * dd = ( DdManager* )pRoot->pNtk->pManFunc;
  DdNode* ddnode = ( DdNode* )pRoot->pData;

  int j;
  Abc_Obj_t *pPi;
  unordered_map<string, int> name2order;
  Abc_NtkForEachPi(pNtk, pPi, j) {
    name2order[Abc_ObjName(pPi)] = j;
  }

  int iF ;
  // find the input var number in bdd
  Abc_Obj_t* pFanin;
  int bdd_i = -1, bdd_j = -1;
  Abc_ObjForEachFanin(pRoot, pFanin, iF) {
    char * name_bdd = Abc_ObjName(pFanin);
    if ( Abc_ObjName( Abc_NtkPi( pNtk, in_i ) ) == name_bdd) {
      bdd_i = iF;
    }
    if ( Abc_ObjName( Abc_NtkPi( pNtk, in_j ) ) == name_bdd) {
      bdd_j = iF;
    }
  }
  //start 
  if (bdd_i == -1 && bdd_j == -1) {
    printf( "symmetric\n" );
    return;
  }
  else if (bdd_i == -1 && bdd_j != -1) {
    // printf( "bdd_i == -1 && bdd_j != -1\n" );
    // cofactor_0
    DdNode *cofactor_0;
    cofactor_0 = Cudd_Cofactor( dd, ddnode, Cudd_Not( Cudd_bddIthVar( dd, bdd_j ) ) );
    Cudd_Ref( cofactor_0 );

    // cofactor_1
    DdNode *cofactor_1;
    cofactor_1 = Cudd_Cofactor( dd, ddnode, Cudd_bddIthVar( dd, bdd_j ) );
    Cudd_Ref( cofactor_1 );

    DdNode *compare;
    compare = Cudd_bddXor( dd, cofactor_0, cofactor_1 );
    Cudd_Ref(compare);

    if ( Cudd_Not( compare ) == DD_ONE( dd ) ) {
      printf( "symmetric\n" );
      Cudd_RecursiveDeref( dd, cofactor_0 );
      Cudd_RecursiveDeref( dd, cofactor_1 );
      Cudd_RecursiveDeref( dd, compare );
      return;
    }
    else {
      printf( "asymmetric\n" );
      int output_counter_example[Abc_NtkPiNum( pNtk )];
      for (int i = 0; i < Abc_NtkPiNum( pNtk ); i++) {
        output_counter_example[i] = 0;
      }
      // int *counter_example = ABC_ALLOC( int, Abc_ObjFaninNum( pRoot ) );
      // double trash;
      int** counter_example = new(int*);
      double* trash = new(double);
      Cudd_FirstCube( dd, compare, counter_example, trash );
      char** order2name_bdd = ( char** )Abc_NodeGetFaninNames( pRoot )->pArray;
      for ( int k = 0; k < Abc_ObjFaninNum(pRoot) ; k++ ) {
        output_counter_example[name2order[order2name_bdd[k]]] = counter_example[0][k];
      }
      delete(counter_example);
      delete(trash);

      for (int k = 0; k < Abc_NtkPiNum(pNtk); k++) {
        if ( ( output_counter_example[k] != 0 ) && ( output_counter_example[k] != 1 ) ) {
          // printf( "what!!!!!!!!!!!!!\n");
          output_counter_example[k] = 0;
        }
      }
      // output
      output_counter_example[in_i] = 0;
      output_counter_example[in_j] = 1;
      for (int i = 0; i < Abc_NtkPiNum( pNtk ); i++) {
        printf( "%d", output_counter_example[i] );
      }
      printf( "\n" );
      output_counter_example[in_i] = 1;
      output_counter_example[in_j] = 0;
      for (int i = 0; i < Abc_NtkPiNum( pNtk ); i++) {
        printf( "%d", output_counter_example[i] );
      }
      printf( "\n" );
    }
    Cudd_RecursiveDeref( dd, cofactor_0 );
    Cudd_RecursiveDeref( dd, cofactor_1 );
    Cudd_RecursiveDeref( dd, compare );
  }
  else if (bdd_i != -1 && bdd_j == -1) {
    // printf( "bdd_i != -1 && bdd_j == -1\n" );
    // cofactor0_
    DdNode *cofactor0_;
    cofactor0_ = Cudd_Cofactor( dd, ddnode, Cudd_Not( Cudd_bddIthVar( dd, bdd_i ) ) );
    Cudd_Ref( cofactor0_ );

    // cofactor1_
    DdNode *cofactor1_;
    cofactor1_ = Cudd_Cofactor( dd, ddnode, Cudd_bddIthVar( dd, bdd_i ) );
    Cudd_Ref( cofactor1_ );

    DdNode *compare;
    compare = Cudd_bddXor( dd, cofactor0_, cofactor1_ );
    Cudd_Ref(compare);

    if ( Cudd_Not( compare ) == DD_ONE( dd ) ) {
      printf( "symmetric\n" );
      Cudd_RecursiveDeref( dd, cofactor0_ );
      Cudd_RecursiveDeref( dd, cofactor1_ );
      Cudd_RecursiveDeref( dd, compare );
      return;
    }
    else {
      printf( "asymmetric\n" );
      int output_counter_example[Abc_NtkPiNum( pNtk )];
      for (int i = 0; i < Abc_NtkPiNum( pNtk ); i++) {
        output_counter_example[i] = 0;
      }
      // int *counter_example = ABC_ALLOC( int, Abc_ObjFaninNum( pRoot ) );
      // double trash;
      int** counter_example = new(int*);
      double* trash = new(double);
      Cudd_FirstCube( dd, compare, counter_example, trash );
      char** order2name_bdd = ( char** )Abc_NodeGetFaninNames( pRoot )->pArray;
      for ( int k = 0; k < Abc_ObjFaninNum(pRoot) ; k++ ) {
        output_counter_example[name2order[order2name_bdd[k]]] = counter_example[0][k];
      }
      delete(counter_example);
      delete(trash);

      for (int k = 0; k < Abc_NtkPiNum(pNtk); k++) {
        if ( ( output_counter_example[k] != 0 ) && ( output_counter_example[k] != 1 ) ) {
          // printf( "what!!!!!!!!!!!!!\n");
          output_counter_example[k] = 0;
        }
      }
      // output
      output_counter_example[in_i] = 0;
      output_counter_example[in_j] = 1;
      for (int i = 0; i < Abc_NtkPiNum( pNtk ); i++) {
        printf( "%d", output_counter_example[i] );
      }
      printf( "\n" );
      output_counter_example[in_i] = 1;
      output_counter_example[in_j] = 0;
      for (int i = 0; i < Abc_NtkPiNum( pNtk ); i++) {
        printf( "%d", output_counter_example[i] );
      }
      printf( "\n" );
    }
    Cudd_RecursiveDeref( dd, cofactor0_ );
    Cudd_RecursiveDeref( dd, cofactor1_ );
    Cudd_RecursiveDeref( dd, compare );
  }
  else {
    // cofactor01
    DdNode *cofactor0;
    cofactor0 = Cudd_Cofactor( dd, ddnode, Cudd_Not( Cudd_bddIthVar( dd, bdd_i ) ) );
    Cudd_Ref( cofactor0 );

    DdNode *cofactor01 = NULL;
    cofactor01 = Cudd_Cofactor(dd, cofactor0, Cudd_bddIthVar(dd, bdd_j));
    Cudd_Ref( cofactor01 );

    Cudd_RecursiveDeref( dd, cofactor0 );

    // cofactor10
    DdNode *cofactor1;
    cofactor1 = Cudd_Cofactor( dd, ddnode, Cudd_bddIthVar( dd, bdd_i) );
    Cudd_Ref( cofactor1 );
    
    DdNode *cofactor10 = NULL;
    cofactor10 = Cudd_Cofactor(dd, cofactor1, Cudd_Not( Cudd_bddIthVar( dd, bdd_j ) ) );
    Cudd_Ref( cofactor10 );

    Cudd_RecursiveDeref( dd, cofactor1 );

    DdNode *compare;
    compare = Cudd_bddXor( dd, cofactor01, cofactor10 );
    Cudd_Ref(compare);

    if ( Cudd_Not( compare ) == DD_ONE( dd ) ) {
      printf( "symmetric\n" );
      Cudd_RecursiveDeref( dd, cofactor01 );
      Cudd_RecursiveDeref( dd, cofactor10 );
      Cudd_RecursiveDeref( dd, compare );
      return;
    }
    else {
      printf( "asymmetric\n" );
      int output_counter_example[Abc_NtkPiNum( pNtk )];
      for (int i = 0; i < Abc_NtkPiNum( pNtk ); i++) {
        output_counter_example[i] = 0;
      }
      // int *counter_example = ABC_ALLOC( int, Abc_ObjFaninNum( pRoot ) );
      // double trash;
      int** counter_example = new(int*);
      double* trash = new(double);
      Cudd_FirstCube( dd, compare, counter_example, trash );
      char** order2name_bdd = ( char** )Abc_NodeGetFaninNames( pRoot )->pArray;
      for ( int k = 0; k < Abc_ObjFaninNum(pRoot) ; k++ ) {
        output_counter_example[name2order[order2name_bdd[k]]] = counter_example[0][k];
      }
      delete(counter_example);
      delete(trash);

      for (int k = 0; k < Abc_NtkPiNum(pNtk); k++) {
        if ( ( output_counter_example[k] != 0 ) && ( output_counter_example[k] != 1 ) ) {
          // printf( "what!!!!!!!!!!!!!\n");
          output_counter_example[k] = 0;
        }
      }
      // output
      output_counter_example[in_i] = 0;
      output_counter_example[in_j] = 1;
      for (int i = 0; i < Abc_NtkPiNum( pNtk ); i++) {
        printf( "%d", output_counter_example[i] );
      }
      printf( "\n" );
      output_counter_example[in_i] = 1;
      output_counter_example[in_j] = 0;
      for (int i = 0; i < Abc_NtkPiNum( pNtk ); i++) {
        printf( "%d", output_counter_example[i] );
      }
      printf( "\n" );
    }
    Cudd_RecursiveDeref( dd, cofactor01 );
    Cudd_RecursiveDeref( dd, cofactor10 );
    Cudd_RecursiveDeref( dd, compare );
  }
  return;
}


int Lsv_CommandSymBdd(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
//   int c;
  Extra_UtilGetoptReset();

  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  int k = atoi(argv[1]);
  int i = atoi(argv[2]);
  int j = atoi(argv[3]);
  Lsv_NtkSymBdd(pNtk, k, i, j);
  return 0;
}

void Lsv_NtkSymSat(Abc_Ntk_t* pNtk, int out_k, int in_i, int in_j) {
  // printf("go\n");
  Abc_Ntk_t *cone_yk;
  Abc_Obj_t *pRoot = Abc_ObjFanin0( Abc_NtkPo(pNtk, out_k) );
  sat_solver *sat;
  Aig_Man_t *aig_ckt;
  Cnf_Dat_t *cnf1, *cnf2;
  int cls[2];

  // step1
  cone_yk = Abc_NtkCreateCone( pNtk, pRoot, Abc_ObjName(Abc_NtkPo(pNtk, out_k)), 1 );
  // step2
  aig_ckt = Abc_NtkToDar( cone_yk, 0, 1 );
  // step3
  sat = sat_solver_new();
  // step4
  cnf1 = Cnf_Derive( aig_ckt, 1 );
  // step5
  Cnf_DataWriteIntoSolverInt(sat, cnf1, 1, 0);
  // step6
  cnf2 = Cnf_Derive( aig_ckt, 1 );
  Cnf_DataLift( cnf2, cnf1->nVars );
  Cnf_DataWriteIntoSolverInt( sat, cnf2, 1, 0 );

   // step7
  Aig_Obj_t *pObj;
  int ith;
  Aig_ManForEachCi(aig_ckt, pObj, ith) {
    if ( (in_i != ith) && (in_j != ith) ) {
      cls[0] = toLitCond(cnf1->pVarNums[pObj->Id], 0);
      cls[1] = toLitCond(cnf2->pVarNums[pObj->Id], 1);
      sat_solver_addclause( sat, cls, cls + 2 );
      cls[0] = toLitCond(cnf1->pVarNums[pObj->Id], 1);
      cls[1] = toLitCond(cnf2->pVarNums[pObj->Id], 0);
      sat_solver_addclause( sat, cls, cls + 2 );
    }
  }

  cls[0] = toLitCond(cnf1->pVarNums[Aig_ManCi( aig_ckt, in_i )->Id], 0);
  cls[1] = toLitCond(cnf1->pVarNums[Aig_ManCi( aig_ckt, in_j )->Id], 0);
  sat_solver_addclause( sat, cls, cls + 2 );
  cls[0] = toLitCond(cnf1->pVarNums[Aig_ManCi( aig_ckt, in_i )->Id], 1);
  cls[1] = toLitCond(cnf1->pVarNums[Aig_ManCi( aig_ckt, in_j )->Id], 1);
  sat_solver_addclause( sat, cls, cls + 2 );

  cls[0] = toLitCond(cnf2->pVarNums[Aig_ManCi( aig_ckt, in_i )->Id], 0);
  cls[1] = toLitCond(cnf2->pVarNums[Aig_ManCi( aig_ckt, in_j )->Id], 0);
  sat_solver_addclause( sat, cls, cls + 2 );
  cls[0] = toLitCond(cnf2->pVarNums[Aig_ManCi( aig_ckt, in_i )->Id], 1);
  cls[1] = toLitCond(cnf2->pVarNums[Aig_ManCi( aig_ckt, in_j )->Id], 1);
  sat_solver_addclause( sat, cls, cls + 2 );

  cls[0] = toLitCond(cnf1->pVarNums[Aig_ManCi( aig_ckt, in_i )->Id], 0);
  cls[1] = toLitCond(cnf2->pVarNums[Aig_ManCi( aig_ckt, in_i )->Id], 0);
  sat_solver_addclause( sat, cls, cls + 2 );
  cls[0] = toLitCond(cnf1->pVarNums[Aig_ManCi( aig_ckt, in_i )->Id], 1);
  cls[1] = toLitCond(cnf2->pVarNums[Aig_ManCi( aig_ckt, in_i )->Id], 1);
  sat_solver_addclause( sat, cls, cls + 2 );

  cls[0] = toLitCond(cnf1->pVarNums[Aig_ManCi( aig_ckt, in_j )->Id], 0);
  cls[1] = toLitCond(cnf2->pVarNums[Aig_ManCi( aig_ckt, in_j )->Id], 0);
  sat_solver_addclause( sat, cls, cls + 2 );
  cls[0] = toLitCond(cnf1->pVarNums[Aig_ManCi( aig_ckt, in_j )->Id], 1);
  cls[1] = toLitCond(cnf2->pVarNums[Aig_ManCi( aig_ckt, in_j )->Id], 1);
  sat_solver_addclause( sat, cls, cls + 2 );

  Aig_ManForEachCo(aig_ckt, pObj, ith) {
    // if ( ith == 0 ) {
    cls[0] = toLitCond( cnf1->pVarNums[pObj->Id], 0 );
    cls[1] = toLitCond( cnf2->pVarNums[pObj->Id], 0 );
    sat_solver_addclause( sat, cls, cls + 2 );
    cls[0] = toLitCond( cnf1->pVarNums[pObj->Id], 1 );
    cls[1] = toLitCond( cnf2->pVarNums[pObj->Id], 1 );
    sat_solver_addclause( sat, cls, cls + 2 );
    // }
  }

  // step8
  int answer = sat_solver_solve( sat, NULL, NULL, 0, 0, 0, 0 );

  // step9
  if ( answer == -1 ) {
    printf("symmetric\n");
    return;
  }
  else {
    printf("asymmetric\n");
    Aig_ManForEachCi(aig_ckt, pObj, ith) {
      printf("%d", sat_solver_var_value( sat, cnf1->pVarNums[pObj->Id] ) );
    }
    printf( "\n" );

    Aig_ManForEachCi(aig_ckt, pObj, ith) {
      printf("%d", sat_solver_var_value( sat, cnf2->pVarNums[pObj->Id] ) );
    }
    printf( "\n" );
    return;
  }
  return;
}


int Lsv_CommandSymSat(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
//   int c;
  Extra_UtilGetoptReset();

  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  int k = atoi(argv[1]);
  int i = atoi(argv[2]);
  int j = atoi(argv[3]);
  Lsv_NtkSymSat(pNtk, k, i, j);
  return 0;
}













void Lsv_NtkSymAll( Abc_Ntk_t* pNtk, int out_k ) {
  // printf( "out_k: %d\n", out_k );
  Abc_Ntk_t *cone_yk;
  Abc_Obj_t *pRoot = Abc_ObjFanin0( Abc_NtkPo(pNtk, out_k) );
  sat_solver *sat;
  Aig_Man_t *aig_ckt;
  Cnf_Dat_t *cnf1, *cnf2, *cnfh;
  int cls[2];
  int clsh[3];
  int cls4[4];

  // step1
  cone_yk = Abc_NtkCreateCone( pNtk, pRoot, Abc_ObjName(Abc_NtkPo(pNtk, out_k)), 1 );
  // step2
  aig_ckt = Abc_NtkToDar( cone_yk, 0, 1 );
  // step3
  sat = sat_solver_new();
  // step4
  cnf1 = Cnf_Derive( aig_ckt, 1 );
  // step5
  Cnf_DataWriteIntoSolverInt(sat, cnf1, 1, 0);
  // step6
  cnf2 = Cnf_Derive( aig_ckt, 1 );
  Cnf_DataLift( cnf2, cnf1->nVars );
  Cnf_DataWriteIntoSolverInt( sat, cnf2, 1, 0 );

  cnfh = Cnf_Derive( aig_ckt, 1 );
  Cnf_DataLift( cnfh, cnf2->nVars * 2 );
  Cnf_DataWriteIntoSolverInt( sat, cnfh, 1, 0 );


  Aig_Obj_t *pObj;
  int ith;

  cls[0] = toLitCond( cnf1->pVarNums[Aig_ManCo( aig_ckt, 0 )->Id], 0 );
  cls[1] = toLitCond( cnf2->pVarNums[Aig_ManCo( aig_ckt, 0 )->Id], 0 );
  sat_solver_addclause( sat, cls, cls + 2 );
  cls[0] = toLitCond( cnf1->pVarNums[Aig_ManCo( aig_ckt, 0 )->Id], 1 );
  cls[1] = toLitCond( cnf2->pVarNums[Aig_ManCo( aig_ckt, 0 )->Id], 1 );
  sat_solver_addclause( sat, cls, cls + 2 );

  Aig_ManForEachCi(aig_ckt, pObj, ith) {
      clsh[0] = toLitCond( cnf1->pVarNums[pObj->Id], 0 );
      clsh[1] = toLitCond( cnf2->pVarNums[pObj->Id], 1 );
      clsh[2] = toLitCond( cnfh->pVarNums[pObj->Id], 0 );
      sat_solver_addclause( sat, clsh, clsh + 3 );
      clsh[0] = toLitCond( cnf1->pVarNums[pObj->Id], 1 );
      clsh[1] = toLitCond( cnf2->pVarNums[pObj->Id], 0 );
      clsh[2] = toLitCond( cnfh->pVarNums[pObj->Id], 0 );
      sat_solver_addclause( sat, clsh, clsh + 3 );

      clsh[0] = toLitCond( cnf1->pVarNums[pObj->Id], 0 );
      clsh[1] = toLitCond( cnf2->pVarNums[pObj->Id], 0 );
      clsh[2] = toLitCond( cnfh->pVarNums[pObj->Id], 1 );
      sat_solver_addclause( sat, clsh, clsh + 3 );
      clsh[0] = toLitCond( cnf1->pVarNums[pObj->Id], 1 );
      clsh[1] = toLitCond( cnf2->pVarNums[pObj->Id], 1 );
      clsh[2] = toLitCond( cnfh->pVarNums[pObj->Id], 1 );
      sat_solver_addclause( sat, clsh, clsh + 3 );
  }


  for ( int i = 0; i < ( int )Aig_ManCiNum( aig_ckt ) - 1; i++ ) {
    for ( int j = i + 1; j < ( int )Aig_ManCiNum(aig_ckt); j++ ) {
      int cnf1_i = cnf1->pVarNums[Aig_ManCi(aig_ckt, i)->Id];
      int cnf1_j = cnf1->pVarNums[Aig_ManCi(aig_ckt, j)->Id];
      int cnf2_i = cnf2->pVarNums[Aig_ManCi(aig_ckt, i)->Id];
      int cnf2_j = cnf2->pVarNums[Aig_ManCi(aig_ckt, j)->Id];
      int cnfh_i = cnfh->pVarNums[Aig_ManCi(aig_ckt, i)->Id];
      int cnfh_j = cnfh->pVarNums[Aig_ManCi(aig_ckt, j)->Id];

      cls4[0] = toLitCond( cnf1_j, 0 );
      cls4[1] = toLitCond( cnf2_i, 1 );
      cls4[2] = toLitCond( cnfh_i, 1 );
      cls4[3] = toLitCond( cnfh_j, 1 );
      sat_solver_addclause( sat, cls4, cls4 + 4 );

      cls4[0] = toLitCond( cnf1_j, 1 );
      cls4[1] = toLitCond( cnf2_i, 0 );
      cls4[2] = toLitCond( cnfh_i, 1 );
      cls4[3] = toLitCond( cnfh_j, 1 );
      sat_solver_addclause( sat, cls4, cls4 + 4 );

      cls4[0] = toLitCond( cnf1_i, 0 );
      cls4[1] = toLitCond( cnf2_j, 1 );
      cls4[2] = toLitCond( cnfh_i, 1 );
      cls4[3] = toLitCond( cnfh_j, 1 );
      sat_solver_addclause( sat, cls4, cls4 + 4 );

      cls4[0] = toLitCond( cnf1_i, 1 );
      cls4[1] = toLitCond( cnf2_j, 0 );
      cls4[2] = toLitCond( cnfh_i, 1 );
      cls4[3] = toLitCond( cnfh_j, 1 );
      sat_solver_addclause( sat, cls4, cls4 + 4 );
    }
  }

  for( int i = 0; i < ( int )Aig_ManCiNum( aig_ckt ) - 1; i++ ){
    for( int j = i + 1; j < ( int )Aig_ManCiNum( aig_ckt ); j++ ){
      // cout<<"try"<<endl;
      lit try_cls[] = {toLitCond( cnfh->pVarNums[Aig_ManCi( aig_ckt, i )->Id], 0 ), 
                 toLitCond( cnfh->pVarNums[Aig_ManCi( aig_ckt, j )->Id], 0 )};
      
      int answer = sat_solver_solve( sat, try_cls, try_cls + 2, 0, 0, 0, 0);
      if( answer == -1 ){
        printf( "%d %d\n", i, j );
      }
    }
  }

  return;
}

int Lsv_CommandSymAll( Abc_Frame_t* pAbc, int argc, char** argv ) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
//   int c;
  Extra_UtilGetoptReset();

  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  int k = atoi( argv[1] );
  Lsv_NtkSymAll( pNtk, k );
  return 0;
}


