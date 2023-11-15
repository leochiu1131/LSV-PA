#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "bdd/extrab/extraBdd.h"
#include <map>
#include <string>
#include <vector>
// #include <iostream>

using namespace std;

#include "sat/cnf/cnf.h" 
extern "C"{ 
  Aig_Man_t* Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
}


class Simulation{
  public:
    Simulation(){}
    Simulation(size_t pattern): psim(pattern){}

    void   push_sim_pattern(int i) { psim = (psim << 1) + i; }
    void   set_sim_pattern(size_t pattern) { psim = pattern; }
    size_t get_sim_pattern() { return psim; }
    void   set_fanin1_pattern(size_t pattern) { psim_fanin1 = pattern; }
    size_t get_fanin1_pattern() { return psim_fanin1; }
    void   set_fanin2_pattern(size_t pattern) { psim_fanin2 = pattern; }
    size_t get_fanin2_pattern() { return psim_fanin2; }
    void   set_complement1(bool is_com) { is_complement1 = is_com; }
    bool   get_complement1() { return is_complement1; }
    void   set_complement2(bool is_com) { is_complement2 = is_com; }
    bool   get_complement2() { return is_complement2; }
    

    void   sim() {
      size_t a, b;
      if (is_complement1){
        a = psim_fanin1 ^ (~0);
      }
      else{
        a = psim_fanin1;
      }
      if (is_complement2){
        b = psim_fanin2 ^ (~0);
      }
      else{
        b = psim_fanin2;
      }
      psim = a & b;
    }

  private:
    size_t psim = 0;
    size_t psim_fanin1;
    size_t psim_fanin2;
    bool   is_complement1;
    bool   is_complement2;
};

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimBdd(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSymBdd(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSymSat(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", Lsv_CommandSimBdd, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig", Lsv_CommandSimAig, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_bdd", Lsv_CommandSymBdd, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_sat", Lsv_CommandSymSat, 0);
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

void Lsv_NtkSimBdd(Abc_Ntk_t* pNtk, char* sim_pattern) {
  int i;
  int j;
  Abc_Obj_t * pObj;
  Vec_Ptr_t * vFuncsGlob;
  DdNode * bFunc;
  DdManager * dd =  (DdManager *)pNtk->pManFunc;
  Abc_Obj_t * pNode1;
  Abc_Ntk_t * pTemp = Abc_NtkStrash(pNtk, 0, 0, 0);
  dd = (DdManager *)Abc_NtkBuildGlobalBdds( pTemp, 10000000, 1, 0, 0, 0 );

  vFuncsGlob = Vec_PtrAlloc( Abc_NtkCoNum(pTemp) );

  Abc_NtkForEachCo( pTemp, pObj, i )
      Vec_PtrPush( vFuncsGlob, Abc_ObjGlobalBdd(pObj) );

  DdNode ** pbAdds = ABC_ALLOC( DdNode *, Vec_PtrSize(vFuncsGlob) );
  Vec_PtrForEachEntry( DdNode *, vFuncsGlob, bFunc, i ) { 
    pbAdds[i] = Cudd_BddToAdd( dd, bFunc );
    Cudd_Ref( pbAdds[i] );
  }

  Abc_NtkForEachPo(pTemp, pNode1, i){
    DdNode* assignmentBDD = Cudd_ReadOne(dd); // Initialize to TRUE
    // Initialize a new BDD node to represent the assignment
    // DdNode * output = (DdNode *)(Abc_ObjFanin0( pNode1 )->pData);
    DdNode * output = pbAdds[i];
    for (j = 0; j < dd->size; j++) {
      DdNode* varBDD = dd->vars[j]; // Get the BDD for the i-th variable
      DdNode* varAssignmentBDD = (sim_pattern[dd->invperm[j]] == '0') ? Cudd_Not(varBDD) : varBDD;
      assignmentBDD = Cudd_bddAnd(dd, assignmentBDD, varAssignmentBDD);
    }
    // printf("\n");
    // Compute the cofactor of the outputBDD with respect to the assignment
    DdNode* resultBDD = Cudd_Cofactor(dd, output, assignmentBDD);
    if (resultBDD == Cudd_ReadOne(dd)) {
      printf("%s: 1\n",Abc_ObjName(pNode1));
    }
    else {
      printf("%s: 0\n",Abc_ObjName(pNode1));
    }
    Cudd_RecursiveDeref(dd, assignmentBDD);
    Cudd_RecursiveDeref(dd, resultBDD);
  }
  Vec_PtrForEachEntry( DdNode *, vFuncsGlob, bFunc, i )
      Cudd_RecursiveDeref( dd, pbAdds[i] );
  ABC_FREE( pbAdds );
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
  Abc_Print(-2, "usage: lsv_sim_bdd <input pattern> [-h]\n");
  Abc_Print(-2, "\t        print output result when simulating the input pattern\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}


int getNthBit(int num, int n) {
    // Shift the 1 bit to the left by n positions
    // and perform a bitwise AND with the number
    // This will isolate the nth bit
    return (num >> n) & 1;
}

void Lsv_NtkSimAig(Abc_Ntk_t* pNtk, char* inputfile) {
  FILE* filePointer;
  char pattern[1024];
  int i,k;
  int count = 0;
  map<string, Simulation*> sim_table;
  vector<string> store_table;
  Abc_Obj_t * pNode, * pFanin;

  Vec_Ptr_t * vNodes;
  vNodes = Abc_NtkDfsIter( pNtk, 0);
  
  filePointer = fopen(inputfile, "r");
  Abc_NtkForEachPo(pNtk, pNode, i) {
    string temp;
    store_table.push_back(temp);
  }
  // Abc_NtkForEachPo(pNtk, pNode, i){
  //   printf("%s: %s\n", Abc_ObjName(pNode), store_table[i].c_str());
  // }


  int times = 0;
  while (fgets(pattern, sizeof(pattern), filePointer) != NULL) {
    // Process each line here
    size_t length = strlen(pattern);
    if (length > 0 && pattern[length - 1] == '\n') {
        pattern[length - 1] = '\0'; // Replace '\n' with '\0' to remove it
    }
    count++;
    Abc_NtkForEachPi(pNtk, pNode, i){
      if (sim_table.find(Abc_ObjName(pNode)) == sim_table.end()) {
        Simulation * new_sim = new Simulation;
        if (pattern[i] == '1') {
          new_sim->push_sim_pattern(1);
        }
        else {
          new_sim->push_sim_pattern(0);
        }

        sim_table[Abc_ObjName(pNode)] = new_sim;
      }
      else {
        if (pattern[i] == '1') {
          sim_table[Abc_ObjName(pNode)]->push_sim_pattern(1);
        }
        else {
          sim_table[Abc_ObjName(pNode)]->push_sim_pattern(0);
        }
      }
    }
    times++;
    if (times % 32 == 0 && times != 0) {
      Vec_PtrForEachEntry( Abc_Obj_t *, vNodes, pNode, i ){
        Simulation * new_sim;
        if (sim_table.find(Abc_ObjName(pNode)) == sim_table.end()) {
          new_sim = new Simulation;
        }
        else {
          new_sim = sim_table[Abc_ObjName(pNode)];
        }
        // printf("name: %s\n", Abc_ObjName(pNode));
        // printf("n: %d\n", Abc_ObjFaninNum(pNode));
        // printf("%s, %s\n", Abc_ObjName(Abc_ObjFanin0(pNode)), Abc_ObjName(Abc_ObjFanin1(pNode)));
        // printf("%d, %d\n", Abc_ObjFaninC(pNode,0), Abc_ObjFaninC(pNode,1));

        Abc_ObjForEachFanin( pNode, pFanin, k )
        {
          if (k == 0) {
            new_sim->set_fanin1_pattern(sim_table[Abc_ObjName(pFanin)]->get_sim_pattern());
          }
          else
            new_sim->set_fanin2_pattern(sim_table[Abc_ObjName(pFanin)]->get_sim_pattern());
        }
        new_sim->set_complement1(Abc_ObjFaninC(pNode,0));
        new_sim->set_complement2(Abc_ObjFaninC(pNode,1));
        new_sim->sim();
        if (sim_table.find(Abc_ObjName(pNode)) == sim_table.end()) {
          sim_table[Abc_ObjName(pNode)] = new_sim;
        }
      }

      Abc_NtkForEachPo(pNtk, pNode, i){
        if (sim_table.find(Abc_ObjName(Abc_ObjFanin0(pNode))) == sim_table.end()) {
          if (Abc_AigNodeIsConst(Abc_ObjFanin0(pNode))) {
            if (Abc_ObjFaninC(pNode,0)) {
              for (int m = 0; m < 32; m++) {
                store_table[i] += "0";
              }
            }
            else {
              for (int m = 0; m < 32; m++) {
                store_table[i] += "1";
              }
            }
          }
          continue;
        }
        Simulation * output = sim_table[Abc_ObjName(Abc_ObjFanin0(pNode))];
        bool is_com = Abc_ObjFaninC(pNode,0);
        
        for (int m = 31; m >= 0; m--) {
          int temp = getNthBit(output->get_sim_pattern(), m);
          if (temp == 0) {
            if (is_com) {
              store_table[i] += "1";
            }
            else {
              store_table[i] += "0";
            }
          }
          else {
            if (is_com) {
              store_table[i] += "0";
            }
            else {
              store_table[i] += "1";
            }
          }
        }
      }
    }
  }

  if (times % 32 != 0) {
    int temp1 = times % 32;
    Vec_PtrForEachEntry( Abc_Obj_t *, vNodes, pNode, i ){
      Simulation * new_sim = new Simulation;
    
      // printf("name: %s\n", Abc_ObjName(pNode));
      // printf("n: %d\n", Abc_ObjFaninNum(pNode));
      // printf("%s, %s\n", Abc_ObjName(Abc_ObjFanin0(pNode)), Abc_ObjName(Abc_ObjFanin1(pNode)));
      // printf("%d, %d\n", Abc_ObjFaninC(pNode,0), Abc_ObjFaninC(pNode,1));

      Abc_ObjForEachFanin( pNode, pFanin, k )
      {
        if (k == 0)
          new_sim->set_fanin1_pattern(sim_table[Abc_ObjName(pFanin)]->get_sim_pattern());
        else
          new_sim->set_fanin2_pattern(sim_table[Abc_ObjName(pFanin)]->get_sim_pattern());
      }
      new_sim->set_complement1(Abc_ObjFaninC(pNode,0));
      new_sim->set_complement2(Abc_ObjFaninC(pNode,1));
      new_sim->sim();
      sim_table[Abc_ObjName(pNode)] = new_sim;
    }

    Abc_NtkForEachPo(pNtk, pNode, i){
      if (sim_table.find(Abc_ObjName(Abc_ObjFanin0(pNode))) == sim_table.end()) {
        if (Abc_AigNodeIsConst(Abc_ObjFanin0(pNode))) {
          if (Abc_ObjFaninC(pNode,0)) {
            for (int m = 0; m < temp1; m++) {
              store_table[i] += "0";
            }
          }
          else {
            for (int m = 0; m < temp1; m++) {
              store_table[i] += "1";
            }
          }
        }
        continue;
      }
      Simulation * output = sim_table[Abc_ObjName(Abc_ObjFanin0(pNode))];
      bool is_com = Abc_ObjFaninC(pNode,0);
      
      for (int m = temp1-1; m >= 0; m--) {
        int temp = getNthBit(output->get_sim_pattern(), m);
        if (temp == 0) {
          if (is_com) {
            store_table[i] += "1";
          }
          else {
            store_table[i] += "0";
          }
        }
        else {
          if (is_com) {
            store_table[i] += "0";
          }
          else {
            store_table[i] += "1";
          }
        }
      }
    }
  }

  Abc_NtkForEachPo(pNtk, pNode, i){
    printf("%s: %s\n", Abc_ObjName(pNode), store_table[i].c_str());
  }

  fclose(filePointer);
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
  Abc_Print(-2, "usage: lsv_sim_aig <input file name> [-h]\n");
  Abc_Print(-2, "\t        print parallel output results when simulating the input pattern\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

bool Recur_get_diff_pattern(DdManager* dd, string& pattern1, string& pattern2, int cur_var, int num_var, DdNode* temp1, DdNode* temp2, int i, int j) {
    if (cur_var == num_var) return true;
    
    if (cur_var == i) {
      pattern1 += "0";
      pattern2 += "1";
      if (!Recur_get_diff_pattern(dd, pattern1, pattern2, cur_var+1, num_var, temp1, temp2, i, j)){
        pattern1.erase(pattern1.size() - 1);
        pattern2.erase(pattern2.size() - 1);
        return false;
      }
      else 
        return true;
    }
    else if (cur_var == j) {
      pattern1 += "1";
      pattern2 += "0";
      if (!Recur_get_diff_pattern(dd, pattern1, pattern2, cur_var+1, num_var, temp1, temp2, i, j)){
        pattern1.erase(pattern1.size() - 1);
        pattern2.erase(pattern2.size() - 1);
        return false;
      }
      else 
        return true;
    }
    else {
      DdNode* temp1_1 = Cudd_Cofactor( dd, temp1, dd->vars[cur_var] );
      DdNode* temp2_1 = Cudd_Cofactor( dd, temp2, dd->vars[cur_var] );
      Cudd_Ref(temp1_1);
      Cudd_Ref(temp2_1);
      if (temp1_1 == temp2_1) {
        Cudd_RecursiveDeref(dd, temp1_1);
        Cudd_RecursiveDeref(dd, temp2_1);
        temp1_1 = Cudd_Cofactor( dd, temp1, Cudd_Not(dd->vars[cur_var]) );
        temp2_1 = Cudd_Cofactor( dd, temp2, Cudd_Not(dd->vars[cur_var]) );

      }
    }
}

void Lsv_NtkSymBdd(Abc_Ntk_t* pNtk, char* output, char* input1, char* input2) {
  DdManager* dd = (DdManager*) pNtk->pManFunc; //bdd manager
  DdNode* temp1_3, * temp2_3;
  int k = stoi(output);
  int i = stoi(input1);
  int j = stoi(input2);
  Abc_Obj_t* Po = Abc_NtkPo(pNtk, k);
  string pattern1, pattern2;
  Abc_Obj_t* func = Abc_ObjFanin0(Po); //the original function
  DdNode* temp1 = (DdNode*) func->pData;
  Cudd_Ref(temp1);
  DdNode* temp1_1 = Cudd_Cofactor( dd, temp1, Cudd_Not(dd->vars[i]) );
  Cudd_Ref(temp1_1);
  DdNode* temp1_2 = Cudd_Cofactor( dd, temp1_1, dd->vars[j] );
  Cudd_Ref(temp1_2);
  DdNode* temp2 = (DdNode*) func->pData;
  Cudd_Ref(temp2);
  DdNode* temp2_1 = Cudd_Cofactor( dd, temp2, dd->vars[i] );
  Cudd_Ref(temp2_1);
  DdNode* temp2_2 = Cudd_Cofactor( dd, temp2_1, Cudd_Not(dd->vars[j]) );
  Cudd_Ref(temp2_2);
  Cudd_RecursiveDeref(dd, temp1);
  Cudd_RecursiveDeref(dd, temp1_1);
  Cudd_RecursiveDeref(dd, temp2);
  Cudd_RecursiveDeref(dd, temp2_1);
  if (temp1_2 == temp2_2) {
    printf("symmetric\n");
    Cudd_RecursiveDeref(dd, temp1_2);
    Cudd_RecursiveDeref(dd, temp2_2);
  }
  else {
    // Recur_get_diff_pattern(dd, pattern1, pattern2, 0, Abc_NtkCiNum(pNtk), temp1_2, temp2_2, i, j);
    for (int n = 0; n < Abc_NtkCiNum(pNtk); n++) {
      if (n == i) {
        pattern1 += "0";
        pattern2 += "1";
      }
      else if (n == j) {
        pattern1 += "1";
        pattern2 += "0";
      }
      else {
        temp1_3 = temp1_2;
        temp2_3 = temp2_2;
        temp1_2 = Cudd_Cofactor( dd, temp1_3, dd->vars[n] );
        temp2_2 = Cudd_Cofactor( dd, temp2_3, dd->vars[n] );
        Cudd_Ref(temp1_2);
        Cudd_Ref(temp2_2);
        if (temp1_2 == temp2_2) {
          Cudd_RecursiveDeref(dd, temp1_2);
          Cudd_RecursiveDeref(dd, temp2_2);
          temp1_2 = Cudd_Cofactor( dd, temp1_3, Cudd_Not(dd->vars[n]) );
          temp2_2 = Cudd_Cofactor( dd, temp2_3, Cudd_Not(dd->vars[n]) );
          pattern1 += "0";
          pattern2 += "0";
        }
        else {
          pattern1 += "1";
          pattern2 += "1";
        }
        Cudd_RecursiveDeref(dd, temp1_3);
        Cudd_RecursiveDeref(dd, temp2_3);
      }
    }
    printf("asymmetric\n");
    printf("%s\n", pattern1.c_str());
    printf("%s\n", pattern2.c_str());
  }
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
  Lsv_NtkSymBdd(pNtk, argv[1], argv[2], argv[3]);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sym_bdd <k> <i> <j> [-h]\n");
  Abc_Print(-2, "\t        print whether the ith variable and jth variable are symmetric for kth output.\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}


void Lsv_NtkSymSat(Abc_Ntk_t* pNtk, char* output, char* input1, char* input2) {
  // int k = stoi(output);
  // int i = stoi(input1);
  // int j = stoi(input2);
  // int m;
  // Abc_Obj_t* Po = Abc_NtkPo(pNtk, k);
  // Abc_Ntk_t* output_cone = Abc_NtkCreateCone(pNtk, Po, Abc_ObjName(Po), 0);
  // Aig_Man_t* aig_1 = Abc_NtkToDar(output_cone, 0, 0);
  // sat_solver* solver = sat_solver_new();
  // Cnf_Dat_t* cnf_1 =  Cnf_Derive(aig_1, 1);
  // solver = (sat_solver*)Cnf_DataWriteIntoSolverInt(solver, cnf_1, 1, 0);
  // Cnf_Dat_t* cnf_2 =  Cnf_DataDup(cnf_1);
  // Cnf_DataLift(cnf_2, cnf_2->nVars * 3);
  // solver = (sat_solver*)Cnf_DataWriteIntoSolverInt(solver, cnf_2, 1, 0);
  // Abc_Obj_t* Pi = Abc_NtkPi(pNtk, i);
  // Abc_Obj_t* Pj = Abc_NtkPi(pNtk, j);
  // int var_1_i = cnf_1->pVarNums[Pi->Id];
  // int var_1_j = cnf_1->pVarNums[Pj->Id];
  // int var_2_i = cnf_2->pVarNums[Pi->Id];
  // int var_2_j = cnf_2->pVarNums[Pj->Id];

  
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
  Lsv_NtkSymSat(pNtk, argv[1], argv[2], argv[3]);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sym_sat <k> <i> <j> [-h]\n");
  Abc_Print(-2, "\t        print whether the ith variable and jth variable are symmetric for kth output.\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}