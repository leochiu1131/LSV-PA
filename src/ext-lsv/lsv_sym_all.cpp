#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <vector>
#include <iostream>
#include <algorithm>
#include <string>
#include <fstream>
#include<time.h>
using namespace std;

#include "sat/cnf/cnf.h"
extern "C"{Aig_Man_t* Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );}

static int Lsv_CommandSymAll(Abc_Frame_t* pAbc, int argc, char** argv);

void init_all(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_all", Lsv_CommandSymAll, 0);
}

void destroy_all(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer_all = {init_all, destroy_all};

struct PackageRegistrationManager_all {
  PackageRegistrationManager_all() { Abc_FrameAddInitializer(&frame_initializer_all); }
} lsvPackageRegistrationManager_all;


void Lsv_NtkSymAll(Abc_Ntk_t* pNtk, int output_index){
/*
  clock_t time_start, time_end;
  time_start = clock();
*/
  Abc_Obj_t* output_node = Abc_NtkPo(pNtk, output_index);
  Abc_Ntk_t* pCone = Abc_NtkCreateCone(pNtk, Abc_ObjFanin0(output_node), Abc_ObjName(output_node), 1);
  Aig_Man_t * pMan_1 = Abc_NtkToDar( pCone, 0, 0 );
  sat_solver* sat = sat_solver_new();
  Cnf_Dat_t* cnf_1 = Cnf_Derive(pMan_1, 1);
  Cnf_DataWriteIntoSolverInt(sat, cnf_1, 1, 0);
  int lift_later = cnf_1->nVars;
  // second copy of total circuit
  Aig_Man_t * pMan_2 = Abc_NtkToDar( pCone, 0, 0 );
  Cnf_Dat_t* cnf_2 = Cnf_Derive(pMan_2, 1);
  Cnf_DataLift(cnf_2, lift_later);
  Cnf_DataWriteIntoSolverInt(sat, cnf_2, 1, 0);

  // add clause to the SAT-solver
  // (a) add clause {VA(yk) xor VB(yk)}
  int index1 = cnf_1->pVarNums[Abc_ObjFanin0(Abc_NtkPo(pCone, 0))->Id];
  int index2 = cnf_2->pVarNums[Abc_ObjFanin0(Abc_NtkPo(pCone, 0))->Id];
  lit Lits[2];
  int Cid;
  Lits[0] = toLitCond( index1, 1 );
  Lits[1] = toLitCond( index2, 1 );
  Cid = sat_solver_addclause( sat, Lits, Lits + 2 );
  assert( Cid );
  Lits[0] = toLitCond( index1, 0 );
  Lits[1] = toLitCond( index2, 0 );
  Cid = sat_solver_addclause( sat, Lits, Lits + 2 );
  assert( Cid );

  // (b)
  vector<int> incremental_variable;
  int no_use;
  Abc_Obj_t* pi_cone;
  Abc_NtkForEachPi(pCone, pi_cone, no_use) {
    int index1 = cnf_1->pVarNums[pi_cone->Id];
    int index2 = cnf_2->pVarNums[pi_cone->Id];
    int index3 = sat_solver_addvar(sat);
    incremental_variable.push_back(index3);
    lit Lits[3];
    int Cid;

    Lits[0] = toLitCond( index1, 1 );
    Lits[1] = toLitCond( index2, 0 );
    Lits[2] = toLitCond( index3, 0 );
    Cid = sat_solver_addclause( sat, Lits, Lits + 3 );
    assert( Cid );

    Lits[0] = toLitCond( index1, 0 );
    Lits[1] = toLitCond( index2, 1 );
    Lits[2] = toLitCond( index3, 0 );
    Cid = sat_solver_addclause( sat, Lits, Lits + 3 );
    assert( Cid );
  }

  // (c) & (d)
  for (int idx=0; idx<Abc_NtkPiNum(pCone); idx++){
    for (int jdx=idx+1; jdx<Abc_NtkPiNum(pCone); jdx++){
      int index1 = cnf_1->pVarNums[Abc_NtkPi(pCone, idx)->Id];
      int index2 = cnf_2->pVarNums[Abc_NtkPi(pCone, jdx)->Id];
      int index3 = cnf_1->pVarNums[Abc_NtkPi(pCone, jdx)->Id];
      int index4 = cnf_2->pVarNums[Abc_NtkPi(pCone, idx)->Id];
      lit Lits[4];
      int Cid;

      Lits[0] = toLitCond( index1, 1 );
      Lits[1] = toLitCond( index2, 0 );
      Lits[2] = toLitCond( incremental_variable[idx], 1 );
      Lits[3] = toLitCond( incremental_variable[jdx], 1 );
      Cid = sat_solver_addclause( sat, Lits, Lits + 4 );
      assert( Cid );

      Lits[0] = toLitCond( index1, 0 );
      Lits[1] = toLitCond( index2, 1 );
      Lits[2] = toLitCond( incremental_variable[idx], 1 );
      Lits[3] = toLitCond( incremental_variable[jdx], 1 );
      Cid = sat_solver_addclause( sat, Lits, Lits + 4 );
      assert( Cid );

      Lits[0] = toLitCond( index3, 1 );
      Lits[1] = toLitCond( index4, 0 );
      Lits[2] = toLitCond( incremental_variable[idx], 1 );
      Lits[3] = toLitCond( incremental_variable[jdx], 1 );
      Cid = sat_solver_addclause( sat, Lits, Lits + 4 );
      assert( Cid );

      Lits[0] = toLitCond( index3, 0 );
      Lits[1] = toLitCond( index4, 1 );
      Lits[2] = toLitCond( incremental_variable[idx], 1 );
      Lits[3] = toLitCond( incremental_variable[jdx], 1 );
      Cid = sat_solver_addclause( sat, Lits, Lits + 4 );
      assert( Cid );

    }
  }

  for(int idx = 0; idx<Abc_NtkPiNum(pCone); idx++){
    for(int jdx = idx+1; jdx<Abc_NtkPiNum(pCone); jdx++){
      lit select[2];
      select[0] = toLitCond(incremental_variable[idx], 0);
      select[1] = toLitCond(incremental_variable[jdx], 0);
      int sat_result = sat_solver_solve(sat, select, select+2, 0, 0, 0, 0);
      if(sat_result == l_False){
        std::cout<<idx<<" "<<jdx<<std::endl;
      }
    }
  }

/*
  time_end = clock();
  std::cout << "Start time: " << time_start << std::endl;
  std::cout << "End time: " << time_end << std::endl;
  std::cout << "Pass time: " << (time_end - time_start) << std::endl;
*/
  return;
}

int Lsv_CommandSymAll(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  if (!Abc_NtkIsStrash(pNtk)){
    Abc_Print(-1, "Only use this command when the current circuit is in AIG type. (run strash)\n");
    return 1;
  }

  int output_index = atoi(argv[1]);
  Lsv_NtkSymAll(pNtk, output_index);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sym_all [-h]\n");
  Abc_Print(-2, "\t        enter lsv_sym_all <k>, which k is the output pin index.\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}
