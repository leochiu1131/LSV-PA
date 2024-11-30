#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/cnf/cnf.h"
#include "aig/aig/aig.h"
#include <iostream>
#include <unordered_map>
#include <set>
#include <vector>
#include <unordered_set>


static std::vector<bool> check_sdc(Aig_Man_t* pAig , sat_solver* pSat) {
    // Create SAT solver
    Aig_Obj_t* pSavedNode = Aig_ObjChild0(Aig_ManCo(pAig,0));
    // Visit each node in AIG and create CNF clauses
    Aig_Obj_t* pObj;
    int i;
    
    Aig_ManForEachObj(pAig, pObj, i) {
      if (Aig_ObjIsConst1(pObj)) {
        // Constant 1 node - add unit clause
        lit Lits[1];
        Lits[0] = toLitCond(Aig_ObjId(pObj), 0); // Positive literal
        sat_solver_addclause(pSat, Lits, Lits + 1);
      }
      else if (Aig_ObjIsCi(pObj)) {
        // Primary input - no clauses needed
      }
      else if (Aig_ObjIsNode(pObj)) {
        // AND gate
        Aig_Obj_t* pFanin0 = Aig_ObjFanin0(pObj);
        Aig_Obj_t* pFanin1 = Aig_ObjFanin1(pObj);
        lit Lits[3];
        
        // z = x AND y clauses: (!x + !y + z)(!z + x)(!z + y)
        Lits[0] = toLitCond(Aig_ObjId(pFanin0), !Aig_ObjFaninC0(pObj));
        Lits[1] = toLitCond(Aig_ObjId(pFanin1), !Aig_ObjFaninC1(pObj));
        Lits[2] = toLitCond(Aig_ObjId(pObj), 0);
        sat_solver_addclause(pSat, Lits, Lits + 3);
        
        Lits[0] = toLitCond(Aig_ObjId(pObj), 1);
        Lits[1] = toLitCond(Aig_ObjId(pFanin0), Aig_ObjFaninC0(pObj));
        sat_solver_addclause(pSat, Lits, Lits + 2);
        
        Lits[0] = toLitCond(Aig_ObjId(pObj), 1);
        Lits[1] = toLitCond(Aig_ObjId(pFanin1), Aig_ObjFaninC1(pObj));
        sat_solver_addclause(pSat, Lits, Lits + 2);
     }
    }

    // Get fanins of saved node
    Aig_Obj_t* pSavedFanin0 = Aig_ObjFanin0(pSavedNode);
    Aig_Obj_t* pSavedFanin1 = Aig_ObjFanin1(pSavedNode);
    int pSavedFaninC0 = Aig_ObjFaninC0(pSavedNode);
    int pSavedFaninC1 = Aig_ObjFaninC1(pSavedNode);

    std::vector<bool> sdcs;
    // Check all possible input combinations
    int combinations[4][2] = {{!pSavedFaninC0, !pSavedFaninC1}, {!pSavedFaninC0, pSavedFaninC1}, {pSavedFaninC0, !pSavedFaninC1}, {pSavedFaninC0, pSavedFaninC1}};
    
    for (int i = 0; i < 4; i++) {
      // Create assumptions for this combination
      lit Lits[2];
      Lits[0] = toLitCond(Aig_ObjId(pSavedFanin0), combinations[i][0]);
      Lits[1] = toLitCond(Aig_ObjId(pSavedFanin1), combinations[i][1]);
      
      int status = sat_solver_solve(pSat, Lits, Lits + 2, 0, 0, 0, 0);
      
      if (status == l_False) {
        sdcs.push_back(false);
      }
      else {
        sdcs.push_back(true);
      }
    }

    // Cleanup
    sat_solver_delete(pSat);
    return sdcs;
}