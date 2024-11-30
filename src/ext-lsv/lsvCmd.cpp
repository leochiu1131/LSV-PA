#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <vector>
#include <algorithm>
#include <string>

#include "sat/cnf/cnf.h"
extern "C"{
  Aig_Man_t* Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
}

using std::vector;
using std::sort;
using std::string;

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintCut(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSDC(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandODC(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandODC_LOOP(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_CommandPrintCut, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sdc", Lsv_CommandSDC, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_odc", Lsv_CommandODC, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_odc_loop", Lsv_CommandODC_LOOP, 0);
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
    if (argv[1][i] < '3' || argv[1][i] > '6') {
      printf("k should be an integer between [3,6]\n");
      return 1;
    } 
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


int Lsv_CommandSDC(Abc_Frame_t* pAbc, int argc, char** argv) {
  if(argc != 2) {
    printf("Format should be \" lsv_sdc <k> \" where k is an integer denotes a node.\n");
    return 1;
  }
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int n;
  sscanf(argv[1], "%d", &n);
  //printf("Target is %d\n", n);
  Abc_Obj_t* Target = Abc_NtkObj(pNtk, n);
  Abc_Obj_t** Fanin_Target = new Abc_Obj_t*[2];
  Abc_Obj_t* pFanin;
  int ii;
  Abc_ObjForEachFanin(Target, pFanin, ii) {
    Fanin_Target[ii] = pFanin;
  }
  Vec_Ptr_t* Fanins;
  Fanins = Vec_PtrAllocExact(2);
  Vec_PtrPush( Fanins, Fanin_Target[0]);
  Vec_PtrPush( Fanins, Fanin_Target[1]);
  Abc_Ntk_t* ConeArray = Abc_NtkCreateConeArray(pNtk, Fanins, 0);
  /*
  Abc_Ntk_t* Cone_of_Target = Abc_NtkCreateCone(pNtk, Target, "Target_Cone_Output", 0);
  
  Abc_Obj_t* Target_in_Cone_of_Target = Abc_NtkObj(Cone_of_Target, Abc_NtkObjNum(Cone_of_Target)-1);
  //printf("Test : %s\n", Abc_ObjName(Target_in_Cone_of_Target));
  Abc_Obj_t** Fanin_of_Target_in_Cone_of_Target = new Abc_Obj_t*[2];
  int * ID_of_Fanin_of_Target_in_Cone_of_Target = new int[2];
  Abc_ObjForEachFanin(Target_in_Cone_of_Target, pFanin, ii) {
    Fanin_of_Target_in_Cone_of_Target[ii] = pFanin;
    ID_of_Fanin_of_Target_in_Cone_of_Target[ii] = Abc_ObjId(pFanin);
  }
  Vec_Ptr_t* Fanins;
  Fanins = Vec_PtrAllocExact(2);
  Vec_PtrPush( Fanins, Fanin_of_Target_in_Cone_of_Target[0]);
  Vec_PtrPush( Fanins, Fanin_of_Target_in_Cone_of_Target[1]);
  Abc_Ntk_t* ConeArray = Abc_NtkCreateConeArray(Cone_of_Target, Fanins, 1);
  */
  //Abc_Obj_t* A;
  int i;
  // === Simulate ===
  //printf("=== Simulate ===\n");
  int* input  = new int[Abc_NtkPiNum(ConeArray)]();
  int* output = new int[2]();
  int* Comp   = new int[2]();
  Comp[0] = Abc_ObjFaninC0(Target);
  Comp[1] = Abc_ObjFaninC1(Target);
  
  //printf("Comps : %d %d\n", Comp[0], Comp[1]);
  output = Abc_NtkVerifySimulatePattern(ConeArray, input);
  //for(int k = 0; k <= 1; k++) output[k] = output[k] ^ Comp[k]; //Comp handle last 

  // === Sat Solving ===
  //printf("=== Sat Solving ===\n");
  sat_solver* sat = sat_solver_new();
  Aig_Man_t* pAig = Abc_NtkToDar(ConeArray, 0, 0);
  Cnf_Dat_t* cnf = Cnf_Derive(pAig, 2);
  //Cnf_DataPrint(cnf,1);
  //Abc_Obj_t* pObj;
  //Abc_NtkForEachCi(ConeArray, pObj, i) printf("Input : %d %d\n", Abc_ObjId(pObj), cnf->pVarNums[pObj->Id]);
  //Abc_NtkForEachCo(ConeArray, pObj, i) printf("Output : %d %d\n", Abc_ObjId(pObj), cnf->pVarNums[pObj->Id]);
  int var[2];
  //Abc_NtkForEachPo(ConeArray, pObj, i) var[i] = cnf->pVarNums[pObj->Id];
  Aig_Obj_t* AigObj;
  Aig_ManForEachCo(pAig, AigObj, i) var[i] = cnf->pVarNums[AigObj->Id];
  Cnf_DataWriteIntoSolverInt(sat,cnf,1,1);
  //sat->verbosity = 1;
  int sol;
  lit* l = new lit[2];
  bool no_SDC = true;
  int sdc_result[4];
  for(int index0 = 0; index0 <= 1; index0++) {
    for(int index1 = 0; index1 <= 1; index1++) {
      if(index1 == output[1] && index0 == output[0]) continue;
      l[0] = var[0] * 2 + (index0 == 0);
      l[1] = var[1] * 2 + (index1 == 0);
      sol = sat_solver_solve(sat, l, l+2, 0, 0, 0, 0);
      if(sol != 1) {
        no_SDC = 0;
        sdc_result[(index0^Comp[0])*2+(index1^Comp[1])] = 1;
        // Stupid output format change 24 hours right before deadline
        //printf("%d%d\n", index0^Comp[0], index1^Comp[1]);

      }
    }
  }
  if(no_SDC) {
    printf("no sdc\n");
  } else {
    for(int index = 0; index < 4; index++) {
      if(sdc_result[index] == 1) printf("%d%d ", index/2, index%2);
    }
    printf("\n");
  }
  
  /*
  l[0] = 1*2;
  l[1] = 2*2;
  sol = sat_solver_solve(sat, l, l+2, 10000,10000,10000,10000);
  printf("sol : %d\n", sol);
  printf("%s\n", (sol == 1) ? "True" : "False");
  ans[3] = sol;
  l[0] = 1*2+1;
  l[1] = 2*2;
  sol = sat_solver_solve(sat, l, l+2, 10000,10000,10000,10000);
  printf("sol : %d\n", sol);
  printf("%s\n", (sol == 1) ? "True" : "False");
  ans[2] = sol;
  l[0] = 1*2;
  l[1] = 2*2+1;
  sol = sat_solver_solve(sat, l, l+2, 10000,10000,10000,10000);
  printf("sol : %d\n", sol);
  printf("%s\n", (sol == 1) ? "True" : "False");
  ans[1] = sol;
  l[0] = 1*2+1;
  l[1] = 2*2+1;
  sol = sat_solver_solve(sat, l, l+2, 10000,10000,10000,10000);
  printf("sol : %d\n", sol);
  printf("%s\n", (sol == 1) ? "True" : "False");
  ans[0] = sol;
  */
  //for(int k = 0; k < Abc_NtkCiNum(Cone_of_Target); k++) printf("%d ", input[k]);
  //printf("\n->\n%d %d\n", output[0], output[1]);
  

  return 0;
}

int Lsv_CommandODC(Abc_Frame_t* pAbc, int argc, char** argv) {
  if(argc != 2) {
    printf("Format should be \" lsv_odc <k> \" where k is an integer denotes a node.\n");
    return 1;
  }
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int n;
  sscanf(argv[1], "%d", &n);
  Abc_Obj_t* Target = Abc_NtkObj(pNtk, n);
  int* Comp   = new int[2]();
  Comp[0] = Abc_ObjFaninC0(Target);
  Comp[1] = Abc_ObjFaninC1(Target);
  Abc_Ntk_t* pNtk2 = Abc_NtkDup(pNtk);
  Abc_Obj_t* Target2 = Abc_NtkObj(pNtk2, n);
  
  
  Vec_Ptr_t* Fanins;
  Fanins = Vec_PtrAllocExact(2);
  Abc_Obj_t** Fanin_Target = new Abc_Obj_t*[2];
  Abc_Obj_t* pFanin;
  int pi;
  Abc_ObjForEachFanin(Target, pFanin, pi){
    Fanin_Target[pi] = pFanin;
  }
  Vec_PtrPush( Fanins, Fanin_Target[0]);
  Vec_PtrPush( Fanins, Fanin_Target[1]);
  Abc_Ntk_t* ConeArray = Abc_NtkCreateConeArray(pNtk, Fanins, 1);
  sat_solver* sat_conearray = sat_solver_new();
  Aig_Man_t* pAig_conearray = Abc_NtkToDar(ConeArray, 0, 0);
  Cnf_Dat_t* cnf_conearray = Cnf_Derive(pAig_conearray, 2);
  int var_conearray[2];
  Aig_Obj_t* Co_cone;
  Aig_ManForEachCo(pAig_conearray, Co_cone, pi) var_conearray[pi] = cnf_conearray->pVarNums[Co_cone->Id];
  //Cnf_DataPrint(cnf_conearray,1);
  Cnf_DataWriteIntoSolverInt(sat_conearray,cnf_conearray,1,1);
  /*
  Abc_Obj_t* p_F;
  int I;
  Abc_ObjForEachFanin(Target, p_F, I) {
    printf("Fanin%d : %d\n", I, Abc_ObjId(p_F));
  }
  Abc_ObjForEachFanin(Target2, p_F, I) {
    printf("Fanin%d : %d\n", I, Abc_ObjId(p_F));
  }
  */
  
  //printf("%s %s\n", Abc_ObjName(Abc_NtkCo(ConeArray, 0)), Abc_ObjName(Abc_NtkCo(ConeArray, 1)));

  Abc_Obj_t* pObj;
  int i;
  Abc_NtkForEachNode(pNtk2, pObj, i) {
    if(Abc_ObjFanin0(pObj) == Target2) {
      //printf("%d %d\n", Abc_ObjId(pObj), 0);
      Abc_ObjXorFaninC(pObj,0);
    }
    if(Abc_ObjFanin1(pObj) == Target2) {
      //printf("%d %d\n", Abc_ObjId(pObj), 1);
      Abc_ObjXorFaninC(pObj,1);
    }
  }
  Abc_NtkForEachPo(pNtk2, pObj, i) {
    if(Abc_ObjFanin0(pObj) == Target2) {
      //printf("%d %d\n", Abc_ObjId(pObj), 0);
      Abc_ObjXorFaninC(pObj,0);
    }
  }
  Abc_Ntk_t* BigNtk;
  BigNtk = Abc_NtkMiter(pNtk, pNtk2, 1, 0, 0, 0);
  
  if(Abc_NtkAppend(BigNtk, ConeArray,1) == 0) printf("Failed to append Network!\n");  
  
  //int* input  = new int[Abc_NtkPiNum(BigNtk)]();
  //printf("Target Name %s\n", Abc_ObjName(Target));
  Abc_Obj_t* TargetBig = NULL;
  string Name_target(Abc_ObjName(Target));
  Abc_NtkForEachNode(BigNtk, pObj,i) {
    string Name_Obj = string(Abc_ObjName(pObj));
    //printf("Check : %s\n",  Name_Obj.c_str());
    //int cmp = strcmp(Name_target,Name_Obj);
    //printf("%d\n", cmp);
    if(Name_target.compare(Abc_ObjName(pObj)) == 0) {
      //printf("Find Target!!! %d\n", Abc_ObjId(pObj));
      TargetBig = pObj;
    }
  }
  if(TargetBig == NULL) {
    printf("no odc\n");
    return 0; 
  }//printf("Cant find target in big network!\n"); 
  /*
  Abc_ObjForEachFanin(TargetBig, p_F, I) {
    printf("Fanin%d : %d\n", I, Abc_ObjId(p_F));
  }
  printf("PI num : %d\n", Abc_NtkPiNum(BigNtk));
  printf("PO num : %d\n", Abc_NtkPoNum(BigNtk));
  printf("Names:\n");
  printf("%s\n", Abc_ObjName(Target));
  printf("%s\n", Abc_ObjName(Target2));
  printf("%s\n", Abc_ObjName(TargetBig));
  printf("Comp0 : %d %d %d\n", Abc_ObjFaninC0(Target), Abc_ObjFaninC0(Target2), Abc_ObjFaninC0(TargetBig));
  printf("Comp1 : %d %d %d\n", Abc_ObjFaninC1(Target), Abc_ObjFaninC1(Target2), Abc_ObjFaninC1(TargetBig));
  */
 /*
  int* output = new int[3]();
  output = Abc_NtkVerifySimulatePattern(BigNtk, input);
  printf("%d%d->%d %d%d\n",input[0], input[1], output[0], output[1], output[2]);
  input[0] = 0; input[1] = 1;
  output = Abc_NtkVerifySimulatePattern(BigNtk, input);
  printf("%d%d->%d %d%d\n",input[0], input[1], output[0], output[1], output[2]);
  input[0] = 1; input[1] = 0;
  output = Abc_NtkVerifySimulatePattern(BigNtk, input);
  printf("%d%d->%d %d%d\n",input[0], input[1], output[0], output[1], output[2]);
  input[0] = 1; input[1] = 1;
  output = Abc_NtkVerifySimulatePattern(BigNtk, input);
  printf("%d%d->%d %d%d\n",input[0], input[1], output[0], output[1], output[2]);
  */
 /*
  Abc_NtkForEachNode(BigNtk, pObj, i){
    Abc_AigPrintNode(pObj);
  }
  */
  bool swap = false;
  Abc_NtkForEachCo(BigNtk, pObj, i){
    //printf("%d %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
    Abc_Obj_t* pFanin;
    int j;
    Abc_ObjForEachFanin(pObj, pFanin, j) {
      if(i == 1) {
        if(TargetBig == NULL) break;
        //printf("%d %d\n", Abc_ObjId(Abc_ObjFanin0(TargetBig)), Abc_ObjId(Abc_ObjFanin1(TargetBig)));
        //printf("%d\n", Abc_ObjId(pFanin));
        //printf("test id : %d %d\n", Abc_ObjId(pFanin), pFanin->Id);
        //printf("ID compare : %d %d\n", Abc_ObjId(pFanin), );
        if(Abc_ObjId(pFanin) == Abc_ObjId(Abc_ObjFanin1(TargetBig))) swap = true;
      }
      //printf("  Fanin-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanin),Abc_ObjName(pFanin));
    }
  }
  //printf("swap : %s\n", (swap)?"True":"False");
  
  sat_solver* sat = sat_solver_new();
  Aig_Man_t* pAig = Abc_NtkToDar(BigNtk, 0, 0);
  Cnf_Dat_t* cnf = Cnf_Derive(pAig, 3);
  Cnf_DataWriteIntoSolverInt(sat,cnf,1,1);
  int var[3];   
  Aig_Obj_t* AigObj;
  Aig_ManForEachCo(pAig, AigObj, i) var[i] = cnf->pVarNums[AigObj->Id];
  if(swap) {
    int t = var[1];
    var[1] = var[2];
    var[2] = t;
    //t = Comp[0];
    //Comp[0] = Comp[1];
    //Comp[1] = t;
  }
  //Cnf_DataPrint(cnf,1);
  /*
  for(int k = 0; k < 3; k++) {
    printf("var[%d] = %d\n", k, var[k]);
  }
  */
  
  //Abc_NtkForEachCi(ConeArray, pObj, i) printf("Input : %d %d\n", Abc_ObjId(pObj), cnf->pVarNums[pObj->Id]);
  //Abc_NtkForEachCo(ConeArray, pObj, i) printf("Output : %d %d\n", Abc_ObjId(pObj), cnf->pVarNums[pObj->Id]);
  
  /*
  for(int k = 0; k < 2; k++) {
    printf("var_conearray[%d] = %d\n", k, var_conearray[k]);
  }
  */
  //printf("%s\n", (swap)?"Swaped":"No swap");
  //printf("var: %d %d \n", var[1], var[2]);
  //printf("COMP : %d%d\n", Comp[0], Comp[1]);
  //printf("var_conearray : %d %d\n", var_conearray[0], var_conearray[1]);
  int odc_result[4];
  bool no_odc = true;
  lit* l = new lit[3];
  lit* l_cone = new lit[2];
  int result;
  l[0] = (var[0]*2);
  //00
  l[1] = (var[1]*2+1);
  l[2] = (var[2]*2+1);
  result = sat_solver_solve(sat, l, l+3, 0, 0, 0, 0);
  if(result == 0) printf("Solver out of limit!");
  //printf("result : %d\n", result);
  if (result != 1) {
    l_cone[0] = var_conearray[0] * 2 + 1;
    l_cone[1] = var_conearray[1] * 2 + 1;
    if(sat_solver_solve(sat_conearray, l_cone, l_cone+2, 0, 0, 0, 0) == 1) {
      no_odc = false;
      odc_result[(0^Comp[0])*2+(0^Comp[1])] = 1;
      //stupid output format change 24 hours right before deadline
      //printf("%d%d\n", 0^Comp[0], 0^Comp[1]);
    } else {
      //printf("%d%d is sdc!\n",  0^Comp[0], 0^Comp[1]);
    }
  }
    
  //01
  l[1] = (var[1]*2+1);
  l[2] = (var[2]*2);
  result = sat_solver_solve(sat, l, l+3, 0, 0, 0, 0);
  if(result == 0) printf("Solver out of limit!");
  //printf("result : %d\n", result);
  if (result != 1) {
    l_cone[0] = var_conearray[0] * 2 + 1;
    l_cone[1] = var_conearray[1] * 2;
    if(sat_solver_solve(sat_conearray, l_cone, l_cone+2, 0, 0, 0, 0) == 1) {
      no_odc = false;
      odc_result[(0^Comp[0])*2+(1^Comp[1])] = 1;
      //stupid output format change 24 hours right before deadline
      //printf("%d%d\n", 0^Comp[0], 1^Comp[1]);
    } else {
      //printf("%d%d is sdc!\n",  0^Comp[0], 1^Comp[1]);
    }
  }
  //10
  l[1] = (var[1]*2);
  l[2] = (var[2]*2+1);
  result = sat_solver_solve(sat, l, l+3, 0, 0, 0, 0);
  if(result == 0) printf("Solver out of limit!");
  //printf("result : %d\n", result);
  if (result != 1) {
    l_cone[0] = var_conearray[0] * 2;
    l_cone[1] = var_conearray[1] * 2 + 1;
    if(sat_solver_solve(sat_conearray, l_cone, l_cone+2, 0, 0, 0, 0) == 1) {
      no_odc = false;
      odc_result[(1^Comp[0])*2+(0^Comp[1])] = 1;
      //stupid output format change 24 hours right before deadline
      //printf("%d%d\n", 1^Comp[0], 0^Comp[1]);
    } else {
      //printf("%d%d is sdc!\n",  1^Comp[0], 0^Comp[1]);
    }
  }
  //11
  l[1] = (var[1]*2);
  l[2] = (var[2]*2);
  result = sat_solver_solve(sat, l, l+3, 0, 0, 0, 0);
  if(result == 0) printf("Solver out of limit!");
  //printf("result : %d\n", result);
  if (result != 1) {
    l_cone[0] = var_conearray[0] * 2;
    l_cone[1] = var_conearray[1] * 2;
    if(sat_solver_solve(sat_conearray, l_cone, l_cone+2, 0, 0, 0, 0) == 1) {
      no_odc = false;
      odc_result[(1^Comp[0])*2+(1^Comp[1])] = 1;
      //stupid output format change 24 hours right before deadline
      //printf("%d%d\n", 1^Comp[0], 1^Comp[1]);
    } else {
      //printf("%d%d is sdc!\n",  1^Comp[0], 1^Comp[1]);
    }
  }

  //sat->fPrintClause = 1;
  if(no_odc == true) {
    printf("no odc\n");
  }  else {
    for(int indexx = 0; indexx < 4; indexx++) {
      if(odc_result[indexx] == 1) printf("%d%d ", indexx/2, indexx%2);
    }
    printf("\n");
  }
  return 0;
}

int Lsv_CommandODC_LOOP(Abc_Frame_t* pAbc, int argc, char** argv) {
  if(argc != 3) {
    printf("Format should be \" lsv_odc <k1> <k2>\" where k is an integer denotes a node.\n");
    return 1;
  } 
  char** my_argv = argv;
  int my_argc = 2;
  int n;
  sscanf(argv[1], "%d", &n);
  int n_end;
  sscanf(argv[2], "%d", &n_end);
  while(n <= n_end) {
    string s = std::to_string(n);
    sscanf(s.c_str(), "%s", my_argv[1]);
    //my_argv[1] = s.c_str();
    printf("lsv_odc %d : \n", n);
    Lsv_CommandODC(pAbc, my_argc, my_argv);
    n++;
  }
  return 0;
}