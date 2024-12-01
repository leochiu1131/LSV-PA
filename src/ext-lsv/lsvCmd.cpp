#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <vector>
#include <set>
#include <iostream>
#include <ostream>
#include <algorithm>
#include <iterator>
#include <random>
#include <unordered_map>
#include <stdio.h>
#include <ctime>
#include <cstdlib>
#include "sat/cnf/cnf.h"
#include "sat/bsat/satSolver.h"

extern "C"{
Aig_Man_t* Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
}

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);

static int Lsv_PrintCut(Abc_Frame_t* pAbc, int argc, char** argv);

static int Lsv_SDC(Abc_Frame_t* pAbc, int argc, char** argv);

static int Lsv_ODC(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_PrintCut, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sdc", Lsv_SDC, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_odc", Lsv_ODC, 0);
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

std::set<int> setUnion(const std::set<int> s1, const std::set<int> s2){
  std::set<int> result = s1;
  result.insert(s2.begin(), s2.end());
  return result;
}

void dominateCheck(std::pair<std::set<int>, bool>& s1, std::pair<std::set<int>, bool>& s2){
  std::set<int> diff1to2, diff2to1;
  std::set_difference(s1.first.begin(), s1.first.end(), s2.first.begin(), s2.first.end(), std::inserter(diff1to2, diff1to2.begin()));
  std::set_difference(s2.first.begin(), s2.first.end(), s1.first.begin(), s1.first.end(), std::inserter(diff2to1, diff2to1.begin()));
  // S1 dominates S2
  if(diff1to2.size()==0 && diff2to1.size()>0){
    s2.second = false;
  }
  // S2 dominates S1
  else if(diff1to2.size()>0 && diff2to1.size()==0){
    s1.second = false;
  }
}

int Lsv_PrintCut(Abc_Frame_t* pAbc, int argc, char** argv){
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

  // PI //
  if(argc==2){
    int k = argv[1][0]-'0';
    Abc_Obj_t *pObj;
    int i;
    std::vector<std::vector<std::set<int>>> eachNodeSet;
    eachNodeSet.resize(2*(Abc_NtkPiNum(pNtk)+Abc_NtkObjNum(pNtk)+Abc_NtkPoNum(pNtk)));

    Abc_NtkForEachPi(pNtk, pObj, i){
      // PIs
      if(Abc_ObjIsPi(pObj)){
        std::vector<std::set<int>> mySet;
        std::set<int> subset = {Abc_ObjId(pObj)};
        mySet.push_back(subset);
        eachNodeSet.at(Abc_ObjId(pObj)) = mySet;
        if(1<=k){
          printf("%d: %d\n", Abc_ObjId(pObj), Abc_ObjId(pObj));
        }
      }
    }

    // AND NODE //
    Abc_NtkForEachNode(pNtk, pObj, i){
      std::vector<std::pair<std::set<int>, bool>> naiveSet;
      std::pair<std::set<int>, bool> tmpPair;
      tmpPair.first = {Abc_ObjId(pObj)};
      tmpPair.second = true;
      naiveSet.push_back(tmpPair);
      // std::cout<<"NODE"<<pObj->Id<<" has "<<Abc_ObjFaninNum(pObj)<<" fanins."<<std::endl;
      Abc_Obj_t *fanin0 = Abc_ObjFanin0(pObj); 
      Abc_Obj_t *fanin1 = Abc_ObjFanin1(pObj);
      std::vector<std::set<int>> f0Set = eachNodeSet.at(fanin0->Id);
      std::vector<std::set<int>> f1Set = eachNodeSet.at(fanin1->Id);

      // First, union two fanins' sets without checking dominance.
      for(const auto& s1: f0Set){
        for(const auto& s2: f1Set){
          std::set<int> unionS = setUnion(s1, s2);
          // Check whether the set has been in naiveSet.
          if(unionS.size()<=k){
            bool rep = false;
            for(const auto& nS: naiveSet){
              if(unionS==nS.first){
                rep = true;
                break;
              }
            }
            if(!rep){
              tmpPair.first = unionS;
              tmpPair.second = true;
              naiveSet.push_back(tmpPair);
            }
          }
        }
      }

      // Domainance checking
      for(size_t i=0; i<naiveSet.size(); ++i){
        if(naiveSet[i].second==true){
          for(size_t j=i; j<naiveSet.size(); ++j){
            if(naiveSet[j].second==true){
              dominateCheck(naiveSet[i], naiveSet[j]);
            }
          }
        }
      }

      // Result
      for(const auto& s: naiveSet){
        if(s.second==true){
          eachNodeSet.at(Abc_ObjId(pObj)).push_back(s.first);
        }
      }

      // Print
      for(const auto& s: eachNodeSet.at(Abc_ObjId(pObj))){
          printf("%d:", Abc_ObjId(pObj));
          for(const auto& mem: s){
            printf(" %d", mem);
          }
          printf("\n");
      }
    }
  }
  else{
    printf("Please give a number k.\n");
  }

  return 0;

usage:
  Abc_Print(-2, "usage: lsv_printcut [-h]\n");
  Abc_Print(-2, "\tk       prints k-feasible cuts in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

void ExportNetworkToDot(Abc_Ntk_t* pConeNtk, const char* dotFileName) {
    if (!pConeNtk) {
        printf("Cone network is NULL.\n");
        return;
    }

    FILE* fp = fopen(dotFileName, "w");
    if (!fp) {
        printf("Failed to open file: %s\n", dotFileName);
        return;
    }

    fprintf(fp, "digraph G {\n"); // 開始 DOT 格式

    Abc_Obj_t* pObj;
    int i;

    // 遍歷所有節點
    Abc_NtkForEachObj(pConeNtk, pObj, i) {
        const char* name = Abc_ObjName(pObj) ? Abc_ObjName(pObj) : "Unnamed";
        int id = Abc_ObjId(pObj);

        // 根據節點類型設置圖形標籤
        if (Abc_ObjIsPi(pObj)) {
            fprintf(fp, "  n%d [label=\"%s\" shape=ellipse];\n", id, name); // PI 節點
        } else if (Abc_ObjIsPo(pObj)) {
            fprintf(fp, "  n%d [label=\"%s\" shape=box];\n", id, name); // PO 節點
        } else if (Abc_ObjIsNode(pObj)) {
            fprintf(fp, "  n%d [label=\"AND\" shape=diamond];\n", id); // AND 節點
        }
    }

    // 遍歷節點的扇入關係
    Abc_NtkForEachObj(pConeNtk, pObj, i) {
        if (Abc_ObjIsNode(pObj) || Abc_ObjIsPo(pObj)) {
            Abc_Obj_t* pFanin;
            int j;
            Abc_ObjForEachFanin(pObj, pFanin, j) {
                fprintf(fp, "  n%d -> n%d;\n", Abc_ObjId(pFanin), Abc_ObjId(pObj));
            }
        }
    }

    fprintf(fp, "}\n"); // 結束 DOT 格式
    fclose(fp);

    printf("DOT file saved to: %s\n", dotFileName);
}

void ExportAigToDot(Aig_Man_t* pAig, const char* dotFileName) {
    if (!pAig) {
        printf("AIG is NULL.\n");
        return;
    }

    FILE* fp = fopen(dotFileName, "w");
    if (!fp) {
        printf("Failed to open file: %s\n", dotFileName);
        return;
    }

    fprintf(fp, "digraph AIG {\n");

    Aig_Obj_t* pObj;
    int i;

    // 遍歷 AIG 節點並添加到 DOT 文件
    Aig_ManForEachObj(pAig, pObj, i) {
        int id = Aig_ObjId(pObj);
        if (Aig_ObjIsCi(pObj)) {
            fprintf(fp, "  n%d [label=\"PI%d\" shape=ellipse];\n", id, id);
        } else if (Aig_ObjIsCo(pObj)) {
            fprintf(fp, "  n%d [label=\"PO%d\" shape=box];\n", id, id);
        } else if (Aig_ObjIsNode(pObj)) {
            fprintf(fp, "  n%d [label=\"AND%d\" shape=diamond];\n", id, id);
        } else if (Aig_ObjIsConst1(pObj)) {
            fprintf(fp, "  n%d [label=\"CONST1\" shape=rectangle];\n", id);
        }
    }

    // 添加邊（連接關係）
    Aig_ManForEachObj(pAig, pObj, i) {
        if (Aig_ObjIsNode(pObj) || Aig_ObjIsCo(pObj)) {
            Aig_Obj_t* pFanin0 = Aig_ObjFanin0(pObj);
            Aig_Obj_t* pFanin1 = Aig_ObjFanin1(pObj);

            if (pFanin0) {
                fprintf(fp, "  n%d -> n%d [label=\"%s\"];\n",
                        Aig_ObjId(pFanin0), Aig_ObjId(pObj),
                        Aig_ObjFaninC0(pObj) ? "inv" : "");
            }

            if (pFanin1) {
                fprintf(fp, "  n%d -> n%d [label=\"%s\"];\n",
                        Aig_ObjId(pFanin1), Aig_ObjId(pObj),
                        Aig_ObjFaninC1(pObj) ? "inv" : "");
            }
        }
    }

    fprintf(fp, "}\n");
    fclose(fp);

    printf("AIG exported to DOT file: %s\n", dotFileName);
}

int * Abc_NtkVerifySimulatePattern_MOD( Abc_Ntk_t * pNtk, int * pModel )
{
    Abc_Obj_t * pNode;
    int * pValues, Value0, Value1, i;
    int fStrashed = 0;
    if ( !Abc_NtkIsStrash(pNtk) )
    {
        pNtk = Abc_NtkStrash(pNtk, 0, 0, 0);
        fStrashed = 1;
    }

    // increment the trav ID
    Abc_NtkIncrementTravId( pNtk );
    // set the CI values
    Abc_AigConst1(pNtk)->pCopy = (Abc_Obj_t *)1;
    Abc_NtkForEachCi( pNtk, pNode, i )
        pNode->pCopy = (Abc_Obj_t *)(ABC_PTRINT_T)pModel[i];
    // simulate in the topological order
    Abc_NtkForEachNode( pNtk, pNode, i )
    {
        Value0 = ((int)(ABC_PTRINT_T)Abc_ObjFanin0(pNode)->pCopy) ^ ((int)Abc_ObjFaninC0(pNode)? 0xFFFFFFFF: 0x0);
        Value1 = ((int)(ABC_PTRINT_T)Abc_ObjFanin1(pNode)->pCopy) ^ ((int)Abc_ObjFaninC1(pNode)? 0xFFFFFFFF: 0x0);
        pNode->pCopy = (Abc_Obj_t *)(ABC_PTRINT_T)(Value0 & Value1);
    }
    // fill the output values
    pValues = ABC_ALLOC( int, Abc_NtkCoNum(pNtk) );
    Abc_NtkForEachCo( pNtk, pNode, i )
        pValues[i] = ((int)(ABC_PTRINT_T)Abc_ObjFanin0(pNode)->pCopy) ^ ((int)Abc_ObjFaninC0(pNode)? 0xFFFFFFFF: 0x0);
    if ( fStrashed )
        Abc_NtkDelete( pNtk );
    return pValues;
}

int Lsv_SDC(Abc_Frame_t* pAbc, int argc, char** argv){
  if(argc<2){
    Abc_Print(-1, "Usage: lsv_sdc <n>\n");
    return 0;
  }

  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  Abc_Obj_t* pNode = Abc_NtkObj(pNtk, atoi(argv[1]));
  
  // Check not PI/PO
  if((!pNode)||Abc_ObjIsPi(pNode)||Abc_ObjIsPo(pNode)){
    Abc_Print(-1, "Node must be an internal node.\n");
    return 0;
  }

  Abc_Obj_t* fanin0 = Abc_ObjFanin0(pNode);
  Abc_Obj_t* fanin1 = Abc_ObjFanin1(pNode);
  int fanin0_inverter = Abc_ObjFaninC0(pNode);
  int fanin1_inverter = Abc_ObjFaninC1(pNode);
  // printf("fanin0_inverter:%d, fanin1_inverter:%d\n", fanin0_inverter, fanin1_inverter);
  if(!fanin0 || !fanin1){
    printf("Missing fanins...\n");
    return 0;
  }
  // Create fanin cone
  Vec_Ptr_t* rootFanins = Vec_PtrAlloc(2);
  Vec_PtrPush(rootFanins, Abc_ObjFanin0(pNode));
  Vec_PtrPush(rootFanins, Abc_ObjFanin1(pNode));
  Abc_Ntk_t* pConeNtk = Abc_NtkCreateConeArray(pNtk, rootFanins, 1); // Include PIs
  if(!pConeNtk) {
    Abc_Print(-1, "Failed to create fanin cone array.\n");
    return 0;
  }
  
  // Turn cone to AIG
  Aig_Man_t* pAig = Abc_NtkToDar(pConeNtk, 0, 0);
  if(!pAig) {
    Abc_Print(-1, "Failed to create AIG.\n");
    return 0;
  }

  int y0ID=0, y1ID=0;
  y0ID = Aig_ObjId(Aig_ManCo(pAig, 0));
  y1ID = Aig_ObjId(Aig_ManCo(pAig, 1));

  sat_solver* pSat = sat_solver_new();
  Cnf_Dat_t* pCnf = Cnf_Derive(pAig, 2);
  std::vector<bool> UNSAT, RANSIMSEEN;
  bool noSDC = true;
  UNSAT.resize(4, false);
  RANSIMSEEN.resize(4, false);

  // RANDOM SIMULATION
  int numPI = Abc_NtkPoNum(pConeNtk);
  int* pattern = new int [numPI];
  srand(time(0));
  for(int p=0; p<numPI; ++p){
      pattern[p] = rand();
  }
  int* simResult = Abc_NtkVerifySimulatePattern_MOD(pConeNtk, pattern);
  for(int b=0; b<32; ++b){
    std::string resultStr = std::to_string(simResult[0] & 1) + std::to_string(simResult[1] & 1);
    if      (resultStr=="00") {RANSIMSEEN[0]=true;} 
    else if (resultStr=="01") {RANSIMSEEN[1]=true;} 
    else if (resultStr=="10") {RANSIMSEEN[2]=true;} 
    else                      {RANSIMSEEN[3]=true;} 
    // printf("RandomSim round %d, Output: %d%d\n", b, (simResult[0] & 1), (simResult[1] & 1));
    simResult[0] = (simResult[0] >> 1);
    simResult[1] = (simResult[1] >> 1);
  }

  Cnf_DataWriteIntoSolverInt(pSat, pCnf, 1, 0); 
  lit Lit[1], Lit2[1];
  if(!RANSIMSEEN[0]){
    Lit[0] = Abc_Var2Lit(pCnf->pVarNums[y0ID], 1);
    Lit2[0] = Abc_Var2Lit(pCnf->pVarNums[y1ID], 1);
    sat_solver_addclause(pSat, Lit, Lit+1);
    sat_solver_addclause(pSat, Lit2, Lit2+1);
    lbool status00 = sat_solver_solve(pSat, nullptr, nullptr, 0,0,0,0);
    if(status00==l_False){
      UNSAT[0] = true;
      noSDC = false;
    }
    sat_solver_restart(pSat);
    Cnf_DataWriteIntoSolverInt(pSat, pCnf, 1, 0); 
  }
  
  if(!RANSIMSEEN[1]){
    Lit[0] = Abc_Var2Lit(pCnf->pVarNums[y0ID], 1);
    Lit2[0] = Abc_Var2Lit(pCnf->pVarNums[y1ID], 0);
    sat_solver_addclause(pSat, Lit, Lit+1);
    sat_solver_addclause(pSat, Lit2, Lit2+1);
    lbool status01 = sat_solver_solve(pSat, nullptr, nullptr, 0,0,0,0);
    if(status01==l_False){
      UNSAT[1] = true;
      noSDC = false;
    }
    sat_solver_restart(pSat);
    Cnf_DataWriteIntoSolverInt(pSat, pCnf, 1, 0); 
  }
  
  if(!RANSIMSEEN[2]){
    Lit[0] = Abc_Var2Lit(pCnf->pVarNums[y0ID], 0);
    Lit2[0] = Abc_Var2Lit(pCnf->pVarNums[y1ID], 1);
    sat_solver_addclause(pSat, Lit, Lit+1);
    sat_solver_addclause(pSat, Lit2, Lit2+1);
    lbool status10 = sat_solver_solve(pSat, nullptr, nullptr, 0,0,0,0);
    if(status10==l_False){
      UNSAT[2] = true;
      noSDC = false;
    }
    sat_solver_restart(pSat);
    Cnf_DataWriteIntoSolverInt(pSat, pCnf, 1, 0); 
  }

  if(!RANSIMSEEN[3]){
    Lit[0] = Abc_Var2Lit(pCnf->pVarNums[y0ID], 0);
    Lit2[0] = Abc_Var2Lit(pCnf->pVarNums[y1ID], 0);
    sat_solver_addclause(pSat, Lit, Lit+1);
    sat_solver_addclause(pSat, Lit2, Lit2+1);
    lbool status11 = sat_solver_solve(pSat, nullptr, nullptr, 0,0,0,0);
    if(status11==l_False){
      UNSAT[3] = true;
      noSDC = false;
    }
  }
  // Delete the solver
  sat_solver_delete(pSat);

  // PRINT THE RESULT
  // std::cout<<"[FINAL RESULT]"<<std::endl;
  std::vector<bool> RESULT_byINV;
  RESULT_byINV.resize(4, false);

  std::string classify;
  if(UNSAT[0]){
    classify = std::to_string(((fanin0_inverter)? 1: 0)) + std::to_string(((fanin1_inverter)? 1: 0));
    if      (classify=="00")  {RESULT_byINV[0]=true;}
    else if (classify=="01")  {RESULT_byINV[1]=true;}
    else if (classify=="10")  {RESULT_byINV[2]=true;}
    else                      {RESULT_byINV[3]=true;}
  }
  if(UNSAT[1]){
    classify = std::to_string(((fanin0_inverter)? 1: 0)) + std::to_string(((fanin1_inverter)? 0: 1));
    if      (classify=="00")  {RESULT_byINV[0]=true;}
    else if (classify=="01")  {RESULT_byINV[1]=true;}
    else if (classify=="10")  {RESULT_byINV[2]=true;}
    else                      {RESULT_byINV[3]=true;}
  }
  if(UNSAT[2]){
    classify = std::to_string(((fanin0_inverter)? 0: 1)) + std::to_string(((fanin1_inverter)? 1: 0));
    if      (classify=="00")  {RESULT_byINV[0]=true;}
    else if (classify=="01")  {RESULT_byINV[1]=true;}
    else if (classify=="10")  {RESULT_byINV[2]=true;}
    else                      {RESULT_byINV[3]=true;}
  }
  if(UNSAT[3]){
    classify = std::to_string(((fanin0_inverter)? 0: 1)) + std::to_string(((fanin1_inverter)? 0: 1));
    if      (classify=="00")  {RESULT_byINV[0]=true;}
    else if (classify=="01")  {RESULT_byINV[1]=true;}
    else if (classify=="10")  {RESULT_byINV[2]=true;}
    else                      {RESULT_byINV[3]=true;}
  }

  ////////////////////////
  // FINAL RESULT PRINT //
  ////////////////////////
  if(RESULT_byINV[0]){printf("00 ");}
  if(RESULT_byINV[1]){printf("01 ");}
  if(RESULT_byINV[2]){printf("10 ");}
  if(RESULT_byINV[3]){printf("11 ");}
  if(noSDC){printf("no sdc");}
  printf("\n");

  return 1;
}

int Lsv_ODC(Abc_Frame_t* pAbc, int argc, char** argv){
  if(argc<2){
    Abc_Print(-1, "Usage: lsv_odc <n>\n");
    return 0;
  }

  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  Abc_Obj_t* pNodeInNtk = Abc_NtkObj(pNtk, atoi(argv[1]));
  Abc_Ntk_t* pNegNtk = Abc_NtkDup(pNtk);
  Abc_Obj_t* pNode = Abc_NtkObj(pNegNtk, atoi(argv[1]));
  Abc_Obj_t* pFanin0 = Abc_ObjFanin0(pNode);
  Abc_Obj_t* pFanin1 = Abc_ObjFanin1(pNode);
  int fanin0_inverter = Abc_ObjFaninC0(pNode);
  int fanin1_inverter = Abc_ObjFaninC1(pNode);
  // printf("pFanin0 Ntk_ID:%d, pFanin1 Ntk_ID:%d\n", Abc_ObjId(pFanin0), Abc_ObjId(pFanin1));
  // printf("fanin0_inverter:%d, fanin1_inverter:%d\n", fanin0_inverter, fanin1_inverter);
  if(!pFanin0 || !pFanin1){
    printf("Missing fanins...\n");
    return 0;
  }

  // Create fanin cone (SDC)
  Vec_Ptr_t* rootFanins = Vec_PtrAlloc(2);
  Vec_PtrPush(rootFanins, Abc_ObjFanin0(pNodeInNtk));
  Vec_PtrPush(rootFanins, Abc_ObjFanin1(pNodeInNtk));
  Abc_Ntk_t* pConeNtk = Abc_NtkCreateConeArray(pNtk, rootFanins, 1); // Include PIs
  if(!pConeNtk) {
    Abc_Print(-1, "Failed to create fanin cone array.\n");
    return 0;
  }

  // SDC part //
  Aig_Man_t* pSDCAig = Abc_NtkToDar(pConeNtk, 0, 0);
  if(!pSDCAig) {
    Abc_Print(-1, "Failed to create AIG.\n");
    return 0;
  }
  int y0ID=0, y1ID=0;
  y0ID = Aig_ObjId(Aig_ManCo(pSDCAig, 0));
  y1ID = Aig_ObjId(Aig_ManCo(pSDCAig, 1));
  sat_solver* pSDCSat = sat_solver_new();
  Cnf_Dat_t* pSDCCnf = Cnf_Derive(pSDCAig, 2);
  std::vector<bool> UNSAT, RANSIMSEEN;
  UNSAT.resize(4, false);
  RANSIMSEEN.resize(4, false);

  // RANDOM SIMULATION
  int numPI = Abc_NtkPoNum(pConeNtk);
  int* pattern = new int [numPI];
  srand(time(0));
  for(int p=0; p<numPI; ++p){
      pattern[p] = rand();
  }
  int* simResult = Abc_NtkVerifySimulatePattern_MOD(pConeNtk, pattern);
  for(int b=0; b<32; ++b){
    std::string resultStr = std::to_string(simResult[0] & 1) + std::to_string(simResult[1] & 1);
    if      (resultStr=="00") {RANSIMSEEN[0]=true;} 
    else if (resultStr=="01") {RANSIMSEEN[1]=true;} 
    else if (resultStr=="10") {RANSIMSEEN[2]=true;} 
    else                      {RANSIMSEEN[3]=true;} 
    // printf("RandomSim round %d, Output: %d%d\n", b, (simResult[0] & 1), (simResult[1] & 1));
    simResult[0] = (simResult[0] >> 1);
    simResult[1] = (simResult[1] >> 1);
  }


  Cnf_DataWriteIntoSolverInt(pSDCSat, pSDCCnf, 1, 0); 
  lit SDCLit[1], SDCLit2[1];
  if(!RANSIMSEEN[0]){  
    SDCLit[0] = Abc_Var2Lit(pSDCCnf->pVarNums[y0ID], 1);
    SDCLit2[0] = Abc_Var2Lit(pSDCCnf->pVarNums[y1ID], 1);
    sat_solver_addclause(pSDCSat, SDCLit, SDCLit+1);
    sat_solver_addclause(pSDCSat, SDCLit2, SDCLit2+1);
    lbool status00 = sat_solver_solve(pSDCSat, nullptr, nullptr, 0,0,0,0);
    if(status00==l_False){
      UNSAT[0] = true;
    }
    sat_solver_restart(pSDCSat);
    Cnf_DataWriteIntoSolverInt(pSDCSat, pSDCCnf, 1, 0); 
  }
  if(!RANSIMSEEN[1]){
    SDCLit[0] = Abc_Var2Lit(pSDCCnf->pVarNums[y0ID], 1);
    SDCLit2[0] = Abc_Var2Lit(pSDCCnf->pVarNums[y1ID], 0);
    sat_solver_addclause(pSDCSat, SDCLit, SDCLit+1);
    sat_solver_addclause(pSDCSat, SDCLit2, SDCLit2+1);
    lbool status01 = sat_solver_solve(pSDCSat, nullptr, nullptr, 0,0,0,0);
    if(status01==l_False){
      UNSAT[1] = true;
    }
    sat_solver_restart(pSDCSat);
    Cnf_DataWriteIntoSolverInt(pSDCSat, pSDCCnf, 1, 0); 
  }
  if(!RANSIMSEEN[2]){
    SDCLit[0] = Abc_Var2Lit(pSDCCnf->pVarNums[y0ID], 0);
    SDCLit2[0] = Abc_Var2Lit(pSDCCnf->pVarNums[y1ID], 1);
    sat_solver_addclause(pSDCSat, SDCLit, SDCLit+1);
    sat_solver_addclause(pSDCSat, SDCLit2, SDCLit2+1);
    lbool status10 = sat_solver_solve(pSDCSat, nullptr, nullptr, 0,0,0,0);
    if(status10==l_False){
      UNSAT[2] = true;
    }
    sat_solver_restart(pSDCSat);
    Cnf_DataWriteIntoSolverInt(pSDCSat, pSDCCnf, 1, 0); 
  }
  if(!RANSIMSEEN[3]){
    SDCLit[0] = Abc_Var2Lit(pSDCCnf->pVarNums[y0ID], 0);
    SDCLit2[0] = Abc_Var2Lit(pSDCCnf->pVarNums[y1ID], 0);
    sat_solver_addclause(pSDCSat, SDCLit, SDCLit+1);
    sat_solver_addclause(pSDCSat, SDCLit2, SDCLit2+1);
    lbool status11 = sat_solver_solve(pSDCSat, nullptr, nullptr, 0,0,0,0);
    if(status11==l_False){
      UNSAT[3] = true;
    }
  }
  // Delete the solver
  sat_solver_delete(pSDCSat);

  // ODC part //
  // Negate 
  Abc_Obj_t* pFanout;
  int i;
  Abc_ObjForEachFanout(pNode, pFanout, i){
    Abc_Obj_t* pFanin;
    int j; 
    Abc_ObjForEachFanin(pFanout, pFanin, j){
      if(pFanin==pNode){
        Abc_ObjXorFaninC(pFanout, j);
      }
    }
  }

  Abc_Ntk_t* pMiterNtk = Abc_NtkMiter(pNtk, pNegNtk, 1, 1, 0, 0);
  if(!pMiterNtk) {
    Abc_Print(-1, "Failed to create Miter.\n");
    return 0;
  }

  // Append pConeNtk to pMiterNtk
  Abc_NtkAppend(pMiterNtk, pConeNtk, 1);

  Aig_Man_t* pAig = Abc_NtkToDar(pMiterNtk, 0, 0);
  if(!pAig) {
    Abc_Print(-1, "Failed to create AIG.\n");
    return 0;
  }
  if (!Aig_ManCheck(pAig)) {
    printf("Error: AIG structure is invalid.\n");
    return 0;
  }

  // Record possible ODC candidators
  std::vector<bool> ODC_incluSDC;
  ODC_incluSDC.resize(4, true);

  sat_solver* pSat = sat_solver_new();
  Cnf_Dat_t* pCnf = Cnf_Derive(pAig, 3);
  // Cnf_DataPrint(pCnf, 1);
  if (pCnf->nVars <= 0 || pCnf->nClauses <= 0) {
    printf("Error: Invalid CNF data.\n");
    return 0;
  }

  Cnf_DataWriteIntoSolverInt(pSat, pCnf, 1, 0); 

  // Set Miter output to 1
  Aig_Obj_t* pPoMiter = Aig_ManCo(pAig, 0);
  Aig_Obj_t* pPoFanin0 = Aig_ManCo(pAig, 1);
  Aig_Obj_t* pPoFanin1 = Aig_ManCo(pAig, 2);

  lit Lit[1];
  Lit[0] = Abc_Var2Lit(pCnf->pVarNums[Aig_ObjId(pPoMiter)], 0);
  
  int counter = 0;
  bool terminate = false;
  if(!sat_solver_addclause(pSat, Lit, Lit+1)){
    terminate = true;
  }

  while((counter<4)&&(!terminate)){
    // printf("========SAT SOLVE %d========\n", counter);
    lbool status = sat_solver_solve(pSat, nullptr, nullptr, 0, 0, 0, 0);
    if(status==l_False){
      // printf("UNSAT\n");
      terminate = true;
    }
    else{
      // printf("SAT\n");
      int valOfFanin0_noInv = sat_solver_var_value(pSat, pCnf->pVarNums[Aig_ObjId(pPoFanin0)]);
      int valOfFanin1_noInv = sat_solver_var_value(pSat, pCnf->pVarNums[Aig_ObjId(pPoFanin1)]);
      // Record pattern
      std::string pattern = std::to_string(valOfFanin0_noInv) + std::to_string(valOfFanin1_noInv);
      if      (pattern=="00") {ODC_incluSDC[0] = false;}
      else if (pattern=="01") {ODC_incluSDC[1] = false;}
      else if (pattern=="10") {ODC_incluSDC[2] = false;}
      else                    {ODC_incluSDC[3] = false;}

      lit Lit2[2];
      Lit2[0] = Abc_Var2Lit(pCnf->pVarNums[Aig_ObjId(pPoFanin0)], valOfFanin0_noInv);
      Lit2[1] = Abc_Var2Lit(pCnf->pVarNums[Aig_ObjId(pPoFanin1)], valOfFanin1_noInv);
      if(!sat_solver_addclause(pSat, Lit2, Lit2+2)){
        terminate = true;
      }

      // printf("valOfFanin0_noInv:%d, valOfFanin1_noInv:%d\n", valOfFanin0_noInv, valOfFanin1_noInv);
      // printf("fanin0_inverter:%d, fanin1_inverter:%d\n", fanin0_inverter, fanin1_inverter);
      counter++;
    }
  }
  sat_solver_delete(pSat);

  // printf("========FINAL RESULT========\n");
  // printf("fanin0_inverter:%d, fanin1_inverter:%d\n", fanin0_inverter, fanin1_inverter);
  bool noODC = true;
  for(int i=0; i<4; ++i){
    ODC_incluSDC[i] = (UNSAT[i])? false: ODC_incluSDC[i];
    noODC = (ODC_incluSDC[i])? false: noODC;
  }
  std::vector<bool> RESULT_byINV;
  RESULT_byINV.resize(4, false);

  std::string classify;
  if(ODC_incluSDC[0]){
    classify = std::to_string(((fanin0_inverter)? 1: 0)) + std::to_string(((fanin1_inverter)? 1: 0));
    if      (classify=="00")  {RESULT_byINV[0]=true;}
    else if (classify=="01")  {RESULT_byINV[1]=true;}
    else if (classify=="10")  {RESULT_byINV[2]=true;}
    else                      {RESULT_byINV[3]=true;}
  }
  if(ODC_incluSDC[1]){
    classify = std::to_string(((fanin0_inverter)? 1: 0)) + std::to_string(((fanin1_inverter)? 0: 1));
    if      (classify=="00")  {RESULT_byINV[0]=true;}
    else if (classify=="01")  {RESULT_byINV[1]=true;}
    else if (classify=="10")  {RESULT_byINV[2]=true;}
    else                      {RESULT_byINV[3]=true;}
  }
  if(ODC_incluSDC[2]){
    classify = std::to_string(((fanin0_inverter)? 0: 1)) + std::to_string(((fanin1_inverter)? 1: 0));
    if      (classify=="00")  {RESULT_byINV[0]=true;}
    else if (classify=="01")  {RESULT_byINV[1]=true;}
    else if (classify=="10")  {RESULT_byINV[2]=true;}
    else                      {RESULT_byINV[3]=true;}
  }
  if(ODC_incluSDC[3]){
    classify = std::to_string(((fanin0_inverter)? 0: 1)) + std::to_string(((fanin1_inverter)? 0: 1));
    if      (classify=="00")  {RESULT_byINV[0]=true;}
    else if (classify=="01")  {RESULT_byINV[1]=true;}
    else if (classify=="10")  {RESULT_byINV[2]=true;}
    else                      {RESULT_byINV[3]=true;}
  }

  ////////////////////////
  // FINAL RESULT PRINT //
  ////////////////////////
  if(RESULT_byINV[0]){printf("00 ");}
  if(RESULT_byINV[1]){printf("01 ");}
  if(RESULT_byINV[2]){printf("10 ");}
  if(RESULT_byINV[3]){printf("11 ");}
  if(noODC){printf("no odc");}
  printf("\n");

  return 1;
}