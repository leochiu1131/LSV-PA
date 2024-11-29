#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "aig/aig/aig.h"
#include <iostream>
#include <map>
#include <vector>
#include <set>
#include <cmath>
#include <iomanip>
#include "sat/cnf/cnf.h"
#include <bitset>


extern "C"{
Aig_Man_t* Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
}
using namespace std;



static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintCuts (Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintsdc  (Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintodc  (Abc_Frame_t* pAbc, int argc, char** argv);


void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut"   , Lsv_CommandPrintCuts , 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sdc"        , Lsv_CommandPrintsdc  , 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_odc"        , Lsv_CommandPrintodc  , 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;



// -------------------------------------------------- example ---------------------------------------------------------
void Lsv_NtkPrintNodes(Abc_Ntk_t* pNtk) {
  Abc_Obj_t* pObj;
  int i;
  Abc_NtkForEachNode(pNtk, pObj, i) {
    printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
    Abc_Obj_t* pFanin;
    Abc_Obj_t* pFanout;
    int j;
    int k;
    Abc_ObjForEachFanin(pObj, pFanin, j) {
      printf("  Fanin-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanin),
             Abc_ObjName(pFanin));
    }

    Abc_ObjForEachFanout(pObj, pFanout, k) {
      printf("  Fanout-%d: Id = %d, name = %s\n", k, Abc_ObjId(pFanout),
            Abc_ObjName(pFanout));
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


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////    PA1   /////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
Abc_NtkForEachCi
Abc_NtkForEachCO
abc_objfanino // obj pointer
Abc_Obj FaninIdO
V/ obj id */



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
  
  
  for (const auto& [key, value] : hash_table) {
        for (const auto& inner_set : value) {
            cout << key << ": ";
            for (const auto& element : inner_set) {
                cout << element << " ";
            }
            cout << endl;
        }
  }
}


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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////    PA2   /////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////






////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////   part 1  /////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
uint32_t * mysimulate( Abc_Ntk_t * pNtk, uint32_t * pModel )
{
    Abc_Obj_t * pNode;
    uint32_t * pValues, Value0, Value1;
    int i;
    int fStrashed = 0;
    if ( !Abc_NtkIsStrash(pNtk) )
    {
        pNtk = Abc_NtkStrash(pNtk, 0, 0, 0);
        fStrashed = 1;
    }

    // increment the trav ID
    Abc_NtkIncrementTravId( pNtk );
    // set the CI values for 32 bits
    Abc_AigConst1(pNtk)->pCopy = (Abc_Obj_t *)0xFFFFFFFF;
    Abc_NtkForEachCi( pNtk, pNode, i )
        pNode->pCopy = (Abc_Obj_t *)(ABC_PTRINT_T)pModel[i];

    // simulate in the topological order for 32 bits
    Abc_NtkForEachNode( pNtk, pNode, i )
    {
        Value0 = ((uint32_t)(ABC_PTRINT_T)Abc_ObjFanin0(pNode)->pCopy) ^ (Abc_ObjFaninC0(pNode) ? 0xFFFFFFFF : 0);
        Value1 = ((uint32_t)(ABC_PTRINT_T)Abc_ObjFanin1(pNode)->pCopy) ^ (Abc_ObjFaninC1(pNode) ? 0xFFFFFFFF : 0);
        pNode->pCopy = (Abc_Obj_t *)(ABC_PTRINT_T)(Value0 & Value1);
    }

    // fill the output values with 32 bits
    pValues = (uint32_t *)ABC_ALLOC( uint32_t, Abc_NtkCoNum(pNtk) );
    Abc_NtkForEachCo( pNtk, pNode, i )
    {
        pValues[i] = ((uint32_t)(ABC_PTRINT_T)Abc_ObjFanin0(pNode)->pCopy) ^ (Abc_ObjFaninC0(pNode) ? 0xFFFFFFFF : 0);
    }
    if ( fStrashed )
        Abc_NtkDelete( pNtk );

    return pValues;
}

set<vector<int>> Lsv_sdc(Abc_Ntk_t* pNtk, int k){
//////////////////////////////////////////////////////////////////////////////// simulation part ///////////////////////////////////////////////////////////////////////////////
  // the node we want to operate on.
  Abc_Obj_t* pTargetNode = Abc_NtkObj(pNtk, k);
  Abc_Obj_t* pObj;

  int y0 = Abc_ObjId(Abc_ObjFanin0(pTargetNode));
  int y1 = Abc_ObjId(Abc_ObjFanin1(pTargetNode));

  //cout << y0 << " " << y1 << endl;

  Abc_Ntk_t * ntknew = Abc_NtkCreateCone(pNtk, pTargetNode, Abc_ObjName(pTargetNode), 1);
  Abc_Ntk_t * ntk0   = Abc_NtkCreateCone(pNtk, Abc_ObjFanin0(pTargetNode), Abc_ObjName(Abc_ObjFanin0(pTargetNode)), 1);
  Abc_Ntk_t * ntk1   = Abc_NtkCreateCone(pNtk, Abc_ObjFanin1(pTargetNode), Abc_ObjName(Abc_ObjFanin1(pTargetNode)), 1);
  
  int numInputs = Abc_NtkCiNum(ntknew);
  uint32_t pModel[numInputs];
  set<vector<int>> sim;

  uint32_t a = Abc_Random(1);
  uint32_t b = Abc_Random(0);

  pModel[0] = a;
  pModel[1] = b;

  //cout << "a: " << a << " b: " << b << endl;
  //cout << "pModel[0]: " << pModel[0] << " pModel[1]: " << pModel[1] << endl;


  uint32_t *s_val0;
  s_val0 = mysimulate(ntk0, pModel);

  uint32_t *s_val1;
  s_val1 = mysimulate(ntk1, pModel);
  
  for (int i = 31; i >= 0; --i) {
    int bit0 = (s_val0[0] >> i) & 1;  
    int bit1 = (s_val1[0] >> i) & 1;
    if(Abc_ObjFaninC0(pTargetNode)){
      bit0 = !bit0;
    }
    if(Abc_ObjFaninC1(pTargetNode)){
      bit1 = !bit1;
    } 
    sim.insert({bit0, bit1});  
  }

  int dc0[4] = {0, 0, 1, 1};
  int dc1[4] = {0, 1, 0, 1};
  bool patternFound[4] = {0, 0, 0, 0};
  

  for (const auto& pattern : sim) {
    if(pattern[0] == 0 && pattern[1] == 0){
      dc0[0] = -1;
      dc1[0] = -1;
      patternFound[0] = 1;
    }
    else if(pattern[0] == 0 && pattern[1] == 1){
      dc0[1] = -1;
      dc1[1] = -1;  
      patternFound[1] = 1;
    }
    else if(pattern[0] == 1 && pattern[1] == 0){
      dc0[2] = -1;
      dc1[2] = -1;  
      patternFound[2] = 1;
    }
    else if(pattern[0] == 1 && pattern[1] == 1){
      dc0[3] = -1;
      dc1[3] = -1;  
      patternFound[3] = 1;
    }

    //cout << "Unique bit patterns:" << endl;
    //cout << "{" << pattern[0] << ", " << pattern[1] << "}" << endl;
  }
  
  /*cout << "Simulated values:" << endl;
  for(int h = 0; h < 4; h++){
    if(dc0[h] != -1 && dc1[h] != -1)
      cout << dc0[h] << dc1[h] << endl;
  }*/
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////// sat solving ////////////////////////////////////////////////////////////////////////////////////
  //cout << "abc ntknodenum : " << Abc_NtkNodeNum(ntknew) <<endl;
  if(patternFound[0] == 1 && patternFound[1] == 1 && patternFound[2] == 1 && patternFound[3] == 1){
    cout << "no sdc" << endl;
    return {};
  }
  else{
    Vec_Ptr_t *vFanin = Vec_PtrAlloc(2);
    Vec_PtrPushTwo(vFanin, Abc_ObjFanin0(pTargetNode), Abc_ObjFanin1(pTargetNode));
    Abc_Ntk_t *pNtkCone = Abc_NtkCreateConeArray(pNtk, vFanin, 1);
    Vec_PtrFree(vFanin);

    Abc_Obj_t* P0 = Abc_NtkFindNode(pNtkCone, Abc_ObjName(Abc_ObjFanin0(pTargetNode)));
    Abc_Obj_t* P1 = Abc_NtkFindNode(pNtkCone, Abc_ObjName(Abc_ObjFanin1(pTargetNode)));

    Aig_Man_t*  finalcone = Abc_NtkToDar(pNtkCone, 0, 0);

    //Cnf_DataWriteIntoSolverInt(psat, pCnf, 1, 1);

    int test1[4] = {0, 0, 1, 1};
    int test2[4] = {0, 1, 0, 1};
    int valid[4];
    int assu[2];

    set<vector<int>> SatResult;

    for(int j = 0; j < 4; j++){
        sat_solver* psat = sat_solver_new();
        Cnf_Dat_t * pCnf  = Cnf_Derive(finalcone, 2);
        psat = (sat_solver*) Cnf_DataWriteIntoSolverInt(psat, pCnf, 1, 1);
        
        assu[0] = toLitCond(pCnf->pVarNums[Abc_ObjId(P0)], !test1[j]);
        assu[1] = toLitCond(pCnf->pVarNums[Abc_ObjId(P1)], !test2[j]);
        
        valid[j] = sat_solver_solve(psat, assu, assu + 2, 0, 0, 0, 0);
        sat_solver_delete(psat);

        int temp1 = test1[j];
        int temp2 = test2[j];

        if(valid[j] == -1){
          if(Abc_ObjFaninC0(pTargetNode)){
            temp1 = !test1[j];
          }
              
          if(Abc_ObjFaninC1(pTargetNode)){
            temp2 = !test2[j];
          }
          SatResult.insert({temp1, temp2});
        }
    }
    if(SatResult.size() == 0){
      cout << "no sdc" << endl;
    }

    else{
      for (const auto& pattern : SatResult) {
        cout << pattern[0]  << pattern[1] << endl;
      }
    }
    return SatResult;
  }
}




////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////// main function //////////////////////////////////////////////////////////////////////////////////

int Lsv_CommandPrintsdc(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  Extra_UtilGetoptReset();
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  set<vector<int>> h = Lsv_sdc(pNtk, atoi(argv[1]));
  
  return 0;
}





////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////  End of part 1  /////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


set<vector<int>> Lsv_sdcforodc(Abc_Ntk_t* pNtk, int k){
//////////////////////////////////////////////////////////////////////////////// simulation part ///////////////////////////////////////////////////////////////////////////////
  // the node we want to operate on.
  Abc_Obj_t* pTargetNode = Abc_NtkObj(pNtk, k);
  Abc_Obj_t* pObj;

  int y0 = Abc_ObjId(Abc_ObjFanin0(pTargetNode));
  int y1 = Abc_ObjId(Abc_ObjFanin1(pTargetNode));

  //cout << y0 << " " << y1 << endl;

  Abc_Ntk_t * ntknew = Abc_NtkCreateCone(pNtk, pTargetNode, Abc_ObjName(pTargetNode), 1);
  Abc_Ntk_t * ntk0   = Abc_NtkCreateCone(pNtk, Abc_ObjFanin0(pTargetNode), Abc_ObjName(Abc_ObjFanin0(pTargetNode)), 1);
  Abc_Ntk_t * ntk1   = Abc_NtkCreateCone(pNtk, Abc_ObjFanin1(pTargetNode), Abc_ObjName(Abc_ObjFanin1(pTargetNode)), 1);
  
  int numInputs = Abc_NtkCiNum(ntknew);
  uint32_t pModel[numInputs];
  set<vector<int>> sim;

  uint32_t a = Abc_Random(1);
  uint32_t b = Abc_Random(0);

  pModel[0] = a;
  pModel[1] = b;

  //cout << "a: " << a << " b: " << b << endl;
  //cout << "pModel[0]: " << pModel[0] << " pModel[1]: " << pModel[1] << endl;


  uint32_t *s_val0;
  s_val0 = mysimulate(ntk0, pModel);

  uint32_t *s_val1;
  s_val1 = mysimulate(ntk1, pModel);
  
  for (int i = 31; i >= 0; --i) {
    int bit0 = (s_val0[0] >> i) & 1;  
    int bit1 = (s_val1[0] >> i) & 1;
    if(Abc_ObjFaninC0(pTargetNode)){
      bit0 = !bit0;
    }
    if(Abc_ObjFaninC1(pTargetNode)){
      bit1 = !bit1;
    } 
    sim.insert({bit0, bit1});  
  }

  int dc0[4] = {0, 0, 1, 1};
  int dc1[4] = {0, 1, 0, 1};
  bool patternFound[4] = {0, 0, 0, 0};
  

  for (const auto& pattern : sim) {
    if(pattern[0] == 0 && pattern[1] == 0){
      dc0[0] = -1;
      dc1[0] = -1;
      patternFound[0] = 1;
    }
    else if(pattern[0] == 0 && pattern[1] == 1){
      dc0[1] = -1;
      dc1[1] = -1;  
      patternFound[1] = 1;
    }
    else if(pattern[0] == 1 && pattern[1] == 0){
      dc0[2] = -1;
      dc1[2] = -1;  
      patternFound[2] = 1;
    }
    else if(pattern[0] == 1 && pattern[1] == 1){
      dc0[3] = -1;
      dc1[3] = -1;  
      patternFound[3] = 1;
    }

    //cout << "Unique bit patterns:" << endl;
    //cout << "{" << pattern[0] << ", " << pattern[1] << "}" << endl;
  }
  
  /*cout << "Simulated values:" << endl;
  for(int h = 0; h < 4; h++){
    if(dc0[h] != -1 && dc1[h] != -1)
      cout << dc0[h] << dc1[h] << endl;
  }*/
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////// sat solving ////////////////////////////////////////////////////////////////////////////////////
  //cout << "abc ntknodenum : " << Abc_NtkNodeNum(ntknew) <<endl;
  if(patternFound[0] == 1 && patternFound[1] == 1 && patternFound[2] == 1 && patternFound[3] == 1){
    return {};
  }
  else{
    Vec_Ptr_t *vFanin = Vec_PtrAlloc(2);
    Vec_PtrPushTwo(vFanin, Abc_ObjFanin0(pTargetNode), Abc_ObjFanin1(pTargetNode));
    Abc_Ntk_t *pNtkCone = Abc_NtkCreateConeArray(pNtk, vFanin, 1);
    Vec_PtrFree(vFanin);

    Abc_Obj_t* P0 = Abc_NtkFindNode(pNtkCone, Abc_ObjName(Abc_ObjFanin0(pTargetNode)));
    Abc_Obj_t* P1 = Abc_NtkFindNode(pNtkCone, Abc_ObjName(Abc_ObjFanin1(pTargetNode)));

    Aig_Man_t*  finalcone = Abc_NtkToDar(pNtkCone, 0, 0);

    //Cnf_DataWriteIntoSolverInt(psat, pCnf, 1, 1);

    int test1[4] = {0, 0, 1, 1};
    int test2[4] = {0, 1, 0, 1};
    int valid[4];
    int assu[2];

    set<vector<int>> SatResult;

    for(int j = 0; j < 4; j++){
        sat_solver* psat = sat_solver_new();
        Cnf_Dat_t * pCnf  = Cnf_Derive(finalcone, 2);
        psat = (sat_solver*) Cnf_DataWriteIntoSolverInt(psat, pCnf, 1, 1);
        
        assu[0] = toLitCond(pCnf->pVarNums[Abc_ObjId(P0)], !test1[j]);
        assu[1] = toLitCond(pCnf->pVarNums[Abc_ObjId(P1)], !test2[j]);
        
        valid[j] = sat_solver_solve(psat, assu, assu + 2, 0, 0, 0, 0);
        sat_solver_delete(psat);

        int temp1 = test1[j];
        int temp2 = test2[j];

        if(valid[j] == -1){
          if(Abc_ObjFaninC0(pTargetNode)){
            temp1 = !test1[j];
          }
              
          if(Abc_ObjFaninC1(pTargetNode)){
            temp2 = !test2[j];
          }
          SatResult.insert({temp1, temp2});
        }
    }
    
    return SatResult;
  }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////   part 2  /////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Lsv_odc(Abc_Ntk_t* pNtk, int k){
  set<vector<int>> sdc = Lsv_sdcforodc(pNtk, k);
  Abc_Obj_t* pObj0;
  Abc_Obj_t* pObj1;
  //Abc_Obj_t* pObj;
  //Abc_Obj_t* pFanout;
  Abc_Ntk_t* pNtkComplemenet = Abc_NtkDup(pNtk);
  Abc_Obj_t* pTargetNode = Abc_NtkObj(pNtkComplemenet, k);
  Abc_Obj_t* pTargetori = Abc_NtkObj(pNtk, k);


  int fanoutnum = Abc_ObjFanoutNum(pTargetNode);

  int j,l;
  Abc_ObjForEachFanout(pTargetNode, pObj0, j) {
    Abc_ObjForEachFanin(pObj0, pObj1, l){
      if(pObj1 -> Id == pTargetNode -> Id)
        Abc_ObjXorFaninC(pObj0, l);
    }
  }


  /*
  for(int l = 0; l < fanoutnum; l++) {
    cout << "fanout: " << fanout[l] << endl;
    if( (Abc_ObjFanin0(Abc_ObjFanout(pTargetNode, l))) -> Id == pTargetNode -> Id)
      Abc_ObjXorFaninC( Abc_ObjFanin0(Abc_ObjFanout(pTargetNode, l)), 0 );
    else if( (Abc_ObjFanin1(Abc_ObjFanout(pTargetNode, l))) -> Id == pTargetNode -> Id )
      Abc_ObjXorFaninC( Abc_ObjFanin1(Abc_ObjFanout(pTargetNode, l)), 1 );
  }*/


  /*for(int l = 0; l < (pTargetNode -> vFanouts).nSize; l++) {
    cout << "fanout: " << fanout[l] << endl;
    Abc_Obj_t* fanoutnode = Abc_NtkObj(pNtkComplemenet, fanout[l]);
    if( (Abc_ObjFanin0(fanoutnode)) -> Id == k)
      Abc_ObjXorFaninC( Abc_ObjFanin0(fanoutnode), 0 );
    else if( (Abc_ObjFanin1(fanoutnode)) -> Id == k )
      Abc_ObjXorFaninC( Abc_ObjFanin1(fanoutnode), 1 );
  }*/


  Abc_Ntk_t * miter = Abc_NtkMiter(pNtk, pNtkComplemenet, 1, 0, 0, 0);

  Vec_Ptr_t *vFanin = Vec_PtrAlloc(2);
  Vec_PtrPushTwo(vFanin, Abc_ObjFanin0(pTargetori), Abc_ObjFanin1(pTargetori));
  Abc_Ntk_t *pNtkCone = Abc_NtkCreateConeArray(pNtk, vFanin, 1);

  Vec_PtrFree(vFanin);


  int append = Abc_NtkAppend(miter, pNtkCone, 1);
  //Lsv_NtkPrintNodes(miter);


  //cout << "append successfully " << append << endl;
  //Lsv_NtkPrintNodes(miter);
  Aig_Man_t*  finalntk = Abc_NtkToDar(miter, 0, 0);

  Aig_Obj_t* P0 = Aig_ManCo(finalntk, 1);
  Aig_Obj_t* P1 = Aig_ManCo(finalntk, 2);
  Aig_Obj_t* pmiter = Aig_ManCo(finalntk, 0);
  // 7 : P0 id = 13, P1 id = 14

  //cout << P0 -> Id << " " << P1 -> Id << endl;

  int test1[4] = {0, 0, 1, 1};
  int test2[4] = {0, 1, 0, 1};
  int valid[4];
  int assu[2];

  set<vector<int>> SatResult;

  for(int j = 0; j < 4; j++){  
    sat_solver* psat = sat_solver_new();
    Cnf_Dat_t * pCnf  = Cnf_Derive(finalntk, 3);
    psat = (sat_solver*) Cnf_DataWriteIntoSolverInt(psat, pCnf, 1, 1);
    
    
    assu[0] = toLitCond(pCnf->pVarNums[P0 -> Id], !test1[j]);
    assu[1] = toLitCond(pCnf->pVarNums[P1 -> Id], !test2[j]);
    assu[2] = toLitCond(pCnf->pVarNums[pmiter -> Id], !1);

    //cout << "assu: " << assu[0] << " " << assu[1] << endl;  // 7 : P0 id = 13, P1 id
    
    valid[j] = sat_solver_solve(psat, assu, assu + 3, 0, 0, 0, 0);

    int temp1 = test1[j];
    int temp2 = test2[j];
    sat_solver_delete(psat); 
    //cout << "valid: " << valid[j] << endl;

    if(valid[j] == -1){
      if(Abc_ObjFaninC0(pTargetori)){
        temp1 = !test1[j];
      }
          
      if(Abc_ObjFaninC1(pTargetori)){
        temp2 = !test2[j];
      }
      SatResult.insert({temp1, temp2});
    }
  }

  

    for (const auto& elem : sdc) {
      SatResult.erase({elem});
    }

    if(SatResult.size() == 0){
      cout << "no odc" << endl;
    }

    else{
      for (const auto& pattern : SatResult) {
        cout << pattern[0]  << pattern[1] << endl;
      }
    }

}




int Lsv_CommandPrintodc(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  Extra_UtilGetoptReset();
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  Lsv_odc(pNtk, atoi(argv[1]));
  return 0;
}














//////////////////////////////////////////////////////////////////////////////  End of part 2  /////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



















////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



