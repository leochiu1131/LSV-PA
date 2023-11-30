#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "bdd/cudd/cudd.h"
#include "bdd/cudd/cuddInt.h"
#include "sat/bsat/satSolver.h"
#include "sat/cnf/cnf.h"
#include "vector"
#include "string"
#include "queue"
#include "fstream"
#include "iostream"
#include "bitset"

extern "C"{
  Aig_Man_t* Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
}

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandBddSimulation(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandAigSimulation(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandBddSymmetry(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSatSymmetry(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSatAllSymmetry(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", Lsv_CommandBddSimulation, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig", Lsv_CommandAigSimulation, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_bdd", Lsv_CommandBddSymmetry, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_sat", Lsv_CommandSatSymmetry, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_all", Lsv_CommandSatAllSymmetry, 0);
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


int Lsv_BddSymmetry(Abc_Ntk_t *pNtk, int k, int i, int j){
  Abc_Obj_t *Po, *Pi, *pObj;
  // std::cout<<"test"<<std::endl;

  DdManager* dd = (DdManager *)pNtk->pManFunc;

  if(dd == NULL){
    Abc_Print(-1, "bdd not found!\n");
    return 0;
  }
  // Abc_NtkForEachPo(pNtk, Po, indi){
  Po = Abc_NtkPo(pNtk, k);
  Po = Abc_ObjFanin0(Po);
    // if(indi != k) continue;
  Vec_Ptr_t *NamesIn = Abc_NodeGetFaninNames(Po);
  char **name = (char**)NamesIn->pArray;
  DdNode *po = (DdNode*)Po->pData;
  // po = Cudd_Regular(po);
  cuddRef(po);


  // DdNode *cof = Cudd_Regular(po[0]);
  
  char* jName = Abc_ObjName(Abc_NtkPi(pNtk, j));//aig jth var name
  char* iName = Abc_ObjName(Abc_NtkPi(pNtk, i));//aig jth var name
  int bddi = -1;
  int bddj = -1;
  // std::cout<<Abc_NtkPiNum(pNtk)<<std::endl;
  for(int d = 0; d<Abc_ObjFaninNum(Po); d++){
    if(name[d] == NULL) break;
    std::string tempd(name[d]);
    std::string tempi(iName);
    std::string tempj(jName);
    
    // std::cout<<tempd<<" "<<tempi<<" "<<std::endl;
    if(tempd == tempj) {
      // std::cout<<"j "<<d<<std::endl;
      bddj = d;
    }
    if(tempd == tempi) {
      // std::cout<<"i "<<d<<std::endl;
      bddi = d;
    }
  }
  //find bdd index

  DdNode* xor_node = NULL;
  if(bddi != -1 & bddj != -1){
    DdNode* Ivar = Cudd_bddIthVar(dd, bddi);
    cuddRef(Ivar);
    DdNode* Jvar =  Cudd_bddIthVar(dd, bddj);
    cuddRef(Jvar);

    DdNode* B1Cof_temp = (bddi != -1) ? Cudd_Cofactor(dd, po, Ivar):po;
    cuddRef(B1Cof_temp);
    DdNode* B1Cof = (bddj != -1) ? Cudd_Cofactor(dd, B1Cof_temp, Cudd_Not(Jvar)):B1Cof_temp;
    cuddRef(B1Cof);
    DdNode* B2Cof_temp = (bddj != -1) ? Cudd_Cofactor(dd, po, Jvar):po;
    cuddRef(B2Cof_temp);
    DdNode* B2Cof = (bddi != -1) ? Cudd_Cofactor(dd, B2Cof_temp, Cudd_Not(Ivar)):B2Cof_temp;
    cuddRef(B2Cof);
    // std::cout<<(B1Cof == DD_ONE(dd))<<" "<<(B2Cof == DD_ONE(dd))<<std::endl;
    // B1Cof = Cudd_Regular(B1Cof);
    // B2Cof = Cudd_Regular(B2Cof);
    xor_node = Cudd_bddXor(dd, B1Cof, B2Cof);
    // xor_node = Cudd_Regular(xor_node);
    cuddRef(xor_node);
    Cudd_RecursiveDeref(dd, B2Cof_temp);
    Cudd_RecursiveDeref(dd, B1Cof_temp);
    Cudd_RecursiveDeref(dd, Ivar);
    Cudd_RecursiveDeref(dd, Jvar);
    Cudd_RecursiveDeref(dd, B1Cof);
    Cudd_RecursiveDeref(dd, B2Cof);
    
  }
  else if(bddi == -1 & bddj == -1){
    std::cout<<"symmetric"<<std::endl;
    return 0;
  }
  else if(bddi == -1){
    xor_node = Cudd_bddBooleanDiff(dd, po, bddj);
    cuddRef(xor_node);
  }
  else if(bddj == -1){
    xor_node = Cudd_bddBooleanDiff(dd, po, bddi);
    cuddRef(xor_node);
  }
  // xor_node = Cudd_Regular(xor_node);
  // std::cout<<xor_node->index<<std::endl;


  if(xor_node == Cudd_Not(DD_ONE(dd)) | xor_node == DD_ZERO(dd)){
    //output symmetry
    std::cout<<"symmetric"<<std::endl;
  }
  else{
    //output asymmetry 
    std::cout<<"asymmetric"<<std::endl;
    //output cex
    // std::cout<<name[Cudd_Regular(cuddE(B1Cof))->index]<<std::endl;
    // std::cout<<name[Cudd_Regular(cuddE(B2Cof))->index]<<std::endl;
    std::vector<std::string> cex_temp;
    std::vector<int> cex_index;
    // std::cout<<"xor success"<<std::endl;
    bool comple = Cudd_IsComplement(xor_node);
    while(true){
      // std::cout<<cuddIsConstant(xor_node)<<std::endl;
      xor_node = Cudd_Regular(xor_node);
      if(cuddIsConstant(xor_node)){
        break;
      }
      DdNode* E = cuddE(xor_node);
      DdNode* T = cuddT(xor_node);
      bool compleT = Cudd_IsComplement(T)? !comple:comple;
      bool compleE = Cudd_IsComplement(E)? !comple:comple;
      T = Cudd_Regular(T);
      E = Cudd_Regular(E);
      // std::cout<<comple<<std::endl;
      
      if((T == Cudd_ReadOne(dd) & !compleT) | (T == Cudd_ReadZero(dd) & compleT)){
        cex_temp.emplace_back("1");
        cex_index.emplace_back(xor_node->index);
        break;
      }
      else if((E == Cudd_ReadOne(dd) & !compleE) | (E == Cudd_ReadZero(dd) & compleE)){
        cex_temp.emplace_back("0");
        cex_index.emplace_back(xor_node->index);
        break;
      }
      else if(!cuddIsConstant(Cudd_Regular(T))){
        xor_node = T;
        comple = compleT;
        cex_temp.emplace_back("1");
        cex_index.emplace_back(xor_node->index);
      }
      else if(!cuddIsConstant(Cudd_Regular(E))){
        xor_node = E;
        comple = compleE;
        cex_temp.emplace_back("0");
        cex_index.emplace_back(xor_node->index);
      }
    
      
    }
    // std::cout<<"while end"<<std::endl;

    std::vector<std::string> cex1, cex2;
    for(int t = 0; t<Abc_NtkPiNum(pNtk); t++){
      bool exist = false;
      if(t == i){
        cex1.emplace_back("0");
        cex2.emplace_back("1");
        continue;
      }
      else if(t == j){
        cex1.emplace_back("1");
        cex2.emplace_back("0");
        continue;
      }
      for(int d = 0; d<Abc_ObjFaninNum(Po); d++){
        if(name[d] == NULL) break;
        std::string tempd(name[d]);
        std::string tempt(Abc_ObjName(Abc_NtkPi(pNtk, t)));
        if(tempd == tempt){
          bool deff0 = true;
          for(int p = 0; p<cex_temp.size(); p++){
            if(cex_index[p] == d){
              cex1.emplace_back(cex_temp[p]);
              cex2.emplace_back(cex_temp[p]);
              deff0 = false;
            }
          }
          if(deff0){
            cex1.emplace_back("0");
            cex2.emplace_back("0");
          }
          exist = true;
          break;
        }
      }
      if(!exist){
        cex1.emplace_back("0");
        cex2.emplace_back("0");
      }

    }
    // for(int t = 0; t<cex1.size(); t++){
    //   std::cout<<Abc_ObjName(Abc_NtkPi(pNtk, t))<<" ";
    // }
    // std::cout<<std::endl; 
    for(int t = 0; t<cex1.size(); t++){
      std::cout<<cex2[t];
    }
    std::cout<<std::endl; 
    for(int t = 0; t<cex1.size(); t++){
      std::cout<<cex1[t];
    }
    std::cout<<std::endl; 
    // Cudd_RecursiveDeref(dd, xor_node);
  }
  //deref
  
  // if(bddi != -1) Cudd_RecursiveDeref(dd, Ivar);
  // if(bddj != -1) Cudd_RecursiveDeref(dd, Jvar);
  Cudd_RecursiveDeref(dd, xor_node);
  Cudd_RecursiveDeref(dd, po);

  return 0;

}
int Lsv_CommandBddSymmetry(Abc_Frame_t* pAbc, int argc, char** argv){
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
  // std::cout<<argc<<std::endl;
  if(argc != 4) {
    printf("Input not declare.\n");
    return 1;
  }
  Lsv_BddSymmetry(pNtk, atoi(argv[1]), atoi(argv[2]), atoi(argv[3]));
  return 0;


  usage:
  Abc_Print(0, "test");
  return 0;

}


int Lsv_SatSymmetry(Abc_Ntk_t *pNtk, int k, int i, int j){
  Abc_Obj_t* Po = Abc_ObjFanin0(Abc_NtkPo(pNtk, k));
  Abc_Ntk_t* Yk =  Abc_NtkCreateCone(pNtk, Po, "yk", 1);
  Po = Abc_ObjFanin0(Abc_NtkPo(Yk, 0));
  Aig_Man_t* pAig =  Abc_NtkToDar(Yk, 0, 0);
  sat_solver * pSolver = sat_solver_new();
 
  //create miter
  Cnf_Dat_t* pCnf1 =  Cnf_Derive(pAig, 1);
  Cnf_DataWriteIntoSolverInt(pSolver,pCnf1, 1, 0);
  Cnf_Dat_t* pCnf2  = Cnf_Derive(pAig, 1);
  Cnf_DataLift(pCnf2, pCnf1->nVars);
  Cnf_DataWriteIntoSolverInt(pSolver,pCnf2, 1, 0);

  Abc_Obj_t* Pi;
  int t;
  lit literal[2];
  //vi1 = vi2
  Abc_NtkForEachPi(Yk, Pi, t){
    if(t == i || t == j) continue;
    int var1 = pCnf1->pVarNums[Pi->Id];
    int var2 = pCnf2->pVarNums[Pi->Id];

    literal[0] = toLitCond(var1, 1);
    literal[1] = toLitCond(var2, 0);
    sat_solver_addclause(pSolver, literal, literal+2);

    literal[0] = toLitCond(var1, 0);
    literal[1] = toLitCond(var2, 1);
    sat_solver_addclause(pSolver, literal, literal+2);
  }

  //symmetry
  int var1 = pCnf1->pVarNums[Abc_NtkPi(Yk, i)->Id];
  int var2 = pCnf2->pVarNums[Abc_NtkPi(Yk, j)->Id];
  // std::cout<<Abc_NtkPi(Yk, i)->Id<<" "<<Abc_NtkPi(pNtk, i)->Id<<std::endl;
  // std::cout<<Abc_NtkPi(Yk, j)->Id<<" "<<Abc_NtkPi(pNtk, j)->Id<<std::endl;

  literal[0] = toLitCond(var1, 1);
  literal[1] = toLitCond(var2, 0);
  sat_solver_addclause(pSolver, literal, literal+2);

  literal[0] = toLitCond(var1, 0);
  literal[1] = toLitCond(var2, 1);
  sat_solver_addclause(pSolver, literal, literal+2);

  var1 = pCnf1->pVarNums[Abc_NtkPi(Yk, j)->Id];
  var2 = pCnf2->pVarNums[Abc_NtkPi(Yk, i)->Id];

  literal[0] = toLitCond(var1, 1);
  literal[1] = toLitCond(var2, 0);
  sat_solver_addclause(pSolver, literal, literal+2);

  literal[0] = toLitCond(var1, 0);
  literal[1] = toLitCond(var2, 1);
  sat_solver_addclause(pSolver, literal, literal+2);

  //equi
  var1 = pCnf1->pVarNums[Po->Id];
  var2 = pCnf2->pVarNums[Po->Id];

  literal[0] = toLitCond(var1, 1);
  literal[1] = toLitCond(var2, 1);
  sat_solver_addclause(pSolver, literal, literal+2);

  literal[0] = toLitCond(var1, 0);
  literal[1] = toLitCond(var2, 0);
  sat_solver_addclause(pSolver, literal, literal+2);

  int status = sat_solver_solve(pSolver, NULL, NULL, 0, 0, 0 , 0);
  if(status == l_False){
    std::cout<<"symmetric"<<std::endl;
  }
  else{
    std::cout<<"asymmetric"<<std::endl;
    Abc_NtkForEachPi(Yk, Pi, t){
      std::cout<<sat_solver_var_value(pSolver, pCnf1->pVarNums[Pi->Id]);
    }
    std::cout<<std::endl;
    Abc_NtkForEachPi(Yk, Pi, t){
      std::cout<<sat_solver_var_value(pSolver, pCnf2->pVarNums[Pi->Id]);
    }
    std::cout<<std::endl;
    
  }
  return 1;

}
int Lsv_CommandSatSymmetry(Abc_Frame_t* pAbc, int argc, char** argv){
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
  if(argc != 4) {
    printf("Input not declare.\n");
    return 1;
  }


  // std::string k(argv[1]);
  // std::string i(argv[2]);
  // std::string j(argv[3]);
  Lsv_SatSymmetry(pNtk, atoi(argv[1]), atoi(argv[2]), atoi(argv[3]));
  return 0;


  usage:
  Abc_Print(0, "test");
  return 0;

}

int Lsv_SatAllSymmetry(Abc_Ntk_t *pNtk, int k){
  Abc_Obj_t* Po = Abc_ObjFanin0(Abc_NtkPo(pNtk, k));
  Abc_Ntk_t* Yk =  Abc_NtkCreateCone(pNtk, Po, "yk", 1);
  Po = Abc_ObjFanin0(Abc_NtkPo(Yk, 0));
  Aig_Man_t* pAig =  Abc_NtkToDar(Yk, 0, 0);
  sat_solver * pSolver = sat_solver_new();
 
  //create miter
  Cnf_Dat_t* pCnf1 =  Cnf_Derive(pAig, 1);
  Cnf_DataWriteIntoSolverInt(pSolver,pCnf1, 1, 0);
  Cnf_Dat_t* pCnf2  = Cnf_Derive(pAig, 1);
  Cnf_DataLift(pCnf2, pCnf1->nVars);
  Cnf_DataWriteIntoSolverInt(pSolver,pCnf2, 1, 0);

  lit literal[4];
  //output
  int var1 = pCnf1->pVarNums[Po->Id];
  int var2 = pCnf2->pVarNums[Po->Id];

  literal[0] = toLitCond(var1, 1);
  literal[1] = toLitCond(var2, 1);
  sat_solver_addclause(pSolver, literal, literal+2);

  literal[0] = toLitCond(var1, 0);
  literal[1] = toLitCond(var2, 0);
  sat_solver_addclause(pSolver, literal, literal+2);

  //equal clause
  Abc_Obj_t *Pi;
  int t;
  std::vector<int> equal_record;
  for(int i = 0; i<Abc_NtkPiNum(Yk); i++){
    int var1 = pCnf1->pVarNums[Abc_NtkPi(Yk, i)->Id];
    int var2 = pCnf2->pVarNums[Abc_NtkPi(Yk, i)->Id];
    int var3 = sat_solver_addvar(pSolver);
    equal_record.emplace_back(var3);
    // std::cout<<var3<<" ";

    literal[0] = toLitCond(var1, 1);
    literal[1] = toLitCond(var2, 0);
    literal[2] = toLitCond(var3, 0);
    sat_solver_addclause(pSolver, literal, literal+3);

    literal[0] = toLitCond(var1, 0);
    literal[1] = toLitCond(var2, 1);
    literal[2] = toLitCond(var3, 0);
    sat_solver_addclause(pSolver, literal, literal+3);
  }
  // std::cout<<std::endl;
  //symmetry clause
  // std::vector<std::vector<int> > symmetry_record;
  for(int i = 0; i<Abc_NtkPiNum(Yk); i++){
    // std::vector<int> temp;
    for(int j = i+1; j<Abc_NtkPiNum(Yk); j++){
      int var1 = pCnf1->pVarNums[Abc_NtkPi(Yk, j)->Id];
      int var2 = pCnf2->pVarNums[Abc_NtkPi(Yk, i)->Id];

      literal[0] = toLitCond(var1, 1);
      literal[1] = toLitCond(var2, 0);
      literal[2] = toLitCond(equal_record[i], 1);
      literal[3] = toLitCond(equal_record[j], 1);
      sat_solver_addclause(pSolver, literal, literal+4);

      literal[0] = toLitCond(var1, 0);
      literal[1] = toLitCond(var2, 1);
      literal[2] = toLitCond(equal_record[i], 1);
      literal[3] = toLitCond(equal_record[j], 1);
      sat_solver_addclause(pSolver, literal, literal+4);

      var1 = pCnf1->pVarNums[Abc_NtkPi(Yk, i)->Id];
      var2 = pCnf2->pVarNums[Abc_NtkPi(Yk, j)->Id];

      literal[0] = toLitCond(var1, 1);
      literal[1] = toLitCond(var2, 0);
      literal[2] = toLitCond(equal_record[i], 1);
      literal[3] = toLitCond(equal_record[j], 1);
      sat_solver_addclause(pSolver, literal, literal+4);

      literal[0] = toLitCond(var1, 0);
      literal[1] = toLitCond(var2, 1);
      literal[2] = toLitCond(equal_record[i], 1);
      literal[3] = toLitCond(equal_record[j], 1);
      sat_solver_addclause(pSolver, literal, literal+4);

    }
    // std::cout<<std::endl;
    // symmetry_record.emplace_back(temp);
  }

  //
  for(int i = 0; i<Abc_NtkPiNum(Yk); i++){
    for(int j = i+1; j<Abc_NtkPiNum(Yk); j++){
      //control signal add
      lit temp[2];
      
      temp[0] = toLit(equal_record[i]);
      temp[1] = toLit(equal_record[j]);
      int status = sat_solver_solve(pSolver, temp, temp+2, 0, 0, 0, 0);
      // std::cout<<status<<std::endl;
      if(status == l_False){
        std::cout<<i<<" "<<j<<std::endl;
      }
    }
  }
  return 0;
}

int Lsv_CommandSatAllSymmetry(Abc_Frame_t* pAbc, int argc, char** argv){
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


  // std::string k(argv[1]);
  // std::string i(argv[2]);
  // std::string j(argv[3]);
  Lsv_SatAllSymmetry(pNtk, atoi(argv[1]));
  return 0;


  usage:
  Abc_Print(0, "test");
  return 0;
}