#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "opt/cut/cut.h"
#include "opt/cut/cutInt.h"
#include "opt/cut/cutList.h"
#include "aig/aig/aig.h"
#include "iostream"
#include "set"
#include "queue"
#include "algorithm"
#include "sat/cnf/cnf.h"
#include "stdint.h"
extern "C"
{
  Aig_Man_t *Abc_NtkToDar(Abc_Ntk_t *pNtk, int fExors, int fRegisters);
}
using namespace std;

static int Lsv_CommandPrintNodes(Abc_Frame_t *pAbc, int argc, char **argv); // command function
static int Lsv_CommandPrintCut(Abc_Frame_t *pAbc, int argc, char **argv);
static int Lsv_CommandPrintSDC(Abc_Frame_t *pAbc, int argc, char **argv);
static int Lsv_CommandPrintODC(Abc_Frame_t *pAbc, int argc, char **argv);

void init(Abc_Frame_t *pAbc)
{
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_CommandPrintCut, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sdc", Lsv_CommandPrintSDC, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_odc", Lsv_CommandPrintODC, 0);
} // register this new command

void destroy(Abc_Frame_t *pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager
{
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

// function implementations

void Lsv_NtkPrintNodes(Abc_Ntk_t *pNtk)
{
  Abc_Obj_t *pObj; // nodes object
  int i;
  Abc_NtkForEachNode(pNtk, pObj, i)
  {
    printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
    Abc_Obj_t *pFanin;
    Abc_Obj_t *pFanout;
    int j;
    Abc_ObjForEachFanin(pObj, pFanin, j)
    {
      printf("  Fanin-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanin),
             Abc_ObjName(pFanin));
    }
    Abc_ObjForEachFanout(pObj, pFanout, j)
    {
      printf("  Fanout-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanout),
             Abc_ObjName(pFanout));
    }
    if (Abc_NtkHasSop(pNtk))
    {
      printf("The SOP of this node:\n%s", (char *)pObj->pData);
    }
  }
}

int Lsv_CommandPrintNodes(Abc_Frame_t *pAbc, int argc, char **argv)
{
  Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc); // top level network, storing nodes and connections
  int c;
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) // pase command line option (argv is an array of strings)
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
inline void helper(unordered_map<int, vector<set<int>>> &cuts, Abc_Obj_t *pObj, int k)
{
  int pObj_id = Abc_ObjId(pObj);
  Abc_Obj_t *pFanin;
  int first_fanin = Abc_ObjId(Abc_ObjFanin0(pObj));
  queue<set<int>> cut_queue;
  for (int k = 0; k < cuts[first_fanin].size(); k++) // cuts[id] -> [{}, {}, {}]
    cut_queue.push(cuts[first_fanin][k]);
  int j;
  Abc_ObjForEachFanin(pObj, pFanin, j) // iterate through all fanin nodes
  {
    int id = Abc_ObjId(pFanin);
    if (id != first_fanin)
    {
      int s = cut_queue.size();
      int curr_level = 1;
      while (curr_level <= s)
      {
        for (int l = 0; l < cuts[id].size(); l++) // iterate through each set in the cut[id] vector
        {
          set<int> temp = {};
          set_union(cuts[id][l].begin(), cuts[id][l].end(), cut_queue.front().begin(), cut_queue.front().end(),
                    inserter(temp, temp.begin()));
          cut_queue.push(temp);
        }
        cut_queue.pop();
        curr_level++;
      }
    }
  }
  cut_queue.push({pObj_id});
  // now the resulting final vector of sets after cartesian product is stored in cut_queue (aka feasible cuts of node pObj)
  // we now update the cuts map
  set<set<int>> cut_existence_set;
  while (!cut_queue.empty())
  {
    if (cut_existence_set.find(cut_queue.front()) == cut_existence_set.end())
    { // not an existing cut
      if (cut_queue.front().size() <= k)
      {
        cuts[pObj_id].push_back(cut_queue.front());
        // cout << cuts.size() << endl;
        cut_existence_set.insert(cut_queue.front());
      }
    }
    cut_queue.pop();
  }
}
void Lsv_NtkPrintCuts(Abc_Ntk_t *pNtk, int k)
{
  unordered_map<int, vector<set<int>>> cuts;
  // walk through all non-pi nodes
  queue<set<int>> cut_queue; // initiazlize cut queue for cartesian products of sets
  // initialize cuts map
  int i;
  Abc_Obj_t *pCi;
  Abc_NtkForEachCi(pNtk, pCi, i)
  {
    int id = Abc_ObjId(pCi);
    cuts[id] = {{id}};
  }

  Abc_Obj_t *pObj;
  Abc_NtkForEachNode(pNtk, pObj, i)
  {
    helper(cuts, pObj, k);
  }
  Abc_Obj_t *pPo;
  Abc_NtkForEachPo(pNtk, pPo, i)
  {
    helper(cuts, pPo, k);
  }
  int count = 0;
  Abc_NtkForEachPi(pNtk, pObj, i)
  {
    int id = Abc_ObjId(pObj);
    for (int i = 0; i < cuts[id].size(); i++)
    { // for each cut
      cout << id << ": ";
      for (const int &node_member : cuts[id][i])
      { // print cut members
        cout << node_member << " ";
      }
      cout << endl;
      count++;
    }
  }
  Abc_NtkForEachPo(pNtk, pObj, i)
  {
    int id = Abc_ObjId(pObj);
    for (int i = 0; i < cuts[id].size(); i++)
    { // for each cut
      cout << id << ": ";
      for (const int &node_member : cuts[id][i])
      { // print cut members
        cout << node_member << " ";
      }
      cout << endl;
      count++;
    }
  }

  Abc_NtkForEachNode(pNtk, pObj, i)
  {
    int id = Abc_ObjId(pObj);
    for (int i = 0; i < cuts[id].size(); i++)
    {
      cout << id << ": ";
      for (const int &node_member : cuts[id][i])
      { // print cut members
        cout << node_member << " ";
      }
      cout << endl;
      count++;
    }
  }

  // cout << "total number of cuts under constraint k = " << k << ": " << count << endl;
}
int Lsv_CommandPrintCut(Abc_Frame_t *pAbc, int argc, char **argv)
{
  Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc); // now pNtk is an aig
  int k = atoi(argv[1]);
  //
  int c;
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) // pase command line option (argv is an array of strings)
  {
    switch (c)
    {
    case 'h':
      goto usage;
    default:
      goto usage;
    }
  }
  if (!pNtk) // and check if it's an aig
  {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  if (!Abc_NtkIsStrash(pNtk))
  {
    Abc_Print(-1, "Cut computation is available only for AIGs (run \"strash\").\n");
    return 1;
  }
  if (k > 6 || k < 3)
  {
    Abc_Print(-1, "Value k should be between 3 and 6 (inclusive).\n");
    return 1;
  }
  Lsv_NtkPrintCuts(pNtk, k);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_print_cuts [-h]\n");
  Abc_Print(-2, "\t        prints the cuts in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}
/*
Abc_NtkForEachCi
    Abc_NtkForEachCO
        abc_objfanin0 // obj pointer
    Abc_ObjFaninId0   // obj id
*/

void PrintCNF(Cnf_Dat_t *pCnf)
{
  for (int num = 0; num < pCnf->nClauses; num++)
  {
    for (int *begin = pCnf->pClauses[num]; begin != pCnf->pClauses[num + 1]; begin++)
      cout << *begin << " ";
    cout << endl;
  }
}
vector<pair<int, int>> Lsv_Ntk_PrintSDC(Abc_Ntk_t *pNtk, int n)
{
  // create cones for fanin nodes of n
  Abc_Obj_t *pTarget;
  Abc_Obj_t *pObj;
  int i;
  Abc_NtkForEachNode(pNtk, pObj, i)
  {
    if (Abc_ObjId(pObj) == n)
    {
      pTarget = pObj;
      break;
    }
  }
  char *pNodeName0 = "CO0";
  char *pNodeName1 = "CO1";

  Abc_Obj_t *pFanin0 = Abc_ObjFanin0(pTarget);
  Abc_Obj_t *pFanin1 = Abc_ObjFanin1(pTarget);

  Abc_Ntk_t *pNtkCone0 = Abc_NtkCreateCone(pNtk, pFanin0, pNodeName0, 1);
  Abc_Ntk_t *pNtkCone1 = Abc_NtkCreateCone(pNtk, pFanin1, pNodeName1, 1);

  // cout << "Original cone: " << endl;
  //  Lsv_NtkPrintNodes(pNtk);

  // cout << "Fainin nodes: " << Abc_ObjId(pFanin0) << " " << Abc_ObjId(pFanin1) << endl;

  ////////////////////////////////////////////////////////////////////////////////////////////////
  //////////////////////////////// Random Simulations ////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////////////////////////
  // Random Simulations
  int numPi = Abc_NtkPiNum(pNtk);
  int numPatterns = 1; // Number of random patterns to simulate
  word *pModel = ABC_ALLOC(word, numPi);
  vector<bool> observedPatterns(4, false); // Four patterns: 00, 01, 10, 11

  for (int k = 0; k < numPatterns; k++)
  {
    Abc_Obj_t *pObj;
    int i;
    Abc_NtkForEachPi(pNtk, pObj, i)
    {
      pModel[i] = Abc_RandomW(0); // Random 64-bit word feed to Pi
      // cout << "Random pattern for PI " << i << ": " << pModel[i] << endl;
    }

    word *pValues0 = Abc_NtkVerifySimulatePatternParallel(pNtkCone0, pModel);
    word *pValues1 = Abc_NtkVerifySimulatePatternParallel(pNtkCone1, pModel);

    for (int bit = 0; bit < 64; bit++)
    {
      int y0 = (pValues0[0] >> bit) & 1; // Extract bit for y0
      int y1 = (pValues1[0] >> bit) & 1;
      // cout << "Bit " << bit << ": y0=" << y0 << " y1=" << y1 << endl;

      int pattern = 2 * y0 + y1;
      observedPatterns[pattern] = true;
    }
    ABC_FREE(pValues0);
    ABC_FREE(pValues1);
  }

  ABC_FREE(pModel);
  /*for (auto p : observedPatterns)
    cout << p;*/
  // cout << endl;

  ////////////////////////////////////////////////////////////////////////////////////////////////
  //////////////////////////////// check pattern with SAT ////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////////////////////////

  Vec_Ptr_t *vPtr = Vec_PtrAlloc(2);
  int j;
  // push in the two fanins
  Vec_PtrPushTwo(vPtr, Abc_ObjFanin0(pTarget), Abc_ObjFanin1(pTarget));
  Abc_Ntk_t *pNtkCone = Abc_NtkCreateConeArray(pNtk, vPtr, 1);
  // cout << "Cone array: " << endl;
  // Lsv_NtkPrintNodes(pNtkCone);
  Aig_Man_t *pAig = Abc_NtkToDar(pNtkCone, 0, 0);
  Cnf_Dat_t *pCnf = Cnf_Derive(pAig, 2);

  // Abc_Obj_t *pY0 = Abc_NtkFindNode(pNtkCone, Abc_ObjName(Abc_ObjFanin0(pTarget)));
  // Abc_Obj_t *pY1 = Abc_NtkFindNode(pNtkCone, Abc_ObjName(Abc_ObjFanin1(pTarget)));
  Abc_NtkDelete(pNtkCone);
  bool no_sdc = true;
  lit assumption[2];
  sat_solver *pSat = sat_solver_new();
  Cnf_DataWriteIntoSolverInt(pSat, pCnf, 1, 1);

  vector<pair<int, int>> sdc;
  for (int i = 0; i < 4; i++)
  {
    int bit0 = (i >> 1) & 1; // Extract bit0 (most significant bit)
    int bit1 = i & 1;        // Extract bit1 (least significant bit)
    assumption[0] = Abc_Var2Lit(pCnf->pVarNums[Aig_ManCo(pAig, 0)->Id], !bit0);
    assumption[1] = Abc_Var2Lit(pCnf->pVarNums[Aig_ManCo(pAig, 1)->Id], !bit1);
    // cout << "assumptions: " << assumption[0] << " " << assumption[1] << " " << endl;
    int status = sat_solver_solve(pSat, assumption, assumption + 2, 0, 0, 0, 0);
    bit0 = (Abc_ObjFaninC(pTarget, 0)) ? bit0 ^ 1 : bit0; // Extract bit for y0
    bit1 = (Abc_ObjFaninC(pTarget, 1)) ? bit1 ^ 1 : bit1;
    if (status == l_False)
    {
      // cout << bit0 << bit1 << endl;
      sdc.push_back({bit0, bit1});
      // no_sdc = false;
    }
  }
  sat_solver_delete(pSat);
  /*if (no_sdc)
    cout << "no sdc" << endl;*/
  ABC_FREE(vPtr);
  return sdc;
}

void AddDummyConstraints(Abc_Ntk_t *pNtk, Abc_Obj_t *pTarget)
{
  Abc_Obj_t *pFanin0 = Abc_ObjFanin0(pTarget);
  Abc_Obj_t *pFanin1 = Abc_ObjFanin1(pTarget);

  // Create a new dummy node that depends on the fanins
  Abc_Obj_t *pDummyNode = Abc_NtkCreateNode(pNtk);
  Abc_ObjAddFanin(pDummyNode, pFanin0);
  Abc_ObjAddFanin(pDummyNode, pFanin1);

  // Create a new dummy output
  Abc_Obj_t *pDummyOutput = Abc_NtkCreatePo(pNtk);
  Abc_ObjAddFanin(pDummyOutput, pDummyNode);
}
void Lsv_Ntk_PrintODC(Abc_Ntk_t *pNtk, int n)
{
  Abc_Obj_t *pTarget, *pTarget_o;
  Abc_Obj_t *pFanout, *pObj;
  Aig_Man_t *pAig, *pAigCopy, *pAigMiter;
  Cnf_Dat_t *pCnf;
  sat_solver *pSat;
  Vec_Ptr_t *vCareSet, *vDontCareSet;
  vector<pair<int, int>> sdc = Lsv_Ntk_PrintSDC(pNtk, n);
  Abc_Ntk_t *pNtkCopy = Abc_NtkDup(pNtk); // the modified network
  int i, j;
  pTarget = Abc_NtkObj(pNtkCopy, n);
  Abc_ObjForEachFanout(pTarget, pFanout, i)
  {
    Abc_ObjForEachFanin(pFanout, pObj, j)
    {
      if (pObj->Id == pTarget->Id)
        Abc_ObjXorFaninC(pFanout, j);
    }
  }
  Abc_Ntk_t *pMiter = Abc_NtkMiter(pNtk, pNtkCopy, 1, 0, 0, 0);
  // Lsv_NtkPrintNodes(pMiter); // structural hashing
  Vec_Ptr_t *vFaninPtr = Vec_PtrAlloc(2);
  // push in the two fanins of target node
  Vec_PtrPushTwo(vFaninPtr, Abc_ObjFanin0(pTarget), Abc_ObjFanin1(pTarget));
  Abc_Ntk_t *pNtkFaninCone = Abc_NtkCreateConeArray(pNtkCopy, vFaninPtr, 1);
  // append the fanin cone to Miter network -> structural hashing, there should be 3 POs now
  Abc_NtkAppend(pMiter, pNtkFaninCone, 1);
  // cout << "done append" << endl;
  // Lsv_NtkPrintNodes(pMiter);
  // cout << "PO number: " << pMiter->vPos->nSize << endl;
  pAigMiter = Abc_NtkToDar(pMiter, 0, 0);
  pCnf = Cnf_Derive(pAigMiter, 3); // POs won't be simplified
  // PrintCNF(pCnf);                  // looks good for now
  pSat = sat_solver_new();
  Cnf_DataWriteIntoSolverInt(pSat, pCnf, 1, 1);
  // cout << "Aig Co num: " << endl;
  // cout << Aig_ManCoNum(pAigMiter) << endl;
  lit miterOutputVar = pCnf->pVarNums[Aig_ManCo(pAigMiter, 0)->Id];
  // cout << "check miter output Id: " << endl;
  // cout << Abc_NtkPo(pMiter, 0)->Id << endl;
  lit assumption = toLitCond(miterOutputVar, 0);
  int status = sat_solver_solve(pSat, &assumption, &assumption + 1, 0, 0, 0, 0);
  // cout << status << endl;
  //  ALLSAT: find all satisfying assignments
  vector<bool> CareSet(4, false);
  vector<bool> DCSet(4, false);
  int var0 = pCnf->pVarNums[Aig_ManCo(pAigMiter, 1)->Id]; // observed
  int var1 = pCnf->pVarNums[Aig_ManCo(pAigMiter, 2)->Id];
  /*cout << "var0, var1 in network: " << endl
       << Abc_NtkPo(pMiter, 1)->Id << " " << Abc_NtkPo(pMiter, 2)->Id << endl;
  cout << "var0, var1 in aig: " << endl
       << Aig_ManCo(pAigMiter, 1)->Id << " " << Aig_ManCo(pAigMiter, 2)->Id << endl;*/
  // cout << "check fanins: " << endl;
  // cout << Abc_ObjFanin(Abc_NtkPo(pMiter, 1), 0)->Id << endl;
  // cout << Abc_ObjFanin(Abc_NtkPo(pMiter, 2), 0)->Id << endl;
  while (status == l_True)
  {
    int current_solution[2];
    int bit0, bit1;
    current_solution[0] = sat_solver_var_value(pSat, var0);
    current_solution[1] = sat_solver_var_value(pSat, var1);
    // cout << current_solution[0] << current_solution[1] << endl;
    bit0 = Abc_ObjFaninC(pTarget, 0) ? current_solution[0] ^ 1 : current_solution[0];
    bit1 = Abc_ObjFaninC(pTarget, 1) ? current_solution[1] ^ 1 : current_solution[1];
    CareSet[bit0 * 2 + bit1] = true;
    lit blocking_clause[2];
    blocking_clause[0] = toLitCond(pCnf->pVarNums[Aig_ObjFanin0(Aig_ManCo(pAigMiter, 1))->Id], current_solution[0]);
    blocking_clause[1] = toLitCond(pCnf->pVarNums[Aig_ObjFanin0(Aig_ManCo(pAigMiter, 2))->Id], current_solution[1]);

    // Add the blocking clause to the SAT solver
    if (!sat_solver_addclause(pSat, blocking_clause, blocking_clause + 2))
    {
      cout << "Failed to add blocking clause" << endl;
      break;
    }
    status = sat_solver_solve(pSat, &assumption, &assumption + 1, 0, 0, 0, 0);
  }
  for (int i = 0; i < 4; i++)
  {
    if (!CareSet[i])
      DCSet[i] = true;
  }
  // remove sdc
  for (const auto s : sdc)
  {
    int index = s.first * 2 + s.second;
    DCSet[index] = false;
  }
  bool no_odc = true;
  for (int i = 0; i < 4; i++)
  {
    int bit0, bit1;
    if (DCSet[i] == true)
    {
      bit0 = (i >> 1) & 1; // Extract bit0 (most significant bit)
      bit1 = i & 1;
      cout << bit0 << bit1 << " ";
      no_odc = false;
    }
  }
  if (no_odc)
    cout << "no odc" << endl;
  else
    cout << endl;
  ABC_FREE(vFaninPtr);
  sat_solver_delete(pSat);
  return;
}

int Lsv_CommandPrintODC(Abc_Frame_t *pAbc, int argc, char **argv)
{
  Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
  int n = atoi(argv[1]);
  int c;
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) // pase command line option (argv is an array of strings)
  {
    switch (c)
    {
    case 'h':
      goto usage;
    default:
      goto usage;
    }
  }
  if (!pNtk) // and check if it's an aig
  {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  if (!Abc_NtkIsStrash(pNtk))
  {
    Abc_Print(-1, "Computation is available only for AIGs (run \"strash\").\n");
    return 1;
  }
  Lsv_Ntk_PrintODC(pNtk, n);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_odc [-h]\n");
  Abc_Print(-2, "\t        compute the local observability don’t cares of n with respect to all primary outputs in terms of its fanin nodes \n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}
int Lsv_CommandPrintSDC(Abc_Frame_t *pAbc, int argc, char **argv)
{
  Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
  vector<pair<int, int>> sdc;
  int n = atoi(argv[1]);
  int c;
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) // pase command line option (argv is an array of strings)
  {
    switch (c)
    {
    case 'h':
      goto usage;
    default:
      goto usage;
    }
  }
  if (!pNtk) // and check if it's an aig
  {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  if (!Abc_NtkIsStrash(pNtk))
  {
    Abc_Print(-1, "Computation is available only for AIGs (run \"strash\").\n");
    return 1;
  }
  sdc = Lsv_Ntk_PrintSDC(pNtk, n);
  if (sdc.size() == 0)
  {
    cout << "no sdc" << endl;
    return 0;
  }

  for (const auto s : sdc)
    cout << s.first << s.second << " ";
  cout << endl;

  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sdc [-h]\n");
  Abc_Print(-2, "\t        prints the sdc of the given node n in terms of its fanins\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}
