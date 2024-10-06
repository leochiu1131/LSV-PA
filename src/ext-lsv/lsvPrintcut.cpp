#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <stdio.h>
#include <stdlib.h>
#include "misc/vec/vec.h"

static int Lsv_CommandPrintCut(Abc_Frame_t* pAbc, int argc, char** argv);
static Vec_Ptr_t* ComputeNodeCuts(Abc_Obj_t* pNode, int k);
static void DisplayCuts(Abc_Obj_t* pNode, Vec_Ptr_t* cut_ptr);
static void ProcessNetworkCuts(Abc_Ntk_t* pNtk, int k);
void init(Abc_Frame_t* pAbc);
void destroy(Abc_Frame_t* pAbc);


void init(Abc_Frame_t* pAbc) {
	Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_CommandPrintCut, 0);
}

void destroy(Abc_Frame_t* pAbc) {}


Abc_FrameInitializer_t frameInitializer = { init, destroy };


struct PackageRegistration {
	PackageRegistration() { Abc_FrameAddInitializer(&frameInitializer); }
} lsvPackageRegistration;


static Vec_Ptr_t* ComputeNodeCuts(Abc_Obj_t* pNode, int k) {
	if (Abc_ObjIsCi(pNode)) {
		Vec_Int_t* cut_int = Vec_IntAlloc(1);
		Vec_Ptr_t* cut_ptr = Vec_PtrAlloc(1);
		Vec_IntPush(cut_int, Abc_ObjId(pNode));
		Vec_PtrPush(cut_ptr, cut_int);
		return cut_ptr;
	}

	// Check for nodes with fewer than 2 fanins
	if (Abc_ObjFaninNum(pNode) < 2) {
		return NULL;
	}

	// Retrieve fanin nodes and their respective cuts
	Abc_Obj_t* pre_fan_in_0 = Abc_ObjFanin0(pNode);
	Abc_Obj_t* pre_fan_in_1 = Abc_ObjFanin1(pNode);
	Vec_Ptr_t* cut_ptr0 = ComputeNodeCuts(pre_fan_in_0, k);
	Vec_Ptr_t* cut_ptr1 = ComputeNodeCuts(pre_fan_in_1, k);

	if (!cut_ptr0 || !cut_ptr1) return NULL;

	// Store combined cuts
	Vec_Ptr_t* total_cuts_vec = Vec_PtrAlloc(100);

	// Add the node itself as an individual cut
	Vec_Int_t* single_cut_vec = Vec_IntAlloc(1);
	Vec_IntPush(single_cut_vec, Abc_ObjId(pNode));
	Vec_PtrPush(total_cuts_vec, single_cut_vec);

	// Merge cuts of fanins using a Cartesian product
	int i, j;
	Vec_Int_t *cut_int0, *cut_int1;
	Vec_PtrForEachEntry(Vec_Int_t*, cut_ptr0, cut_int0, i) {
		Vec_PtrForEachEntry(Vec_Int_t*, cut_ptr1, cut_int1, j) {
			Vec_Int_t* new_cut_int = Vec_IntTwoMerge(cut_int0, cut_int1);

			// Skip cuts that exceed the size limit k
			if (Vec_IntSize(new_cut_int) > k) {
				Vec_IntFree(new_cut_int);
				continue;
			}

			// Add current node to the cut if not already present
			if (!Vec_IntFind(new_cut_int, Abc_ObjId(pNode)))
				Vec_IntPush(new_cut_int, Abc_ObjId(pNode));

			Vec_IntSort(new_cut_int, 0);
			Vec_PtrPush(total_cuts_vec, new_cut_int);
		}
	}

	Vec_PtrFree(cut_ptr0);
	Vec_PtrFree(cut_ptr1);

	return total_cuts_vec;
}

// Display cuts for a given node
static void DisplayCuts(Abc_Obj_t* pNode, Vec_Ptr_t* cut_ptr) {
	Vec_Int_t* cut_int;
	int i, j;

	printf("%d: ", Abc_ObjId(pNode));
	cut_int = (Vec_Int_t*)Vec_PtrEntry(cut_ptr, 0);
	for (j = 0; j < Vec_IntSize(cut_int); j++)
		printf("%d ", Vec_IntEntry(cut_int, j));
	printf("\n");

	for (i = 1; i < Vec_PtrSize(cut_ptr); i++) {
		printf("%d: ", Abc_ObjId(pNode));
		cut_int = (Vec_Int_t*)Vec_PtrEntry(cut_ptr, i);
		for (j = 0; j < Vec_IntSize(cut_int); j++)
			printf("%d ", Vec_IntEntry(cut_int, j));
		printf("\n");
	}
}

// Enumerate and display k-feasible cuts for each node in the network
static void ProcessNetworkCuts(Abc_Ntk_t* pNtk, int k) {
	Abc_Obj_t* pNode;
	int i;

	Abc_NtkForEachObj(pNtk, pNode, i) {
		if (Abc_ObjIsNode(pNode) || Abc_ObjIsCi(pNode)) {
			Vec_Ptr_t* cut_ptr = ComputeNodeCuts(pNode, k);
			if (cut_ptr) {
				DisplayCuts(pNode, cut_ptr);
				Vec_PtrFree(cut_ptr);
			}
		}
	}
}

// Command function for `lsv_printcut`
static int Lsv_CommandPrintCut(Abc_Frame_t* pAbc, int argc, char** argv) {
	int k;

	if (argc != 2 || (k = atoi(argv[1])) < 1) {
		printf("Usage: lsv_printcut <k>\n");
		return 1;
	}

	Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
	if (!pNtk) {
		printf("Error: Network Not Found!!\n");
		return 1;
	}

	ProcessNetworkCuts(pNtk, k);
	return 0;
}
