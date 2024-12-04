#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <stdlib.h>
#include <vector>
#include <set>
#include <algorithm>
#include <cstdlib>
#include "sat/cnf/cnf.h"

extern "C"{
 Aig_Man_t* Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
 }

static int Lsv_CommandPrintNodes(Abc_Frame_t *pAbc, int argc, char **argv);
static int Lsv_CommandPrintCut(Abc_Frame_t *pAbc, int argc, char **argv);
static int Lsv_CommandSDC(Abc_Frame_t *pAbc, int argc, char **argv);
static int Lsv_CommandODC(Abc_Frame_t *pAbc, int argc, char **argv);

void init(Abc_Frame_t *pAbc)
{
	Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
	Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_CommandPrintCut, 0);
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sdc", Lsv_CommandSDC, 0);
    Cmd_CommandAdd(pAbc, "LSV", "lsv_odc", Lsv_CommandODC, 0);
}

void destroy(Abc_Frame_t *pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager
{
	PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

void Lsv_NtkPrintNodes(Abc_Ntk_t *pNtk)
{
	Abc_Obj_t *pObj;
	int i;
	Abc_NtkForEachNode(pNtk, pObj, i)
	{
		printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
		Abc_Obj_t *pFanin;
		int j;
		Abc_ObjForEachFanin(pObj, pFanin, j)
		{
			printf("  Fanin-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanin),
				   Abc_ObjName(pFanin));
		}
		if (Abc_NtkHasSop(pNtk))
		{
			printf("The SOP of this node:\n%s", (char *)pObj->pData);
		}
	}
}

void Lsv_NtkPrintCut(Abc_Ntk_t *pNtk, int k){
	Abc_Obj_t *pObj;
	std::vector<std::vector<std::set<unsigned int>>> cuts; // cuts[node_id-1][set_index]
	int i;
	Abc_NtkForEachPi(pNtk, pObj, i){
		cuts.push_back(std::vector<std::set<unsigned int>>()); 
		cuts[Abc_ObjId(pObj) - 1].push_back(std::set<unsigned int>{Abc_ObjId(pObj)});
		printf("%d: %d\n", Abc_ObjId(pObj), Abc_ObjId(pObj));
	}
	Abc_NtkForEachPo(pNtk, pObj, i){
		cuts.push_back(std::vector<std::set<unsigned int>>()); 
	}
	Abc_NtkForEachNode(pNtk, pObj, i){
		cuts.push_back(std::vector<std::set<unsigned int>>());
		cuts[Abc_ObjId(pObj) - 1].push_back(std::set<unsigned int>{Abc_ObjId(pObj)});
		for(const auto &j : cuts[Abc_ObjId(Abc_ObjFanin0(pObj)) - 1]){
			for(const auto &l : cuts[Abc_ObjId(Abc_ObjFanin1(pObj)) - 1]){
				cuts[Abc_ObjId(pObj) - 1].push_back(std::set<unsigned int>());
				cuts[Abc_ObjId(pObj) - 1].back().insert(j.begin(), j.end());
				cuts[Abc_ObjId(pObj) - 1].back().insert(l.begin(), l.end());
				if(cuts[Abc_ObjId(pObj) - 1].back().size() > k) cuts[Abc_ObjId(pObj) - 1].pop_back();
			}
		}
		// remove redundant cuts
        std::set<unsigned int> redundant_sets{}; // indices of redundant sets
        int idx = 0;
        for(const auto &j : cuts[Abc_ObjId(pObj) - 1]){
            for(const auto &l : cuts[Abc_ObjId(pObj) - 1]){
                if(j != l && std::includes(j.begin(), j.end(), l.begin(), l.end())){
                    redundant_sets.insert(idx);
                }
            }
            ++idx;
        }
        for(auto it = redundant_sets.rbegin(); it != redundant_sets.rend(); ++it){
            cuts[Abc_ObjId(pObj) - 1].erase(cuts[Abc_ObjId(pObj) - 1].begin() + *it);
        }
        for(const auto &j : cuts[Abc_ObjId(pObj) - 1]){
            printf("%d:", Abc_ObjId(pObj));
            for(const auto &l : j) printf(" %d", l);
            printf("\n");
        }
	}
}

void Lsv_SDCRandomSim(bool (&pattern_appearance)[4], Abc_Ntk_t *pCone){
    Abc_Obj_t *pObj;
    srand(0);
    bool fin_pattern[2] = {0, 0};
    for(int i = 0; i < Abc_NtkPiNum(pCone) * Abc_NtkPiNum(pCone); i++){
        int j;
        Abc_NtkForEachPi(pCone, pObj, j){
            pObj->iTemp = rand() % 2;
        }
        Abc_AigForEachAnd(pCone, pObj, j){
            fin_pattern[0] = Abc_ObjFanin0(pObj)->iTemp ^ Abc_ObjFaninC0(pObj);
            fin_pattern[1] = Abc_ObjFanin1(pObj)->iTemp ^ Abc_ObjFaninC1(pObj);
            pObj->iTemp = fin_pattern[0] && fin_pattern[1];
        }
        pattern_appearance[fin_pattern[0] * 2 + fin_pattern[1]] = 1;
    }
}

void Lsv_NtkSDC(Abc_Ntk_t *pNtk, int id){
    Abc_Obj_t *pObj;
    Abc_Ntk_t *pCone = Abc_NtkCreateCone(pNtk, Abc_NtkObj(pNtk, id), "sdc_target", 0);
    int j = 0;
    bool pattern_appearance[4] = {0, 0, 0, 0};
    Lsv_SDCRandomSim(pattern_appearance, pCone);
    //printf("pattern appearance rn is %d, %d, %d, %d\n", pattern_appearance[0], pattern_appearance[1], pattern_appearance[2], pattern_appearance[3]);
    for(int i = 0; i < 4; i++){
        //printf("checking pattern %d%d...\n", i / 2, i % 2);
        if(pattern_appearance[i]) continue;
        int RetValue = 0;
        Abc_AigForEachAnd(pCone, pObj, j);
        if(i == 0 || i == 1) Abc_ObjXorFaninC(pObj, 0);
        if(i == 0 || i == 2) Abc_ObjXorFaninC(pObj, 1);
        /*printf("cone for pattern %d%d:\n", i / 2, i % 2);
        Abc_AigForEachAnd(pCone, pObj, j){
            printf("%c%d %c%d\n", Abc_ObjFaninC0(pObj) ? "!" : "", Abc_ObjId(Abc_ObjFanin0(pObj)), Abc_ObjFaninC1(pObj) ? "!" : "", Abc_ObjId(Abc_ObjFanin1(pObj)));
        }*/
        /*Aig_Man_t *pAig = Abc_NtkToDar(pCone, 0, 0);
        sat_solver *pSat = sat_solver_new();
        Cnf_Dat_t *pCnf = Cnf_Derive(pAig, 1);
        Cnf_DataWriteIntoSolverInt(pSat, pCnf, 1, 0);*/
        //pattern_appearance[i] = sat_solver_solve(pSat, NULL, NULL, 0, 0, 0, 0) == 1;
        RetValue = Abc_NtkMiterSat( pCone, (ABC_INT64_T)0, (ABC_INT64_T)0, 0, NULL, NULL );
        if(RetValue == 0) pattern_appearance[i] = 1;
        if(i == 0 || i == 1) Abc_ObjXorFaninC(pObj, 0);
        if(i == 0 || i == 2) Abc_ObjXorFaninC(pObj, 1);
        
    }
    if(pattern_appearance[0] == 1 && pattern_appearance[1] == 1 && pattern_appearance[2] == 1 && pattern_appearance[3] == 1)
        printf("no sdc\n");
    else {
        for(int i = 0; i < 4; i++){
            if(!pattern_appearance[i]) printf("%d%d ", i / 2, i % 2);
        }
        printf("\n");
    }
}

bool Lsv_ODCRandomSim(Abc_Ntk_t *pMiter){
    Abc_Obj_t *pObj;
    srand(0);
    for(int i = 0; i < Abc_NtkPiNum(pMiter) * Abc_NtkPiNum(pMiter); i++){
        int j;
        Abc_NtkForEachPi(pMiter, pObj, j){
            pObj->iTemp = rand() % 2;
        }
        Abc_AigForEachAnd(pMiter, pObj, j){
            pObj->iTemp = (Abc_ObjFanin0(pObj)->iTemp ^ Abc_ObjFaninC0(pObj)) && (Abc_ObjFanin1(pObj)->iTemp ^ Abc_ObjFaninC1(pObj));
        }
        if(Abc_ObjFanin0(Abc_NtkPo(pMiter, 0))->iTemp){
            //printf("odcrandomsim return 1\n");
            return 1;
        }
    }
    //printf("odcrandomsim return 0\n");
    return 0;
}

void Lsv_NtkODC(Abc_Frame_t *pAbc, Abc_Ntk_t *pNtk, int id){
    Abc_Ntk_t *pMiter = Abc_NtkToLogic(pNtk);
    Abc_Ntk_t *pMiterCopy = Abc_NtkDup(pMiter);
    Abc_Ntk_t *pMiterStrashed;
    Abc_Obj_t *pNode, *pNodeDup, *pFanin, *pPo, *pOr, *pAnd, *pFanin0, *pFanin1, *pObj;
    bool pattern_appearance[4] = {0, 0, 0, 0};
    int i, j;
    ///////sdc/////////////
    Abc_Ntk_t *pCone = Abc_NtkCreateCone(pNtk, Abc_NtkObj(pNtk, id), "sdc_target", 0);
    Lsv_SDCRandomSim(pattern_appearance, pCone);
    for(int i = 0; i < 4; i++){
        if(pattern_appearance[i]) continue;
        int RetValue = 0;
        Abc_AigForEachAnd(pCone, pObj, j);
        if(i == 0 || i == 1) Abc_ObjXorFaninC(pObj, 0);
        if(i == 0 || i == 2) Abc_ObjXorFaninC(pObj, 1);
        RetValue = Abc_NtkMiterSat( pCone, (ABC_INT64_T)0, (ABC_INT64_T)0, 0, NULL, NULL );
        if(RetValue == 0) pattern_appearance[i] = 1;
        if(i == 0 || i == 1) Abc_ObjXorFaninC(pObj, 0);
        if(i == 0 || i == 2) Abc_ObjXorFaninC(pObj, 1);
    }
    for(int i = 0; i < 4; i++) pattern_appearance[i] ^= 1;
    //printf("sdc pattern appearance: %d %d %d %d\n", pattern_appearance[0], pattern_appearance[1], pattern_appearance[2], pattern_appearance[3]);
    ///////sdc done////////
    Abc_NtkForEachNode(pMiterCopy, pNode, i){ // copy nodes
        pNodeDup = Abc_NtkDupObj(pMiter, Abc_NtkObj(pMiter, i), 0);
        Abc_ObjForEachFanin(Abc_NtkObj(pMiter, i), pFanin, j){
            if(Abc_ObjIsPi(pFanin)) Abc_ObjAddFanin(pNodeDup, pFanin);
            else Abc_ObjAddFanin(pNodeDup, pFanin->pCopy);
            //printf("%d : %s\n", i, (char *)pNode->pData);
            if(Abc_ObjId(pFanin) == id) {
                //printf("%d %d\n", Abc_ObjFaninC0(Abc_NtkObj(pNtk, i)), Abc_ObjFaninC1(Abc_NtkObj(pNtk, i)));
                if(!(Abc_ObjFaninC0(Abc_NtkObj(pNtk, i))) && !(Abc_ObjFaninC1(Abc_NtkObj(pNtk, i)))) pNodeDup->pData = (char *)(j ? "10 1\n" : "01 1\n"); //11
                else if(!(Abc_ObjFaninC0(Abc_NtkObj(pNtk, i))) && (Abc_ObjFaninC1(Abc_NtkObj(pNtk, i)))) pNodeDup->pData = (char *)(j ? "11 1\n" : "00 1\n"); //10
                else if((Abc_ObjFaninC0(Abc_NtkObj(pNtk, i))) && !(Abc_ObjFaninC1(Abc_NtkObj(pNtk, i)))) pNodeDup->pData = (char *)(j ? "00 1\n" : "11 1\n"); //01
                else if((Abc_ObjFaninC0(Abc_NtkObj(pNtk, i))) && (Abc_ObjFaninC1(Abc_NtkObj(pNtk, i)))) pNodeDup->pData = (char *)(j ? "01 1\n" : "10 1\n"); //00
            }
        }
    }
    pOr = Abc_NtkCreateNode(pMiter);
    Abc_NtkForEachPo(pMiter, pPo, i){ // create xor gates for each po
        if(Abc_ObjId(Abc_ObjFanin0(pPo)) == id){
            printf("no odc\n");
            return;
        }
        pNode = Abc_NtkCreateNode(pMiter);
        pNode->pData = Abc_SopCreateXor((Mem_Flex_t *)pMiter->pManFunc, 2);
        Abc_ObjAddFanin(pNode, Abc_ObjFanin0(pPo));
        Abc_ObjAddFanin(pNode, Abc_ObjFanin0(pPo)->pCopy);
        Abc_ObjAddFanin(pOr, pNode);
        Abc_NtkDeleteObj(pPo);
    }
    pOr->pData = Abc_SopCreateOr((Mem_Flex_t *)pMiter->pManFunc, Abc_ObjFaninNum(pOr), 0); // or of xor gates
    pAnd = Abc_NtkCreateNode(pMiter);
    Abc_ObjAddFanin(pAnd, pOr);
    pFanin0 = Abc_ObjFanin0(Abc_NtkObj(pMiter, id));
    Abc_ObjAddFanin(pAnd, pFanin0);
    pFanin1 = Abc_ObjFanin1(Abc_NtkObj(pMiter, id));
    Abc_ObjAddFanin(pAnd, pFanin1);
    pPo = Abc_NtkCreatePo(pMiter);
    Abc_ObjAddFanin(pPo, pAnd);
    // miter construction completed

    pAnd->pData = (char*)"111 1\n";
    pMiterStrashed = Abc_NtkStrash(Abc_NtkDup(pMiter), 0, 1, 0);
    if(Lsv_ODCRandomSim(pMiterStrashed)) {
        if(!(Abc_ObjFaninC0(Abc_NtkObj(pNtk, id))) && !(Abc_ObjFaninC1(Abc_NtkObj(pNtk, id)))) pattern_appearance[3] = 1;
        else if(!(Abc_ObjFaninC0(Abc_NtkObj(pNtk, id))) && (Abc_ObjFaninC1(Abc_NtkObj(pNtk, id)))) pattern_appearance[2] = 1;
        else if((Abc_ObjFaninC0(Abc_NtkObj(pNtk, id))) && !(Abc_ObjFaninC1(Abc_NtkObj(pNtk, id)))) pattern_appearance[1] = 1;
        else if((Abc_ObjFaninC0(Abc_NtkObj(pNtk, id))) && (Abc_ObjFaninC1(Abc_NtkObj(pNtk, id)))) pattern_appearance[0] = 1;
    }
    else if(Abc_NtkMiterSat( pMiterStrashed, (ABC_INT64_T)0, (ABC_INT64_T)0, 0, NULL, NULL ) == 0){
        if(!(Abc_ObjFaninC0(Abc_NtkObj(pNtk, id))) && !(Abc_ObjFaninC1(Abc_NtkObj(pNtk, id)))) pattern_appearance[3] = 1;
        else if(!(Abc_ObjFaninC0(Abc_NtkObj(pNtk, id))) && (Abc_ObjFaninC1(Abc_NtkObj(pNtk, id)))) pattern_appearance[2] = 1;
        else if((Abc_ObjFaninC0(Abc_NtkObj(pNtk, id))) && !(Abc_ObjFaninC1(Abc_NtkObj(pNtk, id)))) pattern_appearance[1] = 1;
        else if((Abc_ObjFaninC0(Abc_NtkObj(pNtk, id))) && (Abc_ObjFaninC1(Abc_NtkObj(pNtk, id)))) pattern_appearance[0] = 1;
    }
    pAnd->pData = (char*)"100 1\n";
    pMiterStrashed = Abc_NtkStrash(Abc_NtkDup(pMiter), 0, 1, 0);
    if(Lsv_ODCRandomSim(pMiterStrashed)) {
        if(!(Abc_ObjFaninC0(Abc_NtkObj(pNtk, id))) && !(Abc_ObjFaninC1(Abc_NtkObj(pNtk, id)))) pattern_appearance[0] = 1;
        else if(!(Abc_ObjFaninC0(Abc_NtkObj(pNtk, id))) && (Abc_ObjFaninC1(Abc_NtkObj(pNtk, id)))) pattern_appearance[1] = 1;
        else if((Abc_ObjFaninC0(Abc_NtkObj(pNtk, id))) && !(Abc_ObjFaninC1(Abc_NtkObj(pNtk, id)))) pattern_appearance[2] = 1;
        else if((Abc_ObjFaninC0(Abc_NtkObj(pNtk, id))) && (Abc_ObjFaninC1(Abc_NtkObj(pNtk, id)))) pattern_appearance[3] = 1;
    }
    else if(Abc_NtkMiterSat( pMiterStrashed, (ABC_INT64_T)0, (ABC_INT64_T)0, 0, NULL, NULL ) == 0){
        if(!(Abc_ObjFaninC0(Abc_NtkObj(pNtk, id))) && !(Abc_ObjFaninC1(Abc_NtkObj(pNtk, id)))) pattern_appearance[0] = 1;
        else if(!(Abc_ObjFaninC0(Abc_NtkObj(pNtk, id))) && (Abc_ObjFaninC1(Abc_NtkObj(pNtk, id)))) pattern_appearance[1] = 1;
        else if((Abc_ObjFaninC0(Abc_NtkObj(pNtk, id))) && !(Abc_ObjFaninC1(Abc_NtkObj(pNtk, id)))) pattern_appearance[2] = 1;
        else if((Abc_ObjFaninC0(Abc_NtkObj(pNtk, id))) && (Abc_ObjFaninC1(Abc_NtkObj(pNtk, id)))) pattern_appearance[3] = 1;
    }
    pAnd->pData = (char*)"101 1\n";
    pMiterStrashed = Abc_NtkStrash(Abc_NtkDup(pMiter), 0, 1, 0);
    if(Lsv_ODCRandomSim(pMiterStrashed)) {
        if(!(Abc_ObjFaninC0(Abc_NtkObj(pNtk, id))) && !(Abc_ObjFaninC1(Abc_NtkObj(pNtk, id)))) pattern_appearance[1] = 1;
        else if(!(Abc_ObjFaninC0(Abc_NtkObj(pNtk, id))) && (Abc_ObjFaninC1(Abc_NtkObj(pNtk, id)))) pattern_appearance[0] = 1;
        else if((Abc_ObjFaninC0(Abc_NtkObj(pNtk, id))) && !(Abc_ObjFaninC1(Abc_NtkObj(pNtk, id)))) pattern_appearance[3] = 1;
        else if((Abc_ObjFaninC0(Abc_NtkObj(pNtk, id))) && (Abc_ObjFaninC1(Abc_NtkObj(pNtk, id)))) pattern_appearance[2] = 1;
    }
    else if(Abc_NtkMiterSat( pMiterStrashed, (ABC_INT64_T)0, (ABC_INT64_T)0, 0, NULL, NULL ) == 0){
        if(!(Abc_ObjFaninC0(Abc_NtkObj(pNtk, id))) && !(Abc_ObjFaninC1(Abc_NtkObj(pNtk, id)))) pattern_appearance[1] = 1;
        else if(!(Abc_ObjFaninC0(Abc_NtkObj(pNtk, id))) && (Abc_ObjFaninC1(Abc_NtkObj(pNtk, id)))) pattern_appearance[0] = 1;
        else if((Abc_ObjFaninC0(Abc_NtkObj(pNtk, id))) && !(Abc_ObjFaninC1(Abc_NtkObj(pNtk, id)))) pattern_appearance[3] = 1;
        else if((Abc_ObjFaninC0(Abc_NtkObj(pNtk, id))) && (Abc_ObjFaninC1(Abc_NtkObj(pNtk, id)))) pattern_appearance[2] = 1;
    }
    pAnd->pData = (char*)"110 1\n";
    pMiterStrashed = Abc_NtkStrash(Abc_NtkDup(pMiter), 0, 1, 0);
    if(Lsv_ODCRandomSim(pMiterStrashed)) {
        if(!(Abc_ObjFaninC0(Abc_NtkObj(pNtk, id))) && !(Abc_ObjFaninC1(Abc_NtkObj(pNtk, id)))) pattern_appearance[2] = 1;
        else if(!(Abc_ObjFaninC0(Abc_NtkObj(pNtk, id))) && (Abc_ObjFaninC1(Abc_NtkObj(pNtk, id)))) pattern_appearance[3] = 1;
        else if((Abc_ObjFaninC0(Abc_NtkObj(pNtk, id))) && !(Abc_ObjFaninC1(Abc_NtkObj(pNtk, id)))) pattern_appearance[0] = 1;
        else if((Abc_ObjFaninC0(Abc_NtkObj(pNtk, id))) && (Abc_ObjFaninC1(Abc_NtkObj(pNtk, id)))) pattern_appearance[1] = 1;
    }
    else if(Abc_NtkMiterSat( pMiterStrashed, (ABC_INT64_T)0, (ABC_INT64_T)0, 0, NULL, NULL ) == 0){
        if(!(Abc_ObjFaninC0(Abc_NtkObj(pNtk, id))) && !(Abc_ObjFaninC1(Abc_NtkObj(pNtk, id)))) pattern_appearance[2] = 1;
        else if(!(Abc_ObjFaninC0(Abc_NtkObj(pNtk, id))) && (Abc_ObjFaninC1(Abc_NtkObj(pNtk, id)))) pattern_appearance[3] = 1;
        else if((Abc_ObjFaninC0(Abc_NtkObj(pNtk, id))) && !(Abc_ObjFaninC1(Abc_NtkObj(pNtk, id)))) pattern_appearance[0] = 1;
        else if((Abc_ObjFaninC0(Abc_NtkObj(pNtk, id))) && (Abc_ObjFaninC1(Abc_NtkObj(pNtk, id)))) pattern_appearance[1] = 1;
    }
    
    if(pattern_appearance[0] == 1 && pattern_appearance[1] == 1 && pattern_appearance[2] == 1 && pattern_appearance[3] == 1)
        printf("no odc\n");
    else {
        for(int i = 0; i < 4; i++){
            if(!pattern_appearance[i]) printf("%d%d ", i / 2, i % 2);
        }
        printf("\n");
    }
    //Abc_FrameReplaceCurrentNetwork(pAbc, pMiter);
    return;
}

int Lsv_CommandPrintNodes(Abc_Frame_t *pAbc, int argc, char **argv)
{
	Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
	int c;
	Extra_UtilGetoptReset();
	while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF)
	{
		switch (c)
		{
		case 'h':
			goto usage;
		default:
			goto usage;
		}
	}
	if (!pNtk)
	{
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

int Lsv_CommandPrintCut(Abc_Frame_t *pAbc, int argc, char **argv){
	Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
	int c, k;
	Extra_UtilGetoptReset();
	while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF)
	{
		switch (c)
		{
		case 'h':
			goto usage;
		default:
			goto usage;
		}
	}
	if (!pNtk)
	{
		Abc_Print(-1, "Empty network.\n");
		return 1;
	}
    if (!Abc_NtkIsStrash(pNtk)){
        Abc_Print(-1, "This command works only for AIGs (run \"strash\").\n");
        return 1;
    }
	if (argc != globalUtilOptind + 1){
		Abc_Print(-1, "Wrong number of auguments.\n");
		goto usage;
	}
	k = atoi(argv[globalUtilOptind]);
	if (k < 1){
		Abc_Print(-1, "k should be a positive integer.\n");
		return 1;
	}
	Lsv_NtkPrintCut(pNtk, k);
	return 0;

usage:
	Abc_Print(-2, "usage: lsv_printcut [-h] <k>\n");
	Abc_Print(-2, "\t      prints the k-feasible cut for all AND nodes and PI in the network\n");
	Abc_Print(-2, "\t-h  : print the command usage\n");
	Abc_Print(-2, "\t<k> : the maximum node count in the printed cuts\n");
	return 1;
}

int Lsv_CommandSDC(Abc_Frame_t *pAbc, int argc, char **argv){
    Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
	int c, id;
	Extra_UtilGetoptReset();
	while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF)
	{
		switch (c)
		{
		case 'h':
			goto usage;
		default:
			goto usage;
		}
	}
	if (!pNtk)
	{
		Abc_Print(-1, "Empty network.\n");
		return 1;
	}
    if (!Abc_NtkIsStrash(pNtk)){
        Abc_Print(-1, "This command works only for AIGs (run \"strash\").\n");
        return 1;
    }
	if (argc != globalUtilOptind + 1){
		Abc_Print(-1, "Wrong number of auguments.\n");
		goto usage;
	}
	id = atoi(argv[globalUtilOptind]);
	Lsv_NtkSDC(pNtk, id);
	return 0;

usage:
	Abc_Print(-2, "usage: lsv_sdc [-h] <id>\n");
	Abc_Print(-2, "\t      prints the satisfiability don't-cares of the node\n");
	Abc_Print(-2, "\t-h  : print the command usage\n");
	Abc_Print(-2, "\t<id> : node id\n");
	return 1;
}

int Lsv_CommandODC(Abc_Frame_t *pAbc, int argc, char **argv){
    Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
	int c, id;
	Extra_UtilGetoptReset();
	while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF)
	{
		switch (c)
		{
		case 'h':
			goto usage;
		default:
			goto usage;
		}
	}
	if (!pNtk)
	{
		Abc_Print(-1, "Empty network.\n");
		return 1;
	}
    if (!Abc_NtkIsStrash(pNtk)){
        Abc_Print(-1, "This command works only for AIGs (run \"strash\").\n");
        return 1;
    }
	if (argc != globalUtilOptind + 1){
		Abc_Print(-1, "Wrong number of auguments.\n");
		goto usage;
	}
	id = atoi(argv[globalUtilOptind]);
	Lsv_NtkODC(pAbc, pNtk, id);
	return 0;
usage:
	Abc_Print(-2, "usage: lsv_odc [-h] <id>\n");
	Abc_Print(-2, "\t      prints the observability don't-cares of the node\n");
	Abc_Print(-2, "\t-h  : print the command usage\n");
	Abc_Print(-2, "\t<id> : node id\n");
	return 1;
}