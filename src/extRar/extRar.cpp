
#include "base/io/ioAbc.h"
#include "base/abc/abc.h"
#include "base/main/mainInt.h"
#include "extRar/MySolver.h"
#include "rarMgr.h"

#include <iostream>
#include <fstream>
#include <time.h>

#include <vector>
#include <unordered_set>
#include <string>

ABC_NAMESPACE_IMPL_START

using namespace Minisat;
using namespace std;

int Ext_CommandRar(Abc_Frame_t *pAbc, int argc, char **argv);
int satRar( Abc_Frame_t *pAbc, bool flag_learn );

// register
static void
extRar_Init( Abc_Frame_t * pAbc)
{
    Cmd_CommandAdd( pAbc, "Ext", "ext_rar", Ext_CommandRar, 1);
}
static void
extRar_End( Abc_Frame_t * pAbc)
{

}

// this object should not be modified after the call to Abc_FrameAddInitializer
static Abc_FrameInitializer_t append_frame_initializer = {extRar_Init, extRar_End};

// register the initializer a constructor of a global object
// called before main (and ABC startup)
struct rar_register {
  rar_register() { Abc_FrameAddInitializer(&append_frame_initializer); }
} rar_register_;


// command rar
int Ext_CommandRar(Abc_Frame_t *pAbc, int argc, char **argv)
{

	int c;
	bool flag_learn = false;
	Extra_UtilGetoptReset();
	while ((c = Extra_UtilGetopt(argc, argv, "hl")) != EOF)
	{
		switch (c)
		{
		case 'l':
			flag_learn = true;
			break;
		default:
			goto usage;
		}
	}

    if ( argc != globalUtilOptind ) goto usage;

	// main
	satRar( pAbc, flag_learn );

	return 0;

usage:
	Abc_Print(-2, "usage: ext_rar [-hr]\n");
	Abc_Print(-2, "\t        perform aig optimization using node addition and removal\n");
	Abc_Print(-2, "\t-h    : print the command usage\n");
	Abc_Print(-2, "\t-l    : toggle additional conflict-driven learning (default: off)\n");
	return 1;
}


int satRar( Abc_Frame_t *pAbc, bool flag_learn )
{
	Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);

	// rar
	RarMgr rarMgr;
	rarMgr.fromNtk( pNtk );
	rarMgr.set( flag_learn );

	// rarMgr.findAllDom();
	// rarMgr.rarAll();
	// rarMgr.opt();

	// nar
	rarMgr.nar();

	Abc_Ntk_t* pNtkNew = rarMgr.genAig( pNtk );
	Abc_FrameReplaceCurrentNetwork( pAbc, pNtkNew );

	// rarMgr.test();

	return 0;
}
ABC_NAMESPACE_IMPL_END