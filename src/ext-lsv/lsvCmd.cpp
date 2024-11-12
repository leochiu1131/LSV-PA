#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <iostream>
#include <vector>
#include <set>
#include <algorithm>
#include <map>
#include <random>
// #include <sat_solver.h>

using namespace std;
using CutSet = vector<set<int>>; // To store all k-feasible cut

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintCut(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSdc(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandOdc(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
	Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
	Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_CommandPrintCut, 0);
	Cmd_CommandAdd(pAbc, "LSV", "lsv_sdc", Lsv_CommandSdc, 0);
    Cmd_CommandAdd(pAbc, "LSV", "lsv_odc", Lsv_CommandOdc, 0);
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
			printf("  Fanin-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanin), Abc_ObjName(pFanin));
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

    // // Get children of the current node
    Abc_Obj_t* pFanin0 = Abc_ObjFanin0(pNode);
    Abc_Obj_t* pFanin1 = Abc_ObjFanin1(pNode);

    // // Recursive calls to process children
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

bool contains(const set<int>& a, const set<int>& b) {
	return includes(a.begin(), a.end(), b.begin(), b.end());
}

void Print_AllCut(Abc_Obj_t* pNode) {
	CutSet* cuts = static_cast<CutSet*>(pNode->pData);

	if (cuts != nullptr) {
		sort(cuts->begin(), cuts->end(), compareBySize);

		for (size_t i = 0; i < cuts->size(); ++i) {
			bool isContained = false;

			// check if this cut contains other cuts
			for (size_t j = 0; j < cuts->size(); ++j) {
				if (i != j && contains((*cuts)[i], (*cuts)[j])) {
					isContained = true;
					break;
				}
			}

			// only if the cut 
			if (!isContained) {
				Abc_Print(1, "%d: ", Abc_ObjId(pNode));
				for (const auto& fanin : (*cuts)[i]) {
					Abc_Print(1, "%d ", fanin);
				}
				Abc_Print(1, "\n");
			}
		}
	}
	else {
		Abc_Print(1, "%d: ", Abc_ObjId(pNode));
		Abc_Print(1, "%d", Abc_ObjId(pNode));
		Abc_Print(1, "\n");
	}
}

void Lsv_NtkPrintCut(Abc_Ntk_t* pNtk, int k) {
  	Abc_Obj_t* pObj;
    int i;

    // Iterate over each primary output (PO) in the network
    Abc_NtkForEachPo(pNtk, pObj, i) {
		CutSet* cuts = new CutSet;
        RecursiveProcess(Abc_ObjFanin0(pObj), k);
		CutSet* FaninCuts = static_cast<CutSet*>(Abc_ObjFanin0(pObj)->pData);
		for (const auto& cut : *FaninCuts) {
			cuts->push_back(cut);
		}
		set<int> PoCut;
        PoCut.insert(Abc_ObjId(pObj));
		cuts->push_back(PoCut);
		pObj->pData = cuts;
    }

	Abc_NtkForEachPi(pNtk, pObj, i) {
		Print_AllCut(pObj);
	}
	Abc_NtkForEachPo(pNtk, pObj, i) {
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

// ---------------------------------------------------------------------------- //

void simulate_node(Abc_Obj_t* pObj, map<int, bool>& values) {
    if (Abc_ObjIsCi(pObj)) return;
    
    bool v0 = values[Abc_ObjId(Abc_ObjFanin0(pObj))];
    bool v1 = values[Abc_ObjId(Abc_ObjFanin1(pObj))];
	int fCompl0 = Abc_ObjFaninC0(pObj);
	int fCompl1 = Abc_ObjFaninC1(pObj);
	if (fCompl0) v0 = !v0;
	if (fCompl1) v1 = !v1;
    values[Abc_ObjId(pObj)] = (v0 && v1);
}

void collect_fanin_cone(Abc_Obj_t* pObj, set<Abc_Obj_t*>& cone) {
    if (cone.count(pObj)) return;
    cone.insert(pObj);
    
    if (!Abc_ObjIsCi(pObj)) {
        collect_fanin_cone(Abc_ObjFanin0(pObj), cone);
        collect_fanin_cone(Abc_ObjFanin1(pObj), cone);
    }
}

vector<pair<bool,bool>> Lsv_findSdc(Abc_Ntk_t* pNtk, Abc_Obj_t* pNode) {
    Abc_Obj_t* y0 = Abc_ObjFanin0(pNode);
    Abc_Obj_t* y1 = Abc_ObjFanin1(pNode);
	int pol_y0 = Abc_ObjFaninC0(pNode);
	int pol_y1 = Abc_ObjFaninC1(pNode);
    
    // Step 1: Random simulation
    set<pair<bool,bool>> simulated_patterns;
    const int num_sim = 100;
    
    for(int i = 0; i < num_sim; i++) {
        map<int, bool> sim_values;
        
        // Random PI assignment
        Abc_Obj_t* pObj;
        int j;
        Abc_NtkForEachPi(pNtk, pObj, j) {
            sim_values[Abc_ObjId(pObj)] = rand() % 2;
        }
        
        // Simulate network
        Abc_NtkForEachNode(pNtk, pObj, j) {
            simulate_node(pObj, sim_values);
        }
        
        simulated_patterns.insert({
            sim_values[Abc_ObjId(y0)],
            sim_values[Abc_ObjId(y1)]
        });
    }
    
    // Step 2: SAT check for unsimulated patterns
    vector<pair<bool,bool>> sdc_patterns;
    vector<pair<bool,bool>> all_patterns = {{0,0},{0,1},{1,0},{1,1}};
    
    // Collect fanin cone
    set<Abc_Obj_t*> cone_nodes;
    collect_fanin_cone(y0, cone_nodes);
    collect_fanin_cone(y1, cone_nodes);
    
    for(auto pattern : all_patterns) {
        if(simulated_patterns.count(pattern)) continue;
        
        // Build CNF using ABC's built-in SAT solver
        sat_solver* pSat = sat_solver_new();
        map<Abc_Obj_t*, int> node_to_var;
        int var_count = 0;
        
        // Create variables for nodes in cone
        for(auto node : cone_nodes) {
            node_to_var[node] = ++var_count;
        }
        
        // Add clauses for AND gates in cone
        for(auto node : cone_nodes) {
            if (!Abc_ObjIsCi(node)) {
                int out = node_to_var[node];
                int in1 = node_to_var[Abc_ObjFanin0(node)];
                int in2 = node_to_var[Abc_ObjFanin1(node)];
                
                // fanin polarity
                int fCompl0 = Abc_ObjFaninC0(node);
                int fCompl1 = Abc_ObjFaninC1(node);
                
                lit Lits[3];
                
                // CNF for AND gate with possible inversions
				// c = a & b
				// (c + a' + b')(a + c')(b + c')
                Lits[0] = toLitCond(out, 1);
                Lits[1] = toLitCond(in1, fCompl0);
                sat_solver_addclause(pSat, Lits, Lits + 2);
                
                Lits[0] = toLitCond(out, 1);
                Lits[1] = toLitCond(in2, fCompl1);
                sat_solver_addclause(pSat, Lits, Lits + 2);
                
                Lits[0] = toLitCond(out, 0);
                Lits[1] = toLitCond(in1, !fCompl0);
                Lits[2] = toLitCond(in2, !fCompl1);
                sat_solver_addclause(pSat, Lits, Lits + 3);
            }
        }
        
        // Add constraints for y0,y1 values
        lit Lits[1];
        Lits[0] = toLitCond(node_to_var[y0], !pattern.first);
        sat_solver_addclause(pSat, Lits, Lits + 1);
        Lits[0] = toLitCond(node_to_var[y1], !pattern.second);
        sat_solver_addclause(pSat, Lits, Lits + 1);
        
        // Check satisfiability
        if (sat_solver_solve(pSat, NULL, NULL, 0, 0, 0, 0) == l_False) {
			if (pol_y0) pattern.first = !pattern.first;
			if (pol_y1) pattern.second = !pattern.second;
            sdc_patterns.push_back(pattern);
        }
        
        sat_solver_delete(pSat);
    }
    
    return sdc_patterns;
}

int Lsv_CommandSdc(Abc_Frame_t* pAbc, int argc, char** argv) {
  	Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  	Abc_Obj_t* n;

	if (argc == 2) {
		if (strcmp(argv[1], "-h") == 0) {
			goto usage;
		}
		if (!pNtk) {
			Abc_Print(-1, "Empty network.\n");
			return 1;
		}
		int nodeId = atoi(argv[1]);
		if (nodeId >= Abc_NtkObjNum(pNtk)) {
			Abc_Print(-1, "Node id %d exceeds maximum node id %d\n", 
					  nodeId, Abc_NtkObjNum(pNtk) - 1);
			goto usage;
		}
		if (nodeId < 1) {
			Abc_Print(-1, "node id must be positive\n");
			goto usage;
		}
		n = Abc_NtkObj(pNtk, nodeId);
		if (Abc_ObjIsCi(n) || Abc_ObjIsCo(n)) {
			Abc_Print(-1, "please input internal node id\n");
			goto usage;
		}
		vector<pair<bool,bool>> patterns = Lsv_findSdc(pNtk, n);
		if (patterns.empty()) {
			Abc_Print(-2, "no sdc\n");
		} else {
			for (auto pattern : patterns) {
				Abc_Print(-2, "%d%d\n", pattern.first, pattern.second);
			}
		}
		return 0;
	}

  	Abc_Print(-1, "Missing or invalid argument for <n>.\n");

usage:
  	Abc_Print(-2, "usage: lsv_sdc <n> [h]\n");
  	Abc_Print(-2, "\t        prints the sdc of node n\n");
  	Abc_Print(-2, "\t<n>   : node with id n\n");
  	Abc_Print(-2, "\t-h    : print the command usage\n");
  	return 1;
}

// ---------------------------------------------------------------------------- //

vector<pair<bool,bool>> Lsv_findOdc(Abc_Ntk_t* pNtk, Abc_Obj_t* pNode) {
	Abc_Obj_t* y0 = Abc_ObjFanin0(pNode);
    Abc_Obj_t* y1 = Abc_ObjFanin1(pNode);
	int pol_y0 = Abc_ObjFaninC0(pNode);
	int pol_y1 = Abc_ObjFaninC1(pNode);

    set<Abc_Obj_t*> all_nodes;
    Abc_Obj_t* pObj;
    int i;
    Abc_NtkForEachNode(pNtk, pObj, i) {
        all_nodes.insert(pObj);
    }
    
    // SAT solver
    sat_solver* pSat = sat_solver_new();
    map<Abc_Obj_t*, int> node_to_var;      // circuit C
    map<Abc_Obj_t*, int> node_to_var_mod;  // circuit C'
    int var_count = 0;
    
    // create variables for all nodes
    Abc_NtkForEachPi(pNtk, pObj, i) {
        int var = ++var_count;
        node_to_var[pObj] = var;
        node_to_var_mod[pObj] = var;
    }
    
    for(auto node : all_nodes) {
        node_to_var[node] = ++var_count;
        node_to_var_mod[node] = ++var_count;
    }

    // create CNF for circuit C
    for(auto node : all_nodes) {
        if (!Abc_ObjIsCi(node)) {
            int out = node_to_var[node];
            int in1 = node_to_var[Abc_ObjFanin0(node)];
            int in2 = node_to_var[Abc_ObjFanin1(node)];
            
            int fCompl0 = Abc_ObjFaninC0(node);
            int fCompl1 = Abc_ObjFaninC1(node);
            
            lit Lits[3];
            
            // CNF for AND gate
            Lits[0] = toLitCond(out, 1);
            Lits[1] = toLitCond(in1, fCompl0);
            sat_solver_addclause(pSat, Lits, Lits + 2);
            
            Lits[0] = toLitCond(out, 1);
            Lits[1] = toLitCond(in2, fCompl1);
            sat_solver_addclause(pSat, Lits, Lits + 2);
            
            Lits[0] = toLitCond(out, 0);
            Lits[1] = toLitCond(in1, !fCompl0);
            Lits[2] = toLitCond(in2, !fCompl1);
            sat_solver_addclause(pSat, Lits, Lits + 3);
        }
    }
    
    // create CNF for circuit C'
    for(auto node : all_nodes) {
        if (!Abc_ObjIsCi(node)) {
            int out = node_to_var_mod[node];
            int in1 = node_to_var_mod[Abc_ObjFanin0(node)];
            int in2 = node_to_var_mod[Abc_ObjFanin1(node)];
            
            // if the node is pNode, invert its output
            bool isTargetNode = (node == pNode);
            
            int fCompl0 = Abc_ObjFaninC0(node);
            int fCompl1 = Abc_ObjFaninC1(node);
            
            lit Lits[3];
            
            // CNF for AND gate
            Lits[0] = toLitCond(out, isTargetNode ? 0 : 1);
            Lits[1] = toLitCond(in1, fCompl0);
            sat_solver_addclause(pSat, Lits, Lits + 2);
            
            Lits[0] = toLitCond(out, isTargetNode ? 0 : 1);
            Lits[1] = toLitCond(in2, fCompl1);
            sat_solver_addclause(pSat, Lits, Lits + 2);
            
            Lits[0] = toLitCond(out, isTargetNode ? 1 : 0);
            Lits[1] = toLitCond(in1, !fCompl0);
            Lits[2] = toLitCond(in2, !fCompl1);
            sat_solver_addclause(pSat, Lits, Lits + 3);
        }
    }
    
    // create XOR gates for each PO
    vector<int> xor_outputs;
    Abc_NtkForEachPo(pNtk, pObj, i) {
        int xor_out = ++var_count;
        xor_outputs.push_back(xor_out);
        
        int a = node_to_var[Abc_ObjFanin0(pObj)];
        int b = node_to_var_mod[Abc_ObjFanin0(pObj)];
        
        lit Lits[3];
        // XOR CNF
        Lits[0] = toLitCond(xor_out, 1);
        Lits[1] = toLitCond(a, 1);
        Lits[2] = toLitCond(b, 1);
        sat_solver_addclause(pSat, Lits, Lits + 3);
        
        Lits[0] = toLitCond(xor_out, 1);
        Lits[1] = toLitCond(a, 0);
        Lits[2] = toLitCond(b, 0);
        sat_solver_addclause(pSat, Lits, Lits + 3);
        
        Lits[0] = toLitCond(xor_out, 0);
        Lits[1] = toLitCond(a, 1);
        Lits[2] = toLitCond(b, 0);
        sat_solver_addclause(pSat, Lits, Lits + 3);
        
        Lits[0] = toLitCond(xor_out, 0);
        Lits[1] = toLitCond(a, 0);
        Lits[2] = toLitCond(b, 1);
        sat_solver_addclause(pSat, Lits, Lits + 3);
    }
    
    // create OR gate (miter output)
    int miter_out = ++var_count;
    
    // OR gate CNF
    for(int xor_out : xor_outputs) {
        lit Lits[2];
        Lits[0] = toLitCond(miter_out, 0);
        Lits[1] = toLitCond(xor_out, 1);
        sat_solver_addclause(pSat, Lits, Lits + 2);
    }
    
    lit Lits[xor_outputs.size() + 1];
    Lits[0] = toLitCond(miter_out, 1);
    for(size_t i = 0; i < xor_outputs.size(); i++) {
        Lits[i + 1] = toLitCond(xor_outputs[i], 0);
    }
    sat_solver_addclause(pSat, Lits, Lits + xor_outputs.size() + 1);

    // force miter output to be 1
    lit MiterLit[1];
    MiterLit[0] = toLitCond(miter_out, 0);  // miter_out must be 1
    sat_solver_addclause(pSat, MiterLit, MiterLit + 1);
    
    // solve ALLSAT
    vector<pair<bool,bool>> care_patterns;
	for (int i = 0; i < 4; i++) {
		if (sat_solver_solve(pSat, NULL, NULL, 0, 0, 0, 0) == l_False) {
            printf("no solution\n");
			break;
		}	
        printf("solution %d\n", i+1);
        bool val0 = sat_solver_var_value(pSat, node_to_var[y0]);
        bool val1 = sat_solver_var_value(pSat, node_to_var[y1]);
        care_patterns.push_back({val0, val1});

        // add clauses to avoid duplicate solutions
        lit Litss[2];
        Litss[0] = toLitCond(node_to_var[y0], val0);
        Litss[1] = toLitCond(node_to_var[y1], val1);
        sat_solver_addclause(pSat, Litss, Litss + 2);
	}

    // find ODC patterns
    vector<pair<bool,bool>> odc_patterns;
    vector<pair<bool,bool>> all_patterns = {{0,0},{0,1},{1,0},{1,1}};
    vector<pair<bool,bool>> sdc_patterns = Lsv_findSdc(pNtk, pNode);

    for(auto& pattern : care_patterns) {
        if (pol_y0) pattern.first = !pattern.first;
        if (pol_y1) pattern.second = !pattern.second;
    }
    
    for(auto pattern : all_patterns) {
        if(find(care_patterns.begin(), care_patterns.end(), pattern) == care_patterns.end() &&
           	find(sdc_patterns.begin(), sdc_patterns.end(), pattern) == sdc_patterns.end()) {
        	odc_patterns.push_back(pattern);
        }
    }
    
    sat_solver_delete(pSat);
    return odc_patterns;
}

int Lsv_CommandOdc(Abc_Frame_t* pAbc, int argc, char** argv) {
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    Abc_Obj_t* n;

    if (argc == 2) {
        if (strcmp(argv[1], "-h") == 0) {
            goto usage;
        }
        if (!pNtk) {
            Abc_Print(-1, "Empty network.\n");
            return 1;
        }
        int nodeId = atoi(argv[1]);
        if (nodeId >= Abc_NtkObjNum(pNtk)) {
            Abc_Print(-1, "Node id %d exceeds maximum node id %d\n", 
                      nodeId, Abc_NtkObjNum(pNtk) - 1);
            goto usage;
        }
        if (nodeId < 1) {
            Abc_Print(-1, "node id must be positive\n");
            goto usage;
        }
        n = Abc_NtkObj(pNtk, nodeId);
        if (Abc_ObjIsCi(n) || Abc_ObjIsCo(n)) {
            Abc_Print(-1, "please input internal node id\n");
            goto usage;
        }
        
        vector<pair<bool,bool>> patterns = Lsv_findOdc(pNtk, n);
        if (patterns.empty()) {
            Abc_Print(-2, "no odc\n");
        } else {
            for (auto pattern : patterns) {
                Abc_Print(-2, "%d%d\n", pattern.first, pattern.second);
            }
        }
        return 0;
    }

    Abc_Print(-1, "Missing or invalid argument for <n>.\n");

usage:
    Abc_Print(-2, "usage: lsv_odc <n> [h]\n");
    Abc_Print(-2, "\t        prints the odc of node n\n");
    Abc_Print(-2, "\t<n>   : node with id n\n");
    Abc_Print(-2, "\t-h    : print the command usage\n");
    return 1;
}