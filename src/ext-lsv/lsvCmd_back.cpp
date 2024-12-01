#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include<set>
#include<unordered_map>
#include<map>
#include<vector>
#include<algorithm>
#include<iostream>

#include "sat/cnf/cnf.h"
 extern "C"{
 Aig_Man_t* Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
 }

using std::vector;
using std::cout;
using std::cerr;

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandKFeasibleCut(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSdc(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandOdc(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_CommandKFeasibleCut, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sdc", Lsv_CommandSdc, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_odc", Lsv_CommandOdc, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;


// =========================================
//           LSV PA2 Part1
//          Lsv_CommandSdc
// =========================================
int * Lsv_NtkVerifySimulatePattern( Abc_Ntk_t * pNtk, int * pModel );

void SortBoolVector(std::vector<std::pair<int, int> > &vec){
    sort(vec.begin(), vec.end());
}

bool Lsv_CalSdc(Abc_Ntk_t* pNtk, int node_id, std::vector<std::pair<int, int> >& sdc_vec);
void Lsv_PrintSdc(Abc_Ntk_t* pNtk, int node_id);

int Lsv_CommandSdc(Abc_Frame_t* pAbc, int argc, char** argv) {
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    int c;
    int node_id;
  
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
    if (argc != 2){
        Abc_Print(-1, "Please input the node id.\n");
        return 1;
    }
    node_id = atoi(argv[1]);
    Lsv_PrintSdc(pNtk, node_id);
    return 0;

usage:
    Abc_Print(-2, "usage: lsv_sdc [-h]\n");
    Abc_Print(-2, "\t        prints the satisfiability don't cares (sdc) of the given node\n");
    Abc_Print(-2, "\t-h    : print the command usage\n");
    return 1;
}


bool Lsv_CalSdc(Abc_Ntk_t* pNtk, int node_id, std::vector<std::pair<int, int> >& sdc_vec){
    /*
        Calculate the sdc of the node_id in the pNtk
        return true if there is sdc
        if there is sdc, push the sdc into the sdc_vec
    */
   
    // perform random simulation and observe the values on fanin nodes y0, y1 of n
    /*TODO*/
    // Create two cones of the two transistive fanins and push them into a vector
    Vec_Ptr_t *Fanin_ptr_vec = Vec_PtrAlloc(2);
    Abc_Obj_t *curr_node_obj = Abc_NtkObj(pNtk, node_id);
    Abc_Obj_t *fanin_node_obj0 = Abc_ObjFanin0(curr_node_obj);
    Abc_Obj_t *fanin_node_obj1 = Abc_ObjFanin1(curr_node_obj);
    Vec_PtrPush(Fanin_ptr_vec, fanin_node_obj0);
    Vec_PtrPush(Fanin_ptr_vec, fanin_node_obj1);
    pNtk = Abc_NtkCreateConeArray(pNtk, Fanin_ptr_vec, 1);

    // derive the circuit into cnf form
    Aig_Man_t *pMan = Abc_NtkToDar(pNtk, 0, 0);
    Cnf_Dat_t *pCnf = Cnf_Derive(pMan, 2);

    // Aig_ManShow(pMan, 0, NULL);
    
    // use sat solver to solve the cnf with given four assumptions
    // 1. y0 = 0, y1 = 0
    // 2. y0 = 0, y1 = 1
    // 3. y0 = 1, y1 = 0
    // 4. y0 = 1, y1 = 1
    bool is_sdc = false;
    sat_solver *pSat = sat_solver_new();
    // int fanin_node_cnf_idx0 = pCnf->pVarNums[fanin_node_obj0->Id];
    // int fanin_node_cnf_idx1 = pCnf->pVarNums[fanin_node_obj1->Id];
    int fanin_node_cnf_idx0 = pCnf->pVarNums[Aig_ObjId(Aig_ManCo(pMan, 0))];
    int fanin_node_cnf_idx1 = pCnf->pVarNums[Aig_ObjId(Aig_ManCo(pMan, 1))];

    int Lits[2];
    int status;
    int y0, y1;

    for (int i=0; i<4; i++){
        y0 = i / 2;
        y1 = i % 2;

        Lits[0] = Abc_Var2Lit(fanin_node_cnf_idx0, y0);
        Lits[1] = Abc_Var2Lit(fanin_node_cnf_idx1, y1);

        pSat = (sat_solver*)Cnf_DataWriteIntoSolverInt(pSat, pCnf, 1, 0);
        sat_solver_addclause(pSat, Lits, Lits + 1);
        sat_solver_addclause(pSat, Lits + 1, Lits + 2);

        status = sat_solver_solve(pSat, NULL, NULL, 0, 0, 0, 0);  

        // // debug
        // printf("status: %d\n", status);
        // printf("y0: %d\n", y0);
        // printf("y1: %d\n", y1);
        // // end debug
        
        if (status == l_False){
            // unsat ==> sdc
            is_sdc = true;
            if (Abc_ObjFaninC0(curr_node_obj))  
                y0 = !y0;
            if (Abc_ObjFaninC1(curr_node_obj))
                y1 = !y1;
            sdc_vec.push_back(std::make_pair(!y0, !y1));
        }
        
        sat_solver_restart(pSat);
    }
    
    return is_sdc;
    
}


void Lsv_PrintSdc(Abc_Ntk_t* pNtk, int node_id){
    // get a vector of sdc
    bool is_sdc;
    std::vector<std::pair<int, int> > sdc_vec;
    is_sdc = Lsv_CalSdc(pNtk, node_id, sdc_vec);
    
    // print the nodes
    if (is_sdc){
        SortBoolVector(sdc_vec);
        for (auto sdc : sdc_vec){
            printf("%d%d ", sdc.first, sdc.second);
        }
        printf("\n");
    }
    else{
        printf("no sdc\n");
    }

}

int * Lsv_NtkVerifySimulatePattern( Abc_Ntk_t * pNtk, int * pModel )
{
    Abc_Obj_t * pNode;
    int * pValues, Value0, Value1, i;
    int fStrashed = 0;
    if ( !Abc_NtkIsStrash(pNtk) )
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
    Abc_NtkIncrementTravId( pNtk );
    // set the CI values
    Abc_AigConst1(pNtk)->pCopy = (Abc_Obj_t *)1;
    Abc_NtkForEachCi( pNtk, pNode, i )
        pNode->pCopy = (Abc_Obj_t *)(ABC_PTRINT_T)pModel[i];
    // simulate in the topological order
    Abc_NtkForEachNode( pNtk, pNode, i )
    {
        // to modify
        Value0 = ((int)(ABC_PTRINT_T)Abc_ObjFanin0(pNode)->pCopy) ^ (int)Abc_ObjFaninC0(pNode);
        Value1 = ((int)(ABC_PTRINT_T)Abc_ObjFanin1(pNode)->pCopy) ^ (int)Abc_ObjFaninC1(pNode);
        // end to modify
        pNode->pCopy = (Abc_Obj_t *)(ABC_PTRINT_T)(Value0 & Value1);
    }
    // fill the output values
    pValues = ABC_ALLOC( int, Abc_NtkCoNum(pNtk) );
    Abc_NtkForEachCo( pNtk, pNode, i ){
        // to modify
        pValues[i] = ((int)(ABC_PTRINT_T)Abc_ObjFanin0(pNode)->pCopy) ^ (int)Abc_ObjFaninC0(pNode);
    }
    if ( fStrashed )
        Abc_NtkDelete( pNtk );
    return pValues;
}


// =========================================
//           LSV PA2 Part2
//          Lsv_CommandOdc
// =========================================

bool Lsv_CalOdc(Abc_Ntk_t* pNtk, int node_id, std::vector<std::pair<int, int> >& odc_vec);
void Lsv_PrintOdc(Abc_Ntk_t* pNtk, int node_id);

int Lsv_CommandOdc(Abc_Frame_t* pAbc, int argc, char** argv) {
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    int c;
    int node_id;
  
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
    if (argc != 2){
        Abc_Print(-1, "Please input the node id.\n");
        return 1;
    }
    node_id = atoi(argv[1]);
    Lsv_PrintOdc(pNtk, node_id);
    return 0;

usage:
    Abc_Print(-2, "usage: lsv_odc [-h]\n");
    Abc_Print(-2, "\t        prints the Obervability don't cares (odc) of the given node\n");
    Abc_Print(-2, "\t-h    : print the command usage\n");
    return 1;
}

bool Lsv_CalOdc(Abc_Ntk_t* pNtk, int node_id, std::vector<std::pair<int, int> >& odc_vec){
    /*
        Calculate the odc of the node_id in the pNtk
        return true if there is odc
        if there is odc, push the odc into the odc_vec
        note that odc should not contain the sdc
    */
    
    bool is_odc = false;
    bool fanin0_inv = Abc_ObjFaninC0(Abc_NtkObj(pNtk, node_id));
    bool fanin1_inv = Abc_ObjFaninC1(Abc_NtkObj(pNtk, node_id));
    // perform sdc first
    std::vector<std::pair<int, int> > sdc_vec;
    bool has_sdc = Lsv_CalSdc(pNtk, node_id, sdc_vec);
    
    // duplicate the network
    Abc_Ntk_t *pNtk_dup = Abc_NtkDup(pNtk);
    Abc_Ntk_t *pNtk_node_inv = Abc_NtkDup(pNtk);

    Abc_Obj_t *node_to_be_inv_obj = Abc_NtkObj(pNtk_node_inv, node_id);
    
    // negate the node of the node_id
    int ii;
    Abc_Obj_t *fanout_node_obj;
    Abc_ObjForEachFanout(node_to_be_inv_obj, fanout_node_obj, ii){
        if (Abc_ObjFanin0(fanout_node_obj) == node_to_be_inv_obj){
            Abc_ObjXorFaninC(fanout_node_obj, 0);
        }
        else if (Abc_ObjFanin1(fanout_node_obj) == node_to_be_inv_obj){
            Abc_ObjXorFaninC(fanout_node_obj, 1);
        }
    }
    
    // create another Ntk and create cone array for the node fanin
    Abc_Ntk_t *pNtk_node_cone = Abc_NtkDup(pNtk);
    Abc_Obj_t *node_obj = Abc_NtkObj(pNtk_node_cone, node_id);
    Vec_Ptr_t *Fanin_ptr_vec0 = Vec_PtrAlloc(2);
    Abc_Obj_t *fanin_node0_obj0 = Abc_ObjFanin0(node_obj);
    Abc_Obj_t *fanin_node0_obj1 = Abc_ObjFanin1(node_obj);
    Vec_PtrPush(Fanin_ptr_vec0, fanin_node0_obj0);
    Vec_PtrPush(Fanin_ptr_vec0, fanin_node0_obj1);
    pNtk_node_cone = Abc_NtkCreateConeArray(pNtk_node_cone, Fanin_ptr_vec0, 1);

    // debug
    // Aig_Man_t *tmp1 = Abc_NtkToDar(pNtk_dup, 0, 0);
    // Aig_ManShow(tmp1, 0, NULL);
    // Aig_Man_t *tmp2 = Abc_NtkToDar(pNtk_node_inv, 0, 0);
    // Aig_ManShow(tmp2, 0, NULL);
    // end debug

    // debug
    // int di;
    // Abc_NtkForEachPi(pNtk_dup, node_obj, di){
    //     printf("PI: %d\n", Abc_ObjId(node_obj));
    // }
    // Abc_NtkForEachNode( pNtk, node_obj, i )
    // end debug


    // create a miter of the two networks
    Abc_Ntk_t *miter = Abc_NtkMiter(pNtk_dup, pNtk_node_inv, 1, 0, 0, 0);
    
    
    int a = Abc_NtkAppend(miter, pNtk_node_cone, 1);
    // derive the miter into cnf form
    Aig_Man_t *pMan = Abc_NtkToDar(miter, 0, 0);
    Cnf_Dat_t *pCnf = Cnf_Derive(pMan, 3);
    
    // use sat solver to solve the cnf with given four assumptions
    // 1. y0 = 0, y1 = 0
    // 2. y0 = 0, y1 = 1
    // 3. y0 = 1, y1 = 0
    // 4. y0 = 1, y1 = 1
    sat_solver *pSat = sat_solver_new();
    // int fanin_node_cnf_idx0 = pCnf->pVarNums[fanin_node0_obj0->Id];
    // int fanin_node_cnf_idx1 = pCnf->pVarNums[fanin_node0_obj1->Id];
    int fanin_node_cnf_idx0 = pCnf->pVarNums[Aig_ObjId(Aig_ObjFanin0(Aig_ManCo(pMan, 1)))];
    int fanin_node_cnf_idx1 = pCnf->pVarNums[Aig_ObjId(Aig_ObjFanin0(Aig_ManCo(pMan, 2)))];
    int miter_out_idx = pCnf->pVarNums[Aig_ObjId(Aig_ObjFanin0(Aig_ManCo(pMan, 0)))];
    
    // debug
    // std::cerr << "fanin_node0_obj0->Id: " << fanin_node0_obj0->Id << "\n";
    // std::cerr << "fanin_node0_obj1->Id: " << fanin_node0_obj1->Id << "\n";
    // std::cerr << "Aig_ObjId(Aig_ManCo(pMan, 0)): " << Aig_ObjId(Aig_ManCo(pMan, 0)) << "\n";
    // std::cerr << "Aig_ObjId(Aig_ManCo(pMan, 1)): " << Aig_ObjId(Aig_ManCo(pMan, 1)) << "\n";
    // std::cerr << "Aig_ObjId(Aig_ManCo(pMan, 2)): " << Aig_ObjId(Aig_ManCo(pMan, 2)) << "\n";
    // std::cerr << "Aig_ObjId(Aig_ObjFanin0(Aig_ManCo(pMan, 1))): " << Aig_ObjId(Aig_ObjFanin0(Aig_ManCo(pMan, 1))) << "\n";
    // end debug
    
    int Lits[3];
    int status;
    int y0, y1;
    for (int i=0; i<4; i++){
        sat_solver_restart(pSat);
        y0 = i / 2;
        y1 = i % 2;
        int autual_y0 = fanin0_inv ? !y0 : y0;
        int autual_y1 = fanin1_inv ? !y1 : y1;
        
        // avoid sdc, if (y0, y1) is sdc, skip
        int is_sdc = false;
        if (has_sdc){
            for (auto sdc : sdc_vec){
                if (autual_y0 == sdc.first && autual_y1 == sdc.second){
                    is_sdc = true;
                    break;
                }
            }
        }
        if (is_sdc){
            continue;
        }
        
        // debug
        // std::cerr << "y0: " << y0 << " y1: " << y1 << std::endl;
        // std::cerr << "fanin_node_cnf_idx0: " << fanin_node_cnf_idx0 << " fanin_node_cnf_idx1: " << fanin_node_cnf_idx1 << std::endl;
        // std::cerr << "miter_out_idx: " << miter_out_idx << std::endl;
        // end debug

        // use sat solver to solve the cnf
        Lits[0] = Abc_Var2Lit(fanin_node_cnf_idx0, !y0);
        Lits[1] = Abc_Var2Lit(fanin_node_cnf_idx1, !y1);
        Lits[2] = Abc_Var2Lit(miter_out_idx, 1); // set 0
        pSat = (sat_solver*)Cnf_DataWriteIntoSolverInt(pSat, pCnf, 1, 0);
        sat_solver_addclause(pSat, Lits, Lits + 1);
        sat_solver_addclause(pSat, Lits + 1, Lits + 2);
        sat_solver_addclause(pSat, Lits + 2, Lits + 3);
        status = sat_solver_solve(pSat, NULL, NULL, 0, 0, 0, 0);
        
        // if (status == l_False){
        if (status != l_True){
            // unsat ==> odc
            is_odc = true;
            odc_vec.push_back(std::make_pair(autual_y0, autual_y1));
        }
        
    }
    
    return is_odc;
}

void Lsv_PrintOdc(Abc_Ntk_t* pNtk, int node_id){
    // get a vector of odc
    bool is_odc;
    std::vector<std::pair<int, int> > odc_vec;
    is_odc = Lsv_CalOdc(pNtk, node_id, odc_vec);

    // print the nodes
    if (is_odc){
        SortBoolVector(odc_vec);
        for (auto odc : odc_vec){
            printf("%d%d ", odc.first, odc.second);
        }
        printf("\n");
    }
    else{
        printf("no odc\n");
    }
}


// =========================================
//          Lsv_NtkPrintNodes
// =========================================


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


// =========================================
//          Lsv_CommandKFeasibleCut
// =========================================

void Lsv_PrintKFeasibleCut(Abc_Ntk_t* pNtk, int K_feasible_num);

// void TopSort(Abc_Ntk_t* pNtk, vector<int> &top_order);

// void DFS(Abc_Ntk_t* pNtk, Abc_Obj_t *curr_node_obj, vector<int> &top_order, vector<bool> &node_is_found);

int Lsv_CommandKFeasibleCut(Abc_Frame_t* pAbc, int argc, char** argv) {
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    int c;
    int K_feasible_num = 3;
  
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
    if (argc > 1)
        K_feasible_num = atoi(argv[1]);
    Lsv_PrintKFeasibleCut(pNtk, K_feasible_num);
    return 0;

usage:
    Abc_Print(-2, "usage: lsv_print_cut [-h]\n");
    Abc_Print(-2, "\t        prints all K feasible cut of all the nodes in the network\n");
    Abc_Print(-2, "\t-h    : print the command usage\n");
    return 1;
}

void Lsv_PrintKFeasibleCut(Abc_Ntk_t* pNtk, int K_feasible_num){

    std::unordered_map<int, std::set<std::set<int> > > nodes_cut_map;
    // std::map<int, std::set<std::set<int> > > nodes_cut_map;
    int i;

    // PI
    Abc_Obj_t *pi_obj;

    Abc_NtkForEachPi( pNtk, pi_obj, i ){
        std::set<int> single_cut_set;
        single_cut_set.insert(Abc_ObjId(pi_obj));
        nodes_cut_map[Abc_ObjId(pi_obj)].insert(single_cut_set);
    }

    // inside nodes
    Abc_Obj_t *node_obj;
    // already be sorted as a topological order
    Abc_NtkForEachNode( pNtk, node_obj, i ){
        std::set<int> single_cut_set;
        
        // add itself first
        single_cut_set.insert(Abc_ObjId(node_obj));
        nodes_cut_map[Abc_ObjId(node_obj)].insert(single_cut_set);
        
        // calculate cross product of each children set
        Abc_Obj_t *fanin_obj1 = Abc_ObjFanin0(node_obj);
        Abc_Obj_t *fanin_obj2 = Abc_ObjFanin1(node_obj);
        for (auto &set1 : nodes_cut_map[Abc_ObjId(fanin_obj1)]){
            for (auto &set2 : nodes_cut_map[Abc_ObjId(fanin_obj2)]){
                single_cut_set = set1;
                for (auto &set_element : set2) single_cut_set.insert(set_element);
                // monitor the size of the cut
                if (single_cut_set.size() <= K_feasible_num)
                    nodes_cut_map[Abc_ObjId(node_obj)].insert(single_cut_set);
            }
        }

        // delete redundant cuts
        std::set<std::set<int>>::iterator cut_it;
        std::set<std::set<int>>::iterator cut_it2;
        // std::set<int>::iterator tmp_it;
        cut_it = nodes_cut_map[Abc_ObjId(node_obj)].begin();
        while ( cut_it != nodes_cut_map[Abc_ObjId(node_obj)].end()){
            bool cut_it_is_delete = false;
            cut_it2 = cut_it;
            cut_it2++; // next cut of the cut_it
            while (cut_it2 != nodes_cut_map[Abc_ObjId(node_obj)].end()){
                int size1 = cut_it->size();
                int size2 = cut_it2->size();
                // union of two cut
                // if the union of two cut is the same of any cut,
                // then delete the big size one
                std::set<int> union_cut;
                // tmp_it = std::set_union(cut_it->begin(), cut_it->end(), cut_it2->begin(), cut_it2->end(), union_cut.begin());
                std::set_union(cut_it->begin(), cut_it->end(), cut_it2->begin(), cut_it2->end(), std::inserter(union_cut, union_cut.begin()));
                
                // // debug
                // cerr << "set_union test\n";
                // cerr << "node " << Abc_ObjId(node_obj) << "\n";
                // cerr << "union: \n";
                // for (auto &dit : union_cut){
                //     cerr << dit << " ";
                // }
                // cerr << "\n";
                // cerr << "cut 1: \n";
                // for (auto &dit : *cut_it){
                //     cerr << dit << " ";
                // }
                // cerr << "\n";
                // cerr << "cut 2: \n";
                // for (auto &dit : *cut_it2){
                //     cerr << dit << " ";
                // }
                // cerr << "\n";
                // cerr << "\n";
                // cerr << "\n";
                // // end debug
                
                if (size1 <= size2 && union_cut == *cut_it2){
                    // // debug
                    // cerr << "delete1: node " << Abc_ObjId(node_obj) << "\n";
                    // for (auto &dit : union_cut){
                    //     cerr << dit << " ";
                    // }
                    // cerr << "\n";
                    // // end debug
                    cut_it2 = nodes_cut_map[Abc_ObjId(node_obj)].erase(cut_it2);
                }
                else if (union_cut == *cut_it) {
                    // // debug
                    // cerr << "in delete cut 1\n";
                    // // end debug
                    cut_it = nodes_cut_map[Abc_ObjId(node_obj)].erase(cut_it);
                    cut_it_is_delete = true;
                    break;
                }
                else{
                    cut_it2++;
                }
            }
            if (!cut_it_is_delete)
                cut_it++;
        }
    } // end Abc_NtkForEachNode

    // PO
    Abc_Obj_t *po_obj;
    Abc_NtkForEachPo( pNtk, po_obj, i ){
        bool two_fanin = false;
        std::set<int> single_cut_set;
        
        // calculate cross product of each children set
        Abc_Obj_t *fanin_obj1 = Abc_ObjFanin0(po_obj);
        Abc_Obj_t *fanin_obj2 = Abc_ObjFanin1(po_obj);
        // debug
        // cerr << "node " << Abc_ObjId(po_obj) << " fanin number:  " <<  Abc_ObjFaninNum(po_obj) << "\n";

        // wrong
        // if (!fanin_obj2){
        //     cerr << "In !fanin_obj2 \n";
        //     nodes_cut_map[Abc_ObjId(po_obj)] = nodes_cut_map[Abc_ObjId(fanin_obj1)];
        // }
        // else if (!fanin_obj1){
        //     cerr << "In !fanin_obj1 \n";
        //     nodes_cut_map[Abc_ObjId(po_obj)] = nodes_cut_map[Abc_ObjId(fanin_obj2)];
        // }
        if (Abc_ObjFaninNum(po_obj) == 1){
            // cerr << "In Abc_ObjFaninNum(po_obj) \n";
            nodes_cut_map[Abc_ObjId(po_obj)] = nodes_cut_map[Abc_ObjId(fanin_obj1)];
        }
        else{
            two_fanin = true;
            for (auto &set1 : nodes_cut_map[Abc_ObjId(fanin_obj1)]){
                for (auto &set2 : nodes_cut_map[Abc_ObjId(fanin_obj2)]){
                    single_cut_set = set1;
                    for (auto &set_element : set2) single_cut_set.insert(set_element);
                    // monitor the size of the cut
                    if (single_cut_set.size() <= K_feasible_num)
                        nodes_cut_map[Abc_ObjId(po_obj)].insert(single_cut_set);
                }
            }
        }

        // add itself
        single_cut_set.clear();
        single_cut_set.insert(Abc_ObjId(po_obj));
        nodes_cut_map[Abc_ObjId(po_obj)].insert(single_cut_set);
        
        // one fanin nodes don't have redundant cuts
        if (!two_fanin){
            continue;
        }
        
        // delete redundant cuts
        std::set<std::set<int>>::iterator cut_it;
        std::set<std::set<int>>::iterator cut_it2;
        cut_it = nodes_cut_map[Abc_ObjId(po_obj)].begin();
        while ( cut_it != nodes_cut_map[Abc_ObjId(po_obj)].end()){
            bool cut_it_is_delete = false;
            cut_it2 = cut_it;
            cut_it2++; // next cut of the cut_it
            while (cut_it2 != nodes_cut_map[Abc_ObjId(po_obj)].end()){
                int size1 = cut_it->size();
                int size2 = cut_it2->size();
                // union of two cut
                // if the union of two cut is the same of any cut,
                // then delete the big size one
                std::set<int> union_cut;
                std::set_union(cut_it->begin(), cut_it->end(), cut_it2->begin(), cut_it2->end(), std::inserter(union_cut, union_cut.begin()));
                
                if (size1 <= size2 && union_cut == *cut_it2){
                    cut_it2 = nodes_cut_map[Abc_ObjId(po_obj)].erase(cut_it2);
                }
                else if (union_cut == *cut_it) {
                    cut_it = nodes_cut_map[Abc_ObjId(po_obj)].erase(cut_it);
                    cut_it_is_delete = true;
                    break;
                }
                else{
                    cut_it2++;
                }
            }
            if (!cut_it_is_delete)
                cut_it++;
        }
    }
    
    // print all node cut
    for (auto &cuts : nodes_cut_map){
        for (auto &cut : cuts.second){
            std::cout << cuts.first << ": ";
            for (auto &cut_element : cut){
                std::cout << cut_element << " ";
            }
            std:: cout << std::endl;
        }
    }
}

