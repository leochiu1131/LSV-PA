#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <map>
#include <set>
#include <algorithm>
#include "sat/cnf/cnf.h"
extern "C"{
Aig_Man_t* Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
void Aig_ManShow( Aig_Man_t * pMan, int fHaig, Vec_Ptr_t * vBold );
}

void findcuts(Abc_Obj_t* pObj, std::map<int, std::set<std::set<int>>> &cutMap, int k, bool isPi, bool isPo);
int dominate(std::set<int> a, std::set<int> b);
static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintCuts(Abc_Frame_t* pAbc, int argc, char** argv);
static int LSV_CommandSdc(Abc_Frame_t* pAbc, int argc, char** argv);
static int LSV_CommandOdc(Abc_Frame_t* pAbc, int argc, char** argv);


void Lsv_NtkSdc(Abc_Ntk_t* pNtk, int k);
void InvertOutput(Abc_Ntk_t* pNtk, int Idx);
void Lsv_NtkOdc(Abc_Ntk_t* pNtk, int k);
void InvertFanout(Abc_Ntk_t* pNtk, Abc_Obj_t* pNode);
int * SimulatePattern( Abc_Ntk_t * pNtk, int * pModel );

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_CommandPrintCuts, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sdc", LSV_CommandSdc, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_odc", LSV_CommandOdc, 0);
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


void Lsv_NtkPrintCuts(Abc_Ntk_t* pNtk, int k) {
  // printf("k = %i\n", k);
  Abc_Obj_t* pObj;
  int i;
  std::map<int, std::set<std::set<int>>> cutMap;
  Abc_NtkForEachPi(pNtk, pObj, i) {
    // printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
    findcuts(pObj, cutMap, k, true, false);
  }
  Abc_NtkForEachNode(pNtk, pObj, i) {
    // printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
    findcuts(pObj, cutMap, k, false, false);
    
  }
  // Abc_NtkForEachPo(pNtk, pObj, i) {
  //   // printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
  //   findcuts(pObj, cutMap, k, false, true);
  // }
}

int Lsv_CommandPrintCuts(Abc_Frame_t* pAbc, int argc, char** argv) {
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
  if (argc!=2) {
    goto usage;
  }
  Lsv_NtkPrintCuts(pNtk, std::atoi(argv[1]));
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_print_nodes [-h]\n");
  Abc_Print(-2, "\t        prints the nodes in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

int LSV_CommandSdc(Abc_Frame_t *pAbc, int argc, char **argv)
{
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
  if (argc!=2) {
    goto usage;
  }
  Lsv_NtkSdc(pNtk, std::atoi(argv[1]));
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sdc [-h]\n");
  Abc_Print(-2, "\t        prints the nodes in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

int LSV_CommandOdc(Abc_Frame_t *pAbc, int argc, char **argv)
{
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
  if (argc!=2) {
    goto usage;
  }
  Lsv_NtkOdc(pNtk, std::atoi(argv[1]));
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_odc [-h]\n");
  Abc_Print(-2, "\t        prints the nodes in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

void findcuts(Abc_Obj_t* pObj, std::map<int, std::set<std::set<int>>> &cutMap, int k, bool isPi, bool isPo){
    std::set<std::set<int>> cuts;    

    if(isPo){
      int Fanin0 = Abc_ObjId(Abc_ObjFanin0(pObj));
      cuts = cutMap[Fanin0];
    }

    //itself
    std::set<int> self;
    self.insert(Abc_ObjId(pObj));
    cuts.insert(self);


    //cross product
    if(!isPi&&!isPo){
      int Fanin0 = Abc_ObjId(Abc_ObjFanin0(pObj));
      int Fanin1 = Abc_ObjId(Abc_ObjFanin1(pObj));
      if(cutMap.find(Fanin0)==cutMap.end()){
        printf("cut of %d not found \n", Fanin0); 
      }
      if(cutMap.find(Fanin1)==cutMap.end()){
        printf("cut of %d not found \n", Fanin1);
      }
      for (const auto &s : cutMap[Fanin0]) {      
        for (const auto &t : cutMap[Fanin1]){
          std::set<int> cut = s;
          for (const auto &r : t){
            cut.insert(r);
          } 
          if(cut.size()>k) continue;


          // check dominate;
          bool dom = false;
          for (const auto &r : cuts)
          {
            switch (dominate(r, cut))
            {
            case 0:
              continue;
              break;
            case 1:
              dom = true;
              break;
            case 2:
              cuts.erase(r);
              break;
            case 3:
              dom = true;
              break;
            default:
              break;
            }
          }
          if(!dom) cuts.insert(cut);
        }      
      }
    }
    //print cuts

    for (const auto &s : cuts) {
      if(s.size()>k) continue;
      printf("%d: ", Abc_ObjId(pObj));
      for (const auto &t : s)
        printf("%d ", t);
      printf("\n");
    }
    cutMap[Abc_ObjId(pObj)] = cuts;
}

int dominate(std::set<int> a, std::set<int> b){
  if (a == b)
  {
    return 1;
  }
  else if(std::includes(a.begin(), a.end(), b.begin(), b.end()))
  {
    return 2;
  }
  else if (std::includes(b.begin(), b.end(), a.begin(), a.end()))
  {
    return 3;
  }
  
  return 0;
  
}


void Lsv_NtkSdc(Abc_Ntk_t* pNtk, int k) {
  srand(13943069);
  int patterns[Abc_NtkCiNum(pNtk)];
  for (size_t i = 0; i < Abc_NtkCiNum(pNtk); i++) 
  {
    patterns[i] = rand();
  }
  
  // printf("SDC\n");
  Abc_Obj_t* pObj = Abc_NtkObj(pNtk, k);
  Abc_Obj_t* fanin0 = Abc_ObjFanin0(pObj);
  int compl0 = Abc_ObjFaninC0(pObj); // inversion of fanin0 of node
  Abc_Obj_t* fanin1 = Abc_ObjFanin1(pObj);
  int compl1 = Abc_ObjFaninC1(pObj); // inversion of fanin1 of node
  // printf("%i, %i, %i\n", Abc_ObjId(pObj), Abc_ObjId(fanin0), Abc_ObjId(fanin1));
  Vec_Ptr_t* vOnePtr = Vec_PtrAlloc( 2 ); // 2 output
  Vec_PtrPush(vOnePtr, fanin0);
  Vec_PtrPush(vOnePtr, fanin1);
  Abc_Ntk_t* Ntk = Abc_NtkCreateConeArray( pNtk, vOnePtr, 1 );
  if (compl0) InvertOutput(Ntk, 0);
  if (compl1) InvertOutput(Ntk, 1);
  unsigned *value = (unsigned int *) SimulatePattern( Ntk, patterns );
  bool s0=0, s1=0, s2=0, s3=0;
  // printf("v0, %u\n", value[0]);
  // printf("v1, %u\n", value[1]);
  for (size_t i = 0; i < 32; i++)
  {
    int value0 = value[0] % 2;
    int value1 = value[1] % 2;
    value[0] /= 2;
    value[1] /= 2;
    
    if ((value0 == 1)&&(value1 == 1))
    {
      s0 = 1;
      // printf("11\n");
    } else if ((value0 == 1)&&(value1 == 0))
    {
      s1 = 1;
      // printf("10\n");
    } else if ((value0 == 0)&&(value1 == 1))
    {
      s2 = 1;
      // printf("01\n");
    } else if ((value0 == 0)&&(value1 == 0))
    {
      s3 = 1;
      // printf("00\n");
    } else{
      printf("error\n");
    }
    
  }

  Aig_Man_t * pAig = Abc_NtkToDar( Ntk, 0, 0); // abc_network xors latches
  Aig_Obj_t * y0 = Aig_ManCo( pAig, 0 );
  Aig_Obj_t * y1 = Aig_ManCo( pAig, 1 );
  // Aig_ManShow( pAig, 0, NULL );

  sat_solver* pSat = sat_solver_new();
  Cnf_Dat_t* cnf = Cnf_Derive(pAig, 2);
  int v0 = cnf->pVarNums[Aig_ObjId(y0)];
  int v1 = cnf->pVarNums[Aig_ObjId(y1)];
  // Cnf_DataPrint( cnf, 1 );
  // sat_solver_flip_print_clause(pSat);
  // printf("aig:%i, cnf:%i, aig:%i, cnf:%i\n", Aig_ObjId(y0), v0, Aig_ObjId(y1), v1);
  lit Lits0[1], Lits1[1];
  int status0, status1, status2, status3;
  
  if (!s3)
  {
    sat_solver_restart(pSat);
    Cnf_DataWriteIntoSolverInt( pSat, cnf, 1, 0);
    Lits0[0] = Abc_Var2Lit(v0, 1);
    Lits1[0] = Abc_Var2Lit(v1, 1);
    sat_solver_addclause(pSat, Lits0, Lits0 + 1);
    sat_solver_addclause(pSat, Lits1, Lits1 + 1);
    status3 = sat_solver_solve( pSat, NULL, NULL, 0, 0, 0, 0 );
    // printf("00, SAT:%i\n", status3);
    if (status3 < 0)
    {
      printf("00 ");
    }
  }else{
    status3 = 1;
  }
  
  if (!s2)
  {
    sat_solver_restart(pSat);
    Cnf_DataWriteIntoSolverInt( pSat, cnf, 1, 0);
    Lits0[0] = Abc_Var2Lit(v0, 1);
    Lits1[0] = Abc_Var2Lit(v1, 0);
    sat_solver_addclause(pSat, Lits0, Lits0 + 1);
    sat_solver_addclause(pSat, Lits1, Lits1 + 1);
    status2 = sat_solver_solve( pSat, NULL, NULL, 0, 0, 0, 0 );
    // printf("01, SAT:%i\n", status2);
    if (status2 < 0)
    {
      printf("01 ");
    }
  }else{
    status2 = 1;
  }
  
  if (!s1)
  {
    sat_solver_restart(pSat);
    Cnf_DataWriteIntoSolverInt( pSat, cnf, 1, 0);
    Lits0[0] = Abc_Var2Lit(v0, 0);
    Lits1[0] = Abc_Var2Lit(v1, 1);
    sat_solver_addclause(pSat, Lits0, Lits0 + 1);
    sat_solver_addclause(pSat, Lits1, Lits1 + 1);
    status1 = sat_solver_solve( pSat, NULL, NULL, 0, 0, 0, 0 );
    // printf("10, SAT:%i\n", status1);
    if (status1 < 0)
    {
      printf("10 ");
    }
  }else{
    status1 = 1;
  }
  
  if (!s0)
  {
    Cnf_DataWriteIntoSolverInt( pSat, cnf, 1, 0);
    Lits0[0] = Abc_Var2Lit(v0, 0);
    Lits1[0] = Abc_Var2Lit(v1, 0);
    sat_solver_addclause(pSat, Lits0, Lits0 + 1);
    sat_solver_addclause(pSat, Lits1, Lits1 + 1);
    int status0 = sat_solver_solve( pSat, NULL, NULL, 0, 0, 0, 0 );
    if (status0 < 0)
    {
      printf("11 ");
    }
  }else{
    status0 = 1;
  }
  
  if ((status0 == 1)&&(status1 == 1)&&(status2 == 1)&&(status3 == 1))
  {
    printf("no sdc");
  }
  printf("\n");
  sat_solver_delete(pSat);
  
}

void Lsv_NtkOdc(Abc_Ntk_t* pNtk, int k) {
  // printf("ODC\n");
  Abc_Ntk_t * Ntk = Abc_NtkDup(pNtk);  
  InvertFanout(Ntk, Abc_NtkObj(Ntk, k));
  Abc_Ntk_t * miter = Abc_NtkMiter(pNtk, Ntk, 1, 0, 0, 0);
  Abc_Obj_t* pObj = Abc_NtkObj(pNtk, k);
  Abc_Obj_t* fanin0 = Abc_ObjFanin0(pObj);
  int compl0 = Abc_ObjFaninC0(pObj); 
  Abc_Obj_t* fanin1 = Abc_ObjFanin1(pObj);
  int compl1 = Abc_ObjFaninC1(pObj);
  Vec_Ptr_t* vOnePtr = Vec_PtrAlloc( 2 ); 
  Vec_PtrPush(vOnePtr, fanin0);
  Vec_PtrPush(vOnePtr, fanin1);
  Abc_Ntk_t* Ntk1 = Abc_NtkCreateConeArray( pNtk, vOnePtr, 1 );
  if (compl0) InvertOutput(Ntk1, 0);
  if (compl1) InvertOutput(Ntk1, 1);
  Aig_Man_t * Ntk1_aig = Abc_NtkToDar( Ntk1, 0, 0); 
  Abc_NtkAppend( miter, Ntk1, 1 );
  Aig_Man_t * miter_aig = Abc_NtkToDar( miter, 0, 0); 

  srand(13943069);
  int patterns[Abc_NtkCiNum(pNtk)];
  for (size_t i = 0; i < Abc_NtkCiNum(pNtk); i++) 
  {
    patterns[i] = rand();
  }
  unsigned *value = (unsigned int *) SimulatePattern( Ntk1, patterns );
  bool s0=0, s1=0, s2=0, s3=0;
  // printf("v0, %u\n", value[0]);
  // printf("v1, %u\n", value[1]);
  for (size_t i = 0; i < 32; i++)
  {
    int value0 = value[0] % 2;
    int value1 = value[1] % 2;
    value[0] /= 2;
    value[1] /= 2;
    
    if ((value0 == 1)&&(value1 == 1))
    {
      s0 = 1;
      // printf("11\n");
    } else if ((value0 == 1)&&(value1 == 0))
    {
      s1 = 1;
      // printf("10\n");
    } else if ((value0 == 0)&&(value1 == 1))
    {
      s2 = 1;
      // printf("01\n");
    } else if ((value0 == 0)&&(value1 == 0))
    {
      s3 = 1;
      // printf("00\n");
    } else{
      printf("error\n");
    }
    
  }

  sat_solver* pSat = sat_solver_new();
  Cnf_Dat_t* cnf_sdc = Cnf_Derive(Ntk1_aig, 2);  
  Aig_Obj_t * y0 = Aig_ManCo( Ntk1_aig, 0 );
  Aig_Obj_t * y1 = Aig_ManCo( Ntk1_aig, 1 );
  int v0 = cnf_sdc->pVarNums[Aig_ObjId(y0)];
  int v1 = cnf_sdc->pVarNums[Aig_ObjId(y1)];  
  lit Lits0[1], Lits1[1];
  int status0, status1, status2, status3;

  if (!s0)
  {
    Cnf_DataWriteIntoSolverInt( pSat, cnf_sdc, 1, 0);
    Lits0[0] = Abc_Var2Lit(v0, 0);
    Lits1[0] = Abc_Var2Lit(v1, 0);
    sat_solver_addclause(pSat, Lits0, Lits0 + 1);
    sat_solver_addclause(pSat, Lits1, Lits1 + 1);
    status0 = sat_solver_solve( pSat, NULL, NULL, 0, 0, 0, 0 );
  }else{
    status0 = 1;
  }
  
  if (!s1)
  {
    sat_solver_restart(pSat);
    Cnf_DataWriteIntoSolverInt( pSat, cnf_sdc, 1, 0);
    Lits0[0] = Abc_Var2Lit(v0, 0);
    Lits1[0] = Abc_Var2Lit(v1, 1);
    sat_solver_addclause(pSat, Lits0, Lits0 + 1);
    sat_solver_addclause(pSat, Lits1, Lits1 + 1);
    sat_solver_solve( pSat, NULL, NULL, 0, 0, 0, 0 );
    status1 = sat_solver_solve( pSat, NULL, NULL, 0, 0, 0, 0 );
  }else{
    status1 = 1;
  }
  
  if (!s2)
  {
    sat_solver_restart(pSat);
    Cnf_DataWriteIntoSolverInt( pSat, cnf_sdc, 1, 0);
    Lits0[0] = Abc_Var2Lit(v0, 1);
    Lits1[0] = Abc_Var2Lit(v1, 0);
    sat_solver_addclause(pSat, Lits0, Lits0 + 1);
    sat_solver_addclause(pSat, Lits1, Lits1 + 1);
    status2 = sat_solver_solve( pSat, NULL, NULL, 0, 0, 0, 0 );
  }else{
    status2 = 1;
  }
  
  if (!s3)
  {
    sat_solver_restart(pSat);
    Cnf_DataWriteIntoSolverInt( pSat, cnf_sdc, 1, 0);
    Lits0[0] = Abc_Var2Lit(v0, 1);
    Lits1[0] = Abc_Var2Lit(v1, 1);
    sat_solver_addclause(pSat, Lits0, Lits0 + 1);
    sat_solver_addclause(pSat, Lits1, Lits1 + 1);
    status3 = sat_solver_solve( pSat, NULL, NULL, 0, 0, 0, 0 );
  }else{
    status3 = 1;
  }

  // odc
  Cnf_Dat_t* cnf = Cnf_Derive(miter_aig, 3);
  int status4, status5, status6, status7;
  y0 = Aig_ManCo( miter_aig, 1 );
  y1 = Aig_ManCo( miter_aig, 2 ); 
  v0 = cnf->pVarNums[Aig_ObjId(y0)];
  v1 = cnf->pVarNums[Aig_ObjId(y1)];
  Aig_Obj_t * miter_out = Aig_ManCo( miter_aig, 0 );
  int m_out = cnf->pVarNums[Aig_ObjId(miter_out)];
  lit Lits[1];
  Lits[0] = Abc_Var2Lit(m_out, 0);

  if (status3 == 1)
  {
    sat_solver_restart(pSat);
    Cnf_DataWriteIntoSolverInt( pSat, cnf, 1, 0);
    Lits0[0] = Abc_Var2Lit(v0, 1);
    Lits1[0] = Abc_Var2Lit(v1, 1);
    sat_solver_addclause(pSat, Lits, Lits + 1);
    sat_solver_addclause(pSat, Lits0, Lits0 + 1);
    sat_solver_addclause(pSat, Lits1, Lits1 + 1);
    status7 = sat_solver_solve( pSat, NULL, NULL, 0, 0, 0, 0 );
    // printf("00, SAT:%i\n", status0);
    if (status7 < 0)
    {
      printf("00 ");
    }
  }
  else{
    status7 = 1;
  }

  if (status2 == 1)
  {
    sat_solver_restart(pSat);
    Cnf_DataWriteIntoSolverInt( pSat, cnf, 1, 0);
    Lits0[0] = Abc_Var2Lit(v0, 1);
    Lits1[0] = Abc_Var2Lit(v1, 0);
    sat_solver_addclause(pSat, Lits, Lits + 1);
    sat_solver_addclause(pSat, Lits0, Lits0 + 1);
    sat_solver_addclause(pSat, Lits1, Lits1 + 1);
    status6 = sat_solver_solve( pSat, NULL, NULL, 0, 0, 0, 0 );
    // printf("01, SAT:%i\n", status0);
    if (status6 < 0)
    {
      printf("01 ");
    }
  }
  else{
    status6 = 1;
  }

  if (status1 == 1)
  {
    sat_solver_restart(pSat);
    Cnf_DataWriteIntoSolverInt( pSat, cnf, 1, 0);
    Lits0[0] = Abc_Var2Lit(v0, 0);
    Lits1[0] = Abc_Var2Lit(v1, 1);
    sat_solver_addclause(pSat, Lits, Lits + 1);
    sat_solver_addclause(pSat, Lits0, Lits0 + 1);
    sat_solver_addclause(pSat, Lits1, Lits1 + 1);
    status5 = sat_solver_solve( pSat, NULL, NULL, 0, 0, 0, 0 );
    // printf("11, SAT:%i\n", status0);
    if (status5 < 0)
    {
      printf("10 ");
    }
  }
  else{
    status5 = 1;
  }
  
  if (status0 == 1)
  {
    sat_solver_restart(pSat);
    Cnf_DataWriteIntoSolverInt( pSat, cnf, 1, 0);     
    Lits0[0] = Abc_Var2Lit(v0, 0);
    Lits1[0] = Abc_Var2Lit(v1, 0);
    sat_solver_addclause(pSat, Lits, Lits + 1);
    sat_solver_addclause(pSat, Lits0, Lits0 + 1);
    sat_solver_addclause(pSat, Lits1, Lits1 + 1);
    status4 = sat_solver_solve( pSat, NULL, NULL, 0, 0, 0, 0 );
    // printf("11, SAT:%i\n", status0);
    if (status4 < 0)
    {
      printf("11 ");
    }
  }
  else{
    status4 = 1;
  }

  if ((status4 == 1)&&(status5 == 1)&&(status6 == 1)&&(status7 == 1))
  {
    printf("no odc");
  }
  printf("\n");
  sat_solver_delete(pSat);

}





void InvertOutput(Abc_Ntk_t* pNtk, int Idx) {
    assert(Abc_NtkIsStrash(pNtk)); 
    Abc_Obj_t* pPo = Abc_NtkPo(pNtk, Idx); 
    Abc_ObjXorFaninC(pPo, 0);
    if (!Abc_NtkCheck(pNtk))
        printf("Error: Modified network failed integrity check.\n");
}


void InvertFanout(Abc_Ntk_t* pNtk, Abc_Obj_t* pNode) {
    assert(Abc_NtkIsStrash(pNtk));
    Abc_Obj_t* pFanout;
    int i;
    Abc_ObjForEachFanout(pNode, pFanout, i) {
        if (Abc_ObjFanin0(pFanout) == pNode)
          Abc_ObjXorFaninC(pFanout, 0);
        else if (Abc_ObjFanin1(pFanout) == pNode)
          Abc_ObjXorFaninC(pFanout, 1);
        else
          printf("error\n");
    }
    // if (!Abc_NtkCheck(pNtk)) 
    //     printf("Error: Modified network failed integrity check.\n");

}



int * SimulatePattern( Abc_Ntk_t * pNtk, int * pModel )
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
        // Value0 = ((int)(ABC_PTRINT_T)Abc_ObjFanin0(pNode)->pCopy) ^ (int)Abc_ObjFaninC0(pNode);
        // Value1 = ((int)(ABC_PTRINT_T)Abc_ObjFanin1(pNode)->pCopy) ^ (int)Abc_ObjFaninC1(pNode);
        // pNode->pCopy = (Abc_Obj_t *)(ABC_PTRINT_T)(Value0 & Value1);
        Value0 = Abc_ObjFaninC0(pNode) ? ~((int)(ABC_PTRINT_T)Abc_ObjFanin0(pNode)->pCopy) : ((int)(ABC_PTRINT_T)Abc_ObjFanin0(pNode)->pCopy);
        Value1 = Abc_ObjFaninC1(pNode) ? ~((int)(ABC_PTRINT_T)Abc_ObjFanin1(pNode)->pCopy) : ((int)(ABC_PTRINT_T)Abc_ObjFanin1(pNode)->pCopy);
        pNode->pCopy = (Abc_Obj_t *)(ABC_PTRINT_T)(Value0 & Value1);

    }
    // fill the output values
    pValues = ABC_ALLOC( int, Abc_NtkCoNum(pNtk) );
    Abc_NtkForEachCo( pNtk, pNode, i )
        // pValues[i] = ((int)(ABC_PTRINT_T)Abc_ObjFanin0(pNode)->pCopy) ^ (int)Abc_ObjFaninC0(pNode);
        pValues[i] = Abc_ObjFaninC0(pNode) ? ~((int)(ABC_PTRINT_T)Abc_ObjFanin0(pNode)->pCopy) : ((int)(ABC_PTRINT_T)Abc_ObjFanin0(pNode)->pCopy);
    if ( fStrashed )
        Abc_NtkDelete( pNtk );
    return pValues;
}


