#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"

#include "./lsvCut.h"

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
  return;
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

int find_index(int* id_to_index, int array_size, int node_id) {
  int index = -1;
  for(int i=array_size - 1; i>=0; i--){
    if(id_to_index[i] == node_id){
      index = i;
      break;
    }
  }
  return index;
}

// return index of the node
int Lsv_find_cut(Abc_Obj_t* pObj, const int k_feasible, Cut_t*** k_cut, int& kcut_size, int* id_to_index, int* num_cut_per_obj) {
  int nodeID = Abc_ObjId(pObj);
  int index = find_index(id_to_index, kcut_size, nodeID);
  printf("nodeID = %d, index = %d, kcut_size = %d\n", nodeID, index, kcut_size);
  if(index != -1){
    return index;
  }

  index = kcut_size;
  kcut_size++;
  id_to_index[index] = nodeID;
  printf("nodeID = %d, index = %d, kcut_size = %d\n", nodeID, index, kcut_size);

  int fanin_num = Abc_ObjFaninNum(pObj);
  printf("fanin_num = %d\n", fanin_num);
  if(fanin_num == 0){
    num_cut_per_obj[index] = 1;
    k_cut[index] = (Cut_t**)malloc(sizeof(Cut_t*) * 1);
    k_cut[index][0] = cut_new(&nodeID, 1);
    return index;
  }
  
  int cut_accumulate_size = 1;
  Cut_t** cut_accumulate = NULL;
  // cut_accumulate[0] = cut_new(&nodeID, 1);

  Abc_Obj_t* pFanin;
  int i;
  Abc_ObjForEachFanin(pObj, pFanin, i) {
    int faninID = (int)Abc_ObjId(pFanin);
    int fanin_index = Lsv_find_cut(pFanin, k_feasible, k_cut, kcut_size, id_to_index, num_cut_per_obj);
    printf("faninID = %d, fanin_index = %d\n", faninID, fanin_index);
    printf("num_cut_per_obj[fanin_index] = %d\n", num_cut_per_obj[fanin_index]);
    
    Cut_t** fanin_cut = k_cut[fanin_index];
    // printf("fanin_cut[0] = %p\n", fanin_cut[0]);
    // merge cut_accumulate and fanin_cut
    Cut_t** cut_temp = (Cut_t**)malloc(sizeof(Cut_t*) * (cut_accumulate_size * num_cut_per_obj[fanin_index] + 1));
    printf("cut_temp = %p\n", cut_temp);
    if(cut_accumulate != NULL){
      printf("cut_accumulate != NULL\n");
      for(int j=0; j<cut_accumulate_size; j++){
        for(int k=0; k<num_cut_per_obj[fanin_index]; k++){
          print_cut(cut_accumulate[j]);
          print_cut(fanin_cut[k]);
          cut_temp[j*num_cut_per_obj[fanin_index]+k] = cut_new(cut_accumulate[j], fanin_cut[k]);
        }
      }
      cuts_free(cut_accumulate, cut_accumulate_size);
    }
    else{
      for(int j=0; j<num_cut_per_obj[fanin_index]; j++){
        cut_temp[j] = cut_copy(fanin_cut[j]);
      }
    }
    cut_accumulate_size *= num_cut_per_obj[fanin_index];
    printf("cut_accumulate_size = %d\n", cut_accumulate_size);
    cut_sort_and_eliminate(cut_temp, cut_accumulate_size, k_feasible);
    cut_accumulate = cut_temp;
  }

  if(cut_accumulate == NULL){
    num_cut_per_obj[index] = 1;
    k_cut[index] = (Cut_t**)malloc(sizeof(Cut_t*) * 1);
    k_cut[index][0] = cut_new(&nodeID, 1);
    return index;
  }

  cut_accumulate[cut_accumulate_size++] = cut_new(&nodeID, 1);
  num_cut_per_obj[index] = cut_accumulate_size;
  k_cut[index] = cut_accumulate;
  return index;
}

int Lsv_NtkPrintCut(Abc_Ntk_t* pNtk, int k_feasible) {
  
  int NtkObjNum = Abc_NtkObjNum(pNtk);
  int kcut_size = 0;
  int* id_to_index = (int*)malloc(sizeof(int) * NtkObjNum);
  int* num_cut_per_obj = (int*)malloc(sizeof(int) * NtkObjNum);
  Cut_t*** k_cut = (Cut_t***)malloc(sizeof(Cut_t**) * NtkObjNum);
  
  Abc_Obj_t* pObj;
  int i;
  Abc_NtkForEachNode(pNtk, pObj, i) {
    int nodeID = Abc_ObjId(pObj);
    int index = Lsv_find_cut(pObj, k_feasible, k_cut, kcut_size, id_to_index, num_cut_per_obj);
    for(int j=0; j<num_cut_per_obj[index]; j++){
      printf("%d: ", nodeID);
      print_cut(k_cut[index][j]);
    }
  }

  for(int i=0; i<kcut_size; i++){
    cuts_free(k_cut[i], num_cut_per_obj[i]);
  }
  return 0;
}

// Parse the command line arguments and call the function to print the cuts
int Lsv_CommandPrintCut(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  
  if(argc != 2) {
    Abc_Print(-1, "Wrong number of arguments.\n");
    Abc_Print(-2, "usage: lsv_printcut <k> [-h]\n");
    Abc_Print(-2, "\t        prints all k-feasible cuts for all nodes in the network\n");
    Abc_Print(-2, "\t-h    : print the command usage\n");
    return 1;
  }
  if(strlen(argv[1]) != 1 || argv[1][0] < '3' || argv[1][0] > '6') {
    Abc_Print(-1, "Invalid k value.\n");
    Abc_Print(-2, "usage: lsv_printcut <k>\n");
    Abc_Print(-2, "\t        only accept 3<=k<=6\n");
    return 1;
  }

  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  
  int k_feasible = argv[1][0] - '0';
  Lsv_NtkPrintCut(pNtk, k_feasible);

  return 0;
}