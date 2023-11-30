#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"

static int Lsv_CommandSymBDD(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSymSAT(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSymAll(Abc_Frame_t* pAbc, int argc, char** argv);

void SymmetryBDD(Abc_Ntk_t* pNtk, int id_k, int id_i, int id_j){
	int ithFI, ithPI, bdd_i = -1, bdd_j = -1;
	Abc_Obj_t *pI, *pFI;
	Abc_Obj_t* pPO = Abc_NtkPo(pNtk, id_k);
	Abc_Obj_t* pPIi = Abc_NtkPi(pNtk, id_i);
	Abc_Obj_t* pPIj = Abc_NtkPi(pNtk, id_j);
	Abc_Obj_t* pRoot = Abc_ObjFanin0(pPO);
	DdManager * dd = (DdManager *)pRoot->pNtk->pManFunc;
	DdNode* ddnode = (DdNode *)pRoot->pData;
	Cudd_Ref(ddnode);
	/*
	int ithPo;
	Abc_Obj_t *pObj;
	Abc_NtkForEachObj(pNtk, pObj, ithPo){
		printf("%d: %s  ", Abc_ObjId(pObj), Abc_ObjName(pObj));
	}
	printf("PI: %d, FI: %d\n", Abc_NtkPiNum(pNtk), Abc_ObjFaninNum(pRoot));
	*/
	char** vNamesIn = (char**) Abc_NodeGetFaninNames(pRoot)->pArray;
	int ithbddtoabc[Abc_ObjFaninNum(pRoot)], ithabctobdd[Abc_NtkPiNum(pNtk)];
	Abc_NtkForEachPi(pNtk, pI, ithPI){
		ithabctobdd[ithPI] = -1;
		Abc_ObjForEachFanin(pRoot, pFI, ithFI ){
			if(strcmp(Abc_ObjName(pI), vNamesIn[ithFI]) == 0){
				ithbddtoabc[ithFI] = ithPI;
				ithabctobdd[ithPI] = ithFI;
				//printf("%s: bdd-%d/abc-%d  ", Abc_ObjName(pI), ithFI, ithPI);
			}
		}
	}
	bdd_i = ithabctobdd[id_i];
	bdd_j = ithabctobdd[id_j];
	/* Abc_ObjForEachFanin( pRoot, pI, ithFI ){//matching PI
		//Abc_Print(1, "%s: %i\n", Abc_ObjName(pI), ithFI);
		if(strcmp(Abc_ObjName(pPIi), vNamesIn[ithFI]) == 0){
			Abc_Print(1, "i match%s %d  ", vNamesIn[ithFI], ithFI);
			bdd_i = ithFI;
		}
		if(strcmp(Abc_ObjName(pPIj), vNamesIn[ithFI]) == 0){
			Abc_Print(1, "j match%s %d  ", vNamesIn[ithFI], ithFI);
			bdd_j = ithFI;
		}
	} */
	//if(bdd_i != -1)printf("dd i = %d  ", Cudd_NodeReadIndex(Cudd_ReadVars(dd, bdd_i)));
	//if(bdd_j != -1)printf("dd j = %d\n", Cudd_NodeReadIndex(Cudd_ReadVars(dd, bdd_j)));
	
	DdNode *i0j1, *i1j0;
	if(bdd_i == bdd_j){
		Abc_Print(1, "symmetric\n");
		//printf("sym by id: %d %d\n", id_i, id_j);
		return;
	}
	if(bdd_j == -1){
		//printf("asym id: %d -1\n", id_i);
		i0j1 = Cudd_Cofactor(dd, ddnode, Cudd_Not(Cudd_ReadVars(dd, bdd_i)));
		Cudd_Ref(i0j1);
		i1j0 = Cudd_Cofactor(dd, ddnode, Cudd_ReadVars(dd, bdd_i));
		Cudd_Ref(i1j0);
	}
	else if(bdd_i == -1){
		//printf("asym id: -1 %d\n", id_j);
		i0j1 = Cudd_Cofactor(dd, ddnode, Cudd_ReadVars(dd, bdd_j));
		Cudd_Ref(i0j1);
		i1j0 = Cudd_Cofactor(dd, ddnode, Cudd_Not(Cudd_ReadVars(dd, bdd_j)));
		Cudd_Ref(i1j0);
	}
	else{
		i0j1 = Cudd_Cofactor(dd, ddnode, Cudd_bddAnd(dd, Cudd_Not(Cudd_ReadVars(dd, bdd_i)), Cudd_ReadVars(dd, bdd_j)));
		Cudd_Ref(i0j1);
		i1j0 = Cudd_Cofactor(dd, ddnode, Cudd_bddAnd(dd, Cudd_ReadVars(dd, bdd_i), Cudd_Not(Cudd_ReadVars(dd, bdd_j))));
		Cudd_Ref(i1j0);
	}
	if(i0j1 == i1j0){
		Abc_Print(1, "symmetric\n");
		//printf("sym by const: %d %d\n", id_i, id_j);
	}
	else{
		int asympat[Abc_NtkPiNum(pNtk)] = {};
		char *cubev = new char[Abc_ObjFaninNum(pRoot)];
		
		Abc_Print(1, "asymmetric\n");
		//Cudd_PrintMinterm(dd, Cudd_bddXor(dd, i0j1, i1j0));
		DdNode* XOR = Cudd_bddXor(dd, i0j1, i1j0);
		Cudd_Ref(XOR);
		Cudd_bddPickOneCube(dd, XOR, cubev);
		Cudd_RecursiveDeref(dd, XOR);
		for(int i=0; i<Abc_ObjFaninNum(pRoot); i++){
			if((int)cubev[i] != 2){
				asympat[ithbddtoabc[i]] = (int)cubev[i];
			}
			//printf("bdd-%d abc-%d %d\n", i, ithbddtoabc[i], (int)cubev[i]);
		}
		asympat[id_i] = 1;
		asympat[id_j] = 0;
		for(int i=0; i<Abc_NtkPiNum(pNtk); i++)Abc_Print(1, "%d", asympat[i]);
		Abc_Print(1, "\n");
		asympat[id_i] = 0;
		asympat[id_j] = 1;
		for(int i=0; i<Abc_NtkPiNum(pNtk); i++)Abc_Print(1, "%d", asympat[i]);
		Abc_Print(1, "\n");
		/* Ddnode* cube = Cudd_ReadOne(dd);
		for (int i = 0; i < n; ++i) {
			Ddnode* var = ReadVars(dd, i);
			Cudd_Ref(var);
			Ddnode* new_cube = Cudd_bddAnd(dd, cube, var);
			Cudd_Ref(new_cube);
			Cudd_RecursiveDeref(dd, cube);
			Cudd_RecursiveDeref(dd, var);
			cube = new_cube;
			Cudd_NodeReadIndex(
		} */
	}
	Cudd_RecursiveDeref(dd, i0j1);
	Cudd_RecursiveDeref(dd, i1j0);
	Cudd_RecursiveDeref(dd, ddnode);
	
	return;
}
int Lsv_CommandSymBDD(Abc_Frame_t* pAbc, int argc, char** argv) {
	int id_k, id_i, id_j;
	
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
	if (!pNtk) {
		Abc_Print(-1, "Empty network.\n");
		return 1;
	}
	if ( !Abc_NtkIsBddLogic( pNtk ) ){
		Abc_Print( -1, "This command is only applicable to logic BDD networks (run \"collapse\").\n" );
		return 1;
	}
	if (argc != 4){
		Abc_Print(-1, "Incorrect input format.\n");
		goto usage;
	}
	sscanf(argv[1], "%d", &id_k);
	sscanf(argv[2], "%d", &id_i);
	sscanf(argv[3], "%d", &id_j);
	if (id_k >= Abc_NtkPoNum(pNtk)) {
		Abc_Print(-1, "Output pin out of scope.\n");
		return 1;
	}
	if (id_i >= Abc_NtkPiNum(pNtk) || id_j >= Abc_NtkPiNum(pNtk)) {
		Abc_Print(-1, "Input pin out of scope.\n");
		return 1;
	}
	SymmetryBDD(pNtk, id_k, id_i, id_j);
	return 0;
usage:
	Abc_Print(-2, "usage: lsv_sym_bdd <output_index> <input_index> <input_index>\n");
	Abc_Print(-2, "\t        for the BDD, check whether the given output pin is symmetric for the two given input pin\n");
	Abc_Print(-2, "\t-h    : print the command usage\n");
	return 1;
}
int Lsv_CommandSymSAT(Abc_Frame_t* pAbc, int argc, char** argv) {
	int ex, id_k, id_i, id_j;
	
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
	if (!pNtk) {
		Abc_Print(-1, "Empty network.\n");
		return 1;
	}
	if ( !Abc_NtkIsStrash( pNtk ) ){
		Abc_Print( -1, "This command works only for AIGs (run \"strash\").\n" );
		return 1;
	}
	if (argc != 4){
		Abc_Print(-1, "Incorrect input format.\n");
		goto usage;
	}
	sscanf(argv[1], "%d", &id_k);
	sscanf(argv[2], "%d", &id_i);
	sscanf(argv[3], "%d", &id_j);
	if (id_k >= Abc_NtkPoNum(pNtk)) {
		Abc_Print(-1, "Output pin out of scope.\n");
		return 1;
	}
	if (id_i >= Abc_NtkPiNum(pNtk) || id_j >= Abc_NtkPiNum(pNtk)) {
		Abc_Print(-1, "Input pin out of scope.\n");
		return 1;
	}
	if (id_i == id_j) {
		Abc_Print(-1, "Same input pin.\n");
		return 1;
	}
	
	return 0;
usage:
	Abc_Print(-2, "usage: lsv_sym_sat <output_index> <input_index> <input_index>\n");
	Abc_Print(-2, "\t        for the AIG, check whether the given output pin is symmetric for the two given input pin\n");
	Abc_Print(-2, "\t-h    : print the command usage\n");
	return 1;
}
int Lsv_CommandSymAll(Abc_Frame_t* pAbc, int argc, char** argv) {
	int ex, id_k;
	
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
	if (!pNtk) {
		Abc_Print(-1, "Empty network.\n");
		return 1;
	}
	if ( !Abc_NtkIsStrash( pNtk ) ){
		Abc_Print( -1, "This command works only for AIGs (run \"strash\").\n" );
		return 1;
	}
	if (argc != 2){
		Abc_Print(-1, "Incorrect input format.\n");
		goto usage;
	}
	sscanf(argv[1], "%d", &id_k);
	if (id_k > Abc_NtkPoNum(pNtk)) {
		Abc_Print(-1, "Output pin out of scope.\n");
		return 1;
	}
	
	
	return 0;
usage:
	Abc_Print(-2, "usage: lsv_sym_all <output_index>\n");
	Abc_Print(-2, "\t        for the AIG, find every symmetric input pin pairs for the given output pin\n");
	Abc_Print(-2, "\t-h    : print the command usage\n");
	return 1;
}