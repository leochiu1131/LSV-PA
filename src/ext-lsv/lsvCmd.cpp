#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "bdd/cudd/cudd.h"
#include "bdd/cudd/cuddInt.h"
#include "vector"
#include "string"
#include "queue"
#include "fstream"
#include "iostream"
#include "bitset"

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandBddSimulation(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandAigSimulation(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", Lsv_CommandBddSimulation, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig", Lsv_CommandAigSimulation, 0);
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


int Lsv_BddSimulation(Abc_Ntk_t *pNtk, char *Input){

  Abc_Obj_t *Po, *Pi, *pObj;
  int i, j;
  char ** ppNamesIn, ** ppNamesOut;

  int fVerbose = 1;

  

  DdManager* dd = (DdManager *)pNtk->pManFunc;

  if(dd == NULL){
    Abc_Print(-1, "bdd not found!\n");
    return 0;
  }
  // ppNamesIn = Abc_NtkCollectCioNames( pNtk, 0 );
  // ppNamesOut = Abc_NtkCollectCioNames( pNtk, 1 );

  

  Abc_NtkForEachPo(pNtk, Po, i){
    // Abc_Obj_t *test = Abc_NtkFindNode(pNtk, "y1");
    Vec_Ptr_t *NamesIn = Abc_NodeGetFaninNames(Abc_ObjFanin0(Po));

    int j;
    char **name = (char**)NamesIn->pArray;
    // std::cout<<NamesIn->nSize<<std::endl;;
    // for(int i = 0; i<NamesIn->nSize; i++){
    //   if(name[i] == NULL) break;
    //   std::string s(name[i]);
    //   std::cout<<s<<std::endl;
    // }

    DdNode **po = ((DdNode**)&Abc_ObjFanin0(Po)->pData);
    // DdNode *cof = Cudd_Regular(po[0]);
    bool comp = Cudd_IsComplement(po[0]);
    DdNode *cof = Cudd_Regular(po[0]);
    // printf("%d, %d, %d, %d\n", cof->index, Cudd_ReadOne(dd)->index, Cudd_ReadZero(dd), comp);

    for(int i = 0; i<NamesIn->nSize; i++){
    // Abc_Obj_t *pObj = Abc_NtkPo(pNtk, dd->invperm[i]);
    
    // if(dd->perm[cof->index] >i) continue;
    if(cof == Cudd_ReadOne(dd) | cof == Cudd_ReadZero(dd))break;

    //find pi
    int t ,r;
    Abc_Obj_t *pi;
    Abc_NtkForEachPi(pNtk, pi, t){
      std::string s(name[cof->index]);
      if(Abc_ObjName(pi) == s) r = t;
    }
    // printf("%d\n", r);
    
    if (Input[r] == '1'){//input 1
      comp = Cudd_IsComplement(cuddT(cof)) ? !comp:comp;
      cof = cuddT(cof);
    }
    else{
      comp = Cudd_IsComplement(cuddE(cof))? !comp:comp;
      // printf("%d\n", Cudd_IsComplement(cuddE(cof)));
      cof = Cudd_Regular(cuddE(cof));
    }
    

    // printf("%d, %d, %d, %d\n", cof->index, Cudd_ReadOne(dd)->index, Cudd_ReadZero(dd), comp);
    
  }
  bool result = (cof == Cudd_ReadOne(dd)) ? 1:0;
  result = comp? !result:result;
  printf("%s: %d\n", Abc_ObjName(Po), result);

  }
  return 1;

  

}

int Lsv_CommandBddSimulation(Abc_Frame_t* pAbc, int argc, char** argv){
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
    printf("Empty network.\n");
    return 1;
  }
  if(argc != 2) {
    printf("Input not declare.\n");
    return 1;
  }
  
  Lsv_BddSimulation(pNtk, argv[1]);
  return 0;


  usage:
  Abc_Print(0, "test");
  return 0;
}

int Lsv_AigSimulation(Abc_Ntk_t *pNtk, char* FileName){
  Abc_Obj_t *pObj;
  int i, j;
  std::vector<std::string> Input;
  std::ifstream f;
  std::string buffer;
  Abc_NtkCleanMarkA(pNtk);
  

  f.open(FileName);
  while(getline(f, buffer)){
    Input.emplace_back(buffer);
  }
  std::vector<std::string> result(Abc_NtkCoNum(pNtk), "");

  //all pattern into pi node
  int pattern_size = Input.size();
  int counter = 0;
  while (pattern_size > 0){
    // std::cout<<"test"<<counter*32<<" "<<pattern_size<<std::endl;
    Abc_NtkForEachCi(pNtk, pObj, j){
        pObj->iTemp = 0;
        // std::cout<<p<<std::endl;
    }
    for(int i = counter*32; i<(pattern_size>32 ? counter*32+32:counter*32+pattern_size); i++){
      Abc_NtkForEachCi(pNtk, pObj, j){
        pObj->iTemp = pObj->iTemp << 1;
        pObj->iTemp = pObj->iTemp + Input[i][j] - '0';
        std::bitset<32>p(pObj->iTemp);
        // std::cout<<p<<std::endl;
      }
    }
    // Abc_NtkForEachCi(pNtk, pObj, j){
    //   std::bitset<32>p(pObj->iTemp);
    //   std::cout<<p<<std::endl;
    // }

    Abc_NtkForEachPi(pNtk, pObj, i){
      // std::bitset<32>p(pObj->iTemp);
      pObj->fMarkA = 1;
    }

    std::queue<Abc_Obj_t*> q;
    Abc_Obj_t *fanout;
    int j;
    Abc_NtkForEachCi(pNtk, pObj, i){
      Abc_ObjForEachFanout(pObj, fanout, j){
        q.push(fanout);
      }
    }

    while(!q.empty()){
      Abc_Obj_t *node = q.front();
      // printf("%s\n", Abc_ObjName(q.front()));
      if(node->fMarkA == 1) {
        // printf("%s\n", Abc_ObjName(q.front()));
        q.pop();
        continue;
      }
      if ( Abc_ObjFanin0(node)->fMarkA == 0 & Abc_ObjFaninNum(node) == 1){
        // printf("%s\n", Abc_ObjName(q.front()));
        if(Abc_ObjFanin0(node)->Type == AIG_OBJ_CONST1){
          Abc_ObjFanin0(node)->iTemp = ~0;
          std::bitset<32>p(Abc_ObjFanin0(node)->iTemp);
          // std::cout<<p<<std::endl;
        }
        else{
          q.push(node);
        }
        q.pop();
        continue;
      }
      if(Abc_ObjFaninNum(node) == 2){

        if((Abc_ObjFanin0(node)->fMarkA !=1 & Abc_ObjFanin0(node)->Type != AIG_CONST1) | Abc_ObjFanin1(node)->fMarkA !=1 & Abc_ObjFanin1(node)->Type != AIG_CONST1){
          q.push(node);
          q.pop();
          continue;
        }
      }


      if(Abc_ObjFaninNum(node) == 2){
        int temp0, temp1;
        if(Abc_ObjFanin0(node)->Type == AIG_CONST1){
          temp0 = ~0;
          std::bitset<32>p(temp0);
          // std::cout<<p<<std::endl;
        }
        else{
          temp0 = node->fCompl0 ? ~Abc_ObjFanin0(node)->iTemp:Abc_ObjFanin0(node)->iTemp;
        }
        if(Abc_ObjFanin1(node)->Type == AIG_CONST1){
          temp1 = ~0;
          std::bitset<32>p(temp1);
          // std::cout<<p<<std::endl;
        }
        else{
          temp1 = node->fCompl1 ? ~Abc_ObjFanin1(node)->iTemp:Abc_ObjFanin1(node)->iTemp;
        }
        node->iTemp = temp0 & temp1;
        // std::cout<<Abc_ObjName(node)<<" "<<temp0<<" "<<temp1<<" "<<node->iTemp<<std::endl;
        
      }
      else{
        int temp0 = node->fCompl0 ? ~Abc_ObjFanin0(node)->iTemp:Abc_ObjFanin0(node)->iTemp;
        node->iTemp = temp0;
        // std::cout<<Abc_ObjName(node)<<" "<<temp0<<" "<<Abc_ObjFaninNum(node)<<std::endl;
      }

      node->fMarkA = 1;

      Abc_ObjForEachFanout(node, fanout, i){
        if(fanout->fMarkA == 0) {
          q.push(fanout);
        }
      }
      
      q.pop();

    }
    Abc_NtkCleanMarkA(pNtk);
    int num = pattern_size>32 ? 32:pattern_size;
    // std::cout<<"end"<<pattern_size<<std::endl;
    Abc_NtkForEachCo(pNtk, pObj, j){
      std::bitset<32>p(pObj->iTemp);
      // std::cout<<Abc_ObjName(pObj)<<": ";
      for(int i = num-1; i>=0; i--){
        if(p[i] == 1){
          result[j] = result[j] + '1';
        }
        else {
          result[j] = result[j] + '0';
        }
        
      }
      // std::cout<<result[j]<<std::endl;
    }
    pattern_size = pattern_size - 32;
    counter++;

  }

  int num = Input.size();
  // std::cout<<result.size()<<std::endl;
  Abc_NtkForEachCo(pNtk, pObj, j){
    std::bitset<32>p(pObj->iTemp);
    std::cout<<Abc_ObjName(pObj)<<": ";
    for(int i = 0; i<num; i++){
      std::cout<<result[j][i];
    }
    std::cout<<std::endl;
  }
  return 1;


  

}

int Lsv_CommandAigSimulation(Abc_Frame_t* pAbc, int argc, char** argv){
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
    printf("Empty network.\n");
    return 1;
  }
  if(argc != 2) {
    printf("Input not declare.\n");
    return 1;
  }


  Lsv_AigSimulation(pNtk, argv[1]);
  return 0;


  usage:
  Abc_Print(0, "test");
  return 0;

}