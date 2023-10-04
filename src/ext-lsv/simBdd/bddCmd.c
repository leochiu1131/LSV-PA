#include "base/main/main.h"
#include "bdd.h"

ABC_NAMESPACE_IMPL_START

static int Abc_CommandExtLsvSimBdd ( Abc_Frame_t * pAbc, int argc, char ** argv );

void ExtLsvSimBdd_Init( Abc_Frame_t * pAbc){
    Cmd_CommandAdd( pAbc, "Ext",         "lsv_sim_bdd",    Abc_CommandExtLsvSimBdd,     0 );
}

int Abc_CommandExtLsvSimBdd( Abc_Frame_t * pAbc, int argc, char ** argv )
{
    Abc_Ntk_t * pNtk = Abc_FrameReadNtk(pAbc);
    int c;
    int n = pNtk->vPis->nSize;
    // printf("primary input count = %d.\n",n);

    // set defaults
    Extra_UtilGetoptReset();
    while ( ( c = Extra_UtilGetopt( argc, argv, "h" ) ) != EOF )
    {
        switch ( c )
        {
        case 'h':
            goto usage;
        default:
            goto usage;
        }
    }
    int length = 0;
    if ( pNtk == NULL )
    {
        Abc_Print( -1, "Empty network.\n" );
        return 1;
    }
    if ( !Abc_NtkIsBddLogic(pNtk) )
    {
        Abc_Print( -1, "Simulation only for BDD.\n" );
        return 1;
    }
    if ( argc < 2 ) // TODO: check if there is input pattern
    {
        Abc_Print( -1, "Missing test pattern.\n" );
        return 1;
    }
    while(argv[1][length]!='\0'){
        length++;
    }
    if ( length != n ) // TODO: check if input pattern has correct length
    {
        Abc_Print( -1, "Test pattern length mismatch.\n" );
        return 1;
    }
    if ( !Abc_NtkSimBdd(pNtk,argv[1]) ) // TODO: check if simulation correctly end
    {
        Abc_Print( -1, "Simulation BDD has failed.\n" );
        return 1;
    }
    return 0;

usage:
    Abc_Print( -2, "usage: lsv_sim_bdd [-h] <file>\n" );
    Abc_Print( -2, "\t         do simulations for a given BDD and an input pattern\n" );
    Abc_Print( -2, "\t-h     : print the command usage\n");
    return 1;
}
ABC_NAMESPACE_IMPL_END