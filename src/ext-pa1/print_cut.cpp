#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <iostream>
#include<list>
#include<vector>
#include<map>
#include<set>
#include<algorithm>

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

map<Abc_Obj_t*,vector<set<Abc_Obj_t*>>> cut_list;

void Lsv_NtkPrintCuts(Abc_Ntk_t* pNtk) {
    Abc_Obj_t* pObj;
    int i;

    /*
    #define Abc_NtkForEachNode( pNtk, pNode, i )                                                       \
    for ( i = 0; (i < Vec_PtrSize((pNtk)->vObjs)) && (((pNode) = Abc_NtkObj(pNtk, i)), 1); i++ )   \
        if ( (pNode) == NULL || !Abc_ObjIsNode(pNode) ) {} else
    */
    Abc_NtkForEachNode(pNtk, pObj, i) 
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


vector<set<Abc_Obj_t*>> k_feasible_cut(Abc_Obj_t* pObj, int cut_size)
{
    if(cut_list.find(pObj) != cut_list.end())
    {
        return cut_list[pObj];
    }

    vector<set<Abc_Obj_t*>> fanin_cut[2];
    int cnt = 0;
    int j;
    Abc_Obj_t* pFanin;
    Abc_ObjForEachFanin(pObj, pFanin, j)
    {
        vector<set<Abc_Obj_t*>> temp = k_feasible_cut(pFanin, cut_size);
        fanin_cut[cnt++] = temp;
    }

    vector<set<Abc_Obj_t*>> cur_cut;
    cur_cut.push_back({pObj});
    for(int i = 0 ; i < fanin_cut[0].size(); i++)
    {
        for(int j = 0; j < fanin_cut[1].size(); j++)
        {
            set<Abc_Obj_t*> temp;
            temp.insert(fanin_cut[0][i].begin(),fanin_cut[0][i].end());
            temp.insert(fanin_cut[1][j].begin(),fanin_cut[1][j].end());
            if(temp.size() <= cut_size)
                cur_cut.push_back(temp);
        }
        if(cnt==1)  //only 1 pin
        {
            set<Abc_Obj_t*> temp;
            temp.insert(fanin_cut[0][i].begin(),fanin_cut[0][i].end());
            if(temp.size() <= cut_size)
                cur_cut.push_back(temp);
        }
    }
    sort(cur_cut.begin(),cur_cut.end(),[](set<Abc_Obj_t*> a, set<Abc_Obj_t*> b){return a.size() < b.size();});
    for(int i = 0 ; i < cur_cut.size(); i++)
    {
        if(cur_cut[i].size() > cut_size)
        {
            cut_list.insert(make_pair(pObj,vector<set<Abc_Obj_t*>>{cur_cut.begin(),cur_cut.begin()+i}));
            return vector<set<Abc_Obj_t*>>{cur_cut.begin(),cur_cut.begin()+i};
        }
    }
    
    cut_list.insert(make_pair(pObj,vector<set<Abc_Obj_t*>>{cur_cut.begin(),cur_cut.end()}));
    return cur_cut;
}

void printcut(Abc_Ntk_t* pNtK, int cut_size)
{
    //get_fanin

    list<Abc_Obj_t*> top;
    Abc_Obj_t* pObj;
    int itr;
    int po_size = Abc_NtkPoNum(pNtK);
    for(int i = 0 ; i < po_size; i++)
    {
        top.push_back(Abc_NtkPo(pNtK,i));
    }
    Abc_NtkForEachNode(pNtK, pObj, itr)
    {
        if(Abc_ObjFanoutVec( pObj )->nSize == 0)
            top.push_back(pObj);
    }

    for(auto pObj : top)
    {
        k_feasible_cut(pObj,cut_size);
    }
    
    for(auto obj : cut_list)
    {
        
        for(auto cut : obj.second)
        {
            if(cut.size() > cut_size)
                continue;
            cout<<Abc_ObjId(obj.first)<<": ";
            for(auto itm : cut)
            {
                cout<<Abc_ObjId(itm)<<" ";
            }
            cout<<endl;
        }
    }
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