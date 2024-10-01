#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "aig/aig/aig.h"
#include <iostream>
#include <fstream>
#include <map>
#include <set>
using namespace std;


static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintCuts(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_CommandPrintCuts, 0);

}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

/*
Abc_NtkForEachCi
Abc_NtkForEachCO
abc_objfanino // obj pointer
Abc_Obj FaninIdO
V/ obj id */


// ---------------------------------- my function -----------------------------------
int countTotalSets(const map<unsigned int, set<set<unsigned int>>>& hash_table) {
    int total_sets = 0;
    for (const auto& pair : hash_table) {
        total_sets += pair.second.size();
    }
    
    return total_sets;
}

void Lsv_aig(Abc_Ntk_t* pNtk, int k){
  map<unsigned int, set< set<unsigned int>>> hash_table;
  Abc_Obj_t* pObj;
  int i;
  // Start from Primary Input.
  Abc_NtkForEachCi(pNtk, pObj, i){
    hash_table[Abc_ObjId(pObj)] = {{Abc_ObjId(pObj)}};
  }
  
  int j;
  Abc_NtkForEachNode(pNtk, pObj, j){
    // result of cross product of two children.
    set<set<unsigned int>> result;
    const auto& set1 = hash_table[Abc_ObjId(Abc_ObjFanin0(pObj))];

    // If the Fanin1 exists
    if(Abc_ObjFaninNum(pObj) == 2){
    const auto& set2 = hash_table[Abc_ObjId(Abc_ObjFanin1(pObj))];
    // Cross product
      for (const auto& s1 : set1) {
        for (const auto& s2 : set2) {
            set<unsigned int> temp_set;
            temp_set.insert(s1.begin(), s1.end());  
            temp_set.insert(s2.begin(), s2.end());
            if (temp_set.size() <= k) 
              result.insert(temp_set); 
        }
      }
    }

    else
      result = set1;
    result.insert({Abc_ObjId(pObj)}); 
    hash_table[Abc_ObjId(pObj)] = result;
  }
  
  // Primary Output
  Abc_NtkForEachCo(pNtk, pObj, i){
    // result of cross product of two children.
    set<set<unsigned int>> result;
    const auto& set1 = hash_table[Abc_ObjId(Abc_ObjFanin0(pObj))];

    // If the Fanin1 exists
    if(Abc_ObjFaninNum(pObj) == 2){
    const auto& set2 = hash_table[Abc_ObjId(Abc_ObjFanin1(pObj))];
    // Cross product
      for (const auto& s1 : set1) {
        for (const auto& s2 : set2) {
            set<unsigned int> temp_set;
            temp_set.insert(s1.begin(), s1.end());  
            temp_set.insert(s2.begin(), s2.end());
            if (temp_set.size() <= k) 
              result.insert(temp_set); 
        }
      }
    }

    else
      result = set1;
    result.insert({Abc_ObjId(pObj)}); 
    hash_table[Abc_ObjId(pObj)] = result;
  }
  
  // write output file
  //string filename = to_string(k) + ".txt";
  //ofstream outfile(filename); 
  /*
  for (const auto& [key, value] : hash_table) {
        for (const auto& inner_set : value) {
            //cout << key << ": ";
            //outfile << key << ": ";
            for (const auto& element : inner_set) {
                //cout << element << " ";
                //outfile << element << " ";
            }
            //cout << endl;
            //outfile << endl;
        }
  }*/

  int total_sets = countTotalSets(hash_table);
    cout << "Total number of sets in hash_table: " << total_sets << endl;

}

// -------------------------------------------------- example ---------------------------------------------------------
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


// -------------------------------------------------- end ---------------------------------------------------------

int Lsv_CommandPrintCuts(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  Extra_UtilGetoptReset();
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  Lsv_aig(pNtk, atoi(argv[1]));
  return 0;
}