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

static int Lsv_CommandSymBdd(Abc_Frame_t* pAbc, int argc, char** argv);

void init_sym_bdd(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_bdd", Lsv_CommandSymBdd, 0);
}

void destroy_sym_bdd(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer_sym_bdd = {init_sym_bdd, destroy_sym_bdd};

struct PackageRegistrationManager_sym_bdd {
  PackageRegistrationManager_sym_bdd() { Abc_FrameAddInitializer(&frame_initializer_sym_bdd); }
} lsvPackageRegistrationManager_sym_bdd;

bool get_satisfy (DdManager* dd_root, DdNode* dd_root_node, string& result, int current_idx){
  // std::cout << "current_idx: " << current_idx << endl;
  if ((dd_root_node == DD_ONE(dd_root)) || (dd_root_node == Cudd_ReadOne(dd_root)) || (dd_root_node == Cudd_Not(DD_ZERO(dd_root))) || (dd_root_node == Cudd_Not(Cudd_ReadZero(dd_root)))){return true;}
  if ((dd_root_node == DD_ZERO(dd_root)) || (dd_root_node == Cudd_ReadZero(dd_root)) || (dd_root_node == Cudd_Not(DD_ONE(dd_root))) || (dd_root_node == Cudd_Not(Cudd_ReadOne(dd_root)))){return false;}

  DdNode *current_variable = Cudd_bddIthVar(dd_root, current_idx);
  Cudd_Ref(current_variable);
  DdNode *cofactor = Cudd_Cofactor(dd_root, dd_root_node, current_variable);
  Cudd_Ref(cofactor);
  if (get_satisfy(dd_root, cofactor, result, current_idx+1)){
    result[current_idx] = '1';
    Cudd_RecursiveDeref(dd_root, current_variable);
    Cudd_RecursiveDeref(dd_root, cofactor);
    return true;
  }
  Cudd_RecursiveDeref(dd_root, cofactor);
  cofactor = Cudd_Cofactor(dd_root, dd_root_node, Cudd_Not(current_variable));
  Cudd_Ref(cofactor);
  if (get_satisfy(dd_root, cofactor, result, current_idx+1)){
    result[current_idx] = '0';
    Cudd_RecursiveDeref(dd_root, current_variable);
    Cudd_RecursiveDeref(dd_root, cofactor);
    return true;
  }
  Cudd_RecursiveDeref(dd_root, current_variable);
  Cudd_RecursiveDeref(dd_root, cofactor);
  return false;
}






void Lsv_NtkSymBdd(Abc_Ntk_t* pNtk, int output_index, int input_index_1, int input_index_2) {
  if (! Abc_NtkIsBddLogic(pNtk)){
    std::cout<<"Please use this function when the circuit is at BDD form!\n";
  }
  else{
    //record the order in the original circuit
    vector<string> ori_name(Abc_NtkPiNum(pNtk));
    Abc_Obj_t* temp_pi_node;
    int temp_pi_idx;
    Abc_NtkForEachPi(pNtk, temp_pi_node, temp_pi_idx){
      ori_name[temp_pi_idx] = Abc_ObjName(temp_pi_node);}

    Abc_Obj_t* output_node = Abc_ObjFanin0(Abc_NtkPo(pNtk, output_index));
    assert(Abc_NtkIsBddLogic(output_node->pNtk));
    DdManager* dd_root = (DdManager*)output_node->pNtk->pManFunc;
    DdNode* dd_root_node = (DdNode*)output_node->pData;

    // bdd variable order
    char** bdd_order = (char**)Abc_NodeGetFaninNames(output_node)->pArray;
    int bdd_variable_size = Abc_ObjFaninNum(output_node), in1_order_in_bdd = -1, in2_order_in_bdd = -1;
    for (int idx=0; idx<bdd_variable_size; idx++){
      if (strcmp(bdd_order[idx], Abc_ObjName(Abc_NtkPi(pNtk, input_index_1))) == 0){in1_order_in_bdd = idx;}
      if (strcmp(bdd_order[idx], Abc_ObjName(Abc_NtkPi(pNtk, input_index_2))) == 0){in2_order_in_bdd = idx;}
    }
    // std::cout << "in1_order_in_bdd: " << in1_order_in_bdd << endl;
    // std::cout << "in2_order_in_bdd: " << in2_order_in_bdd << endl;

    DdNode *f_01=NULL, *f_10=NULL, *temp_bdd_in1, *temp_bdd_in2, *temp_cofactor, *f_xor;
    // 如果兩個input都不在fanin of output，那就一定symmetric
    if ((in1_order_in_bdd == -1) && (in2_order_in_bdd == -1)){std::cout<<"symmetric\n"; return;}
    if ((in1_order_in_bdd == -1) || (in2_order_in_bdd == -1)){
      // 只有一個input在fanin of output
      if (in1_order_in_bdd != -1){
        f_xor = Cudd_bddBooleanDiff(dd_root, dd_root_node, in1_order_in_bdd);
        Cudd_Ref(f_xor);
      }
      else{
        f_xor = Cudd_bddBooleanDiff(dd_root, dd_root_node, in2_order_in_bdd);
        Cudd_Ref(f_xor);
      }

    }
    else{
      // f_01
      temp_bdd_in1 = Cudd_Not(Cudd_bddIthVar(dd_root, in1_order_in_bdd));
      temp_bdd_in2 = Cudd_bddIthVar(dd_root, in2_order_in_bdd);
      Cudd_Ref(temp_bdd_in1);
      Cudd_Ref(temp_bdd_in2);
      temp_cofactor = Cudd_Cofactor(dd_root, dd_root_node, temp_bdd_in1);
      Cudd_Ref(temp_cofactor);
      f_01 = Cudd_Cofactor(dd_root, temp_cofactor, temp_bdd_in2);
      Cudd_Ref(f_01);
      Cudd_RecursiveDeref(dd_root, temp_bdd_in1);
      Cudd_RecursiveDeref(dd_root, temp_bdd_in2);
      Cudd_RecursiveDeref(dd_root, temp_cofactor);

      // f_10
      temp_bdd_in1 = Cudd_bddIthVar(dd_root, in1_order_in_bdd);
      temp_bdd_in2 = Cudd_Not(Cudd_bddIthVar(dd_root, in2_order_in_bdd));
      Cudd_Ref(temp_bdd_in1);
      Cudd_Ref(temp_bdd_in2);
      temp_cofactor = Cudd_Cofactor(dd_root, dd_root_node, temp_bdd_in1);
      Cudd_Ref(temp_cofactor);
      f_10 = Cudd_Cofactor(dd_root, temp_cofactor, temp_bdd_in2);
      Cudd_Ref(f_10);
      Cudd_RecursiveDeref(dd_root, temp_bdd_in1);
      Cudd_RecursiveDeref(dd_root, temp_bdd_in2);
      Cudd_RecursiveDeref(dd_root, temp_cofactor);

      f_xor = Cudd_bddXor(dd_root, f_01, f_10);
      Cudd_Ref(f_xor);
    }

    if ((f_xor == DD_ZERO(dd_root)) || (f_xor == Cudd_ReadZero(dd_root)) || (f_xor == Cudd_Not(DD_ONE(dd_root))) || (f_xor == Cudd_Not(Cudd_ReadOne(dd_root)))){
      Cudd_RecursiveDeref(dd_root, f_01);
      Cudd_RecursiveDeref(dd_root, f_10);
      Cudd_RecursiveDeref(dd_root, f_xor);
      std::cout<<"symmetric\n"; return;
    }
    else{
      // asymmetric, need to find counter example
      std::cout<<"asymmetric\n";
      string example_bdd = "", example_abc = "";
      for (int idx=0; idx<bdd_variable_size; idx++){example_bdd+="0";}
      for (int idx=0; idx<Abc_NtkPiNum(pNtk); idx++){example_abc+="0";}

      if (! get_satisfy(dd_root, f_xor, example_bdd, 0)){std::cout << "Can't find counter example\n";}
      for (int idx=0; idx<bdd_variable_size; idx++){
        auto temp_name_pos = find(ori_name.begin(), ori_name.end(), bdd_order[idx]);
        if (temp_name_pos == ori_name.end()){std::cout << "Name mismatch between Abc circuit and Bdd circuit.\n";}
        example_abc[temp_name_pos-ori_name.begin()] = example_bdd[idx];
      }

      // pattern of f_01
      for (int idx=0; idx<Abc_NtkPiNum(pNtk); idx++){
        if (idx == input_index_1){std::cout<<"0";}
        else if (idx == input_index_2){std::cout<<"1";}
        else{std::cout<<example_abc[idx];}
      }
      std::cout<<endl;
      // pattern of f_10
      for (int idx=0; idx<Abc_NtkPiNum(pNtk); idx++){
        if (idx == input_index_1){std::cout<<"1";}
        else if (idx == input_index_2){std::cout<<"0";}
        else{std::cout<<example_abc[idx];}
      }
      std::cout<<endl;
      Cudd_RecursiveDeref(dd_root, f_xor);
      if (f_01 != NULL) {Cudd_RecursiveDeref(dd_root, f_01);}
      if (f_10 != NULL) {Cudd_RecursiveDeref(dd_root, f_10);}
      return;
    }
  }
}


int Lsv_CommandSymBdd(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }

  int output_index = atoi(argv[1]), input_index_1 = atoi(argv[2]), input_index_2 = atoi(argv[3]);
  Lsv_NtkSymBdd(pNtk, output_index, input_index_1, input_index_2);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sym_bdd [-h]\n");
  Abc_Print(-2, "\t        enter lsv_sym_bdd <k> <i> <j>, which k is the output pin index, (i,j) are the input index. Use BDD to check whether (i,j) are symmetry.\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}
