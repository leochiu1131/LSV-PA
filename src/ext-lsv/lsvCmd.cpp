#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <map>
#include <string>

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimBdd(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv);

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
// static void
// ddDFS(
//   DdManager * dd /* manager */,
//   DdNode * node /* current node */,
//   Vec_Ptr_t *& vSimPat,
//   int level )
// {
//     DdNode      *N,*Nv,*Nnv;
//     int         i,v;
//     DdNode      *background, *zero;
//     background = dd->background;
//     zero = Cudd_Not(dd->one);

//     N = Cudd_Regular(node); // pointer of node
//     // Abc_Print(1, "% g\n", cuddV(node));

//     if (cuddIsConstant(N)) {
//         /* Terminal case: Print one cube based on the current recursion
//         ** path, unless we have reached the background value (ADDs) or
//         ** the logical zero (BDDs).
//         */
//         if (node != background && node != zero) {
//             // (void) fprintf(dd->out," % g\n", cuddV(node));
//             (void) fprintf(dd->out,": 1\n");
//             (void) fflush( dd->out );
//         }
//         else if ( node == zero) {
//             // (void) fprintf(dd->out," % g\n", cuddV(node));
//             (void) fprintf(dd->out,": 0\n");
//             (void) fflush( dd->out );
//         }
//         else {
//             (void) fprintf(dd->out,": background\n");
//             (void) fflush( dd->out );
//         }
//     } else {
//         Nv  = cuddT(N);
//         Nnv = cuddE(N);
//         if (Cudd_IsComplement(node)) {
//             Nv  = Cudd_Not(Nv);
//             Nnv = Cudd_Not(Nnv);
//         }

//         // DFS
//         if(!Vec_PtrEntry(vSimPat, N->index)) {
//             ddDFS(dd,Nnv,vSimPat,level+1);
//         }
//         else {
//             ddDFS(dd,Nv,vSimPat,level+1);
//         }
//     }
//     return;

// } /* end of ddPrintMintermAux */

void Lsv_BddSimulation( Abc_Ntk_t * pNtk , Vec_Ptr_t *& vSimPat ) {

  // DdManager * dd = (DdManager *) pNtk->pManFunc;
  // printf( "BDD size = %6d nodes.  \n", Cudd_ReadSize( dd ) );

  // Original Order Names
  int j=0;
  Abc_Obj_t * pPi;
  Vec_Ptr_t * vNames;
  int n = Abc_NtkCiNum(pNtk);
  vNames = Vec_PtrStart( n );

  // print original names // delete later
  Abc_Print(1, "Original names:\n");
  Abc_NtkForEachCi(pNtk, pPi, j) {
      printf("Name %d: %s\n", j, Abc_ObjName(pPi));
      Vec_PtrWriteEntry( vNames, j, Abc_ObjName(pPi));
  }

  // trace every Po's bdd
  int i;
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
      Vec_Ptr_t *vNamesIn;
      vNamesIn = Abc_NodeGetFaninNames( pObj );
      // char** vNamesInC = (char**) Abc_NodeGetFaninNames( pObj )->pArray;
      int i;  char* name;
      // Vec_PtrForEachEntry( char*, vNamesIn, name, i) {
      //     Abc_Print(1, "Name%d : %s\n", i, name);
      // }
      // for (i = 0; i<Vec_PtrSize(vNamesIn); ++i) {
      //   printf("invperm[%d]: %d\n", i, (DdManager *)(static_cast<Abc_Ntk_t *>(pObj->pNtk)->pManFunc)->invperm[i];
      // }

      // Debug Info // USEFUL
      // Cudd_PrintDebug(dd, bdd, 10, 3); 

      // Trace down the variables, until we meet a constant.
      // DdNode *node = Cudd_BddToAdd(dd, bdd);
      DdNode *node = bdd; // node pointer
      DdNode *N;
      N = Cudd_Regular( node );

      i=0;
      while( !cuddIsConstant( N ) ) {
        // Abc_Print(1, "%d\n", N->index);
        char * e = (char*)Vec_PtrEntry(vNamesIn, N->index);
        int j=0;
        Vec_PtrForEachEntry( char*, vNames, name, j) {
          if ( strcmp(name, e) == 0) break;
        }
        // Abc_Print(1, "Input: %d - ", j);

        if( ! Vec_PtrEntry(  vSimPat, j ) ) { // inverse
          // Abc_Print(1, "E\n");
          if (Cudd_IsComplement(node))
            node = Cudd_Not( cuddE( N ) );
          else
            node = cuddE( N );
        }
        else {
          // Abc_Print(1, "T\n");
          if (Cudd_IsComplement(node))
            node = Cudd_Not( cuddT( N ) );
          else
            node = cuddT( N );
        }
        N = Cudd_Regular( node );
      }
      
      // Judge if we walk to ONE or ZERO
      DdNode *azero, *bzero;
      azero = DD_ZERO(dd);
      bzero = Cudd_Not(DD_ONE(dd));
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

      // ddDFS(dd, bdd, vSimPat, 0);
  }
  return;
}

int Lsv_CommandSimBdd(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c, i=0;
  Extra_UtilGetoptReset();
  // [TODO] input binary string
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

  Abc_Print(1, "Start!\n");

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

int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv) {
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
  Abc_Print(-2, "usage: lsv_sim_aig <input_pattern> [-h]\n");
  Abc_Print(-2, "\t        perform simulation on aig\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}