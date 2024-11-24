
#include "aig/aig/aig.h"
#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "bdd/cudd/cudd.h"
#include "misc/util/abc_global.h"
#include "misc/vec/vecPtr.h"
#include "sat/cnf/cnf.h"
#include "aig/gia/giaAig.h"
#include "sat/bsat/satSolver.h"
#include "base/io/ioAbc.h"

#include <set>
#include <vector>
#include <iostream>
#include <algorithm>

#define S_ZERO 0
#define S_ONE ~0

extern "C"{
extern Aig_Man_t * Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
}

using namespace std;

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintCut(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSdc(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandOdc(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_CommandPrintCut, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sdc", Lsv_CommandSdc, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_odc", Lsv_CommandOdc, 0);
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

inline void printCut( const set<int> &cut )
{
    for ( auto it : cut ) cout << it << " ";
}
inline void printCuts( int id, set<set<int> > &cuts )
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
    bool insert;

    vector<set<set<int> > > vCuts;
    vector<set<int> > vRemove;
    vCuts.resize(Abc_NtkObjNum(pNtk));

    Abc_NtkForEachObj(pNtk, pObj, i)
    {
        if ( Abc_ObjId(pObj) == 0 ) continue; 
        vCuts[i].insert({i});
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

                if ( tmp.size() <= k ) 
                {
                    if ( vCuts[i].find(tmp) != vCuts[i].end() ) continue;
                    insert = true;
                    for( auto &cut : vCuts[i] )
                    {
                        if ( cut.size() < tmp.size() )
                        {
                            if ( insert && includes(tmp.begin(), tmp.end(), cut.begin(), cut.end()) )
                                insert = false;
                        }
                        else if ( cut.size() > tmp.size() )
                        {
                            if ( includes(cut.begin(), cut.end(), tmp.begin(), tmp.end()) )
                            {
                                assert( insert == true );
                                vRemove.push_back(cut);
                            }
                        }
                    }
                    for ( auto &cut : vRemove ) vCuts[i].erase(cut);
                    vRemove.clear();

                    if ( insert ) vCuts[i].insert(tmp);
                }
            }
        }
    }
    Abc_NtkForEachCo(pNtk, pObj, i)
    {
        id0 = Abc_ObjId(pObj);
        vCuts[id0] = vCuts[Abc_ObjFaninId0(pObj)];
        vCuts[id0].insert({id0});
    }
    Abc_NtkForEachObj(pNtk, pObj, i)
    {
        if ( i == 0 ) continue;
        printCuts( i, vCuts[i] );
    }
}


void Lsv_NtkSdc(Abc_Ntk_t* pNtk, int nodeId, int* pat ) {

    /*
    Abc_Obj_t* pObj;
    Abc_Obj_t * pFanin;
    int i;
    Abc_Obj_t * pNode = Abc_NtkObj(pNtk, id);

    // collect fanin
    Vec_Ptr_t * vFanin = Vec_PtrAlloc(2);
    Abc_ObjForEachFanin(pNode, pFanin, i)
    {
        Vec_PtrPush(vFanin, pFanin);
    }

    // create 2 output TFI and derive cnf
    Abc_Ntk_t * pCone = Abc_NtkCreateConeArray(pNtk, vFanin, 0);
    Aig_Man_t * pMan = Abc_NtkToDar( pCone, 0, 0 );
    Cnf_Dat_t * pCnf = Cnf_Derive( pMan, 2 );
    sat_solver *solver = sat_solver_new();
    solver = (sat_solver *)Cnf_DataWriteIntoSolverInt( solver, pCnf, 1, 1 );
    // Cnf_DataPrint(pCnf, 1);  


    // print variable of nodes
    // Abc_NtkForEachObj(pCone, pObj, i) {
    //     printf("ID: %d -> Var: %d\n", Abc_ObjId(pObj), pCnf->pVarNums[Abc_ObjId(pObj)]);
    // }

    // clean up
    Vec_PtrFree( vFanin );
    */

    // get the target node
    int i, j, k;
    Abc_Obj_t *pObj;
    Abc_Obj_t *pTargetNode = Abc_NtkObj(pNtk, nodeId);
    if (Abc_ObjIsCo(pTargetNode) || Abc_ObjIsCi(pTargetNode) || pTargetNode == Abc_AigConst1(pNtk)) {
        return;
    }

    // collect two fanin and create cone
    Vec_Ptr_t *vFanin = Vec_PtrAlloc(2);
    Vec_PtrPushTwo(vFanin, Abc_ObjFanin0(pTargetNode), Abc_ObjFanin1(pTargetNode));
    Abc_Ntk_t *pNtkCone = Abc_NtkCreateConeArray(pNtk, vFanin, 1);
    Vec_PtrFree(vFanin);
    
    // fanin0 and fanin1
    Abc_Obj_t *pFanin0 = Abc_ObjFanin0(Abc_NtkPo(pNtkCone, 0));
    Abc_Obj_t *pFanin1 = Abc_ObjFanin0(Abc_NtkPo(pNtkCone, 1));
    int fanin0Comp = Abc_ObjFaninC0(pTargetNode);
    int fanin1Comp = Abc_ObjFaninC1(pTargetNode);

    // perform random simulation
    const int numPatterns = 1024;
    const int numIterations = numPatterns / (sizeof(int) * 8);
    for( i = 0; i < 4; i++ ) pat[i] = 0;
    for (i = 0; i < numIterations; ++i) {
        // random input
        Abc_NtkForEachCi(pNtkCone, pObj, j) {
            pObj->iData = Abc_Random(0);
        }
        // simulate in the topological order
        Abc_NtkForEachNode( pNtkCone, pObj, j ) {
            int value0 = (Abc_ObjFanin0(pObj)->iData) ^ (Abc_ObjFaninC0(pObj) ? S_ONE : S_ZERO);
            int value1 = (Abc_ObjFanin1(pObj)->iData) ^ (Abc_ObjFaninC1(pObj) ? S_ONE : S_ZERO);
            pObj->iData = (value0 & value1);
        }
        // get the pattern
        int fanin0Value = pFanin0->iData ^ (fanin0Comp ? S_ONE : S_ZERO);
        int fanin1Value = pFanin1->iData ^ (fanin1Comp ? S_ONE : S_ZERO);
        for (k = 0; k < sizeof(int) * 8; ++k) {
            int value0 = fanin0Value & 1;
            int value1 = fanin1Value & 1;
            int pattern = (value0 << 1) | value1;
            pat[pattern] = 1;
            fanin0Value >>= 1;
            fanin1Value >>= 1;
        }
    }

    if (pat[0] && pat[1] && pat[2] && pat[3]) {
        Abc_NtkDelete(pNtkCone);
        return;
    }

    // Derive CNF
    Aig_Man_t *pManCone = Abc_NtkToDar(pNtkCone, 0, 0);
    Cnf_Dat_t *pCnfCone = Cnf_Derive(pManCone, 2);
    sat_solver *solver = sat_solver_new();
    solver = (sat_solver *)Cnf_DataWriteIntoSolverInt(solver, pCnfCone, 1, 0);

    // test four cases of the SDC
    for (int i = 0; i < 4; ++i) {
        if (pat[i]) continue;
        // set the assumptions
        int Assumptions[2];
        int po0Var = pCnfCone->pVarNums[Abc_ObjId(Abc_NtkPo(pNtkCone, 0))];
        int po1Var = pCnfCone->pVarNums[Abc_ObjId(Abc_NtkPo(pNtkCone, 1))];
        Assumptions[0] = (i & 2) ? Abc_Var2Lit(po0Var, 0 ^ fanin0Comp) : Abc_Var2Lit(po0Var, 1 ^ fanin0Comp);
        Assumptions[1] = (i & 1) ? Abc_Var2Lit(po1Var, 0 ^ fanin1Comp) : Abc_Var2Lit(po1Var, 1 ^ fanin1Comp);

        // solve the SAT problem
        int status = sat_solver_solve(solver, Assumptions, Assumptions + 2, 1000, 1000, 1000, 1000);
        if (status == l_False) {
            // Abc_Print(1, "%d%d\n", (i & 2) ? 1 : 0, (i & 1) ? 1 : 0);
        }
        else {
            pat[i] = 1;
        }
    }

    Aig_ManStop(pManCone);
    Cnf_DataFree(pCnfCone);
    Abc_NtkDelete(pNtkCone);
    sat_solver_delete(solver);
}


void Lsv_NtkOdc(Abc_Ntk_t* pNtk, int id, int* pat ) 
{

    Abc_Obj_t* pObj;
    Abc_Obj_t* pTarget = Abc_NtkObj(pNtk, id);
    Abc_Obj_t * pFanout, pFanin;
    Aig_Obj_t * pAigObj;
    int i;


    // create ntk2 with pNode flipped
    Abc_Ntk_t * pNtk2 = Abc_NtkDup(pNtk);
    pObj = Abc_NtkObj(pNtk2, id);
    Abc_ObjForEachFanout(pObj, pFanout, i)
    {
        Abc_ObjXorFaninC(pFanout, Abc_ObjFanoutFaninNum(pFanout, pObj) );
    }

    // creat miter
    Abc_Ntk_t * pMiter = Abc_NtkMiter(pNtk, pNtk2, 1, 0, 0, 0);

    // collect fanin
    Vec_Ptr_t * vFanin = Vec_PtrAlloc(2);
    Vec_PtrPushTwo(vFanin, Abc_ObjFanin0(pTarget), Abc_ObjFanin1(pTarget));


    // add the fanin cone to miter
    Abc_Ntk_t * pCone = Abc_NtkCreateConeArray(pNtk, vFanin, 1);
    if ( Abc_ObjFaninC0(pTarget) ) 
    {
        Abc_ObjXorFaninC( Abc_NtkPo(pCone, 0), 0);
        printf("flip c0 to %d\n", Abc_ObjFaninC0(Abc_NtkPo(pCone, 0)));
    }

    if ( Abc_ObjFaninC1(pTarget) )
    {
        Abc_ObjXorFaninC( Abc_NtkPo(pCone, 1), 0);
        printf("flip c1 to %d\n", Abc_ObjFaninC0(Abc_NtkPo(pCone, 1)));
    }
    assert( Abc_NtkAppend(pMiter, pCone, 1) );


    int nLeft = 0;
    for ( i = 0; i < 4; i++)
    {
        if ( pat[i] == 1 ) nLeft ++;
    }
    // use simlation to locate some cares
    // TODO

    // create CNF
    Aig_Man_t * pMiterAig = Abc_NtkToDar(pMiter, 0, 0);
    Cnf_Dat_t *pCnf = Cnf_Derive(pMiterAig, 3);
    sat_solver *solver = sat_solver_new();
    solver = (sat_solver *)Cnf_DataWriteIntoSolverInt(solver, pCnf, 1, 0);


    // set the assumptions
    int poVar [3];
    for ( i = 0; i < 3; i++ )
        poVar[i] = pCnf->pVarNums[Aig_ObjId(Aig_ManCo(pMiterAig, i))];

    // assumptions
    int Assumptions [1];
    Assumptions[0] = Abc_Var2Lit(poVar[0], 0);
    sat_solver_addclause(solver, Assumptions, Assumptions+1); 

    int status, val0, val1;
    lit clause [2];


    // solve and block until unsat
    for ( i = 0; i < nLeft; ++i) {

        // solve the SAT problem
        status = sat_solver_solve(solver, 0, 0, 1000000, 0, 0, 0);

        if ( status == l_True)
        {
            // found care, record pat
            val0 = sat_solver_var_value(solver, poVar[1]);
            val1 = sat_solver_var_value(solver, poVar[2]);
            printf("SAT with fanin value %d %d\n", val0, val1);
            clause[0] = Abc_Var2Lit(poVar[1], val0);
            clause[1] = Abc_Var2Lit(poVar[2], val1);
            if ( !sat_solver_addclause(solver, clause, clause+2) )
            {
                printf("UNSAT (last sol blocked)\n");
                break;
            }

        }
        else 
        {
            // unsat, all care set found
            printf("UNSAT (or timeout)\n");
            break;
        }
    }

    Abc_NtkDelete(pNtk2);
    Abc_NtkDelete(pMiter);
    Abc_NtkDelete(pCone);
    Vec_PtrFree(vFanin);

    return;
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

int Lsv_CommandSdc(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c, id;
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
  id = atoi(argv[globalUtilOptind]);
  int pat [4];
  Lsv_NtkSdc(pNtk, id, pat);
  if ( pat[0] && pat[1] && pat[2] && pat[3] )
  {
    Abc_Print(1, "no sdc\n");
  }
  else 
  {
    for ( int i = 0; i < 4; i++ )
    {
        if (!pat[i]) 
        {
            Abc_Print(1, "%d%d\n", (i & 2) ? 1 : 0, (i & 1) ? 1 : 0);
        }
    }
 }
  
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sdc <n>\n");
  Abc_Print(-2, "\t        given an AIG, prints the sdc of node <n>\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

int Lsv_CommandOdc(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
  int c, id;
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
  id = atoi(argv[globalUtilOptind]);
  int pat[4];

  // find appearing pats
  Lsv_NtkSdc(pNtk, id, pat);

  // remove careset from pat
  Lsv_NtkOdc(pNtk, id, pat);

  if (!pat[0] && !pat[1] && !pat[2] && !pat[3]) {
    Abc_Print(1, "no odc\n");
  } else {
    for (int i = 0; i < 4; i++) {
      if (pat[i]) {
        Abc_Print(1, "%d%d\n", (i & 2) ? 1 : 0, (i & 1) ? 1 : 0);
      }
    }
  }

  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sdc <n>\n");
  Abc_Print(-2, "\t        given an AIG, prints the sdc of node <n>\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}