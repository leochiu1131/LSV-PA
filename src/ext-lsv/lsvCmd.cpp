#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "bdd/cudd/cudd.h"
#include "bdd/cudd/cuddInt.h"

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimBdd(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv);
static int CharIsSpace( char s )  { return s == ' ' || s == '\t' || s == '\r';  }

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes",   Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd"    ,   Lsv_CommandSimBdd    , 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig"    ,   Lsv_CommandSimAig    , 0);
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
