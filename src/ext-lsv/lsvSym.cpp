#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"

static int Lsv_CommandSymBDD(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSymSAT(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSymAll(Abc_Frame_t* pAbc, int argc, char** argv);



int Lsv_CommandSymBDD(Abc_Frame_t* pAbc, int argc, char** argv) {
	Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
	char * pInput = argv[1];
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
	
	
	return 0;
usage:
	Abc_Print(-2, "usage: lsv_sym_bdd <output_index> <input_index> <input_index>\n");
	Abc_Print(-2, "\t        for the BDD, check whether the given output pin is symmetric for the two given input pin\n");
	Abc_Print(-2, "\t-h    : print the command usage\n");
	return 1;
}
int Lsv_CommandSymSAT(Abc_Frame_t* pAbc, int argc, char** argv) {
	Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
	char * pInput = argv[1];
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
	
	
	return 0;
usage:
	Abc_Print(-2, "usage: lsv_sym_sat <output_index> <input_index> <input_index>\n");
	Abc_Print(-2, "\t        for the AIG, check whether the given output pin is symmetric for the two given input pin\n");
	Abc_Print(-2, "\t-h    : print the command usage\n");
	return 1;
}
int Lsv_CommandSymAll(Abc_Frame_t* pAbc, int argc, char** argv) {
	Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
	char * pInput = argv[1];
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
	
	
	return 0;
usage:
	Abc_Print(-2, "usage: lsv_sym_all <output_index>\n");
	Abc_Print(-2, "\t        for the AIG, find every symmetric input pin pairs for the given output pin\n");
	Abc_Print(-2, "\t-h    : print the command usage\n");
	return 1;
}