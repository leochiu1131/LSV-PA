#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/cnf/cnf.h"
#include "sat/bsat/satStore.h"

#include <vector>
#include <queue>
#include <list>
#include <unordered_set>
#include <algorithm>
#include <string>
#include <iostream>
#include <fstream>
#include <map>
#include <tuple>
#include <time.h>

//* Deprecate
#define GET_CONE TRUE

#define SHOW_SIMULATION

#ifdef SHOW_SIMULATION
#define PRINT_SIMULATION_MAP 0
#define PRINT_SIMULATION_PROCESS 0
#define PRINT_SIMULATION_RESULT 0
#endif

// #define DEBUG_EXEC_FAN1_SAT
#define PRINT_DONOT_CARE_SET 0
#define DUMP_ALL_SATISFY_ANS 0
#define SHOW_HEAD_ID 0

#define PRINT_NODE_ID_AND_LITERAL_ID 0
#define PRINT_ALL_CLAUSES 0
#define SAT_SOLVER_VERBOSE 0

#include "simulator.h"

extern "C"
{
  Aig_Man_t *Abc_NtkToDar(Abc_Ntk_t *pNtk, int fExors, int fRegisters);
  Abc_Ntk_t *Abc_NtkDouble(Abc_Ntk_t *pNtk);
}

extern "C"
{
  void Sto_ManDumpClauses(Sto_Man_t *p, char *pFileName);
  void Abc_NtkShow(Abc_Ntk_t *pNtk0, int fGateNames, int fSeq, int fUseReverse, int fKeepDot, int fAigIds);
}

static int Lsv_CommandNtkFindSdc(Abc_Frame_t *pAbc, int argc, char **argv);
static int Lsv_CommandNtkFindOdc(Abc_Frame_t *pAbc, int argc, char **argv);
static int Lsv_CommandNtkSim(Abc_Frame_t *pAbc, int argc, char **argv);

void init(Abc_Frame_t *pAbc)
{
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sdc", Lsv_CommandNtkFindSdc, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_odc", Lsv_CommandNtkFindOdc, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim", Lsv_CommandNtkSim, 0);
}

void destroy(Abc_Frame_t *pAbc) {}

Abc_FrameInitializer_t frame_initializer_sdc = {init, destroy};
Abc_FrameInitializer_t frame_initializer_odc = {init, destroy};
Abc_FrameInitializer_t frame_initializer_sim = {init, destroy};

struct PackageRegistrationManager_sdc
{
  PackageRegistrationManager_sdc() { Abc_FrameAddInitializer(&frame_initializer_sdc); }
} lsvPackageRegistrationManager_sdc;

struct PackageRegistrationManager_odc
{
  PackageRegistrationManager_odc() { Abc_FrameAddInitializer(&frame_initializer_odc); }
} lsvPackageRegistrationManager_odc;

struct PackageRegistrationManager_sim
{
  PackageRegistrationManager_sim() { Abc_FrameAddInitializer(&frame_initializer_sim); }
} lsvPackageRegistrationManager_sim;
// *********************************** Self define program start ***************************
// *****************************************************************************************

int uniformRandInt(int n)
{
  if ((n - 1) == RAND_MAX)
  {
    return rand();
  }
  else
  {

    long end = RAND_MAX / n;
    assert(end > 0L);
    end *= n;
    int r;
    while ((r = rand()) >= end)
      ;

    return r % n;
  }
}

// ******************************** Self define program end ********************************
// *****************************************************************************************

std::map<int, int> Lsv_NtkFindSdc(Abc_Ntk_t *pNtk, int nodeSelect)
{

  Abc_Obj_t *pAbcObj;
  Abc_Obj_t *pTmpObj;
  std::map<int, int> do_not_care_set = {{0, 0}, {1, 1}, {2, 2}, {3, 3}};
  int i;

  Abc_NtkForEachObj(pNtk, pTmpObj, i)
  {

    if (Abc_ObjId(pTmpObj) != nodeSelect)
      continue;
    else
    {
      pAbcObj = pTmpObj;
      break;
    }
  }
#if SHOW_HEAD_ID
  fprintf(stdout, "Node ID: %d\n", Abc_ObjId(pAbcObj));
#endif
#if GET_CONE

  Abc_Obj_t *pFanin0Obj = Abc_ObjFanin0(pAbcObj);
  Abc_Obj_t *pFanin1Obj = Abc_ObjFanin1(pAbcObj);
  Abc_Ntk_t *pFanin0_Ntk, *pFanin1_Ntk;
  // Abc_Ntk_t *pObj_Ntk;
  int fanin0_inv = (pAbcObj->fCompl0);
  int fanin1_inv = (pAbcObj->fCompl0);

  pFanin0_Ntk = Abc_NtkCreateCone(pNtk, pFanin0Obj, Abc_ObjName(pFanin0Obj), 1);
  pFanin1_Ntk = Abc_NtkCreateCone(pNtk, pFanin1Obj, Abc_ObjName(pFanin1Obj), 1);
  // pObj_Ntk = Abc_NtkCreateCone(pNtk, pObj, Abc_ObjName(pObj), 1);

  int fanin0_inv_po_support = 0;
  int fanin1_inv_po_support = 0;
  int fanin0_faninObjId = 0;
  int fanin1_faninObjId = 0;

  Abc_Obj_t *pPoObj;
  Abc_NtkForEachPo(pFanin0_Ntk, pPoObj, i)
  {
    fanin0_inv_po_support = pPoObj->fCompl0;
    fanin0_faninObjId = Vec_IntEntry(&pPoObj->vFanins, 0);
    // printf("First vFanin: %d\n", first_vFanin);
  }

  Abc_NtkForEachPo(pFanin1_Ntk, pPoObj, i)
  {
    fanin1_inv_po_support = pPoObj->fCompl0;
    fanin1_faninObjId = Vec_IntEntry(&pPoObj->vFanins, 0);
    // printf("First vFanin: %d\n", first_vFanin);
  }

#else
  sub_ntk = pNtk;
#endif

  //* Aig_Man_t * Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters )
  Aig_Man_t *pAig_man_fanin0 = Abc_NtkToDar(pFanin0_Ntk, 0, 0);
  Aig_Man_t *pAig_man_fanin1 = Abc_NtkToDar(pFanin1_Ntk, 0, 0);
  // Aig_Man_t *pAig_man_head = Abc_NtkToDar(pObj_Ntk, 0, 0);
  /*
   If you print clause ID in pCnf_data->pClauses
   Please note that the clause ID is not the same as the node ID in AIG

   A positive literal of a variable is the double of it.
   A negative literal is a positive literal plus one

   e.g.
   the first clause ID is  4 7 9
   it means the obj id correspond tp clause is (2 -3 -4)
  */

  std::map<int, std::vector<int>> simResult_fanin;

  // ===================================== Simulation =====================================

  int numPi_fanin0 = Abc_NtkPiNum(pFanin0_Ntk);
  int numPattern_fanin0 = pow(2, numPi_fanin0 - 1);

  //* Because the last parameter of createCone is 1,
  //* PIs of fanin0 will equal to PIs of fanin1

  int numPi_fanin1 = Abc_NtkPiNum(pFanin1_Ntk);
  if (numPi_fanin1 > 16)
    for (int i = 0; i < pow(2, 10); i++)
    {
      int simualtion_pattern = uniformRandInt(pow(2, numPi_fanin1) - 1);
      int sim_result = Simulation(pFanin1_Ntk, simualtion_pattern) * 2 + Simulation(pFanin0_Ntk, simualtion_pattern);
      do_not_care_set.erase(sim_result);
      if (do_not_care_set.empty())
      {
        break;
      }
    }
  else
  {
    for (int i = 0; i < pow(2, numPi_fanin1); i++)
    {
      int sim_result = Simulation(pFanin1_Ntk, i) * 2 + Simulation(pFanin0_Ntk, i);
      do_not_care_set.erase(sim_result);
      if (do_not_care_set.empty())
      {
        break;
      }
    }


    return do_not_care_set;
  }

#if PRINT_DONOT_CARE_SET
  printf("Don't care set: ");
  for (const auto &do_not_care_set_entry : do_not_care_set)
  {
    printf("%d ", do_not_care_set_entry.first);
  }
  printf("\n");
#endif
  // ------------------------------------- End of Simulation -------------------------------------

  Abc_Obj_t *pIteObjFanin0;
  Abc_Obj_t *pIteObjFanin1;
  int k;
  std::map<int, my_aig_node> mObjID_fanin0;
  std::map<int, my_aig_node> mObjID_fanin1;
  // std::map<int, my_aig_node> mObjID_head;

  Cnf_Dat_t *pCnf_fanin0 = Cnf_Derive(pAig_man_fanin0, Aig_ManCoNum(pAig_man_fanin0));
  Cnf_Dat_t *pCnf_fanin1 = Cnf_Derive(pAig_man_fanin1, Aig_ManCoNum(pAig_man_fanin1));
  // Cnf_Dat_t *pCnf_head = Cnf_Derive(pAig_man_head, Aig_ManCoNum(pAig_man_head));

  // Collect all node in fanin0 and fanin1 and obj

#if PRINT_NODE_ID_AND_LITERAL_ID
  printf("=================== fanin0 node ==================\n");
#endif
  Abc_NtkForEachObj(pFanin0_Ntk, pIteObjFanin0, k)
  {
    my_aig_node mAig_IteObj_fanin0;
    mAig_IteObj_fanin0.pObj = pIteObjFanin0;
    mAig_IteObj_fanin0.abc_obj_id = Abc_ObjId(pIteObjFanin0);
    int lit_id = pCnf_fanin0->pVarNums[Abc_ObjId(pIteObjFanin0)];
    mAig_IteObj_fanin0.literal_id = lit_id;
    mObjID_fanin0[lit_id] = mAig_IteObj_fanin0;
#if PRINT_NODE_ID_AND_LITERAL_ID
    if (Abc_ObjIsPi(pIteObjFanin0))
      printf("\tPI  node ID:%d  name:%5s lit id:%d\n", Abc_ObjId(pIteObjFanin0), Abc_ObjName(pIteObjFanin0), lit_id);
    else if (Abc_ObjIsPo(pIteObjFanin0))
      printf("\tPO  node ID:%d  name:%5s lit id:%d\n", Abc_ObjId(pIteObjFanin0), Abc_ObjName(pIteObjFanin0), lit_id);
    else
      printf("\tABC node ID:%d  name:%5s lit id:%d\n", Abc_ObjId(pIteObjFanin0), Abc_ObjName(pIteObjFanin0), lit_id);
#endif
  }
#if PRINT_NODE_ID_AND_LITERAL_ID
  printf("=================== fanin1 node ==================\n");
#endif
  Abc_NtkForEachObj(pFanin1_Ntk, pIteObjFanin1, k)
  {
    my_aig_node mAig_IteObj_fanin1;
    mAig_IteObj_fanin1.pObj = pIteObjFanin1;
    mAig_IteObj_fanin1.abc_obj_id = Abc_ObjId(pIteObjFanin1);
    int lit_id = pCnf_fanin1->pVarNums[Abc_ObjId(pIteObjFanin1)];
    mAig_IteObj_fanin1.literal_id = lit_id;
    mObjID_fanin1[lit_id] = mAig_IteObj_fanin1;
#if PRINT_NODE_ID_AND_LITERAL_ID
    if (Abc_ObjIsPi(pIteObjFanin1))
      printf("\tPI  node ID:%d  name:%5s lit id:%d\n", Abc_ObjId(pIteObjFanin1), Abc_ObjName(pIteObjFanin1), lit_id);
    else if (Abc_ObjIsPo(pIteObjFanin1))
      printf("\tPO  node ID:%d  name:%5s lit id:%d\n", Abc_ObjId(pIteObjFanin1), Abc_ObjName(pIteObjFanin1), lit_id);
    else
      printf("\tABC node ID:%d  name:%5s lit id:%d\n", Abc_ObjId(pIteObjFanin1), Abc_ObjName(pIteObjFanin1), lit_id);
#endif
  }

  // sat_solver *sat_head = sat_solver_new();
  sat_solver *sat_fanin0 = sat_solver_new();
  sat_solver *sat_fanin1 = sat_solver_new();

#if SAT_SOLVER_VERBOSE
  // sat_head->verbosity = 1;
  // sat_head->fPrintClause = 1;
  sat_fanin1->verbosity = 1;
  sat_fanin1->fPrintClause = 1;
  sat_fanin0->verbosity = 1;
  sat_fanin0->fPrintClause = 1;
#endif

#if SAT_SOLVER_VERBOSE
  printf("----------------------CNF fanin0----------------------\n");
#endif
  Cnf_DataWriteIntoSolverInt(sat_fanin0, pCnf_fanin0, 1, 0);
#if SAT_SOLVER_VERBOSE
  printf("----------------------CNF fanin1----------------------\n");
#endif
  Cnf_DataWriteIntoSolverInt(sat_fanin1, pCnf_fanin1, 1, 0);
#if SAT_SOLVER_VERBOSE
  printf("------------------- CNF END  ---------------------\n");
#endif
  //* Transfer don't care result from simulation to SAT solve
  for (const auto &do_not_care_set_entry : do_not_care_set)
  {
    int do_not_care = do_not_care_set_entry.first;

    int sat_fanin0_nodeVal = 0 ^ fanin0_inv ^ fanin0_inv_po_support ? !((bool)(do_not_care & 1)) : (bool)do_not_care & 1; // get fanin0 value and inv
    int sat_fanin1_nodeVal = 0 ^ fanin1_inv ^ fanin1_inv_po_support ? !((bool)((do_not_care & 2) >> 1)) : (bool)((do_not_care & 2) >> 1);

    int sat_fanin0_state = 1;
    int fan0_addclause_state = 1;
    int WHILE_LOOP_MAX_TIMES_FANIN0 = 0;
    std::map<int, std::list<int>> sat_fanin0_ans_list;
    std::vector<std::vector<int>> sat_fanin0_ans_vec;
    std::map<int, int> newClause_Fainin0;

#ifdef DEBUG_EXEC_FAN1_SAT
    goto FANIN1_SAT_SOLVING;
#endif

    //* Fanin0

    for (const auto &mObjID_fanin0_entry : mObjID_fanin0)
    {
      if (mObjID_fanin0_entry.second.abc_obj_id == fanin0_faninObjId)
      {
        newClause_Fainin0[mObjID_fanin0_entry.second.literal_id] = sat_fanin0_nodeVal;
        break;
      }
    }

#if SAT_SOLVER_VERBOSE
    printf("---------------- Add sat_fanin0 output constrain -----------------\n");
#endif
    sat_add_clause(sat_fanin0, newClause_Fainin0);

#if SAT_SOLVER_VERBOSE
    printf("--------------------------------------------\n");
#endif

    //* Fanin 0 sat solving

    while (sat_fanin0_state != -1 && fan0_addclause_state == 1 && WHILE_LOOP_MAX_TIMES_FANIN0 < 10)
    {
#if SAT_SOLVER_VERBOSE
      printf("********************************************\n");
#endif
      sat_fanin0_state = sat_solver_solve_internal(sat_fanin0);

#if SAT_SOLVER_VERBOSE
      printf("sat_fanin0_state:%d\n", sat_fanin0_state);
      printf("********************************************\n");
#endif
      if (sat_fanin0_state == -1)
      {

        break;
      }
      std::map<int, int> satisfiyAns = getSatResult(sat_fanin0, mObjID_fanin0);
      std::vector<int> satAnsVec;

      for (auto &entry : satisfiyAns)
      {
        int lit_id = entry.first;
        int lit_val = entry.second;
        satAnsVec.push_back(lit_val);
        sat_fanin0_ans_list[lit_id].push_back({lit_val});
        // entry.second = !entry.second;
      }
      sat_fanin0_ans_vec.push_back(satAnsVec);
      fan0_addclause_state = sat_add_clause(sat_fanin0, satisfiyAns);

      WHILE_LOOP_MAX_TIMES_FANIN0++;
    }
#if DUMP_ALL_SATISFY_ANS
    for (const auto &entry : sat_fanin0_ans_list)
    {
      printf("Fanin 0 Literal id: %d, Values: ", entry.first);
      for (const auto &val : entry.second)
      {
        printf("%d ", val);
      }
      printf("\n");
    }
#endif
  FANIN1_SAT_SOLVING:
    int sat_fanin1_state = 1;
    int fan1_addclause_state = 1;
    int WHILE_LOOP_MAX_TIMES_FANIN1 = 0;
    std::vector<std::vector<int>> sat_fanin1_ans_vec;
    std::map<int, std::list<int>> sat_fanin1_ans_list;
    std::map<int, int> newClause_fanin1;

    //* Fanin1

    for (const auto &entry : mObjID_fanin1)
    {
      if (entry.second.abc_obj_id == fanin1_faninObjId)
      {
        newClause_fanin1[entry.second.literal_id] = sat_fanin1_nodeVal;
        break;
      }
    }

#if SAT_SOLVER_VERBOSE
    printf("---------------- Add sat_fanin1 output constrain -----------------\n");
#endif
    sat_add_clause(sat_fanin1, newClause_fanin1);
    // sat_add_clause(sat_fanin1,{{3,1},{4,1},{5,1}});

#if SAT_SOLVER_VERBOSE
    printf("--------------------------------------------\n");
#endif

    //* Fanin 1 sat solving
    while (sat_fanin1_state != -1 && fan1_addclause_state == 1 && WHILE_LOOP_MAX_TIMES_FANIN1 < 10)
    {
#if SAT_SOLVER_VERBOSE
      printf("********************************************\n");
#endif
      sat_fanin1_state = sat_solver_solve_internal(sat_fanin1);

#if SAT_SOLVER_VERBOSE
      printf("sat_fanin1_state:%d\n", sat_fanin1_state);
      printf("********************************************\n");
#endif
      if (sat_fanin1_state == -1)
      {

        break;
      }
      std::map<int, int> satisfiyAns = getSatResult(sat_fanin1, mObjID_fanin1);

      std::vector<int> satAnsVec;
      for (auto &entry : satisfiyAns)
      {
        int lit_id = entry.first;
        int lit_val = entry.second;
        satAnsVec.push_back(lit_val);
        sat_fanin1_ans_list[lit_id].push_back({lit_val});
        // entry.second = !entry.second;
      }
      sat_fanin1_ans_vec.push_back(satAnsVec);

      fan1_addclause_state = sat_add_clause(sat_fanin1, satisfiyAns);
      WHILE_LOOP_MAX_TIMES_FANIN1++;
    }
#if DUMP_ALL_SATISFY_ANS
    for (const auto &entry : sat_fanin1_ans_list)
    {
      printf("Fanin1 Literal id: %d, Values: ", entry.first);
      for (const auto &val : entry.second)
      {
        printf("%d ", val);
      }
      printf("\n");
    }
#endif
    //====================================================

    int equal = 0;
    for (const auto &vec_fan0 : sat_fanin0_ans_vec)
    {
      for (const auto &vec_fan1 : sat_fanin1_ans_vec)
      {
        if (std::equal(vec_fan0.begin(), vec_fan0.end(), vec_fan1.begin(), vec_fan1.end()))
        {
          equal = 1;
#if PRINT_DONOT_CARE_SET
          printf("Don't care set remove %d\n", do_not_care_set_entry.first);
#endif
          break;
        }
      }
    }
    if (equal == 1)
    {
      do_not_care_set.erase(do_not_care_set_entry.first);
    }
  }
#if PRINT_DONOT_CARE_SET
  printf("After SAT Don't care set: ");
  for (const auto &do_not_care_set_entry : do_not_care_set)
  {
    printf("%d ", do_not_care_set_entry.first);
  }
  printf("\n");
#endif

  // Abc_NtkDelete(pFanin0_Ntk);
  // Abc_NtkDelete(pFanin1_Ntk);
  // Abc_NtkDelete(pObj_Ntk);
  // Aig_ManStop(pAig);
  sat_solver_delete(sat_fanin0);
  sat_solver_delete(sat_fanin1);
  Cnf_DataFree(pCnf_fanin0);
  Cnf_DataFree(pCnf_fanin1);

  return do_not_care_set;
}

std::map<int, int> Lsv_NtkFindOdc(Abc_Ntk_t *pNtk, int nodeSelect)
{
  Abc_Obj_t *pAbcObj;
  Abc_Obj_t *pTmpObj;
  std::map<int, int> do_not_care_set = {{0, 0}, {1, 1}, {2, 2}, {3, 3}};
  int i;

  Abc_NtkForEachObj(pNtk, pTmpObj, i)
  {

    if (Abc_ObjId(pTmpObj) != nodeSelect)
      continue;
    else
    {
      pAbcObj = pTmpObj;
      break;
    }
  }

  //* Ntk 0 is the original network
  //* Ntk 1 is the network that fanout is inversed

  Abc_Ntk_t *pDumpNtk[2] = {Abc_NtkDup(pNtk), Abc_NtkDup(pNtk)};

  Abc_Obj_t *pFaninIteObj;
  int j = 0;

  Abc_NtkForEachNode(pDumpNtk[0], pFaninIteObj, j)
  {
    Abc_Obj_t *faninId[] = {Abc_ObjFanin0(pFaninIteObj), Abc_ObjFanin1(pFaninIteObj)};
    if (Abc_ObjId(faninId[0]) == Abc_ObjId(pAbcObj))
      pFaninIteObj->fCompl0 = !pFaninIteObj->fCompl0;
    else if (Abc_ObjId(faninId[1]) == Abc_ObjId(pAbcObj))
      pFaninIteObj->fCompl1 = !pFaninIteObj->fCompl1;
  }

  Abc_Obj_t *pDumpNtkObj;
  int k = 0;
  if (!Abc_NtkIsStrash(pDumpNtk[0]))
    pDumpNtk[0] = Abc_NtkStrash(pDumpNtk[0], 0, 0, 0);
  if (!Abc_NtkIsStrash(pDumpNtk[1]))
    pDumpNtk[1] = Abc_NtkStrash(pDumpNtk[1], 0, 0, 0);

  // Abc_NtkAppend(pDumpNtk[0],pDumpNtk[1],1);

  Abc_Ntk_t *pAllMiterNtk;

  pAllMiterNtk = Abc_NtkMiter(pDumpNtk[0], pDumpNtk[1], 1, 0, 0, 0);

  std::vector<int> presimPatten;

  int numPi_fanin1 = Abc_NtkPiNum(pAllMiterNtk);
  if (numPi_fanin1 > 16)
    for (int i = 0; i < pow(2, 10); i++)
    {
      int simualtion_pattern = uniformRandInt(pow(2, numPi_fanin1) - 1);
      if (Simulation(pAllMiterNtk, simualtion_pattern))
      {
        presimPatten.push_back(simualtion_pattern);
      }
    }
  else
  {
    for (int i = 0; i < pow(2, numPi_fanin1); i++)
    {
      if (Simulation(pAllMiterNtk, i))
      {
        presimPatten.push_back(i);
      }
    }
    
    
    for (const auto &entry : presimPatten)
    {
      int careNodeFanin = 0;
      Simulation(pNtk, entry, nodeSelect, &careNodeFanin);
      // printf("@patten %d Fanin %d%d\n",entry,(careNodeFanin&2)>>1,(careNodeFanin&1));
      do_not_care_set.erase(careNodeFanin);
    }

    return do_not_care_set;




  }

  /*
  printf("Don't care set: ");
  for (const auto &entry : do_not_care_set)
    printf("%d ", entry);
  printf("\n");
  */

  pAllMiterNtk = Abc_NtkStrash(pAllMiterNtk, 0, 0, 0);
  // Abc_NtkShow(pAllMiterNtk, 0, 0, 1, 0,1);
  Aig_Man_t *pAigManMiter = Abc_NtkToDar(pAllMiterNtk, 0, 0);
  Cnf_Dat_t *pMiterCnf = Cnf_Derive(pAigManMiter, Abc_NtkPoNum(pAllMiterNtk));
  sat_solver *pMiterSat = sat_solver_new();
#if SAT_SOLVER_VERBOSE
  pMiterSat->verbosity = 1;
  pMiterSat->fPrintClause = 1;
#endif
  Cnf_DataWriteIntoSolverInt(pMiterSat, pMiterCnf, 1, 0);

  std::map<int, my_aig_node> mObjID_miter;
  Abc_Obj_t *pIteObjMiter;

  int l = 0;

  //* record all node od obj id and litral id
  Abc_NtkForEachObj(pAllMiterNtk, pIteObjMiter, l)
  {
    my_aig_node mAig_IteObj_miter;
    mAig_IteObj_miter.pObj = pIteObjMiter;
    mAig_IteObj_miter.abc_obj_id = Abc_ObjId(pIteObjMiter);
    int lit_id = pMiterCnf->pVarNums[Abc_ObjId(pIteObjMiter)];
    mAig_IteObj_miter.literal_id = lit_id;
    mAig_IteObj_miter.node_type = Abc_ObjType(pIteObjMiter);
    mObjID_miter[lit_id] = mAig_IteObj_miter;
#if PRINT_NODE_ID_AND_LITERAL_ID
    if (Abc_ObjIsPi(pIteObjMiter))
      printf("\tPI  node ID:%d  name:%5s lit id:%d\n", Abc_ObjId(pIteObjMiter), Abc_ObjName(pIteObjMiter), lit_id);
    else if (Abc_ObjIsPo(pIteObjMiter))
      printf("\tPO  node ID:%d  name:%5s lit id:%d\n", Abc_ObjId(pIteObjMiter), Abc_ObjName(pIteObjMiter), lit_id);
    else
      printf("\tABC node ID:%d  name:%5s lit id:%d\n", Abc_ObjId(pIteObjMiter), Abc_ObjName(pIteObjMiter), lit_id);
#endif
  }

  Abc_Obj_t *pOutputObj = Abc_NtkPo(pAllMiterNtk, 0);

  //* get edge of miter output
  Abc_Obj_t *pOutputFanin0Obj = Abc_ObjFanin0(pOutputObj);
  int compl0 = pOutputObj->fCompl0;

  std::vector<int> vecPis;
  for (const auto &entry : mObjID_miter)
  {
    if (entry.second.node_type == ABC_OBJ_PI)
      vecPis.push_back(entry.first);
  }

  std::map<int, int> newClause_miter;
  std::map<int, int> presimPattenClause_miter;

  //* set output edge to 1 to find the odc
  if (pOutputFanin0Obj != NULL)
  {
    for (const auto &entry : mObjID_miter)
    {
      if (entry.second.abc_obj_id == Abc_ObjId(pOutputFanin0Obj))
      {
        newClause_miter[entry.second.literal_id] = 1 ^ compl0;
        break;
      }
    }
  }

  //* satrt sat solver

  std::vector<std::vector<int>> vecMiterAns;

  int pMiterSat_state = 1;
  int pMiterSat_addclause_state = 1;
  // printf("Add output constrain\n");

  pMiterSat_addclause_state = sat_add_clause(pMiterSat, newClause_miter);

  newClause_miter.clear();

  // printf("Add Input Pattens\n");

  for (size_t jj = 0; jj < presimPatten.size(); jj++)
  {
    for (int vecPis_i = 0; vecPis_i < vecPis.size(); vecPis_i++)
    {
      presimPattenClause_miter[vecPis[vecPis_i]] = (presimPatten[jj] & (1 << vecPis_i)) ? 1 : 0;
    }
    pMiterSat_addclause_state = sat_add_clause(pMiterSat, presimPattenClause_miter);
    // TODO: if can not add clause, record it in new sim patten
  }

  /*
  printf("Listing simPatten:\n");
  for (const auto &pattern : presimPatten)
  {
    printf("%d ", pattern);
  }
  printf("\n");

  */

  while (1)
  {
    pMiterSat_state = sat_solver_solve(pMiterSat, NULL, NULL, 0, 0, 0, 0);
    if (!(pMiterSat_addclause_state == 1 && pMiterSat_state == 1))
      break;
    std::map<int, int> satisfiyAns = getSatResult(pMiterSat, mObjID_miter);
    std::vector<int> satAnsVec;
    for (auto &entry : satisfiyAns)
    {
      int lit_val = entry.second;
      satAnsVec.push_back(lit_val);
    }
    vecMiterAns.push_back(satAnsVec);
    pMiterSat_addclause_state = sat_add_clause(pMiterSat, satisfiyAns);
    // printf("Add clause state %d\n",pMiterSat_addclause_state);
    // printf("Add SAT state %d\n",pMiterSat_state);
  }

  for (const auto &vec : vecMiterAns)
  {
    int value = 0;
    int index = 0;
    for (const auto &val : vec)
    {
      value += val << index++;
      // printf("%d ", val);
    }
    presimPatten.push_back(value);
    // printf("value %d\n",value);
  }



  for (const auto &entry : presimPatten)
  {
    int careNodeFanin = 0;
    Simulation(pNtk, entry, nodeSelect, &careNodeFanin);
    // printf("@patten %d Fanin %d%d\n",entry,(careNodeFanin&2)>>1,(careNodeFanin&1));
    do_not_care_set.erase(careNodeFanin);
  }

  return do_not_care_set;

  /*


  Abc_ObjXorFaninC( Abc_Obj_t * pObj, int i )

  extern ABC_DLL Abc_Obj_t * Abc_NtkFindNode( Abc_Ntk_t * pNtk, char * pName );

  Abc_NtkMiter(pNtk1, pNtk2, 1, 0, 0, 0)

  Abc_NtkAppend


  */
}

int Lsv_CommandNtkFindSdc(Abc_Frame_t *pAbc, int argc, char **argv)
{
  Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
  int c;
  int nodeSelect;
  std::map<int, int> result;
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
  if (argc <= 1)
  {
    printf("Please type Obj ID to find SDC\n");
  }
  nodeSelect = atoi(argv[1]);
  result = Lsv_NtkFindSdc(pNtk, nodeSelect);
  if (result.empty())
  {
    printf("no sdc\n");
  }
  else
  {
    for (const auto &entry : result)
    {
      switch (entry.second)
      {
      case 0:
        printf("00");
        break;
      case 1:
        printf("01");
        break;
      case 2:
        printf("10");
        break;
      case 3:
        printf("11");
        break;
      }
      printf(" ");
    }
     printf("\n");
  }

  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sdc [-h]\n");
  Abc_Print(-2, "\t        prints the nodes in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

int Lsv_CommandNtkFindOdc(Abc_Frame_t *pAbc, int argc, char **argv)
{
  Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
  int c;
  int nodeSelect;
  std::map<int, int> odc;
  std::map<int, int> sdc;
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
  if (argc <= 1)
  {
    printf("Please type Obj ID to find ODC\n");
  }
  nodeSelect = atoi(argv[1]);
  sdc = Lsv_NtkFindSdc(pNtk, nodeSelect);
  odc = Lsv_NtkFindOdc(pNtk, nodeSelect);

  // Remove common elements in sdc and odc
  for (const auto &entry : sdc)
  {
    if (odc.find(entry.first) != odc.end())
    {
      odc.erase(entry.first);
    }
  }

  if (odc.empty())
  {
    printf("no odc\n");
  }
  else
  {
    for (const auto &entry : odc)
    {
      switch (entry.second)
      {
      case 0:
        printf("00");
        break;
      case 1:
        printf("01");
        break;
      case 2:
        printf("10");
        break;
      case 3:
        printf("11");
        break;
      }
      printf(" ");
    }
    printf("\n");
  }

  /*
  if (result.empty())
  {
    printf("no odc\n");
  }
  else
  {
    for (const auto &entry : result)
    {
      switch (entry.second)
      {
        case 0:printf("00");break;
        case 1:printf("01");break;
        case 2:printf("10");break;
        case 3:printf("11");break;
      }
      printf("\n");
    }
  }
  */

  return 0;

usage:
  Abc_Print(-2, "usage: lsv_odc [-h]\n");
  Abc_Print(-2, "\t        prints the odc of nodes in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

int Lsv_CommandNtkSim(Abc_Frame_t *pAbc, int argc, char **argv)
{
  Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
  int c;
  int pattern;
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
  // pattern = atoi(argv[1]);
  testSim(pNtk);

  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sim [-h]\n");
  Abc_Print(-2, "\t n      simulation the network in specify pattern\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}