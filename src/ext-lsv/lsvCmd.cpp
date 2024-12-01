#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "misc/util/abc_global.h"
#include "misc/vec/vec.h"
#include "opt/cut/cut.h"
#include <vector>
#include <iostream>
#include <set>
#include <array>
#include <algorithm>


static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintCut(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSDC(Abc_Frame_t* pAbc, int argc, char** argv);
void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_CommandPrintCut, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sdc", Lsv_CommandSDC, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

//print node
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

//kcut

void PrintKFeasibleCuts(Abc_Ntk_t* pNtk, int k) {
    std::vector<std::vector<std::set<int>>> vecOfVecOfSets;
    int piCount = 0;
    int pocount = 0;
    piCount = Abc_NtkPiNum(pNtk);
    pocount = Abc_NtkPoNum(pNtk);
    for(int i = 0; i < piCount; i++){
      std::vector<std::set<int>> vecOfSets;
      std::set<int> mySet;
      mySet.insert(i+1);
      vecOfSets.push_back(mySet);
      vecOfVecOfSets.push_back(vecOfSets);
    }
    //printf("size of vecOfVecOfSets : %d\n", vecOfVecOfSets.size());
    Abc_Obj_t* pObj;
    int i;
    //printf("call success\n");
    Abc_NtkForEachNode(pNtk, pObj, i) {
      std::vector<std::set<int>> vecOfSets;
      int i = Abc_ObjId(Abc_ObjFanin0(pObj))-1;
      int j = Abc_ObjId(Abc_ObjFanin1(pObj))-1; 
      if(Abc_ObjIsPi(Abc_ObjFanin0(pObj)) == 0){
        i = i -pocount;
      }
      if(Abc_ObjIsPi(Abc_ObjFanin1(pObj)) == 0){
        j = j -pocount;
      }
      std::set<int> mySet;
      mySet.insert(Abc_ObjId(pObj));
      vecOfSets.push_back(mySet);
      //printf("set success value of i : %d , value of j : %d\n",i ,j );
      //printf("size of vecOfVecOfSets : %d\n", vecOfVecOfSets.size());    
      for(int a = 0; a < vecOfVecOfSets[i].size(); a++){
        for(int b = 0; b < vecOfVecOfSets[j].size(); b++){
              std::set<int> s(vecOfVecOfSets[i][a]);
              s.insert(vecOfVecOfSets[j][b].begin(), vecOfVecOfSets[j][b].end());
              //printf("merge success\n");
              vecOfSets.push_back(s);
        }
      }
      vecOfVecOfSets.push_back(vecOfSets);
      //printf("push success\n");
    }
    for(int i = 0; i < Abc_NtkPoNum(pNtk); i++){
      std::vector<std::set<int>> vecOfSets;
      std::set<int> mySet;
      mySet.insert(Abc_ObjId(Abc_NtkPo(pNtk, i)));
      vecOfSets.push_back(mySet);
      for(int k = 0; k < vecOfVecOfSets[Abc_ObjId(Abc_ObjFanin0(Abc_NtkPo(pNtk, i)))-piCount-1].size(); k++)
        vecOfSets.push_back(vecOfVecOfSets[Abc_ObjId(Abc_ObjFanin0(Abc_NtkPo(pNtk, i)))-piCount-1][k]);
      vecOfVecOfSets.insert(vecOfVecOfSets.begin() + piCount +i , vecOfSets);
    }
    
    // Abc_NtkForEachNode(pNtk, pObj, i) {
    //   if(Abc_ObjISPO(pObj) == 1){
    //     std::vector<std::set<int>> vecOfSets;
    //     Abc_Obj_t* pFanin;
    //     int j;
    //     Abc_ObjForEachFanin(pObj, pFanin, j) {
    //       std::set<int> mySet

    //   }
    // }
    for(int i = 0; i < vecOfVecOfSets.size(); i++){
      for(int j = 0; j < vecOfVecOfSets[i].size(); j++){
        if(vecOfVecOfSets[i][j].size() <= k){
          printf("Node %d : ", i+1);
          for(auto const &e: vecOfVecOfSets[i][j]){
            printf("%d ", e);
          }
          printf("\n");
        }
      }
    }
}

int Lsv_CommandPrintCut(Abc_Frame_t* pAbc, int argc, char** argv) {
    int k = 4; // Default value for k
    if (argc == 2) {
        k = atoi(argv[1]);
        if (k < 3 || k > 6) {
            printf("Error: k must be between 3 and 6.\n");
            return 1;
        }
    }
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    if (pNtk == NULL) {
        printf("Error: There is no current network.\n");
        return 1;
    }
    PrintKFeasibleCuts(pNtk, k);
    return 0;
}


//sdc



//helper function
std::array<bool, 4> logical_not(const std::array<bool, 4>& input) {
    std::array<bool, 4> result;
    std::transform(input.begin(), input.end(), result.begin(), [](bool val) { return !val; });
    return result;
}

//array compare
bool are_boolean_arrays_equal(const std::array<bool, 4>& arr1, const std::array<bool, 4>& arr2) {
    for (size_t i = 0; i < 4; ++i) {
        if (arr1[i] != arr2[i]) {
            return false; // Return false if any element differs
        }
    }
    return true; // All elements are the same
}

// Logical OR operation for two arrays
std::array<bool, 4> logical_or(const std::array<bool, 4>& a, const std::array<bool, 4>& b) {
    std::array<bool, 4> result;
    for (size_t i = 0; i < a.size(); ++i) {
        result[i] = a[i] || b[i];
    }
    return result;
}

//find pattern
std::vector<std::string> find_missing_patterns(const std::array<bool, 4>& arr1, const std::array<bool, 4>& arr2) {
    // Define all possible patterns
    std::vector<std::string> all_patterns = {"00", "01", "10", "11"};
    std::vector<bool> pattern_found(4, false); // Tracks which patterns are found

    // Check patterns formed element-wise
    for (size_t i = 0; i < 4; ++i) {
        std::string pattern = std::to_string(arr1[i]) + std::to_string(arr2[i]);

        // Mark the pattern as found
        for (size_t j = 0; j < all_patterns.size(); ++j) {
            if (pattern == all_patterns[j]) {
                pattern_found[j] = true;
            }
        }
    }

    // Collect patterns that were not found
    std::vector<std::string> missing_patterns;
    for (size_t i = 0; i < all_patterns.size(); ++i) {
        if (!pattern_found[i]) {
            missing_patterns.push_back(all_patterns[i]);
        }
    }

    return missing_patterns;
}


// Function to calculate the node value
std::array<bool, 4> node_value(Abc_Obj_t* node) {
    std::array<bool, 4> base1 = {true, true, false, false};
    std::array<bool, 4> base2 = {false, true, false, true};
    std::array<bool, 4> base3 = {false, false, false, false};
    std::array<bool, 4> base4 = {true, true, true, true};
    
    bool edge1_active = Abc_ObjFaninC0(node);
    bool edge2_active = Abc_ObjFaninC1(node);

    
    if (Abc_ObjIsPi(Abc_ObjFanin0(node)) || Abc_ObjIsPi(Abc_ObjFanin1(node))) {
      
      if(!Abc_ObjIsPi(Abc_ObjFanin0(node))){
        if(are_boolean_arrays_equal(node_value(Abc_ObjFanin0(node)), base3)){
          if (edge1_active && edge2_active) {
            return logical_or(logical_not(base3), logical_not(base2));
          } else if (edge1_active) {
            return logical_or(logical_not(base3), base2);
          } else if (edge2_active) {
            return logical_or(base3, logical_not(base2));
          } else {
            return logical_or(base3, base2);
          }
        }
        else if(are_boolean_arrays_equal(node_value(Abc_ObjFanin1(node)), base4)){
          if (edge1_active && edge2_active) {
            return logical_or(logical_not(base1), logical_not(base4));
          } else if (edge1_active) {
            return logical_or(logical_not(base1), base4);
          } else if (edge2_active) {
            return logical_or(base1, logical_not(base4));
          } else {
            return logical_or(base1, base4);
          }
        }
        else{
          if (edge1_active && edge2_active) {
            return logical_or(logical_not(base1), logical_not(base2));
          } else if (edge1_active) {
            return logical_or(logical_not(base1), base2);
          } else if (edge2_active) {
            return logical_or(base1, logical_not(base2));
            } else {
            return logical_or(base1, base2);
          }
          } 
      }
      else if(!Abc_ObjIsPi(Abc_ObjFanin1(node))){
        if(are_boolean_arrays_equal(node_value(Abc_ObjFanin1(node)), base3)){
          if (edge1_active && edge2_active) {
            return logical_or(logical_not(base1), logical_not(base3));
          } else if (edge1_active) {
            return logical_or(logical_not(base1), base3);
          } else if (edge2_active) {
            return logical_or(base1, logical_not(base3));
          } else {
            return logical_or(base1, base3);
          }
        }
        else if(are_boolean_arrays_equal(node_value(Abc_ObjFanin0(node)), base4)){
          if (edge1_active && edge2_active) {
            return logical_or(logical_not(base4), logical_not(base2));
          } else if (edge1_active) {
            return logical_or(logical_not(base4), base2);
          } else if (edge2_active) {
            return logical_or(base4, logical_not(base2));
          } else {
            return logical_or(base4, base2);
          }
        }
        else{
          if (edge1_active && edge2_active) {
            return logical_or(logical_not(base1), logical_not(base2));
          } else if (edge1_active) {
            return logical_or(logical_not(base1), base2);
          } else if (edge2_active) {
            return logical_or(base1, logical_not(base2));
          } else {
            return logical_or(base1, base2);
          }
        } 
      }
      else{
        if (edge1_active && edge2_active) {
            return logical_or(logical_not(base1), logical_not(base2));
        } else if (edge1_active) {
            return logical_or(logical_not(base1), base2);
        } else if (edge2_active) {
            return logical_or(base1, logical_not(base2));
        } else {
            return logical_or(base1, base2);
        }
      } 
    } else {
        std::array<bool, 4> fanin1_value = node_value(Abc_ObjFanin0(node));
        std::array<bool, 4> fanin2_value = node_value(Abc_ObjFanin1(node));


        if (edge1_active && edge2_active) {
            return logical_or(logical_not(fanin1_value), logical_not(fanin2_value));
        } else if (edge1_active) {
            return logical_or(logical_not(fanin1_value), fanin2_value);
        } else if (edge2_active) {
            return logical_or(fanin1_value, logical_not(fanin2_value));
        } else {
            return logical_or(fanin1_value, fanin2_value);
        }
    }
}

void ComputeSDCs(Abc_Ntk_t* pNtk, Abc_Obj_t* pNode) {
    //Iterate over all nodes in the network
    if (pNode == NULL || !Abc_ObjIsNode(pNode)) {
        printf("Error: Node not found.\n");
        return;
    }
    if(Abc_ObjIsPi(pNode)){
      std::cout<<"No SDC"<<std::endl;
      return;
    }
    std::array<bool, 4> f1;
    std::array<bool, 4> f2;
    if(Abc_ObjIsPi(Abc_ObjFanin0(pNode)))
      f1 = {true, true, false, false};
    else
      f1 = node_value(Abc_ObjFanin0(pNode));
    
    if(Abc_ObjIsPi(Abc_ObjFanin1(pNode)))
      f2 = {false, true, false, true};
    else
      f2 = node_value(Abc_ObjFanin1(pNode));
    
    std::vector<std::string> missing_patterns = find_missing_patterns(f1, f2);
    
    if(missing_patterns.size() == 0){
      std::cout<<"No SDC"<<std::endl;
      return;
    }
    for (const auto& pattern : missing_patterns) {
        std::cout << pattern << " ";
    }
    std::cout << std::endl;

}

// Command function
int Lsv_CommandSDC(Abc_Frame_t* pAbc, int argc, char** argv) {
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    int k;
    k = atoi(argv[1]);
    Abc_Obj_t* pNode = Abc_NtkObj(pNtk, k);
    if (pNtk == NULL) {
        printf("Error: There is no current network.\n");
        return 1;
    }
    ComputeSDCs(pNtk, pNode);
    return 0;
}