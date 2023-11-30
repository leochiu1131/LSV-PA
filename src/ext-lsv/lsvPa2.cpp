#include "lsvCmd.h"

// ABC_NAMESPACE_IMPL_START helps abc to fetch namespace from all files
ABC_NAMESPACE_IMPL_START

extern "C"{
    Aig_Man_t* Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
}

DdNode* Cudd_Cofactor_with_Ref ( DdManager* dd, DdNode* bdd, int index1, int index2, bool rev) {
    DdNode * bddVar1 = Cudd_bddIthVar(dd, index1);
    DdNode * bddVar2 = Cudd_bddIthVar(dd, index2);
    Cudd_Ref(bddVar1);
    Cudd_Ref(bddVar2);

    // The first function
    DdNode* f1;
    if (rev) {
        f1 = Cudd_Cofactor( dd, bdd, bddVar1);
        f1 = Cudd_Cofactor( dd, f1, Cudd_Not(bddVar2));
    }
    else {
        f1 = Cudd_Cofactor( dd, bdd, Cudd_Not(bddVar1));
        f1 = Cudd_Cofactor( dd, f1, bddVar2);
    }
    // Cudd_PrintDebug(dd, f1, 10, 3); 

    Cudd_RecursiveDeref ( dd, bddVar1 );
    Cudd_RecursiveDeref ( dd, bddVar2 );

    return f1;
}

void Lsv_BddSymmetry( Abc_Ntk_t * pNtk , int o, int i1, int i2 ) {

    // trivial case: i1 == i2
    if (i1 == i2) {
        std::cout << "symmetric" << std::endl;
        return;
    }

  // trace every Po's bdd
    Abc_Obj_t * pPo = Abc_NtkPo( pNtk, o ); // pPo
    Abc_Obj_t * pPi1 = Abc_NtkPi( pNtk, i1 );
    Abc_Obj_t * pPi2 = Abc_NtkPi( pNtk, i2 );

    Abc_Obj_t * pObj;
    pObj = Abc_ObjChild0(pPo); // Last bdd Node
    assert( Abc_NtkIsBddLogic(pObj->pNtk) );

    DdManager * dd = (DdManager *)pObj->pNtk->pManFunc;  

    // print current Po name
    // (void) fprintf( dd->out, "%s", Abc_ObjName(pPo) );

    // Get bdd
    DdNode * bdd = ( DdNode * ) pObj->pData;
    if (bdd == NULL) {
        Abc_Print(1, ": No bdd.\n");
        (void) fflush( dd->out );
        return; // irrevelent obj
    }
    
    (void) fflush( dd->out );

    // Get Bdd index, which is different from original index
    // using the fact that they have the same Id / name.

    int bddIndex1 = -1, bddIndex2 = -1;

    Abc_Obj_t * temp;
    int i=0;
    bool ok1 = false, ok2 = false;
    Abc_NtkForEachObj( pNtk, temp, i) {
        if (ok1 && ok2) break;
        if (temp->Id == pPi1->Id) {
            bddIndex1 = i - 1;
            ok1 = true;
        }
        else if (temp->Id == pPi2->Id) {
            bddIndex2 = i - 1;
            ok2 = true;
        }
    }

    if (bddIndex1 == -1 || bddIndex2 == -1) {
        Abc_Print(1, "You've input non-exist index.\n");
        return;
    }

    // Check Permutation of bddIndex
    // std::cout << "bddIndex: "
            //   << bddIndex1 << " " << bddIndex2 << std::endl;

    // Debug Info // USEFUL
    // Cudd_PrintDebug(dd, bdd, 10, 3); 

    // The first function  : x1 x2_bar
    DdNode* f1 = Cudd_Cofactor_with_Ref(dd, bdd, bddIndex1, bddIndex2, true);

    // The second function : x1_bar x2
    DdNode* f2 = Cudd_Cofactor_with_Ref(dd, bdd, bddIndex1, bddIndex2, false);

    // XOR
    DdNode * eq = Cudd_bddXor( dd, f1, f2);
    Cudd_Ref(eq);

    // Check symmetry
    if ( Cudd_CountPathsToNonZero(eq) != 0.0 ) {
        std::cout << "asymmetric" << std::endl;
    }
    else {
        std::cout << "symmetric" << std::endl;
        Cudd_RecursiveDeref ( dd, eq );
        return;
    }

    // if asymmetric, give counter example
    int ** counterCube = new(int*);
    double * value = new(double);
    Cudd_FirstCube(dd, eq, counterCube, value); // return DdGen* is unneeded
    Cudd_RecursiveDeref ( dd, eq );

    if ( counterCube == NULL ) {
        std::cout << "Error: Empty CounterExample with asymmetric function." << std::endl;
        return;
    }
    else {
        // for (int l=0, N=Abc_NtkPiNum( pNtk ); l<N; ++l ) {
        //     std::cout << counterCube[0][l] ;
        // }
        // std::cout << std::endl;
        for (int l=0, N=Abc_NtkPiNum( pNtk ); l<N; ++l ) {
            if ( l == bddIndex1 ) std::cout << "1" ;
            else if ( l == bddIndex2 ) std::cout << "0" ;
            else if (counterCube[0][l] == 2) std::cout << "1";
            else std::cout << counterCube[0][l];
        }
        std::cout << std::endl;
        for (int l=0, N=Abc_NtkPiNum( pNtk ); l<N; ++l ) {
            if ( l == bddIndex1 ) std::cout << "0" ;
            else if ( l == bddIndex2 ) std::cout << "1" ;
            else if (counterCube[0][l] == 2) std::cout << "1" ;
            else std::cout << counterCube[0][l] ;
        }
        std::cout << std::endl;
    }


    delete [] counterCube[0];
    delete [] counterCube;
    delete [] value;
    return;
}

int Lsv_CommandSymBdd(Abc_Frame_t* pAbc, int argc, char** argv) {
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    if (!pNtk) {
        Abc_Print(-1, "Empty network.\n");
        return 1;
    }

    int c;
    Extra_UtilGetoptReset();

    std::string simS;
    int po;
    int pi1, pi2;

    while ( ( c = Extra_UtilGetopt(argc, argv, "h" ) ) != EOF ) {
        switch (c) {
            case 'h':
                goto usage;
            default:
                break;
        }
    }

    // get input string
    char ** pArgvNew;
    char * vString;
    int nArgcNew;
    pArgvNew = argv + globalUtilOptind;
    nArgcNew = argc - globalUtilOptind;
    if ( nArgcNew != 3 )
    {
        Abc_Print( -1, "Number of Input is wrong.\n" );
        return 1;
    }



    vString = pArgvNew[0];
    simS = std::string(vString);
    po = std::stoi(simS);

    vString = pArgvNew[1];
    simS = std::string(vString);
    pi1 = std::stoi(simS);

    vString = pArgvNew[2];
    simS = std::string(vString);
    pi2 = std::stoi(simS);

    // std::cout << po << " " << pi1 << " " << pi2 << " " << std::endl;

    if ( !Abc_NtkIsBddLogic(pNtk) )
    {
        Abc_Print( -1, "Simulating BDDs can only be done for logic BDD networks (run \"collapse\").\n" );
        return 1;
    }

    // Size Check
    if ( po >= Abc_NtkPoNum(pNtk) || po < 0 )
    {
        Abc_Print( -1, "pO index out of bound.\n" );
        return 1;
    }

    if ( pi1 >= Abc_NtkPiNum(pNtk) || pi1 < 0 )
    {
        Abc_Print( -1, "pI1 index out of bound.\n" );
        return 1;
    }

    if ( pi2 >= Abc_NtkPiNum(pNtk) || pi2 < 0 )
    {
        Abc_Print( -1, "pI2 index out of bound.\n" );
        return 1;
    }

    Lsv_BddSymmetry( pNtk , po, pi1, pi2 );
    
    return 0;

usage:
    Abc_Print(-2, "usage: lsv_sym_bdd <pO> <pI1> <pI2> [-h]\n");
    Abc_Print(-2, "\t        Check symmetry on 2 variables in bdd\n");
    Abc_Print(-2, "\t-h    : print the command usage\n");
    return 1;
}

/**
 * Reference: src/aig/aig/aigInter.c
 *      Aig_ManInterFast()
 * **/
void Lsv_AigSymmetry ( Abc_Ntk_t * pNtk , int o, int i1, int i2 ) {

    assert( Abc_NtkIsStrash( pNtk ));

    // trivial case: i1 = i2
    if (i1 == i2) {
        std::cout << "symmetric" << std::endl;
        return;
    }
    
    // int n = Abc_NtkCiNum( pNtk ) ;
    Abc_Obj_t * pObj;
    int Lits[5], i;

    Abc_Obj_t * pO = Abc_NtkPo( pNtk, o );
    Abc_Obj_t * pNode = Abc_ObjFanin0(pO);
    assert( pNode );
    assert( Abc_ObjIsNode(pNode) );
    char * pOName = Abc_ObjName(pO);
    // std::cout << pOName << std::endl;

    Abc_Obj_t * pi1 = Abc_NtkPi( pNtk, i1 );
    Abc_Obj_t * pi2 = Abc_NtkPi( pNtk, i2 );

    int pI1_id = pi1->Id;
    int pI2_id = pi2->Id;

    // Create Cone
    Abc_Ntk_t * Cone = Abc_NtkCreateCone( pNtk, pNode, pOName, 1);
    // pRoot is the po in Cone, which is slightly different from pNode from pNtk.
    Abc_Obj_t * pRoot = Abc_ObjFanin0( Abc_NtkPo(Cone, 0) ); 
    
    // NtkToDar
    Aig_Man_t * aigMan = Abc_NtkToDar( Cone, 0, 0 );
    // int liftNum = Aig_ManObjNum(aigMan); // The number of nodes in AIG

    // New Sat solver
    sat_solver * pSat = sat_solver_new(); // SAT Solver
    // pSat->fVerbose = 1; // verbose
    // pSat->fPrintClause = 1; // verbose

    // Add CNF into SAT solver
    Cnf_Dat_t * pCnf1 = Cnf_Derive( aigMan, Aig_ManCoNum(aigMan)); // CoNum should be 1
    pSat = (sat_solver *)Cnf_DataWriteIntoSolverInt( pSat, pCnf1, 1, 0 );

    // Create another CNF that depend on different input variable
    Cnf_Dat_t * pCnf2 = Cnf_Derive( aigMan, Aig_ManCoNum(aigMan)); // CoNum should be 1
    Cnf_DataLift( pCnf2, pCnf1->nVars + 1 );
    pSat = (sat_solver *)Cnf_DataWriteIntoSolverInt( pSat, pCnf2, 1, 0 );

    // Map input constraints
    // Aig_ManForEachCi( aigMan, pObj, i )
    Abc_NtkForEachPi( Cone, pObj, i)
    {
        if (pI1_id == pObj->Id) continue;
        if (pI2_id == pObj->Id) continue;

        Lits[0] = toLitCond( pCnf1->pVarNums[pObj->Id], 0 );
        Lits[1] = toLitCond( pCnf2->pVarNums[pObj->Id], 1 );
        if ( !sat_solver_addclause( pSat, Lits, Lits+2 ) )
            assert( 0 );
        Lits[0] = toLitCond( pCnf1->pVarNums[pObj->Id], 1 );
        Lits[1] = toLitCond( pCnf2->pVarNums[pObj->Id], 0 );
        if ( !sat_solver_addclause( pSat, Lits, Lits+2 ) )
            assert( 0 );
    }

    // Assertion
    Lits[0] = toLitCond( pCnf1->pVarNums[pI1_id], 0 );
    // Lits[1] = toLitCond( pCnf2->pVarNums[pI2_id], 1 );
    if ( !sat_solver_addclause( pSat, Lits, Lits+1 ) )
        assert( 0 );
    // Lits[0] = toLitCond( pCnf1->pVarNums[pI1_id], 1 );
    Lits[0] = toLitCond( pCnf2->pVarNums[pI2_id], 0 );
    if ( !sat_solver_addclause( pSat, Lits, Lits+1 ) )
        assert( 0 );
    // Lits[0] = toLitCond( pCnf1->pVarNums[pI2_id], 0 );
    Lits[0] = toLitCond( pCnf2->pVarNums[pI1_id], 1 );
    if ( !sat_solver_addclause( pSat, Lits, Lits+1 ) )
        assert( 0 );
    Lits[0] = toLitCond( pCnf1->pVarNums[pI2_id], 1 );
    // Lits[1] = toLitCond( pCnf2->pVarNums[pI1_id], 0 );
    if ( !sat_solver_addclause( pSat, Lits, Lits+1 ) )
        assert( 0 );

    // XOR output
    // std::cout << pCnf1->pVarNums[pRoot->Id] << std::endl;
    Lits[0] = toLitCond( pCnf1->pVarNums[pRoot->Id], 0 );
    Lits[1] = toLitCond( pCnf2->pVarNums[pRoot->Id], 0 );
    if ( !sat_solver_addclause( pSat, Lits, Lits+2 ) )
        assert( 0 );
    Lits[0] = toLitCond( pCnf1->pVarNums[pRoot->Id], 1 );
    Lits[1] = toLitCond( pCnf2->pVarNums[pRoot->Id], 1 );
    if ( !sat_solver_addclause( pSat, Lits, Lits+2 ) )
        assert( 0 );

    // Lits[0] = Abc_Var2Lit(pCnf1->pVarNums[pI1_id], 0);
    // Lits[1] = Abc_Var2Lit(pCnf1->pVarNums[pI2_id], 1);
    // Lits[2] = Abc_Var2Lit(pCnf2->pVarNums[pI1_id], 1);
    // Lits[3] = Abc_Var2Lit(pCnf2->pVarNums[pI2_id], 0);

    // Solve
    int status;
    status = sat_solver_solve( pSat, NULL, NULL, (ABC_INT64_T)1000000, (ABC_INT64_T)0, (ABC_INT64_T)0, (ABC_INT64_T)0 );
    if ( status == l_False )
    {
        printf( "symmetric\n" );
    }
    else if ( status == l_True )
    {
        printf( "asymmetric\n" );

        // derive asymmetric counterexample
        // Aig_ManForEachCi( aigMan, pObj, i )
        Abc_NtkForEachPi( pNtk, pObj, i )
        {
            std::cout << sat_solver_var_value( pSat, (pCnf1->pVarNums[pObj->Id]) );
        }
        std::cout << std::endl;
        // Aig_ManForEachCi( aigMan, pObj, i )
        Abc_NtkForEachPi( pNtk, pObj, i )
        {
            std::cout << sat_solver_var_value( pSat, pCnf2->pVarNums[pObj->Id] );
        }
        std::cout << std::endl;
    }
    else
    {
        printf( "undef\n" );
    }



    sat_solver_delete( pSat );
    Aig_ManStop( aigMan );
    Cnf_DataFree( pCnf1 );
    Cnf_DataFree( pCnf2 );
    Abc_NtkDelete( Cone );
    return;
}

int Lsv_CommandSymSat(Abc_Frame_t* pAbc, int argc, char** argv) {
 Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    if (!pNtk) {
        Abc_Print(-1, "Empty network.\n");
        return 1;
    }

    int c;
    Extra_UtilGetoptReset();

    std::string simS;
    int po;
    int pi1, pi2;

    while ( ( c = Extra_UtilGetopt(argc, argv, "h" ) ) != EOF ) {
        switch (c) {
            case 'h':
                goto usage;
            default:
                break;
        }
    }

    // get input string
    char ** pArgvNew;
    char * vString;
    int nArgcNew;
    pArgvNew = argv + globalUtilOptind;
    nArgcNew = argc - globalUtilOptind;
    if ( nArgcNew != 3 )
    {
        goto usage;
        // Abc_Print( -1, "Number of Input is wrong.\n" );
        // return 1;
    }



    vString = pArgvNew[0];
    simS = std::string(vString);
    po = std::stoi(simS);

    vString = pArgvNew[1];
    simS = std::string(vString);
    pi1 = std::stoi(simS);

    vString = pArgvNew[2];
    simS = std::string(vString);
    pi2 = std::stoi(simS);

    // std::cout << po << " " << pi1 << " " << pi2 << " " << std::endl;

    if ( !Abc_NtkIsStrash(pNtk) )
    {
        Abc_Print( -1, "Simulating SAT can only be done for logic AIG networks (run \"strash\").\n" );
        return 1;
    }

    // Size Check
    if ( po >= Abc_NtkPoNum(pNtk) || po < 0 )
    {
        Abc_Print( -1, "pO index out of bound.\n" );
        return 1;
    }

    if ( pi1 >= Abc_NtkPiNum(pNtk) || pi1 < 0 )
    {
        Abc_Print( -1, "pI1 index out of bound.\n" );
        return 1;
    }

    if ( pi2 >= Abc_NtkPiNum(pNtk) || pi2 < 0 )
    {
        Abc_Print( -1, "pI2 index out of bound.\n" );
        return 1;
    }

    Lsv_AigSymmetry( pNtk , po , pi1 , pi2 );
    
    return 0;

usage:
    Abc_Print(-2, "usage: lsv_sym_sat <pO> <pI1> <pI2> [-h]\n");
    Abc_Print(-2, "\t        Check symmetry on 2 variables in aig\n");
    Abc_Print(-2, "\t-h    : print the command usage\n");
    return 1;
}

/**
 * lsv_sym_all <i>
*/
void Lsv_AigAllSymmetry ( Abc_Ntk_t * pNtk , int o) {
    assert( Abc_NtkIsStrash( pNtk ));

    int n = Abc_NtkCiNum( pNtk ) ;
    Abc_Obj_t * pObj;
    int Lits[100], i;

    Abc_Obj_t * pO = Abc_NtkPo( pNtk, o );
    Abc_Obj_t * pNode = Abc_ObjFanin0(pO);
    assert( pNode );
    assert( Abc_ObjIsNode(pNode) );
    char * pOName = Abc_ObjName(pO);

    // Create Cone
    Abc_Ntk_t * Cone = Abc_NtkCreateCone( pNtk, pNode, pOName, 1);
    // pRoot is the po in Cone, which is slightly different from pNode from pNtk.
    Abc_Obj_t * pRoot = Abc_ObjFanin0( Abc_NtkPo(Cone, 0) ); 
    
    // NtkToDar
    Aig_Man_t * aigMan = Abc_NtkToDar( Cone, 0, 0 );

    // New Sat solver
    sat_solver * pSat = sat_solver_new(); // SAT Solver
    // pSat->fPrintClause = 1; // verbose

    // Add CNF into SAT solver
    Cnf_Dat_t * pCnf1 = Cnf_Derive( aigMan, Aig_ManCoNum(aigMan)); // CoNum should be 1
    pSat = (sat_solver *)Cnf_DataWriteIntoSolverInt( pSat, pCnf1, 1, 0 );

    // Create another CNF that depend on different input variable
    Cnf_Dat_t * pCnf2 = Cnf_Derive( aigMan, Aig_ManCoNum(aigMan)); // CoNum should be 1
    Cnf_DataLift( pCnf2, pCnf1->nVars + 1 );
    pSat = (sat_solver *)Cnf_DataWriteIntoSolverInt( pSat, pCnf2, 1, 0 );


// Below Part is different from Lsv_sym_aig()

    // Map constraints
    // add n-input vH(t) variables
    int assumptionOffset = pSat->size;
    sat_solver_setnvars( pSat, assumptionOffset + n );

    // (b)
    // printf("(b)\n");
    // Aig_ManForEachCi( aigMan, pObj, i )
    Abc_NtkForEachPi( Cone, pObj, i)
    {

        Lits[0] = toLitCond( pCnf1->pVarNums[pObj->Id], 0 );
        Lits[1] = toLitCond( pCnf2->pVarNums[pObj->Id], 1 );
        Lits[2] = toLitCond( assumptionOffset + i,  0 );
        if ( !sat_solver_addclause( pSat, Lits, Lits+3 ) )
            assert( 0 );
        Lits[0] = toLitCond( pCnf1->pVarNums[pObj->Id], 1 );
        Lits[1] = toLitCond( pCnf2->pVarNums[pObj->Id], 0 );
        Lits[2] = toLitCond( assumptionOffset + i,  0 );
        if ( !sat_solver_addclause( pSat, Lits, Lits+3 ) )
            assert( 0 );
    }

    // (c) (d)
    // printf("(c)(d)\n");
    Abc_Obj_t * pObj1, * pObj2;
    Abc_NtkForEachPi( Cone, pObj1, i )
    {
        int j;
        Abc_NtkForEachPi( Cone, pObj2, j )
        {
            if ( j >= i ) continue;
            
            // (c)
            // printf("(c)\n");
            Lits[0] = toLitCond( pCnf1->pVarNums[pObj1->Id], 0 );
            Lits[1] = toLitCond( pCnf2->pVarNums[pObj2->Id], 1 );
            Lits[2] = toLitCond( assumptionOffset + i,  1 );
            Lits[3] = toLitCond( assumptionOffset + j,  1 );
            if ( !sat_solver_addclause( pSat, Lits, Lits+4 ) )
                assert( 0 );
            Lits[0] = toLitCond( pCnf1->pVarNums[pObj1->Id], 1 );
            Lits[1] = toLitCond( pCnf2->pVarNums[pObj2->Id], 0 );
            Lits[2] = toLitCond( assumptionOffset + i,  1 );
            Lits[3] = toLitCond( assumptionOffset + j,  1 );
            if ( !sat_solver_addclause( pSat, Lits, Lits+4 ) )
                assert( 0 );

            // (d)
            // printf("(d)\n");
            Lits[0] = toLitCond( pCnf1->pVarNums[pObj2->Id], 0 );
            Lits[1] = toLitCond( pCnf2->pVarNums[pObj1->Id], 1 );
            Lits[2] = toLitCond( assumptionOffset + i,  1 );
            Lits[3] = toLitCond( assumptionOffset + j,  1 );
            if ( !sat_solver_addclause( pSat, Lits, Lits+4 ) )
                assert( 0 );
            Lits[0] = toLitCond( pCnf1->pVarNums[pObj2->Id], 1 );
            Lits[1] = toLitCond( pCnf2->pVarNums[pObj1->Id], 0 );
            Lits[2] = toLitCond( assumptionOffset + i,  1 );
            Lits[3] = toLitCond( assumptionOffset + j,  1 );
            if ( !sat_solver_addclause( pSat, Lits, Lits+4 ) )
                assert( 0 );
        }
    }



    // XOR output // (a) vA(yk) XOR vB(yk)
    Lits[0] = toLitCond( pCnf1->pVarNums[pRoot->Id], 0 );
    Lits[1] = toLitCond( pCnf2->pVarNums[pRoot->Id], 0 );
    if ( !sat_solver_addclause( pSat, Lits, Lits+2 ) )
        assert( 0 );
    Lits[0] = toLitCond( pCnf1->pVarNums[pRoot->Id], 1 );
    Lits[1] = toLitCond( pCnf2->pVarNums[pRoot->Id], 1 );
    if ( !sat_solver_addclause( pSat, Lits, Lits+2 ) )
        assert( 0 );

    // Assertion should be put in sat_solver_solve( ., Lit* start, Lit* end, ...)
    // Lits[0] = toLitCond( pCnf1->pVarNums[], 0 );
    // Lits[1] = toLitCond( pCnf2->pVarNums[pI2_id], 0 );
    // Lits[2] = toLitCond( pCnf1->pVarNums[pI1_id], 0 );
    // Lits[3] = toLitCond( pCnf2->pVarNums[pI2_id], 0 );

    // Below is the part to iterate all is and js
    int j;
    // i = 0;
    // j = 2;

    // std::cout << std::endl;
    // std::cout << "Start!" << std::endl;
    for (int k=0; k<n; ++k) { // initialize
        Lits[k] = Abc_Var2Lit( assumptionOffset + k, 1);
    }

    Abc_NtkForEachPi( Cone, pObj1, i ) {
        Abc_NtkForEachPi( Cone, pObj2, j) {
            if (i >= j) continue;
            // for (int k=0; k<n; ++k) {
            //     Lits[k] = Abc_Var2Lit( assumptionOffset + k, (k == i || k == j)?0:1);
            // }
            Lits[i] = Abc_Var2Lit(assumptionOffset+i, 0);
            Lits[j] = Abc_Var2Lit(assumptionOffset+j, 0);

            // Solve
            int status;
            status = sat_solver_solve( pSat, Lits, Lits+n, (ABC_INT64_T)1000000, (ABC_INT64_T)0, (ABC_INT64_T)0, (ABC_INT64_T)0 );
            if ( status == l_False )
            {
                std::cout << i << " " << j << std::endl;
                // printf( "symmetric\n" );
            }
            else if ( status == l_True )
            {
                // std::cout << i << " " << j << std::endl;
                // printf( "asymmetric\n" );

                // // derive asymmetric counterexample
                // int temp;
                // Abc_NtkForEachPi( pNtk, pObj, temp )
                // {
                //     std::cout << sat_solver_var_value( pSat, (pCnf1->pVarNums[pObj->Id]) );
                // }
                // std::cout << std::endl;

                // Abc_NtkForEachPi( pNtk, pObj, temp )
                // {
                //     std::cout << sat_solver_var_value( pSat, pCnf2->pVarNums[pObj->Id] );
                // }
                // std::cout << std::endl;
            }
            else
            {
                printf( "undef\n" );
            }

            Lits[i] = Abc_Var2Lit(assumptionOffset+i, 1);
            Lits[j] = Abc_Var2Lit(assumptionOffset+j, 1);
        }
    }



    sat_solver_delete( pSat );
    Aig_ManStop( aigMan );
    Cnf_DataFree( pCnf1 );
    Cnf_DataFree( pCnf2 );
    Abc_NtkDelete( Cone );
    return;
}

int Lsv_CommandSymAll(Abc_Frame_t* pAbc, int argc, char** argv) {
     Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    if (!pNtk) {
        Abc_Print(-1, "Empty network.\n");
        return 1;
    }

    int c;
    Extra_UtilGetoptReset();

    std::string simS;
    int po;

    while ( ( c = Extra_UtilGetopt(argc, argv, "h" ) ) != EOF ) {
        switch (c) {
            case 'h':
                goto usage;
            default:
                break;
        }
    }

    // get input string
    char ** pArgvNew;
    char * vString;
    int nArgcNew;
    pArgvNew = argv + globalUtilOptind;
    nArgcNew = argc - globalUtilOptind;
    if ( nArgcNew != 1 ) {
        goto usage;
    }

    vString = pArgvNew[0];
    simS = std::string(vString);
    po = std::stoi(simS);

    // std::cout << po << " " << pi1 << " " << pi2 << " " << std::endl;

    if ( !Abc_NtkIsStrash(pNtk) )
    {
        Abc_Print( -1, "Simulating SAT can only be done for logic AIG networks (run \"strash\").\n" );
        return 1;
    }

    // Size Check
    if ( po >= Abc_NtkPoNum(pNtk) || po < 0 )
    {
        Abc_Print( -1, "pO index out of bound.\n" );
        return 1;
    }

    Lsv_AigAllSymmetry( pNtk , po );
    
    return 0;

usage:
    Abc_Print(-2, "usage: lsv_sym_all <pO> [-h]\n");
    Abc_Print(-2, "\t        Check symmetry on <pO> output in aig\n");
    Abc_Print(-2, "\t-h    : print the command usage\n");
    return 1;
}

ABC_NAMESPACE_IMPL_END