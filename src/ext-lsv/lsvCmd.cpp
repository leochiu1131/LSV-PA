#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "bdd/extrab/extraBdd.h"
#include <map>
#include <string>
#include <vector>

using namespace std;

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

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", Lsv_CommandSimBdd, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig", Lsv_CommandSimAig, 0);
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

  // pNode = Abc_ObjFanin0( Abc_NtkPo(pNtk, 0) );
  // Abc_Obj_t * pObj;
  // Abc_NtkForEachObj( pNtk, pObj, i ) {
  //   printf("%s\n",Abc_ObjName(pObj));
  // }

  // Abc_NtkForEachPo(pNtk, pNode1, i){
  //   printf("%s\n", Abc_ObjName(pNode1));
  //   DdNode * output = (DdNode *)(Abc_ObjFanin0( pNode1 )->pData);
  //   Cudd_PrintMinterm(dd, output);
  // }
  // Abc_NtkForEachPi(pNtk, pNode2, j) {
  //   printf("%s: %c\n", Abc_ObjName(pNode2), sim_pattern[j]);

  //   printf("%d\n",dd->invperm[j]);
  //   DdNode* output = dd->vars[dd->invperm[j]];
  //   Cudd_PrintMinterm(dd, output);
  // }

    // for ( i = 0; i < dd->size; i++ )
    //     fprintf( pFile, "%d ", dd->invperm[i] );

  Abc_NtkForEachPo(pTemp, pNode1, i){
    DdNode* assignmentBDD = Cudd_ReadOne(dd); // Initialize to TRUE
    // Initialize a new BDD node to represent the assignment
    // DdNode * output = (DdNode *)(Abc_ObjFanin0( pNode1 )->pData);
    DdNode * output = pbAdds[i];
    for (j = 0; j < dd->size; j++) {
      DdNode* varBDD = dd->vars[j]; // Get the BDD for the i-th variable
      DdNode* varAssignmentBDD = (sim_pattern[dd->invperm[j]] == '0') ? Cudd_Not(varBDD) : varBDD;
      // printf("j: %d\n", j);
      // printf("index: %d\n", dd->perm[j]);
      // printf("%c", sim_pattern[dd->perm[j]]);
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
          if (k == 0)
            new_sim->set_fanin1_pattern(sim_table[Abc_ObjName(pFanin)]->get_sim_pattern());
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
          for (int m = 32; m > 0; m--) {
            store_table[i] += "0";
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
        for (int m = 0; m < temp1; m++) {
          store_table[i] += "0";
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
