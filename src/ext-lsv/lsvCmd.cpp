#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "opt/cut/cut.h"
#include "sat/cnf/cnf.h"
#include <vector>
#include <utility>
#include <algorithm>
#include <unordered_map>
#include <iostream>
#include <random>

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintCut(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintSDC(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintODC(Abc_Frame_t* pAbc, int argc, char** argv);

extern "C"{
Aig_Man_t* Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
}

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_CommandPrintCut, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sdc", Lsv_CommandPrintSDC, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_odc", Lsv_CommandPrintODC, 0);
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


void Lsv_NtkPrintCut(Abc_Ntk_t* pNtk, int k) {
  Abc_Obj_t* pObj;
  std::vector<int> now_cuts;
  int i;

  std::unordered_map<int, std::vector<std::vector<int>>> Node_Cuts;

  assert( Abc_NtkIsStrash(pNtk) );

  Abc_NtkForEachObj(pNtk, pObj, i) {
    
    int oID = Abc_ObjId(pObj);
    int cnum = 1;


    if ( Abc_ObjIsCo(pObj) || Abc_ObjIsNone(pObj)){
      continue;
    }

    // save the node itself ( 1-cut )
    now_cuts.push_back(oID);
    Node_Cuts[oID].push_back(now_cuts);
    now_cuts.clear();

    // PIs only have 1-cut
    if ( Abc_ObjFaninNum(pObj)==0 ){
      std::cout << oID << ": " << oID << std::endl;
      continue;
    } else if ( Abc_ObjFaninNum(pObj)==1 ){
      std::cout << oID << ": " << oID << std::endl;
      int child_ID = Abc_ObjFaninId0(pObj);
      for (int j = 0; j < Node_Cuts[child_ID].size(); j++){
        now_cuts.assign(Node_Cuts[child_ID][j].begin(), Node_Cuts[child_ID][j].end());
        Node_Cuts[oID].push_back(now_cuts);
        now_cuts.clear();
        std::cout << oID << ": ";
        for (auto& ele : Node_Cuts[child_ID][j]) {
            std::cout << ele << " ";
        }
        std::cout << "\n";
      }
      continue;
    }

    int lchild_ID = Abc_ObjFaninId0(pObj);
    int rchild_ID = Abc_ObjFaninId1(pObj);
    int lchild_cnum = Node_Cuts[lchild_ID].size();
    int rchild_cnum = Node_Cuts[rchild_ID].size();

    // merge cuts of two childs
    for (int j = 0; j < lchild_cnum; j++){
      now_cuts.clear();
      if( Node_Cuts[lchild_ID][j].size() >= k)
        continue; // too large

      for (int l = 0; l < rchild_cnum; l++){
        now_cuts.clear();
        if( Node_Cuts[rchild_ID][l].size() >= k)
          continue; // too large      

        now_cuts.assign(Node_Cuts[lchild_ID][j].begin(), Node_Cuts[lchild_ID][j].end());
        now_cuts.insert( now_cuts.end(), Node_Cuts[rchild_ID][l].begin(), Node_Cuts[rchild_ID][l].end() );
        // sort the cut in assending order and remove duplicated nodes
        std::sort( now_cuts.begin(), now_cuts.end() );
        now_cuts.erase( std::unique( now_cuts.begin(), now_cuts.end() ), now_cuts.end() );
        if( now_cuts.size() > k)
          continue; // too large         
         
        Node_Cuts[oID].push_back(now_cuts);
        cnum++;
      }
    }

    now_cuts.clear();
    
    // sort cuts with number of cuts
    std::sort(Node_Cuts[oID].begin(), Node_Cuts[oID].end(),
          [](const std::vector<int>& a, const std::vector<int>& b) {
      return a.size() < b.size();
    });

    for (int j = 0; j < cnum; j++){
      std::cout << oID << ": ";
      for (auto& ele : Node_Cuts[oID][j]) {
          std::cout << ele << " ";
      }
      std::cout << "\n";
    }
  }
}


int Lsv_CommandPrintCut(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c, k;

  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
    switch (c) {
      case 'h':
        goto usage;
      default:
        goto usage;
    }
  }
  if ( globalUtilOptind >= argc )
  {
      Abc_Print( -1, "Command line lsv_printcut should be followed by an integer.\n" );
      goto usage;
  }
  k = atoi(argv[globalUtilOptind]);

  if (k < 3 || k > 6) {
    Abc_Print(-1, "Cut size k must be between 3 and 6.\n");
    return 1;
  }

  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  Lsv_NtkPrintCut(pNtk, k);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_printcut [-h] <cut>\n");
  Abc_Print(-2, "\t        prints <cut> cut of the network\n");
  Abc_Print(-2, "\t<cut> : the number of cuts we want\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

std::vector<std::pair<bool, bool>> randomSim(Abc_Ntk_t* pCone, Abc_Obj_t* pCo){
  Abc_Obj_t* pObj;
  int j;
  std::vector<std::pair<bool, bool>> PatternAppear;
  std::unordered_map<int, std::vector<bool>> NodeValue;
  int rand_num = 32;

  Abc_NtkForEachPi(pCone, pObj, j){
    for(int i=0; i<rand_num; i++){  // Random assign Pis
      NodeValue[Abc_ObjId(pObj)].push_back(rand() % 2);
    }
  }
  Abc_NtkForEachNode(pCone, pObj, j) {  
    if (Abc_ObjId(pObj) > Abc_ObjId(pCo)) break;
                      // Simulate other nodes
    std::vector<bool> fvalue0, fvalue1;
    fvalue0.reserve(rand_num);
    fvalue1.reserve(rand_num);
    int fCompl0 = Abc_ObjFaninC0(pObj);
    int fCompl1 = Abc_ObjFaninC1(pObj);
    for(int i=0; i<rand_num; i++){
      bool v0 = NodeValue[Abc_ObjFaninId0(pObj)][i]^fCompl0;
      bool v1 = NodeValue[Abc_ObjFaninId1(pObj)][i]^fCompl1;
      NodeValue[Abc_ObjId(pObj)].push_back(v0 & v1);
    }
    
  }

  for(int i=0; i<rand_num; i++){    // Check y0, y1 appeared patterns
    bool y0 = NodeValue[Abc_ObjFaninId0(pCo)][i]^Abc_ObjFaninC0(pCo);
    bool y1 = NodeValue[Abc_ObjFaninId1(pCo)][i]^Abc_ObjFaninC1(pCo);
    std::pair<bool, bool> checkPair = std::make_pair(y0,y1);
    bool already = std::find(PatternAppear.begin(), PatternAppear.end(), checkPair) != PatternAppear.end();
    if(already){
      continue;
    } else {
      PatternAppear.push_back(checkPair);
    }
  }
  return PatternAppear;

}

std::vector<int> sat_addclauses(Abc_Ntk_t* pNtk, Abc_Obj_t* pObj, sat_solver* pSat, std::vector<int> added){
  // std::cout<<"Current node:"<<Abc_ObjId(pObj)<<"\n";
  if(std::find(added.begin(), added.end(), Abc_ObjId(pObj)) != added.end()) {
    // this literal added already
  } else if(Abc_ObjIsPi(pObj) || Abc_ObjFaninNum(pObj)==0) {
    // no need for primary input
  } else if(Abc_ObjFaninNum(pObj)==1){
    // add clause (a+c')(a'+c) for c=a
    int fCompl0 = Abc_ObjFaninC0(pObj);
    lit Lit_1[2];
    lit Lit_2[2];

    Lit_1[0] = toLitCond(Abc_ObjId(pObj), 1);
    Lit_1[1] = toLitCond(Abc_ObjFaninId0(pObj), fCompl0);
    sat_solver_addclause(pSat, Lit_1, Lit_1 + 2);
    
    Lit_2[0] = toLitCond(Abc_ObjId(pObj), 0);
    Lit_2[1] = toLitCond(Abc_ObjFaninId0(pObj), !fCompl0);
    sat_solver_addclause(pSat, Lit_2, Lit_2 + 2);
    
    added.push_back(Abc_ObjId(pObj));
    added = sat_addclauses(pNtk, Abc_ObjFanin0(pObj), pSat, added);
  
  } else {
    // add clause (a+c')(b+c')(a'+b'+c) for c=ab
    int fCompl0 = Abc_ObjFaninC0(pObj);
    int fCompl1 = Abc_ObjFaninC1(pObj);
    lit Lit_1[2];
    lit Lit_2[2];
    lit Lit_3[3];

    Lit_1[0] = toLitCond(Abc_ObjId(pObj), 1);
    Lit_1[1] = toLitCond(Abc_ObjFaninId0(pObj), fCompl0);
    sat_solver_addclause(pSat, Lit_1, Lit_1 + 2);
    
    Lit_2[0] = toLitCond(Abc_ObjId(pObj), 1);
    Lit_2[1] = toLitCond(Abc_ObjFaninId1(pObj), fCompl1);
    sat_solver_addclause(pSat, Lit_2, Lit_2 + 2);
    
    Lit_3[0] = toLitCond(Abc_ObjId(pObj), 0);
    Lit_3[1] = toLitCond(Abc_ObjFaninId0(pObj), !fCompl0);
    Lit_3[2] = toLitCond(Abc_ObjFaninId1(pObj), !fCompl1);
    sat_solver_addclause(pSat, Lit_3, Lit_3 + 3);

    added.push_back(Abc_ObjId(pObj));
    added = sat_addclauses(pNtk, Abc_ObjFanin0(pObj), pSat, added);
    added = sat_addclauses(pNtk, Abc_ObjFanin1(pObj), pSat, added);
  }
  return added;
}

std::vector<std::pair<bool, bool>> Lsv_Find_SDC(Abc_Ntk_t* pNtk, Abc_Obj_t* pObj){
  assert( Abc_NtkIsStrash(pNtk) );
   
  std::vector<std::pair<bool, bool>> PatternSDC;
  if ( Abc_ObjFaninNum(pObj)==0 ){  // PIs don't have DCs
    std::cout << "no sdc" << std::endl;
    return PatternSDC;
  }

  // Abc_Ntk_t* pCone = Abc_NtkCreateCone( pNtk, Abc_ObjFanin0(pObj), Abc_ObjName(pObj), 0 );
  // Abc_Obj_t* pObjCone = Abc_NtkObj(pCone, Abc_NtkObjNum(pCone)-1);
  // std::cout<<"node ID: " << Abc_ObjId(pObjCone) << "\n";
  // std::cout<<"total fanin: " << Abc_ObjFaninNum(pObjCone) << "\n";
  // std::cout<<"y0 ID" << Abc_ObjFaninId0(pObjCone) << " y1 ID" << Abc_ObjFaninId1(pObjCone) << "\n";

  std::vector<std::pair<bool, bool>> Patterns = {{0, 0}, {0, 1}, {1, 0}, {1, 1}};
  // std::vector<std::pair<bool, bool>> PatternAppear = randomSim(pCone, pObjCone);
  std::vector<std::pair<bool, bool>> PatternAppear = randomSim(pNtk, pObj);

  for(auto pattern: Patterns){
    if(std::find(PatternAppear.begin(), PatternAppear.end(), pattern) != PatternAppear.end())
      continue;

    std::vector<int> added;
    sat_solver* pSat = sat_solver_new();
    added = sat_addclauses(pNtk, Abc_ObjFanin0(pObj), pSat, added);
    added = sat_addclauses(pNtk, Abc_ObjFanin1(pObj), pSat, added);
    lit Lit_0[1];
    Lit_0[0] = toLitCond(Abc_ObjFaninId0(pObj), !pattern.first^Abc_ObjFaninC0(pObj));
    sat_solver_addclause(pSat, Lit_0, Lit_0 + 1);
    Lit_0[0] = toLitCond(Abc_ObjFaninId1(pObj), !pattern.second^Abc_ObjFaninC1(pObj));
    sat_solver_addclause(pSat, Lit_0, Lit_0 + 1);

    if (sat_solver_solve(pSat, NULL, NULL, 0, 0, 0, 0) == l_False){
      PatternSDC.push_back(pattern);
    }
  }
  return PatternSDC;
}

std::vector<std::pair<bool, bool>> Lsv_Find_ODC(Abc_Ntk_t* pNtk, Abc_Obj_t* pObj){
  assert( Abc_NtkIsStrash(pNtk) );

  Abc_Ntk_t* pNtk_neg = Abc_NtkDup(pNtk);
  Abc_Obj_t* pObj_neg = Abc_NtkObj(pNtk_neg, Abc_ObjId(pObj));
  Abc_Obj_t* pObj_neg_fo;
  int i;
  std::vector<std::pair<bool, bool>> Patterns = {{0, 0}, {0, 1}, {1, 0}, {1, 1}};
  std::vector<std::pair<bool, bool>> PatternODC;
  std::vector<std::pair<bool, bool>> PatternCare;
  std::vector<std::pair<bool, bool>> PatternSDC = Lsv_Find_SDC(pNtk, pObj);

  // C′ be the same as C except that node n is negated.
  Abc_ObjForEachFanout(pObj_neg, pObj_neg_fo, i){
    if(Abc_ObjFaninId0(pObj_neg_fo) == Abc_ObjId(pObj_neg)){
      pObj_neg_fo->fCompl0 ^= 1;
    } else {
      pObj_neg_fo->fCompl1 ^= 1;
    }
  }

  Abc_Ntk_t* pMiter = Abc_NtkMiter(pNtk, pNtk_neg, 1, 0, 0, 0);  
  Abc_Obj_t* pObj_Po;
  std::vector<int> added;
  sat_solver* pSat = sat_solver_new();

  Abc_NtkForEachPo(pMiter,pObj_Po,i){
    added = sat_addclauses(pMiter, pObj_Po, pSat, added);
    // Force the miter output be 1
    lit Lits[1];
    Lits[0] = toLitCond(Abc_ObjId(pObj_Po), 0);
    sat_solver_addclause(pSat, Lits, Lits + 1);
  }
  
  Abc_Obj_t* pObj_miter = Abc_NtkObj(pMiter, Abc_ObjId(pObj));
  if(pObj_miter == NULL){
    return PatternODC;
  }

  int pattern_num = 4;
  // solve ALLSAT 
  for(int i=pattern_num;i>0;i--){
    if (sat_solver_solve(pSat, NULL, NULL, 0, 0, 0, 0) == l_False){
      break;
    } else {
      int fCompl0 = Abc_ObjFaninC0(pObj_miter);
      int fCompl1 = Abc_ObjFaninC1(pObj_miter);
      bool v0 = sat_solver_var_value(pSat, Abc_ObjFaninId0(pObj_miter))^fCompl0;
		  bool v1 = sat_solver_var_value(pSat, Abc_ObjFaninId1(pObj_miter))^fCompl1;
      PatternCare.push_back(std::make_pair(v0,v1));
      // std::cout<<"Pushback caresets:" << v0 << v1 <<"\n";

      // block the assignment (v0, v1)
      lit Lits[2];
      Lits[0] = toLitCond(Abc_ObjFaninId0(pObj_miter), v0^fCompl0);
      Lits[1] = toLitCond(Abc_ObjFaninId1(pObj_miter), v1^fCompl1);
      sat_solver_addclause(pSat, Lits, Lits + 2);      
    }
  }

  for(auto pattern: Patterns){
    if(std::find(PatternCare.begin(), PatternCare.end(), pattern) != PatternCare.end() || 
        std::find(PatternSDC.begin(), PatternSDC.end(), pattern) != PatternSDC.end()) {
      continue;
    } else {
      PatternODC.push_back(pattern);
    }
  }
  return PatternODC;
}

int Lsv_CommandPrintSDC(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  Abc_Obj_t* pObj;
  int c, k;
  std::vector<std::pair<bool, bool>> PatternSDC;
  
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
    switch (c) {
      case 'h':
        goto usage;
      default:
        goto usage;
    }
  }
  if ( globalUtilOptind >= argc )
  {
      Abc_Print( -1, "Command line lsv_sdc should be followed by a node ID (in integer).\n" );
      goto usage;
  }
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }

  k = atoi(argv[globalUtilOptind]);
  if (k < 1 || k >= Abc_NtkObjNum(pNtk)) {
    Abc_Print(-1, "You must enter an internal node ID.\n");
    goto usage;
  }

  pObj = Abc_NtkObj(pNtk, k);
  if ( Abc_ObjIsPo(pObj) )
  {
    Abc_Print( -1, "This is an ID of Co, please enter an internal node ID.\n" );
    goto usage;
  }
  if ( Abc_ObjIsPi(pObj) )
  {
    Abc_Print( -1, "This is an ID of Ci, please enter an internal node ID.\n" );
    goto usage;
  }

  PatternSDC = Lsv_Find_SDC(pNtk, pObj);
  if (PatternSDC.empty()) {
    Abc_Print(-2, "no sdc\n");
  } else {
    for (auto pattern: PatternSDC) {
      Abc_Print(-2, "%d%d ", pattern.first, pattern.second);
    }
    Abc_Print(-2, "\n");
  }

  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sdc [-h] <node>\n");
  Abc_Print(-2, "\t        prints all the minterms of the satisfiability don't cares\n");
  Abc_Print(-2, "\t<node>: the ID of the node we want\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

int Lsv_CommandPrintODC(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c, k;
  Abc_Obj_t* pObj;
  std::vector<std::pair<bool, bool>> PatternODC;

  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
    switch (c) {
      case 'h':
        goto usage;
      default:
        goto usage;
    }
  }
  if ( globalUtilOptind >= argc )
  {
      Abc_Print( -1, "Command line lsv_odc should be followed by a node ID (in integer).\n" );
      goto usage;
  }
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }

  k = atoi(argv[globalUtilOptind]);
  if (k < 1 || k > Abc_NtkObjNum(pNtk)) {
    Abc_Print(-1, "You must enter a node ID.\n");
    goto usage;
  }

  pObj = Abc_NtkObj(pNtk, k);
  if ( Abc_ObjIsPo(pObj) )
  {
    Abc_Print( -1, "This is an ID of Co, please enter a node ID.\n" );
    goto usage;
  }
  if ( Abc_ObjIsPi(pObj) )
  {
    Abc_Print( -1, "This is an ID of Ci, please enter a node ID.\n" );
    goto usage;
  }

  PatternODC = Lsv_Find_ODC(pNtk, pObj);
  if (PatternODC.empty()) {
    Abc_Print(-2, "no odc\n");
  } else {
    for (auto pattern: PatternODC) {
      Abc_Print(-2, "%d%d ", pattern.first, pattern.second);
    }
    Abc_Print(-2, "\n");
  }
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_odc [-h] <node>\n");
  Abc_Print(-2, "\t        prints all the minterms of the observability don't cares\n");
  Abc_Print(-2, "\t<node>: the ID of the node we want\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}