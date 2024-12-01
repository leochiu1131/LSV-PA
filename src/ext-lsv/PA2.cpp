#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <vector>
#include <string>
#include <set>
#include <cstdlib>
#include <algorithm>
#include <iostream>
#include "sat/cnf/cnf.h"
extern "C"
{
    Aig_Man_t *Abc_NtkToDar(Abc_Ntk_t *pNtk, int fExors, int fRegisters);
}
using namespace std;
static int lsv_sdc(Abc_Frame_t *pAbc, int argc, char **argv);
static int lsv_odc(Abc_Frame_t *pAbc, int argc, char **argv);

void init(Abc_Frame_t *pAbc)
{
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sdc", lsv_sdc, 0);
    Cmd_CommandAdd(pAbc, "LSV", "lsv_odc", lsv_odc, 0);
}

void destroy(Abc_Frame_t *pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager
{
    PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;
unsigned int *Abc_NtkVerifySimulatePattern_modify(Abc_Ntk_t *pNtk, unsigned int *pModel, Abc_Obj_t *ptargetNode)
{
    Abc_Obj_t *pNode;
    unsigned int *pValues, Value0, Value1, i, V1, V2;
    int fStrashed = 0;
    if (!Abc_NtkIsStrash(pNtk))
    {
        pNtk = Abc_NtkStrash(pNtk, 0, 0, 0);
        fStrashed = 1;
    }
    /*
        printf( "Counter example: " );
        Abc_NtkForEachCi( pNtk, pNode, i )
            printf( " %d", pModel[i] );
        printf( "\n" );
    */
    // increment the trav ID
    Abc_NtkIncrementTravId(pNtk);
    // set the CI values
    Abc_AigConst1(pNtk)->pCopy = (Abc_Obj_t *)1;
    Abc_NtkForEachCi(pNtk, pNode, i)
    {
        pNode->pCopy = (Abc_Obj_t *)(ABC_PTRINT_T)pModel[i];
        // cout << pModel[i] << endl;
    }
    // simulate in the topological order
    Abc_NtkForEachNode(pNtk, pNode, i)
    {
        // Value0 = ((int)(ABC_PTRINT_T)Abc_ObjFanin0(pNode)->pCopy) ^ (int)Abc_ObjFaninC0(pNode);
        // Value1 = ((int)(ABC_PTRINT_T)Abc_ObjFanin1(pNode)->pCopy) ^ (int)Abc_ObjFaninC1(pNode);
        if ((unsigned int)Abc_ObjFaninC0(pNode))
            Value0 = ~((unsigned int)(ABC_PTRINT_T)Abc_ObjFanin0(pNode)->pCopy);
        else
            Value0 = (unsigned int)(ABC_PTRINT_T)Abc_ObjFanin0(pNode)->pCopy;
        if ((unsigned int)Abc_ObjFaninC1(pNode))
            Value1 = ~((unsigned int)(ABC_PTRINT_T)Abc_ObjFanin1(pNode)->pCopy);
        else
            Value1 = (unsigned int)(ABC_PTRINT_T)Abc_ObjFanin1(pNode)->pCopy;
        pNode->pCopy = (Abc_Obj_t *)(ABC_PTRINT_T)(Value0 & Value1);
        if (pNode == ptargetNode)
        {
            V1 = Value0;
            V2 = Value1;
        }
        // cout << pNode->Id << (unsigned int)(ABC_PTRINT_T)Abc_ObjFanin0(pNode)->pCopy << " " << V1 << " " << (unsigned int)(ABC_PTRINT_T)Abc_ObjFanin1(pNode)->pCopy << " " << V2 << endl;
    }
    // fill the output values
    pValues = ABC_ALLOC(unsigned int, Abc_NtkCoNum(pNtk) + 2);
    Abc_NtkForEachCo(pNtk, pNode, i)
    {
        if ((unsigned int)Abc_ObjFaninC0(pNode))
            pValues[i] = ~((unsigned int)(ABC_PTRINT_T)Abc_ObjFanin0(pNode)->pCopy);
        else
            pValues[i] = (unsigned int)(ABC_PTRINT_T)Abc_ObjFanin0(pNode)->pCopy;
    }
    // cout << i << " " << V1 << " " << V2 << endl;
    pValues[i] = V1;
    pValues[i + 1] = V2;
    // pValues[i] = ((int)(ABC_PTRINT_T)Abc_ObjFanin0(pNode)->pCopy) ^ (int)Abc_ObjFaninC0(pNode);
    if (fStrashed)
        Abc_NtkDelete(pNtk);
    return pValues;
}
set<pair<int, int>> computesdc(Abc_Ntk_t *pNtk, Abc_Obj_t *pNode)
{

    Vec_Ptr_t *vFanins = Vec_PtrAlloc(2);
    Abc_Obj_t *pFanin, *pPi;
    int i, j;
    Abc_ObjForEachFanin(pNode, pFanin, i)
    {
        Vec_PtrPush(vFanins, pFanin);
    }
    Abc_Ntk_t *pCone = Abc_NtkCreateConeArray(pNtk, vFanins, 1);

    Aig_Man_t *pMan = Abc_NtkToDar(pCone, 0, 0);
    Cnf_Dat_t *pCnf = Cnf_Derive(pMan, 2);
    // Aig_Obj_t *pObj;
    // int nFanins = 2;
    int count = 0;
    int CoNum = Abc_NtkCoNum(pNtk);
    unsigned int *pValues;
    unsigned int *pModel;
    pModel = new unsigned int(Abc_NtkPiNum(pNtk));

    Abc_NtkForEachPi(pNtk, pPi, j)
    {
        pModel[i] = rand();
    }
    pValues = Abc_NtkVerifySimulatePattern_modify(pNtk, pModel, pNode);
    set<pair<int, int>> sim_pattern;
    int numBits = sizeof(unsigned int) * 8;
    // cout << pValues[CoNum] << " " << pValues[CoNum + 1] << endl;
    for (int i = 0; i < numBits; ++i)
    {
        int bit1 = (pValues[CoNum] >> i) & 1;
        int bit2 = (pValues[CoNum + 1] >> i) & 1;
        sim_pattern.insert({bit1, bit2});
    }
    // for (const auto &p : sim_pattern)
    // {
    //     std::cout << p.first << p.second << std::endl;
    // }

    vector<pair<int, int>> ori_pattern;
    ori_pattern.push_back({0, 0});
    ori_pattern.push_back({0, 1});
    ori_pattern.push_back({1, 0});
    ori_pattern.push_back({1, 1});
    pair<int, int> pattern;
    int sim_p_num = sim_pattern.size();
    set<pair<int, int>> sdc;
    for (int i = 0; i < 4; ++i)
    {
        pattern = ori_pattern[i];
        // cout << sim_p_num << endl;
        sim_p_num = sim_pattern.size();
        sim_pattern.insert(pattern);
        // cout << sim_pattern.size() << endl;
        if (sim_pattern.size() == sim_p_num)
            continue;
        // cout << pattern.first << " " << pattern.second << endl;
        // cout << i << endl;
        sat_solver *pSolver = sat_solver_new();
        Cnf_DataWriteIntoSolverInt(pSolver, pCnf, 1, 0);
        vector<int> clause1, clause2;

        Aig_Obj_t *f0 = Aig_ObjFanin0(Aig_ManCo(pMan, 0));
        Aig_Obj_t *f1 = Aig_ObjFanin0(Aig_ManCo(pMan, 1));

        int var1 = pCnf->pVarNums[Aig_ObjId(f0)];
        // cout << !(pattern & (1 << 0)) << " " << !(pattern & (1 << 1)) << "  ";

        int lit1 = Abc_Var2Lit(var1, !(pattern.first ^ pNode->fCompl0));
        clause1.push_back(lit1);
        sat_solver_addclause(pSolver, clause1.data(), clause1.data() + clause1.size());

        int var2 = pCnf->pVarNums[Aig_ObjId(f1)];
        int lit2 = Abc_Var2Lit(var2, !(pattern.second ^ pNode->fCompl1));
        clause2.push_back(lit2);
        sat_solver_addclause(pSolver, clause2.data(), clause2.data() + clause2.size());
        // cout << var1 << " " << ((!(pattern & (1 << 0))) ^ pNode->fCompl0) << "  ";
        // cout << var2 << " " << ((!(pattern & (1 << 1))) ^ pNode->fCompl1) << "  ";
        int status = sat_solver_solve(pSolver, NULL, NULL, 0, 0, 0, 0);
        // cout << status << "  ";
        if (status == l_False)
        {
            count++;
            sdc.insert(pattern);
            // cout << pattern.first << pattern.second << " ";
        }
        sat_solver_delete(pSolver);
    }
    // if (count == 0)
    // cout << "no sdc";
    // cout << endl;
    Vec_PtrFree(vFanins);
    Abc_NtkDelete(pCone);
    Cnf_DataFree(pCnf);
    Aig_ManStop(pMan);
    return sdc;
}

int lsv_sdc(Abc_Frame_t *pAbc, int argc, char **argv)
{
    int k = atoi(argv[1]);
    Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
    Abc_Obj_t *pNode = Abc_NtkObj(pNtk, k);
    int c;
    Extra_UtilGetoptReset();
    set<pair<int, int>> sdc;
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
    sdc = computesdc(pNtk, pNode);
    if (sdc.size() == 0)
        cout << "no sdc" << endl;
    else
    {
        for (auto &pattern : sdc)
            cout << pattern.first << pattern.second << " ";
        cout << endl;
    }
    return 0;

usage:
    Abc_Print(-2, "usage: lsv_sdc [-h]\n");
    Abc_Print(-2, "\t        prints the nodes in the network\n");
    Abc_Print(-2, "\t-h    : print the command usage\n");
    return 1;
}
void computeODC(Abc_Ntk_t *pNtk, Abc_Obj_t *pNode, int k)
{
    set<pair<int, int>> sdc;
    sdc = computesdc(pNtk, pNode);
    Abc_Ntk_t *pNegNtk = Abc_NtkDup(pNtk);
    Abc_Obj_t *pCompNode = Abc_NtkObj(pNegNtk, k);
    Abc_Obj_t *pFanout;
    bool is_FaninC0 = Abc_ObjFaninC0(pNode);
    bool is_FaninC1 = Abc_ObjFaninC1(pNode);
    // std::cout << is_FaninC0 << is_FaninC1 << std::endl;
    int i;
    Abc_ObjForEachFanout(pCompNode, pFanout, i)
    {
        Abc_Obj_t *pFanin;
        int j;
        Abc_ObjForEachFanin(pFanout, pFanin, j)
        {
            if (pFanin == pCompNode)
            {
                Abc_ObjXorFaninC(pFanout, j);
            }
        }
    }
    Abc_Ntk_t *pMiterNtk = Abc_NtkMiter(pNtk, pNegNtk, 1, 0, 0, 0);
    Vec_Ptr_t *vFanins = Vec_PtrAlloc(2);
    Abc_Obj_t *pFanin0 = Abc_ObjFanin0(pNode);
    Abc_Obj_t *pFanin1 = Abc_ObjFanin1(pNode);
    Vec_PtrPush(vFanins, pFanin0);
    Vec_PtrPush(vFanins, pFanin1);
    Abc_Ntk_t *pCone = Abc_NtkCreateConeArray(pNtk, vFanins, 1);

    Abc_NtkAppend(pMiterNtk, pCone, 1);
    Aig_Man_t *pMan = Abc_NtkToDar(pMiterNtk, 0, 0);

    sat_solver *pSolver = sat_solver_new();
    Cnf_Dat_t *pCnf = Cnf_Derive(pMan, 3);
    Cnf_DataWriteIntoSolverInt(pSolver, pCnf, 1, 0);
    // vector<int> clause1;
    int var1 = pCnf->pVarNums[Aig_ManCo(pMan, 0)->Id];
    int var2 = pCnf->pVarNums[Aig_ManCo(pMan, 1)->Id];
    int var3 = pCnf->pVarNums[Aig_ManCo(pMan, 2)->Id];

    lit lit1[1];
    lit1[0] = Abc_Var2Lit(var1, 0);
    // // clause1.push_back(lit1);
    sat_solver_addclause(pSolver, lit1, lit1 + 1);
    int status = sat_solver_solve(pSolver, NULL, NULL, 0, 0, 0, 0);
    vector<int> care_set({0, 0, 0, 0});
    while (status == l_True)
    {
        lit lit2[2];
        int value1 = sat_solver_var_value(pSolver, var2);
        int value2 = sat_solver_var_value(pSolver, var3);
        lit2[0] = Abc_Var2Lit(var2, value1);
        lit2[1] = Abc_Var2Lit(var3, value2);
        int value1_invert = value1, value2_invert = value2;
        if (is_FaninC0)
            value1_invert = !value1_invert;
        if (is_FaninC1)
            value2_invert = !value2_invert;
        care_set[value1_invert * 2 + value2_invert] = 1;
        // cout << value1 << value2 << endl;
        // cout << value1_invert << value2_invert << endl;
        // cout << (value1_invert) * 2 + (value2_invert) << endl;
        // cout << endl;
        if (!sat_solver_addclause(pSolver, lit2, lit2 + 2))
            break;
        status = sat_solver_solve(pSolver, NULL, NULL, 0, 0, 0, 0);
    }
    int count = 0;
    for (int x = 0; x < care_set.size(); x++)
    {
        if (care_set[x] == 0)
        {
            int first = x / 2;
            int second = x % 2;
            if (sdc.find({first, second}) == sdc.end())
            {
                cout << first << second << " ";
                count++;
            }
        }
    }
    // cout << count;
    if (count == 0)
        cout << "no odc";
    cout << endl;
    Abc_NtkDelete(pNegNtk);
    Abc_NtkDelete(pMiterNtk);
    Cnf_DataFree(pCnf);
    sat_solver_delete(pSolver);
}
int lsv_odc(Abc_Frame_t *pAbc, int argc, char **argv)
{
    int k = atoi(argv[1]);
    Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
    Abc_Obj_t *pNode = Abc_NtkObj(pNtk, k);
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
    computeODC(pNtk, pNode, k);
    return 0;

usage:
    Abc_Print(-2, "usage: lsv_odc [-h]\n");
    Abc_Print(-2, "\t        prints the nodes in the network\n");
    Abc_Print(-2, "\t-h    : print the command usage\n");
    return 1;
}