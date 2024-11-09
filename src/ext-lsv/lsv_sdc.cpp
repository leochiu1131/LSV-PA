#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/cnf/cnf.h"

#include <queue>
#include <list>
#include <unordered_set>
#include <algorithm>
#include <string>
#include <iostream>
#include <fstream>
#include <map>





//#define GET_CONE
//#define PRINT_SIMULATION_PROCESS
#define PRINT_SIMULATION_RESULT
//#define PRINT_SIMULATION_MAP

extern "C"
{
  Aig_Man_t *Abc_NtkToDar(Abc_Ntk_t *pNtk, int fExors, int fRegisters);
}

static int Lsv_CommandNtkFindSdc(Abc_Frame_t *pAbc, int argc, char **argv);

void init(Abc_Frame_t *pAbc)
{
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sdc", Lsv_CommandNtkFindSdc, 0);
}

void destroy(Abc_Frame_t *pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager
{
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

// *********************************** Self define program start ***************************
// *****************************************************************************************

struct Abc_obj_sim
{
  Abc_Obj_t *pObj;
  int sim_value;
};

void Lsv_SimAIG(Abc_Ntk_t *pNtk, Abc_Obj_t *pObj_head, u_int64_t patterns)
{

  // 1. find all pis

  std::map<int, Abc_obj_sim *> obj_sim_map; // first is id, second is obj
  Abc_Obj_t *pObj;

  int pi_ite_num = 0;
  Abc_NtkForEachPi(pNtk, pObj, pi_ite_num)
  {
    Abc_obj_sim *pobj_sim = new Abc_obj_sim;
    pobj_sim->pObj = pObj;
    pobj_sim->sim_value = patterns & (1 << pi_ite_num) ? 1 : 0;
    obj_sim_map[Abc_ObjId(pobj_sim->pObj)] = pobj_sim;
    #ifdef PRINT_SIMULATION_RESULT
      printf("PI ID: %2d, Name: %5s, Value: %d\n", Abc_ObjId(pobj_sim->pObj), Abc_ObjName(pobj_sim->pObj), pobj_sim->sim_value);
    #endif
  }
  // Print obj_sim_map


  int obj_ite_num = 0;
  Abc_NtkForEachNode(pNtk, pObj, obj_ite_num)
  {
    //printf("\tCompute throght ID: %d\n", Abc_ObjId(pObj));
    Abc_obj_sim *pobj_sim = new Abc_obj_sim;
    pobj_sim->pObj = pObj;

    Abc_Obj_t *pFain;
    int fanin_num = 0;
    

    bool fanin_compl_edge[2] = {pObj->fCompl0,pObj->fCompl1};
    int fanin_id[2] = {0,0};

    Abc_ObjForEachFanin(pObj, pFain, fanin_num)
    {
      fanin_id[fanin_num] = Abc_ObjId(pFain);
      #ifdef PRINT_SIMULATION_PROCESS
      if(fanin_num == 1)
      printf("\t\tNode_ID: %3d, Fanin0_ID: %3d, Fanin1_ID: %3d, Fanin0_Compl: %d Fanin1_Compl: %d \n",Abc_ObjId(pObj),fanin_id[0], fanin_id[1],  fanin_compl_edge[0],fanin_compl_edge[1]);
      #endif
    }

    // simulation data
    bool edge_0_val = obj_sim_map.find(fanin_id[0])->second->sim_value ;
    edge_0_val = fanin_compl_edge[0]? !edge_0_val:edge_0_val;
    bool edge_1_val = obj_sim_map.find(fanin_id[1])->second->sim_value ;
    edge_1_val = fanin_compl_edge[1]? !edge_1_val:edge_1_val;

    pobj_sim->sim_value = edge_0_val & edge_1_val;
    obj_sim_map[Abc_ObjId(pobj_sim->pObj)] = pobj_sim;
  }
  
  Abc_NtkForEachPo(pNtk, pObj, obj_ite_num)
  {
    Abc_obj_sim *pobj_sim = new Abc_obj_sim;
    Abc_Obj_t *pFain;
    int fanin_num = 0;
    pobj_sim->pObj = pObj;

    Abc_ObjForEachFanin(pObj, pFain, fanin_num)
    {      
      bool output_val = pObj->fCompl0? !obj_sim_map[Abc_ObjId(pFain)]->sim_value:obj_sim_map[Abc_ObjId(pFain)]->sim_value;
      obj_sim_map[Abc_ObjId(pobj_sim->pObj)] = pobj_sim;
      pobj_sim->sim_value = output_val;
      #ifdef PRINT_SIMULATION_RESULT
        printf("PO ID: %2d, Name: %5s, Value: %d\n",Abc_ObjId(pobj_sim->pObj),Abc_ObjName(pobj_sim->pObj),pobj_sim->sim_value);
      #endif
    }
    
  }
  
  
  // Dump the map
  #ifdef PRINT_SIMULATION_MAP
    for (const auto &entry : obj_sim_map)
    {
      printf("\t\tName: %d,\t Obj: %6s\t\t Value:%d\n", entry.first, Abc_ObjName(entry.second->pObj), (entry.second->sim_value));
    }
  #endif
}

// ******************************** Self define program end ********************************
// *****************************************************************************************

void Lsv_NtkFindSdc(Abc_Ntk_t *pNtk)
{

  Abc_Obj_t *pObj;
  
for (size_t patten = 0; patten <= 7; patten++)
{
  printf("Simulation with %d pattern\n",patten);
  Lsv_SimAIG(pNtk, pObj, patten);
  printf("====================================\n");
}



return;
  int i;
  Abc_NtkForEachNode(pNtk, pObj, i)
  {

    if (Abc_ObjId(pObj) != 11 && Abc_ObjId(pObj) != 15)
      continue;
    fprintf(stdout, "Node ID: %d\n", Abc_ObjId(pObj));

    Abc_Ntk_t *sub_ntk;

#ifdef GET_CONE
    Aig_Man_t *pMan;
    sub_ntk = Abc_NtkCreateCone(pNtk, pObj, Abc_ObjName(pObj), 1);
    int nNodes = Abc_NtkNodeNum(sub_ntk);
#else
    sub_ntk = pNtk;
#endif

    Lsv_SimAIG(sub_ntk, pObj, 0);

    
  }
}

int Lsv_CommandNtkFindSdc(Abc_Frame_t *pAbc, int argc, char **argv)
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
  Lsv_NtkFindSdc(pNtk);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sdc [-h]\n");
  Abc_Print(-2, "\t        prints the nodes in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}
