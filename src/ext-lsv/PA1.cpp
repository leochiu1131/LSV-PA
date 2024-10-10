#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <vector>
#include <string>
#include <set>
#include <cstdlib>
#include <algorithm>
using namespace std;
static int lsv_printcut(Abc_Frame_t *pAbc, int argc, char **argv);

void init(Abc_Frame_t *pAbc)
{
    Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", lsv_printcut, 0);
}

void destroy(Abc_Frame_t *pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager
{
    PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

void lsv_k_feasible(Abc_Ntk_t *pNtk, int k)
{
    vector<set<set<int>>> var;
    var.reserve(Abc_NtkObjNum(pNtk));
    set<set<int>> temp, b;
    set<int> a;
    Abc_Obj_t *pObj;
    int i;
    Abc_NtkForEachObj(pNtk, pObj, i)
    {
        if (Abc_ObjIsPo(pObj) || i == 0)
        {
            temp.clear();
            a.clear();
            temp.insert(a);
            var.push_back(temp);
            continue;
        }
        temp.clear();
        a.clear();
        a.insert(Abc_ObjId(pObj));
        temp.insert(a);
        a.clear();
        if (Abc_ObjIsPi(pObj))
        {
            var.push_back(temp);
            continue;
        }
        int pFanin0, pFanin1;
        pFanin0 = Abc_ObjFaninId0(pObj);
        pFanin1 = Abc_ObjFaninId1(pObj);
        for (const auto &s1 : var[pFanin0])
        {
            for (const auto &s3 : var[pFanin1])
            {
                for (const auto &s2 : s1)
                {
                    a.insert(s2);
                }
                for (const auto &s4 : s3)
                {
                    a.insert(s4);
                }
                if (a.size() > k)
                {
                    a.clear();
                    continue;
                }
                else
                {
                    temp.insert(a);
                    a.clear();
                }
            }
        }
        bool flag = false;
        b.clear();
        for (const auto &s1 : temp)
        {
            flag = false;
            for (const auto &s2 : temp)
            {
                if (s1 != s2)
                {
                    if (includes(s1.begin(), s1.end(), s2.begin(), s2.end()))
                    {
                        flag = true;
                        break;
                    }
                }
            }
            if (flag == false)
            {
                b.insert(s1);
            }
        }
        var.push_back(b);
    }
    // Abc_NtkForEachPo(pNtk, pObj, i)
    // {
    //     temp.clear();
    //     a.clear();
    //     int pFanin0, pFanin1, id;
    //     id = Abc_ObjId(pObj);
    //     if (Abc_ObjFaninNum(pObj) == 2)
    //     {
    //         pFanin0 = Abc_ObjFaninId0(pObj);
    //         pFanin1 = Abc_ObjFaninId1(pObj);
    //         for (const auto &s1 : var[pFanin0])
    //         {
    //             for (const auto &s3 : var[pFanin1])
    //             {
    //                 for (const auto &s2 : s1)
    //                 {
    //                     a.insert(s2);
    //                 }
    //                 for (const auto &s4 : s3)
    //                 {
    //                     a.insert(s4);
    //                 }
    //                 if (a.size() > k)
    //                 {
    //                     a.clear();
    //                     continue;
    //                 }
    //                 else
    //                 {
    //                     temp.insert(a);
    //                     a.clear();
    //                 }
    //             }
    //         }
    //         a.insert(id);
    //         temp.insert(a);
    //         var[id] = temp;
    //         a.clear();
    //         temp.clear();
    //     }
    //     else
    //     {
    //         pFanin0 = Abc_ObjFaninId0(pObj);
    //         for (const auto &s1 : var[pFanin0])
    //         {
    //             for (const auto &s2 : s1)
    //             {
    //                 a.insert(s2);
    //             }
    //             if (a.size() > k)
    //             {
    //                 a.clear();
    //                 continue;
    //             }
    //             else
    //             {
    //                 temp.insert(a);
    //                 a.clear();
    //             }
    //         }
    //         a.insert(id);
    //         temp.insert(a);
    //         var[id] = temp;
    //         a.clear();
    //         temp.clear();
    //     }
    // }
    for (int i = 1; i < var.size(); i++)
    {
        for (const auto &s1 : var[i])
        {
            if (s1.size() == 0)
                continue;
            printf("%d: ", i);
            for (const auto &s2 : s1)
            {
                printf("%d ", s2);
            }
            printf("\n");
        }
    }
}
int lsv_printcut(Abc_Frame_t *pAbc, int argc, char **argv)
{
    int k = atoi(argv[1]);
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

    lsv_k_feasible(pNtk, k);
    return 0;

usage:
    Abc_Print(-2, "usage: lsv_print_nodes [-h]\n");
    Abc_Print(-2, "\t        prints the nodes in the network\n");
    Abc_Print(-2, "\t-h    : print the command usage\n");
    return 1;
}