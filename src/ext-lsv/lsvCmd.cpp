#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <stdlib.h>
#include <vector>
#include <set>
#include <algorithm>

static int Lsv_CommandPrintNodes(Abc_Frame_t *pAbc, int argc, char **argv);
static int Lsv_CommandPrintCut(Abc_Frame_t *pAbc, int argc, char **argv);

void init(Abc_Frame_t *pAbc)
{
	Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
	Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_CommandPrintCut, 0);
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