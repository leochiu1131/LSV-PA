#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "bdd/cudd/cudd.h"
#include "sat/cnf/cnf.h"
extern "C"{
   Aig_Man_t* Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
}
#include <string>
#include <iostream>
#include <vector>

using namespace std;
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

int Lsv_CommandSimBdd(Abc_Frame_t* pAbc, int argc, char** argv)
{
   if (argc != 2) {
      printf("invalid number of argument\n");
      return 0;
   }
   long long patternLen = strlen(argv[1]); 
   Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
   if (!Abc_NtkHasBdd(pNtk)) {
      printf("Please collapse first\n");
      return 0;
   }
   if (patternLen != Abc_NtkPiNum(pNtk)) {
      printf("invalid length of pattern\n");
      return 0;
   }

   int ithPo = 0;
   Abc_Obj_t* pPo = 0;

   Abc_NtkForEachPo(pNtk, pPo, ithPo) {
      Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo); // n9-n12
      assert( Abc_NtkIsBddLogic(pRoot->pNtk) );
      DdManager* dd = (DdManager *)pRoot->pNtk->pManFunc;  
      
      // Todo: do cofatoring
      Abc_Obj_t* pFanin = 0;
      DdNode* y = (DdNode*)pRoot->pData;
      int i = 0;
      int iFanin = 0;
      Abc_ObjForEachFanin(pRoot, pFanin, i) {
         if ((iFanin = Vec_IntFind(&pRoot->vFanins, pFanin->Id)) == -1) {
            printf( "Node %s should be among", Abc_ObjName(pFanin) );
            printf( " the fanins of node %s...\n", Abc_ObjName(pRoot) );
            return 0;
         }
         DdNode* bVar = Cudd_bddIthVar(dd, iFanin);
         int id = 0, j = 0;
         Abc_Obj_t* pPi = 0;
         Abc_NtkForEachPi(pNtk, pPi, j) {
            if (pPi->Id == pFanin->Id) {
               id = j;
               break;
            }
         }
         if (argv[1][id] == '0') 
            y = Cudd_Cofactor(dd, y, Cudd_Not(bVar));
         else if (argv[1][id] == '1')
            y = Cudd_Cofactor(dd, y, bVar);
         else {
            printf("invalid input!!!!\n");
            return 0;
         }   
         // delete buff; 
      }
      printf("%s: ", Abc_ObjName(pPo));
      printf("%d\n", y==dd->one);
   }
  
   return 1;
}

int zero2one(int n, int k) 
{
   return n | (1 << k);
}

int one2zero(int n, int k)
{
   return n & ~(1 << k);
}

void simulate(Abc_Ntk_t* pNtk)
{
   int ithGate = 0;
   Abc_Obj_t* gate = 0;
   Abc_NtkForEachObj(pNtk, gate, ithGate) {
      if (gate->Type == 7) {
         int fanin0 = gate->fCompl0==1 ? ~(Abc_ObjFanin0(gate)->iTemp) : Abc_ObjFanin0(gate)->iTemp;
         int fanin1 = gate->fCompl1==1 ? ~(Abc_ObjFanin1(gate)->iTemp) : Abc_ObjFanin1(gate)->iTemp;
         gate->iTemp = (fanin0 & fanin1);
      }
   }
   
}

int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv)
{
   if (argc != 2) {
      printf("invalid number of argument\n");
      return 0;
   }
   Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
   if (!Abc_NtkIsStrash(pNtk)) {
      printf("Please strash first\n");
      return 0;
   }
   FILE* fPtr = 0;
   fPtr = fopen(argv[1], "r");
   if (!fPtr) {
      printf("no such file\n");
      return 0;
   }

   int piNum = Abc_NtkPiNum(pNtk);
   int poNum = Abc_NtkPoNum(pNtk);
   long long maxPatternNum = 0, realPatternNum = 0;
   for (char c = getc(fPtr); c != EOF; c = getc(fPtr))
      if (c == '\n') // Increment count if this character is newline
         maxPatternNum = maxPatternNum + 1;
   fclose(fPtr);
   bool poOutput[poNum][maxPatternNum] = {0};
   char* pattern = (char*)malloc((piNum+3)*sizeof(char));
   int ithPi = 0, ithPo = 0, count = 0;
   Abc_Obj_t* pPi = 0;
   Abc_Obj_t* pPo = 0;
   
   fPtr = fopen(argv[1], "r");
   while (fgets(pattern, piNum+3, fPtr)) {
      if (strlen(pattern)-1 != piNum) {
         // printf("len = %ld\n", strlen(pattern)-1);
         printf("invalid pattern num\n");
         return 0;
      }
      Abc_NtkForEachPi(pNtk, pPi, ithPi) {
         if (pattern[ithPi] == '0')
            pPi->iTemp = one2zero(pPi->iTemp, count);
         else if (pattern[ithPi] == '1')
            pPi->iTemp = zero2one(pPi->iTemp, count);
         else {
            printf("invalid pattern!!!! \n");
            printf("%s", pattern);
            return 0;
         }
      }
      realPatternNum++; count++;
      if (count == 32) {
         simulate(pNtk);
         count = 0;
         Abc_NtkForEachPo(pNtk, pPo, ithPo) {
            int simVal = pPo->fCompl0 ==1 ? ~(Abc_ObjFanin0(pPo)->iTemp) : Abc_ObjFanin0(pPo)->iTemp;
            for (long long i = realPatternNum-32, j = 0; i < realPatternNum; i++, j++) {
               poOutput[ithPo][i] = (simVal >> j) & 1;
            }
         }
      }     
   }
   if (count > 0 && count < 32) {
      simulate(pNtk);
      Abc_NtkForEachPo(pNtk, pPo, ithPo) {
         int simVal = pPo->fCompl0 ==1 ? ~(Abc_ObjFanin0(pPo)->iTemp) : Abc_ObjFanin0(pPo)->iTemp;
         for (long long i = realPatternNum-count, j = 0; i < realPatternNum; i++, j++) {
            poOutput[ithPo][i] = (simVal >> j) & 1;
         }
      }
   }
   

   Abc_NtkForEachPo(pNtk, pPo, ithPo) {
      printf("%s: ", Abc_ObjName(pPo));
      for (int i = 0; i < realPatternNum; i++) {
         printf("%d", poOutput[ithPo][i]);
      }
      printf("\n");
   }

   fclose(fPtr);
   return 1;
}


DdNode* cofactor(DdManager* dd, DdNode* ddnode, int bdd_in_idx, bool in_neg)
{
   DdNode* f = 0;
   if (bdd_in_idx < 0) {
      return ddnode;
   }
   DdNode* in_var = in_neg ? Cudd_Not(Cudd_bddIthVar(dd, bdd_in_idx)) : Cudd_bddIthVar(dd, bdd_in_idx);
   Cudd_Ref(in_var);
   f = Cudd_Cofactor(dd, ddnode, in_var);
   Cudd_Ref(f);
   Cudd_RecursiveDeref(dd, in_var);
   return f;
}

bool find_counter_example(DdManager* dd, DdNode* f, string& counter_example, int curr_idx)
{
   if (f == Cudd_ReadZero(dd) || f == Cudd_Not(DD_ONE(dd))) {
      return false;
   }
   
   if (f == Cudd_ReadOne(dd) || f == Cudd_Not(DD_ZERO(dd))) return true;
   
   // f_1
   DdNode* curr_var = Cudd_bddIthVar(dd, curr_idx);
   Cudd_Ref(curr_var);
   DdNode* next_f = Cudd_Cofactor(dd, f, curr_var);
   Cudd_Ref(next_f);
   counter_example[curr_idx] = '1';
   if (find_counter_example(dd, next_f, counter_example, curr_idx+1)) {
      Cudd_RecursiveDeref(dd, next_f);
      Cudd_RecursiveDeref(dd, curr_var);
      return true;
   }
   // cout << "curr idx = " << curr_idx << endl;
   // f_0
   Cudd_RecursiveDeref(dd, next_f);
   curr_var = Cudd_Not(curr_var);
   Cudd_Ref(curr_var);
   next_f = Cudd_Cofactor(dd, f, curr_var);
   Cudd_Ref(next_f);
   counter_example[curr_idx] = '0';
   if (find_counter_example(dd, next_f, counter_example, curr_idx+1)) {
      Cudd_RecursiveDeref(dd, next_f);
      Cudd_RecursiveDeref(dd, curr_var);
      return true;
   }
   
   return false;
}


int Lsv_CommandSymBdd(Abc_Frame_t* pAbc, int argc, char** argv)
{

   if (argc != 4) {
      cout << "Invalid number of argument" << endl;
      return 0;
   }

   Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
   
   if (!Abc_NtkIsBddLogic(pNtk)) {
      cout << "Please collapse first" << endl;
      return 0;
   }
   
   int in0_idx, in1_idx, out_idx;
   out_idx = stoi(argv[1]);
   in0_idx = stoi(argv[2]);
   in1_idx = stoi(argv[3]);

   if (out_idx >= Abc_NtkPoNum(pNtk) || in0_idx >= Abc_NtkPiNum(pNtk) 
      || in1_idx >= Abc_NtkPiNum(pNtk) || in0_idx == in1_idx) {
      cout << "Invalid argument" << endl;
      return 0;
   }

   // case 1 in0_idx, in1_idx not int the bdd -> symmetric
   Abc_Obj_t* out_ptr = Abc_NtkPo(pNtk, out_idx);
   Abc_Obj_t* root_ptr = Abc_ObjFanin0(out_ptr);
   Abc_Obj_t* in0_ptr = Abc_NtkPi(pNtk, in0_idx);
   Abc_Obj_t* in1_ptr = Abc_NtkPi(pNtk, in1_idx);
   Abc_Obj_t* temp = 0;
   int ithPi = 0, bdd_in0_idx = -1, bdd_in1_idx = -1;

   // bdd order 
   Abc_ObjForEachFanin(root_ptr, temp, ithPi) {
      if (Abc_ObjId(temp) == Abc_ObjId(in0_ptr)) {
         bdd_in0_idx = ithPi;
      }
      if (Abc_ObjId(temp) == Abc_ObjId(in1_ptr)) {
         bdd_in1_idx = ithPi;
      }
   }  
   
   // cout << "in0 idx = " << bdd_in0_idx << endl;
   // cout << "in1_idx = " << bdd_in1_idx << endl;

   
   if (bdd_in0_idx == -1 && bdd_in1_idx == -1) {
      cout << "symmetric" << endl;
      return 1;
   }

   // case 2 at most one in_idx not in bdd
   DdManager* dd = (DdManager*)root_ptr->pNtk->pManFunc;
   DdNode* ddnode = (DdNode*)root_ptr->pData;
   DdNode *f_01 = 0, *f_10 = 0, *XOR = 0;


   f_01 = cofactor(dd, ddnode, bdd_in0_idx, true);
   f_01 = cofactor(dd, f_01, bdd_in1_idx, false);
   f_10 = cofactor(dd, ddnode, bdd_in0_idx, false);
   f_10 = cofactor(dd, f_10, bdd_in1_idx, true);
   XOR = Cudd_bddXor(dd, f_01, f_10);
   Cudd_Ref(XOR);

   if (XOR == DD_ZERO(dd) || XOR == Cudd_Not(DD_ONE(dd))) {
      cout << "symmetric" << endl;
      return 1;
   }
   
   cout << "asymmetric" << endl;
   string bdd_counter_example(Abc_ObjFaninNum(root_ptr), '0');
   string ntk_counter_example(Abc_NtkPiNum(pNtk), '0');
   int ntk_id_idx_map[Abc_NtkPiNum(pNtk)];
   Abc_Obj_t* pPi = 0;
   find_counter_example(dd, XOR, bdd_counter_example, 0);
   Abc_NtkForEachPi(pNtk, pPi, ithPi) { ntk_id_idx_map[Abc_ObjId(pPi)] = ithPi; }
   Abc_ObjForEachFanin(root_ptr, temp, ithPi) { ntk_counter_example[ntk_id_idx_map[Abc_ObjId(temp)]] = bdd_counter_example[ithPi]; }

   // Abc_NtkForEachPi(pNtk, pPi, ithPi) { cout << "ntk idx = " << ithPi << endl; }
   // Abc_ObjForEachFanin(root_ptr, temp, ithPi) { cout << "bdd to ntk idx= " << ntk_id_idx_map[Abc_ObjId(temp)] << endl; }   

   // print f_01
   // cout << "bdd counter = " << bdd_counter_example << endl;
   // cout << "ntk counter = " << ntk_counter_example << endl;
   for (int i = 0 ; i < Abc_NtkPiNum(pNtk); i++) {
      if (i == in0_idx) cout << "0";
      else if (i == in1_idx) cout << "1";
      else cout << ntk_counter_example[i];
   }

   cout << endl;
   // print f_10
   for (int i = 0 ; i < Abc_NtkPiNum(pNtk); i++) {
      if (i == in0_idx) cout << "1";
      else if (i == in1_idx) cout << "0";
      else cout << ntk_counter_example[i];
   }

   cout << endl;

   Cudd_RecursiveDeref(dd, XOR);
   Cudd_RecursiveDeref(dd, f_01);
   Cudd_RecursiveDeref(dd, f_10);

   return 1; 
}

int Lsv_CommandSymSat(Abc_Frame_t* pAbc, int argc, char** argv)
{
   if (argc != 4) {
      cout << "Invalid number of argument" << endl;
      return 0;
   }

   Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
   int out_idx, in0_idx, in1_idx;

   if (!Abc_NtkIsStrash(pNtk)) {
      cout << "Strash First" << endl;
   }
   
   out_idx = stoi(argv[1]);
   in0_idx = stoi(argv[2]);
   in1_idx = stoi(argv[3]);

   if (out_idx >= Abc_NtkPoNum(pNtk) || in0_idx >= Abc_NtkPiNum(pNtk) 
      || in1_idx >= Abc_NtkPiNum(pNtk) || in0_idx == in1_idx) {
      cout << "Invalid argument" << endl;
      return 0;
   }

   Abc_Obj_t* out_ptr = Abc_NtkPo(pNtk, out_idx);
   Abc_Ntk_t* sub_ntk = Abc_NtkCreateCone(pNtk, Abc_ObjFanin0(out_ptr), Abc_ObjName(Abc_ObjFanin0(out_ptr)), 1);
   Aig_Man_t* aig_ntk = Abc_NtkToDar(sub_ntk, 0, 1);
   Aig_Obj_t* pObj = 0;
   int i = 0, Lits[3];
   
   sat_solver* sat = sat_solver_new();
   Cnf_Dat_t* f01_cnf = Cnf_Derive(aig_ntk, 1);
   Cnf_DataWriteIntoSolverInt(sat, f01_cnf, 1, 0);

   Cnf_Dat_t* f10_cnf = Cnf_Derive(aig_ntk, 1);
   Cnf_DataLift(f10_cnf, f01_cnf->nVars);
   
   Cnf_DataWriteIntoSolverInt(sat, f10_cnf, 1, 0);
   

   Aig_ManForEachCo(aig_ntk, pObj, i) {
      // cout << "01 id = " << f01_cnf->pVarNums[pObj->Id] << endl;
      // cout << "10 id = " << f10_cnf->pVarNums[pObj->Id] << endl;
      Lits[0] = toLitCond(f01_cnf->pVarNums[pObj->Id], 0);
      Lits[1] = toLitCond(f10_cnf->pVarNums[pObj->Id], 0);
      sat_solver_addclause(sat, Lits, Lits+2);
      Lits[0] = toLitCond(f01_cnf->pVarNums[pObj->Id], 1);
      Lits[1] = toLitCond(f10_cnf->pVarNums[pObj->Id], 1);
      sat_solver_addclause(sat, Lits, Lits+2);
   }

   Aig_ManForEachCi(aig_ntk, pObj, i) {
      if (i == in0_idx) {
         Lits[0] = toLitCond(f01_cnf->pVarNums[pObj->Id], 0);
         sat_solver_addclause(sat, Lits, Lits+1);
         Lits[0] = toLitCond(f10_cnf->pVarNums[pObj->Id], 1);
         sat_solver_addclause(sat, Lits, Lits+1);
      }
      else if (i == in1_idx) {
         Lits[0] = toLitCond(f01_cnf->pVarNums[pObj->Id], 1);
         sat_solver_addclause(sat, Lits, Lits+1);
         Lits[0] = toLitCond(f10_cnf->pVarNums[pObj->Id], 0);
         sat_solver_addclause(sat, Lits, Lits+1);
      }
      else {
         Lits[0] = toLitCond(f01_cnf->pVarNums[pObj->Id], 0);
         Lits[1] = toLitCond(f10_cnf->pVarNums[pObj->Id], 1);
         sat_solver_addclause(sat, Lits, Lits+2);
         Lits[0] = toLitCond(f01_cnf->pVarNums[pObj->Id], 1);
         Lits[1] = toLitCond(f10_cnf->pVarNums[pObj->Id], 0);
         sat_solver_addclause(sat, Lits, Lits+2);
      }
   }
   
   

   int result = sat_solver_solve(sat, 0, 0, (ABC_INT64_T)0, (ABC_INT64_T)0, (ABC_INT64_T)0, (ABC_INT64_T)0);
   if (result == l_False) {
      cout << "symmetric" << endl;
      return 0;
   }

   cout << "asymmetric" << endl;
   Aig_ManForEachCi(aig_ntk, pObj, i) {
      if (i == in0_idx) cout << "0";
      else if (i == in1_idx) cout << "1";
      else cout << sat_solver_var_value(sat, f01_cnf->pVarNums[pObj->Id]);
   }

   cout << endl;

   Aig_ManForEachCi(aig_ntk, pObj, i) {
      if (i == in0_idx) cout << "1";
      else if (i == in1_idx) cout << "0";
      else cout << sat_solver_var_value(sat, f10_cnf->pVarNums[pObj->Id]);
   }

   cout << endl;
   return 1;
}

int Lsv_CommandSymAll(Abc_Frame_t* pAbc, int argc, char** argv)
{
   if (argc != 2) {
      cout << "Invalid number of argument" << endl;
      return 0;
   }

   Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
   int out_idx;

   if (!Abc_NtkIsStrash(pNtk)) {
      cout << "Strash First" << endl;
   }
   
   out_idx = stoi(argv[1]);
   

   if (out_idx >= Abc_NtkPoNum(pNtk)) {
      cout << "Invalid argument" << endl;
      return 0;
   }

   Abc_Obj_t* out_ptr = Abc_NtkPo(pNtk, out_idx);
   Abc_Ntk_t* sub_ntk = Abc_NtkCreateCone(pNtk, Abc_ObjFanin0(out_ptr), Abc_ObjName(Abc_ObjFanin0(out_ptr)), 1);
   Aig_Man_t* aig_ntk = Abc_NtkToDar(sub_ntk, 0, 1);
   Aig_Obj_t *obj_1 = 0, *obj_2 = 0;
   int i = 0, j = 0, Lits[4];
   
   sat_solver* sat = sat_solver_new();
   Cnf_Dat_t* f_A = Cnf_Derive(aig_ntk, 1);
   Cnf_DataWriteIntoSolverInt(sat, f_A, 1, 0);

   Cnf_Dat_t* f_B = Cnf_Derive(aig_ntk, 1);
   Cnf_DataLift(f_B, f_A->nVars);
   Cnf_DataWriteIntoSolverInt(sat, f_B, 1, 0);

   Cnf_Dat_t* f_H = Cnf_Derive(aig_ntk, 1);
   Cnf_DataLift(f_H, f_A->nVars*2);
   Cnf_DataWriteIntoSolverInt(sat, f_H, 1, 0);
   
   // f_A xor f_B
   Aig_ManForEachCo(aig_ntk, obj_1, i) {
      Lits[0] = toLitCond(f_A->pVarNums[obj_1->Id], 0);
      Lits[1] = toLitCond(f_B->pVarNums[obj_1->Id], 0);
      sat_solver_addclause(sat, Lits, Lits+2);
      Lits[0] = toLitCond(f_A->pVarNums[obj_1->Id], 1);
      Lits[1] = toLitCond(f_B->pVarNums[obj_1->Id], 1);
      sat_solver_addclause(sat, Lits, Lits+2);
   }

   // (Va = Vb)+Vh = (Va + ~Vb + Vh)(~Va + Vb + Vh)
   Aig_ManForEachCi(aig_ntk, obj_1, i) {
      Lits[0] = toLitCond(f_A->pVarNums[obj_1->Id], 0);
      Lits[1] = toLitCond(f_B->pVarNums[obj_1->Id], 1);
      Lits[2] = toLitCond(f_H->pVarNums[obj_1->Id], 1);
      sat_solver_addclause(sat, Lits, Lits+3);
      Lits[0] = toLitCond(f_A->pVarNums[obj_1->Id], 1);
      Lits[1] = toLitCond(f_B->pVarNums[obj_1->Id], 0);
      Lits[2] = toLitCond(f_H->pVarNums[obj_1->Id], 1);
      sat_solver_addclause(sat, Lits, Lits+3);
   }

   // (Va(i) = Vb(j))+~Vh(i)+~Vh(j) = (Va(i) + ~Vb(j) + ~Vh(i) + ~Vh(j))(~Va(i) + Vb(j) + ~Vh(i) + ~Vh(j)) for 0 <= i < j < m
   Aig_ManForEachCi(aig_ntk, obj_1, i) {
      Aig_ManForEachCi(aig_ntk, obj_2, j) {
         if (i >= j) continue;

         Lits[0] = toLitCond(f_A->pVarNums[obj_1->Id], 1);
         Lits[1] = toLitCond(f_B->pVarNums[obj_2->Id], 0);
         Lits[2] = toLitCond(f_H->pVarNums[obj_1->Id], 0);
         Lits[3] = toLitCond(f_H->pVarNums[obj_2->Id], 0);
         sat_solver_addclause(sat, Lits, Lits+4);

         Lits[0] = toLitCond(f_A->pVarNums[obj_1->Id], 0);
         Lits[1] = toLitCond(f_B->pVarNums[obj_2->Id], 1);
         Lits[2] = toLitCond(f_H->pVarNums[obj_1->Id], 0);
         Lits[3] = toLitCond(f_H->pVarNums[obj_2->Id], 0);   
         sat_solver_addclause(sat, Lits, Lits+4);

         Lits[0] = toLitCond(f_A->pVarNums[obj_2->Id], 1);
         Lits[1] = toLitCond(f_B->pVarNums[obj_1->Id], 0);
         Lits[2] = toLitCond(f_H->pVarNums[obj_2->Id], 0);
         Lits[3] = toLitCond(f_H->pVarNums[obj_1->Id], 0);
         sat_solver_addclause(sat, Lits, Lits+4);   

         Lits[0] = toLitCond(f_A->pVarNums[obj_2->Id], 0);
         Lits[1] = toLitCond(f_B->pVarNums[obj_1->Id], 1);
         Lits[2] = toLitCond(f_H->pVarNums[obj_2->Id], 0);
         Lits[3] = toLitCond(f_H->pVarNums[obj_1->Id], 0);   
         sat_solver_addclause(sat, Lits, Lits+4);
      }
   }

   int ci_num = Aig_ManCiNum(aig_ntk), result = 0;
   vector<vector<bool>> sym_table(ci_num, vector<bool>(ci_num, false));
   Aig_ManForEachCi(aig_ntk, obj_1, i) {
      Aig_ManForEachCi(aig_ntk, obj_2, j) {
         if (i >= j) continue;
         if (sym_table[i][j]) {
            cout << i << " " << j << endl;
            continue;
         }
         Lits[0] = toLitCond(f_H->pVarNums[obj_1->Id], 1);
         Lits[1] = toLitCond(f_H->pVarNums[obj_2->Id], 1);
         result = sat_solver_solve(sat, Lits, Lits+2, (ABC_INT64_T)0, (ABC_INT64_T)0, (ABC_INT64_T)0, (ABC_INT64_T)0);

         if (result == l_False) {
            sym_table[i][j] = true;
            cout << i << " " << j << endl;
            for (int k = i+1 ; k < j ; k++) {
               if (sym_table[i][k]) sym_table[k][j] = true;
            }
         }

      }
   }
   return 1;
}