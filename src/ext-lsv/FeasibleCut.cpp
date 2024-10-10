#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <iostream>
#include <unordered_map>
#include <set>
#include <vector>


static int Lsv_CommandKFeasibleCut(Abc_Frame_t* pAbc, int argc, char** argv);

void init_cut(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_CommandKFeasibleCut, 0);
}

void destroy_cut(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer_cut = {init_cut, destroy_cut};

struct CutRegistrationManager {
  CutRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer_cut); }
} lsvCutRegistrationManager;

// Define a type for cuts (set of node IDs)
typedef std::set<int> Cut;
typedef std::vector<Cut> Cuts;

// Define the Node struct which includes fanin, fanout, and node ID
struct Node {
    int id;                 // Node ID
    int out_size;
    int in_size;
    bool Isin;
    bool Isout;
    std::set<int> fanin;     // Set of fanin node IDs
    std::set<int> fanout;    // Set of fanout node IDs

    Cuts cuts;              // List of k-feasible cuts for the node

};

std::unordered_map<int, Node> nodeMap;

// The main data structure: maps each node ID to a Node struct
void addNode(int nodeID) {
        nodeMap[nodeID] = Node{nodeID};  // Create a new Node with the given nodeID
}

void addFanin(int nodeID, int faninID) {
    nodeMap[nodeID].fanin.insert(faninID);
}

void addFanout(int nodeID, int fanoutID) {
    nodeMap[nodeID].fanout.insert(fanoutID);
}

void addCut(int nodeID, const Cut& cut) {
    nodeMap[nodeID].cuts.push_back(cut);
}

void setIOnum (int nodeID, int in, int out){
    nodeMap[nodeID].in_size = in;
    nodeMap[nodeID].out_size = out;
    nodeMap[nodeID].Isin  = (in == 0) ? 1 : 0;
    nodeMap[nodeID].Isout  = (out == 0) ? 1 : 0;
}

size_t getNumberOfNodes() {
    return nodeMap.size();  // Returns the number of nodes in the map
}

void printCut(int nodeID) {
  auto it = nodeMap.find(nodeID);
  if (it != nodeMap.end()) {
    const Node& node = it->second;

    for (const auto& cut : node.cuts) {
      printf("%d: ", nodeID);
      for (int elem : cut) {
          std::cout << elem << " ";
      }
      std::cout << "\n";
    }
  }
}

void Lsv_NtkReadNodes(Abc_Ntk_t* pNtk) {
  Abc_Obj_t* pObj;
  int i;

  Abc_NtkForEachObj(pNtk, pObj, i) {

    addNode(i);
    Abc_Obj_t* pFanin;
    Abc_Obj_t* pFanout;
    int in;
    int out;

    Abc_ObjForEachFanin(pObj, pFanin, in) {
      addFanin(i,Abc_ObjId(pFanin));
    }
    
    Abc_ObjForEachFanout(pObj, pFanout, out) {
      addFanout(i,Abc_ObjId(pFanout));
    }

    setIOnum (i,  in,  out);

  }
}

void find_cuts(int k) {
  int graph_size = getNumberOfNodes();
  for(int i = 1 ; i <= graph_size ; i ++){
    auto it = nodeMap.find(i);
    if (it != nodeMap.end()) {
      const Node& node = it->second;
      if(node.Isin){
        addCut(i, {i});
      }
      else if(node.Isout){
        continue;
      }
      // intermideate
      else{ 
        if(node.out_size != 1){
          addCut(i, {i});
        }
        Cuts l_cut;
        Cuts r_cut;
        int left = 0;
        int right = 0;
        for (int faninID : node.fanin) {
            if(left == 0 && right == 0){
              left = faninID;
            }
            else{
              right = faninID;
            }
        }
        for (const auto& leftCut : nodeMap[left].cuts) {
            for (const auto& rightCut : nodeMap[right].cuts){
              std::set<int> temp; // Use a vector to hold combined elements
              temp.insert(leftCut.begin(), leftCut.end());
              temp.insert(rightCut.begin(), rightCut.end());
              // for (int elem : temp) {
              //     std::cout << elem << " ";
              // }
              // std::cout << "\n";
              if (size(temp) <= k){
                bool new_set = 1;
                for (const auto& prev_cuts : nodeMap[i].cuts){
                  if(prev_cuts == temp){
                    new_set = 0;
                  }
                }
                if(new_set) {addCut(i,temp);}
              }
            }
        }

        //s1.insert(s2.begin(), s2.end());
      }
    }
    printCut(i);
  }
}


int Lsv_CommandKFeasibleCut(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);

  int c;
  int k = 0;
  if (argc > 1) {
    k = atoi(argv[1]);
  }
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
    switch (c) {
      case 'h':
        goto usage;
      
      default:
        goto usage;
    }
  }
  if (k > 6 || k < 3) {
    Abc_Print(-1, " k should be 3 <= k <= 6 \n");
    return 0;
  }
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 0;
  }

  Lsv_NtkReadNodes(pNtk);
  find_cuts(k);

  return 0;

usage:
  Abc_Print(-2, "usage: lsv_print_gates [-h] [-k <value>]\n");
  Abc_Print(-2, "\t        prints the gates in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}
