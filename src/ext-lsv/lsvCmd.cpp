#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <vector>
#include <set>
#include <algorithm>
using std::vector;
using std::set;
using std::includes;

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

void printcutmsg(){
  Abc_Print(-2, "usage: lsv_printcut <k> [-h]\n");
  Abc_Print(-2, "\t        prints the <k> feasable cuts in the network,3<=k<=6\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
}

void find_cut(Abc_Ntk_t* pNtk, Abc_Obj_t* pObj, vector<vector<set<int>>>& cuts,vector<int>& cuts_checked, int k){
  int id=Abc_ObjId(pObj);
  if(Abc_ObjFanoutNum(pObj)==0&&Abc_ObjFaninNum(pObj)==0)return;
  if(Abc_ObjFaninNum(pObj)==0&&cuts_checked[id]!=1){
    cuts[id].push_back(set<int>{id});
    cuts_checked[id]=1;
    return;
  }
  cuts[id].push_back(set<int>{id});
  Abc_Obj_t* pFanin;
  int j;
  vector<Abc_Obj_t*> pins;
  Abc_ObjForEachFanin(pObj, pFanin, j) {
    if(cuts_checked[Abc_ObjId(pFanin)]!=1)find_cut(pNtk,pFanin,cuts,cuts_checked,k);
    pins.push_back(pFanin);
  }
  if(pins.size()==1){
    for(auto i=cuts[Abc_ObjId(pins[0])].begin();i!=cuts[Abc_ObjId(pins[0])].end();i++)cuts[id].push_back(*i);
    return;
  }
  set<int> new_cut;
  for(int i=0;i<cuts[Abc_ObjId(pins[0])].size();i++){
    for(int ii=0;ii<cuts[Abc_ObjId(pins[1])].size();ii++){
      new_cut.clear();
      new_cut.insert(cuts[Abc_ObjId(pins[0])][i].begin(),cuts[Abc_ObjId(pins[0])][i].end());
      new_cut.insert(cuts[Abc_ObjId(pins[1])][ii].begin(),cuts[Abc_ObjId(pins[1])][ii].end());
      int add=1;
      for(int iii=0;iii<cuts[id].size();iii++){
        if(new_cut.size()>k||includes(new_cut.begin(),new_cut.end(),cuts[id][iii].begin(),cuts[id][iii].end())){
          add=0;
          break;
        }
        if(includes(cuts[id][iii].begin(),cuts[id][iii].end(),new_cut.begin(),new_cut.end())){
          cuts[id].erase(cuts[id].begin()+iii);
          iii--;
        }
      }
      if(add==1){
        cuts[id].push_back(new_cut);
      }
    }
  }
  cuts_checked[id]=1;
}

void Lsv_NtkPrintCut(Abc_Ntk_t* pNtk,int k) {
  Abc_Obj_t* pObj;
  int i;
  vector<vector<set<int>>> cuts(Abc_NtkObjNum(pNtk)+1);
  vector<int> cuts_checked(Abc_NtkObjNum(pNtk)+1,0);
  Abc_NtkForEachObj(pNtk, pObj, i) {
    if(cuts_checked[Abc_ObjId(pObj)]!=1)find_cut(pNtk,pObj,cuts,cuts_checked,k);
    int id=Abc_ObjId(pObj);
    for(int ii=0;ii<cuts[id].size();ii++){
      printf("%d:",id);
      for(auto iii=cuts[id][ii].begin();iii!=cuts[id][ii].end();iii++){
        printf(" %d", *iii);
      }
      printf("\n");
    }
  }
}

int Lsv_CommandPrintCut(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c;
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
    switch (c) {
      case 'h':
        printcutmsg();
        return 1;
      default:
        printcutmsg();
        return 1;
    }
  }
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  if(argc==1){
    printf("please provide k\n");
    printcutmsg();
    return 1;
  }
  if(argc>2){
    printf("too much assign value of k\n");
    printcutmsg();
    return 1;
  }
  if(strlen(argv[1])>1){
    printf("k out of range\n");
    printcutmsg();
    return 1;
  }
  int k=argv[1][0]-'0';
  if(k<3||k>6){
    printf("k out of range\n");
    printcutmsg();
    return 1;
  }
  Lsv_NtkPrintCut(pNtk,k);
  return 0;

}