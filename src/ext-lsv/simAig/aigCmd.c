#include "base/main/main.h"
#include "aig.h"

ABC_NAMESPACE_IMPL_START

static int Abc_CommandExtLsvSimAig ( Abc_Frame_t * pAbc, int argc, char ** argv );

void ExtLsvSimAig_Init( Abc_Frame_t * pAbc){
    Cmd_CommandAdd( pAbc, "Ext",         "lsv_sim_aig",    Abc_CommandExtLsvSimAig,     0 );
}

int Abc_CommandExtLsvSimAig( Abc_Frame_t * pAbc, int argc, char ** argv )
{
    Abc_Ntk_t * pNtk = Abc_FrameReadNtk(pAbc);
    int c;
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
    if ( pNtk == NULL )
    {
        Abc_Print( -1, "Empty network.\n" );
        return 1;
    }
    if ( !Abc_NtkIsStrash(pNtk) )
    {
        Abc_Print( -1, "Simulation only for AIG.\n" );
        return 1;
    }
    if ( argc < 2 ) // TODO: check if there is input pattern file
    {
        Abc_Print( -1, "Missing test pattern file.\n" );
        return 1;
    }
    if ( Abc_NtkSimAig(pNtk,argv[1]) ) // TODO: check if simulation correctly end
    {
        Abc_Print( -1, "Simulation AIG has failed.\n" );
        return 1;
    }
    return 0;

usage:
    Abc_Print( -2, "usage: lsv_sim_aig [-h] <file>\n" );
    Abc_Print( -2, "\t         do simulations for a given AIG and an input pattern\n" );
    Abc_Print( -2, "\t-h     : print the command usage\n");
    return 1;
}
ABC_NAMESPACE_IMPL_END