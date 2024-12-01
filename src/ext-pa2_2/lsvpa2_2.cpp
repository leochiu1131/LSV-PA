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

static int Lsv_CommandODC(Abc_Frame_t* pAbc, int argc, char** argv);

void init_pa22(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_odc", Lsv_CommandODC, 0);
}

void destroy_pa22(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer_pa22 = {init_pa22, destroy_pa22};


int Lsv_CommandODC(Abc_Frame_t* pAbc, int argc, char** argv) 
{
    Abc_Ntk_t* C = Abc_FrameReadNtk(pAbc);   
    Abc_Ntk_t * Cp = Abc_NtkDup(C);

    int node_idx = std::stoi(argv[1]);
    Abc_Obj_t* INV_node = Abc_NtkObj(Cp, node_idx);

    Abc_Obj_t* HEAD = Abc_NtkObj(C, node_idx);
    Abc_Obj_t* Y0 = Abc_ObjFanin0(HEAD);
    Abc_Obj_t* Y1 = Abc_ObjFanin1(HEAD);


    Vec_Ptr_t* vNodes = Vec_PtrAlloc(2);
    Vec_PtrPush(vNodes, Y0);
    Vec_PtrPush(vNodes, Y1);

    Abc_Ntk_t* pNtkAig1 = Abc_NtkCreateConeArray( C, vNodes, 1);
    
    int i;
    Abc_Obj_t * Abc_pObj;
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
    int node_size = Abc_NtkNodeNum(pNtkMiter) + Abc_NtkPiNum(pNtkMiter) + Abc_NtkPoNum(pNtkMiter);
    Vec_Ptr_t* All_nodes = Vec_PtrAlloc(node_size);
    Abc_NtkForEachCo(pNtkMiter, Abc_pObj, i)
    {
        Vec_PtrPush(All_nodes, Abc_pObj);
    }
    Abc_NtkForEachCi(pNtkMiter, Abc_pObj, i)
    {
        Vec_PtrPush(All_nodes, Abc_pObj);
    }
    Abc_NtkForEachNode(pNtkMiter, Abc_pObj, i)
    {
        Vec_PtrPush(All_nodes, Abc_pObj);
    }
    Abc_Ntk_t* All_NODE_CIR = Abc_NtkCreateConeArray( pNtkMiter, All_nodes, node_size);

    std::cout<<1<<std::endl;
    Aig_Man_t * pMan = Abc_NtkToDar( All_NODE_CIR, 0, 0 );

    Cnf_Dat_t * pCnf = Cnf_Derive(pMan, node_size);


    Aig_Obj_t * pY = Aig_ObjFanin0(Aig_ManCo(pMan, 0));
    int varY = pCnf->pVarNums[Aig_ObjId(pY)];
    std::cout<<varY<<std::endl;
    lit assumptions[1];
    assumptions[0] = toLitCond(varY, 1);
    sat_solver* pSat = (sat_solver*)Cnf_DataWriteIntoSolver(pCnf, 1, 0);
    sat_solver_addclause(pSat, assumptions, assumptions + 1);

   std::cout<<"ID"<<std::endl;
   
    Aig_Obj_t * Aig_pObj;
    Aig_ManForEachNode(pMan, Aig_pObj, i) 
    {
        std::cout<<Aig_ObjId(Aig_pObj)<<"   "<<pCnf->pVarNums[Aig_ObjId(Aig_pObj)]<<std::endl;
        std::cout<<Aig_ObjId(Aig_ObjFanin0(Aig_pObj))<<" "<<Aig_ObjId(Aig_ObjFanin1(Aig_pObj))<<std::endl;
    }

    std::cout<<std::endl;
    std::cout<<std::endl;
    Abc_NtkForEachNode(All_NODE_CIR, Abc_pObj, i) 
    {
        std::cout<<Abc_ObjId(Abc_pObj)<<"   "<<std::endl;
        std::cout<<Abc_ObjId(Abc_ObjFanin0(Abc_pObj))<<" "<<Abc_ObjId(Abc_ObjFanin1(Abc_pObj))<<std::endl;
    }

    int Y0_cnf_idx = pCnf->pVarNums[Aig_ObjId(Aig_ObjFanin0(Aig_ManCo(pMan, 1)))];
    int Y1_cnf_idx = pCnf->pVarNums[Aig_ObjId(Aig_ObjFanin0(Aig_ManCo(pMan, 2)))];

    int cnt = 0;
    while(sat_solver_solve(pSat,0,0,0,0,0,0)==l_True)
    {
        std::cout<<"SAT"<<std::endl;
        cnt++;
        if(cnt==4) break;
        int value0 = sat_solver_var_value(pSat, Y0_cnf_idx);
        printf("SAT  0 = %d\n", value0);
        int value1 = sat_solver_var_value(pSat, Y1_cnf_idx);
        printf("SAT  1 = %d\n", value1);
        //print all sat val
        lit blocking_clause[2];
        blocking_clause[0] = toLitCond(Y0_cnf_idx, value0);
        blocking_clause[1] = toLitCond(Y1_cnf_idx, value1);
        if(!sat_solver_addclause(pSat, blocking_clause, blocking_clause + 2))
            break;
        std::cout<<"BLOCK"<<std::endl;
    }

    std::cout<<cnt<<std::endl;

    return 0;
}