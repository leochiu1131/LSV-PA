#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/cnf/cnf.h"
#include "sat/satoko/satoko.h"
#include "misc/util/utilTruth.h"
#include "aig/aig/aig.h"
#include "sat/bsat/satSolver.h"
#include <vector>
#include <stack>
#include <map>
#include <set>
#include <iostream>
#include <algorithm>  
#include <iterator>   
#include <bitset>
#include <chrono> 
#include <ctime>
#include "aig/aig/aig.h" 
#include <unordered_set>
#include <cstdlib>

extern "C" {
    Aig_Man_t* Abc_NtkToDar(Abc_Ntk_t * pNtk, int fExors, int fRegisters);
}



using namespace std;

#define NUM_SIMULATIONS 100
#define AIG_NODE_COUNT_MAX 10000  // or an appropriate limit

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintcut (Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSdc(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandOdcs(Abc_Frame_t* pAbc, int argc, char** argv) ;
void ComputeOdcs(Abc_Ntk_t* pNtk, Abc_Obj_t* pNode );

void init(Abc_Frame_t* pAbc) 
{
    Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
    Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_CommandPrintcut, 0);
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sdc", Lsv_CommandSdc, 0); 
    Cmd_CommandAdd(pAbc, "LSV", "lsv_odc", Lsv_CommandOdcs, 0);  
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


struct MapCompare {
    bool operator()(Abc_Obj_t* a, Abc_Obj_t* b) const {
        return Abc_ObjId(a) < Abc_ObjId(b); 
    }
};

struct SetCompare {
    // ?? operator() 靘??箸?頛
    bool operator()(const set<Abc_Obj_t*,MapCompare>& a, const set<Abc_Obj_t*,MapCompare>& b) const {
        return a.size() < b.size(); 
    }
};





 map<Abc_Obj_t*,  set<set<Abc_Obj_t*,MapCompare>>,MapCompare > isVisited_map;


bool isSubset(const set<Abc_Obj_t*, MapCompare>& set1, const set<Abc_Obj_t*, MapCompare>& set2)
{
    for (const auto& elem : set1)
    {
        if (set2.find(elem) == set2.end()) 
        {
            return false;
        }
    }
    return true;  
}




void Mergeset(Abc_Obj_t* ptr1 , Abc_Obj_t* ptr2 , Abc_Obj_t* Mergeresult, int k)
{
    set<set<Abc_Obj_t*, MapCompare>> com_mergeset;
    for (const auto& elem1 : isVisited_map[ptr1]) 
    {
     for (const auto& elem2 : isVisited_map[ptr2]) 
      {            
        set<Abc_Obj_t*,MapCompare> mergeset;
        set_union(elem1.begin(), elem1.end(), elem2.begin(), elem2.end(), inserter(mergeset, mergeset.begin()));
        if(mergeset.size() <= k )
        {
          com_mergeset.insert(mergeset);
        }
        
      }
    }

    auto& dest = com_mergeset;
    set<set<Abc_Obj_t*, MapCompare>> to_erase;  

    for (auto it = dest.begin(); it != dest.end(); ++it)
    {
        for (auto itin = dest.begin(); itin != dest.end(); ++itin)
        {
            if(itin == it)
            {
              continue;
            }
            if (isSubset(*itin, *it))
            {
                to_erase.insert(*it);  
            }
            if (isSubset(*it, *itin))
            {
                to_erase.insert(*itin); 
            }
        }
    }
    for (const auto& elem : to_erase)
    {
        com_mergeset.erase(elem);
    }
    for (const auto& setelem : com_mergeset)
    {
        isVisited_map[Mergeresult].insert(setelem);
    }

}
 






void findNodes(Abc_Obj_t* topNode, int k) {
    if (topNode == nullptr) {
        cout << "Top node is null." << endl;
        return;
    }

    stack<Abc_Obj_t*> q;
    q.push(topNode); 

    while (!q.empty()) {
        Abc_Obj_t* current = q.top();
        // 撠椰?喳?蝭暺?仿???撅斗活 +1
        if(isVisited_map.find(current)!= isVisited_map.end())
        {
          q.pop();
        }
        else
        {
            if(Abc_ObjFaninNum(current)==2)
            {
                if((isVisited_map.find(Abc_ObjFanin0(current)) != isVisited_map.end() ) && (isVisited_map.find(Abc_ObjFanin1(current)) != isVisited_map.end() ))
                { 
                  
                  set<Abc_Obj_t*,MapCompare> initial ;
                  initial.insert(current);
                  isVisited_map[current].insert(initial) ;
                  Mergeset(Abc_ObjFanin0(current),Abc_ObjFanin1(current),current,k);
                  q.pop();
                }
                if (isVisited_map.find(Abc_ObjFanin0(current)) == isVisited_map.end() ) {
                  q.push(Abc_ObjFanin0(current));
                }
                if (isVisited_map.find(Abc_ObjFanin1(current)) == isVisited_map.end() ) {
                  q.push(Abc_ObjFanin1(current));
                }
            } 
            else if( Abc_ObjFaninNum(current)==1)
            {
               if((isVisited_map.find(Abc_ObjFanin0(current)) != isVisited_map.end() ) )
               {
                  set<Abc_Obj_t*,MapCompare> initial ;
                  initial.insert(current);
                  isVisited_map[current] =  isVisited_map[Abc_ObjFanin0(current)];
                  isVisited_map[current].insert(initial);
                  q.pop();
               }
               else
               {
                   q.push(Abc_ObjFanin0(current));
               }
            }
            else if(Abc_ObjFaninNum(current)==0)
            {
              set<Abc_Obj_t*,MapCompare> initial ;
              initial.insert(current);
              isVisited_map[current].insert(initial);
              q.pop();
            }
            else
            {
              q.pop();
            }
            
        }
    }
}



int Lsv_CommandPrintcut(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  if (argc != 2) {
    Abc_Print(-1, "Invalid number of arguments. Usage: lsv printcut <k>\n");
    return 1;
  }

  int k = atoi(argv[1]);
 
  if (k <= 0) {
    Abc_Print(-1, "Invalid k value. It should be a positive integer.\n");
    return 1;
  }
  Abc_Obj_t* pObj;
  int z;
  Abc_NtkForEachObj(pNtk, pObj, z)
  {
     if(Abc_ObjIsPi(pObj)==1)
     {
       
       set<Abc_Obj_t*,MapCompare> initial ;
       initial.insert(pObj);
       isVisited_map[pObj].insert(initial) ;
     }
  }
  Abc_NtkForEachObj(pNtk, pObj, z)
  {
      findNodes(pObj,k);
  }
  int count = 0;
  for (map<Abc_Obj_t*,  set<set<Abc_Obj_t*,MapCompare>>,MapCompare> ::iterator it = isVisited_map.begin(); it != isVisited_map.end(); ++it) {
  set<set<Abc_Obj_t*,MapCompare>> ttt = it->second;
  vector<set<Abc_Obj_t*,MapCompare>> tttVec(ttt.begin(), ttt.end());
  sort(tttVec.begin(), tttVec.end(), SetCompare());
  // cout<<Abc_ObjId(it->first)<<": "<<it->second.size()<<endl;
  for (const auto& uuu : tttVec) 
  {
        cout<<Abc_ObjId(it->first)<<": ";
        for (const auto& hello : uuu) 
        {
            cout<<Abc_ObjId(hello)<<" ";
        }
        cout<<endl;
   }
    }


   return 0;
  }



string generateRandomPattern() 
{
    int val0 = rand() % 2;
    int val1 = rand() % 2;
    return to_string(val0) + to_string(val1);
}


Abc_Obj_t* FindNodeByName(Abc_Ntk_t* pNtk, const char* node_name) 
{
    Abc_Obj_t* pObj;
    int i;
    Abc_NtkForEachObj(pNtk, pObj, i) 
    {
        if (strcmp(Abc_ObjName(pObj), node_name) == 0)
        {
            return pObj;
        }
    }
    return nullptr; 
}

void SimulatePattern_z(Abc_Ntk_t* pNtk, int pattern) 
{
    Abc_Obj_t * pObj;
    int i;
    Abc_NtkForEachPi(pNtk, pObj, i) 
    {
        pObj->iData = (pattern >> i) & 1;
    }
    Abc_NtkForEachNode(pNtk, pObj, i) 
    {
        int simValue0 = Abc_ObjFanin0(pObj)->iData ^ Abc_ObjFaninC0(pObj);
        int simValue1 = Abc_ObjFanin1(pObj)->iData ^ Abc_ObjFaninC1(pObj);
        pObj->iData = simValue0 & simValue1;
    }

    Abc_NtkForEachPo(pNtk, pObj, i) 
    {
        pObj->iData = Abc_ObjFanin0(pObj)->iData;
    }
}

void RandomSimulate_z( Abc_Ntk_t* pNtk, int numSimulations, uint8_t& oPatterns, Abc_Obj_t* fin1, Abc_Obj_t* fin2, bool fanin1Bool, bool fanin2Bool) 
{
    oPatterns = 0; 
    srand(time(nullptr));

    for (int i = 0; i < numSimulations && oPatterns != 0xF; ++i)
    { 
        int pattern = rand();
        SimulatePattern_z(pNtk, pattern);

        int data1 = fin1->iData;
        int data2 = fin2->iData;

        data1 = fanin1Bool ? !data1 : data1;
        data2 = fanin2Bool ? !data2 : data2;

        int patternEncoded = (data1 << 1) | data2; 
        oPatterns |= (1 << patternEncoded);  

        if (oPatterns == 0xF) 
        {
            break;
        }
    }
}

bool isPatternSatisfiable(sat_solver* pSat, int var1, int data1, int var2, int data2) 
{
    int lit1 = Abc_Var2Lit(var1, !data1); 
    int lit2 = Abc_Var2Lit(var2, !data2);
    int assumptions[2] = {lit1, lit2};
    return sat_solver_solve(pSat, assumptions, assumptions + 2, 0, 0, 0, 0) == l_True;
}

void computeSDCs(Abc_Ntk_t* pNtk, Abc_Obj_t* targetNode , set<pair<int,int>>& result ) 
{
    Abc_Obj_t* fin1 = Abc_ObjFanin0(targetNode);
    Abc_Obj_t* fin2 = Abc_ObjFanin1(targetNode);

    if (!fin1 || !fin2) 
    {
        return;
    }

    bool fanin1Bool = Abc_ObjFaninC0(targetNode);
    bool fanin2Bool = Abc_ObjFaninC1(targetNode);

    Vec_Ptr_t* pVec = Vec_PtrAlloc(2);
    Vec_PtrPush(pVec, fin1);
    Vec_PtrPush(pVec, fin2);

    Abc_Ntk_t* coneNtk = Abc_NtkCreateConeArray(pNtk, pVec, 1);
    Aig_Man_t* pAig = Abc_NtkToDar(coneNtk, 0, 0);


    Cnf_Dat_t* pCnf = Cnf_Derive(pAig, 2);

    sat_solver* pSat = sat_solver_new();
    Cnf_DataWriteIntoSolverInt((void*)pSat, pCnf, 1, 0);

    Aig_Obj_t* aigFanin1 = Aig_ObjFanin0(Aig_ManCo(pAig, 0));
    Aig_Obj_t* aigFanin2 = Aig_ObjFanin0(Aig_ManCo(pAig, 1));


    int var1 = pCnf->pVarNums[Aig_ObjId(aigFanin1)];
    int var2 = pCnf->pVarNums[Aig_ObjId(aigFanin2)];
    const int numSimulations = 8;
    uint8_t oPatterns;
    
    RandomSimulate_z(pNtk, numSimulations, oPatterns, fin1, fin2, fanin1Bool, fanin2Bool );

    for (int i = 0; i < 4; ++i) 
    {
        if (!(oPatterns & (1 << i))) 
        {
            int pattern1 = (i >> 1) & 1;
            int pattern2 = i & 1;
            int newPattern1 = fanin1Bool ? !pattern1 : pattern1;
            int newPattern2 = fanin2Bool ? !pattern2 : pattern2;

            if (!isPatternSatisfiable(pSat, var1, newPattern1, var2, newPattern2)) 
            {
                result.insert(make_pair(pattern1, pattern2));
            }
        }
    }
}

int Lsv_CommandSdc(Abc_Frame_t* pAbc, int argc, char** argv) 
{
    if (argc != 2) 
    {
        Abc_Print(-1, "Usage: lsv_sdc <node_id>\n");
        return 1;
    }

    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    Abc_Ntk_t* pNtkBackup = NULL;
    pNtkBackup = Abc_NtkDup(pNtk);
    if (!pNtkBackup) 
    {
        Abc_Print(-1, "duplicate the network Wrong.\n");
        return 1;
    }
    int targetNodeId = atoi(argv[1]);
    Abc_Obj_t* targetNode = Abc_NtkObj(pNtkBackup, targetNodeId);
    set<pair<int, int>> temp;

    computeSDCs(pNtkBackup, targetNode, temp);

    vector<pair<int, int>> sortedPairs(temp.begin(), temp.end());
    sort(sortedPairs.begin(), sortedPairs.end());

    if (temp.empty()) 
    {
        Abc_Print(1, "no sdc\n");
    } 
    else
    {
        for (const auto& p : sortedPairs) 
        {
            cout << p.first << p.second << " ";
        }
        cout << endl;
    }

    return 0;
}




void ComputeOdcs(Abc_Ntk_t* pNtk, Abc_Obj_t* pNode) 
{
    Abc_Ntk_t* pNtkBackup = NULL;
    Abc_Ntk_t* pNtkDupOrigin = NULL;
    Abc_Ntk_t* pMiter = NULL;
    Abc_Ntk_t* pCone = NULL; 
    Aig_Man_t* pMan = NULL;
    Cnf_Dat_t* pCnf = NULL;
    sat_solver* pSat = NULL;

    vector<int> faninVars;
    vector<Abc_Obj_t*> vFaninNodes;
    set<pair<int,int>> careSet;
    set<pair<int,int>> odcSet;

    pNtkBackup = Abc_NtkDup(pNtk);
    if (!pNtkBackup) 
    {
        Abc_Print(-1, "XXX duplicate the network Wrong XXX\n");
        return;
    }

    pNtkDupOrigin = Abc_NtkDup(pNtk);
    if (!pNtkDupOrigin) 
    {
        Abc_Print(-1, "XX duplicate the network Wrong XXX\n");
        return;
    }

    Abc_Obj_t* pNodeOrigin = Abc_NtkObj(pNtkDupOrigin, Abc_ObjId(pNode));
    Abc_Obj_t* pNodeBackup = Abc_NtkObj(pNtkBackup, Abc_ObjId(pNode));

    Abc_Obj_t* pFanout;
    int k;
    Abc_ObjForEachFanout(pNodeBackup, pFanout, k) 
    {
        int iFaninPos;
        Abc_Obj_t* pFaninTemp;
        Abc_ObjForEachFanin(pFanout, pFaninTemp, iFaninPos) 
        {
            if (pFaninTemp == pNodeBackup) 
            {
                Abc_ObjXorFaninC(pFanout, iFaninPos);
                break;
            }
        }
    }

    Abc_Obj_t* pFanin;
    int idx;
    Abc_ObjForEachFanin(pNode, pFanin, idx) 
    {
        vFaninNodes.push_back(pFanin);
    }

    Vec_Ptr_t* vNodes = Vec_PtrAlloc(1 + vFaninNodes.size());
    for (auto pFaninNode : vFaninNodes) 
    {
        Vec_PtrPush(vNodes, pFaninNode);
    }

    pCone = Abc_NtkCreateConeArray(pNtk, vNodes, 1);
    Vec_PtrFree(vNodes);

    if (!pCone) 
    {
        Abc_Print(-1, "XXX create cone network Wrong XXX\n");
        Abc_NtkDelete(pNtkBackup);
        return;
    }

    pMiter = Abc_NtkMiter(pNtk, pNtkBackup, 1, 0, 0, 0);
    if (!pMiter) 
    {
        Abc_Print(-1, "XXX create the miter Wrong XXX\n");
        Abc_NtkDelete(pNtkBackup);
        Abc_NtkDelete(pCone);
        return;
    }

    if (!Abc_NtkAppend(pMiter, pCone, 1)) 
    {
        Abc_Print(-1, "XXX append cone to miter Wrong XXX\n");
        Abc_NtkDelete(pNtkBackup);
        Abc_NtkDelete(pMiter);
        Abc_NtkDelete(pCone);
        return;
    }

    pMan = Abc_NtkToDar(pMiter, 0, 0);
    pCnf = Cnf_Derive(pMan, Aig_ManCoNum(pMan));

    pSat = sat_solver_new();
    sat_solver_setnvars(pSat, pCnf->nVars);

    for (int clauseindex = 0; clauseindex < pCnf->nClauses; ++clauseindex) 
    {
        if (!sat_solver_addclause(pSat, pCnf->pClauses[clauseindex], pCnf->pClauses[clauseindex + 1])) 
        {
            sat_solver_delete(pSat);
            Cnf_DataFree(pCnf);
            Aig_ManStop(pMan);
            Abc_NtkDelete(pNtkBackup);
            Abc_NtkDelete(pMiter);
            return;
        }
    }
    
    Aig_Obj_t* pMiterCo = Aig_ManCo(pMan, 0);
    int outputVar = pCnf->pVarNums[Aig_ObjId(pMiterCo)];
    lit outputLit = Abc_Var2Lit(outputVar, 0);

    if (!sat_solver_addclause(pSat, &outputLit, &outputLit + 1)) 
    {
        Abc_Print(-1, "XXX assert miter output Wrong XXX\n");
        sat_solver_delete(pSat);
        Cnf_DataFree(pCnf);
        Aig_ManStop(pMan);
        Abc_NtkDelete(pNtkBackup);
        Abc_NtkDelete(pMiter);
        return;
    }

    int numFanins = vFaninNodes.size();
    faninVars.clear();
    for (int i = 0; i < numFanins; ++i) 
    {
        Aig_Obj_t* aigFanin = Aig_ObjFanin0(Aig_ManCo(pMan, i + 1)); 
        int var = pCnf->pVarNums[Aig_ObjId(aigFanin)];
        faninVars.push_back(var);
    }


    int status;
    while (true) 
    {
        status = sat_solver_solve(pSat, NULL, 0, 0, 0, 0, 0);
        if (status == l_False) 
        {
            break;
        } 
        else if (status == l_Undef) 
        {
            break;
        }
        vector<int> assignment;
        for (int var : faninVars) 
        {
            int value = sat_solver_var_value(pSat, var);
            assignment.push_back(value);
        }
        vector<lit> blockingClause;
        pair<int,int> result_temp ;
        for (size_t j = 0; j < faninVars.size(); ++j) 
        {
            int var = faninVars[j];
            int value = sat_solver_var_value(pSat, var);
            if(j==0)
            {
                if(Abc_ObjFaninC0(pNode))
                {
                    result_temp.first = !value ;
                }
                else
                {
                    result_temp.first = value ;
                }
            }
            else
            {
                if(Abc_ObjFaninC1(pNode))
                {
                    result_temp.second = !value ;
                }
                else
                {
                    result_temp.second = value ;
                }
            }
            blockingClause.push_back(toLitCond(var,value));
        }
        careSet.insert(result_temp);
        if (!sat_solver_addclause(pSat, blockingClause.data(), blockingClause.data() + blockingClause.size())) 
        {
            break;
        }
    }

    set<pair<int,int>> allPoss;
    set<pair<int,int>> SDC_set;
    allPoss.insert(make_pair(0,0));
    allPoss.insert(make_pair(0,1));
    allPoss.insert(make_pair(1,0));
    allPoss.insert(make_pair(1,1));


    set<pair<int,int>> tempResult;
    set_difference(allPoss.begin(), allPoss.end(),careSet.begin(), careSet.end(),inserter(tempResult, tempResult.end()));
    computeSDCs(pNtkDupOrigin, pNodeOrigin, SDC_set);
    set_difference(tempResult.begin(), tempResult.end(),SDC_set.begin(), SDC_set.end(), inserter(odcSet, odcSet.end()));
    vector<pair<int, int>> sortedPairs(odcSet.begin(), odcSet.end());
    sort(sortedPairs.begin(), sortedPairs.end());


    if (odcSet.empty()) 
    {
        Abc_Print(1, "no odc\n");
    } 
    else
    {
        for (const auto& p : sortedPairs) 
        {
            cout <<p.first << p.second << " ";
        }
        cout<<endl;

    }
}




int Lsv_CommandOdcs(Abc_Frame_t* pAbc, int argc, char** argv) 
{
    if (argc != 2) 
    {
        Abc_Print(-1, "Usage: lsv_odc <node_id>\n");
        return 1;
    }

    int nodeId = atoi(argv[1]);

    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    if (!pNtk) 
    {
        Abc_Print(-1, "XXX Network is loaded Wrong XXX\n");
        return 1;
    }
    Abc_Ntk_t* pNtkBackup = NULL;
    pNtkBackup = Abc_NtkDup(pNtk);
    if (!pNtkBackup) 
    {
        Abc_Print(-1, "XXX duplicate network Wrong XXX \n");
        return 1;
    }

    Abc_Obj_t* pNode = Abc_NtkObj(pNtkBackup, nodeId);
    if (!pNode) 
    {
        Abc_Print(-1, "XXX ID %d Node not exist Wrong XXX \n", nodeId);
        return 1;
    }
    
    ComputeOdcs(pNtkBackup, pNode);
    return 0;
}