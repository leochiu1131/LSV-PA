#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <vector>
#include <algorithm>

using std::vector;
using std::sort;

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintCut(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_CommandPrintCut, 0);
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

bool vector_cmp(vector<int> v1, vector<int> v2) {
  return (v1.size() < v2.size());
  /*
  if(v1.size() != v2.size()) return (v1.size() < v2.size());
  for(int i = 0; i < v1.size(); i++) {
    if(v1[i] != v2[i]) return (v1[i] < v2[i]);
  }
  return true;
  */
}

bool is_sub_vector(vector<int> v1, vector<int> v2) { // return true is v1 is subvector of v2
  if(v1.size() > v2.size()) return false;
  // v1.size() <= v2.size() now;
  int same_int_num = 0;
  for(int i = 0; i < v1.size(); i++) {
    for(int j = 0; j < v2.size(); j++) {
      if(v1[i] == v2[j]) {
        same_int_num++;
        break;
      }
    }
  }
  return (same_int_num == v1.size());
}

bool find_cut(Abc_Ntk_t* pNtk, Abc_Obj_t* pObj, vector< vector< vector<int> > >& All_Cuts, vector<int>& finish_find_cut, int k_fea) {
  int ID_now = Abc_ObjId(pObj);
  vector< vector<int> > Cuts_temp;
  vector<int> cut_temp;
  Abc_Obj_t* pFanin;
  int t;
  Abc_ObjForEachFanin( pObj, pFanin, t ){
    cut_temp.clear();
    int ID_fanin = Abc_ObjId(pFanin);
    if(finish_find_cut[ID_fanin] == 0) {
      All_Cuts[ID_fanin].clear();
      vector<int> cut_initial;
      cut_initial.push_back(ID_fanin);
      vector<vector<int> > Cuts_initial;
      Cuts_initial.push_back(cut_initial);
      All_Cuts[ID_fanin] = Cuts_initial;
      finish_find_cut[ID_fanin] = 1;
      find_cut(pNtk, pFanin, All_Cuts, finish_find_cut, k_fea);
      
    }
    if(Cuts_temp.empty()) {
      Cuts_temp = All_Cuts[ID_fanin];
      continue;
    }
    vector<vector<int> > Cuts_left;
    Cuts_left = Cuts_temp;
    vector<vector<int> > Cuts_right;
    Cuts_right = All_Cuts[ID_fanin];
    Cuts_temp.clear();
    for(int i = 0; i < Cuts_left.size(); i++) {
      
      //bool both_side_have_this_cut = false;
      /*
      //check if this left is in right
      for(int j = 0; j < Cuts_right.size(); j++) {  
        if(is_sub_vector(Cuts_left[i], Cuts_right[j])) { //left is sub of right
          // 

        }
        if(is_sub_vector(Cuts_right[j], Cuts_left[i])) { // right is sub of left
          // 
          both_side_have_this_cut = true;
          // this is also a cut of pObj --> check if cut exist already
          bool not_in_cut_yet = true;
          for(int q = 0; q < Cuts_temp.size(); q++) {
            if(is_sub_vector(Cuts_temp[q], Cuts_left[i]) && is_sub_vector(Cuts_left[i], Cuts_temp[q])) not_in_cut_yet = false;
          }
          if(not_in_cut_yet) Cuts_temp.push_back(Cuts_left[i]);
          break;
        }
      }
      if(both_side_have_this_cut) continue;
      */
      // merge all cuts in right with this left 
      for(int j = 0; j < Cuts_right.size(); j++) {
        //printf("ID:%d i:%d j:%d\n", ID_now, i, j);
        //check if this right is in left
        /*
        both_side_have_this_cut = false;
        for(int ii = 0; ii < Cuts_left.size(); ii++) { 
          if(is_sub_vector(Cuts_left[ii], Cuts_right[j]) && is_sub_vector(Cuts_right[j], Cuts_left[ii])) {
            both_side_have_this_cut = true;
            break;
          }
        }
        if(both_side_have_this_cut) continue; 
        */
        // this is also a cut of pObj
        // But do not check if need to add since will add in the above case 
        cut_temp.clear();
        //new cut
        cut_temp = Cuts_left[i];
        for(int w = 0; w < Cuts_right[j].size(); w++) {
          cut_temp.push_back(Cuts_right[j][w]);
        }
        //sort in increasing order
        sort(cut_temp.begin(), cut_temp.end());
        //remove all same node in cut
        int index = 1;
        while(index < cut_temp.size()) {
          if(cut_temp[index] == cut_temp[index - 1]) {
            cut_temp.erase(cut_temp.begin() + index);
          } else {
            index++;
          }
        }
        //add to Cuts
        if(cut_temp.size() <= k_fea && cut_temp.size() > 0) {
          bool not_in_cut_yet = true;
          
          for(int w = 0; w < Cuts_temp.size(); w++) {
            if(is_sub_vector(Cuts_temp[w], cut_temp)) not_in_cut_yet = false;
            if(is_sub_vector(cut_temp, Cuts_temp[w]))  {
              not_in_cut_yet = false;
              Cuts_temp[w] = cut_temp;
            }
          }
          
          if(not_in_cut_yet) Cuts_temp.push_back(cut_temp);
        }
      }
    }
    //sort(Cuts_temp.begin(), Cuts_temp.end(), vector_cmp);
  }
  //printf("hAHAHAH\n");
  sort(Cuts_temp.begin(), Cuts_temp.begin() + Cuts_temp.size(), vector_cmp);
  //printf("========================\n");
  //printf("%d =======================\n", ID_now);
  //printf("%ld\n", Cuts_temp.size());
  int index = 0;
  while(index < (Cuts_temp.size() - 1.0)) {
    int index2 = index + 1;
    while(index2 < Cuts_temp.size()) { 
      //printf("checking\n");
      if(is_sub_vector(Cuts_temp[index], Cuts_temp[index2])) {
        Cuts_temp.erase(Cuts_temp.begin() + index2);
      } else {
        index2++;
      }
    }
    index++;
  }
  //printf("finish delete :)\n");
  //printf("%ld\n", Cuts_temp.size());
  for(int i = 0; i < Cuts_temp.size(); i++) {
    //printf(" %d", Cuts_temp[i][0]);
    All_Cuts[ID_now].push_back(Cuts_temp[i]);
  }
  //printf("\n");
  finish_find_cut[ID_now] = 1;
  return true;
}

int Lsv_CommandPrintCut(Abc_Frame_t* pAbc, int argc, char** argv) {
  if(argc != 2) {
    printf("Format should be \" lsv_printcut <k> \" where k is an integer between [3,6]\n");
    return 1;
  }
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int k = 0;
  for(int i = 0; argv[1][i] != '\0'; i++) {
    k = k * 10;
    k += argv[1][i] - '0';
  }
  if(k < 3 || k > 6) {
    printf("k should be an integer between [3,6]\n");
    return 1;
  }
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  //printf("k = %d\n", k);
  Abc_Obj_t* pObj;
  int i;
  vector<int> finish_find_cut;
  vector< vector< vector<int> > > All_Cuts;
  vector< vector<int> > Cuts;
  vector<int> cut;
  finish_find_cut.resize(Abc_NtkObjNum(pNtk));
  All_Cuts.resize(Abc_NtkObjNum(pNtk));
  Abc_NtkForEachObj(pNtk, pObj, i) {
    int ID_now = Abc_ObjId(pObj);
    //printf("<%d>\n", ID_now);
    Cuts.clear();
    cut.clear();
    if(Abc_ObjFaninNum(pObj) == 0 && Abc_ObjFanoutNum(pObj) == 0) continue;
    
    if(finish_find_cut[ID_now] == 0) {
      All_Cuts[ID_now].clear();
      cut.push_back(ID_now);
      Cuts.push_back(cut);
      All_Cuts[ID_now] = Cuts;
      find_cut(pNtk, pObj, All_Cuts, finish_find_cut, k);
    }
    //printf("\n");
    for(int ii = 0; ii < All_Cuts[ID_now].size(); ii++) {
      //if(All_Cuts[ID_now][i].size() > k) break;
      printf("%d:", ID_now);
      for(int v = 0; v < All_Cuts[ID_now][ii].size(); v++) {
        printf(" %d", All_Cuts[ID_now][ii][v]);
      }
      printf("\n");
    }
  }
  return 0;
}