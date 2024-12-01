#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <vector>
#include <set>
#include <algorithm>
#include <iterator>
#include "sat/cnf/cnf.h"
extern "C"{
Aig_Man_t* Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
}

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintCuts(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintSDC(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintODC(Abc_Frame_t* pAbc, int argc, char** argv);
static void Lsv_CommandPrintNodes_Usage();
static void Lsv_CommandPrintCuts_Usage();
static void Lsv_CommandPrintSDC_Usage();
static void Lsv_CommandPrintODC_Usage();

//-------------------------------------------------------------------------

void LSV_init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_CommandPrintCuts, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sdc", Lsv_CommandPrintSDC, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_odc", Lsv_CommandPrintODC, 0);
}

void LSV_destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {LSV_init, LSV_destroy};

//-------------------------------------------------------------------------

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

//-------------------------------------------------------------------------

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
        Lsv_CommandPrintNodes_Usage();
        return 1;
      default:
        Lsv_CommandPrintNodes_Usage();
        return 1;
    }
  }
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  Lsv_NtkPrintNodes(pNtk);
  return 0;
}

void Lsv_CommandPrintNodes_Usage() {
  Abc_Print(-2, "usage: lsv_print_nodes [-h]\n");
  Abc_Print(-2, "\t        prints the nodes in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return;
}

//-------------------------------------------------------------------------

struct id_cuts_pair {
  int id;
  std::vector<std::set<Abc_Obj_t*>> cuts;
};

std::vector<std::set<Abc_Obj_t*>> ComputeKFeasibleCuts(std::vector<id_cuts_pair>& all_cuts, Abc_Obj_t* pObj, int k) {
  id_cuts_pair id_cuts;
  id_cuts.id = Abc_ObjId(pObj);
  std::vector<std::set<Abc_Obj_t*>> cuts;
  std::set<Abc_Obj_t*> cut;
  for (const auto& id_cut : all_cuts) {
    if (id_cut.id == Abc_ObjId(pObj)) {
      return id_cut.cuts;
    }
  }
  if (Abc_ObjIsPi(pObj)) {
    cut.insert(pObj);
    cuts.push_back(cut);
    id_cuts.cuts = cuts;
    all_cuts.push_back(id_cuts);
    return cuts;
  }

  Abc_Obj_t* pFanin;
  int i;
  std::vector<std::set<Abc_Obj_t*>> cuts1;
  std::vector<std::set<Abc_Obj_t*>> cuts2;
  Abc_ObjForEachFanin(pObj, pFanin, i) {
    if (i == 0) {
      cuts1 = ComputeKFeasibleCuts(all_cuts, pFanin, k);
      }
    if (i == 1) {
      cuts2 = ComputeKFeasibleCuts(all_cuts, pFanin, k);
      }
  }
  for (const auto& cut1 : cuts1) {
    for (const auto& cut2 : cuts2) {
      if (cut1.size() + cut2.size() <= k) {
        cut.insert(cut1.begin(), cut1.end());
        cut.insert(cut2.begin(), cut2.end());
        cuts.push_back(cut);
        cut.clear();
      }
    }
  }
  cut.insert(pObj);
  cuts.push_back(cut);
  id_cuts.cuts = cuts;
  all_cuts.push_back(id_cuts);
  return cuts;
}

int Lsv_CommandPrintCuts(Abc_Frame_t* pAbc, int argc, char** argv) {
  int c;
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
    switch (c) {
      case 'h':
        Lsv_CommandPrintCuts_Usage();
        return 1;
      default:
        Lsv_CommandPrintCuts_Usage();
        return 1;
    }
  }

  if (argc != 2) {
    Lsv_CommandPrintCuts_Usage();
    return 1;
  }
  int k = atoi(argv[1]);
  if (k < 3 || k > 6) {
    Lsv_CommandPrintCuts_Usage();
    return 1;
  }

  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }

  std::vector<std::set<Abc_Obj_t*>> cuts;
  std::vector<id_cuts_pair> all_cuts;  // all_cuts[i] is the cuts of node i
  all_cuts.reserve(Abc_NtkObjNumMax(pNtk));
  Abc_Obj_t* pObj;
  int i;
  Abc_NtkForEachObj(pNtk, pObj, i) {
    cuts = ComputeKFeasibleCuts(all_cuts, pObj, k);
    for (const auto& cut : cuts) {
      printf("%d:", Abc_ObjId(pObj));
        for (auto& c1 : cut) {
          printf(" %d", Abc_ObjId(c1));
        }
      printf("\n");
    }
  }

  return 0;
}

void Lsv_CommandPrintCuts_Usage(){
  Abc_Print(-2, "usage: lsv_printcut [-h] <k>\n");
  Abc_Print(-2, "\t        prints k-feasible cuts for each nodes in the network, where 3 <= k <= 6\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  Abc_Print(-2, "\t<k>   : the number of feasible cuts\n");
  return;
}

//-------------------------------------------------------------------------

int * RandomSimulation(Abc_Ntk_t * pNtk, int * pModel)
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
      if ((int)Abc_ObjFaninC0(pNode) == 1) {
        Value0 = ~(int)(ABC_PTRINT_T)Abc_ObjFanin0(pNode)->pCopy;
      }else{
        Value0 = (int)(ABC_PTRINT_T)Abc_ObjFanin0(pNode)->pCopy;
      }
      if ((int)Abc_ObjFaninC1(pNode) == 1) {
        Value1 = ~(int)(ABC_PTRINT_T)Abc_ObjFanin1(pNode)->pCopy;
      }else{
        Value1 = (int)(ABC_PTRINT_T)Abc_ObjFanin1(pNode)->pCopy;
      }
      pNode->pCopy = (Abc_Obj_t *)(ABC_PTRINT_T)(Value0 & Value1);
    }
    // fill the output values
    assert ( Abc_NtkCoNum(pNtk) == 1 );
    pValues = ABC_ALLOC( int, 2 );
    Abc_NtkForEachCo( pNtk, pNode, i ){
      Abc_Obj_t * pFanin = Abc_ObjFanin0(pNode);
      if ((int)Abc_ObjFaninC0(pFanin) == 1) {
        pValues[0] = ~(int)(ABC_PTRINT_T)Abc_ObjFanin0(pFanin)->pCopy;
      }else{
        pValues[0] = (int)(ABC_PTRINT_T)Abc_ObjFanin0(pFanin)->pCopy;
      }
      if ((int)Abc_ObjFaninC1(pFanin) == 1) {
        pValues[1] = ~(int)(ABC_PTRINT_T)Abc_ObjFanin1(pFanin)->pCopy;
      }else{ 
        pValues[1] = (int)(ABC_PTRINT_T)Abc_ObjFanin1(pFanin)->pCopy;
      }
    }
    if ( fStrashed ){
      Abc_NtkDelete( pNtk );
    }
    return pValues;
}

int checksat(Abc_Ntk_t* pNtk, Abc_Obj_t* pObj, int value0, int value1) {
  Aig_Man_t * pFanin0cone = Abc_NtkToDar(Abc_NtkCreateCone(pNtk, Abc_ObjFanin0(pObj), "Fanin0cone", 0), 0, 0);
  Aig_Man_t * pFanin1cone = Abc_NtkToDar(Abc_NtkCreateCone(pNtk, Abc_ObjFanin1(pObj), "Fanin1cone", 0), 0, 0);
  sat_solver * pSat = sat_solver_new();
  Cnf_Dat_t * pCnf0 = Cnf_Derive(pFanin0cone, 1);
  Cnf_Dat_t * pCnf1 = Cnf_Derive(pFanin1cone, 1);
  Cnf_DataWriteIntoSolverInt(pSat, pCnf0, 1, 0);
  Cnf_DataLift(pCnf1, pCnf0->nVars - 1);
  Cnf_DataWriteIntoSolverInt(pSat, pCnf1, 1, 0);
  int pLits[1]
  pLits[0] = toLitCond(pCnf0->pVarNums[Abc_ObjId(Abc_ObjFanin0(pObj))], value0);
  sat_solver_addclause(pSat, pLits, pLits + 1);
  pLits[0] = toLitCond(pCnf1->pVarNums[Abc_ObjId(Abc_ObjFanin1(pObj))], value1);
  sat_solver_addclause(pSat, pLits, pLits + 1);
  int i;
  int pLits[2];
  Aig_ManForEachCi(pFanin0cone, pObj0, i) {
    pLits[0] = toLitCond(pCnf0->pVarNums[Abc_ObjId(pObj0)], 0);
    sat_solver_addclause(pSat, pLits, pLits + 1);
  }
  
  int status = sat_solver_solve(pSat, NULL, NULL, 0, 0, 0, 0);
  sat_solver_delete(pSat);
  return status;
}

int Lsv_CommandPrintSDC(Abc_Frame_t* pAbc, int argc, char** argv) {
  int c;
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
    switch (c) {
      case 'h':
        Lsv_CommandPrintSDC_Usage();
        return 1;
      default:
        Lsv_CommandPrintSDC_Usage();
        return 1;
    }
  }

  if (argc != 2) {
    Lsv_CommandPrintSDC_Usage();
    return 1;
  }
  Abc_Ntk_t * pNtk = Abc_FrameReadNtk(pAbc);
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  int k = atoi(argv[1]);
  if (k > Abc_NtkObjNumMax(pNtk)) {
    Lsv_CommandPrintSDC_Usage();
    return 1;
  }

  Abc_Obj_t * pObj = Abc_NtkObj(pNtk, k);
  if (Abc_ObjIsCo(pObj) || Abc_ObjIsCi(pObj)) {
    printf("no sdc\n");
    return 0;
  }
  Abc_Ntk_t * pCone = Abc_NtkCreateCone(pNtk, pObj, "cone", 0);
  int * pModel = new int[Abc_NtkCiNum(pCone)];
  for (int i = 0; i < pow(2, std::min(Abc_NtkCiNum(pCone), 32)); i++) {
    for (int j = 0; j < Abc_NtkCiNum(pCone); j++) {
      pModel[j] = (pModel[j]<<1) + ((i >> j) & 1);
    }
  }
  int * pValues = RandomSimulation(pCone, pModel);
  printf("%d %d\n", pValues[0], pValues[1]);

  int num00=0, num01=0, num10=0, num11=0;
  int pValue0, pValue1;
  for (int i = 0; i < pow(2, std::min(Abc_NtkCiNum(pCone), 32)); i++) {
    pValue0 = pValues[0]%2;
    pValue1 = pValues[1]%2;
    pValues[0] = pValues[0] >> 1;
    pValues[1] = pValues[1] >> 1;
    if (pValue0 == 0 && pValue1 == 0) {
      num00++;
    }else if (pValue0 == 0 && pValue1 == 1) {
      num01++;
    }else if (pValue0 == 1 && pValue1 == 0) {
      num10++;
    }else if (pValue0 == 1 && pValue1 == 1) {
      num11++;
    }
  }
  bool sdc00 = false , sdc01 = false, sdc10 = false, sdc11 = false;
  if (num00 == 0){
    if (!checksat(pNtk, pObj, 0, 0)){
      sdc00 = true;
      printf("00 ");
    }
  }
  if (num01 == 0){
    if (!checksat(pNtk, pObj, 0, 1)){
      sdc01 = true;
      printf("01 ");
    }
  }
  if (num10 == 0){
    if (!checksat(pNtk, pObj, 1, 0)){
      sdc10 = true;
      printf("10 ");
    }
  }
  if (num11 == 0){
    if (!checksat(pNtk, pObj, 1, 1)){
      sdc11 = true;
      printf("11 ");
    }
  }
  if (!sdc00 && !sdc01 && !sdc10 && !sdc11){
    printf("no sdc\n");
    return 0;
  }
  printf("\n");

  return 0;
}

void Lsv_CommandPrintSDC_Usage() {
  Abc_Print(-2, "usage: lsv_sdc [-h] <k>\n");
  Abc_Print(-2, "\t        prints the SDCs of the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  Abc_Print(-2, "\t<k>   : the node that will be computed\n");
  return;
}

//-------------------------------------------------------------------------

int ComputeODC() {
  return 0;
}

int Lsv_CommandPrintODC(Abc_Frame_t* pAbc, int argc, char** argv) {
  int c;
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
    switch (c) {
      case 'h':
        Lsv_CommandPrintODC_Usage();
        return 1;
      default:
        Lsv_CommandPrintODC_Usage();
        return 1;
    }
  }

  if (argc != 2) {
    Lsv_CommandPrintODC_Usage();
    return 1;
  }
  int k = atoi(argv[1]);
  if (k < 3 || k > 6) {
    Lsv_CommandPrintODC_Usage();
    return 1;
  }


  return 0;
}

void Lsv_CommandPrintODC_Usage() {
  Abc_Print(-2, "usage: lsv_odc [-h] <k>\n");
  Abc_Print(-2, "\t        prints the ODCs of the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  Abc_Print(-2, "\t<k>   : the node that will be computed\n");
  return;
}