#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <iomanip>
#include <stdio.h>
#include "base/abc/abc.h"
#include "bdd/cudd/cudd.h"
#include "bdd/cudd/cuddInt.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/cnf/cnf.h"
extern "C" {
  Aig_Man_t* Abc_NtkToDar(Abc_Ntk_t* pNtk, int fExors, int fRegisters);
  Cnf_Dat_t* Cnf_DataDup(Cnf_Dat_t* p);
  void Abc_NtkShow(Abc_Ntk_t* pNtk0, int fGateNames, int fSeq, int fUseReverse, int fKeepDot);
}

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimBdd(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSymBdd(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSymSat(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSymAll(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc)
{
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", Lsv_CommandSimBdd, 1);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig", Lsv_CommandSimAig, 2);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_bdd", Lsv_CommandSymBdd, 3);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_sat", Lsv_CommandSymSat, 4);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_all", Lsv_CommandSymAll, 5);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = { init, destroy };

struct PackageRegistrationManager
{
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

void Lsv_NtkPrintNodes(Abc_Ntk_t* pNtk)
{
  Abc_Obj_t* pObj;
  int i;
  Abc_NtkForEachObj(pNtk, pObj, i)
  {
    printf("Object Id = %d, name = %s\t%d\t%d\n", Abc_ObjId(pObj), Abc_ObjName(pObj), pObj->fCompl0, pObj->fCompl1);
    Abc_Obj_t* pFanin;
    int j;
    Abc_ObjForEachFanin(pObj, pFanin, j)
    {
      printf("  Fanin-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanin),
        Abc_ObjName(pFanin));
    }
    if (Abc_NtkHasSop(pNtk))
    {
      printf("The SOP of this node:\n%s", (char*)pObj->pData);
    }
  }
}

int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv)
{
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c;
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF)
  {
    switch (c)
    {
    case 'h':
      goto usage;
    default:
      goto usage;
    }
  }
  if (!pNtk)
  {
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
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  Nm_Man_t* pManName = pNtk->pManName;
  // DdManager* pManFunc = (DdManager*)(pNtk->pManFunc);
  int c;
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF)
  {
    switch (c)
    {
    case 'h':
      goto usage;
    default:
      goto usage;
    }
  }
  const char* pattern;
  int patternLength;
  if (argc == 2)
  {
    pattern = argv[1];
    patternLength = strlen(pattern);
  }
  // printf("%s: %d\n", pattern, patternLength);
  if (!pNtk)
  {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  // Lsv_NtkPrintNodes(pNtk);
  if (!(Abc_NtkHasBdd(pNtk)))
  {
    printf("BDD is not found\n");
  }
  else
  {
    int i, j;
    Abc_Obj_t* pPi;
    Abc_NtkForEachPi(pNtk, pPi, i)
    {
      pPi->iTemp = pattern[i] - '0';
    }
    pPi = nullptr;

    Abc_Obj_t* pPo;
    Abc_Obj_t* pNode;
    Abc_Obj_t* pFanin;
    DdManager* pDDman = (DdManager*)(pNtk->pManFunc);
    i = 0;
    Abc_NtkForEachPo(pNtk, pPo, i)
    {
      pNode = Abc_ObjFanin(pPo, 0);
      j = 0;
      DdNode* poDDnode = (DdNode*)(pNode->pData);
      Abc_ObjForEachFanin(pNode, pFanin, j)
      {
        // printf("%s: %d\n", Nm_ManFindNameById(pManName, pFanin->Id), pFanin->iTemp);
        if (pFanin->iTemp)
        {
          poDDnode = Cudd_Cofactor(pDDman, poDDnode, Cudd_bddIthVar(pDDman, j));
        }
        else
        {
          poDDnode = Cudd_Cofactor(pDDman, poDDnode, Cudd_Not(Cudd_bddIthVar(pDDman, j)));
        }
      }
      int poval;
      if (poDDnode == Cudd_ReadLogicZero(pDDman))
      {
        poval = 0;
      }
      else if (poDDnode == Cudd_ReadOne(pDDman))
      {
        poval = 1;
      }
      else
      {
        printf("sth wrong\n");
        exit(1);
      }
      printf("%s: %d\n", Nm_ManFindNameById(pManName, pPo->Id), poval);
    }
  }
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sim_bdd [-h]\n");
  Abc_Print(-2, "\t        do simulations for a given BDD and an input pattern.\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv)
{
  std::unordered_map<Abc_Obj_t*, std::vector<int>> pPo2pat;
  std::unordered_map<Abc_Obj_t*, int> pObj2val;
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c;
  Abc_Obj_t* pObj = nullptr;
  Abc_Obj_t* pPi = nullptr;
  Abc_Obj_t* pPo = nullptr;
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF)
  {
    switch (c)
    {
    case 'h':
      goto usage;
    default:
      goto usage;
    }
  }
  if (!pNtk)
  {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }

  FILE* fin;
  char line[1000];
  // int patterns;
  if (!(Abc_NtkHasAig(pNtk)))
  {
    std::cout << "Aig is not found\n";
  }
  else
  {
    const char* inputFile;
    if (argc != 2)
    {
      goto usage;
    }

    // input file name
    inputFile = argv[1];
    // index
    int i;
    // initialize the value of every obj(pi node po)
    i = 0;
    Abc_NtkForEachObj(pNtk, pObj, i)
    {
      pObj2val[pObj] = 0;
      if (Abc_AigNodeIsConst(pObj)) {
        pObj2val[pObj] = -1;
      }
    }
    // initialize the value of every ptr of pos
    i = 0;
    Abc_NtkForEachPo(pNtk, pPo, i)
    {
      if (i == 0)
      {
        continue;
      }
      pPo2pat[pPo] = std::vector<int>();
    }

    fin = fopen(inputFile, "r");
    if (fin == NULL)
    {
      perror("sth wrong opening file");
      return 1;
    }

    int patternCount = 0;
    while (fgets(line, sizeof(line), fin) != NULL)
    {
      ++patternCount;
      // read in the PIs until pattern count reach 32
      i = 0;
      Abc_NtkForEachPi(pNtk, pPi, i)
      {
        if (line[i] == '1')
        {
          int mask = 1 << ((patternCount - 1));
          pObj2val[pPi] |= mask;
        }
      }

      // if pattern count reach 32, do simulation
      if ((patternCount % 32) == 0 && (patternCount != 0))
      {

        // calculate the simulated 32-bit result for each node
        i = 0;
        Abc_NtkForEachNode(pNtk, pObj, i)
        {
          if (pObj->fCompl0 && pObj->fCompl1)
          {
            pObj2val[pObj] = (~(pObj2val[Abc_ObjFanin0(pObj)]) & ~(pObj2val[Abc_ObjFanin1(pObj)]));
          }
          else if (pObj->fCompl0)
          {
            pObj2val[pObj] = (~(pObj2val[Abc_ObjFanin0(pObj)]) & pObj2val[Abc_ObjFanin1(pObj)]);
          }
          else if (pObj->fCompl1)
          {
            pObj2val[pObj] = (pObj2val[Abc_ObjFanin0(pObj)] & ~(pObj2val[Abc_ObjFanin1(pObj)]));
          }
          else
          {
            pObj2val[pObj] = (pObj2val[Abc_ObjFanin0(pObj)] & pObj2val[Abc_ObjFanin1(pObj)]);
          }
        }

        // write the po value into map
        i = 0;
        Abc_NtkForEachPo(pNtk, pPo, i)
        {
          pObj2val[pPo] = (pPo->fCompl0) ? ~(pObj2val[Abc_ObjFanin0(pPo)]) : pObj2val[Abc_ObjFanin0(pPo)];
          for (int index = 0; index < 32; ++index)
          {
            int mask = 1 << index;
            if ((pObj2val[pPo]) & mask)
            {
              pPo2pat[pPo].push_back(1);
            }
            else
            {
              pPo2pat[pPo].push_back(0);
            }
          }
        }

        // reinitialize all the value of pObj
        i = 0;
        Abc_NtkForEachObj(pNtk, pObj, i)
        {
          pObj2val[pObj] = 0;
          if (Abc_AigNodeIsConst(pObj)) {
            pObj2val[pObj] = -1;
          }
        }
      }
    }

    // if there is still some pattern left not simulated after EOF
    if ((patternCount % 32) != 0)
    {
      int restCount = patternCount % 32;

      // calculate the simulated 32-bit result for each node
      i = 0;
      Abc_NtkForEachNode(pNtk, pObj, i)
      {
        if (pObj->fCompl0 && pObj->fCompl1)
        {
          pObj2val[pObj] = (~(pObj2val[Abc_ObjFanin0(pObj)]) & ~(pObj2val[Abc_ObjFanin1(pObj)]));
        }
        else if (pObj->fCompl0)
        {
          pObj2val[pObj] = (~(pObj2val[Abc_ObjFanin0(pObj)]) & pObj2val[Abc_ObjFanin1(pObj)]);
        }
        else if (pObj->fCompl1)
        {
          pObj2val[pObj] = (pObj2val[Abc_ObjFanin0(pObj)] & ~(pObj2val[Abc_ObjFanin1(pObj)]));
        }
        else
        {
          pObj2val[pObj] = (pObj2val[Abc_ObjFanin0(pObj)] & pObj2val[Abc_ObjFanin1(pObj)]);
        }
      }

      // write the po value into map
      i = 0;
      Abc_NtkForEachPo(pNtk, pPo, i)
      {
        pObj2val[pPo] = (pPo->fCompl0) ? ~(pObj2val[Abc_ObjFanin0(pPo)]) : pObj2val[Abc_ObjFanin0(pPo)];
        for (int index = 0; index < restCount; ++index)
        {
          int mask = 1 << index;
          if ((pObj2val[pPo]) & mask)
          {
            pPo2pat[pPo].push_back(1);
          }
          else
          {
            pPo2pat[pPo].push_back(0);
          }
        }
      }
    }
    // write the po value into pTemp
    i = 0;
    Abc_NtkForEachPo(pNtk, pPo, i)
    {
      if (!(pPo2pat[pPo].empty()))
      {
        std::string result = "";
        for (int pat : pPo2pat[pPo])
        {
          result += std::to_string(pat);
        }
        std::cout << Nm_ManFindNameById(pNtk->pManName, Abc_NtkPo(pNtk, i)->Id) << ": " << result << "\n";
      }
    }

    fclose(fin);
  }
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sim_aig [-h]\n");
  Abc_Print(-2, "\t        do 32-bit parallel simulations for a given AIG and some input patterns\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

int Lsv_CommandSymBdd(Abc_Frame_t* pAbc, int argc, char** argv)
{
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  Nm_Man_t* pManName = pNtk->pManName;
  int c;
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF)
  {
    switch (c)
    {
    case 'h':
      goto usage;
    default:
      goto usage;
    }
  }

  int outputIndex, input1index, input2index;
  if (argc == 4)
  {
    outputIndex = std::stoi(argv[1]);
    input1index = std::stoi(argv[2]);
    input2index = std::stoi(argv[3]);
  }
  else {
    goto usage;
  }

  if (!pNtk)
  {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  if (!(Abc_NtkHasBdd(pNtk)))
  {
    printf("BDD is not found\n");
  }
  else
  {
    bool symmetric = true;
    Abc_Obj_t* pPo = Abc_NtkPo(pNtk, outputIndex);
    Abc_Obj_t* pNode = Abc_ObjFanin0(pPo);
    Abc_Obj_t* pFanin = nullptr;
    DdManager* pDDman = (DdManager*)(pNtk->pManFunc);
    DdNode* poDDNode = (DdNode*)(pNode->pData);

    DdNode* pBDD1 = poDDNode;
    DdNode* pBDD2 = poDDNode;
    int numberOfCIs = Abc_NtkCiNum(pNtk);
    std::string input1name = Nm_ManFindNameById(pNtk->pManName, Abc_NtkCi(pNtk, input1index)->Id);
    std::string input2name = Nm_ManFindNameById(pNtk->pManName, Abc_NtkCi(pNtk, input2index)->Id);
    std::unordered_map<int, std::string> faninindex2name;
    std::unordered_map<std::string, int> name2originalIndex;
    if(input1index == input2index){
      std::cout << symmetric << "\n";
      return 0;
    }
    int index;
    index = 0;
    Abc_NtkForEachPi(pNtk, pFanin, index)
    {
      name2originalIndex.emplace(Nm_ManFindNameById(pNtk->pManName, pFanin->Id), index);
    }
    index = 0;
    Abc_ObjForEachFanin(pNode, pFanin, index)
    {
      faninindex2name.emplace(index, Nm_ManFindNameById(pNtk->pManName, pFanin->Id));
    }
    // std::cout << "inputindex: " << input1index << " " << input2index << "\toutputindex: " << outputIndex << "\n";

    index = 0;
    Abc_ObjForEachFanin(pNode, pFanin, index)
    {
      if (input1name == faninindex2name[index]) {
        pBDD1 = Cudd_Cofactor(pDDman, pBDD1, Cudd_Not(Cudd_bddIthVar(pDDman, index)));
        Cudd_Ref(pBDD1);
        pBDD2 = Cudd_Cofactor(pDDman, pBDD2, Cudd_bddIthVar(pDDman, index));
        Cudd_Ref(pBDD2);
      }
      else if (input2name == faninindex2name[index])
      {
        pBDD1 = Cudd_Cofactor(pDDman, pBDD1, Cudd_bddIthVar(pDDman, index));
        Cudd_Ref(pBDD1);
        pBDD2 = Cudd_Cofactor(pDDman, pBDD2, Cudd_Not(Cudd_bddIthVar(pDDman, index)));
        Cudd_Ref(pBDD2);
      }
    }

    if (pBDD1 == pBDD2) {
      std::cout << "symmetric\n";
    }
    else {
      int numberOfPIs = Abc_NtkCiNum(pNtk);
      std::vector<int> assymetricPattern1(numberOfPIs);
      std::vector<int> assymetricPattern2(numberOfPIs);
      std::unordered_map<int, int> record;
      DdNode* pXOR = Cudd_bddXor(pDDman, pBDD1, pBDD2);
      Cudd_Ref(pXOR);

      Abc_ObjForEachFanin(pNode, pFanin, index)
      {
        int originalIndex = name2originalIndex[faninindex2name[index]];
        if (Cudd_ReadLogicZero(pDDman) != Cudd_Cofactor(pDDman, pXOR, Cudd_Not(Cudd_bddIthVar(pDDman, index))))
        {
          pXOR = Cudd_Cofactor(pDDman, pXOR, Cudd_Not(Cudd_bddIthVar(pDDman, index)));
          record.emplace(originalIndex, 0);
        }
        else
        {
          pXOR = Cudd_Cofactor(pDDman, pXOR, Cudd_bddIthVar(pDDman, index));
          record.emplace(originalIndex, 1);
        }
        Cudd_Ref(pXOR);
      }
      Cudd_RecursiveDeref(pDDman, pXOR);

      for (auto it : record)
      {
        assymetricPattern1[it.first] = it.second;
        assymetricPattern2[it.first] = it.second;
      }
      assymetricPattern1[input1index] = 0;
      assymetricPattern1[input2index] = 1;
      assymetricPattern2[input1index] = 1;
      assymetricPattern2[input2index] = 0;

      std::string pattern1 = "";
      std::string pattern2 = "";
      for (int j = 0;j < numberOfPIs;++j) {
        if (assymetricPattern1[j] == 0) {
          pattern1 += '0';
        }
        else {
          pattern1 += '1';
        }
        if (assymetricPattern2[j] == 0) {
          pattern2 += '0';
        }
        else {
          pattern2 += '1';
        }
      }
      std::cout << "asymmetric\n";
      std::cout << pattern1 << "\n";
      std::cout << pattern2 << "\n";
    }
    Cudd_RecursiveDeref(pDDman, pBDD1);
    Cudd_RecursiveDeref(pDDman, pBDD2);
    return 0;
  }

usage:
  Abc_Print(-2, "usage: lsv_sym_bdd [-h]\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

int Lsv_CommandSymSat(Abc_Frame_t* pAbc, int argc, char** argv)
{
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c;
  Abc_Obj_t* pObj = nullptr;
  Abc_Obj_t* pPi = nullptr;
  Abc_Obj_t* pPo = nullptr;
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF)
  {
    switch (c)
    {
    case 'h':
      goto usage;
    default:
      goto usage;
    }
  }

  int outputIndex, input1index, input2index;
  if (argc == 4)
  {
    outputIndex = std::stoi(argv[1]);
    input1index = std::stoi(argv[2]);
    input2index = std::stoi(argv[3]);
  }
  else {
    goto usage;
  }

  if (!pNtk)
  {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }

  if (!(Abc_NtkHasAig(pNtk)))
  {
    std::cout << "Aig is not found\n";
    return 1;
  }
  else
  {
    bool symmetric = true;
    int input1AigID, input2AigID, outputAigID;
    Aig_Obj_t* pAigObj;
    Abc_Obj_t* pAbcObj, * pNode;
    std::string input1name = Nm_ManFindNameById(pNtk->pManName, Abc_NtkCi(pNtk, input1index)->Id);
    std::string input2name = Nm_ManFindNameById(pNtk->pManName, Abc_NtkCi(pNtk, input2index)->Id);
    std::unordered_map<int, std::string> aigindex2name;
    std::unordered_map<std::string, int> name2originalIndex;
    lbool satisfiable = l_False;
    if(input1index == input2index){
      std::cout << "symmetric" << "\n";
      return 0;
    }
    int index;
    index = 0;
    int numberOfPIs = Abc_NtkCiNum(pNtk);
    Abc_Obj_t* pPo = Abc_NtkPo(pNtk, outputIndex);
    pNode = Abc_ObjFanin0(pPo);
    index = 0;
    Abc_NtkForEachCi(pNtk, pAbcObj, index)
    {
      name2originalIndex.emplace(Nm_ManFindNameById(pNtk->pManName, pAbcObj->Id), index);
    }
    Abc_Ntk_t* pConeNtk = Abc_NtkCreateCone(pNtk, pNode, Abc_ObjName(pPo), 1);
    index = 0;
    Aig_Man_t* pAigman = Abc_NtkToDar(pConeNtk, 0, 0);
    Aig_ManForEachCi(pAigman, pAigObj, index)
    {
      aigindex2name.emplace(index, Nm_ManFindNameById(pConeNtk->pManName, pAigObj->Id));
    }
    lit Lits[2];
    sat_solver* pSatSolver = sat_solver_new();
    Cnf_Dat_t* pCnfDat = Cnf_Derive(pAigman, 1);
    Cnf_DataWriteIntoSolverInt(pSatSolver, pCnfDat, 1, 0);
    Cnf_DataLift(pCnfDat, pCnfDat->nVars);

    int originalSatSolverVarNum = pCnfDat->nVars;
    Cnf_DataWriteIntoSolverInt(pSatSolver, pCnfDat, 1, 0);

    // std::cout << "inputindex: " << input1index << " " << input2index << "\toutputindex: " << outputIndex << "\n";
    index = 0;
    Aig_ManForEachCi(pAigman, pAigObj, index)
    {
      if (input1name == aigindex2name[index]) {
        input1AigID = pAigObj->Id;
      }
      else if (input2name == aigindex2name[index]) {
        input2AigID = pAigObj->Id;
      }
      else {
        Lits[0] = toLitCond(pCnfDat->pVarNums[pAigObj->Id] - originalSatSolverVarNum, 0);
        Lits[1] = toLitCond(pCnfDat->pVarNums[pAigObj->Id], 1);
        if (!sat_solver_addclause(pSatSolver, Lits, Lits + 2)) { goto skip; };

        Lits[0] = toLitCond(pCnfDat->pVarNums[pAigObj->Id] - originalSatSolverVarNum, 1);
        Lits[1] = toLitCond(pCnfDat->pVarNums[pAigObj->Id], 0);
        if (!sat_solver_addclause(pSatSolver, Lits, Lits + 2)) { goto skip; };
      }
    }
    Lits[0] = toLitCond(pCnfDat->pVarNums[input2AigID] - originalSatSolverVarNum, 0);
    Lits[1] = toLitCond(pCnfDat->pVarNums[input1AigID], 1);
    if (!sat_solver_addclause(pSatSolver, Lits, Lits + 2)) { goto skip; };
    Lits[0] = toLitCond(pCnfDat->pVarNums[input2AigID] - originalSatSolverVarNum, 1);
    Lits[1] = toLitCond(pCnfDat->pVarNums[input1AigID], 0);
    if (!sat_solver_addclause(pSatSolver, Lits, Lits + 2)) { goto skip; };

    Lits[0] = toLitCond(pCnfDat->pVarNums[input1AigID] - originalSatSolverVarNum, 0);
    Lits[1] = toLitCond(pCnfDat->pVarNums[input2AigID], 1);
    if (!sat_solver_addclause(pSatSolver, Lits, Lits + 2)) { goto skip; };
    Lits[0] = toLitCond(pCnfDat->pVarNums[input1AigID] - originalSatSolverVarNum, 1);
    Lits[1] = toLitCond(pCnfDat->pVarNums[input2AigID], 0);
    if (!sat_solver_addclause(pSatSolver, Lits, Lits + 2)) { goto skip; };

    outputAigID = (Aig_ManCo(pAigman, 0))->Id;
    Lits[0] = toLitCond(pCnfDat->pVarNums[outputAigID] - originalSatSolverVarNum, 0);
    Lits[1] = toLitCond(pCnfDat->pVarNums[outputAigID], 0);
    if (!sat_solver_addclause(pSatSolver, Lits, Lits + 2)) { goto skip; };
    Lits[0] = toLitCond(pCnfDat->pVarNums[outputAigID] - originalSatSolverVarNum, 1);
    Lits[1] = toLitCond(pCnfDat->pVarNums[outputAigID], 1);
    if (!sat_solver_addclause(pSatSolver, Lits, Lits + 2)) { goto skip; };

    satisfiable = sat_solver_solve(pSatSolver, nullptr, nullptr, 0, 0, 0, 0);
  skip:
    if (satisfiable == l_True) {
      std::vector<int> assymetricPattern1(numberOfPIs);
      std::vector<int> assymetricPattern2(numberOfPIs);
      std::string pattern1 = "";
      std::string pattern2 = "";
      Aig_ManForEachCi(pAigman, pAigObj, index)
      {
        int a = sat_solver_var_value(pSatSolver, pCnfDat->pVarNums[pAigObj->Id] - originalSatSolverVarNum);
        int b = sat_solver_var_value(pSatSolver, pCnfDat->pVarNums[pAigObj->Id]);
        assymetricPattern1[name2originalIndex[aigindex2name[index]]] = a;
        assymetricPattern2[name2originalIndex[aigindex2name[index]]] = b;
      }
      for (int j = 0;j < numberOfPIs;++j) {
        if (assymetricPattern1[j] == 0) {
          pattern1 += '0';
        }
        else {
          pattern1 += '1';
        }
        if (assymetricPattern2[j] == 0) {
          pattern2 += '0';
        }
        else {
          pattern2 += '1';
        }
      }
      std::cout << "asymmetric\n";
      std::cout << pattern1 << "\n";
      std::cout << pattern2 << "\n";
    }
    else if (satisfiable == l_False) {
      std::cout << "symmetric\n";
    }
    else {
      printf("undefined\n");
    }
    sat_solver_delete(pSatSolver);
    return 0;
  }

usage:
  Abc_Print(-2, "usage: lsv_sym_sat [-h]\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

int Lsv_CommandSymAll(Abc_Frame_t* pAbc, int argc, char** argv)
{
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c;
  Abc_Obj_t* pObj = nullptr;
  Abc_Obj_t* pPi = nullptr;
  Abc_Obj_t* pPo = nullptr;
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF)
  {
    switch (c)
    {
    case 'h':
      goto usage;
    default:
      goto usage;
    }
  }

  int outputIndex, input1index, input2index;
  if (argc == 2)
  {
    outputIndex = std::stoi(argv[1]);
  }
  else {
    goto usage;
  }

  if (!pNtk)
  {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }

  if (!(Abc_NtkHasAig(pNtk)))
  {
    std::cout << "Aig is not found\n";
    return 1;
  }
  else
  {
    bool symmetric;
    int input1AigID, input2AigID, outputAigID;
    int index;
    lit Lits[4];
    Aig_Obj_t* pAigObj;
    Abc_Obj_t* pAbcObj, * pAbcNode;
    std::unordered_map<int, std::string> aigindex2name;
    std::unordered_map<std::string, int> name2originalIndex;
    int numberOfPIs = Abc_NtkCiNum(pNtk);
    Abc_Obj_t* pPo = Abc_NtkPo(pNtk, outputIndex);
    pAbcNode = Abc_ObjFanin0(pPo);
    index = 0;
    Abc_NtkForEachCi(pNtk, pAbcObj, index)
    {
      name2originalIndex.emplace(Nm_ManFindNameById(pNtk->pManName, pAbcObj->Id), index);
    }
    Abc_Ntk_t* pConeNtk = Abc_NtkCreateCone(pNtk, pAbcNode, Abc_ObjName(pPo), 1);
    index = 0;
    Aig_Man_t* pAigman = Abc_NtkToDar(pConeNtk, 0, 0);
    Aig_ManForEachCi(pAigman, pAigObj, index)
    {
      aigindex2name.emplace(index, Nm_ManFindNameById(pConeNtk->pManName, pAigObj->Id));
    }
    sat_solver* pSatSolver = sat_solver_new();
    Cnf_Dat_t* pCnfDat = Cnf_Derive(pAigman, 1);
    Cnf_DataWriteIntoSolverInt(pSatSolver, pCnfDat, 1, 0);
    Cnf_DataLift(pCnfDat, pCnfDat->nVars);

    int originalSatSolverVarNum = pCnfDat->nVars;
    Cnf_DataWriteIntoSolverInt(pSatSolver, pCnfDat, 1, 0);

    Cnf_DataLift(pCnfDat, originalSatSolverVarNum);
    // Cnf_DataWriteIntoSolverInt(pSatSolver, pCnfDat, 1, 0);
    index = 0;
    lbool satisfiable = l_Undef;
    Aig_ManForEachCi(pAigman, pAigObj, index)
    {
      Lits[0] = toLitCond(pCnfDat->pVarNums[pAigObj->Id] - 2 * originalSatSolverVarNum, 0);
      Lits[1] = toLitCond(pCnfDat->pVarNums[pAigObj->Id] - originalSatSolverVarNum, 1);
      Lits[2] = toLitCond(pCnfDat->pVarNums[pAigObj->Id], 0);
      if (!sat_solver_addclause(pSatSolver, Lits, Lits + 3)) { satisfiable = l_False; };

      Lits[0] = toLitCond(pCnfDat->pVarNums[pAigObj->Id] - 2 * originalSatSolverVarNum, 1);
      Lits[1] = toLitCond(pCnfDat->pVarNums[pAigObj->Id] - originalSatSolverVarNum, 0);
      Lits[2] = toLitCond(pCnfDat->pVarNums[pAigObj->Id], 0);
      if (!sat_solver_addclause(pSatSolver, Lits, Lits + 3)) { satisfiable = l_False; };
    }

    index = 0;
    Aig_ManForEachCi(pAigman, pAigObj, index)
    {
      if ((index + 1) == numberOfPIs) {
        continue;
      }
      int index2 = 0;
      Aig_Obj_t* pAigObj2 = nullptr;
      Aig_ManForEachCi(pAigman, pAigObj2, index2)
      {
        if (index2 <= index) {
          continue;
        }
        Lits[0] = toLitCond(pCnfDat->pVarNums[pAigObj->Id] - 2 * originalSatSolverVarNum, 0);
        Lits[1] = toLitCond(pCnfDat->pVarNums[pAigObj2->Id] - originalSatSolverVarNum, 1);
        Lits[2] = toLitCond(pCnfDat->pVarNums[pAigObj->Id], 1);
        Lits[3] = toLitCond(pCnfDat->pVarNums[pAigObj2->Id], 1);
        if (!sat_solver_addclause(pSatSolver, Lits, Lits + 4)) { satisfiable = l_False; };

        Lits[0] = toLitCond(pCnfDat->pVarNums[pAigObj->Id] - 2 * originalSatSolverVarNum, 1);
        Lits[1] = toLitCond(pCnfDat->pVarNums[pAigObj2->Id] - originalSatSolverVarNum, 0);
        Lits[2] = toLitCond(pCnfDat->pVarNums[pAigObj->Id], 1);
        Lits[3] = toLitCond(pCnfDat->pVarNums[pAigObj2->Id], 1);
        if (!sat_solver_addclause(pSatSolver, Lits, Lits + 4)) { satisfiable = l_False; };

        Lits[0] = toLitCond(pCnfDat->pVarNums[pAigObj2->Id] - 2 * originalSatSolverVarNum, 0);
        Lits[1] = toLitCond(pCnfDat->pVarNums[pAigObj->Id] - originalSatSolverVarNum, 1);
        Lits[2] = toLitCond(pCnfDat->pVarNums[pAigObj->Id], 1);
        Lits[3] = toLitCond(pCnfDat->pVarNums[pAigObj2->Id], 1);
        if (!sat_solver_addclause(pSatSolver, Lits, Lits + 4)) { satisfiable = l_False; };

        Lits[0] = toLitCond(pCnfDat->pVarNums[pAigObj2->Id] - 2 * originalSatSolverVarNum, 1);
        Lits[1] = toLitCond(pCnfDat->pVarNums[pAigObj->Id] - originalSatSolverVarNum, 0);
        Lits[2] = toLitCond(pCnfDat->pVarNums[pAigObj->Id], 1);
        Lits[3] = toLitCond(pCnfDat->pVarNums[pAigObj2->Id], 1);
        if (!sat_solver_addclause(pSatSolver, Lits, Lits + 4)) { satisfiable = l_False; };
      }
    }

    outputAigID = (Aig_ManCo(pAigman, 0))->Id;
    Lits[0] = toLitCond(pCnfDat->pVarNums[outputAigID] - 2 * originalSatSolverVarNum, 0);
    Lits[1] = toLitCond(pCnfDat->pVarNums[outputAigID] - originalSatSolverVarNum, 0);
    if (!sat_solver_addclause(pSatSolver, Lits, Lits + 2)) { satisfiable = l_False; };
    Lits[0] = toLitCond(pCnfDat->pVarNums[outputAigID] - 2 * originalSatSolverVarNum, 1);
    Lits[1] = toLitCond(pCnfDat->pVarNums[outputAigID] - originalSatSolverVarNum, 1);
    if (!sat_solver_addclause(pSatSolver, Lits, Lits + 2)) { satisfiable = l_False; };

    std::unordered_map<int, int> groups;
    for(int i = 0;i<numberOfPIs;++i){
      groups[i] = i;
    }
    std::vector<std::pair<int, int>> symmetricPairs;
    if (satisfiable == l_False) {
      for (int i = 0;i < numberOfPIs - 1;++i) {
        for (int j = i + 1;j < numberOfPIs;++j) {
          symmetricPairs.emplace_back(i, j);
        }
      }
      for (auto it : symmetricPairs) {
        std::cout << it.first << " " << it.second << "\n";
      }
      return 0;
    }

    index = 0;
    lit assumptions[numberOfPIs];
    Aig_ManForEachCi(pAigman, pAigObj, index)
    {
      Aig_Obj_t* pAigObj2 = nullptr;
      if ((index + 1) == numberOfPIs) {
        continue;
      }
      int index2 = 0;
      Aig_ManForEachCi(pAigman, pAigObj2, index2)
      {
        if (index2 <= index) {
          continue;
        }

        int temp1 = index;
        int temp2 = index2;
        int flag = false;
        while ((temp1 != groups[temp1]) && (temp2 != groups[temp2]))
        {
          if(temp1 == temp2){
            groups[index2] = groups[temp1];
            groups[index] = groups[temp1];
            symmetricPairs.emplace_back(name2originalIndex[aigindex2name[index]], name2originalIndex[aigindex2name[index2]]);
            flag = true;
            break;
          }
          else{
            temp1 = groups[temp1];
            temp2 = groups[temp2];
          }
        }
        if(flag){
          continue;
        }
        
        int index3 = 0;
        Aig_Obj_t* pAigObj3 = nullptr;
        Aig_ManForEachCi(pAigman, pAigObj3, index3)
        {
          assumptions[index3] = toLitCond(pCnfDat->pVarNums[pAigObj3->Id], 1);
        }
        assumptions[index] = toLitCond(pCnfDat->pVarNums[pAigObj->Id], 0);
        assumptions[index2] = toLitCond(pCnfDat->pVarNums[pAigObj2->Id], 0);
        satisfiable = sat_solver_solve(pSatSolver, assumptions, assumptions + numberOfPIs, 0, 0, 0, 0);
        if (satisfiable == l_False) {
          groups[index2] = groups[index];
          symmetricPairs.emplace_back(name2originalIndex[aigindex2name[index]], name2originalIndex[aigindex2name[index2]]);
        }
      }
    }
    for (auto it : symmetricPairs) {
      std::cout << it.first << " " << it.second << "\n";
    }


    return 0;
  }

usage:
  Abc_Print(-2, "usage: lsv_sym_all [-h]\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}
