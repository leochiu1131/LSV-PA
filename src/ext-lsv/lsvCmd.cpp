#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/cnf/cnf.h"
#include "aig/aig/aig.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_set>
#include <stdlib.h>

extern "C"{
    Aig_Man_t* Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
}

/* -> Some hints about PA#1

1. You are not allowed to call the function "Cudd_Eval"
2. The variable order is the same as defined in the BLIF file. You can use "Abc_NtkForEachPi" to traverse all PIs.
3. Please be aware that the variable order may be changed after transforming the network into BDD. 
Hint: Since the "show_bdd" command shows the variable name of each decision level, 
you may look into the source code of that command to find some information.

1. To find the BDD node associated with each PO, use the codes below.

Abc_NtkForEachPo(pNtk, pPo, ithPo) {
    Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo); 
    assert( Abc_NtkIsBddLogic(pRoot->pNtk) );
    DdManager * dd = (DdManager *)pRoot->pNtk->pManFunc;  
    DdNode* ddnode = (DdNode *)pRoot->pData;
}


2. To find the variable order of the BDD, you may use the following codes to find the variable name array.

char** vNamesIn = (char**) Abc_NodeGetFaninNames(pRoot)->pArray;

*/

static int Lsv_CommandPrintNodes      ( Abc_Frame_t *pAbc, int argc, char **argv );
static int Abc_CommandHello           ( Abc_Frame_t *pAbc, int argc, char **argv );
static int Abc_CommandLSVSimBDD       ( Abc_Frame_t *pAbc, int argc, char **argv );
static int Abc_CommandLSVPallSimAIG   ( Abc_Frame_t *pAbc, int argc, char **argv );
// Added function for PA#2
static int Abc_CommandLSVSymBDD   	  ( Abc_Frame_t *pAbc, int argc, char **argv );
static int Abc_CommandLSVSymSAT   	  ( Abc_Frame_t *pAbc, int argc, char **argv );

void displayAIGMan(Aig_Man_t *pMan);
void displayCNFDat(Cnf_Dat_t *pCnf);
void displaySATSolver(sat_solver_t *ss);

void conductSimulation(Abc_Ntk_t *pNtk, Abc_Obj_t *pObj, bool simulaitonAct[], unsigned **simulationarr, int simArrWid);

unsigned int **simulationArr;
bool *simulationAct;

void init(Abc_Frame_t* pAbc) {
    Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
    /* LSV PA#1 */
    Cmd_CommandAdd( pAbc, "Printing",     "hello",         Abc_CommandHello,            0 );
    Cmd_CommandAdd( pAbc, "Various",      "lsv_sim_bdd",   Abc_CommandLSVSimBDD,        0 );
    Cmd_CommandAdd( pAbc, "Various",      "lsv_sim_aig",   Abc_CommandLSVPallSimAIG,    0 );
    /* LSV PA#2 */
    Cmd_CommandAdd( pAbc, "Various",      "lsv_sym_bdd",   Abc_CommandLSVSymBDD,    	0 );
    Cmd_CommandAdd( pAbc, "Various",      "lsv_sym_sat",   Abc_CommandLSVSymSAT,    	0 );
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

// PA#2 1.Symmetry Checking with BDD
static int Abc_CommandLSVSymBDD ( Abc_Frame_t *pAbc, int argc, char **argv ){
	
    Abc_Ntk_t * pNtk = Abc_FrameReadNtk(pAbc);
    int c;

    pNtk = Abc_FrameReadNtk(pAbc);
	
	// argument parsing
	if(argc != 4){
		Abc_Print( -2, "usage: lsv_sym_bdd <k> <i> <j> \n" );
		Abc_Print( -2, "\t        Determine if the output pin yk is symmetric in xi and xj.\n" );
		return 1;
	}
	int inK, inI, inJ;
	inK = atoi(argv[1]);
	inI = atoi(argv[2]);
	inJ = atoi(argv[3]);
	
	// printf("This networks has Pi/Po = %d/%d\n", Abc_NtkPiNum(pNtk), Abc_NtkPoNum(pNtk));

    Extra_UtilGetoptReset();
    while ( ( c = Extra_UtilGetopt( argc, argv, "h" ) ) != EOF )
    {
        switch ( c )
        {
			case 'h':
			default:{
				Abc_Print( -2, "usage: lsv_sym_bdd <k> <i> <j> \n" );
				Abc_Print( -2, "\t        Determine if the BDD's output pin yk is symmetric in xi and xj.\n" );
				return 1;
			}
				
        }
    }
	if ( pNtk == NULL )
    {
        Abc_Print( -1, "Empty network.\n" );
        return 1;
    }

	// Start deriving Cofactor of ~XiXj & Xi~Xj
	//
	Abc_Obj_t *pPo;
	pPo = Abc_NtkPo(pNtk, inK);
	// printf("%s: ",Abc_ObjName(pPo) );
	// printf("Po node fanin/fanout : %d / %d\n",Abc_ObjFaninNum(pPo) , Abc_ObjFanoutNum(pPo));

	Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo);
	assert( Abc_NtkIsBddLogic(pRoot->pNtk) );

	DdManager *manager = (DdManager *)pRoot->pNtk->pManFunc;  
	// DdNode* ddnode = (DdNode *)pRoot->pData;

	DdNode *PNCofact =(DdNode *)pRoot->pData;
	DdNode *NPCofact = (DdNode *)pRoot->pData;

	Vec_Int_t *pRootIdxArr;
	pRootIdxArr = Abc_ObjFaninVec(pRoot);
	for(int i = 0; i < pRootIdxArr->nSize; ++i){
		// printf("*%d*", pRootIdxArr->pArray[i]);
		// char inputChar = argv[1][pRootIdxArr->pArray[i] - 1];
		int currentIndex = pRootIdxArr->pArray[i];
		// std::cout << "input( " << i << "):" <<  currentIndex << std::endl;

		if(currentIndex == (inI + 1)){
			Cudd_RecursiveDeref(manager, PNCofact);
			PNCofact = Cudd_Cofactor(manager, PNCofact, Cudd_bddIthVar(manager, i));
			Cudd_Ref(PNCofact);

			Cudd_RecursiveDeref(manager, NPCofact);
			NPCofact = Cudd_Cofactor(manager, NPCofact, Cudd_Not(Cudd_bddIthVar(manager, i)));
			Cudd_Ref(NPCofact);
		} else if(currentIndex == (inJ + 1)){
			Cudd_RecursiveDeref(manager, PNCofact);
			PNCofact = Cudd_Cofactor(manager, PNCofact, Cudd_Not(Cudd_bddIthVar(manager, i)));
			Cudd_Ref(PNCofact);

			Cudd_RecursiveDeref(manager, NPCofact);
			NPCofact = Cudd_Cofactor(manager, NPCofact, Cudd_bddIthVar(manager, i));
			Cudd_Ref(NPCofact);

		}
	}

	// Check if two cofactor are equal, if yes, output and return
	if(Cudd_EquivDC(manager, PNCofact, NPCofact, Cudd_ReadZero(manager))){
		printf("symmetric\n");
		return 0;
	}else{
		printf("asymmetric\n");
	}

	// Perform Exclusive or on two cofactored BDDs
	DdNode *XorNode = Cudd_bddXor(manager, PNCofact, NPCofact);
	Cudd_Ref(XorNode);
	
	// Cudd_PrintDebug(manager, XorNode, pRootIdxArr->nSize, 3);
	// std::cout << "new 1: " << Cudd_CountPathsToNonZero(XorNode) << std::endl;
	// Cudd_PrintMinterm(manager, XorNode);
	
	// Get some cubes in BDD
	int *cube;
	CUDD_VALUE_TYPE value;
	
	Cudd_FirstCube(manager, XorNode, &cube, &value);
	
	int numPis = Abc_NtkPiNum(pNtk);

	// Two Counter examples are the size of Primary Inputs
	char ctrExample1[numPis+1] = "";
	char ctrExample2[numPis+1] = "";
	memset(ctrExample1, '0', numPis);
	memset(ctrExample2, '0', numPis);

	// making the two counter examples by extracting the FirstCube informaiton
	for(int i = 0; i < pRootIdxArr->nSize; ++i){

		int BddIndex = (pRootIdxArr->pArray[i] - 1);
		// std::cout << "input( " << i << "):" <<  BddIndex << std::endl;
		
		if(cube[i] == 1){
			// includes 1 and 2(done't care)
			ctrExample1[BddIndex] = ctrExample2[BddIndex] = '1';
		}else{
			ctrExample1[BddIndex] = ctrExample2[BddIndex] = '0';
		}
		// boolean value @<i> and @<j> shall be inverted 0/1
		if(BddIndex == inI){
			ctrExample1[BddIndex] = '1';
			ctrExample2[BddIndex] = '0';
		}else if(BddIndex == inJ){
			ctrExample1[BddIndex] = '0';
			ctrExample2[BddIndex] = '1';
		}
	}

	printf("%s\n", ctrExample1);
	printf("%s\n", ctrExample2);

	return 0;
}

// PA#2 2.Symmetry Checking with SAT
static int Abc_CommandLSVSymSAT( Abc_Frame_t *pAbc, int argc, char **argv ){
    Abc_Ntk_t * pNtk = Abc_FrameReadNtk(pAbc);
    int c;

	// argument parsing
	if(argc != 4){
		Abc_Print( -2, "usage: lsv_sym_sat <k> <i> <j> \n" );
		Abc_Print( -2, "\t        Determine if the AIG's output pin yk is symmetric in xi and xj using SAT solver.\n" );
		return 1;
	}
	int inK, inI, inJ;
	inK = atoi(argv[1]);
	inI = atoi(argv[2]);
	inJ = atoi(argv[3]);

    Extra_UtilGetoptReset();
    while ( ( c = Extra_UtilGetopt( argc, argv, "h" ) ) != EOF )
    {
        switch ( c )
        {
			case 'h':{
				Abc_Print( -2, "usage: lsv_sym_sat <k> <i> <j> \n" );
				Abc_Print( -2, "\t        Determine if the AIG's output pin yk is symmetric in xi and xj using SAT solver.\n" );
				return 1;
				break;
			}
			default:{
				Abc_Print( -2, "usage: lsv_sym_sat <k> <i> <j> \n" );
				Abc_Print( -2, "\t        Determine if the AIG's output pin yk is symmetric in xi and xj using SAT solver.\n" );
				return 1;
			}
        }
    }

    if ( pNtk == NULL )
    {
        Abc_Print( -1, "Empty network.\n" );
        return 1;
    }

	// printf("Original Ntk: \n");
	// Lsv_NtkPrintNodes(pNtk);

	Abc_Obj_t *pCo = Abc_NtkCo(pNtk, inK);
	//Extract the Cone of Yk
	Abc_Ntk_t *pNtk_cone = Abc_NtkCreateCone(pNtk, Abc_ObjFanin0(pCo), Abc_ObjName(pCo), 1);
	// derives an equivalent Aig_Man_t (global AIG circuit) from an Abc_Ntk_t network.
	Aig_Man_t *pMan = Abc_NtkToDar(pNtk_cone, 0, 0);
	// displayAIGMan(pMan);
	
	// These stores the AIG I/O indexes
	std::vector<int> cinIdxvec; 
	int cinIdxi = inI + 1, cinIdxj = inJ + 1;
	int coutIdx;
	int twinDistance;
	
	std::vector <Aig_Obj_t *>cinObjvec;
	std::vector <Aig_Obj_t *>cinObjIJ;
	Aig_Obj_t *coutObj;

	
	// Extract Ci indexes
	Aig_Obj_t *ppIndexes; int counterIdx;
	Aig_ManForEachCi(pMan, ppIndexes, counterIdx){
		int canIndex = Aig_ObjId(ppIndexes);
		if(!(canIndex == cinIdxi || canIndex == cinIdxj)){
			cinIdxvec.push_back(canIndex);
			cinObjvec.push_back(ppIndexes);
		}else{
			cinObjIJ.push_back(ppIndexes);
		}
	}
	
	//Extract Co index
	coutObj = ((Aig_Obj_t *)Vec_PtrEntry(pMan->vCos, 0));
	coutIdx = Aig_ObjId(coutObj);


	// Display indexes
	// std::cout << "These are Cins: [" << cinIdxi << "] [" << cinIdxj << "] ";
	// for(std::vector<int>::iterator it = cinIdxvec.begin(); it != cinIdxvec.end(); it++){
	// 	std::cout << *it << " ";
	// }
	// std::cout << std::endl << "This is Cout: " << coutIdx << std::endl;

	// Initialise SAT Solver
	sat_solver *sv = sat_solver_new();
	// sv->fPrintClause = 1;
	// sv->verbosity = 2;
	// sv->fVerbose = 1;
	
	// obtain the corresponding CNF formula CA, which depends on variables V1,...,Vn
	Cnf_Dat_t *cnfdat1 = Cnf_Derive(pMan, Aig_ManCoNum(pMan));

	// Readjust the indexes that map to PI/ SymPIs and Po
	std::vector<int> cinAllIdxVec_Cnf;

	std::vector<int> cinidxvec_Cnf;
	int cinIdxi_Cnf, cinIdxj_Cnf;
	int coutIdx_Cnf;

	Aig_ManForEachCi(pMan, ppIndexes, counterIdx){
		cinAllIdxVec_Cnf.push_back(cnfdat1->pVarNums[ppIndexes->Id]);
	}

	// std::cout << "Full PI order: ";
	for(std::vector<int>::iterator it= cinAllIdxVec_Cnf.begin(); it != cinAllIdxVec_Cnf.end(); ++it){
		// std::cout << *it << " ";
	}
	// std::cout << std::endl;

	for(int i = 0; i < cinObjvec.size(); ++i){
		Aig_Obj_t *payload = cinObjvec[i];
		cinidxvec_Cnf.push_back(cnfdat1->pVarNums[payload->Id]);
		// std::cout << i << "]" << payload->Id << " -> " << cnfdat1->pVarNums[payload->Id] << std::endl;
	}
	cinIdxi_Cnf = cnfdat1->pVarNums[cinObjIJ[0]->Id];
	cinIdxj_Cnf = cnfdat1->pVarNums[cinObjIJ[1]->Id];
	coutIdx_Cnf = cnfdat1->pVarNums[coutObj->Id];
	// std::cout << "SYM1: " << cinObjIJ[0]->Id << " -> " << cinIdxi_Cnf << std::endl;
	// std::cout << "SYM2: " << cinObjIJ[1]->Id << " -> " << cinIdxj_Cnf << std::endl;
	// std::cout << "OUT: " << coutObj->Id << " -> " << coutIdx_Cnf << std::endl;
	

	// add the CNF into the SAT solver.
	// displayCNFDat(cnfdat1);
	Cnf_DataWriteIntoSolverInt(sv, cnfdat1, 1, 0);

	// create another CNF formula CB that depends on different input variables V(n+1),...,V(2n), add to solver.
	Cnf_Dat_t *cnfdat2 = Cnf_Derive(pMan, Aig_ManCoNum(pMan));
	twinDistance = cnfdat1->nVars;
	Cnf_DataLift(cnfdat2, twinDistance);
	// displayCNFDat(cnfdat2);
	Cnf_DataWriteIntoSolverInt(sv, cnfdat2, 1, 0);

	// This is the enable variable that is required
	int envar = sat_solver_addvar(sv);
	lit Lit[1];
	Lit[0] = toLitCond(envar, 0);
	int Cid = sat_solver_addclause(sv, Lit, Lit + 1);
	assert(Cid);
	// std::cout << "Added control enable var: " << envar << std::endl;

	for (int i = 0; i < cinidxvec_Cnf.size(); i++){
		sat_solver_add_buffer_enable(sv, cinidxvec_Cnf[i], cinidxvec_Cnf[i]+twinDistance, envar, 0);
	}
	sat_solver_add_buffer_enable(sv, cinIdxi_Cnf, cinIdxj_Cnf+twinDistance, envar, 0);
	sat_solver_add_buffer_enable(sv, cinIdxj_Cnf, cinIdxi_Cnf+twinDistance, envar, 0);

	sat_solver_add_buffer_enable(sv, coutIdx_Cnf, coutIdx_Cnf+twinDistance, envar, 1);

	int solveAns = sat_solver_solve(sv, NULL, NULL, 0, 0, 0, 0);
	if(solveAns == l_False){
		printf("symmetric\n");
		return 0;
	}

	// Start processing asymmetric scenarios
	printf("asymmetric\n");
	
	// Two Counter examples are the size of Primary Inputs
	int numPis = cinAllIdxVec_Cnf.size();
	char ctrExample1[numPis+1] = "";
	char ctrExample2[numPis+1] = "";
	memset(ctrExample1, '0', numPis);
	memset(ctrExample2, '0', numPis);

	
	// sat_solver_var_value
	// for(std::vector<int>::iterator it= cinAllIdxVec_Cnf.begin(); it != cinAllIdxVec_Cnf.end(); ++it){

	// 	std::cout << (*it) << ": " << sat_solver_var_value(sv, *it) << std::endl;
	// }

	for(int i = 0; i < cinAllIdxVec_Cnf.size(); ++i){
		if(sat_solver_var_value(sv, cinAllIdxVec_Cnf[i]) == 0){
			ctrExample1[i] = ctrExample2[i] = '0';
		}else{
			ctrExample1[i] = ctrExample2[i] = '1';
		}
	}
	ctrExample1[inI] = '0';
	ctrExample2[inI] = '1';
	ctrExample1[inJ] = '1';
	ctrExample2[inJ] = '0';

	printf("%s\n", ctrExample1);
	printf("%s\n", ctrExample2);

	return 0;
}

void displayAIGMan(Aig_Man_t *pMan) {
    int i;
    Aig_Obj_t *pObj;

    printf("Number of Nodes: %d\n", Aig_ManObjNum(pMan));
	printf("Ci/Co: %d/%d\n", Aig_ManCiNum(pMan), Aig_ManCoNum(pMan));

    Aig_ManForEachObj(pMan, pObj, i) {
        // Print information for each node, e.g., pObj->Type, fanins, etc.
        printf("Node-%d: ID = %d\n", i, pObj->Id);
        printf("PI: %d, PO: %d\n", Aig_ObjIsCi(pObj), Aig_ObjIsCo(pObj));
        if ( Aig_ObjFanin0(pObj) ) {
            printf("Fanin0: ID = %d\n", Aig_ObjFanin0(pObj)->Id);
        }
        if ( Aig_ObjFanin1(pObj) ) {
            printf("Fanin1: ID = %d\n", Aig_ObjFanin1(pObj)->Id);
        }
        printf("\n");
    }
}

void displayCNFDat(Cnf_Dat_t *pCnf) {
	printf("%d nVars, %d nLiterals, %d clauses\n", pCnf->nVars, pCnf->nLiterals, pCnf->nClauses);
	printf("pVarNums:");
	for(int i = 0; i < pCnf->nVars; ++i){
		printf("%d ", pCnf->pVarNums[i]);
	}
	printf("\n");
    for ( int i = 0; i < pCnf->nClauses; i++ ) {
        int *pLit, *pStop;
        for ( pLit = pCnf->pClauses[i], pStop = pCnf->pClauses[i + 1]; pLit < pStop; pLit++ ) {
            printf("%d ", ( *pLit & 1 ) ? -( *pLit >> 1 ) : ( *pLit >> 1 ));
        }
        printf("\n");
    }
	for (int i=0; i < pCnf->nClauses; ++i){

	}
}

void displaySATSolver(sat_solver_t *ss){
	printf("%d nvars, %d nClauses", sat_solver_nvars(ss), sat_solver_nclauses(ss));

	Sat_Mem_t *pMem = &(ss->Mem);
	clause *c;
	int i,k;
	Sat_MemForEachClause(pMem, c, i, k){
		clause_print_(c);
	}
	Sat_MemForEachClause2(pMem, c, i, k){
		clause_print_(c);
	}
	// for(int i = 0; i < sat_solver_nclauses(ss); ++i){
	// 	for(int j = 0; j < ss->wlists[i].size; ++j){
	// 		std::cout << ss->wlists->ptr[j];
	// 	}
	// 	std::cout << std::endl;
	// }
	
}


static int Abc_CommandHello ( Abc_Frame_t *pAbc, int argc, char **argv ){
    Abc_Ntk_t * pNtk = Abc_FrameReadNtk(pAbc);
    int c;

	Abc_Obj_t *thing;
	int i;

    pNtk = Abc_FrameReadNtk(pAbc);

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
    if ( !Abc_NtkIsStrash(pNtk))
    {
        Abc_Print( -1, "This command must be used under AIG.\n" );
        return 1;
    }
	
	Abc_NtkForEachObj(pNtk, thing, i){
		printf("Object Id: %5d\t", thing->Id);
		printf("Object layer = %3d\n", thing->Level);
	}
    return 0;

usage:
    Abc_Print( -2, "usage: print_stats [-fbdltmpgscuh]\n" );
    Abc_Print( -2, "\t        prints the network internal AIG structure\n" );
    Abc_Print( -2, "\t-h    : print the command usage\n");
    return 1;
}

static int Abc_CommandLSVSimBDD ( Abc_Frame_t *pAbc, int argc, char **argv )
{
    Abc_Ntk_t * pNtk = Abc_FrameReadNtk(pAbc);
    int c;

    pNtk = Abc_FrameReadNtk(pAbc);

	// printf("Printing args(%d): ", argc);
	// for(int i = 0; i < argc; ++i){
	// 	printf("%s ", argv[i]);
	// }
	// printf("\n");
	// printf("This networks has Pi/Po = %d/%d\n", Abc_NtkPiNum(pNtk), Abc_NtkPoNum(pNtk));

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

	// Abc_Obj_t *pPi;
	// int ithPi;

	Abc_Obj_t *pPo;
	int ithPo;
	Abc_NtkForEachPo(pNtk, pPo, ithPo){
		printf("%s: ",Abc_ObjName(pPo) );
		// printf("Po node fanin/fanout : %d / %d\n",Abc_ObjFaninNum(pPo) , Abc_ObjFanoutNum(pPo));

		Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo);
		assert( Abc_NtkIsBddLogic(pRoot->pNtk) );

		// Abc_Obj_t *pFanin;
		// int ithFanin;
		// Abc_ObjForEachFanin(pRoot, pFanin, ithFanin){
		// 	printf("Fanin: %d\n",pFanin->Id);
		// 	char** vNamesIn = (char**) Abc_NodeGetFaninNames(pRoot)->pArray;
		// 	for(int i  = 0; i < Abc_NodeGetFaninNames(pRoot)->nSize; ++i){
		// 		printf("%s ", vNamesIn[i]);
		// 	}
		// 	printf("\n");
		// }

		// char** vNamesIn = (char**) Abc_NodeGetFaninNames(pRoot)->pArray;
		// for(int i  = 0; i < Abc_NodeGetFaninNames(pRoot)->nSize; ++i){
		// 	printf("%s ", vNamesIn[i]);
		// }
		// printf("\n");

		DdManager *manager = (DdManager *)pRoot->pNtk->pManFunc;  
		DdNode* ddnode = (DdNode *)pRoot->pData;

		DdNode* workingDD = ddnode;

		Vec_Int_t *pRootIdxArr = Abc_ObjFaninVec(pRoot);
		// printf("[");
		for(int i = 0; i < pRootIdxArr->nSize; ++i){
			// printf("*%d*", pRootIdxArr->pArray[i]);
			char inputChar = argv[1][pRootIdxArr->pArray[i] - 1];
			if(inputChar == '1'){
				// printf("Do: %d= 1, ",pRootIdxArr->pArray[i] - 1);
				workingDD = Cudd_Cofactor(manager, workingDD, Cudd_bddIthVar(manager, i));

			}else if(inputChar == '0'){
				// printf("Do: %d= 0, ",pRootIdxArr->pArray[i] - 1);
				workingDD = Cudd_Cofactor(manager, workingDD, Cudd_Not(Cudd_bddIthVar(manager, i)));
			}else{
				printf("Strange Things happened!\n");
			}
			
		}

		// printf("]");
		
		if(!Cudd_IsNonConstant(workingDD)){
			if(workingDD == Cudd_ReadLogicZero(manager)) printf("0\n");
			else if (workingDD == Cudd_ReadOne(manager)) printf("1\n");
			else printf("Fail! not 1 and not 0\n");
		}else{
			printf("Fail, not constant!\n");
		}

	}

    return 0;

usage:
    Abc_Print( -2, "usage: lsv_sim_bdd [-h] <input_pattern> \n" );
    Abc_Print( -2, "\t        simulations for a given BDD and an input pattern\n" );
    Abc_Print( -2, "\t-h    : print the command usage\n");
    return 1;
}

static int Abc_CommandLSVPallSimAIG       ( Abc_Frame_t *pAbc, int argc, char **argv )
{
    Abc_Ntk_t * pNtk = Abc_FrameReadNtk(pAbc);
    int c;

    pNtk = Abc_FrameReadNtk(pAbc);

    Extra_UtilGetoptReset();
    while ( ( c = Extra_UtilGetopt( argc, argv, "h" ) ) != EOF )
    {
        switch ( c )
        {
			case 'h':{
				Abc_Print( -2, "usage: lsv_sim_bdd [-h] <input_pattern_file> \n" );
				Abc_Print( -2, "\t        32-bit parallel simulations for a given AIG and some input patterns\n" );
				Abc_Print( -2, "\t-h    : print the command usage\n");
				return 1;
				break;
			}
			default:{
				Abc_Print( -2, "usage: lsv_sim_bdd [-h] <input_pattern_file> \n" );
				Abc_Print( -2, "\t        32-bit parallel simulations for a given AIG and some input patterns\n" );
				Abc_Print( -2, "\t-h    : print the command usage\n");
				return 1;
			}
        }
    }

    if ( pNtk == NULL )
    {
        Abc_Print( -1, "Empty network.\n" );
        return 1;
    }

	// Start writing from below > <

	assert(argc == 2);


	std::ifstream ifs;
	ifs.open(argv[1]);
	std::string str;

	bool initialized = false;
	int inputVars = -1;
	int stimulusSize = 0;
	std::vector<std::vector<unsigned int>> inputs;
	
	while(!ifs.eof()){
		ifs >> str;
		if(!initialized){
			initialized = true;
			inputVars = str.length();
			for(int i = 0; i < inputVars; ++i){
				inputs.push_back(std::vector<unsigned>());
			}
		}
		if((stimulusSize%32) == 0){
			for (int i = 0; i < inputVars; i++){
				inputs[i].push_back(((unsigned)0));
			}

		}
		unsigned filter = ((unsigned) 1) << (stimulusSize % 32);
		// std::cout << stimulusSize << "] " << str;
		// printf("Filter = %x\n", filter);
		for(int i = 0; i < inputVars; ++i){
			if(str[i] == '1'){
				inputs[i][stimulusSize/32] |= filter;
			}
		}
		// for(int i = 0; i < inputVars; ++i){
		// 	for(int j = 0; j < inputs[i].size(); ++j){
		// 		printf("%x, ", inputs[i][j]);
		// 	}		
		// 	printf("\n");
		// }

		stimulusSize++;
	}

	// printf("inputVars = %d, Ssize = %d\n", inputVars, stimulusSize);
	// for(int i = 0; i < inputVars; ++i){
	// 	for(int j = 0; j < inputs[i].size(); ++j){
	// 		printf("%x, ", inputs[i][j]);
	// 	}		
	// 	printf("\n");
	// }

	int simArrLen = Vec_PtrSize((pNtk)->vObjs);
	int simArrWid = (stimulusSize % 32)? (stimulusSize/32 + 1): stimulusSize/32;
	
	// Arrays to record the simulation values and initialization 

	simulationArr = new unsigned int *[simArrLen];
	for(int i = 0; i < simArrLen; ++i){
		simulationArr[i] = new unsigned int [simArrWid];
	}

	// bool simulationAct[simArrLen];
	simulationAct = new bool [simArrLen];
	for(int i = 0; i < simArrLen; ++i){
		simulationAct[i] = false;
	}
	
	// Initialize the Obj #1: constant nodes
	Abc_Obj_t *pConst = Abc_NtkObj(pNtk, 0);
	if(Abc_ObjType(pConst) == ABC_OBJ_CONST1){
		simulationAct[0] = true;
		for(int i  = 0; i < simArrWid; ++i){
			simulationArr[0][i] = (unsigned)0xffffffff;
		}
	}

	// Initialize the Pis
	Abc_Obj_t *pInitPis;
	int ithInitPis;
	Abc_NtkForEachPi(pNtk, pInitPis, ithInitPis){
		// std::cout << "ithInitPis + 1 = " <<  ithInitPis + 1 << "equal TRUE\n";
		simulationAct[ithInitPis + 1] = true;
		for(int i = 0; i < simArrWid; ++i){
			simulationArr[ithInitPis + 1][i] = inputs[ithInitPis][i];
		}
	}

	// // Printing all initialized stuff;
	// printf("Printing all initialized stuff:\n");
	// for(int i = 0; i < simArrLen; ++i){
	// 	if(simulationAct[i]){
	// 		printf("Id = %02d]:", i);
	// 		for(int j = 0; j < simArrWid; ++j){
	// 			printf("%x ", simulationArr[i][j]);
	// 		}
	// 		std::cout << std::endl;
	// 	}
	// }
	Abc_Obj_t *pInternal;
	int ithInternal;
	Abc_NtkForEachNode(pNtk, pInternal, ithInternal){
		int nodeId = (int)Abc_ObjId(pInternal);
		if(simulationAct[nodeId] == false){
			conductSimulation(pNtk, pInternal, simulationAct, simulationArr, simArrWid);
		}
	}

	// Printing all initialized stuff;
	// printf("This is final printing:\n");
	// for(int i = 0; i < simArrLen; ++i){
	// 	if(simulationAct[i]){
	// 		printf("Id = %02d]:", i);
	// 		for(int j = 0; j < simArrWid; ++j){
	// 			printf("%x ", simulationArr[i][j]);
	// 		}
	// 		printf("\n");
	// 	}
	// }

	//Output Answer:
	Abc_Obj_t *pPo;
	int ithPo;
	Abc_NtkForEachPo(pNtk, pPo, ithPo){
		unsigned representNodeId = Abc_ObjFanin0(pPo)->Id;
		
		printf("%s: ",Abc_ObjName(pPo) );
		int bucket, noElement;
		unsigned filter;
		for(int i = 0; i < stimulusSize; ++i){
			bucket = i / 32;
			noElement = i % 32;
			filter = ((unsigned)1) << (noElement);

			unsigned thingy = simulationArr[representNodeId][bucket] & filter;

			if(thingy != 0){
				if(Abc_ObjFaninC0(pPo) == 0) printf("1");
				else printf("0");
			}else{
				if(Abc_ObjFaninC0(pPo) == 0) printf("0");
				else printf("1");

			}
			
		}
		// for(int j = 0; j < simArrWid; ++j){
		// 	printf("%x ", simulationArr[representNodeId][j]);
		// }
		printf("\n");
	}

	// Abc_Obj_t *pObj;
	// int ithObj;
	// Abc_NtkForEachNode(pNtk, pObj, ithObj){
	// 	std::cout << "[" << ithObj << "]";
	// 	printf("%u %u %d", Abc_ObjType(pObj), Abc_ObjId(pObj), Abc_ObjLevel(pObj));
	// 	printf("Left: |%d|%u(%u), |%d|Right: %u(%u)\n", Abc_ObjFaninC0(pObj), Abc_ObjFanin0(pObj)->Id, Abc_ObjFanin0(pObj)->Type, 
	// 	Abc_ObjFaninC1(pObj), Abc_ObjFanin1(pObj)->Id, Abc_ObjFanin1(pObj)->Type);
	// }

	// Abc_NtkForEachObj(pNtk, pObj, ithObj){
	// 	std::cout << "[" << ithObj << "]";
	// 	printf("%u %u %d\n", Abc_ObjType(pObj), Abc_ObjId(pObj), Abc_ObjLevel(pObj));
	// }
	// Abc_NtkForEachPi(pNtk, pObj, ithObj){
	// 	std::cout << "PI" << ithObj << ")";
	// 	printf("%u %u %d\n", Abc_ObjType(pObj), Abc_ObjId(pObj), Abc_ObjLevel(pObj));
	// }
	// Abc_NtkForEachPo(pNtk, pObj, ithObj){
	// 	std::cout << "Po" << ithObj << ")";
	// 	printf("%u %u %d\n", Abc_ObjType(pObj), Abc_ObjId(pObj), Abc_ObjLevel(pObj));
	// }
	ifs.close();
	delete [] simulationAct;
	for(int i = 0; i < simArrLen; ++i){
		delete [] simulationArr[i];
	}
	delete [] simulationArr;
    return 0;

}

void conductSimulation(Abc_Ntk_t *pNtk, Abc_Obj_t *pObj, bool simulaitonAct[], unsigned **simulationarr, int simArrWid){
	
	unsigned thisNodeId = Abc_ObjId(pObj);
	Abc_Obj_t *leftFanin = Abc_ObjFanin0(pObj);
	Abc_Obj_t *rightFanin = Abc_ObjFanin1(pObj);
	unsigned leftIdx = leftFanin->Id;
	unsigned rightIdx = rightFanin->Id;

	if(!simulaitonAct[leftIdx]) conductSimulation(pNtk, leftFanin, simulaitonAct, simulationarr, simArrWid);
	if(!simulaitonAct[rightIdx]) conductSimulation(pNtk, rightFanin, simulaitonAct, simulationarr, simArrWid);

	// printf("Doing %u -> %u <= %u", leftIdx, thisNodeId, rightIdx);
	
	simulaitonAct[(int)thisNodeId] = true;
	
	if((Abc_ObjFaninC0(pObj) == 0) && (Abc_ObjFaninC1(pObj) == 0)){
		for(int i = 0; i < simArrWid; ++i){
			simulationarr[thisNodeId][i] = simulationarr[leftIdx][i] & simulationarr[rightIdx][i];
		}
	}else if((Abc_ObjFaninC0(pObj) == 0) && (Abc_ObjFaninC1(pObj) == 1)){
		for(int i = 0; i < simArrWid; ++i){
			simulationarr[thisNodeId][i] = simulationarr[leftIdx][i] & (~simulationarr[rightIdx][i]);
		}
	}else if((Abc_ObjFaninC0(pObj) == 1) && (Abc_ObjFaninC1(pObj) == 0)){
		for(int i = 0; i < simArrWid; ++i){
			simulationarr[thisNodeId][i] = (~simulationarr[leftIdx][i]) & simulationarr[rightIdx][i];
		}
	}else{ // 11
		for(int i = 0; i < simArrWid; ++i){
			simulationarr[thisNodeId][i] = (~simulationarr[leftIdx][i]) & (~simulationarr[rightIdx][i]);
		}

	}
	
}
