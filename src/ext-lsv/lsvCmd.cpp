#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "bdd/cudd/cudd.h"
#include "bdd/cudd/cuddInt.h"
#include "aig/aig/aig.h"

#include <iostream>
#include <string>
#include <vector>

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


void Lsv_SimBdd(Abc_Ntk_t* pNtk, int num_input, char* inputChars) {
  Abc_Obj_t* pObj;
  Abc_Obj_t* pPo;
  int i;
  int ithPo;

  // construct the input pattern
  std::vector<int> inputPattern;
  for(int ithIn = 0; ithIn < num_input; ++ithIn){
    inputPattern.emplace_back(int(inputChars[ithIn] - '0'));
  }

  Abc_NtkForEachPo(pNtk, pPo, ithPo) {
    Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo);
    char** vNamesIn = (char**) Abc_NodeGetFaninNames(pRoot)->pArray;
    int vNamesIn_size = Abc_NodeGetFaninNames(pRoot)->nSize;

    if( !(Abc_NtkIsBddLogic(pRoot->pNtk)) ) {
      printf("This command is for BDD only!\nPlease use \"collapse\" first.\n");
      return;
    }

    // map the original input ordering to the ordering in BDD
    std::vector<int> orderMap; // -1 for non-PI input
    Abc_NtkForEachPi(pNtk, pObj, i) {
      // linear search for PI names
      orderMap.emplace_back(-1);
      for(int ithBddPi = 0; ithBddPi < vNamesIn_size; ++ithBddPi){
        if(strcmp(vNamesIn[ithBddPi], Abc_ObjName(pObj)) == 0){
          orderMap.back() = ithBddPi;
          break;
        }
      }
    }

    DdManager * dd = (DdManager *)pRoot->pNtk->pManFunc;  
    DdNode* ddnode = (DdNode *)pRoot->pData;
    DdNode* ddnode_cof = ddnode;
    for(int ithInput = 0; ithInput < num_input; ++ithInput){
      int ithVar = orderMap[ithInput];
      if(ithVar != -1){
        DdNode* var = Cudd_bddIthVar(dd, ithVar);
        if(inputPattern[ithInput] == 1){
          ddnode_cof = Cudd_Cofactor(dd, ddnode_cof, var);
        }
        else{
          ddnode_cof = Cudd_Cofactor(dd, ddnode_cof, Cudd_Not(var));
        }
      }
    }
    DdNode* zero = Cudd_Not(DD_ONE(dd));
    if (ddnode_cof == zero || ddnode_cof == DD_ZERO(dd)) {
      printf("%s: %d\n", Abc_ObjName(pPo), 0);
    }
    else{
      printf("%s: %d\n", Abc_ObjName(pPo), 1);
    }
  }
}

int Lsv_CommandSimBdd(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c;
  int nArgc;
  char **nArgv;
  int num_input = 0;
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
    switch (c) {
      case 'h':
        goto usage;
      default:
        goto usage;
    }
  }

  nArgc = argc - globalUtilOptind;
  nArgv = argv + globalUtilOptind;

  // check for input format
  if( nArgc != 1 ) {
    printf("Wrong input format!\n");
    goto usage;
  }
  for(int ithCh = 0; nArgv[0][ithCh] != '\0'; ++ithCh){
    if( nArgv[0][ithCh] != '0' && nArgv[0][ithCh] != '1'){
      printf("Wrong input format!\n");
      goto usage;
    }
    ++num_input;
  }
  if( num_input != Abc_NtkPiNum(pNtk) ) {
    printf("Wrong input number!\nPlease enter %d input(s).\n", Abc_NtkPiNum(pNtk));
    return 1;
  }

  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  Lsv_SimBdd(pNtk, num_input, nArgv[0]);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sim_bdd [-h] <input pattern>\n");
  Abc_Print(-2, "\t      : do simulations for a given BDD and an input pattern\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

// ----------------------------------------------------------

void Lsv_SimAig_32bit_parallel(Abc_Ntk_t* pNtk, int* inputPatterns, std::vector<std::vector<int>>& result){
  int i;
  Abc_Obj_t * pNode;
  int inputVal0, inputVal1;

  Abc_NtkForEachCi(pNtk, pNode, i){
    pNode->iTemp = inputPatterns[i];
  }
  Abc_NtkForEachNode(pNtk, pNode, i)
  {
    inputVal0 = (Abc_ObjFanin0(pNode)->iTemp) ^ (0xffffffff * Abc_ObjFaninC0(pNode)); // complement
    inputVal1 = (Abc_ObjFanin1(pNode)->iTemp) ^ (0xffffffff * Abc_ObjFaninC1(pNode));
    pNode->iTemp = (inputVal0 & inputVal1);
  }
  Abc_NtkForEachPo(pNtk, pNode, i){
    result[i].emplace_back(0);
    result[i].back() = (Abc_ObjFanin0(pNode)->iTemp) ^ (0xffffffff * Abc_ObjFaninC0(pNode));
  }

}

int Lsv_SimAig(Abc_Ntk_t* pNtk, char* fileName) {
  if( !(Abc_NtkIsStrash(pNtk)) ) {
    printf("This command is for AIG only!\nPlease use \"strash\" first.\n");
    return 1;
  }

  int* inputPatterns;
  int num_input = Abc_NtkPiNum(pNtk);

  // read input
  FILE *inputFile;
  inputFile = fopen(fileName, "r");
  if(inputFile == NULL)
  {
    printf("No input file!\n");   
    return 1;           
  }

  inputPatterns = (int *)calloc(num_input, sizeof(int));

  char *line;
  size_t len = 0;
  ssize_t read;
  int num_line = 0;
  std::vector<std::vector<int>> result;
  result.resize(num_input, {});

  while ((read = getline(&line, &len, inputFile)) != -1) {
    if( (num_line != 0) && (num_line % 32 == 0) ) {
      Lsv_SimAig_32bit_parallel(pNtk, inputPatterns, result);
      for(int ithIn = 0; ithIn < num_input; ++ithIn){
        inputPatterns[ithIn] = 0;
      }
    }
    for(int ithIn = 0; ithIn < num_input; ++ithIn){
      if(line[ithIn] == '\n' || line[ithIn] == '\0'){
        printf("Wrong input patterns!\nPlease check the input file.\n");
        return 1;
      }
      if(line[ithIn] == '1'){
        inputPatterns[ithIn] |= 1 << num_line;
      }
      else if(line[ithIn] != '0'){
        printf("Wrong input patterns!\nPlease check the input file.\n");
        return 1;
      }
    }
    ++num_line;
  }

  Lsv_SimAig_32bit_parallel(pNtk, inputPatterns, result);

  // print result
  int ithPo;
  Abc_Obj_t* pPo;
  // int num_remainBits = num_line % 32;
  // int num_sim = (num_remainBits == 0) ? (num_line / 32) : (num_line / 32 + 1);
  // printf("num_sim: %d\n", num_sim);
  
  // Abc_NtkForEachPo(pNtk, pPo, ithPo){
  //   printf("%s: ", Abc_ObjName(pPo));
  //   for(int ithSim = 0; ithSim < num_sim; ++ithSim){
  //     if(ithSim != (num_sim - 1)){
  //       for(int j = 0; j < 32; ++j){
  //         printf("%d", ((result[ithPo][ithSim] & (1 << j)) != 0));
  //       }
  //     }
  //     else{
  //       for(int j = 0; j < num_remainBits; ++j){
  //         printf("%d", ((result[ithPo][ithSim] & (1 << j)) != 0));
  //       }
  //     }
  //   }
  //   printf("\n");

  
  Abc_NtkForEachPo(pNtk, pPo, ithPo){
    printf("%s: ", Abc_ObjName(pPo));
    int num_remainBits = num_line;
    int ithSim = 0;
    while(num_remainBits >= 32){
      for(int ithBit = 0; ithBit < 32; ++ithBit){
        printf("%d", ((result[ithPo][ithSim] & (1 << ithBit)) != 0));
      }
      num_remainBits -= 32;
      ++ithSim;
    }
    for(int ithBit = 0; ithBit < num_remainBits; ++ithBit){
      printf("%d", ((result[ithPo][ithSim] & (1 << ithBit)) != 0));
    }
    printf("\n");
  }
  

  return 0;

}

int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c;
  int nArgc;
  char **nArgv;
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
    switch (c) {
      case 'h':
        goto usage;
      default:
        goto usage;
    }
  }

  nArgc = argc - globalUtilOptind;
  nArgv = argv + globalUtilOptind;

  // check for input format
  if( nArgc != 1 ) {
    printf("Wrong input format!\n");
    goto usage;
  }

  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }

  if( Lsv_SimAig(pNtk, nArgv[0]) ) {
    return 1;
  }

  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sim_aig [-h] <input pattern file>\n");
  Abc_Print(-2, "\t      : do 32-bit parallel simulations for a given AIG and some input patterns\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

// ----------------------------------------------------------

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
