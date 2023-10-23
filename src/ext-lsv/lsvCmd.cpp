#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"

#include <iostream>
#include <vector>
#include <unordered_map>
#include <queue>
#include <fstream>
#include <string>
#include <algorithm>
#include <optional>

using namespace std;

static int Lsv_CommandPrintNodes(Abc_Frame_t *pAbc, int argc, char **argv);
static int Lsv_CommandSimBdd(Abc_Frame_t *pAbc, int argc, char **argv);
static int Lsv_CommandSimAig(Abc_Frame_t *pAbc, int argc, char **argv);

void init(Abc_Frame_t *pAbc)
{
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", Lsv_CommandSimBdd, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig", Lsv_CommandSimAig, 0);
}

void destroy(Abc_Frame_t *pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager
{
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

void Lsv_NtkPrintNodes(Abc_Ntk_t *pNtk)
{
  Abc_Obj_t *pObj;
  int i;
  Abc_NtkForEachNode(pNtk, pObj, i)
  {
    printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
    Abc_Obj_t *pFanin;
    int j;
    Abc_ObjForEachFanin(pObj, pFanin, j)
    {
      printf("  Fanin-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanin),
             Abc_ObjName(pFanin));
    }
    if (Abc_NtkHasSop(pNtk))
    {
      printf("The SOP of this node:\n%s", (char *)pObj->pData);
    }
  }
}

vector<bool> Readdata(char *array)
{
  vector<bool> ans;

  for (int i = 0; i < strlen(array) / sizeof(char); ++i)
    ans.push_back(array[i] - '0');
  return ans;
}

int Lsv_CommandSimBdd(Abc_Frame_t *pAbc, int argc, char **argv)
{
  vector<bool> pattern = Readdata(argv[1]);
  Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
  Abc_Obj_t *pPo;
  int ithPo = 0;
  Abc_NtkForEachPo(pNtk, pPo, ithPo)
  {
    Abc_Obj_t *pRoot = Abc_ObjFanin0(pPo);
    assert(Abc_NtkIsBddLogic(pRoot->pNtk));
    DdManager *dd = (DdManager *)pRoot->pNtk->pManFunc;
    DdNode *ddnode = (DdNode *)pRoot->pData;

    Abc_Obj_t *pPi;
    int ithPi = 0;
    Abc_NtkForEachPi(pNtk, pPi, ithPi)
    {
      int ithCube = Vec_IntFind(&pRoot->vFanins, pPi->Id);
      if (ithCube == -1)
        continue;

      // iteration
      DdNode *tmp = (pattern[ithPi]) ? Cudd_bddIthVar(dd, ithCube) : Cudd_Not(Cudd_bddIthVar(dd, ithCube));
      ddnode = Cudd_Cofactor(dd, ddnode, tmp);
    }

    cout << Abc_ObjName(pPo) << ": " << (ddnode == dd->one) << endl;
  }
  return 0;
}

int Lsv_CommandPrintNodes(Abc_Frame_t *pAbc, int argc, char **argv)
{
  Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
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

int setBit(int n, int bitPosition)
{
  return n | (1 << bitPosition);
}

// Clear the k-th bit of n to 0.
int clearBit(int n, int bitPosition)
{
  return n & ~(1 << bitPosition);
}

// Simulate the network behavior based on the provided gate configurations.
void simulate(Abc_Ntk_t *pNtk)
{
  int ithGate = 0;
  Abc_Obj_t *gate = 0;
  // Assuming 7 is a constant that refers to a specific type of gate.
  const int TARGET_GATE_TYPE = 7;

  Abc_NtkForEachObj(pNtk, gate, ithGate)
  {
    if (gate->Type == TARGET_GATE_TYPE)
    {
      // Use ternary operator for more concise assignment.
      int fanin0 = gate->fCompl0 ? ~(Abc_ObjFanin0(gate)->iTemp) : Abc_ObjFanin0(gate)->iTemp;
      int fanin1 = gate->fCompl1 ? ~(Abc_ObjFanin1(gate)->iTemp) : Abc_ObjFanin1(gate)->iTemp;
      gate->iTemp = (fanin0 & fanin1);
    }
  }
}

int Lsv_CommandSimAig(Abc_Frame_t *pAbc, int argc, char **argv)
{
  if (argc != 2)
  {
    printf("Error: Invalid number of arguments.\n");
    return 0;
  }
  Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
  if (!Abc_NtkIsStrash(pNtk))
  {
    printf("Error: Please strash first.\n");
    return 0;
  }
  FILE *fPtr = 0;
  fPtr = fopen(argv[1], "r");
  if (!fPtr)
  {
    printf("Error: Unable to open file.\n");
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
  char *pattern = (char *)malloc((piNum + 3) * sizeof(char));
  int ithPi = 0, ithPo = 0, count = 0;
  Abc_Obj_t *pPi = 0;
  Abc_Obj_t *pPo = 0;

  fPtr = fopen(argv[1], "r");
  while (fgets(pattern, piNum + 3, fPtr))
  {
    if (strlen(pattern) - 1 != piNum)
    {
      // printf("len = %ld\n", strlen(pattern)-1);
      printf("invalid pattern num\n");
      return 0;
    }
    Abc_NtkForEachPi(pNtk, pPi, ithPi)
    {
      if (pattern[ithPi] == '0')
        pPi->iTemp = clearBit(pPi->iTemp, count);
      else if (pattern[ithPi] == '1')
        pPi->iTemp = setBit(pPi->iTemp, count);
      else
      {
        printf("invalid pattern!!!! \n");
        printf("%s", pattern);
        return 0;
      }
    }
    realPatternNum++;
    count++;
    if (count == 32)
    {
      simulate(pNtk);
      count = 0;
      Abc_NtkForEachPo(pNtk, pPo, ithPo)
      {
        int simVal = pPo->fCompl0 == 1 ? ~(Abc_ObjFanin0(pPo)->iTemp) : Abc_ObjFanin0(pPo)->iTemp;
        for (long long i = realPatternNum - 32, j = 0; i < realPatternNum; i++, j++)
        {
          poOutput[ithPo][i] = (simVal >> j) & 1;
        }
      }
    }
  }
  if (count > 0 && count < 32)
  {
    simulate(pNtk);
    Abc_NtkForEachPo(pNtk, pPo, ithPo)
    {
      int simVal = pPo->fCompl0 == 1 ? ~(Abc_ObjFanin0(pPo)->iTemp) : Abc_ObjFanin0(pPo)->iTemp;
      for (long long i = realPatternNum - count, j = 0; i < realPatternNum; i++, j++)
      {
        poOutput[ithPo][i] = (simVal >> j) & 1;
      }
    }
  }

  Abc_NtkForEachPo(pNtk, pPo, ithPo)
  {
    printf("%s: ", Abc_ObjName(pPo));
    for (int i = 0; i < realPatternNum; i++)
    {
      printf("%d", poOutput[ithPo][i]);
    }
    printf("\n");
  }

  fclose(fPtr);
  return 1;
}
