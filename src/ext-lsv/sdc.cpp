#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/cnf/cnf.h"
#include "bdd/llb/llb.h"
#include "sdc.h"
#include "aig/gia/giaAig.h"
#include "aig/aig/aig.h"
#include <iostream>
#include <unordered_map>
#include <set>
#include <vector>
#include <unordered_set>

extern "C"{
  Aig_Man_t* Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
}



static int Lsv_CommandSDC(Abc_Frame_t* pAbc, int argc, char** argv);

void init_SDC(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sdc", Lsv_CommandSDC, 0);
}

void destroy_SDC(Abc_Frame_t* pAbc) {
  
}

Abc_FrameInitializer_t frame_initializer_sdc = {init_SDC, destroy_SDC};

struct SdcRegistrationManager {
  SdcRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer_sdc); }
} lsvSdcRegistrationManager;
 

static int Lsv_CommandSDC(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  char node;
  if (argc <= 1) {
    // Abc_Print(-1, " enter a node id\n");
    return 0;
  } else {
    node = atoi(argv[1]);
    
  }

  Abc_Obj_t* pNode = Abc_NtkObj(pNtk, node);
  if (pNode == NULL) {
    // Abc_Print(-1, "Node with id %d does not exist in the network.\n", node);
    return 0;
  }
  // Create a cone network rooted at pNode
  Abc_Ntk_t* pCone = Abc_NtkCreateCone(pNtk, pNode, Abc_ObjName(pNode), 0);
  if (pCone == NULL) {
    //Abc_Print(-1, "Failed to create cone network.\n");
    return 0;
  }

  // Convert cone to AIG
  sat_solver* pSat = sat_solver_new();
  Aig_Man_t* pAig = Abc_NtkToDar(pCone, 0, 0);
  if (pAig == NULL) {
    // Abc_Print(-1, "Converting cone to AIG failed.\n");
    Abc_NtkDelete(pCone);
    return 0;
  }
  std::vector<bool> sdcs = check_sdc(pAig,pSat);
  // Cleanup
  int show_combinations[4][2] = {{0,0}, {0,1}, {1,0}, {1,1}};
  bool found = false;
  for (int i = 0; i < 4; i++) {
    if (!sdcs[i]) {
      Abc_Print(1, "%d%d ", show_combinations[i][0], show_combinations[i][1]);
      found = true;
    }
  }
  if (!found) {
    Abc_Print(1, "no sdc");
  }
  Abc_Print(1, "\n");


  Aig_ManStop(pAig);
  Abc_NtkDelete(pCone);
  
  
 }