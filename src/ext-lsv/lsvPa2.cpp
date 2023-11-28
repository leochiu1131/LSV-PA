#include "lsvCmd.h"

// ABC_NAMESPACE_IMPL_START helps abc to fetch namespace from all files
ABC_NAMESPACE_IMPL_START

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

void Lsv_AigSymmetry ( Abc_Ntk_t * pNtk, std::vector<std::vector<unsigned int>>& vSimPatGrp, int lineCount ) {
  int i=0;
  Abc_Obj_t * pObj;
  std::vector<unsigned int> temp;
  std::vector<std::vector<unsigned int>> output(Abc_NtkPoNum(pNtk), temp);

    for( int j=0,N=vSimPatGrp.size(); j<N; ++j) {

        std::vector<unsigned int> vSimPat = vSimPatGrp[j];

        // reset iTemp
        Abc_NtkForEachObj(pNtk, pObj, i) {
            pObj->iTemp = 0;
        }
        // Assign value to PI
        Abc_NtkForEachPi( pNtk, pObj, i ) {
            pObj->iTemp = (int)vSimPat[i];
        }

        // Traverse AIG, run simulation
        Abc_NtkForEachObj(pNtk, pObj, i) {
            // skip PI/PO and const1/0s
            if (Abc_ObjType(pObj) == ABC_OBJ_PI) continue;
            if (Abc_ObjType(pObj) == ABC_OBJ_CONST1) {
                pObj->iTemp = (int)UINT64_MAX;
                continue;
            }
            if (Abc_ObjType(pObj) == ABC_OBJ_PO) {
                // Abc_Print(1, "Obj %s | c0: %s\n", Abc_ObjName(pObj), Abc_ObjName(Abc_ObjFanin0(pObj)));
                continue;
            }
            if (Abc_ObjType(pObj) != ABC_OBJ_NODE) continue;

            // Abc_Print(1, "Obj : %s", Abc_ObjName(pObj));
            // Abc_Print(1, "%s(%d)| %s(%d) \n", Abc_ObjName(Fanin0), Abc_ObjFaninC0(pObj), Abc_ObjName(Fanin1), Abc_ObjFaninC1(pObj));


            // And-Inverter Simulation
            Abc_Obj_t * Fanin0 = Abc_ObjFanin0(pObj);
            Abc_Obj_t * Fanin1 = Abc_ObjFanin1(pObj);
            unsigned int a = (unsigned int)Fanin0->iTemp;
            if (Abc_ObjFaninC0(pObj)) a ^= UINT64_MAX;
            unsigned int b = (unsigned int)Fanin1->iTemp;
            if (Abc_ObjFaninC1(pObj)) b ^= UINT64_MAX;
            unsigned int data = a & b;
            
            pObj->iTemp = (int)data;
            // Abc_Print(1, "Data: ");
            // printBits(data);
            // Abc_Print(1, "\n");
        }

        // Print PO
        Abc_NtkForEachPo(pNtk, pObj, i) {

            unsigned int data;
            if (Abc_ObjFaninNum(pObj) == 1) {
                Abc_Obj_t * Fanin0 = Abc_ObjFanin0(pObj);
                // Abc_Obj_t * Fanin1 = Abc_ObjChild1(pObj);
                // Abc_Print(1, "Obj : %s", Abc_ObjName(pObj));
                // Abc_Print(1, "%s(%d)| %s(%d) \n", Abc_ObjName(Fanin0), Abc_ObjFaninC0(pObj), Abc_ObjName(Fanin1), Abc_ObjFaninC1(pObj));
                unsigned int iTemp0 = (unsigned int)Fanin0->iTemp;
                if (Abc_ObjFaninC0(pObj)) 
                    iTemp0 ^= UINT32_MAX;

                data = iTemp0;
            }
            else {
                Abc_Print(-1, "Error: Fanin of Po has problem.");
            }
            // else if (Abc_ObjFaninNum(pObj) > 1) {
            //     Abc_Obj_t * Fanin0 = Abc_ObjFanin0(pObj);
            //     Abc_Obj_t * Fanin1 = Abc_ObjFanin1(pObj);
            //     Abc_Print(1, "Obj : %s", Abc_ObjName(pObj));
            //     Abc_Print(1, "%s(%d)| %s(%d) \n", Abc_ObjName(Fanin0), Abc_ObjFaninC0(pObj), Abc_ObjName(Fanin1), Abc_ObjFaninC1(pObj));
            //     unsigned int iTemp0 = (unsigned int)Fanin0->iTemp;
            //     if ( Abc_ObjFaninC0(pObj) ) 
            //         iTemp0 ^= UINT32_MAX;
            //     unsigned int iTemp1 = (unsigned int)Fanin1->iTemp;
            //     if ( Abc_ObjFaninC1(pObj) ) 
            //         iTemp1 ^= UINT32_MAX;

            //     data = iTemp0 & iTemp1;
            // }
            // else {
            //     Abc_Print(-1, "Error: Fanin of Po has problem.");
            // }
            // Abc_Print(1, "Po : %s | Child : %s\n", Abc_ObjName(pObj), Abc_ObjName(Fanin0));
            // Abc_Print(1, "%s(%d)| %s(%d) \n", Abc_ObjName(Fanin0), Abc_ObjFaninC0(pObj), Abc_ObjName(Fanin1), Abc_ObjFaninC1(pObj));

            pObj->iTemp = (int)data; // Is this needed?

            output[i].push_back(data);
        }
    }

    // Abc_NtkForEachObj(pNtk, pObj, i) {
    //     Abc_Print(1, "%s : ", Abc_ObjName(pObj));
    //     printBits((unsigned int)pObj->iTemp, 3);
    //     Abc_Print(1, "\n");
    // }

    // Print
    Abc_NtkForEachPo(pNtk, pObj, i) {
        Abc_Print(1, "%s : ", Abc_ObjName(pObj));
        int eachLine = lineCount;
        for (auto& o : output[i]) {
            if (eachLine >= 32) {
                printBits(o, 32);
            }
            else {
                printBits(o, eachLine);
            }
            eachLine -= 32;
        }
        Abc_Print(1, "\n");
    }
  
//   Abc_Print(1, "Done Simulation.\n");
  return;
}

int Lsv_CommandSymAig(Abc_Frame_t* pAbc, int argc, char** argv) {
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    int c;
    Extra_UtilGetoptReset();
    if ( pNtk == NULL ) {
        Abc_Print( -1, "Empty network.\n" );
        return 1;
    }

    int n = Abc_NtkCiNum( pNtk );
    // Simulation Pattern in one round
    std::vector<unsigned int> vSimPat(n+1, 0);
    // Simulation Patterns read from infile
    std::vector<std::vector<unsigned int>> vSimPatGrp;

    std::ifstream infile; // Read file ifstream
    std::string line; // Read file buffer

    int lineCount = 0; // Line count for output

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
    char * fileSim; // Name of infile
    int nArgcNew;
    pArgvNew = argv + globalUtilOptind;
    nArgcNew = argc - globalUtilOptind;
    if ( nArgcNew != 1 )
    {
        Abc_Print( -1, "There is no File.\n" );
        return 1;
    }

    if (!pNtk) {
        Abc_Print(-1, "Empty network.\n");
        return 1;
    }
    if ( !Abc_NtkIsStrash(pNtk) )
    {
        Abc_Print( -1, "Simulating Aigs can only be done for Strashed networks (run \"strash\").\n" );
        return 1;
    }

    fileSim = pArgvNew[0];
    infile.open( fileSim );
    if (!infile) {
        Abc_Print( -1, "File %s cannot open.\n", fileSim );
        return 1;
    }

    // Abc_Print(1, "Reading file: %s\n", fileSim);
    while (std::getline(infile, line)) {
        if (line.length() == n + 1) {
            if (line.back() == '\r') {
                // Abc_Print(1, "Windows enter MDFK!\n");
            } else return 1;
        }
        else if (line.length() != n) {
          Abc_Print( -1, "The length of simulation pattern is not equal to the number of inputs.\n" );
          return 1;
        }
        for (int j=0; j<n ;++j) {
          unsigned int& input = vSimPat[j];
          if (line[j] == '0') {
            input = (input << 1) + 0;
          }
          else if (line[j] == '1') {
            input = (input << 1) + 1;
          }
          else {
            Abc_Print( -1, "The simulation pattern should be binary.\n" );
            return 1;
          }
        }
        ++lineCount;
        if ( lineCount % 32 == 0 ) {
            // put vSimPat into vSimPatGrp
            vSimPatGrp.push_back(vSimPat);
            // reset vSimPat
            for( auto& a: vSimPat ) {
                a = (int)0;
            }
        }
    }
    infile.close();

    // if remains unpush_back patterns
    if ( lineCount % 32 != 0 ) {
        vSimPatGrp.push_back(vSimPat);
    }

    // Abc_Print(1, "Number of Simulation Groups: %d\n",vSimPatGrp.size() );
    // for (auto& v: vSimPatGrp) {
    //     for (auto& v1: v) {
    //         printBits(v1, lineCount);
    //         Abc_Print(1, "\n");
    //     }
    // }

    // Abc_Print(1, "Start!\n");

    Lsv_AigSymmetry( pNtk , vSimPatGrp, lineCount );
    
    // Clear All memory
    for( auto& grp: vSimPatGrp ) {
        grp.clear();
    }
    vSimPatGrp.clear();
    vSimPat.clear();

    return 0;

usage:
    Abc_Print(-2, "usage: lsv_sim_aig <input_file> [-h]\n");
    Abc_Print(-2, "\t        Perform 32-bit parallel simulation on AIG.\n");
    Abc_Print(-2, "\t-h    : print the command usage\n");
    return 1;
}

ABC_NAMESPACE_IMPL_END