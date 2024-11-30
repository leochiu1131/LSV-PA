#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <stdlib.h>
#include <map>
#include <vector>
#include <algorithm>
#include <iostream>
#include <set>
#include "sat/bsat/satSolver.h"
#include "opt/sim/sim.h"
#include "aig/aig/aig.h"
#include "sat/cnf/cnf.h"
extern "C"{
Aig_Man_t* Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
}

using Cut = std::vector<unsigned int>;
using CutSet = std::set<Cut>;

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintCut(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSDC(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandODC(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_CommandPrintCut, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sdc", Lsv_CommandSDC, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_odc", Lsv_CommandODC, 0);
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
      printf("  Fanin-%d: Id = %d, name = %s, compl = %d\n", j, Abc_ObjId(pFanin),
             Abc_ObjName(pFanin), Abc_ObjFaninC(pObj, j));
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

// TODO PA1 Q4
void Lsv_NtkCrossProduct(const std::map<unsigned int, CutSet> &mp, 
                                const std::vector<unsigned int> &fanins, CutSet &res, const int& k) {
  // Check if fanins is empty
  if (fanins.empty()) {
    return;  // Nothing to do
  }

  // Initialize result with the vectors from the first element in fanins
  int first_fanin = fanins[0];
  if (mp.find(first_fanin) != mp.end())
    res = mp.at(first_fanin);  // Start with the first set of vectors
   else 
    return;  // First element not in the map, nothing to process
  
  for (size_t i = 1; i < fanins.size(); ++i) {
    int fanin = fanins[i];

    // Check if the fanin is in the map
    if (mp.find(fanin) == mp.end()) {
        continue;  // Skip if the fanin is not in the map
    }

    const CutSet &next_set = mp.at(fanin);
    CutSet new_res;

    // Perform cross-product between current result and the next set of cuts
    for (const auto &current_cut : res) {
      for (const auto &next_cut : next_set) {
        Cut combined_cut = current_cut;
        combined_cut.insert(combined_cut.end(), next_cut.begin(), next_cut.end());

        // Remove duplicates within the cut
        std::sort(combined_cut.begin(), combined_cut.end());
        combined_cut.erase(
            std::unique(combined_cut.begin(), combined_cut.end()),
            combined_cut.end());

        // Check the size after removing duplicates
        if (combined_cut.size() > static_cast<size_t>(k))
          continue;

        new_res.insert(combined_cut);  // Add to the new result set
      }
    }

    res = std::move(new_res);  // Update result
  }

}

void Lsv_NtkPrintCuts(Abc_Ntk_t* pNtk, int k) {
  std::map<unsigned int, CutSet> mp;
  Abc_Obj_t* pObj;
  int i;

  Abc_NtkForEachPi(pNtk, pObj, i) {
    unsigned int obj_id = Abc_ObjId(pObj);
    mp[obj_id] = {{obj_id}};
    printf("%d: %d\n", obj_id, obj_id);
  }

  Abc_NtkForEachNode(pNtk, pObj, i) {
    unsigned int obj_id = Abc_ObjId(pObj);
    Abc_Obj_t* pFanin;
    int j;
    std::vector<unsigned int> fanins;
    // For each node, find their fanins
    Abc_ObjForEachFanin(pObj, pFanin, j) {
      fanins.push_back(Abc_ObjId(pFanin));
    }

    CutSet res;
    Lsv_NtkCrossProduct(mp, fanins, res, k);

    // Include the node itself as a cut
    res.insert({obj_id});

    // Store the cuts for future use
    mp[obj_id] = res;
    
    // Output the cuts
    for (const auto &cut : res) {
      printf("%d:", obj_id);
      for (size_t idx = 0; idx < cut.size(); ++idx) {
        printf(" %d", cut[idx]);
      }
      printf("\n");
    }
  }
}

int Lsv_CommandPrintCut(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  if (argc != 2){
    Abc_Print(-2, "usage: lsv_printcut k\n");
    Abc_Print(-2, "\t        prints the k-feasible cuts of primary inputs and AND nodes\n");
    // Abc_Print(-2, "\t-h    : print the command usage\n");
    return 1;
  }
  // printf("argc: %d, argv[1]: %s\n", argc, argv[1]);
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  int k = atoi(argv[1]);
  Lsv_NtkPrintCuts(pNtk, k);
  return 0;

// usage:
//   Abc_Print(-2, "usage: lsv_printcut k\n");
//   Abc_Print(-2, "\t        prints the nodes in the network\n");
//   Abc_Print(-2, "\t-h    : print the command usage\n");
//   return 1;
}

// TODO PA2 Q1
std::vector<std::pair<int, int>> Lsv_NtkFindSDC(Abc_Ntk_t* pNtk, int n, int ret = 0) {
  Abc_Obj_t* pNode = Abc_NtkObj(pNtk, n);
  if (!pNode) {
    Abc_Print(-1, "Node not found.\n");
  }
  //extract the cone of yk.
  // 1 (true): Use all CIs from the original network.
  // 0 (false): Use only the necessary CIs required for the logic cone of pNode.
  Abc_Ntk_t* pConeNtk = Abc_NtkCreateCone(pNtk, pNode, "Cone_for_target_node", 0);
  if (!pConeNtk) {
    Abc_Print(-1, "Failed to create cone network.\n");
  }

  // print the cone network
  // Lsv_NtkPrintNodes(pConeNtk);

  // Initialize the simulation manager
    int fLightweight = 0; // Use detailed simulation
    Sim_Man_t* pSimMan = Sim_ManStart(pConeNtk, fLightweight);
    if (!pSimMan) {
        Abc_Print(-1, "Failed to start simulation manager.\n");
        Abc_NtkDelete(pConeNtk);
    }

    // Set up random simulation patterns for the PIs
    int nSimWords = pSimMan->nSimWords;
    Abc_Obj_t* pPi;
    int i;
    Abc_NtkForEachPi(pConeNtk, pPi, i) {
      unsigned* pSimInfo = (unsigned*)pSimMan->vSim0->pArray[pPi->Id];
      Sim_UtilSetRandom(pSimInfo, nSimWords);
    }

    // Perform the simulation
    Sim_UtilSimulate(pSimMan, 0);

    // Retrieve and print simulation results for POs
    // Abc_Obj_t* pObj;
    unsigned* pSimInfo;
    int j, k;
    // Abc_Print(1, "Simulation results for POs:\n");
    // Abc_NtkForEachPo(pConeNtk, pObj, i) {
    //   pSimInfo = (unsigned*)pSimMan->vSim0->pArray[pObj->Id];
    //   Abc_Print(1, "PO %s: \n", Abc_ObjName(pObj));
    //   for (j = 0; j < nSimWords; j++) {
    //     for (k = 31; k >= 0; k--) {
    //       Abc_Print(1, "%d", (pSimInfo[j] >> k) & 1);
    //     }
    //   }
    //   Abc_Print(1, "\n");
    // }

    // Retrieve and print simulation results for fanins of pNode
    std::vector<std::vector<int>> fanin_patterns(nSimWords*32);
    Abc_Obj_t* pFanin;
    // Abc_Print(1, "Simulation results for fanins of node %s:\n", Abc_ObjName(pNode));
    Abc_ObjForEachFanin(pNode, pFanin, i) {
      pSimInfo = (unsigned*)pSimMan->vSim0->pArray[pFanin->Id];
      // Abc_Print(1, "Fanin %s: \n", Abc_ObjName(pFanin));
      for (j = 0; j < nSimWords; j++) {
        for (k = 31; k >= 0; k--) {
          // check the complement
          if (Abc_ObjFaninC(pNode, i)) {
            // Abc_Print(1, "%d", !((pSimInfo[j] >> k) & 1));
            fanin_patterns[j*32+31-k].push_back(!((pSimInfo[j] >> k) & 1));
          } else {
            // Abc_Print(1, "%d", (pSimInfo[j] >> k) & 1);
            fanin_patterns[j*32+31-k].push_back((pSimInfo[j] >> k) & 1);
          }
        }
      }
      // Abc_Print(1, "\n");
    }

    // Abc_NtkForEachPi(pConeNtk, pObj, i) {
    //   pSimInfo = (unsigned*)pSimMan->vSim0->pArray[pObj->Id];
    //   Abc_Print(1, "PI %s: \n", Abc_ObjName(pObj));
    //   for (j = 0; j < nSimWords; j++) {
    //     for (k = 31; k >= 0; k--) {
    //       Abc_Print(1, "%d", (pSimInfo[j] >> k) & 1);
    //     }
    //   }
    //   Abc_Print(1, "\n");
    // }

    // Detect what fanin patterns(00, 01, 10, 11) don't appear in the simulation results of the node
    bool found[4] = {false, false, false, false};
    for (const auto &pattern : fanin_patterns) {
      if (pattern[0] == 0 && pattern[1] == 0) {
        found[0] = true;
      } else if (pattern[0] == 0 && pattern[1] == 1) {
        found[1] = true;
      } else if (pattern[0] == 1 && pattern[1] == 0) {
        found[2] = true;
      } else if (pattern[0] == 1 && pattern[1] == 1) {
        found[3] = true;
      }
      if (found[0] && found[1] && found[2] && found[3]) {
        break;
      }
    }

    // Print the missing patterns
    std::vector<std::pair<int, int>> dontCareSet;
    std::vector<std::vector<int>> missing_patterns;
    if (found[0] && found[1] && found[2] && found[3]) {
      if (ret == 0) {
        Abc_Print(1, "no sdc\n");
      }
    } else {
      if (!found[0]) {
        missing_patterns.push_back({0, 0});
      }
      if (!found[1]) {
        missing_patterns.push_back({0, 1});
      }
      if (!found[2]) {
        missing_patterns.push_back({1, 0});
      }
      if (!found[3]) {
        missing_patterns.push_back({1, 1});
      }
      int flag = 0;
      for (const auto &pattern : missing_patterns) {
        pNode = Abc_NtkObj(pNtk, n);
        // use SAT solver to check the missing patterns
        Abc_Ntk_t* pConeFanin0 = Abc_NtkCreateCone(pNtk, Abc_ObjFanin(pNode, 0), "Cone_for_fanin0", 1);
        Abc_Ntk_t* pConeFanin1 = Abc_NtkCreateCone(pNtk, Abc_ObjFanin(pNode, 1), "Cone_for_fanin1", 1);
        Cnf_Dat_t* pCnf0 = Cnf_Derive(Abc_NtkToDar(pConeFanin0, 0, 0), 1);
        Cnf_Dat_t* pCnf1 = Cnf_Derive(Abc_NtkToDar(pConeFanin1, 0, 0), 1);

        // Cnf_DataPrint(pCnf0, 1);
        
        int VarShift = pCnf0->nVars;
        Cnf_DataLift(pCnf1, VarShift);
        // Cnf_DataPrint(pCnf1, 1);

        sat_solver* pSat = sat_solver_new();
        Cnf_DataWriteIntoSolverInt(pSat, pCnf0, 1, 0);
        Cnf_DataWriteIntoSolverInt(pSat, pCnf1, 1, 0);

        // Map the literals in CNF0 to the corresponding literals in CNF1 in the SAT solver
        Aig_Obj_t * pAigObj;
        int v;
        Aig_ManForEachObj( pCnf0->pMan, pAigObj, v ) {
          if ( pCnf0->pVarNums[pAigObj->Id] >= 0 ) {
            int varA = pCnf0->pVarNums[pAigObj->Id];
            int varB = pCnf1->pVarNums[pAigObj->Id];
            // printf("%d %d %d %d\n", varA, varB, pAigObj->Id, pCnf1->pVarNums[pAigObj->Id]);
            int lits1[2] = { Abc_Var2Lit(varA, 0), Abc_Var2Lit(varB, 1) };
            int lits2[2] = { Abc_Var2Lit(varA, 1), Abc_Var2Lit(varB, 0) };
            sat_solver_addclause(pSat, lits1, lits1 + 2);
            sat_solver_addclause(pSat, lits2, lits2 + 2);
          }
        }

        int VarPo0 = pCnf0->pVarNums[Abc_ObjFaninId(pNode, 0)];
        int VarPo1 = pCnf1->pVarNums[Abc_ObjFaninId(pNode, 1)] + VarShift;
        // printf("%d %d %d %d\n", Abc_ObjFaninId(pNode, 0), Abc_ObjFaninId(pNode, 1), VarPo0, VarPo1);
        // Assume the values of y0 and y1 to be v0 and v1 respectively
        int v0 = pattern[0]; // Example value for y0
        int v1 = pattern[1]; // Example value for y1

        // Create literals based on the assumed values
        int Lit0 = Abc_Var2Lit(VarPo0, (v0 == 0) ^ Abc_ObjFaninC(pNode, 0));
        int Lit1 = Abc_Var2Lit(VarPo1, (v1 == 0) ^ Abc_ObjFaninC(pNode, 1));

        // Add unit clauses to enforce y0 = v0 and y1 = v1
        int res0 = sat_solver_addclause(pSat, &Lit0, &Lit0 + 1);
        int res1 = sat_solver_addclause(pSat, &Lit1, &Lit1 + 1);

        if (res0 == 0 || res1 == 0) {
            printf("Failed to add unit clauses to the SAT solver.\n");
        }

        Sat_SolverWriteDimacs(pSat, "./output_dimacs", NULL, NULL, 0);
        // Solve the CNF formula
        int status = sat_solver_solve(pSat, NULL, NULL, 0, 0, 0, 0);

        if (status == l_False) {
          // UNSAT: (v0, v1) is an SDC of n in terms of y0, y1
          // printf("(%d, %d) is an SDC of n in terms of y0, y1.\n", v0, v1);
          if (ret == 0)
            printf("%d%d\n", v0, v1);
          dontCareSet.push_back({v0, v1});
          flag = 1;
        } else if (status == l_True) {
          // SAT: (v0, v1) is not an SDC
          // printf("(%d, %d) is not an SDC of n in terms of y0, y1.\n", v0, v1);
        } else {
          // Undefined status
          // printf("SAT solver returned undefined status.\n");
        }
      }

      if (flag == 0 && ret == 0) {
        printf("no sdc\n");
      }
    }
    // Clean up
    Sim_Man_t* p = pSimMan;
    if ( p->vSim0 )        Sim_UtilInfoFree( p->vSim0 );       
    if ( p->vSim1 )        Sim_UtilInfoFree( p->vSim1 );       
    if ( p->vSuppStr )     Sim_UtilInfoFree( p->vSuppStr );    
//    if ( p->vSuppFun )     Sim_UtilInfoFree( p->vSuppFun );    
    if ( p->vSuppTargs )   Vec_VecFree( p->vSuppTargs );
    if ( p->pMmPat )       Extra_MmFixedStop( p->pMmPat );
    if ( p->vFifo )        Vec_PtrFree( p->vFifo );
    if ( p->vDiffs )       Vec_IntFree( p->vDiffs );
    ABC_FREE( p );
    Abc_NtkDelete(pConeNtk);
    return dontCareSet;
}

int Lsv_CommandSDC(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  
  if (argc != 2){
    Abc_Print(-2, "usage: lsv_sdc <n>\n");
    Abc_Print(-2, "\t        prints the local SDC for node n\n");
    // Abc_Print(-2, "\t-h    : print the command usage\n");
    return 1;
  }
  // printf("argc: %d, argv[1]: %s\n", argc, argv[1]);
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  int n = atoi(argv[1]);
  Lsv_NtkFindSDC(pNtk, n);
  return 0;

}

// TODO PA2 Q2
void Lsv_NtkFindODC(Abc_Ntk_t* pNtk, int n) {
    // Attempt1: Use Cnf_Derive; however, it optimizes the CNF formula and therefore some variables are missing.
    // Attmept2: Add the block clause to the original network and regerate the CNF formula every time; however, the formula remains the same and I don't know the reason. 
    // Attmept3: Convert the network by myself
    // Cnf_Dat_t* pCnf = Cnf_Derive(Abc_NtkToDar(pNtk, 0, 0), 1);
    // sat_solver* pSat1 = sat_solver_new();
    // Cnf_DataWriteIntoSolverInt(pSat1, pCnf, 1, 0);
    // int s = sat_solver_solve(pSat1, NULL, NULL, 0, 0, 0, 0);
    // if (s == l_False) {
    //   printf("no odc\n");
    // } else if (s == l_True) {
    //   Cnf_DataPrint(pCnf, 1);
    // } else {
    //   printf("SAT solver returned undefined status.\n");
    // }
    // return;

    Abc_Ntk_t* pComplNtk = Abc_NtkDup( pNtk );
    Abc_Obj_t* pComplNode = Abc_NtkObj(pComplNtk, n);
    Abc_Obj_t* pFanout;
    int i;
    Abc_ObjForEachFanout(pComplNode, pFanout, i) {
      Abc_Obj_t* pFanin;
      int k;
      Abc_ObjForEachFanin(pFanout, pFanin, k) {
        if (pFanin == pComplNode) {
          Abc_ObjXorFaninC(pFanout, k);
        }
      }
    }

    Abc_Ntk_t* pMiter = Abc_NtkMiter(pNtk, pComplNtk, 1, 0, 0, 0);
    // Aig_ManDumpBlif(Abc_NtkToDar(pMiter, 0, 0), "../miter.blif", NULL, NULL);

    // Create sat solver
    sat_solver* pSat = sat_solver_new();
    std::map<Abc_Obj_t*, int> ntkObjToVar, ntkPoToVar;
    std::map<int, Abc_Obj_t*> ntkVarToObj;

    // Create the CNF formula for the original network
    Abc_Obj_t* pObj;
    int totalVars = 0;
    Abc_NtkForEachObj(pMiter, pObj, i) {
      ntkObjToVar[pObj] = totalVars;
      ntkVarToObj[totalVars] = pObj;
      totalVars += 1;
    }

    Abc_NtkForEachNode(pMiter, pObj, i) {
      if (Abc_ObjIsCi(pObj))
        continue;

      int compl0 = Abc_ObjFaninC(pObj, 0);
      int compl1 = Abc_ObjFaninC(pObj, 1);
      Abc_Obj_t* fanin0 = Abc_ObjFanin(pObj, 0);
      Abc_Obj_t* fanin1 = Abc_ObjFanin(pObj, 1);
      // printf("Node: %s, %d; Fanin0: %s, %d; Fanin1: %s, %d\n", Abc_ObjName(pObj), Abc_ObjId(pObj), Abc_ObjName(fanin0), Abc_ObjId(fanin0), Abc_ObjName(fanin1), Abc_ObjId(fanin1));
      // ab <-> c, a + c' and b + c' and a' + b' + c
      // Add the original network
      int lits[3];
      lits[0] = toLitCond(ntkObjToVar[pObj], 1);
      lits[1] = toLitCond(ntkObjToVar[fanin1], compl1);
      sat_solver_addclause(pSat, lits, lits + 2);


      lits[0] = toLitCond(ntkObjToVar[fanin0], compl0);
      lits[1] = toLitCond(ntkObjToVar[pObj], 1);
      sat_solver_addclause(pSat, lits, lits + 2);

      lits[0] = toLitCond(ntkObjToVar[fanin0], compl0 ^ 1);
      lits[1] = toLitCond(ntkObjToVar[fanin1], compl1 ^ 1);
      lits[2] = toLitCond(ntkObjToVar[pObj], 0);
      sat_solver_addclause(pSat, lits, lits + 3);
    }
    // print sat size
    // printf("Size: %d\n", pSat->size);
    if (pSat->size == 0) {
      printf("no odc\n");
      return;
    }

    // Add the POs
    // Abc_NtkForEachPo(pMiter, pObj, i) {
    //   Abc_Obj_t* fanin;
    //   Abc_ObjForEachFanin(pObj, fanin, i) {
    //     printf("Po: %s, %d; Fanin: %s, %d\n", Abc_ObjName(pObj), Abc_ObjId(pObj), Abc_ObjName(fanin), Abc_ObjId(fanin));
    //     int compl_i = Abc_ObjFaninC(pObj, i);
    //     int lits[2];
    //     lits[0] = toLitCond(ntkObjToVar[fanin], compl_i);
    //     lits[1] = toLitCond(ntkObjToVar[pObj], 1);
    //     sat_solver_addclause(pSat, lits, lits + 2);

    //     lits[0] = toLitCond(ntkObjToVar[fanin], compl_i ^ 1);
    //     lits[1] = toLitCond(ntkObjToVar[pObj], 0);
    //     sat_solver_addclause(pSat, lits, lits + 2);
    //   }
    // }

    // Po must be 1
    Abc_NtkForEachPo(pMiter, pObj, i) {
      int lits[1];
      Abc_Obj_t* fanin;
      Abc_ObjForEachFanin(pObj, fanin, i) {
        int compl_i = Abc_ObjFaninC(pObj, i);
        lits[0] = toLitCond(ntkObjToVar[fanin], compl_i);
        sat_solver_addclause(pSat, lits, lits + 1);
      }
    }
    // printf("Total vars: %d\n", totalVars);
    // fflush(stdout);
    // Sat_SolverWriteDimacs(pSat, "../output_dimacs1", NULL, NULL, 0);

    // iterate all variables
    // for (const auto &p : ntkObjToVar) {
    //   printf("Var: %d, %s\n", p.second, Abc_ObjName(p.first));
    // }

    // int lits[2];
    // int var0 = ntkObjToVar[Abc_ObjFanin0(Abc_NtkObj(pMiter, n))];
    // int var1 = ntkObjToVar[Abc_ObjFanin1(Abc_NtkObj(pMiter, n))];
    // lits[0] = toLitCond(var0, 0);
    // lits[1] = toLitCond(var1, 1);
    // int ret = sat_solver_addclause(pSat, lits, lits + 2);

    // Solve the CNF formula
    std::vector<std::pair<int, int>> odcPatterns;
    std::vector<std::pair<int, int>> careSet;
    int status, ret_status_flag = 0;
    int counter = 0;
    do {
      status = sat_solver_solve(pSat, NULL, NULL, 0, 0, 0, 0);
      // printf("Status: %d\n", status);
      fflush(stdout);
      if (status == l_False || ret_status_flag == 1) {
        // printf("l_False\n");
        std::vector<std::pair<int, int>> sdcSet = Lsv_NtkFindSDC(pNtk, n, 1);
        // for (const auto& pattern : sdcSet) {
        //   printf("%d%d ", pattern.first, pattern.second);
        // }
        // for (const auto& pattern : careSet) {
        //   printf("%d%d ", pattern.first, pattern.second);
        // }
        // printf("\n");
        int compl0 = Abc_ObjFaninC(Abc_NtkObj(pMiter, n), 0);
        int compl1 = Abc_ObjFaninC(Abc_NtkObj(pMiter, n), 1);
        // printf("compl0: %d, compl1: %d\n", compl0, compl1);
        // printf("fanin0: %d; fanin1: %d\n", Abc_ObjFaninId(Abc_NtkObj(pMiter, n), 0), Abc_ObjFaninId(Abc_NtkObj(pMiter, n), 1));
        for (int i = 0; i < careSet.size(); i++) {
          if (compl0)
            careSet[i].first = careSet[i].first ^ 1;
          if (compl1)
            careSet[i].second = careSet[i].second ^ 1;
        }
        std::vector<std::pair<int, int>> allPatterns = {{0, 0}, {0, 1}, {1, 0}, {1, 1}};
        for (const auto& pattern : allPatterns) {
          if (std::find(sdcSet.begin(), sdcSet.end(), pattern) == sdcSet.end() && std::find(careSet.begin(), careSet.end(), pattern) == careSet.end()) {
            odcPatterns.push_back(pattern);
          }
        }
        break;
      } else if (status == l_True) {
        // printf("l_True\n");
        // fflush(stdout);
        // for (int i = 0; i < pSat->size; ++i) {
        //   if (ntkVarToObj.find(i) != ntkVarToObj.end()) {
        //     printf("Name: %s, %d\n", Abc_ObjName(ntkVarToObj[i]), sat_solver_var_value(pSat, i));
        //   } else {
        //     printf("Var: %d, %d\n", i, sat_solver_var_value(pSat, i));
        //   }
        //   fflush(stdout);
        // }
        // block the assignment
        int var0 = ntkObjToVar[Abc_ObjFanin0(Abc_NtkObj(pMiter, n))];
        int var1 = ntkObjToVar[Abc_ObjFanin1(Abc_NtkObj(pMiter, n))];
        int n0 = sat_solver_var_value(pSat, var0);
        int n1 = sat_solver_var_value(pSat, var1);
        // printf("Block: %d %d, %d %d\n", var0, var1, n0, n1);
        // fflush(stdout);
        // if n0 == 0, then y0 xor n0 -> y0 xor 0 -> !y0*0 + y0*1 -> y0
        // if n0 == 1, then y0 xor n0 -> y0 xor 1 -> !y0*1 + y0*0 -> !y0
        // 0, 1 -> y0 + !y1
        int lits[2];
        lits[0] = toLitCond(var0, n0);
        lits[1] = toLitCond(var1, n1);
        int ret = sat_solver_addclause(pSat, lits, lits + 2);
        // if (counter == 0)
        //   Sat_SolverWriteDimacs(pSat, "../output_dimacs2", NULL, NULL, 0);
        careSet.push_back({n0, n1});
        if (ret ==  0) {
          // printf("Failed to add blocking clause.\n");
          ret_status_flag = 1;
          continue;
        } 
      }
      counter++;
    } while (status == l_True && counter <= 4);

    if (odcPatterns.size() == 0) {
      printf("no odc\n");
    } else {
      for (const auto& pattern : odcPatterns) {
        printf("%d%d ", pattern.first, pattern.second);
      }
      printf("\n");
    }
    
}


int Lsv_CommandODC(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  
  if (argc != 2){
    Abc_Print(-2, "usage: lsv_odc <n>\n");
    Abc_Print(-2, "\t        prints the local ODC for node n\n");
    // Abc_Print(-2, "\t-h    : print the command usage\n");
    return 1;
  }
  // printf("argc: %d, argv[1]: %s\n", argc, argv[1]);
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  int n = atoi(argv[1]);
  Lsv_NtkFindODC(pNtk, n);
  return 0;
}