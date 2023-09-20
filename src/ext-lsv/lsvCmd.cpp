#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"

#include "bdd/extrab/extraBdd.h"

#include <unordered_map>
#include <fstream>

static int Lsv_CommandPrintNodes(Abc_Frame_t *pAbc, int argc, char **argv);
static int Lsv_CommandSimulateBDD(Abc_Frame_t *pAbc, int argc, char **argv);
static int Lsv_CommandSimulateAIG(Abc_Frame_t *pAbc, int argc, char **argv);

void init(Abc_Frame_t *pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", Lsv_CommandSimulateBDD, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig", Lsv_CommandSimulateAIG, 0);
}

void destroy(Abc_Frame_t *pAbc) {}

Abc_FrameInitializer_t frame_initializer = { init, destroy };

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

void Lsv_NtkPrintNodes(Abc_Ntk_t *pNtk) {
  Abc_Obj_t *pObj;
  int i;
  Abc_NtkForEachNode(pNtk, pObj, i) {
    printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
    Abc_Obj_t *pFanin;
    int j;
    Abc_ObjForEachFanin(pObj, pFanin, j) {
      printf("  Fanin-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanin),
        Abc_ObjName(pFanin));
    }
    if ( Abc_NtkHasSop(pNtk) ) {
      printf("The SOP of this node:\n%s", ( char * ) pObj->pData);
    }
  }
}

int Lsv_CommandPrintNodes(Abc_Frame_t *pAbc, int argc, char **argv) {
  Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
  int c;
  Extra_UtilGetoptReset();
  while ( ( c = Extra_UtilGetopt(argc, argv, "h") ) != EOF ) {
    switch ( c ) {
    case 'h':
      goto usage;
    default:
      goto usage;
    }
  }
  if ( !pNtk ) {
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

void Lsv_NtkSimulateBdd(Abc_Ntk_t *pNtk, char *inputPattern) {
  int i;
  Abc_Obj_t *pNode;
  std::unordered_map <char *, bool> PIname2Value;
  DdManager *dd = ( DdManager * ) pNtk->pManFunc;
  if ( !Abc_NtkIsBddLogic(pNtk) )
  {
    Abc_Print(-1, "BDD simulation can only be done for logic BDD networks (run \"bdd\").\n");
    return;
  }
  // printf("PIs:\n");
  Abc_NtkForEachPi(pNtk, pNode, i) {
    PIname2Value[Abc_ObjName(pNode)] = ( int ) ( inputPattern[i] - 48 );
    // printf("%s (ID=%d): %d\n", Abc_ObjName(pNode), Abc_ObjId(pNode), ( int ) ( inputPattern[i] - 48 ));
  }

  // printf("Bdd Variables Count = %d\n", Cudd_ReadSize(dd));
  // printf("POs:\n");
  Abc_NtkForEachNode(pNtk, pNode, i) {
    Abc_Obj_t *pFanin, *pFanout;
    int j;
    pFanout = Abc_ObjFanout(pNode, 0);
    // printf("  PO-%d: Id = %d, name = %s\n", i, Abc_ObjId(pFanout), Abc_ObjName(pFanout));
    printf("%s: ", Abc_ObjName(pFanout));

    DdNode *funcNode = ( DdNode * ) pNode->pData;
    Abc_ObjForEachFanin(pNode, pFanin, j) {
      bool patternValue = PIname2Value[Abc_ObjName(pFanin)];
      // printf("  Fanin-%d: Id = %d, name = %s, value = %d\n", j, Abc_ObjId(pFanin), Abc_ObjName(pFanin), patternValue);
      DdNode *curDD = Cudd_bddIthVar(dd, j);
      if ( patternValue ) {
        funcNode = Cudd_Cofactor(dd, funcNode, curDD);
      }
      else {
        funcNode = Cudd_Cofactor(dd, funcNode, Cudd_Not(curDD));
      }
    }
    if ( funcNode == Cudd_ReadLogicZero(dd) ) {
      printf("0\n");
    }
    else if ( funcNode == Cudd_ReadOne(dd) ) {
      printf("1\n");
    }
    else {
      printf("Unexpected result.\n");
    }
  }
}

int Lsv_CommandSimulateBDD(Abc_Frame_t *pAbc, int argc, char **argv) {
  Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
  int c;
  Extra_UtilGetoptReset();
  while ( ( c = Extra_UtilGetopt(argc, argv, "h") ) != EOF ) {
    switch ( c ) {
    case 'h':
      goto usage;
    default:
      goto usage;
    }
  }

  if ( !pNtk ) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  Lsv_NtkSimulateBdd(pNtk, argv[1]);

  return 0;

usage:
  Abc_Print(-2, "usage: I don't know how to use it too.\n");
  return 1;
}

void Lsv_NtkSimulateAIG(Abc_Ntk_t *pNtk, char *inputFileName) {
  std::ifstream infile(inputFileName);
  if ( !infile.is_open() ) {
    Abc_Print(-1, "File not found.\n");
    return;
  }

  std::string line;
  int simulationCount = 0;
  while ( std::getline(infile, line) ) {
    ++simulationCount;
  }
  infile.close();

  int varCount = line.size();
  int intCount = simulationCount / 32 + 1;
  unsigned int **simulationInt = new unsigned int *[intCount];
  for ( int i = 0; i < intCount; ++i ) {
    simulationInt[i] = new unsigned int[varCount];
    for ( int v = 0; v < varCount; ++v ) {
      simulationInt[i][v] = 0;
    }
  }

  infile.open(inputFileName);
  while ( std::getline(infile, line) ) {
    for ( int v = 0; v < varCount; ++v ) {
      ( simulationInt[0][v] ) <<= 1;
      simulationInt[0][v] += ( line[v] == '0' ) ? 0 : 1;
    }
  }
  infile.close();

  int i;
  Abc_Obj_t *pNode;
  Abc_NtkForEachNode(pNtk, pNode, i) {  // * topological order
    // printf("%s\n", Abc_ObjName(pNode));
    Abc_AigPrintNode(pNode);
  }

  for ( int i = 0; i < intCount; ++i ) {
    delete[] simulationInt[i];
  }
  delete[] simulationInt;
}

int Lsv_CommandSimulateAIG(Abc_Frame_t *pAbc, int argc, char **argv) {
  Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
  int c;
  Extra_UtilGetoptReset();
  while ( ( c = Extra_UtilGetopt(argc, argv, "h") ) != EOF ) {
    switch ( c ) {
    case 'h':
      goto usage;
    default:
      goto usage;
    }
  }

  if ( !pNtk ) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  Lsv_NtkSimulateAIG(pNtk, argv[1]);

  return 0;

usage:
  Abc_Print(-2, "usage: I don't know how to use it too.\n");
  return 1;
}