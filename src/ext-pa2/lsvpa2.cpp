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

void init_pa2(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sdc", Lsv_CommandSDC, 0);
}

void destroy_pa2(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer_pa2 = {init_pa2, destroy_pa2};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer_pa2); }
} lsvPackageRegistrationManager_pa2;

void Lsv_NtkPNodes(Abc_Ntk_t* pNtk) {
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

bool RandomSimulation(Aig_Man_t * pAig, std::vector<int>& appear_cnt, int nPatterns) {
    Aig_Obj_t * pObj;
    int i, j;

    // 遍歷多組輸入模式
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
        // 隨機設置所有 PI 值
        std::cout<<"Generate Pattern "<<i<<std::endl;
        Aig_ManForEachCi(pAig, pObj, j) {
            int* randomValue = new int(rand() & 1); // 隨機生成 0 或 1
            std::cout<<Aig_ObjId(pObj)<<": "<<*randomValue<<std::endl;
            pObj->pData = (void *)randomValue;
        }
        std::cout<<"END Generate Pattern "<<i<<std::endl;

        // 模擬所有節點
        Aig_ManForEachNode(pAig, pObj, j) 
        {
            int value0 = *(int*)Aig_ObjFanin0(pObj)->pData;
            int value1 = *(int*)Aig_ObjFanin1(pObj)->pData;
            if(Aig_ObjFaninC0(pObj))
                value0 = !value0;
            if(Aig_ObjFaninC1(pObj))
                value1 = !value1;

            int* result = new int(value0 & value1); // 假設是 AND 節點
            std::cout<<"FANIN0  "<<Aig_ObjId(Aig_ObjFanin0(pObj))<<" "<<value0<<std::endl;
            std::cout<<"FANIN1  "<<Aig_ObjId(Aig_ObjFanin1(pObj))<<" "<<value1<<std::endl;
            std::cout<<"CUR: "<<Aig_ObjId(pObj)<<" "<<*result<<std::endl;
            pObj->pData = (void *)result;
        }

        // 打印 PO 值
        Aig_ManForEachCo(pAig, pObj, j) {
            int outputValue = *(int*)Aig_ObjFanin0(pObj)->pData;
            Aig_Obj_t* pOUT = Aig_ObjFanin0(pObj);  //true output
            int value0 = *(int*)Aig_ObjFanin0(pOUT)->pData;
            int value1 = *(int*)Aig_ObjFanin1(pOUT)->pData;
            if(Aig_ObjFaninC0(pOUT))
                value0 = !value0;
            if(Aig_ObjFaninC1(pOUT))
                value1 = !value1;

            std::cout<<value0<<" "<<value1<<std::endl;
            if(!value0 && !value1)
                appear_cnt[0]++;
            else if(value0 && !value1)
                appear_cnt[1]++;
            else if(!value0 && value1)
                appear_cnt[2]++;
            else
                appear_cnt[3]++;
                
            printf("Pattern %d, PO %d: %d\n", i, j, outputValue);
        }
    }
    return 0;
}

bool solve_sdc(Aig_Man_t* pMan, bool v0, bool v1)
{
    Cnf_Dat_t * pCnf = Cnf_Derive(pMan, 0);
    Aig_Obj_t * HEAD = Aig_ObjFanin0(Aig_ManCo(pMan, 0));
    Aig_Obj_t * pY0 = Aig_ObjFanin0(HEAD);
    Aig_Obj_t * pY1 = Aig_ObjFanin1(HEAD);
    
    int varY0 = pCnf->pVarNums[Aig_ObjId(pY0)];
    int varY1 = pCnf->pVarNums[Aig_ObjId(pY1)];
    
    if(Aig_ObjFaninC0(HEAD))
        v0 = !v0;
    if(Aig_ObjFaninC1(HEAD))
        v1 = !v1;
    std::cout<<"Y0_ID: "<<Aig_ObjId(pY0)<<" Y1_ID: "<<Aig_ObjId(pY1)<<std::endl;
    std::cout<<"Y0: "<<v0<<" Y1: "<<v1<<std::endl;
    std::cout<<"Y0_VAR: "<<varY0<<" Y1_VAR: "<<varY1<<std::endl;
    lit assumptions[2];
    assumptions[0] = toLitCond(varY0, !v0);
    assumptions[1] = toLitCond(varY1, !v1); //if Y0 needs to be 1 then use 0 = =
    sat_solver* pSat = (sat_solver*)Cnf_DataWriteIntoSolver(pCnf, 1, 0);
    if(!sat_solver_addclause(pSat, assumptions, assumptions + 2))
        return 0;

    int i;
    Aig_Obj_t * pObj;
    Aig_ManForEachNode(pMan, pObj, i) 
    {
        std::cout<<pCnf->pVarNums[Aig_ObjId(pObj)]<<std::endl;
    }

    const int nInitVars = pCnf->nVars,
              nCi = Aig_ManCiNum(pMan);
    //std::cout<<"vars: "<<nInitVars<<" pi: "<<nCi<<std::endl;

    int status = sat_solver_solve(pSat, NULL, NULL, 0, 0, 0, 0);
    for (int i = 0; i < pCnf->nVars; i++)
    {
        int value = sat_solver_var_value(pSat, i);
        printf("SAT 变量 %d = %d\n", i, value);
    }


    sat_solver_delete(pSat);
    std::cout<<status<<std::endl;
    return status == l_True;
}

int Lsv_CommandSDC(Abc_Frame_t* pAbc, int argc, char** argv) 
{
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);   
    int node_idx = std::stoi(argv[1]);
    std::cout<<node_idx<<std::endl; 
    Abc_Obj_t* HEAD = Abc_NtkObj(pNtk, node_idx);
    std::cout<<Abc_ObjName(HEAD)<<std::endl;

    Abc_Ntk_t* pNtkAig = Abc_NtkCreateCone( pNtk, HEAD,"obj_ntk", 1);
    
    std::cout<<"MAKE CONE"<<std::endl;  
    std::cout<<Abc_NtkPiNum(pNtkAig)<<std::endl;

    Aig_Man_t * pMan = Abc_NtkToDar( pNtkAig, 0, 0 );   //make aig circuit



    std::vector<int> appear_cnt;
    appear_cnt.resize(4,0);
    bool all_sat = RandomSimulation(pMan,appear_cnt, 100); //simulate 10 times
    if(all_sat)
    {
        std::cout<<"no sdc"<<std::endl;
        return 0;
    }

    for(int i = 0 ; i < 4; i++)
    {
        std::cout<<appear_cnt[i]<<std::endl;
    }
    for(int i = 0 ; i < 4; i++)
    {
        if(appear_cnt[i] == 0)
        {
            std::cout<<"Y COND:"<<(i&1)<<(i&2>>1)<<std::endl;
            bool res = solve_sdc(pMan,i&1,i&2>>1);
            if(!res)
            {
                std::cout<<"SDC: ";
                std::cout<<std::bitset<2>(i)<<std::endl;
                return 0;
            }
        }
    }
    std::cout<<"no sdc"<<std::endl;
    return 0;

    

usage:
    Abc_Print(-2, "usage: lsv_print_nodes [-h]\n");
    Abc_Print(-2, "\t        prints the nodes in the network\n");
    Abc_Print(-2, "\t-h    : print the command usage\n");
    return 1;
}
