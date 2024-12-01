#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <vector>
#include <set>
#include <algorithm>
#include "sat/cnf/cnf.h"
extern "C"
{
  Aig_Man_t *Abc_NtkToDar(Abc_Ntk_t *pNtk, int fExors, int fRegisters);
}

static int Lsv_CommandPrintNodes(Abc_Frame_t *pAbc, int argc, char **argv);
static int Lsv_CommandPrintCuts(Abc_Frame_t *pAbc, int argc, char **argv);
static int Lsv_CommandSDC(Abc_Frame_t *pAbc, int argc, char **argv);
static int Lsv_CommandODC(Abc_Frame_t *pAbc, int argc, char **argv);

void init(Abc_Frame_t *pAbc)
{
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_CommandPrintCuts, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sdc", Lsv_CommandSDC, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_odc", Lsv_CommandODC, 0);
}

void destroy(Abc_Frame_t *pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager
{
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

void Lsv_NtkPrintNodes(Abc_Ntk_t *pNtk)
{
  Abc_Obj_t *pObj;
  int i;
  Abc_NtkForEachNode(pNtk, pObj, i)
  {
    printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
    Abc_Obj_t *pFanin;
    int j;
    Abc_ObjForEachFanin(pObj, pFanin, j)
    {
      printf("  Fanin-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanin),
             Abc_ObjName(pFanin));
    }
    if (Abc_NtkHasSop(pNtk))
    {
      printf("The SOP of this node:\n%s", (char *)pObj->pData);
    }
  }
}

int Lsv_CommandPrintNodes(Abc_Frame_t *pAbc, int argc, char **argv)
{
  Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
  int c;
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF)
  {
    switch (c)
    {
    case 'h':
      goto usage;
    default:
      goto usage;
    }
  }
  if (!pNtk)
  {
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

void Lsv_NtkPrintCuts(Abc_Ntk_t *pNtk, int k)
{
  Abc_Obj_t *pObj;
  std::vector<std::vector<std::set<int>>> CutList(Abc_NtkObjNumMax(pNtk));
  int i;

  CutList.resize(Abc_NtkObjNumMax(pNtk));

  Abc_NtkForEachPi(pNtk, pObj, i)
  {
    std::set<int> trivial;
    int ObjId = Abc_ObjId(pObj);
    trivial.insert(ObjId);
    CutList[ObjId].push_back(trivial);
  }

  Abc_NtkForEachNode(pNtk, pObj, i)
  {

    std::set<int> trivial;
    int ObjId = Abc_ObjId(pObj);
    trivial.insert(ObjId);
    CutList[ObjId].push_back(trivial);
    int FaninId0 = Abc_ObjFaninId0(pObj), FaninId1 = Abc_ObjFaninId1(pObj);
    for (int m = 0; m < CutList[FaninId0].size(); m++)
    {
      for (int n = 0; n < CutList[FaninId1].size(); n++)
      {
        std::set<int> New_Cut;
        std::merge(CutList[FaninId0][m].begin(), CutList[FaninId0][m].end(),
                   CutList[FaninId1][n].begin(), CutList[FaninId1][n].end(), std::inserter(New_Cut, New_Cut.begin()));
        if (New_Cut.size() <= k)
          CutList[ObjId].push_back(New_Cut);
      }
    }
  }
  Abc_NtkForEachPo(pNtk, pObj, i)
  {

    std::set<int> trivial;
    int ObjId = Abc_ObjId(pObj);
    trivial.insert(ObjId);
    CutList[ObjId].push_back(trivial);
    int FaninId0 = Abc_ObjFaninId0(pObj);
    for (int m = 0; m < CutList[FaninId0].size(); m++)
    {
      std::set<int> New_Cut;
      New_Cut = CutList[FaninId0][m];
      if (New_Cut.size() <= k)
        CutList[ObjId].push_back(New_Cut);
    }
  }
  for (int m = 0; m < Abc_NtkObjNumMax(pNtk); m++)
  {
    for (int n = 0; n < CutList[m].size(); n++)
    {
      for (int p = 0; p < CutList[m].size(); p++)
      {
        if (n != p)
        {
          if (std::includes(CutList[m][n].begin(), CutList[m][n].end(), CutList[m][p].begin(), CutList[m][p].end()))
          {
            CutList[m].erase(CutList[m].begin() + n);
            n--;
            break;
          }
        }
      }
    }
  }

  for (int m = 0; m < Abc_NtkObjNumMax(pNtk); m++)
  {
    for (int n = 0; n < CutList[m].size(); n++)
    {
      printf("%d: ", m);
      for (auto it = CutList[m][n].begin(); it != CutList[m][n].end(); ++it)
      {
        printf("%d ", *it);
      }
      printf("\n");
    }
  }
}

int Lsv_CommandPrintCuts(Abc_Frame_t *pAbc, int argc, char **argv)
{
  Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
  int c;
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF)
  {
    switch (c)
    {
    case 'h':
      goto usage;
    default:
      goto usage;
    }
  }
  if (argc != globalUtilOptind + 1)
  {
    Abc_Print(-1, "Wrong number of auguments.\n");
    goto usage;
  }
  if (!pNtk)
  {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  Lsv_NtkPrintCuts(pNtk, atoi(argv[1]));

  return 0;

usage:
  Abc_Print(-2, "usage: lsv_printcut [-h] <k>\n");
  Abc_Print(-2, "\t        prints the k-feasible cut enumeration of every node on an AIG\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

void Lsv_NtkSDC(Abc_Ntk_t *pNtk, int n)
{
  // Initialization
  Abc_Obj_t *pObj = Abc_NtkObj(pNtk, n);
  Abc_Obj_t *pFanin0 = Abc_ObjFanin0(pObj);
  Abc_Obj_t *pFanin1 = Abc_ObjFanin1(pObj);
  int edge_Fanin0 = Abc_ObjFaninC0(pObj), edge_Fanin1 = Abc_ObjFaninC1(pObj);

  // Create the network using cone
  Vec_Ptr_t *vCone = Vec_PtrAlloc(1);
  Vec_PtrPush(vCone, pObj);
  Vec_PtrPush(vCone, pFanin0);
  Vec_PtrPush(vCone, pFanin1);
  Abc_Ntk_t *pConeNtk = Abc_NtkCreateConeArray(pNtk, vCone, 1);

  // Create Cnf
  Aig_Man_t *pAig = Abc_NtkToDar(pConeNtk, 0, 0);
  Cnf_Dat_t *pCnf = Cnf_Derive(pAig, 3);

  // Create sat solver
  sat_solver *pSatSolver = sat_solver_new();
  Cnf_DataWriteIntoSolverInt(pSatSolver, pCnf, 1, 0);

  // Solver initialization
  int varFanin0 = pCnf->pVarNums[Aig_ManCo(pAig, 1)->Id];
  int varFanin1 = pCnf->pVarNums[Aig_ManCo(pAig, 2)->Id];
  bool nosdc = 1;

  // Start solve
  for (int v0 = 0; v0 <= 1; v0++)
  {
    for (int v1 = 0; v1 <= 1; v1++)
    {
      // Create assignments
      lit assumptions[2] = {
          Abc_Var2Lit(varFanin0, v0),
          Abc_Var2Lit(varFanin1, v1),
      };

      // Solve
      int satisfiable = sat_solver_solve(pSatSolver, assumptions, assumptions + 2, 0, 0, 0, 0);
      if (satisfiable == l_False)
      {
        printf("%d%d ", (v0 == edge_Fanin0), (v1 == edge_Fanin1));
        nosdc = 0;
      }
    }
  }
  if (nosdc)
    printf("no sdc");
  printf("\n");

  // Free memory
  sat_solver_delete(pSatSolver);
  Cnf_DataFree(pCnf);
  Vec_PtrFree(vCone);
}

int Lsv_CommandSDC(Abc_Frame_t *pAbc, int argc, char **argv)
{
  Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
  int c;
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF)
  {
    switch (c)
    {
    case 'h':
      goto usage;
    default:
      goto usage;
    }
  }
  if (argc != globalUtilOptind + 1)
  {
    Abc_Print(-1, "Wrong number of auguments.\n");
    goto usage;
  }
  if (!pNtk)
  {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  Lsv_NtkSDC(pNtk, atoi(argv[1]));

  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sdc [-h] <n>\n");
  Abc_Print(-2, "\t        list all the minterms of the satisfiability don't cares\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

std::set<std::pair<int, int>> Lsv_SetSDC(Abc_Ntk_t *pNtk, int n)
{
  Abc_Obj_t *pObj = Abc_NtkObj(pNtk, n);
  Abc_Obj_t *pFanin0 = Abc_ObjFanin0(pObj);
  Abc_Obj_t *pFanin1 = Abc_ObjFanin1(pObj);
  int edge_Fanin0 = Abc_ObjFaninC0(pObj), edge_Fanin1 = Abc_ObjFaninC1(pObj);

  Vec_Ptr_t *vCone = Vec_PtrAlloc(1);
  Vec_PtrPush(vCone, pObj);
  Vec_PtrPush(vCone, pFanin0);
  Vec_PtrPush(vCone, pFanin1);
  Abc_Ntk_t *pConeNtk = Abc_NtkCreateConeArray(pNtk, vCone, 1);
  Aig_Man_t *pAig = Abc_NtkToDar(pConeNtk, 0, 0);
  Cnf_Dat_t *pCnf = Cnf_Derive(pAig, 3);

  sat_solver *pSatSolver = sat_solver_new();
  Cnf_DataWriteIntoSolverInt(pSatSolver, pCnf, 1, 0);

  int varFanin0 = pCnf->pVarNums[Aig_ManCo(pAig, 1)->Id];
  int varFanin1 = pCnf->pVarNums[Aig_ManCo(pAig, 2)->Id];
  std::set<std::pair<int, int>> sdc_set;
  // bool nosdc = 1;
  for (int v0 = 0; v0 <= 1; v0++)
  {
    for (int v1 = 0; v1 <= 1; v1++)
    {
      lit assumptions[2] = {
          Abc_Var2Lit(varFanin0, v0),
          Abc_Var2Lit(varFanin1, v1),
      };

      int satisfiable = sat_solver_solve(pSatSolver, assumptions, assumptions + 2, 0, 0, 0, 0);
      if (satisfiable == l_False)
      {
        sdc_set.insert({(v0 == edge_Fanin0), (v1 == edge_Fanin1)});
        // printf("%d%d ", (v0 == edge_Fanin0), (v1 == edge_Fanin1));
        // nosdc = 0;
      }
    }
  }
  // if (nosdc)
  //   printf("no sdc");
  // printf("\n");

  sat_solver_delete(pSatSolver);
  Cnf_DataFree(pCnf);
  Vec_PtrFree(vCone);
  return sdc_set;
}

void Lsv_NtkODC(Abc_Ntk_t *pNtk, int n)
{
  // Initialization
  Abc_Ntk_t *pNtk1 = Abc_NtkDup(pNtk), *pNtk2 = Abc_NtkDup(pNtk);
  Abc_Obj_t *pObj1 = Abc_NtkObj(pNtk1, n), *pObj2 = Abc_NtkObj(pNtk2, n);
  int edge_Fanin0 = Abc_ObjFaninC0(pObj1), edge_Fanin1 = Abc_ObjFaninC1(pObj1);

  // Negate the fanouts of node n
  Abc_Obj_t *iFanout;
  int i;
  Abc_ObjForEachFanout(pObj2, iFanout, i)
  {
    if (Abc_ObjIsPo(iFanout)) // Fanout is PO: no odc
    {
      printf("no odc\n");
      return;
    }
    Abc_ObjXorFaninC(iFanout, Abc_ObjFanoutEdgeNum(pObj2, iFanout));
  }

  // Create the miter
  Abc_Ntk_t *pMiter = Abc_NtkMiter(pNtk1, pNtk2, 1, 0, 0, 0);
  Abc_NtkAppend(pNtk1, pMiter, 1);

  // Create the miter using cone
  Abc_Obj_t *pPo = Abc_NtkPo(pNtk1, 1);
  Abc_Obj_t *pFanin0 = Abc_NtkObj(pNtk1, Abc_ObjFaninId0(Abc_NtkObj(pNtk1, n)));
  Abc_Obj_t *pFanin1 = Abc_NtkObj(pNtk1, Abc_ObjFaninId1(Abc_NtkObj(pNtk1, n)));

  Vec_Ptr_t *vCone = Vec_PtrAlloc(1);
  Vec_PtrPush(vCone, pPo);
  Vec_PtrPush(vCone, pFanin0);
  Vec_PtrPush(vCone, pFanin1);

  Abc_Ntk_t *pConeMiter = Abc_NtkCreateConeArray(pNtk1, vCone, 1);
  // Create Cnf
  Aig_Man_t *pAig = Abc_NtkToDar(pConeMiter, 0, 0);
  Cnf_Dat_t *pCnf = Cnf_Derive(pAig, 3);

  // debug
  // Aig_ManDumpBlif(pAig, "debug.blif", NULL, NULL);

  // Create sat solver
  sat_solver *pSatSolver = sat_solver_new();
  Cnf_DataWriteIntoSolverInt(pSatSolver, pCnf, 1, 0);

  // Set PO = 1
  int varPo = pCnf->pVarNums[Aig_ManCo(pAig, 0)->Id];
  lit litPo = Abc_Var2Lit(varPo, 0);
  sat_solver_addclause(pSatSolver, &litPo, &litPo + 1);

  // Solver initialization

  int varFanin0 = pCnf->pVarNums[Aig_ManCo(pAig, 1)->Id];
  int varFanin1 = pCnf->pVarNums[Aig_ManCo(pAig, 2)->Id];
  int satisfiable = sat_solver_solve(pSatSolver, NULL, NULL, 0, 0, 0, 0);
  std::set<std::pair<int, int>> dc_set = {
      {0, 0},
      {0, 1},
      {1, 0},
      {1, 1}};

  // Start solve
  while (satisfiable == 1)
  {
    // Create clause
    lit Lits[2];
    Lits[0] = Abc_Var2Lit(varFanin0, sat_solver_var_value(pSatSolver, varFanin0));
    Lits[1] = Abc_Var2Lit(varFanin1, sat_solver_var_value(pSatSolver, varFanin1));

    // Remove care pattern
    auto care = dc_set.find({(sat_solver_var_value(pSatSolver, varFanin0) != edge_Fanin0), (sat_solver_var_value(pSatSolver, varFanin1) != edge_Fanin1)});
    if (care != dc_set.end())
      dc_set.erase(care);

    // Add clauses
    if (!sat_solver_addclause(pSatSolver, Lits, Lits + 2))
      break;

    // Solve
    satisfiable = sat_solver_solve(pSatSolver, NULL, NULL, 0, 0, 0, 0);
  }

  // Create odc set
  std::set<std::pair<int, int>> sdc_set = Lsv_SetSDC(pNtk, n);
  std::set<std::pair<int, int>> odc_set;

  std::set_difference(dc_set.begin(), dc_set.end(),
                      sdc_set.begin(), sdc_set.end(),
                      std::inserter(odc_set, odc_set.begin()));

  if (odc_set.empty())
    printf("no odc");
  else
  {
    for (const auto &odc : odc_set)
      printf("%d%d ", odc.first, odc.second);
  }
  printf("\n");

  // Free memory
  sat_solver_delete(pSatSolver);
  Cnf_DataFree(pCnf);
  Vec_PtrFree(vCone);
}

int Lsv_CommandODC(Abc_Frame_t *pAbc, int argc, char **argv)
{
  Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
  int c;
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF)
  {
    switch (c)
    {
    case 'h':
      goto usage;
    default:
      goto usage;
    }
  }
  if (argc != globalUtilOptind + 1)
  {
    Abc_Print(-1, "Wrong number of auguments.\n");
    goto usage;
  }
  if (!pNtk)
  {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  Lsv_NtkODC(pNtk, atoi(argv[1]));

  return 0;

usage:
  Abc_Print(-2, "usage: lsv_odc [-h] <n>\n");
  Abc_Print(-2, "\t        list all the minterms of the observability don't cares\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}