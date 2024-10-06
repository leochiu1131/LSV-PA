#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include<set>
#include<unordered_map>
#include<map>
#include<vector>
#include<algorithm>
#include<iostream>

using std::vector;
using std::cout;
using std::cerr;

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);

static int Lsv_CommandKFeasibleCut(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_CommandKFeasibleCut, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;


// =========================================
//          Lsv_NtkPrintNodes
// =========================================


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


// =========================================
//          Lsv_CommandKFeasibleCut
// =========================================

void Lsv_PrintKFeasibleCut(Abc_Ntk_t* pNtk, int K_feasible_num);

// void TopSort(Abc_Ntk_t* pNtk, vector<int> &top_order);

// void DFS(Abc_Ntk_t* pNtk, Abc_Obj_t *curr_node_obj, vector<int> &top_order, vector<bool> &node_is_found);

int Lsv_CommandKFeasibleCut(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c;
  int K_feasible_num = 3;
  
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
  if (argc > 1)
    K_feasible_num = atoi(argv[1]);
  Lsv_PrintKFeasibleCut(pNtk, K_feasible_num);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_print_cut [-h]\n");
  Abc_Print(-2, "\t        prints all K feasible cut of all the nodes in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

void Lsv_PrintKFeasibleCut(Abc_Ntk_t* pNtk, int K_feasible_num){

    std::unordered_map<int, std::set<std::set<int> > > nodes_cut_map;
    // std::map<int, std::set<std::set<int> > > nodes_cut_map;
    int i;

    // PI
    Abc_Obj_t *pi_obj;

    Abc_NtkForEachPi( pNtk, pi_obj, i ){
        std::set<int> single_cut_set;
        single_cut_set.insert(Abc_ObjId(pi_obj));
        nodes_cut_map[Abc_ObjId(pi_obj)].insert(single_cut_set);
    }

    // inside nodes
    Abc_Obj_t *node_obj;
    // already be sorted as a topological order
    Abc_NtkForEachNode( pNtk, node_obj, i ){
        std::set<int> single_cut_set;
        
        // add itself first
        single_cut_set.insert(Abc_ObjId(node_obj));
        nodes_cut_map[Abc_ObjId(node_obj)].insert(single_cut_set);
        
        // calculate cross product of each children set
        Abc_Obj_t *fanin_obj1 = Abc_ObjFanin0(node_obj);
        Abc_Obj_t *fanin_obj2 = Abc_ObjFanin1(node_obj);
        for (auto &set1 : nodes_cut_map[Abc_ObjId(fanin_obj1)]){
            for (auto &set2 : nodes_cut_map[Abc_ObjId(fanin_obj2)]){
                single_cut_set = set1;
                for (auto &set_element : set2) single_cut_set.insert(set_element);
                // monitor the size of the cut
                if (single_cut_set.size() <= K_feasible_num)
                    nodes_cut_map[Abc_ObjId(node_obj)].insert(single_cut_set);
            }
        }

        // delete redundant cuts
        std::set<std::set<int>>::iterator cut_it;
        std::set<std::set<int>>::iterator cut_it2;
        // std::set<int>::iterator tmp_it;
        cut_it = nodes_cut_map[Abc_ObjId(node_obj)].begin();
        while ( cut_it != nodes_cut_map[Abc_ObjId(node_obj)].end()){
            bool cut_it_is_delete = false;
            cut_it2 = cut_it;
            cut_it2++; // next cut of the cut_it
            while (cut_it2 != nodes_cut_map[Abc_ObjId(node_obj)].end()){
                int size1 = cut_it->size();
                int size2 = cut_it2->size();
                // union of two cut
                // if the union of two cut is the same of any cut,
                // then delete the big size one
                std::set<int> union_cut;
                // tmp_it = std::set_union(cut_it->begin(), cut_it->end(), cut_it2->begin(), cut_it2->end(), union_cut.begin());
                std::set_union(cut_it->begin(), cut_it->end(), cut_it2->begin(), cut_it2->end(), std::inserter(union_cut, union_cut.begin()));
                
                // // debug
                // cerr << "set_union test\n";
                // cerr << "node " << Abc_ObjId(node_obj) << "\n";
                // cerr << "union: \n";
                // for (auto &dit : union_cut){
                //     cerr << dit << " ";
                // }
                // cerr << "\n";
                // cerr << "cut 1: \n";
                // for (auto &dit : *cut_it){
                //     cerr << dit << " ";
                // }
                // cerr << "\n";
                // cerr << "cut 2: \n";
                // for (auto &dit : *cut_it2){
                //     cerr << dit << " ";
                // }
                // cerr << "\n";
                // cerr << "\n";
                // cerr << "\n";
                // // end debug
                
                if (size1 <= size2 && union_cut == *cut_it2){
                    // // debug
                    // cerr << "delete1: node " << Abc_ObjId(node_obj) << "\n";
                    // for (auto &dit : union_cut){
                    //     cerr << dit << " ";
                    // }
                    // cerr << "\n";
                    // // end debug
                    cut_it2 = nodes_cut_map[Abc_ObjId(node_obj)].erase(cut_it2);
                }
                else if (union_cut == *cut_it) {
                    // // debug
                    // cerr << "in delete cut 1\n";
                    // // end debug
                    cut_it = nodes_cut_map[Abc_ObjId(node_obj)].erase(cut_it);
                    cut_it_is_delete = true;
                    break;
                }
                else{
                    cut_it2++;
                }
            }
            if (!cut_it_is_delete)
                cut_it++;
        }
    } // end Abc_NtkForEachNode

    // PO
    Abc_Obj_t *po_obj;
    Abc_NtkForEachPo( pNtk, po_obj, i ){
        bool two_fanin = false;
        std::set<int> single_cut_set;
        
        // calculate cross product of each children set
        Abc_Obj_t *fanin_obj1 = Abc_ObjFanin0(po_obj);
        Abc_Obj_t *fanin_obj2 = Abc_ObjFanin1(po_obj);
        // debug
        // cerr << "node " << Abc_ObjId(po_obj) << " fanin number:  " <<  Abc_ObjFaninNum(po_obj) << "\n";

        // wrong
        // if (!fanin_obj2){
        //     cerr << "In !fanin_obj2 \n";
        //     nodes_cut_map[Abc_ObjId(po_obj)] = nodes_cut_map[Abc_ObjId(fanin_obj1)];
        // }
        // else if (!fanin_obj1){
        //     cerr << "In !fanin_obj1 \n";
        //     nodes_cut_map[Abc_ObjId(po_obj)] = nodes_cut_map[Abc_ObjId(fanin_obj2)];
        // }
        if (Abc_ObjFaninNum(po_obj) == 1){
            // cerr << "In Abc_ObjFaninNum(po_obj) \n";
            nodes_cut_map[Abc_ObjId(po_obj)] = nodes_cut_map[Abc_ObjId(fanin_obj1)];
        }
        else{
            two_fanin = true;
            for (auto &set1 : nodes_cut_map[Abc_ObjId(fanin_obj1)]){
                for (auto &set2 : nodes_cut_map[Abc_ObjId(fanin_obj2)]){
                    single_cut_set = set1;
                    for (auto &set_element : set2) single_cut_set.insert(set_element);
                    // monitor the size of the cut
                    if (single_cut_set.size() <= K_feasible_num)
                        nodes_cut_map[Abc_ObjId(po_obj)].insert(single_cut_set);
                }
            }
        }

        // add itself
        single_cut_set.clear();
        single_cut_set.insert(Abc_ObjId(po_obj));
        nodes_cut_map[Abc_ObjId(po_obj)].insert(single_cut_set);
        
        // one fanin nodes don't have redundant cuts
        if (!two_fanin){
            continue;
        }
        
        // delete redundant cuts
        std::set<std::set<int>>::iterator cut_it;
        std::set<std::set<int>>::iterator cut_it2;
        cut_it = nodes_cut_map[Abc_ObjId(po_obj)].begin();
        while ( cut_it != nodes_cut_map[Abc_ObjId(po_obj)].end()){
            bool cut_it_is_delete = false;
            cut_it2 = cut_it;
            cut_it2++; // next cut of the cut_it
            while (cut_it2 != nodes_cut_map[Abc_ObjId(po_obj)].end()){
                int size1 = cut_it->size();
                int size2 = cut_it2->size();
                // union of two cut
                // if the union of two cut is the same of any cut,
                // then delete the big size one
                std::set<int> union_cut;
                std::set_union(cut_it->begin(), cut_it->end(), cut_it2->begin(), cut_it2->end(), std::inserter(union_cut, union_cut.begin()));
                
                if (size1 <= size2 && union_cut == *cut_it2){
                    cut_it2 = nodes_cut_map[Abc_ObjId(po_obj)].erase(cut_it2);
                }
                else if (union_cut == *cut_it) {
                    cut_it = nodes_cut_map[Abc_ObjId(po_obj)].erase(cut_it);
                    cut_it_is_delete = true;
                    break;
                }
                else{
                    cut_it2++;
                }
            }
            if (!cut_it_is_delete)
                cut_it++;
        }
    }
    
    // print all node cut
    for (auto &cuts : nodes_cut_map){
        for (auto &cut : cuts.second){
            std::cout << cuts.first << ": ";
            for (auto &cut_element : cut){
                std::cout << cut_element << " ";
            }
            std:: cout << std::endl;
        }
    }
}

