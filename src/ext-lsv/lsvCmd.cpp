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
static int Lsv_SimAig(Abc_Frame_t *pAbc, int argc, char **argv);

void init(Abc_Frame_t *pAbc)
{
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", Lsv_CommandSimBdd, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig", Lsv_SimAig, 0);
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

std::vector<std::vector<bool>> ReadFile(const char *path)
{
  std::vector<std::vector<bool>> result;

  ifstream ifs(path);
  if (!ifs)
  {
    cout << "error ee" << endl;
    return result;
  }

  for (string line; getline(ifs, line);)
  {
    std::vector<bool> row;

    for (auto c : line)
    {
      row.push_back(c - '0');
    }
    result.push_back(move(row));
  }
  return result;
}

std::vector<bool> BFS_Simulation(Abc_Ntk_t *pNtk, const std::vector<bool> &input)
{
  unordered_map<Abc_Obj_t *, bool> nodeValue;
  queue<Abc_Obj_t *> q;

  Abc_Obj_t *pObj;
  int iObj = 0;
  Abc_NtkForEachPi(pNtk, pObj, iObj)
  {
    nodeValue[pObj] = input[iObj];
    q.push(pObj);
  }

  while (!q.empty())
  {

    auto currNode = q.front();
    q.pop();

    if (nodeValue.count(currNode) == 0)
    {
      if (nodeValue.count(Abc_ObjFanin0(currNode)) == 0 || nodeValue.count(Abc_ObjFanin1(currNode)) == 0)
        continue;

      bool input0 = (Abc_ObjFaninC0(currNode)) ? !nodeValue[Abc_ObjFanin0(currNode)] : nodeValue[Abc_ObjFanin0(currNode)];
      bool input1 = (Abc_ObjFaninC1(currNode)) ? !nodeValue[Abc_ObjFanin1(currNode)] : nodeValue[Abc_ObjFanin1(currNode)];
      nodeValue[currNode] = input0 & input1;
    }

    Abc_Obj_t *pFanout;
    int i_tmp = 0;
    Abc_ObjForEachFanout(currNode, pFanout, i_tmp)
    {
      q.push(pFanout);
    }
  }

  std::vector<bool> output;
  Abc_NtkForEachPo(pNtk, pObj, iObj)
  {
    bool ans = (Abc_ObjFaninC0(pObj)) ? !nodeValue[Abc_ObjFanin0(pObj)] : nodeValue[Abc_ObjFanin0(pObj)];
    output.push_back(ans);
  }
  return output;
}

int Lsv_SimAig(Abc_Frame_t *pAbc, int argc, char **argv)
{
  auto inputVal = ReadFile(argv[1]);

  Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
  std::vector<std::vector<bool>> outputVal;
  for (const auto &input : inputVal)
  {
    outputVal.push_back(BFS_Simulation(pNtk, input));
  }

  Abc_Obj_t *pObj;
  int iObj = 0;
  Abc_NtkForEachPo(pNtk, pObj, iObj)
  {
    cout << Abc_ObjName(pObj) << ": ";
    for (const auto &output : outputVal)
    {
      cout << output[iObj];
    }
    cout << endl;
  }
  return 0;
}
