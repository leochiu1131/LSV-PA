#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <iostream>
#include <vector>
#include <set>
#include <algorithm>

using namespace std;
using CutSet = vector<set<int>>; // To store all k-feasible cut

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintCut(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
	Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
	Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_CommandPrintCut, 0);
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

// ---------------------------------------------------------------------------- //

void RecursiveProcess(Abc_Obj_t* pNode, int k) {
	// Check if the cuts are already computed and cached in pNode's pData
	if (pNode->pData != nullptr) return;

    CutSet* cuts = new CutSet;

	if (Abc_ObjIsCi(pNode)) {
        set<int> singleCut;
        singleCut.insert(Abc_ObjId(pNode));
        cuts->push_back(singleCut);
        pNode->pData = cuts;  // Cache the cuts
		return;
    }

    // Get children of the current node
    Abc_Obj_t* pFanin0 = Abc_ObjFanin0(pNode);
    Abc_Obj_t* pFanin1 = Abc_ObjFanin1(pNode);

    // Recursive calls to process children
	RecursiveProcess(pFanin0, k);
	RecursiveProcess(pFanin1, k);
	CutSet* fanin0Cuts = static_cast<CutSet*>(pFanin0->pData);
	CutSet* fanin1Cuts = static_cast<CutSet*>(pFanin1->pData);

	// Combine cuts from both fanins
    for (const auto& cut0 : *fanin0Cuts) {
        for (const auto& cut1 : *fanin1Cuts) {
            set<int> combinedCut = cut0;
            combinedCut.insert(cut1.begin(), cut1.end());
            if (combinedCut.size() <= k) {
                cuts->push_back(combinedCut);
            }
        }
    }
	set<int> selfCut;
	selfCut.insert(Abc_ObjId(pNode));
	cuts->push_back(selfCut);
	pNode->pData = cuts;
}

// Function for sorting
bool compareBySize(const set<int>& a, const set<int>& b) {
    return a.size() < b.size();
}

void Print_AllCut(Abc_Obj_t* pNode) {
	CutSet* cuts = static_cast<CutSet*>(pNode->pData);

	sort(cuts->begin(), cuts->end(), compareBySize);

	// Print the k-feasible cuts
	for (const auto& cut : *cuts) {
		Abc_Print(1, "%d: ", Abc_ObjId(pNode));
		for (const auto& fanin : cut) {
			Abc_Print(1, "%d ", fanin);
		}
		Abc_Print(1, "\n");
	}
}

void Lsv_NtkPrintCut(Abc_Ntk_t* pNtk, int k) {
  	Abc_Obj_t* pObj;
    int i;

    // Iterate over each primary output (PO) in the network
    Abc_NtkForEachPo(pNtk, pObj, i) {
        RecursiveProcess(Abc_ObjFanin0(pObj), k);
    }

	Abc_NtkForEachPi(pNtk, pObj, i) {
		Print_AllCut(pObj);
	}
	Abc_NtkForEachNode(pNtk, pObj, i) {
		Print_AllCut(pObj);
	}

	// Reset pData for all nodes in the network
    Abc_NtkForEachObj(pNtk, pObj, i) {
        if (pObj->pData != nullptr) {
            delete static_cast<CutSet*>(pObj->pData);
            pObj->pData = nullptr;
        }
    }
}

int Lsv_CommandPrintCut(Abc_Frame_t* pAbc, int argc, char** argv) {
  	Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  	int k;

	if (argc == 2) {
		if (strcmp(argv[1], "-h") == 0) {
		goto usage;
		}
		k = atoi(argv[1]);
		if (k < 3 || k > 6) {
		Abc_Print(-1, "k must be between 3 and 6.\n");
		goto usage;
		}
		if (!pNtk) {
		Abc_Print(-1, "Empty network.\n");
		return 1;
		}
		Lsv_NtkPrintCut(pNtk, k);
		return 0;
	}

  	Abc_Print(-1, "Missing or invalid argument for <k>.\n");

usage:
  	Abc_Print(-2, "usage: lsv_printcut <k> [h]\n");
  	Abc_Print(-2, "\t        prints the k-feasible cut enumeration\n");
  	Abc_Print(-2, "\t<k>   : k-feasible cuts where 3<=k<=6\n");
  	Abc_Print(-2, "\t-h    : print the command usage\n");
  	return 1;
}