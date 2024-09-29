#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "opt/cut/cut.h"
#include "opt/cut/cutInt.h"
#include "opt/cut/cutList.h"
#include "aig/aig/aig.h"
#include "iostream"
#include "set"
#include "algorithm"
using namespace std;

static int Lsv_CommandPrintNodes(Abc_Frame_t *pAbc, int argc, char **argv); // command function
static int Lsv_CommandPrintCut(Abc_Frame_t *pAbc, int argc, char **argv);

void init(Abc_Frame_t *pAbc)
{
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_CommandPrintCut, 0);
} // register this new command

void destroy(Abc_Frame_t *pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager
{
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

// function implementations

void Lsv_NtkPrintNodes(Abc_Ntk_t *pNtk)
{
  Abc_Obj_t *pObj; // nodes object
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

int Lsv_CommandPrintNodes(Abc_Frame_t *pAbc, int argc, char **argv)
{
  Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc); // top level network, storing nodes and connections
  int c;
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) // pase command line option (argv is an array of strings)
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
inline void helper(unordered_map<int, vector<set<int>>> &cuts, Abc_Obj_t *pObj, int k)
{
  int pObj_id = Abc_ObjId(pObj);
  Abc_Obj_t *pFanin;
  int first_fanin = Abc_ObjId(Abc_ObjFanin0(pObj));
  queue<set<int>> cut_queue;
  for (int k = 0; k < cuts[first_fanin].size(); k++) // cuts[id] -> [{}, {}, {}]
    cut_queue.push(cuts[first_fanin][k]);
  int j;
  Abc_ObjForEachFanin(pObj, pFanin, j) // iterate through all fanin nodes
  {
    int id = Abc_ObjId(pFanin);
    if (id != first_fanin)
    {
      int s = cut_queue.size();
      int curr_level = 1;
      while (curr_level <= s)
      {
        for (int l = 0; l < cuts[id].size(); l++) // iterate through each set in the cut[id] vector
        {
          set<int> temp = {};
          set_union(cuts[id][l].begin(), cuts[id][l].end(), cut_queue.front().begin(), cut_queue.front().end(),
                    inserter(temp, temp.begin()));
          cut_queue.push(temp);
        }
        cut_queue.pop();
        curr_level++;
      }
    }
  }
  cut_queue.push({pObj_id});
  // now the resulting final vector of sets after cartesian product is stored in cut_queue (aka feasible cuts of node pObj)
  // we now update the cuts map
  set<set<int>> cut_existence_set;
  while (!cut_queue.empty())
  {
    if (cut_existence_set.find(cut_queue.front()) == cut_existence_set.end())
    { // not an existing cut
      if (cut_queue.front().size() <= k)
      {
        cuts[pObj_id].push_back(cut_queue.front());
        // cout << cuts.size() << endl;
        cut_existence_set.insert(cut_queue.front());
      }
    }
    cut_queue.pop();
  }
}
void Lsv_NtkPrintCuts(Abc_Ntk_t *pNtk, int k)
{
  unordered_map<int, vector<set<int>>> cuts;
  // walk through all non-pi nodes
  queue<set<int>> cut_queue; // initiazlize cut queue for cartesian products of sets
  // initialize cuts map
  int i;
  Abc_Obj_t *pCi;
  Abc_NtkForEachCi(pNtk, pCi, i)
  {
    int id = Abc_ObjId(pCi);
    cuts[id] = {{id}};
  }

  Abc_Obj_t *pObj;
  Abc_NtkForEachNode(pNtk, pObj, i)
  {
    helper(cuts, pObj, k);
  }
  Abc_Obj_t *pPo;
  Abc_NtkForEachPo(pNtk, pPo, i)
  {
    helper(cuts, pPo, k);
  }
  int count = 0;
  Abc_NtkForEachPi(pNtk, pObj, i)
  {
    int id = Abc_ObjId(pObj);
    for (int i = 0; i < cuts[id].size(); i++)
    { // for each cut
      cout << id << ": ";
      for (const int &node_member : cuts[id][i])
      { // print cut members
        cout << node_member << " ";
      }
      cout << endl;
      count++;
    }
  }
  Abc_NtkForEachPo(pNtk, pObj, i)
  {
    int id = Abc_ObjId(pObj);
    for (int i = 0; i < cuts[id].size(); i++)
    { // for each cut
      cout << id << ": ";
      for (const int &node_member : cuts[id][i])
      { // print cut members
        cout << node_member << " ";
      }
      cout << endl;
      count++;
    }
  }

  Abc_NtkForEachNode(pNtk, pObj, i)
  {
    int id = Abc_ObjId(pObj);
    for (int i = 0; i < cuts[id].size(); i++)
    {
      cout << id << ": ";
      for (const int &node_member : cuts[id][i])
      { // print cut members
        cout << node_member << " ";
      }
      cout << endl;
      count++;
    }
  }

  cout << "total number of cuts under constraint k = " << k << ": " << count << endl;
}
int Lsv_CommandPrintCut(Abc_Frame_t *pAbc, int argc, char **argv)
{
  Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc); // now pNtk is an aig
  int k = atoi(argv[1]);
  //
  int c;
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) // pase command line option (argv is an array of strings)
  {
    switch (c)
    {
    case 'h':
      goto usage;
    default:
      goto usage;
    }
  }
  if (!pNtk) // and check if it's an aig
  {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  if (!Abc_NtkIsStrash(pNtk))
  {
    Abc_Print(-1, "Cut computation is available only for AIGs (run \"strash\").\n");
    return 1;
  }
  Lsv_NtkPrintCuts(pNtk, k);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_print_cuts [-h]\n");
  Abc_Print(-2, "\t        prints the cuts in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}
/*
Abc_NtkForEachCi
    Abc_NtkForEachCO
        abc_objfanin0 // obj pointer
    Abc_ObjFaninId0   // obj id
*/