#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "bdd/cudd/cudd.h"
#include "bdd/cudd/cuddInt.h"
#include "sat/cnf/cnf.h"
extern "C" {
  Aig_Man_t * Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
}

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimBdd(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSymBdd(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSymSat(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSymAll(Abc_Frame_t* pAbc, int argc, char** argv);
static int CharIsSpace( char s )  { return s == ' ' || s == '\t' || s == '\r';  }

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes",   Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd"    ,   Lsv_CommandSimBdd    , 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig"    ,   Lsv_CommandSimAig    , 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_bdd"    ,   Lsv_CommandSymBdd    , 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_sat"    ,   Lsv_CommandSymSat    , 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_all"    ,   Lsv_CommandSymAll    , 0);
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
    Abc_ObjForEachFanout(pObj, pFanin, j) {
      printf("  Fanout-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanin),
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

int Lsv_CommandSimBdd( Abc_Frame_t * pAbc, int argc, char ** argv)
{
  Abc_Ntk_t * pNtk;
  Abc_Obj_t * pPo, *pPi;
  char *inputPattern;
  int *PiIds;
  int ithPo, PiNum, ithPi, j, k;
  pNtk = Abc_FrameReadNtk(pAbc);

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

  if (argc != 2)
    goto usage;
    
  inputPattern = argv[1];
  PiNum = Abc_NtkPiNum(pNtk);
  if ((size_t)PiNum != strlen(inputPattern)) {
    printf("The length of inputPattern is different with number of PiNum(%d)\n", PiNum);
    goto usage;
  }
    
  PiIds = ABC_ALLOC(int, PiNum);
  Abc_NtkForEachPi(pNtk, pPi, ithPi)
    *(PiIds + ithPi) = Abc_ObjId(pPi);

  Abc_NtkForEachPo(pNtk, pPo, ithPo) {
    Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo);
    assert(Abc_NtkIsBddLogic(pRoot->pNtk));
    DdManager *dd = (DdManager *)pRoot->pNtk->pManFunc;
    DdNode *f = (DdNode*)pRoot->pData;
    Abc_Obj_t* pFanin;
    Abc_ObjForEachFanin(pRoot, pFanin, j) {
      for (k = 0; k < PiNum; ++k)
        if (Abc_ObjId(pFanin) == *(PiIds + k))
          break;
      int iFanin = Vec_IntFind( &pRoot->vFanins, pFanin->Id );
      DdNode *bVar = Cudd_bddIthVar(dd, iFanin);
      if (*(inputPattern + k) == '0')
        f = Cudd_Cofactor(dd, f, Cudd_Not(bVar));
      else if (*(inputPattern + k) == '1')
        f = Cudd_Cofactor(dd, f, bVar);
      else {
        fprintf(stderr, "unknown character(%c)\n", *(inputPattern + k));
        return 1;
      }
    }
    printf("%s: %s\n", Abc_ObjName(pPo), (f == DD_ONE(dd))? "1": "0");
  }
  ABC_FREE(PiIds);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sim_bdd <input_pattern> [-h]\n");
  Abc_Print(-2, "\t                  simulations for a given BDD and an input pattern\n");
  Abc_Print(-2, "\t<input_pattern> : input pattern\n");
  Abc_Print(-2, "\t-h              : print the command usage\n");
  return 1;
}


void clean(Abc_Ntk_t* pNtk) {
  Abc_Obj_t* pObj;
  int i;
  Abc_NtkForEachNode(pNtk, pObj, i) {
    // printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
    Abc_Obj_t* pFanin, *pFanout;
    int j;
    pObj->iTemp = 0;
    Abc_ObjForEachFanin(pObj, pFanin, j)
      pFanin->iTemp = 0;
    Abc_ObjForEachFanout(pObj, pFanout, j)
      pFanout->iTemp = 0;
  }
}

void simAig(Abc_Obj_t* pNode) {
  // printf("in simAig: %s\n", Abc_ObjName(pNode));
  Abc_Obj_t* pFanin;
  int ans = ~0, i;

  Abc_ObjForEachFanin(pNode, pFanin, i) {
    // printf("\t Fanin-%s, %032b\n", Abc_ObjName(pFanin), pFanin->iTemp);
    if (pFanin->iTemp == 0 && !Abc_ObjIsPi(pFanin))
      simAig(pFanin);
  }
  ans &= (pNode->fCompl0)? ~Abc_ObjFanin(pNode, 0)->iTemp: Abc_ObjFanin(pNode, 0)->iTemp;
  if (pNode->vFanins.nSize == 2)    
    ans &= (pNode->fCompl1)? ~Abc_ObjFanin(pNode, 1)->iTemp: Abc_ObjFanin(pNode, 1)->iTemp;
  pNode->iTemp = ans;
}

int Lsv_CommandSimAig( Abc_Frame_t * pAbc, int argc, char ** argv)
{
  Abc_Ntk_t* pNtk;
  Abc_Obj_t* pPo, *pPi;
  char *pFileName, *buffer, **ans;
  char **inputs;
  FILE *pFile;
  int nFileSize, inputLines = 0, ithPo, inputCount = 0, hasInput = 0, num = 0, ithPi, tmp, RetValue;
  pNtk = Abc_FrameReadNtk(pAbc);

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

  if (argc != 2)
    goto usage;

  if (!Abc_NtkIsStrash(pNtk)) {
    fprintf(stderr, "The network is not structurally hashed AIG.\n");
    return 1;
  }


  pFileName = argv[1];
  // check file exists
  pFile = fopen( pFileName, "r" );
  if ( pFile == NULL )
  {
      fprintf( stderr, "Cannot open input file \"%s\".\n", pFileName );
      return 1;
  }
  fseek(pFile, 0, SEEK_END );
  nFileSize = ftell(pFile);
  if ( nFileSize == 0 ) {
    fclose(pFile);
    fprintf( stderr, "The file is empty\n");
    return 1;
  }

  buffer = ABC_ALLOC( char, nFileSize + 1);
  memset( buffer, 0, nFileSize + 1);
  rewind( pFile );
  RetValue = fread( buffer, nFileSize, 1, pFile );
  fclose( pFile );

  for (char *c = buffer; *c; ++c) {
    // printf("c = %c: \n", *c);
    for (; CharIsSpace(*c); ++c);
    if (*c == '0' || *c == '1') {
      // printf("inputLines = %d, inputCount = %d\n", inputLines, inputCount);
      inputLines = (!hasInput)? inputLines + 1: inputLines;
      hasInput = 1;
      inputCount++;
    }
    else if (*c == '\n') {
      // printf("hasInput = %d\n", hasInput);
      if (inputCount != Abc_NtkPiNum(pNtk) && hasInput) {
        fprintf(stderr, "The length of input pattern is different with the number of primary inputs\n");
        ABC_FREE(buffer);
        return 1;
      }
      inputCount = hasInput = 0;
    }
    else {
      fprintf(stderr, "There is unknown input (%c) in the given file(%s)\n", *c, pFileName);
      ABC_FREE(buffer);
      return 1;
    }
  }

  // printf("check inputLines:\n");
  // printf("\t inputLines = %d\n", inputLines);

  inputs = ABC_ALLOC( char*, inputLines );
  for (int j = 0; j < inputLines; ++j) {
    *(inputs + j) = ABC_ALLOC( char, Abc_NtkPiNum(pNtk) + 1);
    memset(*(inputs + j), 0, Abc_NtkPiNum(pNtk) + 1);
  }
  for (char *c = buffer; *c; ++c) {
    if (*c == '0' || *c == '1') {
      inputs[num / Abc_NtkPiNum(pNtk)][num % Abc_NtkPiNum(pNtk)] = *c;
      ++num;
    }
  }
  ABC_FREE(buffer);

  // printf("checking inputs:\n");
  // for (int i = 0; i < inputLines; ++i)
  //   printf("\t %s\n", inputs[i]);


  ans = ABC_ALLOC( char*, Abc_NtkPoNum(pNtk) );
  for (int i = 0; i < Abc_NtkPoNum(pNtk); ++i){
    ans[i] = ABC_ALLOC( char, inputLines + 1);
    memset(ans[i], 0, inputLines + 1);
  }

  for (int i = 0; i < inputLines; i += 32) {
    tmp = (i + 32 > inputLines)? inputLines: i + 32;
    Abc_NtkForEachPi(pNtk, pPi, ithPi){
      pPi->iTemp = 0;
      // printf("%p\n", pPi->pTemp);
    }
    for (int j = i; j < tmp; ++j) {
      Abc_NtkForEachPi(pNtk, pPi, ithPi)
        pPi->iTemp = (pPi->iTemp << 1) + (inputs[j][ithPi] - '0');
    }
    // Abc_NtkForEachPi(pNtk, pPi, ithPi) {
    //   printf("check Pi->iTemp:\n");
    //   printf("%s: %032b\n", Abc_ObjName(pPi), pPi->iTemp);
    // }
    Abc_NtkForEachPo(pNtk, pPo, ithPo) {
      simAig(pPo);
      // printf("%032b\n", pPo->iTemp);
      for (int j = i; j < tmp; ++j) {
        ans[ithPo][tmp - j - 1 + i] = '0' + (pPo->iTemp & 1);
        pPo->iTemp >>= 1;
      }
    }
    clean(pNtk);
  }
  Abc_NtkForEachPo(pNtk, pPo, ithPo)
    printf("%s: %s\n", Abc_ObjName(pPo), ans[ithPo]);

  for (int i = 0; i < Abc_NtkPoNum(pNtk); ++i)
    ABC_FREE(ans[i]);
  ABC_FREE(ans);

  for (int j = 0; j < inputLines; ++j)
    ABC_FREE(*(inputs + j));
  ABC_FREE(inputs);

  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sim_aig <input_pattern_file> [-h]\n");
  Abc_Print(-2, "\t                       32-bit parallel simulations for a given AIG and some input patterns\n");
  Abc_Print(-2, "\t<input_pattern_file> : input pattern file\n");
  Abc_Print(-2, "\t-h                   : print the command usage\n");
  return 1;
}

int Lsv_CommandSymBdd(Abc_Frame_t * pAbc, int argc, char ** argv) {

  Abc_Ntk_t *pNtk;
  Abc_Obj_t *kthPo, *pRoot, *pFanin, *pPi;
  DdManager *dd;
  DdGen *gen;
  DdNode *f0, *f1, *f01, *f10, *check, *yk, *xi, *xj;
  char *ithPiName, *jthPiName;
  int i, j, k, poNum, piNum, ithFanin, *counter, *cube, ithPiInBdd, jthPiInBdd, ithPi;
  CUDD_VALUE_TYPE value;

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
  if (argc != 4)
    goto usage;

  pNtk = Abc_FrameReadNtk(pAbc);
  assert(Abc_NtkIsBddLogic(pNtk));

  k = atoi(argv[1]);
  i = atoi(argv[2]);
  j = atoi(argv[3]);

  // Abc_Print(1, "i = %d, j = %d, k = %d\n", i, j, k);
  poNum = Abc_NtkPoNum(pNtk);
  piNum = Abc_NtkPiNum(pNtk);
  // printf("poNum = %d, piNum = %d\n", poNum, piNum);
  if (k >= poNum) {
    Abc_Print(-2, "The given k has to be less than the number of primary outputs.\n");
    goto usage;
  }
  if (i >= piNum || j >= piNum) {
    Abc_Print(-2, "The given i and j has to be less than the number of primary inputs.\n");
    goto usage;
  }

  if (i == j) {
    printf("symmetric\n");
    return 0;
  }

  ithPiName = Abc_ObjName(Abc_NtkPi(pNtk, i));
  jthPiName = Abc_ObjName(Abc_NtkPi(pNtk, j));

  kthPo = Abc_NtkPo(pNtk, k);
  pRoot = Abc_ObjFanin0(kthPo);
  // printf("faninNum = %d\n", Abc_ObjFaninNum(pRoot));
  // printf("PiName:\n");
  // Abc_NtkForEachPi(pNtk, pFanin, ithFanin) {
  //   printf("%s ", Abc_ObjName(pFanin));
  // }
  // printf("\n");
  // printf("FaninName:\n");
  // Abc_ObjForEachFanin(pRoot, pFanin, ithFanin) {
  //   printf("%s ", Abc_ObjName(pFanin));
  // }
  // printf("\n");
  
  ithPiInBdd = -1; jthPiInBdd = -1;
  Abc_ObjForEachFanin(pRoot, pFanin, ithFanin) {
    if (Abc_ObjName(pFanin) == ithPiName)
      ithPiInBdd = ithFanin;
    else if (Abc_ObjName(pFanin) == jthPiName)
      jthPiInBdd = ithFanin;
  }

  dd = (DdManager*) pRoot->pNtk->pManFunc;
  yk = (DdNode*)pRoot->pData;
  if (ithPiInBdd == -1 && jthPiInBdd == -1) {
    printf("symmetric\n");
    return 0;
  }
  if (ithPiInBdd == -1 || jthPiInBdd == -1)
    check = (jthPiInBdd == -1)? Cudd_bddBooleanDiff(dd, yk, ithPiInBdd): Cudd_bddBooleanDiff(dd, yk, jthPiInBdd);
  else {
    xi = Cudd_bddIthVar(dd, ithPiInBdd);
    Cudd_Ref(xi);
    xj = Cudd_bddIthVar(dd, jthPiInBdd);
    Cudd_Ref(xj);
    f0 = Cudd_Cofactor(dd, yk, Cudd_Not(xi));
    Cudd_Ref(f0);
    f1 = Cudd_Cofactor(dd, yk, xi);
    Cudd_Ref(f1);
    Cudd_RecursiveDeref(dd, xi);
    f01 = Cudd_Cofactor(dd, f0, xj);
    Cudd_Ref(f01);
    f10 = Cudd_Cofactor(dd, f1, Cudd_Not(xj));
    Cudd_Ref(f10);
    Cudd_RecursiveDeref(dd, xj);
    Cudd_RecursiveDeref(dd, f0);
    Cudd_RecursiveDeref(dd, f1);

    check = Cudd_bddXor(dd, f01, f10);
    Cudd_RecursiveDeref(dd, f01);
    Cudd_RecursiveDeref(dd, f10);
  }
  Cudd_Ref(check);

  if (check == DD_ZERO(dd) || check == Cudd_Not(DD_ONE(dd))) {
    printf("symmetric\n");
    return 0;
  }
  
  printf("asymmetric\n");
  cube = ABC_ALLOC(int, Abc_ObjFaninNum(pRoot));
  counter = ABC_ALLOC(int, Abc_NtkPiNum(pNtk));
  for (int s = 0; s < Abc_NtkPiNum(pNtk); ++s) {
    counter[s] = 0;
  }
  gen = Cudd_FirstCube(dd, check, &cube, &value);

  // printf("cube\n");
  // for (int s = 0; s < Abc_ObjFaninNum(pRoot); ++s) {
  //   printf("%d", cube[s]);
  // }
  // printf("\n");

  Abc_NtkForEachPi(pNtk, pPi, ithPi) {
    Abc_ObjForEachFanin(pRoot, pFanin, ithFanin) {
      if (Abc_ObjName(pPi) == Abc_ObjName(pFanin))
        counter[ithPi] = cube[ithFanin];
    }
  }
  for (int s = 0; s < Abc_NtkPiNum(pNtk); ++s) {
    counter[s] = (counter[s] == 2)? 0:counter[s];
  }
  // printf("counter:\n");
  Cudd_RecursiveDeref(dd, check);
  for (int s = 0; s < Abc_NtkPiNum(pNtk); ++s) {
    if (s == i)
      printf("0");
    else if (s == j)
      printf("1");
    else
      printf("%d", counter[s]);
  }
  printf("\n");
  for (int s = 0; s < Abc_NtkPiNum(pNtk); ++s) {
    if (s == i)
      printf("1");
    else if (s == j)
      printf("0");
    else
      printf("%d", counter[s]);
  }
  printf("\n");

  ABC_FREE(counter);
  ABC_FREE(cube);
  return 0;

  usage:
    Abc_Print(-2, "usage: lsv_sym_bdd <k> <i> <j> [-h]\n");
    Abc_Print(-2, "\t      check kth output is symmetric in the ith and the jth variable\n");
    Abc_Print(-2, "\t<k> : kth output pin\n");
    Abc_Print(-2, "\t<i> : ith input pin\n");
    Abc_Print(-2, "\t<j> : jth input pin\n");
    Abc_Print(-2, "\t-h  : print the command usage\n");
  return 1;
}

int Lsv_CommandSymSat(Abc_Frame_t * pAbc, int argc, char ** argv) {

  Abc_Ntk_t *pNtk, *pNtkCone;
  Abc_Obj_t *kthPo, *pRoot;
  Aig_Man_t *aigNtk;
  Aig_Obj_t *pObj;
  Cnf_Dat_t *pCnfA, *pCnfB;
  sat_solver *sat;

  int i, j, k, poNum, piNum, ithObj, Lits[2], result;

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
  if (argc != 4)
    goto usage;

  pNtk = Abc_FrameReadNtk(pAbc);
  assert(Abc_NtkIsStrash(pNtk));

  k = atoi(argv[1]);
  i = atoi(argv[2]);
  j = atoi(argv[3]);

  // Abc_Print(1, "i = %d, j = %d, k = %d\n", i, j, k);
  poNum = Abc_NtkPoNum(pNtk);
  piNum = Abc_NtkPiNum(pNtk);
  // printf("poNum = %d, piNum = %d\n", poNum, piNum);
  if (k >= poNum) {
    Abc_Print(-2, "The given k has to be less than the number of primary outputs.\n");
    goto usage;
  }
  if (i >= piNum || j >= piNum) {
    Abc_Print(-2, "The given i and j has to be less than the number of primary inputs.\n");
    goto usage;
  }

  if (i == j) {
    printf("symmetric\n");
    return 0;
  }

  kthPo = Abc_NtkPo(pNtk, k);
  pRoot = Abc_ObjFanin0(kthPo);

  pNtkCone = Abc_NtkCreateCone(pNtk, pRoot, Abc_ObjName(pRoot), 1);
  
  aigNtk = Abc_NtkToDar(pNtkCone, 0, 1);
  
  sat = sat_solver_new();
  pCnfA = Cnf_Derive(aigNtk, Aig_ManCoNum(aigNtk));
  Cnf_DataWriteIntoSolverInt(sat, pCnfA, 1, 0);
  
  pCnfB = Cnf_Derive(aigNtk, Aig_ManCoNum(aigNtk));
  Cnf_DataLift(pCnfB, pCnfA->nVars);
  Cnf_DataWriteIntoSolverInt(sat, pCnfB, 1, 0);


  Aig_ManForEachCi(aigNtk, pObj, ithObj) {
    if (i == ithObj) {
      Lits[0] = toLitCond(pCnfA->pVarNums[pObj->Id], 0);
      sat_solver_addclause( sat, Lits, Lits + 1 );
      Lits[0] = toLitCond(pCnfB->pVarNums[pObj->Id], 1);
      sat_solver_addclause( sat, Lits, Lits + 1 );
    }
    else if (j == ithObj) {
      Lits[0] = toLitCond(pCnfA->pVarNums[pObj->Id], 1);
      sat_solver_addclause( sat, Lits, Lits + 1 );
      Lits[0] = toLitCond(pCnfB->pVarNums[pObj->Id], 0);
      sat_solver_addclause( sat, Lits, Lits + 1 );
    }
    else {
      Lits[0] = toLitCond(pCnfA->pVarNums[pObj->Id], 0);
      Lits[1] = toLitCond(pCnfB->pVarNums[pObj->Id], 1);
      sat_solver_addclause( sat, Lits, Lits + 2 );
      Lits[0] = toLitCond(pCnfA->pVarNums[pObj->Id], 1);
      Lits[1] = toLitCond(pCnfB->pVarNums[pObj->Id], 0);
      sat_solver_addclause( sat, Lits, Lits + 2 );
    }
  }

  Aig_ManForEachCo(aigNtk, pObj, ithObj) {
    Lits[0] = toLitCond(pCnfA->pVarNums[pObj->Id], 0);
    Lits[1] = toLitCond(pCnfB->pVarNums[pObj->Id], 0);
    sat_solver_addclause( sat, Lits, Lits + 2 );
    Lits[0] = toLitCond(pCnfA->pVarNums[pObj->Id], 1);
    Lits[1] = toLitCond(pCnfB->pVarNums[pObj->Id], 1);
    sat_solver_addclause( sat, Lits, Lits + 2 );
  }

  result = sat_solver_solve(sat, 0, 0, 0, 0, 0, 0);

  if (result == -1) {
    printf("symmetric\n");
    return 0;
  }

  printf("asymmetric\n");
  Aig_ManForEachCi(aigNtk, pObj, ithObj)
    printf("%d", sat_solver_var_value(sat, pCnfA->pVarNums[pObj->Id]));
  printf("\n");
  Aig_ManForEachCi(aigNtk, pObj, ithObj)
    printf("%d", sat_solver_var_value(sat, pCnfB->pVarNums[pObj->Id]));
  printf("\n");

  return 0;

  usage:
    Abc_Print(-2, "usage: lsv_sym_sat <k> <i> <j> [-h]\n");
    Abc_Print(-2, "\t      check kth output is symmetric in the ith and the jth variable\n");
    Abc_Print(-2, "\t<k> : kth output pin\n");
    Abc_Print(-2, "\t<i> : ith input pin\n");
    Abc_Print(-2, "\t<j> : jth input pin\n");
    Abc_Print(-2, "\t-h  : print the command usage\n");
  return 1;
}

int Lsv_CommandSymAll(Abc_Frame_t * pAbc, int argc, char ** argv) {

  Abc_Ntk_t *pNtk, *pNtkCone;
  Abc_Obj_t *kthPo, *pRoot;
  Aig_Man_t *aigNtk;
  Aig_Obj_t *pObj;
  Cnf_Dat_t *pCnfA, *pCnfB, *pCnfH;
  sat_solver *sat;

  int k, poNum, ithObj, *Lits, result;

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
  if (argc != 2)
    goto usage;

  pNtk = Abc_FrameReadNtk(pAbc);
  assert(Abc_NtkIsStrash(pNtk));

  k = atoi(argv[1]);

  // Abc_Print(1, "i = %d, j = %d, k = %d\n", i, j, k);
  poNum = Abc_NtkPoNum(pNtk);
  // printf("poNum = %d, piNum = %d\n", poNum, piNum);
  if (k >= poNum) {
    Abc_Print(-2, "The given k has to be less than the number of primary outputs.\n");
    goto usage;
  }

  kthPo = Abc_NtkPo(pNtk, k);
  pRoot = Abc_ObjFanin0(kthPo);

  pNtkCone = Abc_NtkCreateCone(pNtk, pRoot, Abc_ObjName(pRoot), 1);
  
  aigNtk = Abc_NtkToDar(pNtkCone, 0, 1);
  
  sat = sat_solver_new();
  pCnfA = Cnf_Derive(aigNtk, Aig_ManCoNum(aigNtk));
  Cnf_DataWriteIntoSolverInt(sat, pCnfA, 1, 0);
  
  pCnfB = Cnf_Derive(aigNtk, Aig_ManCoNum(aigNtk));
  Cnf_DataLift(pCnfB, pCnfA->nVars);
  Cnf_DataWriteIntoSolverInt(sat, pCnfB, 1, 0);

  pCnfH = Cnf_Derive(aigNtk, Aig_ManCoNum(aigNtk));
  Cnf_DataLift(pCnfH, pCnfB->nVars * 2);
  Cnf_DataWriteIntoSolverInt(sat, pCnfH, 1, 0);

  Lits = ABC_ALLOC(int, Aig_ManCiNum(aigNtk));

  // constraint (a)
  Aig_ManForEachCo(aigNtk, pObj, ithObj) {
    Lits[0] = toLitCond(pCnfA->pVarNums[pObj->Id], 0);
    Lits[1] = toLitCond(pCnfB->pVarNums[pObj->Id], 0);
    sat_solver_addclause( sat, Lits, Lits + 2 );
    Lits[0] = toLitCond(pCnfA->pVarNums[pObj->Id], 1);
    Lits[1] = toLitCond(pCnfB->pVarNums[pObj->Id], 1);
    sat_solver_addclause( sat, Lits, Lits + 2 );
  }

  // constraint (b)
  Aig_ManForEachCi(aigNtk, pObj, ithObj) {
    Lits[0] = toLitCond(pCnfA->pVarNums[pObj->Id], 0);
    Lits[1] = toLitCond(pCnfB->pVarNums[pObj->Id], 1);
    Lits[2] = toLitCond(pCnfH->pVarNums[pObj->Id], 1);
    sat_solver_addclause( sat, Lits, Lits + 3 );
    Lits[0] = toLitCond(pCnfA->pVarNums[pObj->Id], 1);
    Lits[1] = toLitCond(pCnfB->pVarNums[pObj->Id], 0);
    Lits[2] = toLitCond(pCnfH->pVarNums[pObj->Id], 1);
    sat_solver_addclause( sat, Lits, Lits + 3 );
  }

  // constraint (c) and (d)
  for (int i = 0; i < Aig_ManCiNum(aigNtk) - 1; ++i) {
    for (int j = i + 1; j < Aig_ManCiNum(aigNtk); ++j) {
      Lits[0] = toLitCond(pCnfA->pVarNums[Aig_ManCi(aigNtk, i)->Id], 0);
      Lits[1] = toLitCond(pCnfB->pVarNums[Aig_ManCi(aigNtk, j)->Id], 1);
      Lits[2] = toLitCond(pCnfH->pVarNums[Aig_ManCi(aigNtk, i)->Id], 0);
      Lits[3] = toLitCond(pCnfH->pVarNums[Aig_ManCi(aigNtk, j)->Id], 0);
      sat_solver_addclause( sat, Lits, Lits + 4 );

      Lits[0] = toLitCond(pCnfA->pVarNums[Aig_ManCi(aigNtk, i)->Id], 1);
      Lits[1] = toLitCond(pCnfB->pVarNums[Aig_ManCi(aigNtk, j)->Id], 0);
      Lits[2] = toLitCond(pCnfH->pVarNums[Aig_ManCi(aigNtk, i)->Id], 0);
      Lits[3] = toLitCond(pCnfH->pVarNums[Aig_ManCi(aigNtk, j)->Id], 0);
      sat_solver_addclause( sat, Lits, Lits + 4 );

      Lits[0] = toLitCond(pCnfA->pVarNums[Aig_ManCi(aigNtk, j)->Id], 0);
      Lits[1] = toLitCond(pCnfB->pVarNums[Aig_ManCi(aigNtk, i)->Id], 1);
      Lits[2] = toLitCond(pCnfH->pVarNums[Aig_ManCi(aigNtk, i)->Id], 0);
      Lits[3] = toLitCond(pCnfH->pVarNums[Aig_ManCi(aigNtk, j)->Id], 0);
      sat_solver_addclause( sat, Lits, Lits + 4 );

      Lits[0] = toLitCond(pCnfA->pVarNums[Aig_ManCi(aigNtk, j)->Id], 1);
      Lits[1] = toLitCond(pCnfB->pVarNums[Aig_ManCi(aigNtk, i)->Id], 0);
      Lits[2] = toLitCond(pCnfH->pVarNums[Aig_ManCi(aigNtk, i)->Id], 0);
      Lits[3] = toLitCond(pCnfH->pVarNums[Aig_ManCi(aigNtk, j)->Id], 0);
      sat_solver_addclause( sat, Lits, Lits + 4 );
    }
  }

  bool **symTable;
  symTable = ABC_ALLOC(bool*, Aig_ManCiNum(aigNtk));
  for (int i = 0; i < Aig_ManCiNum(aigNtk); ++i)
    symTable[i] = ABC_ALLOC(bool, Aig_ManCiNum(aigNtk));
  
  for (int i = 0; i < Aig_ManCiNum(aigNtk); ++i)
    for (int j = 0; j < Aig_ManCiNum(aigNtk); ++j)
      symTable[i][j] = false;

  for (int i = 0; i < Aig_ManCiNum(aigNtk) - 1; ++i) {
    for (int j = i + 1; j < Aig_ManCiNum(aigNtk); ++j) {
      if (symTable[i][j]) {
        printf("%d %d\n", i, j);
        continue;
      }

      Lits[0] = toLitCond(pCnfH->pVarNums[Aig_ManCi(aigNtk, i)->Id], 1);
      Lits[1] = toLitCond(pCnfH->pVarNums[Aig_ManCi(aigNtk, j)->Id], 1);
      // for (int s = 0; s < Aig_ManCiNum(aigNtk); ++s)
      //   Lits[s] = (s == i || s == j)? toLitCond(pCnfH->pVarNums[Aig_ManCi(aigNtk, s)->Id], 0): toLitCond(pCnfH->pVarNums[Aig_ManCi(aigNtk, s)->Id], 0);
      result = sat_solver_solve(sat, Lits, Lits + 2, 0, 0, 0, 0);

      if (result == -1) {
        printf("%d %d\n", i, j);
        symTable[i][j] = true;

        for (int s = i + 1; s < j; ++s) {
          if (symTable[i][s])
            symTable[s][j] = true;
        }
      }
    }
  }

  ABC_FREE(Lits);
  for (int i = 0; i < Aig_ManCiNum(aigNtk); ++i)
    ABC_FREE(symTable[i]);
  ABC_FREE(symTable);

  return 0;

  usage:
    Abc_Print(-2, "usage: lsv_sym_sat <k> [-h]\n");
    Abc_Print(-2, "\t      find all (i, j) pairs (i<j) to make the output pin yk symmetric in xi and xj.\n");
    Abc_Print(-2, "\t<k> : kth output pin\n");
    Abc_Print(-2, "\t-h  : print the command usage\n");
  return 1;
}