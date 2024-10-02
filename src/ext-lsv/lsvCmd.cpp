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
  if(v1.size() != v2.size()) return (v1.size() < v2.size());
  for(int i = 0; i < v1.size(); i++) {
    if(v1[i] != v2[i]) return (v1[i] < v2[i]);
  }
  return true;
}

bool is_vector_same(vector<int> v1, vector<int> v2) {
  if(v1.size() != v2.size()) return false;
  for(int i = 0; i < v1.size(); i++) {
    if(v1[i] != v2[i]) return false;
  }
  return true;
}

bool find_cut(Abc_Ntk_t* pNtk, Abc_Obj_t* pObj, vector< vector< vector<int> > >& All_Cuts, vector<int>& finish_find_cut, int k) {
  int ID_now = Abc_ObjId(pObj);
  vector< vector<int> > Cuts_temp;
  vector<int> cut_temp;
  Abc_Obj_t* pFanin;
  int t;
  Abc_ObjForEachFanin( pObj, pFanin, t ){
    cut_temp.clear();
    int ID_fanin = Abc_ObjId(pFanin);
    if(finish_find_cut[ID_fanin] == 0) {
      cut_temp.push_back(ID_fanin);
      All_Cuts[ID_fanin].push_back(cut_temp);
      find_cut(pNtk, pFanin, All_Cuts, finish_find_cut, k);
      finish_find_cut[ID_fanin] = 1;
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
      bool both_side_have_this_cut = false;
      //check if this left is in right
      for(int j = 0; j < Cuts_right.size(); j++) {  
        if(is_vector_same(Cuts_left[i], Cuts_right[j])) {
          // both side has this cut
          both_side_have_this_cut = true;
          // this is also a cut of pObj --> check if cut exist already
          bool not_in_cut_yet = true;
          for(int q = 0; q < Cuts_temp.size(); q++) {
            if(is_vector_same(Cuts_temp[k], Cuts_left[i])) not_in_cut_yet = false;
          }
          if(not_in_cut_yet) Cuts_temp.push_back(Cuts_left[i]);
          break;
        }
      }
      if(both_side_have_this_cut) continue;
      // merge all cuts in right with this left 
      for(int j = 0; j < Cuts_right.size(); j++) {
        //check if this right is in left
        both_side_have_this_cut = false;
        for(int ii = 0; ii < Cuts_left.size(); ii++) { 
          if(is_vector_same(Cuts_left[ii], Cuts_right[j])) {
            both_side_have_this_cut = true;
            break;
          }
        }
        if(both_side_have_this_cut) continue; 
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
        if(cut_temp.size() > k) continue;
        bool not_in_cut_yet = true;
        for(int w = 0; w < Cuts_temp.size(); w++) {
          if(is_vector_same(Cuts_temp[w], cut_temp)) not_in_cut_yet = false;
        }
        if(not_in_cut_yet) Cuts_temp.push_back(cut_temp);

      }
    }
    sort(Cuts_temp.begin(), Cuts_temp.end(), vector_cmp);
  }
  for(int i = 0; i < Cuts_temp.size(); i++) {
    All_Cuts[ID_now].push_back(Cuts_temp[i]);
  }
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
    Cuts.clear();
    cut.clear();
    if(Abc_ObjFaninNum(pObj) == 0) {
      All_Cuts[ID_now].clear();
      if(Abc_ObjFanoutNum(pObj) == 0) {  // constant 0 node (not sure)  
        finish_find_cut[ID_now] = 1;
      } else {
        printf("%d: %d\n", ID_now, ID_now);
        cut.push_back(ID_now);
        Cuts.clear();
        Cuts.push_back(cut);
        All_Cuts[ID_now] = Cuts;
      }
      continue;
    }
    if(finish_find_cut[ID_now] == 0) {
      All_Cuts[ID_now].clear();
      cut.push_back(ID_now);
      Cuts.clear();
      All_Cuts[ID_now].push_back(cut);
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