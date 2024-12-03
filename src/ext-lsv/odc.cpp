#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/cnf/cnf.h"
#include "bdd/llb/llb.h"
#include "aig/gia/giaAig.h"
#include "aig/aig/aig.h"
#include "sdc.h"
#include <iostream>
#include <unordered_map>
#include <set>
#include <vector>
#include <unordered_set>

extern "C"{
  Aig_Man_t* Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
}



static int Lsv_CommandODC(Abc_Frame_t* pAbc, int argc, char** argv);

void init_ODC(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_odc", Lsv_CommandODC, 0);
}

void destroy_ODC(Abc_Frame_t* pAbc) {
  
}

Abc_FrameInitializer_t frame_initializer_odc = {init_ODC, destroy_ODC};

struct OdcRegistrationManager {
  OdcRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer_odc); }
} lsvOdcRegistrationManager;
 
  void print_clause(lit* Lits, lit* end) {
    printf("Adding clause: ");
    for (lit* l = Lits; l < end; l++) {
      printf("%d ", *l);
    }
    printf("\n");
  }


static int Lsv_CommandODC(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  char node;
  if (argc <= 1) {
    // Abc_Print(-1, " enter a node id\n");
    return 0;
  } else {
    node = atoi(argv[1]);
  }

  
  Abc_Obj_t* pNode = Abc_NtkObj(pNtk, node);

  // Create a cone network rooted at pNode
  Abc_Ntk_t* pCone = Abc_NtkCreateCone(pNtk, pNode, Abc_ObjName(pNode), 0);


  // Convert cone to AIG
  
  Aig_Man_t* pAig = Abc_NtkToDar(pNtk, 0, 0);
  Aig_Man_t* pSubAig = Abc_NtkToDar(pCone, 0, 0);

  sat_solver* pSat = sat_solver_new();
  sat_solver* pSubSat = sat_solver_new();

  std::vector<int> pPrimaryOuts;
  int numOutputs = Aig_ManCoNum(pAig);

  for (int i = 0; i < Aig_ManCoNum(pAig); i++) {
    pPrimaryOuts.push_back(Aig_ObjId(Aig_ObjFanin0(Aig_ManCo(pAig, i))) );
    //Abc_Print(1, "pPrimaryOuts: %d\n", pPrimaryOuts[i]);
  }
  

  std::vector<bool> sdcs = check_sdc(pSubAig, pSubSat);



  Aig_Obj_t* pSavedNode =  Aig_ManObj(pAig, Abc_ObjId(pNode) - numOutputs);
  //Abc_Print(1, "pSavedNode: %d\n", Aig_ObjId(pSavedNode));
  Aig_Obj_t* pSavedFanin0 = Aig_ObjFanin0(pSavedNode);
  //Abc_Print(1, "pSavedFanin0: %d\n", Aig_ObjId(pSavedFanin0));
  Aig_Obj_t* pSavedFanin1 = Aig_ObjFanin1(pSavedNode);
  //Abc_Print(1, "pSavedFanin1: %d\n", Aig_ObjId(pSavedFanin1));

  
  int pSavedFaninC0 = Aig_ObjFaninC0(pSavedNode);
  int pSavedFaninC1 = Aig_ObjFaninC1(pSavedNode);

  int combinations[4][2] = {{!pSavedFaninC0, !pSavedFaninC1}, {!pSavedFaninC0, pSavedFaninC1}, {pSavedFaninC0, !pSavedFaninC1}, {pSavedFaninC0, pSavedFaninC1}};
  int show_combinations[4][2] = {{0,0}, {0,1}, {1,0}, {1,1}};

  // Visit each node in AIG and create CNF clauses
  Aig_Obj_t* pObj;
  int i;
  
  int numObj = Aig_ManObjNum(pAig);
  // Add function to print clause literals before adding them


  lit or_lits[numOutputs];
  int kk = 0;
  Aig_ManForEachObj(pAig, pObj, i) {
    //Abc_Print(1, "pObj: %d\n", Aig_ObjId(pObj));

    if (Aig_ObjIsCi(pObj)) {
      //Abc_Print(1, "ci\n");
      lit Lits[2];
      Lits[0] = toLitCond(Aig_ObjId(pObj), 0);
      Lits[1] = toLitCond(Aig_ObjId(pObj) + numObj, 1);
      sat_solver_addclause(pSat, Lits, Lits + 2);
      //print_clause(Lits, Lits + 2);
      Lits[0] = toLitCond(Aig_ObjId(pObj), 1);
      Lits[1] = toLitCond(Aig_ObjId(pObj) + numObj, 0);
      sat_solver_addclause(pSat, Lits, Lits + 2);
    }
    // if(Aig_ObjIsCo(pObj)) {
    //   Abc_Print(1, "co\n");
    //   or_lits[kk++] = toLitCond(Aig_ObjId(pObj) + 2*numObj, 1);
    // }
    else if (Aig_ObjIsNode(pObj)) {
      //Abc_Print(1, "node\n");
      // Skip the node under output of pSubAig
      lit Lits[3];
      // AND gate
      Aig_Obj_t* pFanin0 = Aig_ObjFanin0(pObj);
      Aig_Obj_t* pFanin1 = Aig_ObjFanin1(pObj);
      //Abc_Print(1, "pFanin0: %d\n", Aig_ObjId(pFanin0));
      //Abc_Print(1, "pFanin1: %d\n", Aig_ObjId(pFanin1));

      Lits[0] = toLitCond(Aig_ObjId(pFanin0), !Aig_ObjFaninC0(pObj));
      Lits[1] = toLitCond(Aig_ObjId(pFanin1), !Aig_ObjFaninC1(pObj));
      Lits[2] = toLitCond(Aig_ObjId(pObj), 0);
      sat_solver_addclause(pSat, Lits, Lits + 3);
      //print_clause(Lits, Lits + 3);
      Lits[0] = toLitCond(Aig_ObjId(pObj), 1);
      Lits[1] = toLitCond(Aig_ObjId(pFanin0), Aig_ObjFaninC0(pObj));
      sat_solver_addclause(pSat, Lits, Lits + 2);
      //print_clause(Lits, Lits + 2);       
      Lits[0] = toLitCond(Aig_ObjId(pObj), 1);
      Lits[1] = toLitCond(Aig_ObjId(pFanin1), Aig_ObjFaninC1(pObj));
      sat_solver_addclause(pSat, Lits, Lits + 2);
      //print_clause(Lits, Lits + 2);
      if( Aig_ObjId(pFanin0) == Aig_ObjId(pSavedNode) ) {
        //Abc_Print(1, "saved fanin0\n");
          Lits[0] = toLitCond(Aig_ObjId(pFanin0) + numObj, Aig_ObjFaninC0(pObj));
          Lits[1] = toLitCond(Aig_ObjId(pFanin1) + numObj, !Aig_ObjFaninC1(pObj));
          Lits[2] = toLitCond(Aig_ObjId(pObj) + numObj, 0);
          sat_solver_addclause(pSat, Lits, Lits + 3);
          //print_clause(Lits, Lits + 3);          
          Lits[0] = toLitCond(Aig_ObjId(pObj) + numObj, 1);
          Lits[1] = toLitCond(Aig_ObjId(pFanin0) + numObj, !Aig_ObjFaninC0(pObj));
          sat_solver_addclause(pSat, Lits, Lits + 2);
          //print_clause(Lits, Lits + 2);       
          Lits[0] = toLitCond(Aig_ObjId(pObj) + numObj, 1);
          Lits[1] = toLitCond(Aig_ObjId(pFanin1) + numObj, Aig_ObjFaninC1(pObj));
            sat_solver_addclause(pSat, Lits, Lits + 2);
          //print_clause(Lits, Lits + 2);
      }
      else if (Aig_ObjId(pFanin1) == Aig_ObjId(pSavedNode)) {
        //Abc_Print(1, "saved fanin1\n");
          Lits[0] = toLitCond(Aig_ObjId(pFanin0) + numObj, !Aig_ObjFaninC0(pObj));
          Lits[1] = toLitCond(Aig_ObjId(pFanin1) + numObj, Aig_ObjFaninC1(pObj));
          Lits[2] = toLitCond(Aig_ObjId(pObj) + numObj, 0);
          sat_solver_addclause(pSat, Lits, Lits + 3);
          //print_clause(Lits, Lits + 3);           
          Lits[0] = toLitCond(Aig_ObjId(pObj) + numObj, 1);
          Lits[1] = toLitCond(Aig_ObjId(pFanin0) + numObj, Aig_ObjFaninC0(pObj));
          sat_solver_addclause(pSat, Lits, Lits + 2);
          //print_clause(Lits, Lits + 2); 
          Lits[0] = toLitCond(Aig_ObjId(pObj) + numObj, 1);
          Lits[1] = toLitCond(Aig_ObjId(pFanin1) + numObj, !Aig_ObjFaninC1(pObj));
            sat_solver_addclause(pSat, Lits, Lits + 2);
          //print_clause(Lits, Lits + 2);
      }
      else {
          Lits[0] = toLitCond(Aig_ObjId(pFanin0) + numObj, !Aig_ObjFaninC0(pObj));
          Lits[1] = toLitCond(Aig_ObjId(pFanin1) + numObj, !Aig_ObjFaninC1(pObj));
          Lits[2] = toLitCond(Aig_ObjId(pObj) + numObj, 0);
          sat_solver_addclause(pSat, Lits, Lits + 3);
          //print_clause(Lits, Lits + 3);
          Lits[0] = toLitCond(Aig_ObjId(pObj) + numObj, 1);
          Lits[1] = toLitCond(Aig_ObjId(pFanin0) + numObj, Aig_ObjFaninC0(pObj));
          sat_solver_addclause(pSat, Lits, Lits + 2);
          //print_clause(Lits, Lits + 2);
          Lits[0] = toLitCond(Aig_ObjId(pObj) + numObj, 1);
          Lits[1] = toLitCond(Aig_ObjId(pFanin1) + numObj, Aig_ObjFaninC1(pObj));
          sat_solver_addclause(pSat, Lits, Lits + 2);
          //print_clause(Lits, Lits + 2);
      }
      if(std::find(pPrimaryOuts.begin(), pPrimaryOuts.end(), Aig_ObjId(pObj)- numOutputs) != pPrimaryOuts.end()) {
          //Abc_Print(1, "primary out\n");
          if(Aig_ObjId(pObj) == Aig_ObjId(pSavedNode)) {
            lit Lits[3];
            Lits[0] = toLitCond(Aig_ObjId(pObj), 1);
            Lits[1] = toLitCond(Aig_ObjId(pObj) + numObj, 0);
            Lits[2] = toLitCond(Aig_ObjId(pObj) + 2*numObj, 0);
            //print_clause(Lits, Lits + 3);
            sat_solver_addclause(pSat, Lits, Lits + 3);
            Lits[0] = toLitCond(Aig_ObjId(pObj), 0); 
            Lits[1] = toLitCond(Aig_ObjId(pObj) + numObj, 1);
            Lits[2] = toLitCond(Aig_ObjId(pObj) + 2*numObj, 0);
            //print_clause(Lits, Lits + 3);
            sat_solver_addclause(pSat, Lits, Lits + 3);
            Lits[0] = toLitCond(Aig_ObjId(pObj), 0); 
            Lits[1] = toLitCond(Aig_ObjId(pObj) + numObj, 0);
            Lits[2] = toLitCond(Aig_ObjId(pObj) + 2*numObj, 1);
            sat_solver_addclause(pSat, Lits, Lits + 3);
            //print_clause(Lits, Lits + 3);
            Lits[0] = toLitCond(Aig_ObjId(pObj), 1); 
            Lits[1] = toLitCond(Aig_ObjId(pObj) + numObj, 1);
            Lits[2] = toLitCond(Aig_ObjId(pObj) + 2*numObj, 1);
            sat_solver_addclause(pSat, Lits, Lits + 3);
            //print_clause(Lits, Lits + 3);
          }
          else {
            lit Lits[3];
            Lits[0] = toLitCond(Aig_ObjId(pObj), 1);
            Lits[1] = toLitCond(Aig_ObjId(pObj) + numObj, 1);
            Lits[2] = toLitCond(Aig_ObjId(pObj) + 2*numObj, 0); 
            sat_solver_addclause(pSat, Lits, Lits + 3);
            //print_clause(Lits, Lits + 3);
            Lits[0] = toLitCond(Aig_ObjId(pObj), 0); 
            Lits[1] = toLitCond(Aig_ObjId(pObj) + numObj, 0);
            Lits[2] = toLitCond(Aig_ObjId(pObj) + 2*numObj, 0);
            sat_solver_addclause(pSat, Lits, Lits + 3);
            //print_clause(Lits, Lits + 3);
            Lits[0] = toLitCond(Aig_ObjId(pObj), 0); 
            Lits[1] = toLitCond(Aig_ObjId(pObj) + numObj, 1);
            Lits[2] = toLitCond(Aig_ObjId(pObj) + 2*numObj, 1);
            sat_solver_addclause(pSat, Lits, Lits + 3);
            //print_clause(Lits, Lits + 3);
            Lits[0] = toLitCond(Aig_ObjId(pObj), 1); 
            Lits[1] = toLitCond(Aig_ObjId(pObj) + numObj, 0);
            Lits[2] = toLitCond(Aig_ObjId(pObj) + 2*numObj, 1);
            sat_solver_addclause(pSat, Lits, Lits + 3);
            //print_clause(Lits, Lits + 3);
          }
          or_lits[kk++] = toLitCond(Aig_ObjId(pObj) + 2*numObj, 1);
        }
    }
  }
  sat_solver_addclause(pSat, or_lits, or_lits + numOutputs);





  bool found = false;
  // Check if the saved node is satisfiable
  for (int i = 0; i < 4; i++) {
    if(sdcs[i]) {
      lit Lits[4];
      Lits[0] = toLitCond(Aig_ObjId(pSavedFanin0), combinations[i][0]);
      Lits[1] = toLitCond(Aig_ObjId(pSavedFanin1), combinations[i][1]);
      Lits[2] = toLitCond(Aig_ObjId(pSavedFanin0) + numObj, combinations[i][0]);
      Lits[3] = toLitCond(Aig_ObjId(pSavedFanin1) + numObj, combinations[i][1]);

      int result = sat_solver_solve(pSat, Lits, Lits + 4, 0, 0, 0, 0);
      if (result == l_False) {
        Abc_Print(1, "%d%d ", show_combinations[i][0], show_combinations[i][1]);
        found = true;
      }
    }
  }
  if (found == false) {
    Abc_Print(1, "no odc");
  } 
  Abc_Print(1, "\n");

  // Cleanup
  sat_solver_delete(pSat);
  Aig_ManStop(pAig);

}
