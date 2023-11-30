#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <vector>
#include <iostream>
#include <algorithm>
#include <string>
#include <fstream>
using namespace std;

#include "sat/cnf/cnf.h"
extern "C"{Aig_Man_t* Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );}

static int Lsv_CommandSymSat(Abc_Frame_t* pAbc, int argc, char** argv);

void init_sat(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_sat", Lsv_CommandSymSat, 0);
}

void destroy_sat(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer_sat = {init_sat, destroy_sat};

struct PackageRegistrationManager_sat {
  PackageRegistrationManager_sat() { Abc_FrameAddInitializer(&frame_initializer_sat); }
} lsvPackageRegistrationManager_sat;


void Lsv_NtkSymSat(Abc_Ntk_t* pNtk, int output_index, int input_index_1, int input_index_2){

  Abc_Obj_t* output_node = Abc_NtkPo(pNtk, output_index);
  Abc_Ntk_t* pCone = Abc_NtkCreateCone(pNtk, Abc_ObjFanin0(output_node), Abc_ObjName(output_node), 1);

  int first_test;
  Abc_Obj_t* all_input_node;
  Abc_NtkForEachObj(pCone, all_input_node, first_test){
    // std::cout << "Id: " << all_input_node->Id << endl;
    // std::cout << "Name: " << Abc_ObjName(all_input_node) << endl;
    // std::cout << "Type: " << all_input_node->Type << endl << endl;
  }

  // 如果兩個input都不在fanin of output，那就一定symmetric
  // check_pi_1 is false, 則pi_1不在fanin of output裡面
  Abc_Obj_t* pi_1 = Abc_NtkPi(pNtk, input_index_1);
  Abc_Obj_t* pi_2 = Abc_NtkPi(pNtk, input_index_2);
  bool check_pi_1 = false, check_pi_2 = false;
  Abc_Obj_t* pi_all;
  int no_use;
  Abc_NtkForEachPi(pCone, pi_all, no_use) {
    if (pi_all->Id == pi_1->Id){check_pi_1 = true;}
    if (pi_all->Id == pi_2->Id){check_pi_2 = true;}
  }
  if ((!check_pi_1) && (!check_pi_2)){std::cout<<"symmetric\n"; return;}

  Aig_Man_t * pMan_1 = Abc_NtkToDar( pCone, 0, 1 );
  sat_solver* sat = sat_solver_new();
  Cnf_Dat_t* cnf_1 = Cnf_Derive(pMan_1, 1);
  Cnf_DataWriteIntoSolverInt(sat, cnf_1, 1, 0);
  int lift_later = cnf_1->nVars;
  // int lift_later = sat->size;
  // std::cout << "First CNF in sat: " << sat->size << endl;

  // second copy of total circuit
  Aig_Man_t * pMan_2 = Abc_NtkToDar( pCone, 0, 1 );
  Cnf_Dat_t* cnf_2 = Cnf_Derive(pMan_2, 1);
  Cnf_DataLift(cnf_2, lift_later);
  Cnf_DataWriteIntoSolverInt(sat, cnf_2, 1, 0);
  // std::cout << "Second CNF in sat: " << sat->size << endl;

  // add clause to the SAT-solver
  Abc_Obj_t* pi_cone;
  int temp_cnf_index_1_1=0, temp_cnf_index_1_2=0, temp_cnf_index_2_1=0, temp_cnf_index_2_2=0;
  Abc_NtkForEachPi(pCone, pi_cone, no_use) {
    int index1 = cnf_1->pVarNums[pi_cone->Id];
    int index2 = cnf_2->pVarNums[pi_cone->Id];
    // std::cout << Abc_ObjName(pi_cone) << endl;
    if ((strcmp(Abc_ObjName(pi_cone), Abc_ObjName(Abc_NtkPi(pNtk, input_index_1))) != 0) && (strcmp(Abc_ObjName(pi_cone), Abc_ObjName(Abc_NtkPi(pNtk, input_index_2))) != 0)){
      // std::cout << "VA(t)=VB(t)\n";
      // add clause a=b, (a+~b)*(~a+b)
      lit Lits[2];
      int Cid;

      Lits[0] = toLitCond( index1, 1 );
      Lits[1] = toLitCond( index2, 0 );
      Cid = sat_solver_addclause( sat, Lits, Lits + 2 );
      assert( Cid );

      Lits[0] = toLitCond( index1, 0 );
      Lits[1] = toLitCond( index2, 1 );
      Cid = sat_solver_addclause( sat, Lits, Lits + 2 );
      assert( Cid );
    }
    else if ((!check_pi_1) || (!check_pi_2)){
      // std::cout << "only one care input in the fanin of care output\n";
      // add clause a xor b, (~a+~b)*(a+b)
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
    }
    else{
      if (strcmp(Abc_ObjName(pi_cone), Abc_ObjName(Abc_NtkPi(pNtk, input_index_1))) != 0){
        temp_cnf_index_1_1 = index1;
        temp_cnf_index_1_2 = index2;
      }
      else{
        temp_cnf_index_2_1 = index1;
        temp_cnf_index_2_2 = index2;
      }
    }
  }

  if (check_pi_1 && check_pi_2){
    // std::cout << "VA(i)=VB(j), VA(j)=VB(i)\n";
    // std::cout << "temp_cnf_index_1_1: " << temp_cnf_index_1_1 << endl;
    // std::cout << "temp_cnf_index_1_2: " << temp_cnf_index_1_2 << endl;
    // std::cout << "temp_cnf_index_2_1: " << temp_cnf_index_2_1 << endl;
    // std::cout << "temp_cnf_index_2_2: " << temp_cnf_index_2_2 << endl;
    // add clause {VA(i)=VB(j), VA(j)=VB(i)}
    lit Lits[2];
    int Cid;

    Lits[0] = toLitCond( temp_cnf_index_1_1, 0 );
    Lits[1] = toLitCond( temp_cnf_index_2_2, 1 );
    Cid = sat_solver_addclause( sat, Lits, Lits + 2 );
    assert( Cid );

    Lits[0] = toLitCond( temp_cnf_index_1_1, 1 );
    Lits[1] = toLitCond( temp_cnf_index_2_2, 0 );
    Cid = sat_solver_addclause( sat, Lits, Lits + 2 );
    assert( Cid );

    Lits[0] = toLitCond( temp_cnf_index_1_2, 0 );
    Lits[1] = toLitCond( temp_cnf_index_2_1, 1 );
    Cid = sat_solver_addclause( sat, Lits, Lits + 2 );
    assert( Cid );

    Lits[0] = toLitCond( temp_cnf_index_1_2, 1 );
    Lits[1] = toLitCond( temp_cnf_index_2_1, 0 );
    Cid = sat_solver_addclause( sat, Lits, Lits + 2 );
    assert( Cid );
  }
  // add clause {VA(yk) xor VB(yk)}
  // std::cout << "VA(yk) xor VB(yk)\n";
  int index1 = cnf_1->pVarNums[Abc_ObjFanin0(Abc_NtkPo(pCone, 0))->Id];
  int index2 = cnf_2->pVarNums[Abc_ObjFanin0(Abc_NtkPo(pCone, 0))->Id];
  // std::cout << "output_index 1: " << index1 << endl;
  // std::cout << "output_index 2: " << index2 << endl;
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

  // std::cout << "Start solve the sat.\n";
  // solve
  int sat_result = sat_solver_solve(sat, NULL, NULL, 0, 0, 0, 0 );
  // std::cout << "sat result: " << sat_result << endl;
  if (sat_result == -1){std::cout<<"symmetric\n"; return;}
  else{
/*
    int * result_2, temp_final;
    temp_final = sat_solver_final(sat, &result_2);
    for (int i=0; i<sizeof(result_2); i++){
      std::cout << result_2[i] << "\t";
    }
    std::cout << endl;
*/ 
    std::cout<<"asymmetric\n";
    vector<int> satisfying_assignment;
    for (int i=0; i<sat->size; i++){
      satisfying_assignment.push_back(sat_solver_var_value(sat, i));
    }
    Abc_Obj_t* for_output_total, *for_output_cone;
    int for_output_idx_total, for_output_idx_cone;
    vector<bool> pi_of_cone_to_total(Abc_NtkPiNum(pNtk), false);
    Abc_NtkForEachPi(pNtk, for_output_total, for_output_idx_total){
      Abc_NtkForEachPi(pNtk, for_output_cone, for_output_idx_cone){
        if (strcmp(Abc_ObjName(for_output_total), Abc_ObjName(for_output_cone)) == 0){
          pi_of_cone_to_total[for_output_idx_total] = true;
        }
      }
    }
    Abc_NtkForEachPi(pNtk, for_output_total, for_output_idx_total){
      if (! pi_of_cone_to_total[for_output_idx_total]){std::cout << "0";}
      else {std::cout << satisfying_assignment[cnf_1->pVarNums[for_output_total->Id]];}
    }
    std::cout << "\n";
    Abc_NtkForEachPi(pNtk, for_output_total, for_output_idx_total){
      if (! pi_of_cone_to_total[for_output_idx_total]){std::cout << "0";}
      else {std::cout << satisfying_assignment[cnf_2->pVarNums[for_output_total->Id]];}
    }
    std::cout << "\n";
    //for (int i=0; i<satisfying_assignment.size(); i++){std::cout << satisfying_assignment[i] << " ";}
    
  }
}

int Lsv_CommandSymSat(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  if (!Abc_NtkIsStrash(pNtk)){
    Abc_Print(-1, "Only use this command when the current circuit is in AIG type. (run strash)\n");
    return 1;
  }

  int output_index = atoi(argv[1]), input_index_1 = atoi(argv[2]), input_index_2 = atoi(argv[3]);
  Lsv_NtkSymSat(pNtk, output_index, input_index_1, input_index_2);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sym_sat [-h]\n");
  Abc_Print(-2, "\t        enter lsv_sym_sat <k> <i> <j>, which k is the output pin index, (i,j) are the input index.\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}
