#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "bdd/cudd/cudd.h"
#include <unordered_map>
#include <fstream>
#include <math.h>
#include <string>
#include <vector>
#include "sat/cnf/cnf.h"
extern "C"{
Aig_Man_t* Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
void Abc_NtkShow( Abc_Ntk_t * pNtk0, int fGateNames, int fSeq, int fUseReverse, int fKeepDot );
}

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimBdd(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSymBdd(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSymSat(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSymAll(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", Lsv_CommandSimBdd, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig", Lsv_CommandSimAig, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_bdd", Lsv_CommandSymBdd, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_sat", Lsv_CommandSymSat, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_all", Lsv_CommandSymAll, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

struct disjointSet{
    std::vector<int> parent, rank;
    std::vector<std::vector<int>> ans;
    disjointSet(int n){
        parent.resize(n);
        rank.resize(n);
        for (int i = 0; i < n; i++){
            parent[i] = i;
            rank[i] = 1;
        }
    }

    int find_set(int v){
        if(parent[v]!=v)
            parent[v] = find_set(parent[v]);
        return parent[v];
    }

    void union_set(int x,int y){
        x = find_set(x);
        y = find_set(y);
        if (rank[x] > rank[y])
            parent[y] = x;
        else{
            parent[x] = y;
            if(rank[x]==rank[y])
                rank[y]++;
        }
    }
    void show(){
      for (int i = 0; i < parent.size(); i++){
        printf("parent[%d]: %d\n", i, parent[i]);
      }
      for (int i = 0; i < parent.size(); i++){
        printf("rank[%d]: %d\n", i, rank[i]);
      }
      for (int i = 0; i < parent.size(); i++){
        printf("find_set[%d]: %d\n", i, find_set(i));
      }
    }
    void finalresult(){
      //test
      // for (int i = 0; i < parent.size(); i++){
      //   printf("%d: %d\n", i, find_set(i));
      // }   
      //endoftest
      ans.resize(parent.size());
      for (int i = 0; i < parent.size(); i++){
        ans[find_set(i)].push_back(i);
      }    
      for (int i = 0; i < parent.size(); i++){
        if (ans[i].size() >= 2){
          for (int j = 0; j < ans[i].size(); j++){
            for (int k = 0; k < ans[i].size(); k++){
              if (ans[i][j] < ans[i][k]) {
                printf("%d %d\n", ans[i][j], ans[i][k]);
              }
            }
          }
        }
      }
    }
};
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
// y3: 2147516416
// 10000000000000001000000000000000
// y2: 1275087872
// 01001100000000000100110000000000
// y1: 1790995136
// 01101010110000000110101011000000
// y0: 2694881440
// 10100000101000001010000010100000

// void decToBinary(unsigned n)
// {
//     // array to store binary number
//     int binaryNum[32];
  
//     // counter for binary array
//     int i = 0;
//     while (n > 0) {
//         // storing remainder in binary array
//         binaryNum[i] = n % 2;
//         n = n / 2;
//         i++;
//     }
  
//     // printing binary array in reverse order
//     for (int j = i - 1; j >= 0; j--)
//         printf("%d", binaryNum[j]);
//     printf("\n");
// }


void Lsv_NtkSimAig(Abc_Ntk_t* pNtk, char* infile) {
  std::ifstream fin;  std::string input;  fin.open(infile);
  
  Abc_Obj_t *pPi, *pPo, *pNode;
  int numPi=0, numNode=0, numPo=0; 
  int iPi, iNode, iPo, iLine=0; 
  std::unordered_map<std::string , unsigned> m;
  std::unordered_map<std::string, std::string> ans;

  Abc_NtkForEachPi(pNtk, pPi, iPi) {
    m[Abc_ObjName(pPi)] = 0;
    numPi++;
  }
  Abc_NtkForEachNode(pNtk, pNode, iNode) {
    m[Abc_ObjName(pNode)] = 0;
    numNode++;
  }
  Abc_NtkForEachPo(pNtk, pPo, iPo) {
    m[Abc_ObjName(pPo)] = 0;
    ans[Abc_ObjName(pPo)] = "";
    numPo++;
  }

  while(std::getline(fin, input)) {
    Abc_NtkForEachPi(pNtk, pPi, iPi) {
      m[Abc_ObjName(pPi)] = m[Abc_ObjName(pPi)] + ((input.c_str()[iPi] == '1') ? unsigned(pow(2, (iLine % 32))) : 0) ;
      numPi++;
    }
    if(iLine % 32 == 31){
      // simulation
      Abc_NtkForEachNode(pNtk, pNode, iNode) {
        if (m[Abc_ObjName(pNode)] == 0){ // not present
          if (!Abc_ObjFaninC0(pNode) && !Abc_ObjFaninC1(pNode)) { // + +
            m[Abc_ObjName(pNode)] = m[Abc_ObjName(Abc_ObjFanin0(pNode))] & m[Abc_ObjName(Abc_ObjFanin1(pNode))];
          }
          else if (!Abc_ObjFaninC0(pNode) && Abc_ObjFaninC1(pNode)) { //  + -
            m[Abc_ObjName(pNode)] = m[Abc_ObjName(Abc_ObjFanin0(pNode))] & ~m[Abc_ObjName(Abc_ObjFanin1(pNode))];
          }
          else if (Abc_ObjFaninC0(pNode) && !Abc_ObjFaninC1(pNode)) { // - +
            m[Abc_ObjName(pNode)] = ~m[Abc_ObjName(Abc_ObjFanin0(pNode))] & m[Abc_ObjName(Abc_ObjFanin1(pNode))];
          }
          else if (Abc_ObjFaninC0(pNode) && Abc_ObjFaninC1(pNode)) { // - -
            m[Abc_ObjName(pNode)] = ~m[Abc_ObjName(Abc_ObjFanin0(pNode))] & ~m[Abc_ObjName(Abc_ObjFanin1(pNode))];
          }
        }
      }
      Abc_NtkForEachPo(pNtk, pNode, iNode) {
        if (m[Abc_ObjName(pNode)] == 0){ // not present
          if (!Abc_ObjFaninC0(pNode)) { // +
            m[Abc_ObjName(pNode)] = m[Abc_ObjName(Abc_ObjFanin0(pNode))];
          }
          else{ // -
            m[Abc_ObjName(pNode)] = ~m[Abc_ObjName(Abc_ObjFanin0(pNode))];
          }
        }

        for (int i = 0; i < 32; i++) {
          if(m[Abc_ObjName(pNode)] > 0) {
            if(m[Abc_ObjName(pNode)] % 2 == 1){
              ans[Abc_ObjName(pNode)] = ans[Abc_ObjName(pNode)] + "1";
            }
            else{
              ans[Abc_ObjName(pNode)] = ans[Abc_ObjName(pNode)] + "0";
            }
            m[Abc_ObjName(pNode)] = m[Abc_ObjName(pNode)] / 2;
          }
          else{
            ans[Abc_ObjName(pNode)] = ans[Abc_ObjName(pNode)] + "0";
          }
        }
      }

      // after 32 bits simulation, clear
      Abc_NtkForEachPi(pNtk, pPi, iPi) {
        m[Abc_ObjName(pPi)] = 0;
      }
      Abc_NtkForEachNode(pNtk, pNode, iNode) {
        m[Abc_ObjName(pNode)] = 0;
      }
      Abc_NtkForEachPo(pNtk, pPo, iPo) {
        m[Abc_ObjName(pPo)] = 0;
      }
    }

    iLine++;
  }

  if(iLine % 32 != 0){ // some of them not simulated yet
      Abc_NtkForEachNode(pNtk, pNode, iNode) {
        if (m[Abc_ObjName(pNode)] == 0){ // not present
          if (!Abc_ObjFaninC0(pNode) && !Abc_ObjFaninC1(pNode)) { // + +
            m[Abc_ObjName(pNode)] = m[Abc_ObjName(Abc_ObjFanin0(pNode))] & m[Abc_ObjName(Abc_ObjFanin1(pNode))];
          }
          else if (!Abc_ObjFaninC0(pNode) && Abc_ObjFaninC1(pNode)) { //  + -
            m[Abc_ObjName(pNode)] = m[Abc_ObjName(Abc_ObjFanin0(pNode))] & ~m[Abc_ObjName(Abc_ObjFanin1(pNode))];
          }
          else if (Abc_ObjFaninC0(pNode) && !Abc_ObjFaninC1(pNode)) { // - +
            m[Abc_ObjName(pNode)] = ~m[Abc_ObjName(Abc_ObjFanin0(pNode))] & m[Abc_ObjName(Abc_ObjFanin1(pNode))];
          }
          else if (Abc_ObjFaninC0(pNode) && Abc_ObjFaninC1(pNode)) { // - -
            m[Abc_ObjName(pNode)] = ~m[Abc_ObjName(Abc_ObjFanin0(pNode))] & ~m[Abc_ObjName(Abc_ObjFanin1(pNode))];
          }
        }
      }
      Abc_NtkForEachPo(pNtk, pNode, iNode) {
        if (m[Abc_ObjName(pNode)] == 0){ // not present
          if (!Abc_ObjFaninC0(pNode)) { // +
            m[Abc_ObjName(pNode)] = m[Abc_ObjName(Abc_ObjFanin0(pNode))];
          }
          else{ // -
            m[Abc_ObjName(pNode)] = ~m[Abc_ObjName(Abc_ObjFanin0(pNode))];
          }
        }
      
        for (int i = 0; i < (iLine % 32); i++) {
          if(m[Abc_ObjName(pNode)] > 0) {
            if(m[Abc_ObjName(pNode)] % 2 == 1){
              ans[Abc_ObjName(pNode)] = ans[Abc_ObjName(pNode)] + "1";
            }
            else{
              ans[Abc_ObjName(pNode)] = ans[Abc_ObjName(pNode)] + "0";
            }
            m[Abc_ObjName(pNode)] = m[Abc_ObjName(pNode)] / 2;
          }
          else{
            ans[Abc_ObjName(pNode)] = ans[Abc_ObjName(pNode)] + "0";
          }
        }
      }
  }

  Abc_NtkForEachPo(pNtk, pNode, iNode) {
    printf("%s: %s\n", Abc_ObjName(pNode), ans[Abc_ObjName(pNode)].c_str());
  }
}

void Lsv_NtkSimBdd(Abc_Ntk_t* pNtk, char* sim){
  Abc_Obj_t* pPi; Abc_Obj_t* pPo;
  int ithPi; int ithPo;

  Abc_NtkForEachPo(pNtk, pPo, ithPo) {
      Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo); 
      assert( Abc_NtkIsBddLogic(pRoot->pNtk) );
      DdManager * dd = (DdManager *)pRoot->pNtk->pManFunc;  
      DdNode* ddnode = (DdNode *)pRoot->pData;

      Abc_NtkForEachPi(pNtk, pPi, ithPi) {
        int id = Vec_IntFind( &pRoot->vFanins, pPi->Id );

        if(id != -1){
          if(sim[ithPi] == '0'){
            ddnode = Cudd_Cofactor(dd, ddnode,Cudd_Not(Cudd_bddIthVar( dd, id))); 
          }
          else{
            ddnode = Cudd_Cofactor(dd, ddnode,Cudd_bddIthVar( dd, id)); 
          }
        }

      }
      if(ddnode == dd->one){
        printf("%s: 1\n", Abc_ObjName(pPo));
      }
      else{
        printf("%s: 0\n", Abc_ObjName(pPo));
      }

  }
}

void Lsv_NtkSymBdd(Abc_Ntk_t* pNtk, int k, int i, int j){
  // printf("%d, %d, %d\n", k, i, j);
  Abc_Obj_t* pPo   = Abc_NtkPo(pNtk, k);
  Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo); 
  assert( Abc_NtkIsBddLogic(pRoot->pNtk) );
  DdManager * dd = (DdManager *)pRoot->pNtk->pManFunc;  
  DdNode* ddnode = (DdNode *)pRoot->pData;
  DdNode * cof01 = ddnode; Cudd_Ref(cof01);
  DdNode * cof10 = ddnode; Cudd_Ref(cof10);
  // printf("263\n");
  // char** vNamesIn = (char**) Abc_NodeGetFaninNames(pRoot)->pArray;
  // for ( int q = 0; q < Abc_NtkPiNum(pNtk); q++) {
  //   printf("%d: %s\n", q, vNamesIn[q]);
  // }
  int id_i = Vec_IntFind( &pRoot->vFanins, i+1);
  int id_j = Vec_IntFind( &pRoot->vFanins, j+1);
  if (id_i != -1) {
    cof01 = Cudd_Cofactor(dd, cof01, Cudd_Not(Cudd_bddIthVar( dd, id_i)));
    cof10 = Cudd_Cofactor(dd, cof10, Cudd_bddIthVar( dd, id_i));
  }
  if (id_j != -1) {
    cof01 = Cudd_Cofactor(dd, cof01, Cudd_bddIthVar( dd, id_j));
    cof10 = Cudd_Cofactor(dd, cof10, Cudd_Not(Cudd_bddIthVar( dd, id_j)));
  }

  // printf("268\n");

  if (cof01 == cof10) {
    printf("symmetric\n");
  }
  else {
    printf("asymmetric\n");
    std::string s01 = "";
    std::string s10 = "";
    for ( int q = 0; q < Abc_NtkPiNum(pNtk); q++) {
      int id_q = Vec_IntFind( &pRoot->vFanins, q+1);
      if (q == i) {
        s01 += "0";
        s10 += "1";
      }
      else if (q == j) {
        s01 += "1";
        s10 += "0";
      }
      else {
        if (id_q != -1) {
          DdNode * tmp01;
          DdNode * tmp10;
          tmp01 = Cudd_Cofactor(dd, cof01, Cudd_Not(Cudd_bddIthVar( dd, id_q))); Cudd_Ref(tmp01);
          tmp10 = Cudd_Cofactor(dd, cof10, Cudd_Not(Cudd_bddIthVar( dd, id_q))); Cudd_Ref(tmp10);
          if (tmp01 == tmp10) {
            cof01 = Cudd_Cofactor(dd, cof01, Cudd_bddIthVar( dd, id_q));
            cof10 = Cudd_Cofactor(dd, cof10, Cudd_bddIthVar( dd, id_q));
            s01 += "1";
            s10 += "1";
            assert(cof01 != cof10);
          }
          else {
            cof01 = tmp01;
            cof10 = tmp10;
            s01 += "0";
            s10 += "0";
          }

          Cudd_RecursiveDeref( dd, tmp01 );
          Cudd_RecursiveDeref( dd, tmp10 );
        }
        else {
          s01 += "0";
          s10 += "0";
        }
      }
    }
    printf("%s\n", s01.c_str());
    printf("%s\n", s10.c_str());
  }

  Cudd_RecursiveDeref( dd, cof01 );
  Cudd_RecursiveDeref( dd, cof10 );

}

void Lsv_NtkSymSat(Abc_Ntk_t* pNtk, int k, int i, int j){
  Abc_Obj_t* pPo = Abc_NtkCo(pNtk, k);
  // printf("%d\n", Abc_ObjIsNode(Abc_ObjFanin0(pPo)));
  // printf("%d\n", Abc_NtkIsStrash(pNtk));
  // printf("%d\n", Abc_AigNodeIsConst(Abc_ObjFanin0(pPo)));
  // printf("%d\n\n", Abc_ObjIsCi(Abc_ObjFanin0(pPo)));
  
  // printf("%d\n", Abc_ObjFaninC0(pPo));
  // printf("%d\n", Abc_ObjFaninC0(Abc_ObjFanin0(pPo)));
  // printf("%s\n", Abc_ObjName(pPo));
  // printf("%s\n", Abc_ObjName(Abc_ObjFanin0(pPo)));
  

  Abc_Ntk_t* pCone = Abc_NtkCreateCone(pNtk, Abc_ObjFanin0(pPo), Abc_ObjName(pPo), 1);
  Aig_Man_t* pAig = Abc_NtkToDar(pCone, 0, 1);
  sat_solver * pSat;
  pSat = sat_solver_new();
  Cnf_Dat_t * pCnf, * pCnf2;
  pCnf = Cnf_Derive(pAig, 1);
  pSat = (sat_solver *)Cnf_DataWriteIntoSolverInt( pSat, pCnf, 1, 0 );
  
  int idx;
  Abc_Obj_t* pObj;
  // printf("pCnf\n");
  // Abc_NtkForEachNode(pCone, pObj, idx){
  //   printf("%s %d %d\n",Abc_ObjName(pObj), pObj->Id, pCnf->pVarNums[pObj->Id]);
  // }
  // Abc_NtkForEachPi(pCone, pObj, idx){
  //   printf("%s %d %d\n",Abc_ObjName(pObj), pObj->Id, pCnf->pVarNums[pObj->Id]);
  // }
  // Abc_NtkForEachPo(pCone, pObj, idx){
  //   printf("%s %d %d\n",Abc_ObjName(pObj), pObj->Id, pCnf->pVarNums[pObj->Id]);
  // }
  // printf("\npCnf clause\n");
  // Cnf_DataPrint(pCnf, 1);
 // test
  // int status = sat_solver_solve(pSat, NULL, NULL, 0, 0, 0, 0);
  // if (status == l_False) {
  //   printf("symmetric\n");
  // }
  // else if (status == l_True) {
  //   printf("asymmetric\n");
  //   for (int v = 0; v < pSat->size; v++) {
  //     printf("v: %d, %d\n", v, sat_solver_var_value(pSat, v));
  //   }
  // }

 // end of test



  pCnf2 = Cnf_Derive(pAig, 1);
  // pCnf2 = Cnf_DataDup(pCnf);
  int diff = sat_solver_nvars(pSat);
  Cnf_DataLift( pCnf2, diff);
  pSat = (sat_solver *)Cnf_DataWriteIntoSolverInt( pSat, pCnf2, 1, 0 );

  // int * clause = ABC_ALLOC(int, 2);
  int clause[2];
  int val = 0;
  Abc_NtkForEachPi(pCone, pObj, idx){
    // printf("%s %d %d %d\n",Abc_ObjName(pObj), idx, pObj->Id, pCnf2->pVarNums[pObj->Id]);
    // printf("idx: %d\n", idx);
    if (idx != i && idx != j) {
      // int * clause = ABC_ALLOC(int, 2);
      // printf("same %d %d\n", pCnf->pVarNums[pObj->Id], pCnf2->pVarNums[pObj->Id]);
      clause[0] = toLitCond(pCnf->pVarNums[pObj->Id], 0); 
      clause[1] = toLitCond(pCnf2->pVarNums[pObj->Id], 1);
      val = sat_solver_addclause(pSat, clause, clause + 2);
      assert(val);

      clause[0] = toLitCond(pCnf->pVarNums[pObj->Id], 1); 
      clause[1] = toLitCond(pCnf2->pVarNums[pObj->Id], 0);
      val = sat_solver_addclause(pSat, clause, clause + 2);
      assert(val);
    }
  }
  // printf("Abc_NtkPi(pCone, i)->Id: %d \n", Abc_NtkPi(pCone, i)->Id);
  // printf("Abc_NtkPi(pCone, j)->Id: %d \n", Abc_NtkPi(pCone, j)->Id);
  // printf("pCnf  i: %d\n", pCnf->pVarNums[Abc_NtkPi(pCone, i)->Id]);
  // printf("pCnf2 i: %d\n", pCnf2->pVarNums[Abc_NtkPi(pCone, i)->Id]);
  // printf("pCnf  j: %d\n", pCnf->pVarNums[Abc_NtkPi(pCone, j)->Id]);
  // printf("pCnf2 j: %d\n\n", pCnf2->pVarNums[Abc_NtkPi(pCone, j)->Id]);
  clause[0] = toLitCond(pCnf->pVarNums[Abc_NtkPi(pCone, i)->Id], 0); 
  clause[1] = toLitCond(pCnf2->pVarNums[Abc_NtkPi(pCone, j)->Id], 1);
  val = sat_solver_addclause(pSat, clause, clause + 2);
  assert(val);

  clause[0] = toLitCond(pCnf->pVarNums[Abc_NtkPi(pCone, i)->Id], 1); 
  clause[1] = toLitCond(pCnf2->pVarNums[Abc_NtkPi(pCone, j)->Id], 0);
  val = sat_solver_addclause(pSat, clause, clause + 2);
  assert(val);

  clause[0] = toLitCond(pCnf->pVarNums[Abc_NtkPi(pCone, j)->Id], 0); 
  clause[1] = toLitCond(pCnf2->pVarNums[Abc_NtkPi(pCone, i)->Id], 1);
  val = sat_solver_addclause(pSat, clause, clause + 2);
  assert(val);

  clause[0] = toLitCond(pCnf->pVarNums[Abc_NtkPi(pCone, j)->Id], 1); 
  clause[1] = toLitCond(pCnf2->pVarNums[Abc_NtkPi(pCone, i)->Id], 0);
  val = sat_solver_addclause(pSat, clause, clause + 2);
  assert(val);

  // vA(yk) xor vB(yk)   // yk is represented by n??, which connect to yk
  // printf("vA(yk): %d\n", pCnf->pVarNums[Abc_ObjFanin0(Abc_NtkCo(pCone, 0))->Id]);
  // printf("vB(yk): %d\n\n", pCnf2->pVarNums[Abc_ObjFanin0(Abc_NtkCo(pCone, 0))->Id]);
  clause[0] = toLitCond(pCnf->pVarNums[Abc_ObjFanin0(Abc_NtkCo(pCone, 0))->Id], 0); 
  clause[1] = toLitCond(pCnf2->pVarNums[Abc_ObjFanin0(Abc_NtkCo(pCone, 0))->Id], 0);
  val = sat_solver_addclause(pSat, clause, clause + 2);
  assert(val);

  clause[0] = toLitCond(pCnf->pVarNums[Abc_ObjFanin0(Abc_NtkCo(pCone, 0))->Id], 1); 
  clause[1] = toLitCond(pCnf2->pVarNums[Abc_ObjFanin0(Abc_NtkCo(pCone, 0))->Id], 1);
  val = sat_solver_addclause(pSat, clause, clause + 2);
  assert(val);

  int status = sat_solver_solve(pSat, NULL, NULL, 0, 0, 0, 0);
  if (status == l_False) {
    printf("symmetric\n");
  }
  else if (status == l_True) {
    printf("asymmetric\n");
    // for (int v = 0; v < pSat->size; v++) {
    //   printf("v: %d, %d\n", v, sat_solver_var_value(pSat, v));
    // }
    Abc_NtkForEachPi(pCone, pObj, idx){
      printf("%d", sat_solver_var_value(pSat, pCnf->pVarNums[pObj->Id]));
    }
    printf("\n");
    Abc_NtkForEachPi(pCone, pObj, idx){
      printf("%d", sat_solver_var_value(pSat, pCnf2->pVarNums[pObj->Id]));
    }
    printf("\n");

  }
  // printf("%d\n", pCnf->pVarNums[Abc_ObjFanin0(Abc_NtkCo(pCone, 0))->Id]);
  // printf("%d\n", pCnf2->pVarNums[Abc_ObjFanin0(Abc_NtkCo(pCone, 0))->Id]);

  // printf("pCnf\n");
  // Abc_NtkForEachNode(pCone, pObj, idx){
  //   printf("%s %d %d\n",Abc_ObjName(pObj), pObj->Id, pCnf->pVarNums[pObj->Id]);
  // }
  // Abc_NtkForEachPi(pCone, pObj, idx){
  //   printf("%s %d %d\n",Abc_ObjName(pObj), pObj->Id, pCnf->pVarNums[pObj->Id]);
  // }
  // Abc_NtkForEachPo(pCone, pObj, idx){
  //   printf("%s %d %d\n",Abc_ObjName(pObj), pObj->Id, pCnf->pVarNums[pObj->Id]);
  // }

  // printf("\npCnf2\n");
  // Abc_NtkForEachNode(pCone, pObj, idx){
  //   printf("%s %d %d\n",Abc_ObjName(pObj), pObj->Id, pCnf2->pVarNums[pObj->Id]);
  // }
  // Abc_NtkForEachPi(pCone, pObj, idx){
  //   printf("%s %d %d\n",Abc_ObjName(pObj), pObj->Id, pCnf2->pVarNums[pObj->Id]);
  // }
  // Abc_NtkForEachPo(pCone, pObj, idx){
  //   printf("%s %d %d\n",Abc_ObjName(pObj), pObj->Id, pCnf2->pVarNums[pObj->Id]);
  // }

  // printf("\npCnf clause\n");
  // Cnf_DataPrint(pCnf, 1);
  // printf("pCnf2 clause\n");
  // Cnf_DataPrint(pCnf2, 1);
  // printf("%d\n", Abc_ObjFaninC0(Abc_NtkCo(pCone,0)));
  // printf("%d\n", Abc_ObjFaninC0(Abc_ObjFanin0(Abc_NtkCo(pCone,0))));
  
  // Aig_ManShow(pAig, 0, NULL);
  // Abc_NtkShow(pNtk, 1, 1, 1, 1);
  // Abc_NtkShow(pCone, 1, 1, 1, 1);

}

void Lsv_NtkSymAll(Abc_Ntk_t* pNtk, int k){
  std::vector<int> control; // control varable
  Abc_Obj_t* pPo = Abc_NtkCo(pNtk, k);
  Abc_Ntk_t* pCone = Abc_NtkCreateCone(pNtk, Abc_ObjFanin0(pPo), Abc_ObjName(pPo), 1);
  Aig_Man_t* pAig = Abc_NtkToDar(pCone, 0, 1);
  sat_solver * pSat;
  pSat = sat_solver_new();
  Cnf_Dat_t * pCnf, * pCnf2;
  pCnf = Cnf_Derive(pAig, 1);
  pSat = (sat_solver *)Cnf_DataWriteIntoSolverInt( pSat, pCnf, 1, 0 );
  
  int idx;
  Abc_Obj_t* pObj;

  pCnf2 = Cnf_Derive(pAig, 1);
  int diff = sat_solver_nvars(pSat);
  Cnf_DataLift( pCnf2, diff);
  pSat = (sat_solver *)Cnf_DataWriteIntoSolverInt( pSat, pCnf2, 1, 0 );

  // Cnf_DataPrint(pCnf, 1);
  // Cnf_DataPrint(pCnf2, 1);


  int clause2[2];
  int clause3[3];
  int clause4[4];
  int val = 0;
  disjointSet S(Abc_NtkPiNum(pCone));

  Abc_NtkForEachPi(pCone, pObj, idx){
      int nVar = sat_solver_addvar(pSat);
      control.push_back(nVar);
      // printf("idx: %d, nVar: %d\n", idx, nVar);

      // constraint (b)
      clause3[0] = toLitCond(pCnf->pVarNums[pObj->Id], 0); 
      clause3[1] = toLitCond(pCnf2->pVarNums[pObj->Id], 1);
      clause3[2] = toLitCond( nVar, 0);
      val = sat_solver_addclause(pSat, clause3, clause3 + 3);
      assert(val);

      clause3[0] = toLitCond(pCnf->pVarNums[pObj->Id], 1); 
      clause3[1] = toLitCond(pCnf2->pVarNums[pObj->Id], 0);
      clause3[2] = toLitCond( nVar, 0);
      val = sat_solver_addclause(pSat, clause3, clause3 + 3);
      assert(val);
  }

  // for (int i = 0; i < control.size(); i++) printf("control[%d]: %d\n", i, control[i]);
  
  for (int i = 0; i < Abc_NtkPiNum(pCone); i++) {
    for (int j = 0; j < Abc_NtkPiNum(pCone); j++) {
      if (i < j){
        // constraint (c)
        clause4[0] = toLitCond(pCnf->pVarNums[Abc_NtkPi(pCone, i)->Id], 0); 
        clause4[1] = toLitCond(pCnf2->pVarNums[Abc_NtkPi(pCone, j)->Id], 1);
        clause4[2] = toLitCond(control[i], 1);
        clause4[3] = toLitCond(control[j], 1);
        val = sat_solver_addclause(pSat, clause4, clause4 + 4);
        assert(val);

        clause4[0] = toLitCond(pCnf->pVarNums[Abc_NtkPi(pCone, i)->Id], 1); 
        clause4[1] = toLitCond(pCnf2->pVarNums[Abc_NtkPi(pCone, j)->Id], 0);
        clause4[2] = toLitCond(control[i], 1);
        clause4[3] = toLitCond(control[j], 1);
        val = sat_solver_addclause(pSat, clause4, clause4 + 4);
        assert(val);

        // constraint (d)
        clause4[0] = toLitCond(pCnf->pVarNums[Abc_NtkPi(pCone, j)->Id], 0); 
        clause4[1] = toLitCond(pCnf2->pVarNums[Abc_NtkPi(pCone, i)->Id], 1);
        clause4[2] = toLitCond(control[i], 1);
        clause4[3] = toLitCond(control[j], 1);
        val = sat_solver_addclause(pSat, clause4, clause4 + 4);
        assert(val);

        clause4[0] = toLitCond(pCnf->pVarNums[Abc_NtkPi(pCone, j)->Id], 1); 
        clause4[1] = toLitCond(pCnf2->pVarNums[Abc_NtkPi(pCone, i)->Id], 0);
        clause4[2] = toLitCond(control[i], 1);
        clause4[3] = toLitCond(control[j], 1);
        val = sat_solver_addclause(pSat, clause4, clause4 + 4);
        assert(val);
      }
    }
  }
  // constraint (a)
  clause2[0] = toLitCond(pCnf->pVarNums[Abc_ObjFanin0(Abc_NtkCo(pCone, 0))->Id], 0); 
  clause2[1] = toLitCond(pCnf2->pVarNums[Abc_ObjFanin0(Abc_NtkCo(pCone, 0))->Id], 0);
  val = sat_solver_addclause(pSat, clause2, clause2 + 2);
  assert(val);

  clause2[0] = toLitCond(pCnf->pVarNums[Abc_ObjFanin0(Abc_NtkCo(pCone, 0))->Id], 1); 
  clause2[1] = toLitCond(pCnf2->pVarNums[Abc_ObjFanin0(Abc_NtkCo(pCone, 0))->Id], 1);
  val = sat_solver_addclause(pSat, clause2, clause2 + 2);
  assert(val);

  //test
  // int test = sat_solver_solve(pSat, NULL, NULL, 0, 0, 0, 0);
  // if (test == l_True){
  //   printf("LT\n");
  //   for (int v = 0; v < pSat->size; v++) {
  //     printf("v: %d, %d\n", v, sat_solver_var_value(pSat, v));
  //   }

  // }
  // else if (test == l_False) printf("LF\n");
  // else printf("else\n");
  //   printf("pCnf\n");
  // Abc_NtkForEachNode(pCone, pObj, idx){
  //   printf("%s %d %d\n",Abc_ObjName(pObj), pObj->Id, pCnf->pVarNums[pObj->Id]);
  // }
  // Abc_NtkForEachPi(pCone, pObj, idx){
  //   printf("%s %d %d\n",Abc_ObjName(pObj), pObj->Id, pCnf->pVarNums[pObj->Id]);
  // }
  // Abc_NtkForEachPo(pCone, pObj, idx){
  //   printf("%s %d %d\n",Abc_ObjName(pObj), pObj->Id, pCnf->pVarNums[pObj->Id]);
  // }

  // printf("\npCnf2\n");
  // Abc_NtkForEachNode(pCone, pObj, idx){
  //   printf("%s %d %d\n",Abc_ObjName(pObj), pObj->Id, pCnf2->pVarNums[pObj->Id]);
  // }
  // Abc_NtkForEachPi(pCone, pObj, idx){
  //   printf("%s %d %d\n",Abc_ObjName(pObj), pObj->Id, pCnf2->pVarNums[pObj->Id]);
  // }
  // Abc_NtkForEachPo(pCone, pObj, idx){
  //   printf("%s %d %d\n",Abc_ObjName(pObj), pObj->Id, pCnf2->pVarNums[pObj->Id]);
  // }
  //endoftest

  int* assump = ABC_ALLOC(int, Abc_NtkPiNum(pCone));
  // printf("Abc_NtkPiNum(pCone) %d\n", Abc_NtkPiNum(pCone));
  for (int i = 0; i < Abc_NtkPiNum(pCone); i++) {
    assump[i] = toLitCond(control[i], 1);
  }
  for (int i = 0; i < Abc_NtkPiNum(pCone); i++) {
    for (int j = 0; j < Abc_NtkPiNum(pCone); j++) {
      if (i < j){
        if (S.find_set(i) != S.find_set(j)){
          assump[i] = toLitCond(control[i], 0);
          assump[j] = toLitCond(control[j], 0);
          // for (int q = 0; q < Abc_NtkPiNum(pCone); q++){
          //   // printf("assump[%d]: %d\n", q, assump[q]);
          //   sat_solver_push(pSat, assump[q]);
          // }
          int status = sat_solver_solve(pSat, assump, assump + Abc_NtkPiNum(pCone), 0, 0, 0, 0);
          // int status = sat_solver_solve(pSat, NULL, NULL, 0, 0, 0, 0);
          // int status = sat_solver_solve_internal(pSat);
          if (status == l_False) {
            // printf("symm: i j: %d %d\n", i, j);
            S.union_set(i, j);
            // sat_solver_final()
          }
          assump[i] = toLitCond(control[i], 1);
          assump[j] = toLitCond(control[j], 1);

          // for (int q = 0; q < Abc_NtkPiNum(pCone); q++){
          //   sat_solver_pop(pSat);
          // }
        }
      }
    }
  }
  // S.show();
  S.finalresult();
  ABC_FREE(assump);
  // int status = sat_solver_solve(pSat, NULL, NULL, 0, 0, 0, 0);
  // if (status == l_False) {
  //   printf("symmetric\n");
  // }
  // else if (status == l_True) {
  //   printf("asymmetric\n");
  //   Abc_NtkForEachPi(pCone, pObj, idx){
  //     printf("%d", sat_solver_var_value(pSat, pCnf->pVarNums[pObj->Id]));
  //   }
  //   printf("\n");
  //   Abc_NtkForEachPi(pCone, pObj, idx){
  //     printf("%d", sat_solver_var_value(pSat, pCnf2->pVarNums[pObj->Id]));
  //   }
  //   printf("\n");

  // }
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

int Lsv_CommandSimBdd(Abc_Frame_t* pAbc, int argc, char** argv) {
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
  Lsv_NtkSimBdd(pNtk, argv[1]);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sim_bdd [-h] <sim pattern>\n");
  Abc_Print(-2, "\t        simulation with cofactor\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv) {
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
  Lsv_NtkSimAig(pNtk, argv[1]);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sim_bdd [-h] <sim pattern>\n");
  Abc_Print(-2, "\t        simulation with cofactor\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

int Lsv_CommandSymBdd(Abc_Frame_t* pAbc, int argc, char** argv) {
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
  Lsv_NtkSymBdd(pNtk, atoi(argv[1]), atoi(argv[2]), atoi(argv[3]));
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sim_bdd [-h] <sim pattern>\n");
  Abc_Print(-2, "\t        simulation with cofactor\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

int Lsv_CommandSymSat(Abc_Frame_t* pAbc, int argc, char** argv) {
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
  Lsv_NtkSymSat(pNtk, atoi(argv[1]), atoi(argv[2]), atoi(argv[3]));
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sim_bdd [-h] <sim pattern>\n");
  Abc_Print(-2, "\t        simulation with cofactor\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

int Lsv_CommandSymAll(Abc_Frame_t* pAbc, int argc, char** argv) {
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
  Lsv_NtkSymAll(pNtk, atoi(argv[1]));
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sim_bdd [-h] <sim pattern>\n");
  Abc_Print(-2, "\t        simulation with cofactor\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}