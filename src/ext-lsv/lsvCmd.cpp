#include <iostream>
#include <vector>
#include <fstream>
#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/cnf/cnf.h"
#include "aig/aig/aig.h"
extern "C"{
 Aig_Man_t* Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters);
}
using namespace std;

static int Lsv_sdc(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_odc(Abc_Frame_t* pAbc, int argc, char** argv);
static int PrintAIGasDAG(Abc_Frame_t* pAbc, int argc, char** argv);
void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sdc", Lsv_sdc, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_odc", Lsv_odc, 0);
  Cmd_CommandAdd(pAbc, "LSV", "write_aigraph", PrintAIGasDAG, 0);
}
void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

int getSize(Abc_Ntk_t* pNtk){
	Abc_Obj_t* pObj;
	int i;
	Abc_NtkForEachObj(pNtk, pObj, i){}
	return i;
}
void Lsv_NtkPrintGates(Abc_Ntk_t* pNtk){
  	Abc_Obj_t* pObj;
  	int i;
  	Abc_NtkForEachObj(pNtk, pObj, i){
    	printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
    	Abc_Obj_t* pFanin;
    	int j;
    	Abc_ObjForEachFanin(pObj, pFanin, j) {
      	printf("  Fanin-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanin),
            Abc_ObjName(pFanin));
    	}
  	}
}
int PrintAIGasDAG(Abc_Frame_t* pAbc, int argc, char** argv){
	ofstream fout(argv[1]);
	Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  	Abc_Obj_t* pObj;
  	int i;
  	Abc_NtkForEachObj(pNtk, pObj, i){
    	Abc_Obj_t* pFanin;
    	int j;
    	Abc_ObjForEachFanin(pObj, pFanin, j) {
    		if(j==0){
    			fout<<Abc_ObjId(pObj)<<" "<<Abc_ObjId(pFanin)<<" "<<pObj->fCompl0<<endl;
			}
			else{
				fout<<Abc_ObjId(pObj)<<" "<<Abc_ObjId(pFanin)<<" "<<pObj->fCompl1<<endl;
			}
    	}
  	}
  	return 0;
}
int Lsv_sdc(Abc_Frame_t* pAbc, int argc, char** argv){
	Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
	int TargetNode = atoi(argv[1]);
	Abc_Obj_t* pNode = Abc_NtkObj(pNtk,TargetNode);
	Abc_Ntk_t* FICone = Abc_NtkCreateCone(pNtk,pNode,Abc_ObjName(pNode),1);
	Abc_Obj_t* ConeTop = Abc_NtkPo(FICone,0);
	Abc_Obj_t* ConeSecond = ConeTop;
	int j;
	Abc_ObjForEachFanin(ConeTop, ConeSecond, j) {}
	bool sdc_exist = false;
	vector<unsigned int> oc = {ConeSecond -> fCompl0,ConeSecond -> fCompl1};
	for(int i=0;i<4;i++){
		ConeSecond -> fCompl0 = i%2;
		ConeSecond -> fCompl1 = i/2;
		Aig_Man_t* pMan = Abc_NtkToDar(FICone,0,0);
		sat_solver* pSAT = sat_solver_new();
		Cnf_Dat_t * pCNF = Cnf_Derive(pMan, 1);
		Cnf_DataWriteIntoSolverInt(pSAT,pCNF,1,0);
	
		lit assumption[] = {4};
		lit* begin = assumption;
		lit* end = assumption+1;
		int satisfiability = sat_solver_solve(pSAT, begin, end, 1000, 1000, 1000, 1000);
		if(satisfiability<0){
	  		printf("%d%d ",1-(oc[0] ^ ConeSecond -> fCompl0),1-(oc[1] ^ ConeSecond -> fCompl1));
	  		sdc_exist = true;
		}
	}
	if(!sdc_exist){
  		printf("no sdc\n");
	}
	else{
  		printf("\n");
	}
  	int c;
  	Extra_UtilGetoptReset();
  	while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF){
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
	return 0;

	usage:
  		Abc_Print(-2, "usage: lsv_print_nodes [-h]\n");
  		Abc_Print(-2, "\t        prints the nodes in the network\n");
  		Abc_Print(-2, "\t-h    : print the command usage\n");
  	return 1;
}
Abc_Obj_t* GetConst1(Abc_Ntk_t* pNtk){
	int i;
	Abc_Obj_t* pObj;
	Abc_NtkForEachObj(pNtk, pObj, i){
		return pObj;
	}
	return pObj;
}
int Lsv_odc(Abc_Frame_t* pAbc, int argc, char** argv){
	Abc_Ntk_t* pNtk_0 = Abc_FrameReadNtk(pAbc);
	Abc_Ntk_t* pNtk = Abc_NtkDup(pNtk_0);
	Abc_Ntk_t* pNtk_new = Abc_NtkDup(pNtk_0);
  	Abc_Obj_t* pObj;
  	int i;
  	Abc_NtkForEachObj(pNtk_new, pObj, i){
    	Abc_Obj_t* pFanin; int j;
    	Abc_ObjForEachFanin(pObj, pFanin, j){
    		if(Abc_ObjId(pFanin)==atoi(argv[1])){
    			if(j==0){
    				pObj->fCompl0 = 1 - pObj->fCompl0;
				}
    			if(j==1){
    				pObj->fCompl1 = 1 - pObj->fCompl1;
				}				
			}
    	}
  	}
	int TargetNode = atoi(argv[1]);
	Abc_Obj_t* pNode = Abc_NtkObj(pNtk,TargetNode);
	Abc_Obj_t* pChild; int j; Abc_Ntk_t* FICone0; Abc_Ntk_t* FICone1;
	Abc_ObjForEachFanin(pNode, pChild, j){
		if(j==0){FICone0 = Abc_NtkCreateCone(pNtk,pChild,Abc_ObjName(pChild),1);}
		if(j==1){FICone1 = Abc_NtkCreateCone(pNtk,pChild,Abc_ObjName(pChild),1);}
	}
	Aig_Man_t* Manf0 = Abc_NtkToDar(FICone0,0,0);
	Aig_Man_t* Manf1 = Abc_NtkToDar(FICone0,0,0);
	Aig_Man_t* pMan = Abc_NtkToDar(pNtk,0,0);
	Aig_Man_t* pMan_new = Abc_NtkToDar(pNtk_new,0,0);
	sat_solver* pSAT_sdc = sat_solver_new();
	sat_solver* pSAT = sat_solver_new();
	Cnf_Dat_t * CNFf0 = Cnf_Derive(Manf0, Aig_ManCoNum(Manf0));
	Cnf_Dat_t * CNFf1 = Cnf_Derive(Manf1, Aig_ManCoNum(Manf1));
	Cnf_Dat_t * pCNF = Cnf_Derive(pMan, Aig_ManCoNum(pMan));
	Cnf_Dat_t * pCNF_new = Cnf_Derive(pMan_new, Aig_ManCoNum(pMan_new));
	Cnf_DataLift(pCNF_new, pCNF->nVars);
	Cnf_DataLift(CNFf0, pCNF->nVars + pCNF_new->nVars);
	Cnf_DataLift(CNFf1, pCNF->nVars + pCNF_new->nVars + CNFf0->nVars);
  	Abc_NtkForEachObj(pNtk, pObj, i){
    	//printf("%d %d\n",Abc_ObjId(pObj) ,pCNF->pVarNums[pObj->Id]);
  	}
	Cnf_DataWriteIntoSolverInt(pSAT,pCNF,1,0);
	Cnf_DataWriteIntoSolverInt(pSAT,pCNF_new,1,0);
	Cnf_DataWriteIntoSolverInt(pSAT,CNFf0,1,0);
	Cnf_DataWriteIntoSolverInt(pSAT,CNFf1,1,0);
	Cnf_DataWriteIntoSolverInt(pSAT_sdc,CNFf0,1,0);
	Cnf_DataWriteIntoSolverInt(pSAT_sdc,CNFf1,1,0);
	Abc_NtkForEachObj(pNtk, pObj, i){
		if(Abc_ObjIsPi(pObj) && i>0){
			sat_solver_add_xor(pSAT_sdc,CNFf0->pVarNums[pObj->Id],CNFf1->pVarNums[pObj->Id],0,0);
			sat_solver_add_xor(pSAT,CNFf0->pVarNums[pObj->Id],CNFf1->pVarNums[pObj->Id],0,0);
			sat_solver_add_xor(pSAT,CNFf0->pVarNums[pObj->Id],pCNF->pVarNums[pObj->Id],0,0);
			sat_solver_add_xor(pSAT,CNFf0->pVarNums[pObj->Id],pCNF_new->pVarNums[pObj->Id],0,0);
		}
	}
	
	vector<int> sat_ = {};
	vector<int> odc_ = {};
	bool has_odc = false;
	for(int i=0;i<4;i++){
		lit assumption[] = {4+pCNF->nVars + pCNF_new->nVars + i%2, 4+pCNF->nVars + pCNF_new->nVars + CNFf0->nVars+ i/2};
		lit* begin = assumption;
		lit* end = assumption+2;
		int satisfiability = sat_solver_solve(pSAT_sdc, begin, end, 1000, 1000, 1000, 1000);
		sat_.push_back(satisfiability);
	}
	for(int i=0;i<4;i++){
		lit assumption[] = {4};
		lit* begin = assumption;
		lit* end = assumption+1;
		int satisfiability = sat_solver_solve(pSAT, begin, end, 1000, 1000, 1000, 1000);
		odc_.push_back(satisfiability);
		if(sat_[i]>0 and odc_[i]<0){
			printf("%d%d ",i/2,i%2);
			has_odc = true;
		}
	}
	if(!has_odc){
		printf("no odc\n");
	}
	else{
		printf("\n");
	}
	return 0;
}
