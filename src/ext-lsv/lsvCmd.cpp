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
void Lsv_NtkFindSDC(Abc_Ntk_t* pNtk, int n) {
  Abc_Obj_t* pNode = Abc_NtkObj(pNtk, n);
  if (!pNode) {
    Abc_Print(-1, "Node not found.\n");
    return;
  }
  //extract the cone of yk.
  // 1 (true): Use all CIs from the original network.
  // 0 (false): Use only the necessary CIs required for the logic cone of pNode.
  Abc_Ntk_t* pConeNtk = Abc_NtkCreateCone(pNtk, pNode, "Cone_for_target_node", 0);
  if (!pConeNtk) {
    Abc_Print(-1, "Failed to create cone network.\n");
    return;
  }

  // print the cone network
  // Lsv_NtkPrintNodes(pConeNtk);

  // Initialize the simulation manager
    int fLightweight = 0; // Use detailed simulation
    Sim_Man_t* pSimMan = Sim_ManStart(pConeNtk, fLightweight);
    if (!pSimMan) {
        Abc_Print(-1, "Failed to start simulation manager.\n");
        Abc_NtkDelete(pConeNtk);
        return;
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
    std::vector<std::vector<int>> missing_patterns;
    if (found[0] && found[1] && found[2] && found[3]) {
      Abc_Print(1, "no sdc\n");
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
          printf("%d%d\n", v0, v1);
          flag = 1;
        } else if (status == l_True) {
          // SAT: (v0, v1) is not an SDC
          // printf("(%d, %d) is not an SDC of n in terms of y0, y1.\n", v0, v1);
        } else {
          // Undefined status
          // printf("SAT solver returned undefined status.\n");
        }
      }

      if (flag == 0) {
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

// 递归函数：计算节点的值
int ComputeNodeValue(Abc_Obj_t* pNode, Cnf_Dat_t* pCnf, sat_solver* pSat) {
    int varNum = pCnf->pVarNums[Abc_ObjId(pNode)];
    printf("Node %d, varNum %d\n", Abc_ObjId(pNode), varNum);
    fflush(stdout);
    if (varNum >= 0) {
        // 节点在 CNF 中有对应的变量，从 SAT 求解器获取值
        return sat_solver_var_value(pSat, varNum);
    } else if (Abc_ObjIsPi(pNode)) {
        // PI 节点应该有变量号
        varNum = pCnf->pVarNums[Abc_ObjId(pNode)];
        if (varNum >= 0) {
            return sat_solver_var_value(pSat, varNum);
        } else {
            printf("Error: PI node with no variable number.\n");
            return -1;
        }
    } else if (Abc_AigNodeIsConst(pNode)) {
        return 1;
    } else if (Abc_ObjIsNode(pNode)) {
        // 内部节点，递归计算其 Fanin 的值
        int value0 = ComputeNodeValue(Abc_ObjFanin0(pNode), pCnf, pSat);
        int value1 = ComputeNodeValue(Abc_ObjFanin1(pNode), pCnf, pSat);

        if (value0 == -1 || value1 == -1) {
            printf("Error: Unable to compute value for node %d.\n", Abc_ObjId(pNode));
            return -1;
        }

        // 处理反向（Complemented）边
        int isCompl0 = Abc_ObjFaninC0(pNode);
        int isCompl1 = Abc_ObjFaninC1(pNode);
        int val0 = isCompl0 ? !value0 : value0;
        int val1 = isCompl1 ? !value1 : value1;

        // AIG 中的节点表示 AND 门
        return val0 & val1;
    } else {
        printf("Error: Unsupported node type (ID %d).\n", Abc_ObjId(pNode));
        return -1;
    }
}

// TODO PA2 Q2
void Lsv_NtkFindODC(Abc_Ntk_t* pNtk, int n) {
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

  Aig_ManDumpBlif(Abc_NtkToDar(pComplNtk, 0, 0), "../compl.blif", NULL, NULL);
  Abc_Ntk_t* pMiter = Abc_NtkMiter(pNtk, pComplNtk, 1, 0, 0, 0);
  Aig_ManDumpBlif(Abc_NtkToDar(pMiter, 0, 0), "../miter.blif", NULL, NULL);

  Abc_Obj_t *pObj, *pObjCopy, *pComplNodeInMiter, *pNodeInMiter;
  Abc_NtkForEachObj(pComplNtk, pObj, i) {
    pObjCopy = pObj->pCopy;
    if (pObjCopy) {
      printf("Node %d in original network corresponds to node %d in miter.\n", Abc_ObjId(pObj), Abc_ObjId(pObjCopy));
      if (Abc_ObjId(pObj) == n) {
        pComplNodeInMiter = pObjCopy;
      }
    }
  }
  Abc_NtkForEachObj(pNtk, pObj, i) {
    pObjCopy = pObj->pCopy;
    if (pObjCopy) {
      printf("Node %d in original network corresponds to node %d in miter.\n", Abc_ObjId(pObj), Abc_ObjId(pObjCopy));
      if (Abc_ObjId(pObj) == n) {
        pNodeInMiter = pObjCopy;
      }
    }
  }
  if (!pNodeInMiter || !pComplNodeInMiter) {
    Abc_Print(-1, "Failed to find the corresponding nodes in the miter.\n");
    return;
  }
  Abc_Print(1, "Node %d in original network corresponds to node %d in miter.\n", n, Abc_ObjId(pNodeInMiter));
  Abc_Print(1, "Node %d in complement network corresponds to node %d in miter.\n", n, Abc_ObjId(pComplNodeInMiter));
  Abc_Print(1, "%s %s %d\n", pNodeInMiter->pNtk->pName, pComplNodeInMiter->pNtk->pName, pNodeInMiter->pNtk->nObjs);
  // Derive CNF for the miter
  Cnf_Dat_t* pCnf = Cnf_Derive(Abc_NtkToDar(pMiter, 0, 0), 1);
  Abc_Obj_t* pNodeInMiterFanin0 = Abc_NtkObj(pMiter, Abc_ObjFaninId(pNodeInMiter, 0)), *pNodeInMiterFanin1 = Abc_NtkObj(pMiter, Abc_ObjFaninId(pNodeInMiter, 1));
  Abc_Obj_t* pNodeInComplMiterFanin0 = Abc_NtkObj(pMiter, Abc_ObjFaninId(pComplNodeInMiter, 0)), *pNodeInComplMiterFanin1 = Abc_NtkObj(pMiter, Abc_ObjFaninId(pComplNodeInMiter, 1));
  Abc_AigForEachAnd(pMiter, pObj, i) {
      if (pCnf->pVarNums[Abc_ObjId(pObj)] == -1) {
          pCnf->pVarNums[Abc_ObjId(pObj)] = pCnf->nVars++;
      }
  }
  
  int status = 1;

  // Create a SAT solver
  sat_solver* pSat = sat_solver_new();
  Cnf_DataWriteIntoSolverInt(pSat, pCnf, 1, 0);
  Cnf_DataPrint(pCnf, 1);
  for (int i = 0; i < pCnf->nVars; i++) {
    printf("Var %d: %d\n", i, pCnf->pVarNums[i]);
  }
  i = 0;
  while (status && i++ < 4) {

    // Solve the CNF formula
    status = sat_solver_solve(pSat, NULL, NULL, 0, 0, 0, 0);

    if (status == l_False) {
      // UNSAT: The rest patterns(all patterns: 00, 01, 10, 11) are ODCs
      break;
    } else if (status == l_True) {
      // SAT: find the satisfying assignment by sat solver var value
      printf("%d %d %d\n", Abc_ObjId(pNodeInMiter), Abc_ObjId(pNodeInMiterFanin0), Abc_ObjId(pNodeInMiterFanin1));
      printf("%d %d\n", pCnf->pVarNums[Abc_ObjId(pNodeInMiterFanin0)], pCnf->pVarNums[Abc_ObjId(pNodeInMiterFanin1)]);
      fflush(stdout);
      int v0, v1, n1, n0;
      v0 = sat_solver_var_value(pSat, pCnf->pVarNums[Abc_ObjFaninId(pNodeInMiter, 0)]);
      v1 = sat_solver_var_value(pSat, pCnf->pVarNums[Abc_ObjFaninId(pNodeInMiter, 1)]);
      n0 = (v0 == 0) ^ Abc_ObjFaninC(pNodeInMiter, 0);
      n1 = (v1 == 0) ^ Abc_ObjFaninC(pNodeInMiter, 1);
      printf("Fanin values: (%d, %d)\n", v0, v1);
      printf("Node value: (%d, %d)\n", n0, n1);
      printf("complement: %d %d\n", Abc_ObjFaninC(pNodeInMiter, 0), Abc_ObjFaninC(pNodeInMiter, 1));
      // find the local assignment from Primary Inputs
      // v0 = ComputeNodeValue(pNodeInMiterFanin0, pCnf, pSat);
      // v1 = ComputeNodeValue(pNodeInMiterFanin1, pCnf, pSat);
      // int cv0 = ComputeNodeValue(pNodeInComplMiterFanin0, pCnf, pSat);
      // int cv1 = ComputeNodeValue(pNodeInComplMiterFanin1, pCnf, pSat);

      // printf("Fanin values: (%d, %d)\n", v0, v1);
      // printf("Fanin values: (%d, %d)\n", cv0, cv1);
      // printf("Node value: %d\n", ComputeNodeValue(pNodeInMiter, pCnf, pSat));
      // assert(v0 == cv0 && v1 == cv1);

      // 输出 PI 的赋值
      printf("PI assignments:\n");
      Abc_Obj_t* pPi;
      int i;
      Abc_NtkForEachPi(pMiter, pPi, i) {
        int varNum = pCnf->pVarNums[Abc_ObjId(pPi)];
        if (varNum >= 0) {
          int value = sat_solver_var_value(pSat, varNum);
          printf("PI %d (ID %d): %d\n", i, Abc_ObjId(pPi), value);
        } else {
          printf("PI %d (ID %d): Variable number not found.\n", i, Abc_ObjId(pPi));
        }
      }

      // lock the assignment (n0, n1) by adding a (y0 xor n0 ∨ y1 xor n1) clause
      // if n0 == 0, then y0 xor n0 -> y0 xor 0 -> !y0*0 + y0*1 -> y0
      // if n0 == 1, then y0 xor n0 -> y0 xor 1 -> !y0*1 + y0*0 -> !y0
      int varNum0 = pCnf->pVarNums[Abc_ObjId(pNodeInMiterFanin0)];
      int varNum1 = pCnf->pVarNums[Abc_ObjId(pNodeInMiterFanin1)];
      printf("Fanin varNums: %d %d\n", varNum0, varNum1);
      lit pLits[2];
      int j = 0;

      if (n0 == 1) {
        // not y0
        pLits[j++] = Abc_Var2Lit(varNum0, 1);
        printf("add not y0\n");
      } else {
        // y0
        pLits[j++] = Abc_Var2Lit(varNum0, 0);
        printf("add y0\n");
      }

      if (n1 == 1) {
        // not y1
        pLits[j++] = Abc_Var2Lit(varNum1, 1);
        printf("add not y1\n");
      } else {
        // y1
        pLits[j++] = Abc_Var2Lit(varNum1, 0);
        printf("add y1\n");
      }

      sat_solver_addclause(pSat, pLits, pLits + j);
      Sat_SolverWriteDimacs(pSat, "./output_dimacs", NULL, NULL, 0);
      // break;
    } else {
      // Undefined status
      Abc_Print(1, "SAT solver returned undefined status.\n");
      break;
    }
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