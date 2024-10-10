#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"

#include <iostream>
#include<list>
#include<vector>
#include<map>
#include<unordered_map>
#include<set>
#include<algorithm>

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
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



using namespace std;
static int Lsv_printcut(Abc_Frame_t* pAbc, int argc, char** argv);

void init_pa1(Abc_Frame_t* pAbc) 
{
    Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_printcut, 0);
}

void destroy_pa1(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer_pa1 = {init_pa1, destroy_pa1};

struct lsvPackageRegistrationManager_PA1 
{
    lsvPackageRegistrationManager_PA1() { Abc_FrameAddInitializer(&frame_initializer_pa1); }
} lsvPackageRegistrationManager_PA1;

unordered_map<Abc_Obj_t*,set<set<int>>> cut_list;
list<Abc_Obj_t*> top;

void Lsv_NtkPrintCuts(Abc_Ntk_t* pNtk) {
    Abc_Obj_t* pObj;
    int i;

    /*
    #define Abc_NtkForEachNode( pNtk, pNode, i )                                                       \
    for ( i = 0; (i < Vec_PtrSize((pNtk)->vObjs)) && (((pNode) = Abc_NtkObj(pNtk, i)), 1); i++ )   \
        if ( (pNode) == NULL || !Abc_ObjIsNode(pNode) ) {} else
    */
    Abc_NtkForEachObj(pNtk, pObj, i) 
    {
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

bool is_subset(set<int>& set1, set<int>& set2)  //set1 is subset of set2
{
    if(set1.size() > set2.size())
        return 0;
    for(auto itm : set1)
    {
        if(set2.find(itm) == set2.end())
            return 0;
    }
    return 1;
}

void remove_redudant(set<set<int>>& cur_cut)
{
    vector<set<int>> temp{cur_cut.begin(),cur_cut.end()};
    
    for(int i = 0; i < temp.size(); i++)
    {
        for(int j = 0; j < temp.size(); j++)
        {
            if(i==j)
                continue;
            
            if(is_subset(temp[i],temp[j])) //temp[i] is subset of temp[j]
            {
                temp[j] = temp[temp.size()-1];
                temp.pop_back();
                j--;
                continue;
            }
            else if(is_subset(temp[j],temp[i]))
            {
                temp[i] = temp[temp.size()-1];
                temp.pop_back();
                i--;
                break;
            }
        }
    }
    cur_cut.clear();
    cur_cut.insert(temp.begin(),temp.end());
}




bool is_PO(Abc_Obj_t* pObj, list<Abc_Obj_t*> top)
{
    for(auto obj : top)
    {
        if(obj == pObj)
            return true;
    }
    return false;
}

set<set<int>> k_feasible_cut(Abc_Obj_t* pObj, int cut_size)
{
    if(cut_list.find(pObj) != cut_list.end())
    {
        return cut_list[pObj];
    }

    vector<set<int>> fanin_cut[2];
    fanin_cut[1].clear();
    fanin_cut[0].clear();
    int cnt = 0;
    int j;
    Abc_Obj_t* pFanin;
    Abc_ObjForEachFanin(pObj, pFanin, j)
    {
        if(Abc_ObjId(pFanin) == 0)
            continue;
        set<set<int>> temp = k_feasible_cut(pFanin, cut_size);
        fanin_cut[cnt] =vector<set<int>>{temp.begin(),temp.end()};
        cnt++;
    }

    set<set<int>> cur_cut;
    cur_cut.insert({int(Abc_ObjId(pObj))});
    for(int i = 0 ; i < fanin_cut[0].size(); i++)
    {
        for(int j = 0; j < fanin_cut[1].size(); j++)
        {
            set<int> temp;
            temp.insert(fanin_cut[0][i].begin(),fanin_cut[0][i].end());
            temp.insert(fanin_cut[1][j].begin(),fanin_cut[1][j].end());
            if(temp.size() <= cut_size)
                cur_cut.insert(temp);
        }
        if(cnt==1)  //only 1 pin
        {
            set<int> temp;
            temp.insert(fanin_cut[0][i].begin(),fanin_cut[0][i].end());
            if(temp.size() <= cut_size)
                cur_cut.insert(temp);
        }
    }
    for(auto & cut : cur_cut)
    {
        if(cut.size() > cut_size)
        {
            cur_cut.erase(cut);
        }
    }
    
    if(!is_PO(pObj,top))
        remove_redudant(cur_cut);

    cut_list.insert(make_pair(pObj,cur_cut));
    return cur_cut;
}



void printcut(Abc_Ntk_t* pNtK, int cut_size)
{
    //get_fanin

    
    Abc_Obj_t* pObj;
    int itr;
    int po_size = Abc_NtkPoNum(pNtK);
    for(int i = 0 ; i < po_size; i++)
    {
        top.push_back(Abc_NtkPo(pNtK,i));
    }

    
    Abc_NtkForEachObj(pNtK, pObj, itr)
    {
        if(!is_PO(pObj,top) && Abc_ObjId(pObj) != 0)
            k_feasible_cut(pObj,cut_size);
    }
    for(auto pObj : top)
    {
        k_feasible_cut(pObj,cut_size);
    }
    vector<pair<Abc_Obj_t*,set<set<int>>>> sol_out{cut_list.begin(),cut_list.end()};
    sort(sol_out.begin(),sol_out.end(),[](pair<Abc_Obj_t*,set<set<int>>> a, pair<Abc_Obj_t*,set<set<int>>> b)
    {
        return Abc_ObjId(a.first) < Abc_ObjId(b.first);
    });
    for(auto obj : sol_out)
    {
        
        for(auto cut : obj.second)
        {
            if(cut.size() > cut_size)
                continue;
            cout<<Abc_ObjId(obj.first)<<": ";
            for(auto itm : cut)
            {
                cout<<itm<<" ";
            }
            cout<<endl;
        }
    }
    cut_list.clear();
    top.clear();
}


int Lsv_printcut(Abc_Frame_t* pAbc, int argc, char** argv)
{
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc); //get network type
    int c;
    Extra_UtilGetoptReset();
    int size = 0;
    //Extra_UtilGetopt get the -<alphabat> value
    while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) 
    {
        cout<<c<<endl;
        switch (c) {
            case 'h':
            { 
                goto usage;
            }
            default:
            {
                goto usage;
            }
        }
    }
    
    if (!pNtk) 
    {
        Abc_Print(-1, "Empty network.\n");
        return 1;
    }

    if (argc < 2) 
    {
        cout << "Invalid Argument: Expected 1 argument" << endl;
        return 1;
    }
    for (int i = 0; argv[1][i] != '\0'; ++i) {
        if (!isdigit(argv[1][i])) {
            cout << "Invalid Argument: Size must be an integer" << endl;
            return 1;
        }
    }
    if ( !Abc_NtkIsStrash(pNtk) )
    {
        Abc_Print( -1, "Cut computation is available only for AIGs (run \"strash\").\n" );
        return 1;
    }
    size = stoi(argv[1]);
    
    
    printcut(pNtk,size);
    //Lsv_NtkPrintCuts(pNtk);
    return 0;

usage:
    Abc_Print(-2, "usage: lsv_print_nodes [-h]\n");
    Abc_Print(-2, "\t        prints the nodes in the network\n");
    Abc_Print(-2, "\t-h    : print the command usage\n");
    return 1;
}