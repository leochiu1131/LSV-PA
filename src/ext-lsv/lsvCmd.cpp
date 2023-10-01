#include <string>
#include <fstream>
#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimBdd(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv);

/**
 * auxiliary function: printBits()
*/
void printBits(unsigned int num, int size=32){
    unsigned int maxPow = 1<<(size-1);
    // printf("MAX POW : %u\n",maxPow);
    int i=0;
    for(;i<size;++i){
      // print last bit and shift left.
      printf("%u ", !!(num&maxPow));
      num = num<<1;
    }
    printf("\n");
}

void init(Abc_Frame_t* pAbc) {
    Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", Lsv_CommandSimBdd, 0);
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig", Lsv_CommandSimAig, 0);
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

// coding starts here

void Lsv_BddSimulation( Abc_Ntk_t * pNtk , Vec_Ptr_t *& vSimPat ) {

    // DdManager * dd = (DdManager *) pNtk->pManFunc;
    // printf( "BDD size = %6d nodes.  \n", Cudd_ReadSize( dd ) );

    // Original Order Names
    int j=0;
    Abc_Obj_t * pPi;
    int n = Abc_NtkCiNum(pNtk);
    Vec_Ptr_t * vNames = Vec_PtrStart( n );

    // Write Names
    Abc_NtkForEachCi(pNtk, pPi, j) {
        Vec_PtrWriteEntry( vNames, j, Abc_ObjName(pPi));
    }

  // trace every Po's bdd
    int i=0;
    Abc_Obj_t * pPo;
    Abc_NtkForEachPo( pNtk, pPo, i ) {
        Abc_Obj_t * pObj;
        pObj = Abc_ObjChild0(pPo);
        assert( Abc_NtkIsBddLogic(pObj->pNtk) );

        DdManager * dd = (DdManager *)pObj->pNtk->pManFunc;  
        // DdNode * bdd = (DdNode * )Abc_ObjGlobalBdd(pCo); // Not working
        // Reason: Abc_ObjGlobalBdd(pCo) is uncallable (segfault)
        // So, how can we get the globalBdds?

        // print current Po name
        (void) fprintf( dd->out, "%s", Abc_ObjName(pPo) );

        // Get bdd
        DdNode * bdd = ( DdNode * ) pObj->pData; // They share the same DdManager.
        if (bdd == NULL) {
            Abc_Print(1, ": No bdd.\n");
            (void) fflush( dd->out );
            continue; // irrevelent obj
        }
        
        (void) fflush( dd->out );

        // get Fanin Names for Simulation
        Vec_Ptr_t * vNamesIn = Vec_PtrStart( 0 );
        vNamesIn = Abc_NodeGetFaninNames( pObj );
        // char** vNamesInC = (char**) Abc_NodeGetFaninNames( pObj )->pArray;
        int i;  char* name;
        // Vec_PtrForEachEntry( char*, vNamesIn, name, i) {
        //     Abc_Print(1, "Name%d : %s\n", i, name);
        // }

        // Debug Info // USEFUL
        // Cudd_PrintDebug(dd, bdd, 10, 3); 

        // Trace down the variables, until we meet a constant.
        // Reference: ddPrintMintermAux()
        // DdNode *node = Cudd_BddToAdd(dd, bdd);
        DdNode *node = bdd; // node pointer
        DdNode *N;
        N = Cudd_Regular( node );

        i=0;
        while( !cuddIsConstant( N ) ) {
          char * e = (char*)Vec_PtrEntry( vNamesIn, N->index );
          int j=0;
          Vec_PtrForEachEntry( char*, vNames, name, j ) {
              if ( strcmp(name, e) == 0) break;
          }

          if( ! Vec_PtrEntry(  vSimPat, j ) ) { // inverse
              // Abc_Print(1, "E\n");
              if ( Cudd_IsComplement( node ) )
                node = Cudd_Not( cuddE( N ) );
              else
                node = cuddE( N );
          }
          else {
              // Abc_Print(1, "T\n");
              if ( Cudd_IsComplement( node ) )
                node = Cudd_Not( cuddT( N ) );
              else
                node = cuddT( N );
          }
          N = Cudd_Regular( node );
        }
        
        // Judge if we walk to ONE or ZERO
        DdNode *azero, *bzero;
        azero = DD_ZERO( dd );
        bzero = Cudd_Not( DD_ONE( dd ) );
        if (node != azero && node != bzero && node != dd->background){
            (void) fprintf(dd->out,": 1\n");
            (void) fflush(dd->out);
        }
        // else if (!(node == azero || node == bzero)) {
        //   (void) fprintf(dd->out,": 0\n");
        //   (void) fflush(dd->out);
        // }
        else {
            (void) fprintf(dd->out,": 0\n");
            (void) fflush(dd->out);
        }
    }
    return;
}

int Lsv_CommandSimBdd(Abc_Frame_t* pAbc, int argc, char** argv) {
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    int c, i=0;
    Extra_UtilGetoptReset();

    int n = Abc_NtkCiNum( pNtk );
    Vec_Ptr_t * vSimPat = Vec_PtrStart( n );
    std::string simS;

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
    char * vSimString;
    int nArgcNew;
    pArgvNew = argv + globalUtilOptind;
    nArgcNew = argc - globalUtilOptind;
    if ( nArgcNew != 1 )
    {
        Abc_Print( -1, "There is no simulation pattern.\n" );
        return 1;
    }

    vSimString = pArgvNew[0];
    simS = std::string(vSimString);
    if (simS.length() != n) {
        Abc_Print( -1, "The length of simulation pattern is not equal to the number of inputs.\n" );
        return 1;
    }

    for (auto c : simS) {
        if (c == '0') {
           Vec_PtrWriteEntry( vSimPat, i++,(void *) false );
        }
        else if (c == '1') {
           Vec_PtrWriteEntry( vSimPat, i++,(void *) true );
        }
        else {
            Abc_Print( -1, "The simulation pattern should be binary.\n" );
            return 1;
        }
    }



    if (!pNtk) {
        Abc_Print(-1, "Empty network.\n");
        return 1;
    }
    if ( !Abc_NtkIsBddLogic(pNtk) )
    {
        Abc_Print( -1, "Simulating BDDs can only be done for logic BDD networks (run \"bdd\").\n" );
        return 1;
    }

    // Abc_Print(1, "Start!\n");

    // Abc_Print(1, "simPattern Size: %6d\n", vSimPat->nSize);
    // Vec_PtrWriteEntry( vSimPat, 0, (void *) true);
    // Vec_PtrWriteEntry( vSimPat, 1, (void *) false);
    // Vec_PtrWriteEntry( vSimPat, 2, (void *) false);
    // Vec_PtrWriteEntry( vSimPat, 3, (void *) true);

    Lsv_BddSimulation( pNtk , vSimPat );
    
    Vec_PtrErase( vSimPat );
    return 0;

usage:
    Abc_Print(-2, "usage: lsv_sim_bdd <input_pattern> [-h]\n");
    Abc_Print(-2, "\t        perform simulation on bdd\n");
    Abc_Print(-2, "\t-h    : print the command usage\n");
    return 1;
}

void Lsv_AigSimulation ( Abc_Ntk_t * pNtk, Vec_Ptr_t *& vSimPatGrp, int lineCount ) {
  int i=0, j=0;
  Abc_Obj_t * pObj;
  Vec_Ptr_t * vSimPat;

  Vec_PtrForEachEntry( Vec_Ptr_t *, vSimPatGrp, vSimPat, j) {
    Abc_Print(1, "=== Group %d Simulation ===\n", j);

    // Assign value to PI
    Abc_NtkForEachPi( pNtk, pObj, i ) {
      pObj->pData = (void *)Vec_PtrEntry( vSimPat, i );
      // printBits((size_t)pObj->pData);
    }

    // Traverse AIG, run simulation
    Abc_NtkForEachObj(pNtk, pObj, i) {
        // skip PI/PO and const1/0s
        if (Abc_ObjType(pObj) == ABC_OBJ_PI) continue;
        if (Abc_ObjType(pObj) == ABC_OBJ_CONST1) {
            pObj->pData = (void * )1;
            continue;
        }
        if (Abc_ObjType(pObj) == ABC_OBJ_PO) {
            // Abc_Print(1, "Obj %s | c0: %s\n", Abc_ObjName(pObj), Abc_ObjName(Abc_ObjFanin0(pObj)));
            continue;
        }

        // Abc_Obj_t * pFanin;
        // int j;
        // Abc_Obj_t * Fanin0;
        // Abc_Obj_t * Fanin1;
        // Abc_ObjForEachFanin(pObj, pFanin, j) {
        //   assert(j < 2);
        //   // printf("  Fanin-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanin),
        //   //        Abc_ObjName(pFanin));
        //   if (j == 0) {
        //     Fanin0 = pFanin;
        //     // printf("  Fanin-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanin),
        //     //      Abc_ObjName(Fanin0));
        //   }
        //   else if (j == 1){
        //     Fanin1 = pFanin;
        //     // printf("  Fanin-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanin),
        //     //      Abc_ObjName(Fanin1));
        //   }
        // }

        Abc_Obj_t * Fanin0 = Abc_ObjFanin(pObj, 0);
        Abc_Obj_t * Fanin1 = Abc_ObjFanin(pObj, 1);

        // Abc_Print(1, "Obj : %s", Abc_ObjName(pObj));

        // This print fails for some unknown reason.....
        // Abc_Print(1, " | c0:%s, c1:%s\n", 
        //           Abc_ObjName(Fanin0), 
        //           Abc_ObjName(Fanin1));

        // Abc_Print(1, "%d, %d \n", Abc_ObjFaninC0(pObj), Abc_ObjFaninC1(pObj));
        // Abc_Print(1, "sizeof(size_t): %d\n", sizeof(size_t) ); 
        size_t data = Abc_ObjFaninC0(pObj) ? ~(size_t)Fanin0->pData : (size_t)Fanin0->pData;
        data = Abc_ObjFaninC1(pObj) ? data & ~(size_t)Fanin1->pData 
                                                  : data & (size_t)Fanin1->pData;
        pObj->pData = (void *)data;
        // printBits((size_t)pObj->pData);
    }

    // Print PO
    Abc_NtkForEachPo(pNtk, pObj, i) {
        Abc_Print(1, "%s : ", Abc_ObjName(pObj));

        Abc_Obj_t * Fanin0 = Abc_ObjFanin0(pObj);
        // Abc_Print(1, "Child :%s\n", Abc_ObjName(Fanin0));
        size_t data = Abc_ObjFaninC0(Fanin0) ? ~(size_t)Fanin0->pData : (size_t)Fanin0->pData;
        pObj->pData = (void *)data;

        if (lineCount >= 32) {
          printBits((size_t)pObj->pData);
        }
        else {
          printBits((size_t)pObj->pData, lineCount);
        }
    }
    lineCount -= 32;
    
  }
  
  Abc_Print(1, "Done Simulation.\n");
  return;
}

int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv) {
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    int c;
    Extra_UtilGetoptReset();

    int n = Abc_NtkCiNum( pNtk );
    Vec_Ptr_t * vSimPat = Vec_PtrStart( n );
    Vec_PtrFill( vSimPat, n, 0 );
    Vec_Ptr_t * vSimPatGrp = Vec_PtrAlloc( 0 );
    std::ifstream infile;
    std::string line;
    int lineCount = 0;

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
    char * fileSim;
    int nArgcNew;
    pArgvNew = argv + globalUtilOptind;
    nArgcNew = argc - globalUtilOptind;
    if ( nArgcNew != 1 )
    {
        Abc_Print( -1, "There is no simulation pattern.\n" );
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
        if (line.length() != n) {
          Abc_Print( -1, "The length of simulation pattern is not equal to the number of inputs.\n" );
          return 1;
        }
        for (int j=0; j<n ;++j) {
          size_t input = (size_t)Vec_PtrEntry( vSimPat, j );
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
          Vec_PtrWriteEntry( vSimPat, j, (void *)input );
        }
        ++lineCount;
        if ( lineCount % 32 == 0 ) {
            Vec_PtrPush( vSimPatGrp, vSimPat );
            vSimPat = Vec_PtrStart( n );
            // Vec_PtrClear( vSimPat );
            Vec_PtrFill( vSimPat, n, 0 );
        }
    }
    infile.close();

    if ( lineCount % 32 != 0 ) {
        Vec_PtrPush( vSimPatGrp, vSimPat );
    }

    Abc_Print(1, "Number of Simulation Groups: %d\n", Vec_PtrSize( vSimPatGrp ));
    // vSimPat = (Vec_Ptr_t *)Vec_PtrEntry( vSimPatGrp, 0 );
    // Abc_Print(1, "Sim Pattern Size: %d\n", Vec_PtrSize( vSimPat ));
    // for (size_t i = 0; i < vSimPat->nSize; ++i){
    //   printBits((size_t)Vec_PtrEntry( vSimPat, i ));
    // }

    // Abc_Print(1, "Start!\n");

    Lsv_AigSimulation( pNtk , vSimPatGrp, lineCount );
    
    Vec_PtrReleaseArray( vSimPatGrp );
    Vec_PtrErase( vSimPat );

    return 0;

usage:
    Abc_Print(-2, "usage: lsv_sim_aig <input_pattern> [-h]\n");
    Abc_Print(-2, "\t        perform 32-bit parallel simulation on aig\n");
    Abc_Print(-2, "\t-h    : print the command usage\n");
    return 1;
}