#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "proof/int/intInt.h"
#include <iostream>
#include<list>
#include<vector>
#include<map>
#include<unordered_map>
#include<set>
#include<algorithm>
#include<bitset>



extern "C" Aig_Man_t * Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );

static int Lsv_CommandSDC(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandODC(Abc_Frame_t* pAbc, int argc, char** argv);


void init_pa2(Abc_Frame_t* pAbc) 
{
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sdc", Lsv_CommandSDC, 0);
    Cmd_CommandAdd(pAbc, "LSV", "lsv_odc", Lsv_CommandODC, 0);
}

void destroy_pa2(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer_pa2 = {init_pa2, destroy_pa2};

struct lsvPackageRegistrationManager_PA2
{
    lsvPackageRegistrationManager_PA2() { Abc_FrameAddInitializer(&frame_initializer_pa2); }
} lsvPackageRegistrationManager_PA2;



bool RandomSimulation(Aig_Man_t * pAig, std::vector<int>& appear_cnt, int nPatterns) {
    Aig_Obj_t * pObj;
    int i, j;

    
    for (i = 0; i < nPatterns; i++) {
        Aig_ManForEachNode(pAig, pObj, j) 
        {
            if(pObj->pData != NULL)
                delete pObj->pData;
            pObj->pData = NULL;
        }
        bool all_sat = 1;
        for(int j = 0 ; j < appear_cnt.size(); j++)
        {
            if(appear_cnt[j]==0)
            {
                all_sat = 0;
                break;
            }
        }
        if(all_sat)
        {
            return 1;
        }
        
        Aig_ManForEachCi(pAig, pObj, j) {
            int* randomValue = new int(rand() & 1);
            pObj->pData = (void *)randomValue;
        }

        
        Aig_ManForEachNode(pAig, pObj, j) 
        {
            int value0 = *(int*)Aig_ObjFanin0(pObj)->pData;
            int value1 = *(int*)Aig_ObjFanin1(pObj)->pData;
            if(Aig_ObjFaninC0(pObj))
                value0 = !value0;
            if(Aig_ObjFaninC1(pObj))
                value1 = !value1;

            int* result = new int(value0 & value1); 
            pObj->pData = (void *)result;
        }

        int value0 = *(int*)Aig_ObjFanin0(Aig_ManCo(pAig,0))->pData;
        int value1 = *(int*)Aig_ObjFanin0(Aig_ManCo(pAig,1))->pData;
        if(!value0 && !value1)
            appear_cnt[0]++;
        else if(value0 && !value1)
            appear_cnt[1]++;
        else if(!value0 && value1)
            appear_cnt[2]++;
        else
            appear_cnt[3]++;
    }
    return 0;
}

bool solve_sdc(Aig_Man_t* pMan, bool v0, bool v1)
{
    Cnf_Dat_t * pCnf = Cnf_Derive(pMan, 2);
    Aig_Obj_t * pY0 = Aig_ObjFanin0(Aig_ManCo(pMan, 0));
    Aig_Obj_t * pY1 = Aig_ObjFanin0(Aig_ManCo(pMan, 1));
    
    int varY0 = pCnf->pVarNums[Aig_ObjId(pY0)];
    int varY1 = pCnf->pVarNums[Aig_ObjId(pY1)];

    lit assumptions[2];
    assumptions[0] = toLitCond(varY0, !v0);
    assumptions[1] = toLitCond(varY1, !v1); //if Y0 needs to be 1 then use 0 = =
    sat_solver* pSat = (sat_solver*)Cnf_DataWriteIntoSolver(pCnf, 1, 0);
    
    if(sat_solver_addclause(pSat, assumptions, assumptions + 1) == 0)
    {
        return 0;
    }
    if(sat_solver_addclause(pSat, assumptions+1, assumptions + 2) == 0)
    {
        return 0;
    }
    

    int i;
    Aig_Obj_t * pObj;


    int status = sat_solver_solve(pSat, 0,0, 0, 0, 0, 0);
    for (int i = 0; i < pCnf->nVars; i++)
    {
        int value = sat_solver_var_value(pSat, i);
    }


    sat_solver_delete(pSat);
    return status == l_True;
}

std::vector<bool> obtain_sdc(Abc_Frame_t* pAbc, int argc, char** argv) ;
int Lsv_CommandSDC(Abc_Frame_t* pAbc, int argc, char** argv) 
{
    std::vector<bool> SDCs = obtain_sdc(pAbc, argc, argv);
    if(SDCs.size() == 0)
    {
        std::cout<<"no sdc"<<std::endl;
        return 0;
    }
    for(int i = 0 ; i < 4 ; i++)
    {
        if(SDCs[i])
        {
            std::cout<<std::bitset<2>(i)<<" ";
        }
    }
    std::cout<<std::endl;
    return 0;
}



std::vector<bool> obtain_sdc(Abc_Frame_t* pAbc, int argc, char** argv) 
{
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);   
    int node_idx = std::stoi(argv[1]);
    Abc_Obj_t* HEAD = Abc_NtkObj(pNtk, node_idx);
    Abc_Obj_t* Y0 = Abc_ObjFanin0(HEAD);
    Abc_Obj_t* Y1 = Abc_ObjFanin1(HEAD);

    Vec_Ptr_t* vNodes = Vec_PtrAlloc(2);
    Vec_PtrPush(vNodes, Y0);
    Vec_PtrPush(vNodes, Y1);

    Abc_Ntk_t* pNtkAig1 = Abc_NtkCreateConeArray( pNtk, vNodes, 1);
     

    Aig_Man_t * pMan1 = Abc_NtkToDar( pNtkAig1, 0, 0 );


    std::vector<int> appear_cnt;
    appear_cnt.resize(4,0);
    bool all_sat = 0;
    
    all_sat = RandomSimulation(pMan1,appear_cnt, 8);

    if(all_sat)
    {
        return std::vector<bool>{};
    }

    std::vector<bool> SDCs;
    SDCs.resize(4,0);

    bool res = 1;
    for(int i = 0 ; i < 4; i++)
    {
            bool cur = solve_sdc(pMan1,i&1,(i&2)>>1);
            res = res & cur;
            if(!cur)
            {
                int value0 = Abc_ObjFaninC0(HEAD) ? !(i&1) : i&1;
                int value1 = Abc_ObjFaninC1(HEAD) ? !((i&2)>>1) : (i&2)>>1;
                SDCs[value0*2+value1] = 1;
            }

    }

    if(res)
        return std::vector<bool>{};
    else
        return SDCs;
}

int Lsv_CommandODC(Abc_Frame_t* pAbc, int argc, char** argv) 
{
    Abc_Ntk_t* C = Abc_FrameReadNtk(pAbc);   
    Abc_Ntk_t * Cp = Abc_NtkDup(C);

    int node_idx = std::stoi(argv[1]);
    

    Abc_Obj_t* HEAD = Abc_NtkObj(C, node_idx);
    Abc_Obj_t* Y0 = Abc_ObjFanin0(HEAD);
    Abc_Obj_t* Y1 = Abc_ObjFanin1(HEAD);
    bool is_FaninC0 = Abc_ObjFaninC0(HEAD);
    bool is_FaninC1 = Abc_ObjFaninC1(HEAD);

    Vec_Ptr_t* vNodes = Vec_PtrAlloc(2);
    Vec_PtrPush(vNodes, Y0);
    Vec_PtrPush(vNodes, Y1);

    Abc_Ntk_t* pNtkAig1 = Abc_NtkCreateConeArray( C, vNodes, 1);
    int i;
    Abc_Obj_t * Abc_pObj;
    Abc_Obj_t* INV_node = Abc_NtkObj(Cp, node_idx);
    Abc_NtkForEachNode(Cp,Abc_pObj,i)
    {
        if(Abc_ObjFanin0(Abc_pObj) == INV_node)
            Abc_ObjXorFaninC(Abc_pObj, 0);
        if(Abc_ObjFanin1(Abc_pObj) == INV_node)
            Abc_ObjXorFaninC(Abc_pObj, 1);
    }
    Abc_NtkForEachCo(Cp,Abc_pObj,i)
    {
        if(Abc_ObjFanin0(Abc_pObj) == INV_node)
            Abc_ObjXorFaninC(Abc_pObj, 0);
    }

    

    Abc_Ntk_t * pNtkMiter = Abc_NtkMiter(C, Cp, 1, 0, 0, 0);

    

    if(!Abc_NtkAppend(pNtkMiter, pNtkAig1,1)) std::cout<<"ERROR"<<std::endl;
    
    Aig_Man_t * pMan = Abc_NtkToDar( pNtkMiter, 0, 0 );
    Cnf_Dat_t * pCnf = Cnf_Derive(pMan, 3);

    Aig_Obj_t * pY = Aig_ManCo(pMan, 0);
    int varY = pCnf->pVarNums[Aig_ObjId(pY)];
    lit assumptions[1];
    assumptions[0] = toLitCond(varY, 0);  
    sat_solver* pSat = (sat_solver*)Cnf_DataWriteIntoSolver(pCnf, 1, 0);
    sat_solver_addclause(pSat, assumptions, assumptions + 1);

    Aig_Obj_t * Aig_pObj;

    Abc_Obj_t * pObj;

    int Y0_cnf_idx = pCnf->pVarNums[Aig_ObjId(Aig_ObjFanin0(Aig_ManCo(pMan, 1)))];
    int Y1_cnf_idx = pCnf->pVarNums[Aig_ObjId(Aig_ObjFanin0(Aig_ManCo(pMan, 2)))];
    int cnt = 0;

    std::vector<bool> CARE_set;
    CARE_set.resize(4,0);
    while(sat_solver_solve(pSat,0,0,0,0,0,0)==l_True)
    {
        cnt++;

        int value0 = sat_solver_var_value(pSat, Y0_cnf_idx);
        int value1 = sat_solver_var_value(pSat, Y1_cnf_idx);

        
        int insert_v0 = value0;
        int insert_v1 = value1;
        if(is_FaninC0)
            insert_v0 = !insert_v0;
        if(is_FaninC1)
            insert_v1 = !insert_v1;
        CARE_set[insert_v0*2+insert_v1] = 1;

        
        //print all sat val
        lit blocking_clause[2];
        blocking_clause[0] = toLitCond(Y0_cnf_idx, value0);
        blocking_clause[1] = toLitCond(Y1_cnf_idx, value1);

        if(!sat_solver_addclause(pSat, blocking_clause, blocking_clause + 2))
            break;
    }
    sat_solver_delete(pSat);
    std::vector<bool> SDCs = obtain_sdc(pAbc, argc, argv);
    if(SDCs.size() == 0)
        SDCs.resize(4,0);
    


    std::vector<bool>& ODCs = CARE_set;
    for(int i = 0 ; i < 4 ; i++)
        ODCs[i] = !ODCs[i];
    

    for(int i = 0 ; i < 4 ; i++)
    {
        if(SDCs[i])
            ODCs[i] = 0;
    }
    
    bool no_odc = 1;
    for(int i = 0 ; i < 4 ; i++)
        if(ODCs[i])
            no_odc = 0;
    if(no_odc)
    {
        std::cout<<"no odc"<<std::endl;
    }
    else
    {
        for(int i = 0 ; i < 4 ; i++)
        {
            if(ODCs[i])
            {
                std::cout<<std::bitset<2>(i)<<" ";
            }
        }
        std::cout<<std::endl;
    }
    
    return 0;
}

