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

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimBdd(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc)
{
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", Lsv_CommandSimBdd, 1);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig", Lsv_CommandSimAig, 2);
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
    std::cerr << "Aig is not found\n";
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