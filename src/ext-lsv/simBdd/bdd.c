#include "base/abc/abc.h"
#include "bdd/bbr/bbr.h"

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_NtkSimBdd(Abc_Ntk_t * pNtk, char* piValue)
{
    Abc_Obj_t *pObj;
    DdNode * bFunc,*scan;
    Vec_Ptr_t * vFuncsGlob;

    int         slots;
    DdNodePtr   *nodelist;

    st__table    *visited = NULL;
    st__generator *gen = NULL;
    int i,j,k, fReorder=1,n,nvars,retval;
    char ** POname,**PIname ;
    int *output;
    long    refAddr, diff, mask;

    Abc_Ntk_t * pTemp = Abc_NtkIsStrash(pNtk) ? pNtk : Abc_NtkStrash(pNtk, 0, 0, 0);
    assert( Abc_NtkIsStrash(pTemp) );
    DdManager* dd = (DdManager *)Abc_NtkBuildGlobalBdds( pTemp, 10000000, 1, fReorder, 0, 0 );
    nvars = dd->size;
    
    vFuncsGlob = Vec_PtrAlloc( Abc_NtkCoNum(pTemp) );
    Abc_NtkForEachCo( pTemp, pObj, i )
        Vec_PtrPush( vFuncsGlob, Abc_ObjGlobalBdd(pObj) );
    DdNode ** pbAdds = ABC_ALLOC( DdNode *, Vec_PtrSize(vFuncsGlob) );
    Vec_PtrForEachEntry( DdNode *, vFuncsGlob, bFunc, i )
        { pbAdds[i] = Cudd_BddToAdd( dd, bFunc ); Cudd_Ref( pbAdds[i] ); }
    n =Abc_NtkCoNum(pTemp);
    POname  = Abc_NtkCollectCioNames( pTemp, 1 );
    PIname  = Abc_NtkCollectCioNames( pTemp, 0 );
    output = malloc(n*sizeof(int));

    /* Initialize symbol table for visited nodes. */
    visited = st__init_table( st__ptrcmp, st__ptrhash);
    if (visited == NULL) goto failure;
    /* Collect all the nodes of this DD in the symbol table. */
    for (i = 0; i < n; i++) {
        retval = cuddCollectNodes(Cudd_Regular(pbAdds[i]),visited);
        if (retval == 0) goto failure;
    }

    /* Find the bits that are different. */
    refAddr = (long) Cudd_Regular(pbAdds[0]);
    diff = 0;
    gen = st__init_gen(visited);
    if (gen == NULL) goto failure;
    while ( st__gen(gen, (const char **)&scan, NULL)) {
        diff |= refAddr ^ (long) scan;
    }
    st__free_gen(gen); gen = NULL;
    /* Choose the mask. */
    for (i = 0; (unsigned) i < 8 * sizeof(long); i += 4) {
        mask = (1 << i) - 1;
        if (diff <= mask) break;
    }
    long *nodeId = malloc(n*sizeof(long));

    for(i=0;i<n;i++){
        nodeId[i]=((mask & (ptrint) pbAdds[i]) / sizeof(DdNode));
    }    
    for (i = 0; i < nvars; i++) {
            nodelist = dd->subtables[i].nodelist;
            slots = dd->subtables[i].slots;
            for (j = 0; j < slots; j++) {
                scan = nodelist[j];
                while (scan != NULL) {
                    if ( st__is_member(visited,(char *) scan)) {
                        for(k=0;k<n;k++){
                            if(nodeId[k]==((mask & (ptrint) scan) / sizeof(DdNode))){
                                if(piValue[dd->invperm[i]]=='1'){
                                    nodeId[k] = (mask & (ptrint) cuddT(scan)) / sizeof(DdNode);
                                    if(cuddIsConstant(cuddT(scan))) {
                                        if((ptrint)(dd->one) == (ptrint) cuddT(scan)){
                                            output[k]=1;
                                        }else{
                                            output[k]=0;
                                        }
                                    }
                                }else{
                                    nodeId[k] = (mask & (ptrint) cuddE(scan)) / sizeof(DdNode);
                                    if(cuddIsConstant(cuddE(scan))) {
                                        if((ptrint)(dd->one) == (ptrint) cuddE(scan)){
                                            output[k]=1;
                                        }else{
                                            output[k]=0;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    scan = scan->next;
                }
            }
    }
    for(i=0;i<n;i++){
        printf("%s: %d\n",POname[i],output[i]);
    }
    st__free_table(visited);
    Vec_PtrForEachEntry( DdNode *, vFuncsGlob, bFunc, i )
        Cudd_RecursiveDeref( dd, pbAdds[i] );
    ABC_FREE( pbAdds );

    ABC_FREE( PIname );
    ABC_FREE( POname );
    // cleanup
    Abc_NtkFreeGlobalBdds( pTemp, 0 );
    Vec_PtrForEachEntry( DdNode *, vFuncsGlob, bFunc, i )
        Cudd_RecursiveDeref( dd, bFunc );
    Vec_PtrFree( vFuncsGlob );
    Extra_StopManager( dd );
    Abc_NtkCleanCopy( pTemp );
    return 1;
    failure:
        printf("ERROR");
        return 0;
}
