#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/cnf/cnf.h"
extern "C"{
  Aig_Man_t* Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
}
#include <set>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <algorithm>

using namespace std;

static int Lsv_SDCcompute(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_ODCcompute(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sdc", Lsv_SDCcompute, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_odc", Lsv_ODCcompute, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

int * Abc_NtkVerifySimulatePattern_modified( Abc_Ntk_t * pNtk, int * pModel )
{
    Abc_Obj_t * pNode;
    int * pValues, Value0, Value1, i;
    int fStrashed = 0;
    if ( !Abc_NtkIsStrash(pNtk) )
    {
        pNtk = Abc_NtkStrash(pNtk, 0, 0, 0);
        fStrashed = 1;
    }
/*
    printf( "Counter example: " );
    Abc_NtkForEachCi( pNtk, pNode, i )
        printf( " %d", pModel[i] );
    printf( "\n" );
*/
    // increment the trav ID
    Abc_NtkIncrementTravId( pNtk );
    // set the CI values
    Abc_AigConst1(pNtk)->pCopy = (Abc_Obj_t *)1;
    Abc_NtkForEachCi( pNtk, pNode, i )
        pNode->pCopy = (Abc_Obj_t *)(ABC_PTRINT_T)pModel[i];
    // simulate in the topological order
    Abc_NtkForEachNode( pNtk, pNode, i )
    {
        Value0 = ((int)(ABC_PTRINT_T)Abc_ObjFanin0(pNode)->pCopy) ^ (Abc_ObjFaninC0(pNode) ? ~0 : 0);
        Value1 = ((int)(ABC_PTRINT_T)Abc_ObjFanin1(pNode)->pCopy) ^ (Abc_ObjFaninC1(pNode) ? ~0 : 0);
        pNode->pCopy = (Abc_Obj_t *)(ABC_PTRINT_T)(Value0 & Value1);
    }
    // fill the output values
    pValues = ABC_ALLOC( int, Abc_NtkCoNum(pNtk) );
    Abc_NtkForEachCo( pNtk, pNode, i )
        pValues[i] = ((int)(ABC_PTRINT_T)Abc_ObjFanin0(pNode)->pCopy) ^ (Abc_ObjFaninC0(pNode) ? ~0 : 0);
    if ( fStrashed )
        Abc_NtkDelete( pNtk );
    return pValues;
}

int * SimulateCircuitWithRandomInputs(Abc_Ntk_t *y_cone, int *observedPatterns) {
  
  int numPIs = Abc_NtkCiNum(y_cone);
  int *pModel = (int *)malloc(numPIs * sizeof(int));

  for (int i = 0; i < numPIs; i++) {
      pModel[i] = rand();
  }

  int *pValues = Abc_NtkVerifySimulatePattern_modified(y_cone, pModel);

  int outputPattern, out0, out1;
  for (int i = 31; i >= 0; i--) {
    out0 = (pValues[0] >> i) & 1;
    out1 = (pValues[1] >> i) & 1;
    outputPattern = (out0 << 1) | out1;
    observedPatterns[outputPattern] = 1;
  }

  free(pModel);
  ABC_FREE(pValues);
  return observedPatterns;
}
vector<vector<int>> SDC_check_sat(Abc_Ntk_t* pNtk,int n) {
  Abc_Obj_t *obj;
  Aig_Obj_t *po;
  lit clause[2];
  int pattern[4] = {0};
  vector<vector<int>> output;
  int i;

  Abc_Obj_t *target = Abc_NtkObj(pNtk, n);
  Vec_Ptr_t *fanin = Vec_PtrAlloc(2);
  Vec_PtrPush(fanin, Abc_ObjFanin0(target));
  Vec_PtrPush(fanin, Abc_ObjFanin1(target));
  Abc_Ntk_t *y_cone = Abc_NtkCreateConeArray(pNtk, fanin, 1);
  SimulateCircuitWithRandomInputs(y_cone, pattern);
  // Abc_NtkForEachObj(y_cone,obj,i){
  //   cout <<Abc_ObjId(obj)<<" ";
  // }
  // cout<<endl;
  // cout<<endl;
  Aig_Man_t *y_aig = Abc_NtkToDar(y_cone,0,0);
  Cnf_Dat_t *y_cnf = Cnf_Derive(y_aig,2);
  // cout << Abc_ObjId(y0)<<endl;
  // Cnf_CnfForClause(y_cnf,pBeg,pEnd,i){
  //   for(j = 0; &pBeg[j] < pEnd; j++){
  //     cout <<pBeg[j]<<" ";
  //   }
  //   cout<<endl;
  // }

  // Cnf_DataLift(y1_cnf, y0_cnf->nVars);
  // Abc_NtkForEachPi(y1_cone, obj, i){
  //   pCnf->pVarNums[obj->ID];
  // }
  sat_solver *sat = sat_solver_new();

  for (int p = 0; p < 4; p++) {
    if(pattern[p] == 1){
      continue;
    }
    Cnf_DataWriteIntoSolverInt(sat, y_cnf, 1, 0);

    int y[2];
    y[0] = (p >> 1) & 1;
    y[1] = p & 1;
    
    Aig_ManForEachCo(y_aig, po, i){
      clause[i] = Abc_Var2Lit(y_cnf->pVarNums[Aig_ObjId(po)], !y[i]);
      //cout << clause[i]<<endl;
    }

    sat_solver_addclause(sat, clause, clause+1);
    sat_solver_addclause(sat, clause+1, clause+2);

    if (sat_solver_solve(sat, NULL, NULL, 0, 0, 0, 0) == l_False) {
      vector<int> v;
      if(Abc_ObjFaninC0(target)){
        v.push_back(!y[0]);
      }else{
        v.push_back(y[0]);
      }
      if(Abc_ObjFaninC1(target)){
        v.push_back(!y[1]);
      }else{
        v.push_back(y[1]);
      }
      output.push_back(v);
    }
    sat_solver_restart(sat);
  }
  return output;
}

vector<vector<int>> ODC_check_sat(Abc_Ntk_t* pNtk1, int n) {
  Abc_Obj_t *pFanout, *obj;
  Aig_Obj_t *po;
  int i, EdgeNum;
  lit clause[3];
  vector<vector<int>> output;

  //Abc_Obj_t *target1 = Abc_NtkObj(pNtk1, n);
  Abc_Ntk_t *pNtk2 = Abc_NtkDup(pNtk1);
  // Abc_NtkForEachObj(pNtk1,obj,i){
  //   cout <<Abc_ObjId(obj)<<" ";
  // }
  // cout<<endl;
  // cout<<endl;
  // Abc_NtkForEachObj(pNtk2,obj,i){
  //   cout <<Abc_ObjId(obj)<<" ";
  // }
  // Abc_NtkForEachPi(pNtk1,obj,i){
  //   cout <<Abc_ObjId(obj)<<" ";
  // }
  // cout<<endl;
  Abc_Obj_t *target = Abc_NtkObj(pNtk2, n);
  Abc_ObjForEachFanout( target, pFanout, i ){
    EdgeNum = Abc_ObjFanoutEdgeNum( target, pFanout );
    Abc_ObjXorFaninC( pFanout, EdgeNum );
  }
  // Abc_NtkForEachObj(pNtk2,obj,i){
  //   cout <<Abc_ObjId(obj)<<" ";
  // }
  //cout<<"a"<<endl;
  Abc_Ntk_t *miter = Abc_NtkMiter(pNtk1, pNtk2, 1, 0, 0, 0);
  // Abc_NtkForEachObj(miter,obj,i){
  //   cout <<Abc_ObjId(obj)<<" ";
  // }
  // cout<<endl;
  // cout << Abc_NtkPoNum(miter);
  // cout<<"c"<<endl;
  // Aig_Man_t *y_aig = Abc_NtkToDar(miter,0,0);
  // Aig_ManShow(y_aig,0,NULL);
  //cout<<"c"<<endl;
  target = Abc_NtkObj(pNtk1, n);
  Vec_Ptr_t *fanin = Vec_PtrAlloc(2);
  Vec_PtrPush(fanin, Abc_ObjFanin0(target));
  Vec_PtrPush(fanin, Abc_ObjFanin1(target));
  Abc_Ntk_t *y_cone = Abc_NtkCreateConeArray(pNtk1, fanin, 1);
  Abc_NtkAppend(miter,y_cone,1);
  // cout<<"b"<<endl;
  Aig_Man_t *y_aig = Abc_NtkToDar(miter,0,0);
  //Aig_ManShow(y_aig,0,NULL);
  Cnf_Dat_t *y_cnf = Cnf_Derive(y_aig,3);
  sat_solver *sat = sat_solver_new();
  //cout<<"d"<<endl;
  for (int p = 0; p < 4; p++) {
    Cnf_DataWriteIntoSolverInt(sat, y_cnf, 1, 0);

    int y[2];
    y[0] = (p >> 1) & 1;
    y[1] = p & 1;
    
    Aig_ManForEachCo(y_aig, po, i){
      //cout << Aig_ObjId(po)<<endl;
      if(i == 0){
        clause[i] = Abc_Var2Lit(y_cnf->pVarNums[Aig_ObjId(po)], 0);
      }
      else{
        clause[i] = Abc_Var2Lit(y_cnf->pVarNums[Aig_ObjId(po)], !y[i-1]);
      }
      //cout << clause[i]<<endl;
    }

    sat_solver_addclause(sat, clause, clause+1);
    sat_solver_addclause(sat, clause+1, clause+2);
    sat_solver_addclause(sat, clause+2, clause+3);
    
    if (sat_solver_solve(sat, NULL, NULL, 0, 0, 0, 0) == l_False) {
      vector<int> v;

      if(Abc_ObjFaninC0(target)){
        v.push_back(!y[0]);
      }else{
        v.push_back(y[0]);
      }
      if(Abc_ObjFaninC1(target)){
        v.push_back(!y[1]);
      }else{
        v.push_back(y[1]);
      }
      output.push_back(v);
      //cout << v[0] << " "<< v[1] << endl;
    }
    sat_solver_restart(sat);
  }
  return output;
}
bool compare(const std::vector<int>& a, const std::vector<int>& b) {
    int valA = a[0]*2 + a[1];
    int valB = b[0]*2 + b[1];
    return valA < valB;
}
void print_result_s(vector<vector<int>> out){
  if(out.size() == 0){
    printf("no sdc\n");
  }else{
    sort(out.begin(), out.end(), compare);
    for (int p = 0; p < out.size(); p++){
      printf("%d%d ",out[p][0],out[p][1]);
    }
    printf("\n");
  }
}
void print_result_o(vector<vector<int>> out_s, vector<vector<int>> out_o){
  auto it = out_o.begin();
  while (it != out_o.end()) {
    if (find(out_s.begin(), out_s.end(), *it) != out_s.end()) {
      it = out_o.erase(it);
    } else {
      ++it;
    }
  }
  if(out_o.size() == 0){
    printf("no odc\n");
  }else{
    sort(out_o.begin(), out_o.end(), compare);
    for (int p = 0; p < out_o.size(); p++){
      printf("%d%d ",out_o[p][0],out_o[p][1]);
    }
    printf("\n");
  }
}
int Lsv_SDCcompute(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c;

  int k = atoi(argv[1]);
  vector<vector<int>> out;
  // Abc_Obj_t* pPo, *pPi;
  // int i, id;
  // unordered_map<int, set<set<int>>> umap;

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
  out = SDC_check_sat(pNtk, k);
  print_result_s(out);
  //cout <<Abc_NtkPiNum( pNtk ) <<endl;
  // Abc_NtkForEachPi(pNtk, pPi, i) {
  //   id = Abc_ObjId(pPi);
  //   set<int> cut{id};
  //   set<set<int>> v;
  //   v.insert(cut);
  //   umap[id] = v;
  // }
  // Abc_NtkForEachPo(pNtk, pPo, i) {
  //   Lsv_recurNtk(umap, pPo, k);
  // }
  // PrintNode(umap);

  return 0;

usage:
  Abc_Print(-2, "usage: lsv_print_nodes [-h]\n");
  Abc_Print(-2, "\t        prints the nodes in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

int Lsv_ODCcompute(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c;
  vector<vector<int>> out_s,out_o;
  int k = atoi(argv[1]);

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
  out_s = SDC_check_sat(pNtk, k);
  out_o = ODC_check_sat(pNtk, k);
  print_result_o(out_s,out_o);
  //Lsv_check_sat(pNtk, k);
  //cout <<Abc_NtkPiNum( pNtk ) <<endl;
  // Abc_NtkForEachPi(pNtk, pPi, i) {
  //   id = Abc_ObjId(pPi);
  //   set<int> cut{id};
  //   set<set<int>> v;
  //   v.insert(cut);
  //   umap[id] = v;
  // }
  // Abc_NtkForEachPo(pNtk, pPo, i) {
  //   Lsv_recurNtk(umap, pPo, k);
  // }
  // PrintNode(umap);

  return 0;

usage:
  Abc_Print(-2, "usage: lsv_print_nodes [-h]\n");
  Abc_Print(-2, "\t        prints the nodes in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}