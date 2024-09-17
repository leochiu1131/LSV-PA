#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <set>
#include <vector>
#include <iostream>

using namespace std;

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintCut(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_CommandPrintCut, 0);
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

void printCut( const set<int> &cut )
{
    for ( auto it : cut )
        cout << it << " ";
}
void printCuts( int id, set<set<int> > &cuts )
{
    for ( auto &it : cuts )
    {
        cout << id << ": ";
        printCut(it);
        cout << endl;
    }
}

void Lsv_NtkPrintCuts(Abc_Ntk_t* pNtk, int k) {

    Abc_Obj_t * pObj;
    int i, id0, id1;

    vector<set<set<int> > > vCuts;
    vCuts.resize(Abc_NtkObjNum(pNtk));

    Abc_NtkForEachObj(pNtk, pObj, i)
    {
        vCuts[i].insert({i});
        if ( Abc_ObjIsCi(pObj) ) continue;
        if ( Abc_ObjIsCo(pObj) ) continue;
        if ( Abc_ObjFaninNum(pObj) < 2 ) continue;

        id0 = Abc_ObjFaninId0(pObj);
        id1 = Abc_ObjFaninId1(pObj);

        for( auto &cut0 : vCuts[id0] )
        {
            for( auto &cut1 : vCuts[id1] )
            {
                set<int> tmp;
                for ( auto id : cut0 ) tmp.insert(id);
                for ( auto id : cut1 ) tmp.insert(id);

                if ( tmp.size() <= k ) vCuts[i].insert(tmp);
            }
        }
    }
    Abc_NtkForEachObj(pNtk, pObj, i)
        printCuts( i, vCuts[i] );
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

int Lsv_CommandPrintCut(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c, k;
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
  k = atoi(argv[globalUtilOptind]);
  Lsv_NtkPrintCuts(pNtk, k);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_printcut [-h] <k>\n");
  Abc_Print(-2, "\t        prints the nodes in the network\n");
  Abc_Print(-2, "\t<k>   : k-feasible cut\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}