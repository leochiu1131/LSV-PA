#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <vector>
#include <iostream>
#include <algorithm>
#include <string>
#include <fstream>
using namespace std;

static int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv);

void init_aig(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig", Lsv_CommandSimAig, 0);
}

void destroy_aig(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer_aig = {init_aig, destroy_aig};

struct PackageRegistrationManager_aig {
  PackageRegistrationManager_aig() { Abc_FrameAddInitializer(&frame_initializer_aig); }
} lsvPackageRegistrationManager_aig;


void Lsv_NtkSimAig(Abc_Ntk_t* pNtk, vector<string> input){

  vector<vector<bool>> result;
  Abc_Obj_t* pObj;
  int no_use, total_node = Abc_NtkObjNum(pNtk), total_input = input.size(), total_circuit_output = Abc_NtkPoNum(pNtk);

  for (int idx=0; idx<total_input; idx++){
    vector<bool> temp_vector(total_node);
    result.push_back(temp_vector);
    for (int j=1; j<input[idx].size()+1; j++){
      if (input[idx][j-1]=='0'){result[idx][j] = false;}
      else {result[idx][j] = true;}
    }
  }

  Abc_NtkForEachNode(pNtk, pObj, no_use) {
    if (Abc_ObjIsPi(pObj) == true){}
    else{
      for (int a=0; a<total_input; a++){
        int fanin_num = Abc_ObjFaninNum(pObj);
        vector<bool> value_of_fanin(fanin_num);
        int obj_id = Abc_ObjId(pObj);
        //std::cout << "obj id: " << obj_id << std::endl;

        for (int idx=0; idx<fanin_num; idx++){
          Abc_Obj_t* temp_fanin = Abc_ObjFanin(pObj, idx);
          int temp_phase = Abc_ObjFaninC(pObj, idx);
          //std::cout << "fanin id: " << Abc_ObjId(temp_fanin) << std::endl;
          //std::cout << "phase: " << temp_phase << std::endl;

          if ((result[a][Abc_ObjId(temp_fanin)]==false && temp_phase==0) || (result[a][Abc_ObjId(temp_fanin)]==true && temp_phase==1)){
            value_of_fanin[idx] = false;
          }
          else{value_of_fanin[idx] = true;}
        }

        bool node_result = true;
        for (int idx=0; idx<fanin_num; idx++){
          if (value_of_fanin[idx] == false){
            node_result = false;
            break;
          }
        }
        result[a][obj_id] = node_result;
        //std::cout << "\n\n";
      }
    }
  }
  vector<vector<bool>> po_result;
  for (int a=0; a<total_input; a++){
    //std::cout << "----------------------------------------------------------------------------------------------------\n";
    vector<bool> temp_vector_po;
    
    Abc_Obj_t* ppo;
    int po_idx;
    Abc_NtkForEachPo(pNtk, ppo, po_idx){
      int fanin_num = Abc_ObjFaninNum(ppo);
      vector<bool> value_of_fanin(fanin_num);
      int obj_id = Abc_ObjId(ppo);
      //std::cout << "output obj id: " << obj_id << std::endl;

      for (int idx=0; idx<fanin_num; idx++){
        Abc_Obj_t* temp_fanin = Abc_ObjFanin(ppo, idx);
        int temp_phase = Abc_ObjFaninC(ppo, idx);
        //std::cout << "fanin id: " << Abc_ObjId(temp_fanin) << std::endl;
        //std::cout << "phase: " << temp_phase << std::endl;

        if ((result[a][Abc_ObjId(temp_fanin)]==false && temp_phase==0) || (result[a][Abc_ObjId(temp_fanin)]==true && temp_phase==1)){
          value_of_fanin[idx] = false;
        }
        else{value_of_fanin[idx] = true;}
      }

      bool node_result = true;
      for (int idx=0; idx<fanin_num; idx++){
        if (value_of_fanin[idx] == false){
          node_result = false;
          break;
        }
      }
      temp_vector_po.push_back(node_result);
      //std::cout << "\n\n";
    }
    po_result.push_back(temp_vector_po);
  }
  
  for (int idx=0; idx<total_input; idx++){
    for (int j=0; j<total_node; j++){
      //std::cout << result[idx][j] << "\t";
    }
    //std::cout << std::endl << std::endl;
  }

  //std::cout << "After parallel simulation: \n";
  Abc_Obj_t* ppo;
  int po_idx;
  Abc_NtkForEachPo(pNtk, ppo, po_idx){
    char * ouput_name = Abc_ObjName(ppo);
    std::cout << ouput_name << ": ";
    for (int idx=0; idx<total_input; idx++){
      std::cout << po_result[idx][po_idx];
    }
    std::cout << "\n";
  }

  

/*
  std::cout << "After parallel simulation: \n";
  for (int idx=0; idx<total_input; idx++){
    for (int j=0; j<Abc_NtkPoNum(pNtk); j++){
      std::cout << po_result[idx][j] << "\t";
    }
    std::cout << std::endl << std::endl;
  }
*/

}

int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv) {
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

  vector<string> input;
  string str_temp;
  std::ifstream infile;
  infile.open(argv[1]);
  while (infile >> str_temp) {
    input.push_back(str_temp);
  }
  for (int i=0; i<input.size(); i++){
    //std::cout << "input string: " << input[i] << std::endl;
  }
  infile.close();
  
  Lsv_NtkSimAig(pNtk, input);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sim_aig [-h]\n");
  Abc_Print(-2, "\t        given input file, simulate the aig circuit.\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}
