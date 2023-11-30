#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <iostream>
#include <vector>
#include <string>
#include <fstream> 
#include "sat/cnf/cnf.h"
extern "C"{
Aig_Man_t* Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
}
using namespace std;

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_simulation_bdd(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_simulation_aig(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_symmetric_bdd(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_symmetric_sat(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_symmetric_all(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd"    , Lsv_simulation_bdd, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig"    , Lsv_simulation_aig, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_bdd"    , Lsv_symmetric_bdd, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_sat"    , Lsv_symmetric_sat, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_all"    , Lsv_symmetric_all, 0);
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

int Lsv_simulation_bdd(Abc_Frame_t* pAbc, int argc, char** argv) {
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    int i, j, output;
    Abc_Obj_t * pPo, * pPi;

    assert(strlen(argv[1]) == Abc_NtkPiNum(pNtk));
    int cube[strlen(argv[1])], input[strlen(argv[1])];
    for(int i = 0; i < strlen(argv[1]); i++){
        input[i] = argv[1][i] - '0';
        cube[i] = -1;
    }

    Abc_NtkForEachPo(pNtk, pPo, i){
        Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo); 
        assert( Abc_NtkIsBddLogic(pRoot->pNtk) );
        DdManager * dd = (DdManager *)pRoot->pNtk->pManFunc;  
        DdNode* f = (DdNode *)pRoot->pData;
        Abc_ObjForEachFanin( pRoot, pPi, j ){
            cube[j] = input[pPi->Id - 1];
        }
        
        DdNode* g = Cudd_CubeArrayToBdd(dd, cube);
        f = Cudd_Cofactor(dd, f, g);
        if(f == dd->one){
            output = 1;
        }
        else{
            output = 0;
        }

        cout<<Abc_ObjName(pPo)<<": "<<output<<endl;
    }
    return 0;
}

int Lsv_simulation_aig(Abc_Frame_t* pAbc, int argc, char** argv) {
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    ifstream in(argv[1]);
    int j, k;
    Abc_Obj_t * pObj, * pPi, * pPo;
    string bit_pattern, output[Abc_NtkPoNum(pNtk)];
    vector<string> bit_patterns;
    vector<unsigned> patterns;
    unsigned fanin0, fanin1;

    while (getline(in, bit_pattern)){
        bit_patterns.push_back(bit_pattern);
    }
    int remainder = 32;
    for(int i = 0; i < (bit_patterns.size() - 1) / 32 + 1; i++){
        if(i == (bit_patterns.size() - 1) / 32){
            remainder = (bit_patterns.size() - 1) % 32 + 1;
        }
        Abc_NtkForEachPi(pNtk, pPi, k){
            unsigned pattern = 0;
            for(int j = 0; j < remainder; j++){
                pattern = pattern | (unsigned((bit_patterns[i*32+j][k] - '0')) << j);
            }
            pPi->iTemp = pattern; 
        }

        Abc_NtkForEachObj( pNtk, pObj, j ){
            if(pObj->Type != ABC_OBJ_CONST1 && pObj->Type != ABC_OBJ_PI && pObj->Type != ABC_OBJ_PO){
                fanin0 = pObj->fCompl0? ~Abc_ObjFanin0(pObj)->iTemp : Abc_ObjFanin0(pObj)->iTemp;
                fanin1 = pObj->fCompl1? ~Abc_ObjFanin1(pObj)->iTemp : Abc_ObjFanin1(pObj)->iTemp;
                pObj->iTemp = fanin0 & fanin1;
            }
        }

        Abc_NtkForEachPo(pNtk, pPo, k){
            fanin0 = pPo->fCompl0? ~Abc_ObjFanin0(pPo)->iTemp : Abc_ObjFanin0(pPo)->iTemp;
            for(int j = 0; j < remainder; j++){
                output[k] += to_string((fanin0>>j) & 1);
            }
        }
    }
    
    Abc_NtkForEachPo(pNtk, pPo, k){
        cout<<Abc_ObjName(pPo)<<": "<<output[k]<<endl;
    }

    return 0;
}

int Lsv_symmetric_bdd(Abc_Frame_t* pAbc, int argc, char** argv) {
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    Abc_Obj_t * pPo, * pPi;

    assert(argc == 4);
    int x, k = atoi(argv[1]), i = atoi(argv[2]), j = atoi(argv[3]), trans_i = -1, trans_j = -1;

    pPo = Abc_NtkPo(pNtk, k);
    Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo); 
    assert( Abc_NtkIsBddLogic(pRoot->pNtk) );
    DdManager * dd = (DdManager *)pRoot->pNtk->pManFunc;  
    DdNode *cube_i, *cube_j, *f = (DdNode *)pRoot->pData;
    
    Abc_ObjForEachFanin( pRoot, pPi, x){
      //cout<<"test "<<pPi->Id<<" "<<x<<endl;
      if((pPi->Id - 1) == i){
        trans_i = x;
      }
      if((pPi->Id - 1) == j){
        trans_j = x;
      }
    }
    if(trans_i == -1 && trans_j == -1){
        cerr<<"symmetric"<<endl;
        return 0;
    }
    else if(trans_i == -1){
      cerr<<"asymmetric"<<endl;
      DdNode* bdd_j = Cudd_Cofactor(dd, f, dd->vars[trans_j]);
      DdNode* bdd_not_j = Cudd_Cofactor(dd, f, Cudd_Not( dd->vars[trans_j]));
      DdNode* diff = Cudd_bddXor(dd, bdd_j, bdd_not_j);

      int cex[Abc_NtkPiNum(pNtk)];
      int** cube_cex = new(int*);
      CUDD_VALUE_TYPE* type = new(CUDD_VALUE_TYPE);
      for(x = 0; x < Abc_NtkPiNum(pNtk); x++)
        cex[x] = 0;

      Cudd_FirstCube(dd, diff, cube_cex, type);
      Abc_ObjForEachFanin( pRoot, pPi, x){
        //cout<<"id : "<<pPi->Id<<" "<<Abc_ObjName(pPi)<<" x : "<<x<<" cube "<<cube_cex[0][x]<<endl;
        cex[pPi->Id - 1] = cube_cex[0][x];
        if(cube_cex[0][x] == 2){
          cex[pPi->Id - 1] = 1;
        }
      }
      delete(cube_cex);
      delete(type);
      cex[i] = 1;
      cex[j] = 0;
      for(x = 0; x < Abc_NtkPiNum(pNtk); x++){
        cout<<cex[x];
      }
      cout<<endl;

      cex[i] = 0;
      cex[j] = 1;
      for(x = 0; x < Abc_NtkPiNum(pNtk); x++){
        cout<<cex[x];
      }
      cout<<endl;
      return 0;
    }
    else if(trans_j == -1){
      cout<<"asymmetric"<<endl;
      DdNode* bdd_i = Cudd_Cofactor(dd, f, dd->vars[trans_i]);
      DdNode* bdd_not_i = Cudd_Cofactor(dd, f, Cudd_Not( dd->vars[trans_i]));
      DdNode* diff = Cudd_bddXor(dd, bdd_i, bdd_not_i);

      int cex[Abc_NtkPiNum(pNtk)];
      int** cube_cex = new(int*);
      CUDD_VALUE_TYPE* type = new(CUDD_VALUE_TYPE);
      for(x = 0; x < Abc_NtkPiNum(pNtk); x++)
        cex[x] = 0;

      Cudd_FirstCube(dd, diff, cube_cex, type);
      Abc_ObjForEachFanin( pRoot, pPi, x){
        //cout<<"id : "<<pPi->Id<<" "<<Abc_ObjName(pPi)<<" x : "<<x<<" cube "<<cube_cex[0][x]<<endl;
        cex[pPi->Id - 1] = cube_cex[0][x];
        if(cube_cex[0][x] == 2){
          cex[pPi->Id - 1] = 1;
        }
      }
      delete(cube_cex);
      delete(type);
      cex[i] = 1;
      cex[j] = 0;
      for(x = 0; x < Abc_NtkPiNum(pNtk); x++){
        cout<<cex[x];
      }
      cout<<endl;

      cex[i] = 0;
      cex[j] = 1;
      for(x = 0; x < Abc_NtkPiNum(pNtk); x++){
        cout<<cex[x];
      }
      cout<<endl;
      return 0;
    }

    cube_i = Cudd_bddAnd( dd, Cudd_Not( dd->vars[trans_j] ), dd->vars[trans_i] );
    cube_j = Cudd_bddAnd( dd, Cudd_Not( dd->vars[trans_i] ), dd->vars[trans_j] );

    DdNode* bdd_i = Cudd_Cofactor(dd, f, cube_i);
    DdNode* bdd_j = Cudd_Cofactor(dd, f, cube_j);

    if(bdd_i == bdd_j){
      cout<<"symmetric"<<endl;
    }
    else{
      DdNode* diff = Cudd_bddXor(dd, bdd_i, bdd_j);
      int cex[Abc_NtkPiNum(pNtk)];
      int** cube_cex = new(int*);
      CUDD_VALUE_TYPE* type = new(CUDD_VALUE_TYPE);
      for(x = 0; x < Abc_NtkPiNum(pNtk); x++)
        cex[x] = 0;

      Cudd_FirstCube(dd, diff, cube_cex, type);
      Abc_ObjForEachFanin( pRoot, pPi, x){
        //cout<<"id : "<<pPi->Id<<" "<<Abc_ObjName(pPi)<<" x : "<<x<<" cube "<<cube_cex[0][x]<<endl;
        cex[pPi->Id - 1] = cube_cex[0][x];
        if(cube_cex[0][x] == 2){
          cex[pPi->Id - 1] = 1;
        }
      }
      delete(cube_cex);
      delete(type);

      cout<<"asymmetric"<<endl;

      cex[i] = 1;
      cex[j] = 0;
      for(x = 0; x < Abc_NtkPiNum(pNtk); x++){
        cout<<cex[x];
      }
      cout<<endl;

      cex[i] = 0;
      cex[j] = 1;
      for(x = 0; x < Abc_NtkPiNum(pNtk); x++){
        cout<<cex[x];
      }
      cout<<endl;
    }

    return 0;
}

int Lsv_symmetric_sat(Abc_Frame_t* pAbc, int argc, char** argv) {
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    Abc_Obj_t * pPo, * pPi;
    assert(argc == 4);
    int x, k = atoi(argv[1]), i = atoi(argv[2]), j = atoi(argv[3]);

    //find output
    pPo = Abc_NtkPo(pNtk, k);
    assert( Abc_NtkIsStrash(Abc_ObjFanin0(pPo)->pNtk) );

    //write circuit cnf to sat solver
    Abc_Ntk_t* Ntk_k = Abc_NtkCreateCone(pNtk, Abc_ObjFanin0(pPo), Abc_ObjName(pPo), 1);
    Aig_Man_t* aig_k = Abc_NtkToDar(Ntk_k, 0, 0);
    sat_solver * pSat = sat_solver_new();
    Cnf_Dat_t * pCnf = Cnf_Derive(aig_k, Aig_ManCoNum(aig_k));
    Cnf_DataWriteIntoSolverInt( pSat, pCnf, 1, 0);
    Cnf_DataLift( pCnf, pCnf->nVars );
    Cnf_DataWriteIntoSolverInt( pSat, pCnf, 1, 0);
    Cnf_DataLift( pCnf, -pCnf->nVars );

    int Lit[5], Var_i[2] = {-1, -1}, Var_j[2] = {-1, -1};
    Abc_NtkForEachPi(Ntk_k, pPi, x){
        if(strcmp(Abc_ObjName(Abc_NtkPi(pNtk, i)), Abc_ObjName(pPi)) == 0){//record i variable
            Var_i[0] = pCnf->pVarNums[pPi->Id];
            Cnf_DataLift( pCnf, pCnf->nVars );
            Var_i[1] = pCnf->pVarNums[pPi->Id];
            Cnf_DataLift( pCnf, -pCnf->nVars );
        }
        else if(strcmp(Abc_ObjName(Abc_NtkPi(pNtk, j)), Abc_ObjName(pPi)) == 0){//record j variable
            Var_j[0] = pCnf->pVarNums[pPi->Id];
            Cnf_DataLift( pCnf, pCnf->nVars );
            Var_j[1] = pCnf->pVarNums[pPi->Id];
            Cnf_DataLift( pCnf, -pCnf->nVars );
        }
        else{//assume equal when x != i and j
            Lit[0] = Abc_Var2Lit(pCnf->pVarNums[pPi->Id], 0);
            Cnf_DataLift( pCnf, pCnf->nVars );
            Lit[1] = Abc_Var2Lit(pCnf->pVarNums[pPi->Id], 1);
            Cnf_DataLift( pCnf, -pCnf->nVars );
            sat_solver_addclause( pSat, Lit, Lit+2);

            Lit[0] = Abc_Var2Lit(pCnf->pVarNums[pPi->Id], 1);
            Cnf_DataLift( pCnf, pCnf->nVars );
            Lit[1] = Abc_Var2Lit(pCnf->pVarNums[pPi->Id], 0);
            Cnf_DataLift( pCnf, -pCnf->nVars );
            sat_solver_addclause( pSat, Lit, Lit+2);
        }
    }

    //check whether assigned input is support of assigned output

    //assume output xor
    Lit[0] = Abc_Var2Lit(pCnf->pVarNums[Abc_ObjFanin0(Abc_NtkPo(Ntk_k, 0))->Id], 0);
    Cnf_DataLift( pCnf, pCnf->nVars );
    Lit[1] = Abc_Var2Lit(pCnf->pVarNums[Abc_ObjFanin0(Abc_NtkPo(Ntk_k, 0))->Id], 0);
    Cnf_DataLift( pCnf, -pCnf->nVars );
    sat_solver_addclause( pSat, Lit, Lit+2);
    Lit[0] = Abc_Var2Lit(pCnf->pVarNums[Abc_ObjFanin0(Abc_NtkPo(Ntk_k, 0))->Id], 1);
    Cnf_DataLift( pCnf, pCnf->nVars );
    Lit[1] = Abc_Var2Lit(pCnf->pVarNums[Abc_ObjFanin0(Abc_NtkPo(Ntk_k, 0))->Id], 1);
    Cnf_DataLift( pCnf, -pCnf->nVars );
    sat_solver_addclause( pSat, Lit, Lit+2);

    //assume 10 and 01
    Lit[0] = Abc_Var2Lit(Var_i[0], 0);
    Lit[1] = Abc_Var2Lit(Var_j[0], 1);
    Lit[2] = Abc_Var2Lit(Var_i[1], 1);
    Lit[3] = Abc_Var2Lit(Var_j[1], 0);

    //solve
    int status = sat_solver_solve(pSat, Lit, Lit+4, 0, 0, 0, 0);
    if(status == l_True){
        cout<<"asymmetric"<<endl;

        //find counter example
        int cex[Abc_NtkPiNum(pNtk)];
        for(x = 0; x < Abc_NtkPiNum(pNtk); x++)
            cex[x] = 0;
        Abc_NtkForEachPi(Ntk_k, pPi, x){
            cex[pPi->Id - 1] = sat_solver_var_value(pSat, pCnf->pVarNums[pPi->Id]);
        }
        for(x = 0; x < Abc_NtkPiNum(pNtk); x++)
            cout<<cex[x];
        cout<<endl;
        Cnf_DataLift( pCnf, pCnf->nVars );
        Abc_NtkForEachPi(Ntk_k, pPi, x){
            cex[pPi->Id - 1] = sat_solver_var_value(pSat, pCnf->pVarNums[pPi->Id]);
        }
        for(x = 0; x < Abc_NtkPiNum(pNtk); x++)
            cout<<cex[x];
        cout<<endl;
    }
    else if(status == l_False){
        cout<<"symmetric"<<endl;
    }
    
    return 0;
}

int Lsv_symmetric_all(Abc_Frame_t* pAbc, int argc, char** argv) {
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    Abc_Obj_t * pPo, * pPi, * pPj;
    assert(argc == 2);
    int x, k = atoi(argv[1]), i = 0, j = 0;

    //find output
    pPo = Abc_NtkPo(pNtk, k);
    assert( Abc_NtkIsStrash(Abc_ObjFanin0(pPo)->pNtk) );

    //write circuit cnf to sat solver
    Abc_Ntk_t* Ntk_k = Abc_NtkCreateCone(pNtk, Abc_ObjFanin0(pPo), Abc_ObjName(pPo), 1);
    Aig_Man_t* aig_k = Abc_NtkToDar(Ntk_k, 0, 0);
    sat_solver * pSat = sat_solver_new();
    Cnf_Dat_t * pCnf = Cnf_Derive(aig_k, Aig_ManCoNum(aig_k));
    Cnf_DataWriteIntoSolverInt( pSat, pCnf, 1, 0);
    Cnf_DataLift( pCnf, pCnf->nVars );
    Cnf_DataWriteIntoSolverInt( pSat, pCnf, 1, 0);
    Cnf_DataLift( pCnf, -pCnf->nVars );

    //add control var
    sat_solver_setnvars(pSat, pSat->size + Abc_NtkPiNum(Ntk_k));
    int Lit[Abc_NtkPiNum(Ntk_k)], trans_id[Abc_NtkPiNum(Ntk_k)];

    //assume output xor
    Lit[0] = Abc_Var2Lit(pCnf->pVarNums[Abc_ObjFanin0(Abc_NtkPo(Ntk_k, 0))->Id], 0);
    Cnf_DataLift( pCnf, pCnf->nVars );
    Lit[1] = Abc_Var2Lit(pCnf->pVarNums[Abc_ObjFanin0(Abc_NtkPo(Ntk_k, 0))->Id], 0);
    Cnf_DataLift( pCnf, -pCnf->nVars );
    sat_solver_addclause( pSat, Lit, Lit+2);
    Lit[0] = Abc_Var2Lit(pCnf->pVarNums[Abc_ObjFanin0(Abc_NtkPo(Ntk_k, 0))->Id], 1);
    Cnf_DataLift( pCnf, pCnf->nVars );
    Lit[1] = Abc_Var2Lit(pCnf->pVarNums[Abc_ObjFanin0(Abc_NtkPo(Ntk_k, 0))->Id], 1);
    Cnf_DataLift( pCnf, -pCnf->nVars );
    sat_solver_addclause( pSat, Lit, Lit+2);

    Abc_NtkForEachPi(Ntk_k, pPi, i){
      Abc_NtkForEachPi(pNtk, pPj, j){
        if(strcmp(Abc_ObjName(pPj), Abc_ObjName(pPi)) == 0){
          trans_id[i] = j;
        }
      }
    }

    //case i == j
    Abc_NtkForEachPi(Ntk_k, pPi, i){
      Lit[0] = Abc_Var2Lit(pCnf->pVarNums[pPi->Id], 0);
      Cnf_DataLift( pCnf, pCnf->nVars );
      Lit[1] = Abc_Var2Lit(pCnf->pVarNums[pPi->Id], 1);
      Cnf_DataLift( pCnf, -pCnf->nVars );
      Lit[2] = Abc_Var2Lit(2 * pCnf->nVars + i + 1, 0);
      sat_solver_addclause( pSat, Lit, Lit+3);

      Lit[0] = Abc_Var2Lit(pCnf->pVarNums[pPi->Id], 1);
      Cnf_DataLift( pCnf, pCnf->nVars );
      Lit[1] = Abc_Var2Lit(pCnf->pVarNums[pPi->Id], 0);
      Cnf_DataLift( pCnf, -pCnf->nVars );
      Lit[2] = Abc_Var2Lit(2 * pCnf->nVars + i + 1, 0);
      sat_solver_addclause( pSat, Lit, Lit+3);
    }

    //case i != j, assume 10 and 01
    Abc_NtkForEachPi(Ntk_k, pPi, i){
      Abc_NtkForEachPi(Ntk_k, pPj, j){
        if(i < j){
          Lit[1] = Abc_Var2Lit(2 * pCnf->nVars + i + 1, 1);
          Lit[2] = Abc_Var2Lit(2 * pCnf->nVars + j + 1, 1);
          Lit[0] = Abc_Var2Lit(pCnf->pVarNums[pPi->Id], 0);
          sat_solver_addclause( pSat, Lit, Lit+3);
          Lit[0] = Abc_Var2Lit(pCnf->pVarNums[pPj->Id], 1);
          sat_solver_addclause( pSat, Lit, Lit+3);
          Cnf_DataLift( pCnf, pCnf->nVars );

          Lit[0] = Abc_Var2Lit(pCnf->pVarNums[pPi->Id], 1);
          sat_solver_addclause( pSat, Lit, Lit+3);
          Lit[0] = Abc_Var2Lit(pCnf->pVarNums[pPj->Id], 0);
          sat_solver_addclause( pSat, Lit, Lit+3);
          Cnf_DataLift( pCnf, -pCnf->nVars );
        }
      }
    }

    Abc_NtkForEachPi(Ntk_k, pPi, i){
      Abc_NtkForEachPi(Ntk_k, pPj, j){
        if(i < j){
          //assumption
          for(x = 0; x < Abc_NtkPiNum(Ntk_k); x++){
            Lit[x] = Abc_Var2Lit(2 * pCnf->nVars + x + 1, !(x == i || x == j));
          }

          //solve
          int status = sat_solver_solve(pSat, Lit, Lit+Abc_NtkPiNum(Ntk_k), 0, 0, 0, 0);
          if(status == l_False){
            cout<<trans_id[i]<<" "<<trans_id[j]<<endl;
          }
        }
      }
    }
    
    return 0;
}