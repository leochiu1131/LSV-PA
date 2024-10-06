#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <set>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <algorithm>

using namespace std;

static int Lsv_K_Feasible(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_K_Feasible, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

void setUnion(set<set<int>>& result, const set<int>& set1,const set<int>& set2,int k) {
  set<int> resultSet;
  resultSet.insert(set1.begin(), set1.end());
  resultSet.insert(set2.begin(), set2.end());

  if(resultSet.size() > k){
    return;
  }

  for (auto it1 = result.begin(); it1 != result.end(); ++it1) {
    if(includes(resultSet.begin(), resultSet.end(), it1->begin(), it1->end())){
      return;
    }
  }
  result.insert(resultSet);
}

void Lsv_recurNtk(unordered_map<int, set<set<int>>>& umap, Abc_Obj_t* pNtk, int k) {
  int size = Abc_ObjFaninNum(pNtk);
  set<set<int>> v[size];
  Abc_Obj_t* pFanin;
  int i;

  //cout << Abc_ObjId(pNtk) <<endl;
  Abc_ObjForEachFanin(pNtk, pFanin, i){
    int F_id = Abc_ObjId(pFanin);
    if(F_id == 0){
      continue;
    }
    auto it = umap.find(F_id);

    if (it == umap.end()) {
      Lsv_recurNtk(umap, pFanin, k);
      v[i] = umap[F_id];
    } else {
      v[i] = it->second;
    }
  }
  
  set<set<int>> result;
  set<int> cut{Abc_ObjId(pNtk)};
  result.insert(cut);
  if(size == 1){
    result.insert(v[0].begin(), v[0].end());
  }else{
    for(auto &a : v[0]){
      for(auto &b : v[1]){
        setUnion(result,a,b,k);
      }
    }
  }

  for(int a = 0; a < size; a++){
    v[a].clear();
  }

  umap[Abc_ObjId(pNtk)] = result;
}

void PrintNode(unordered_map<int, set<set<int>>>& umap){
  /*set<set<int>> setsToDelete;

  for (auto &a : umap) {
    for (auto &s1 : a.second) {
      for (auto &s2 : a.second) {
        if(s1 == s2)
          continue;

        if (includes(s1.begin(), s1.end(), s2.begin(), s2.end())) {
          setsToDelete.insert(s1);
          break;
        }
      }
    }
    for (const auto& subset : setsToDelete) {
      a.second.erase(subset);
    }
  }*/

  for (const auto &a : umap) {
    for(const auto &b : a.second){
      cout << a.first <<": ";
      for (const auto &s : b) {
        cout << s << " ";
      }
      cout << endl;
    }
  }
}

int Lsv_K_Feasible(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c;

  string str = argv[1];
  int k = stoi(str);
  Abc_Obj_t* pPo, *pPi;
  int i, id;
  unordered_map<int, set<set<int>>> umap;

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
  //cout <<Abc_NtkPiNum( pNtk ) <<endl;
  Abc_NtkForEachPi(pNtk, pPi, i) {
    id = Abc_ObjId(pPi);
    set<int> cut{id};
    set<set<int>> v;
    v.insert(cut);
    umap[id] = v;
  }
  Abc_NtkForEachPo(pNtk, pPo, i) {
    Lsv_recurNtk(umap, pPo, k);
  }
  PrintNode(umap);

  return 0;

usage:
  Abc_Print(-2, "usage: lsv_print_nodes [-h]\n");
  Abc_Print(-2, "\t        prints the nodes in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}