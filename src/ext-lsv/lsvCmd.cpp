#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "opt/sim/sim.h"
#include "sat/cnf/cnf.h"
#include <unordered_set>
#include <vector>
#include <algorithm>


extern "C"{
    Aig_Man_t* Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
}
// reference : Prof.Chang 's lecture note's Pseudocode
template <class T>
class node
{
private:
    int _rank;
    int _key;
    node<T> *_parent;

public:
    node() : _rank(0), _parent(NULL), _key(-1000){};
    node(int k) : _rank(0), _parent(NULL), _key(k){};
    ~node() { delete _parent; };
    inline int getRank() { return _rank; }
    inline void setRank(int r) { _rank = r; }
    inline node<T> *getParent() { return _parent; }
    inline void setParent(node<T> *n) { _parent = n; }
    inline int getKey() { return _key; }
    inline void setKey(int k) { _key = k; }
};

template <class T>
class dsForest
{
private:
    std::vector<node<T> *> _list;

public:
    dsForest(){};
    // dsForest(node<T> *n) { _list.push_back(n); }
    ~dsForest()
    {
        // for (auto i : _list)
        //     delete i;
        // _list.clear();
    }

    node<T> *getelement(int x) { return _list[x]; }

    inline void makeSet(int k)
    {
        node<T> *_n = new node<T>(k);
        _n->setParent(_n);
        _list.push_back(_n);
    }

    inline void Union(int x, int y)
    {
        Link(FindSet(_list[x]), FindSet(_list[y]));
    }
    inline void Link(node<T> *x, node<T> *y)
    {
        int xRank = x->getRank(), yRank = y->getRank();
        if (yRank > xRank)
            x->setParent(y);
        else
        {
            y->setParent(x);
            if (xRank == yRank)
                x->setRank(xRank + 1);
        }
    }
    inline node<T> *FindSet(node<T> *x)
    {
        if (x != x->getParent())
            x->setParent(FindSet(x->getParent()));
        return x->getParent();
    }
    inline int size(){
      return _list.size();
    }
};

static void print_binary(unsigned int number, int bits);
static int  myStrtol(char* num, int base);
static int readSimPattern(Abc_Ntk_t * pNtk ,Abc_Obj_t* pFanin, char ** argv);
static void myAddXorCNF(sat_solver * pSat, int vf, int va, int vb);
static int mySat_solver_add_buffer_enable( sat_solver * pSat, int iVarA, int iVarB, int iVarEn1, int iVarEn2);
static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Abc_CommandLSVAigSim( Abc_Frame_t * pAbc, int argc, char ** argv );
static int Abc_CommandLSVBDDSim( Abc_Frame_t * pAbc, int argc, char ** argv );
static int Abc_CommandLSVSymBDD( Abc_Frame_t * pAbc, int argc, char ** argv );
static int Abc_CommandLSVSymSat( Abc_Frame_t * pAbc, int argc, char ** argv );
static int Abc_CommandLSVSymAll( Abc_Frame_t * pAbc, int argc, char ** argv );

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd( pAbc, "LSV","lsv_sim_aig",Abc_CommandLSVAigSim,0 );
  Cmd_CommandAdd( pAbc, "LSV","lsv_sim_bdd",Abc_CommandLSVBDDSim,0 );
  Cmd_CommandAdd( pAbc, "LSV","lsv_sym_bdd",Abc_CommandLSVSymBDD,0 );
  Cmd_CommandAdd( pAbc, "LSV","lsv_sym_sat",Abc_CommandLSVSymSat,0 );
  Cmd_CommandAdd( pAbc, "LSV","lsv_sym_all",Abc_CommandLSVSymAll,0 );
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

/**Function*************************************************************

  Synopsis    [to implement parallel simulation using aig]

  Description [PA1 of LSV]

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_CommandLSVAigSim( Abc_Frame_t * pAbc, int argc, char ** argv ){
    struct aigSimPattern {
      unsigned _simPattern;
    };
    Abc_Ntk_t * pNtk = Abc_FrameReadNtk(pAbc);
    Sim_Man_t * p;

    p = Sim_ManStart( pNtk, 0 );
    if(argc != 2){
        printf("should have only one argument!\n");
        return 0;
    }
    else if(!Abc_NtkIsStrash(pNtk)){
        printf("this command can only be called after strashed.\n");
        return 0;
    }
    FILE* fin;

    fin = fopen(argv[1], "r");
    int piNum = Abc_NtkPiNum(pNtk) + 1;
    aigSimPattern simPatterns[Abc_NtkObjNum(p->pNtk) + 2];
    char pattern[piNum];
    Abc_Obj_t * pNode;
    int i; 
    printf("==========================================================\n");
    printf("======================= for LSV PA1 ======================\n");
    printf("==========================================================\n");
    while(true){
        printf("\n\n ======================= simulation start =======================\n");
        int isFirst = 1, bitCount = 0, tobreak = 1;
        while(fgets(pattern, piNum, fin)){
            if(pattern[0] == '\n')
                continue;
            unsigned* pSimInfo;
            // assign the random/systematic simulation info to the PIs
            Abc_NtkForEachCi( p->pNtk, pNode, i )
            {
                pSimInfo = &(simPatterns[pNode->Id]._simPattern);
                if(isFirst == 1)
                  *pSimInfo = (unsigned)0;
                *pSimInfo |= ((pattern[i] == '0' ? (unsigned)0 : (unsigned)1) << bitCount);
                // pSimInfo = (unsigned *)p->vSim0->pArray[pNode->Id];
                // for ( k = 0; k < p->nSimWords; k++ ){
                //     if(isFirst == 1)
                //         pSimInfo[k] = (unsigned)0;
                //     pSimInfo[k] |= ((pattern[i] == '0' ? (unsigned)0 : (unsigned)1) << bitCount);
                // }
            }
            isFirst = 0;
            ++bitCount;
            if(bitCount > 31){
                tobreak = 0;
                break;
            }
        }
        // Abc_NtkForEachPi( p->pNtk, pNode, i ){
        //   printf("%4s : ", Abc_ObjName(pNode));
        //   print_binary(simPatterns[pNode->Id]._simPattern, 32);
        //   printf("\n");
        // }
        Abc_NtkForEachNode( p->pNtk, pNode, i ){
            unsigned* psimNode = &(simPatterns[pNode->Id]._simPattern);
            unsigned* psimNode0 = &(simPatterns[Abc_ObjFaninId0(pNode)]._simPattern);
            unsigned* psimNode1 = &(simPatterns[Abc_ObjFaninId1(pNode)]._simPattern);
            *psimNode = (unsigned)0;
            
            // printf("id = %2d : ", pNode->Id);
            int compare0 = Abc_ObjFaninC0(pNode);   // == 0 -> positive input , == 1 -> negative input
            int compare1 = Abc_ObjFaninC1(pNode);
            // printf("compare0 = %d, compare1 = %d\n", compare0, compare1);
            
            unsigned inputPattern0 = compare0 == 0 ? *psimNode0 : (~(*psimNode0)); 
            unsigned inputPattern1 = compare1 == 0 ? *psimNode1 : (~(*psimNode1)); 
            *psimNode = inputPattern0 & inputPattern1;
        }
        Abc_NtkForEachCo( p->pNtk, pNode, i ){
            unsigned* psimNode = &(simPatterns[pNode->Id]._simPattern);
            unsigned* psimNode0 = &(simPatterns[Abc_ObjFaninId0(pNode)]._simPattern);
            *psimNode = (unsigned)0;
            // printf("id = %2d : ", pNode->Id);
            int compare0 = Abc_ObjFaninC0(pNode);   // == 0 -> positive input , == 1 -> negative input
            // printf("compare0 = %d\n", compare0);
            
            *psimNode = compare0 == 0 ? *psimNode0 : (~(*psimNode0)); 
        }


        // Abc_NtkForEachCi( p->pNtk, pNode, i ){
        //     // printf("%s = %u", Abc_ObjName(pNode), ((unsigned *)p->vSim0->pArray[pNode->Id])[0]);
        //     printf("%4s = ", Abc_ObjName(pNode));
        //     // printf("%10d   |   ", ((unsigned *)p->vSim0->pArray[pNode->Id])[0]);
        //     print_binary(((unsigned *)p->vSim0->pArray[pNode->Id])[0], bitCount);
        //     printf("\n");
        // }
        Abc_NtkForEachCo( p->pNtk, pNode, i ){
            printf("%10s: ", Abc_ObjName(pNode));
            print_binary(simPatterns[pNode->Id]._simPattern, bitCount);
            printf("\n");
        }
        printf("\n ======================= simulation end =======================\n\n");
        if(tobreak == 1)
            break;
    }
    printf("==========================================================\n");
    printf("=========================== END ==========================\n");
    printf("==========================================================\n");
    Sim_ManStop( p );
    return 0;
}
/**Function*************************************************************

  Synopsis    [to implement simulation using BDD]

  Description [PA1 of LSV]

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_CommandLSVBDDSim( Abc_Frame_t * pAbc, int argc, char ** argv ){
  Abc_Ntk_t * pNtk = Abc_FrameReadNtk(pAbc);
  Abc_Obj_t * pObj1;
  if(argc != 2){
      printf("should have only one argument!\n");
      return 0;
  }
  // DdManager * dd = (DdManager *)pNtk->pManFunc;
  // printf("%s\n", argv[1]);
  int i;
  Abc_NtkForEachPo(pNtk, pObj1, i) {
    Abc_Obj_t* pRoot = Abc_ObjFanin0(pObj1); 
    // char** vNamesIn = (char**) Abc_NodeGetFaninNames(pRoot)->pArray;
    // printf("==================fanin===============\n");
    // for(int kk = 0; kk < Abc_ObjFaninNum(pRoot); ++kk)
    //   printf("%s\n", vNamesIn[kk]);
    // printf("=================================\n");
    // printf("=============  PI  ====================\n");
    // Abc_Obj_t* pTemp;
    // int kk;
    // Abc_NtkForEachPi(pNtk, pTemp, kk)
    //   printf("%s\n", Abc_ObjName(pTemp));
    // printf("=================================\n");

    assert( Abc_NtkIsBddLogic(pRoot->pNtk) );
    DdManager * dd = (DdManager *)pRoot->pNtk->pManFunc;  
    DdNode* ddnode = (DdNode *)pRoot->pData;

    Abc_Obj_t* pFanin;
    int j;
    Abc_ObjForEachFanin(pRoot, pFanin, j){
        int comple = readSimPattern(pNtk, pFanin, argv);
        assert(comple != -1);
        ddnode = Cudd_Cofactor(dd, ddnode, Cudd_NotCond(dd->vars[j], comple)); 
        if(Cudd_IsConstant(ddnode))
            break;
    }
    if(!Cudd_IsConstant(ddnode)){
    // if(!cuddIsConstant(ddnode)){
      printf("ERROR!!!!!after cofactor the ddnode should be constant!\n");
      return 0;
    }
    printf("%4s: %d\n", Abc_ObjName(pObj1), (ddnode == DD_ONE(dd)));
    // assert();

      // Cudd_Cofactor(dd, ddnode, ((DdNode *)pObj1->pData));
  }
  // Abc_NtkForEachPi( pNtk, pObj1, i ){
  //   Abc_Obj_t * pObj2;
  //   int j;
  //   // Abc_NtkForEachPo( pNtk, pObj2, j ){
  //   //   Cudd_Cofactor(dd, (DdNode *)pObj2->pData, Cudd_Not((DdNode *)pObj1->pData));
  //   // }
  // }

  return 0;

}
/**Function*************************************************************

  Synopsis    [determine whether an output is symmetric under two given inputs by BDD]

  Description [PA2 of LSV]

  SideEffects []

  Usage [lsv_sym_bdd <output index> <input1 index> <input2 index>]
  
  SeeAlso     []

***********************************************************************/
int Abc_CommandLSVSymBDD( Abc_Frame_t * pAbc, int argc, char ** argv ){
  Abc_Ntk_t * pNtk = Abc_FrameReadNtk(pAbc);
  // Abc_Obj_t * pObj1;
  if(argc != 4){
      printf("should have only 3 argument!\n");
      return 0;
  }
  if(!Abc_NtkIsBddLogic(pNtk)){
    printf("Run \"collapse\" before using this command!\n");
    return 0;
  }
  //output index, input 1 index, input 2 index
  int oId = myStrtol(argv[1], 10), i1Id = myStrtol(argv[2], 10), i2Id = myStrtol(argv[3], 10);
  if(oId == -1 || i1Id == -1 || i2Id == -1){
    printf("parameters should all be positive integer!\n");
    return 0;
  }
  if(i1Id == i2Id){
    printf("symmetric\n");
    return 0;
  }
  if(oId >= Abc_NtkPoNum(pNtk)){
    printf("output index should <= %d\n", Abc_NtkPoNum(pNtk) - 1);
    return 0;
  }
  if(i1Id >= Abc_NtkPiNum(pNtk) || i2Id >= Abc_NtkPiNum(pNtk)){
    printf("input index should <= %d\n", Abc_NtkPiNum(pNtk) - 1);
    return 0;
  }

  Abc_Obj_t* pRoot = Abc_ObjFanin0(Abc_NtkCo(pNtk, oId));
  char* cex1 = new char[Abc_NtkCiNum(pNtk)], *cex2 = new char[Abc_NtkCiNum(pNtk)];
  // std::unordered_map<int, int> obj2bdd;
  int* obj2bdd;
  obj2bdd = new int[Abc_NtkCiNum(pNtk)];
  for(int i = 0, n = Abc_NtkCiNum(pNtk); i < n; ++i)
    obj2bdd[i] = -1;
  int origId1 = i1Id, origId2 = i2Id;
  char** vNamesIn = (char**) Abc_NodeGetFaninNames(pRoot)->pArray;
  for(int i = 0, ni = Abc_ObjFaninNum(pRoot); i < ni; ++i){
    for(int j = 0, nj = Abc_NtkCiNum(pNtk); j < nj; ++j){
      if(strcmp(Abc_ObjName(Abc_NtkCi(pNtk, j)), vNamesIn[i]) == 0){
        obj2bdd[j] = i;
        break;
      }
    }
  }
  /*
  printf("logic : \n");
  for(int i = 0, ni = Abc_NtkCiNum(pNtk); i < ni; ++i){
    printf("%s, ", Abc_ObjName(Abc_NtkCi(pNtk, i)));
  }
  printf("\nbdd : \n");
  for(int i = 0, ni = Abc_ObjFaninNum(pRoot); i < ni; ++i){
    printf("%s, ", vNamesIn[i]);
  }
  printf("\n");
  for(int i = 0, n = Abc_NtkCiNum(pNtk); i < n; ++i){
    if(obj2bdd[i] != -1)
      printf("obj = %d, bdd = %d\n", i, obj2bdd[i]);
  }
  // for(auto it=obj2bdd.begin();it!=obj2bdd.end();it++) {
  //   printf("obj = %d, bdd = %d \n", it->first, it->second);
  // }
  */
  int first1 = 1, first2 = 1;
  for(int i = 0, n = Abc_ObjFaninNum(pRoot); i < n; ++i){
    if(first1 == 1 && strcmp(Abc_ObjName(Abc_NtkCi(pNtk, i1Id)), vNamesIn[i]) == 0){
      i1Id = i;
      first1 = 0;
    }
    else if(first2 == 1 && strcmp(Abc_ObjName(Abc_NtkCi(pNtk, i2Id)), vNamesIn[i]) == 0){
      i2Id = i;
      first2 = 0;
    }
  }
  // printf("first1 = %d, %s; first2 = %d, %s\n", first1, Abc_ObjName(Abc_NtkCi(pNtk, origId1)), first2, Abc_ObjName(Abc_NtkCi(pNtk, origId2)));
  if(first1 && first2){
    printf("symmetric\n");
    return 0;
  }
  DdManager * dd = (DdManager *)pNtk->pManFunc;
  DdNode* ddfnotx1, *ddfx1, *ddfnotx2, *ddfx2, *ddfnotx1x2, *ddfx1notx2;
  DdNode* ddInput1, *ddInput2;
  DdNode* ddOutput = (DdNode *)pRoot->pData; Cudd_Ref(ddOutput);
  ddInput1 = Cudd_bddIthVar(dd, i1Id); Cudd_Ref(ddInput1);
  ddInput2 = Cudd_bddIthVar(dd, i2Id); Cudd_Ref(ddInput2);
  int isSymm = -1, type = -1;
  if(first2 == 1 && first1 == 0){
    ddfx1    = Cudd_Cofactor(dd, ddOutput, ddInput1); Cudd_Ref(ddfx1);
    ddfnotx1 = Cudd_Cofactor(dd, ddOutput, Cudd_Not(ddInput1)); Cudd_Ref(ddfnotx1);
    isSymm = (ddfx1 == ddfnotx1) ? 1 : 0;
    type = 1;
    
  }
  else if(first1 == 1 && first2 == 0){
    ddfx2    = Cudd_Cofactor(dd, ddOutput, ddInput2); Cudd_Ref(ddfx2);
    ddfnotx2 = Cudd_Cofactor(dd, ddOutput, Cudd_Not(ddInput2)); Cudd_Ref(ddfnotx2);
    isSymm = (ddfx2 == ddfnotx2) ? 1 : 0;
    type = 2;

    
  }
  else{
    ddfnotx1x2 = Cudd_Cofactor(dd, ddOutput, Cudd_Not(ddInput1)); Cudd_Ref(ddfnotx1x2);
    ddfnotx1x2 = Cudd_Cofactor(dd, ddfnotx1x2, ddInput2);

    ddfx1notx2 = Cudd_Cofactor(dd, ddOutput, ddInput1); Cudd_Ref(ddfx1notx2);
    ddfx1notx2 = Cudd_Cofactor(dd, ddfx1notx2, Cudd_Not(ddInput2));
    isSymm = (ddfnotx1x2 == ddfx1notx2) ? 1 : 0;
    type = 3;

    
  }
  // printf("type = %d\n", type);
  if(isSymm == -1 || isSymm == 1)
    printf("symmetric\n");
  else{
    printf("asymmetric\n");
    DdNode* tmp1, *tmp2;
    // char cex1[Abc_ObjFaninNum(pRoot)], cex2[Abc_ObjFaninNum(pRoot)];
    for(int i = 0, n = Abc_NtkCiNum(pNtk); i < n; ++i){
        cex1[i] = '0'; cex2[i] = '0';}
    if(type == 1){
      tmp1 = ddfnotx1; tmp2 = ddfx1;
      // cex1[origId1] = '1'; cex2[origId1] = '0';
    }
    else if(type == 2){
      tmp1 = ddfnotx2; tmp2 = ddfx2;
      // cex1[origId2] = '1'; cex2[origId2] = '0';
    }
    else{
      tmp1 = ddfnotx1x2; tmp2 = ddfx1notx2;
    }
    // Cudd_Ref(tmp1); Cudd_Ref(tmp2);
    cex1[origId1] = '0'; cex1[origId2] = '1';
    cex2[origId1] = '1'; cex2[origId2] = '0';
    for(int i = 0, ni = Abc_NtkCiNum(pNtk); i < ni; ++i){
      if(obj2bdd[i] == -1 || (type == 1 && i == origId1) || (type == 2 && i == origId2) || (type == 3 && i == origId1) || (type == 3 && i == origId2))
        continue;
      DdNode* curVar = Cudd_bddIthVar(dd, obj2bdd[i]); Cudd_Ref(curVar);
      DdNode* tmp3 = Cudd_Cofactor(dd, tmp1, curVar); Cudd_Ref(tmp3);
      DdNode* tmp4 = Cudd_Cofactor(dd, tmp2, curVar); Cudd_Ref(tmp4);
      // if(Cudd_Cofactor(dd, tmp1, curVar) == Cudd_Cofactor(dd, tmp2, curVar)){
      if(tmp3 == tmp4){
        Cudd_RecursiveDeref(dd, tmp3);
        Cudd_RecursiveDeref(dd, tmp4);
        tmp3 = Cudd_Cofactor(dd, tmp1, Cudd_Not(curVar)); Cudd_Ref(tmp1);
        tmp4 = Cudd_Cofactor(dd, tmp2, Cudd_Not(curVar)); Cudd_Ref(tmp2);
        Cudd_RecursiveDeref(dd, tmp1);
        Cudd_RecursiveDeref(dd, tmp2);
        tmp1 = tmp3;
        tmp2 = tmp4;
        // tmp1 = Cudd_Cofactor(dd, tmp1, Cudd_Not(curVar)); Cudd_Ref(tmp1);
        // tmp2 = Cudd_Cofactor(dd, tmp2, Cudd_Not(curVar)); Cudd_Ref(tmp2);
        cex1[i] = '0'; cex2[i] = '0';
      }
      else{
        Cudd_RecursiveDeref(dd, tmp1);
        Cudd_RecursiveDeref(dd, tmp2);
        tmp1 = tmp3;
        tmp2 = tmp4;
        // tmp1 = Cudd_Cofactor(dd, tmp1, curVar); Cudd_Ref(tmp1);
        // tmp2 = Cudd_Cofactor(dd, tmp2, curVar); Cudd_Ref(tmp2);
        cex1[i] = '1'; cex2[i] = '1';
      }
      Cudd_RecursiveDeref(dd, curVar);
    }
    Cudd_RecursiveDeref(dd, tmp1);
    Cudd_RecursiveDeref(dd, tmp2);
    for(int i = 0, ni = Abc_NtkCiNum(pNtk); i < ni; ++i)
      printf("%c", cex1[i]);
    printf("\n");
    for(int i = 0, ni = Abc_NtkCiNum(pNtk); i < ni; ++i)
      printf("%c", cex2[i]);
    printf("\n");
  }
  switch(type){
    case 1:
      Cudd_RecursiveDeref(dd, ddfx1);
      Cudd_RecursiveDeref(dd, ddfnotx1);
      break;
    case 2:
      Cudd_RecursiveDeref(dd, ddfx2);
      Cudd_RecursiveDeref(dd, ddfnotx2);
      break;
    case 3:
      Cudd_RecursiveDeref(dd, ddfnotx1x2);
      Cudd_RecursiveDeref(dd, ddfx1notx2);
      break;
    default:
      return 0;
  }

  Cudd_RecursiveDeref(dd, ddInput1);
  Cudd_RecursiveDeref(dd, ddInput2);
  Cudd_RecursiveDeref(dd, ddOutput);
  delete []cex1;
  delete []cex2;
  delete []obj2bdd;
  return 0;

}
/**Function*************************************************************

  Synopsis    [determine whether an output is symmetric under two given inputs by sat solver]

  Description [PA2 of LSV]

  SideEffects []

  Usage       [lsv_sym_sat <output index> <input1 index> <input2 index>]
  
  SeeAlso     []

***********************************************************************/
int Abc_CommandLSVSymSat( Abc_Frame_t * pAbc, int argc, char ** argv ){
  Abc_Ntk_t * pNtk = Abc_FrameReadNtk(pAbc);
  // Abc_Obj_t * pObj1;
  if(argc != 4){
      printf("should have only 3 argument!\n");
      return 0;
  }
  if(!Abc_NtkIsStrash(pNtk)){
    printf("Run \"strash\" before using this command!\n");
    return 0;
  }
  //output index, input 1 index, input 2 index
  int oId = myStrtol(argv[1], 10), i1Id = myStrtol(argv[2], 10), i2Id = myStrtol(argv[3], 10);
  if(oId == -1 || i1Id == -1 || i2Id == -1){
    printf("parameters should all be positive integer!\n");
    return 0;
  }
  
  if(oId >= Abc_NtkPoNum(pNtk)){
    printf("output index should <= %d\n", Abc_NtkPoNum(pNtk) - 1);
    return 0;
  }
  if(i1Id >= Abc_NtkPiNum(pNtk) || i2Id >= Abc_NtkPiNum(pNtk)){
    printf("input index should <= %d\n", Abc_NtkPiNum(pNtk) - 1);
    return 0;
  }
  if(i1Id == i2Id){
    printf("symmetric\n");
    return 0;
  }
  Abc_Ntk_t* coneNtk = Abc_NtkCreateCone(pNtk, Abc_ObjFanin0(Abc_NtkCo(pNtk, oId)), "output", 1);
  Aig_Man_t * pAig;
  pAig = Abc_NtkToDar( coneNtk, 0, 1 );
  Cnf_Dat_t* pCnf1 = Cnf_Derive( pAig, Aig_ManCoNum(pAig) );
  Cnf_Dat_t* pCnf2 = Cnf_Derive( pAig, Aig_ManCoNum(pAig) );
  Cnf_DataLift(pCnf2, pCnf2->nVars);
  sat_solver * pSat;
  pSat = sat_solver_new();
  sat_solver_setnvars( pSat, pCnf1->nVars + pCnf2->nVars );
  for (int i = 0; i < pCnf1->nClauses; i++ )
  {
      if ( !sat_solver_addclause( pSat, pCnf1->pClauses[i], pCnf1->pClauses[i+1] ) )
      {
        Cnf_DataFree( pCnf1 );
        Cnf_DataFree( pCnf2 );
        sat_solver_delete( pSat );
        Aig_ManStop( pAig );
        return 0;
      }
  }

  for ( int i = 0; i < pCnf2->nClauses; i++ )
  {
        if ( !sat_solver_addclause( pSat, pCnf2->pClauses[i], pCnf2->pClauses[i+1] ) )
        {
          Cnf_DataFree( pCnf1 );
          Cnf_DataFree( pCnf2 );
          sat_solver_delete( pSat );
          Aig_ManStop( pAig );
          return 0;
        }
  }
  // pSat = (sat_solver*)Cnf_DataWriteIntoSolver( pCnf2, 1, 0 );
  int* ctrlVars, *Lits;
  ctrlVars = new int[Aig_ManCiNum(pAig)];
  Lits = new int[Aig_ManCiNum(pAig) + 1];
  for(int i = 0, n = Aig_ManCiNum(pAig); i < n; ++i){
    ctrlVars[i] = sat_solver_addvar(pSat);
    Lits[i] = toLit(ctrlVars[i]);
  }
  int vf = sat_solver_addvar(pSat);
  Lits[Aig_ManCiNum(pAig)] = toLit(vf);
  Aig_Obj_t * pObj;
  int i;
  Aig_ManForEachCi( pAig, pObj, i ){
    if(i == i1Id)
      sat_solver_add_buffer_enable(pSat, pCnf1->pVarNums[Aig_ManCi(pAig, i1Id)->Id], pCnf2->pVarNums[Aig_ManCi(pAig, i2Id)->Id], ctrlVars[i], 0);
    else if(i == i2Id)
      sat_solver_add_buffer_enable(pSat, pCnf1->pVarNums[Aig_ManCi(pAig, i2Id)->Id], pCnf2->pVarNums[Aig_ManCi(pAig, i1Id)->Id], ctrlVars[i], 0);
    else
      sat_solver_add_buffer_enable(pSat, pCnf1->pVarNums[pObj->Id], pCnf2->pVarNums[pObj->Id], ctrlVars[i], 0);
  }
  myAddXorCNF(pSat, vf, pCnf1->pVarNums[Aig_ManCo(pAig, 0)->Id], pCnf2->pVarNums[Aig_ManCo(pAig, 0)->Id]);
  // sat_solver_add_buffer_enable(pSat, pCnf1->pVarNums[Aig_ManCo(pAig, 0)->Id], pCnf2->pVarNums[Aig_ManCo(pAig, 0)->Id], ctrlVars[i],1);
  int status = sat_solver_solve(pSat, Lits, Lits+ Aig_ManCiNum(pAig) + 1, 100000000, 0,0,0);
  if(status == l_False){
    printf("symmetric\n");
  }
  else{
    char* cex1, *cex2;
    cex1 = new char[Aig_ManCiNum(pAig)];
    cex2 = new char[Aig_ManCiNum(pAig)];
    for(int j = 0, nj = Aig_ManCiNum(pAig); j < nj; ++j){
        cex1[j] = sat_solver_var_value( pSat, pCnf1->pVarNums[Aig_ManCi(pAig, j)->Id]) == 1 ? '1' : '0';
        cex2[j] = sat_solver_var_value( pSat, pCnf2->pVarNums[Aig_ManCi(pAig, j)->Id]) == 1 ? '1' : '0';
    }
    printf("asymmetric\n");
    for(int j = 0, nj = Aig_ManCiNum(pAig); j < nj; ++j)
      printf("%c", cex1[j]);
    printf("\n");
    for(int j = 0, nj = Aig_ManCiNum(pAig); j < nj; ++j)
      printf("%c", cex2[j]);
    printf("\n");
    delete []cex1;
    delete []cex2;
  }
  delete []ctrlVars;
  Cnf_DataFree( pCnf1 );
  Cnf_DataFree( pCnf2 );
  sat_solver_delete( pSat );
  Aig_ManStop( pAig );

  return 0;
}

/**Function*************************************************************

  Synopsis    [derive all symmetric pairs of a given output]

  Description [PA2 of LSV]

  SideEffects []

  Usage       [lsv_sym_all <output index>]
  
  SeeAlso     []

***********************************************************************/
int Abc_CommandLSVSymAll( Abc_Frame_t * pAbc, int argc, char ** argv ){
  Abc_Ntk_t * pNtk = Abc_FrameReadNtk(pAbc);
  // Abc_Obj_t * pObj1;
  if(argc != 2){
      printf("should have only 1 argument!\n");
      return 0;
  }
  if(!Abc_NtkIsStrash(pNtk)){
    printf("Run \"strash\" before using this command!\n");
    return 0;
  }
  //output index, input 1 index, input 2 index
  int oId = myStrtol(argv[1], 10);
  if(oId == -1){
    printf("parameters should all be positive integer!\n");
    return 0;
  }
  if(oId >= Abc_NtkPoNum(pNtk)){
    printf("output index should <= %d\n", Abc_NtkPoNum(pNtk) - 1);
    return 0;
  }
  Abc_Ntk_t* coneNtk = Abc_NtkCreateCone(pNtk, Abc_ObjFanin0(Abc_NtkCo(pNtk, oId)), "output", 1);
  Aig_Man_t* pAig;
  pAig = Abc_NtkToDar( coneNtk, 0, 1 );
  Cnf_Dat_t* pCnf1 = Cnf_Derive( pAig, Aig_ManCoNum(pAig) );
  Cnf_Dat_t* pCnf2 = Cnf_Derive( pAig, Aig_ManCoNum(pAig) );
  Cnf_DataLift(pCnf2, pCnf2->nVars);
  sat_solver* pSat;
  pSat = sat_solver_new();
  sat_solver_setnvars( pSat, pCnf1->nVars + pCnf2->nVars );
  for (int i = 0; i < pCnf1->nClauses; i++ )
  {
      if ( !sat_solver_addclause( pSat, pCnf1->pClauses[i], pCnf1->pClauses[i+1] ) )
      {
        Cnf_DataFree( pCnf1 );
        Cnf_DataFree( pCnf2 );
        sat_solver_delete( pSat );
        Aig_ManStop( pAig );
        return 0;
      }
  }

  for ( int i = 0; i < pCnf2->nClauses; i++ )
  {
        if ( !sat_solver_addclause( pSat, pCnf2->pClauses[i], pCnf2->pClauses[i+1] ) )
        {
          Cnf_DataFree( pCnf1 );
          Cnf_DataFree( pCnf2 );
          sat_solver_delete( pSat );
          Aig_ManStop( pAig );
          return 0;
        }
  }
  int* ctrlVars, *assumps;
  ctrlVars = new int[Aig_ManCiNum(pAig)];
  assumps = new int[Aig_ManCiNum(pAig) + 5];
  for(int i = 0, n = Aig_ManCiNum(pAig); i < n; ++i){
    ctrlVars[i] = sat_solver_addvar(pSat);
    assumps[i] = toLit(ctrlVars[i]);
  }
  int vf = sat_solver_addvar(pSat);
  assumps[Aig_ManCiNum(pAig)] = toLit(vf);
  myAddXorCNF(pSat, vf, pCnf1->pVarNums[Aig_ManCo(pAig, 0)->Id], pCnf2->pVarNums[Aig_ManCo(pAig, 0)->Id]);
  Aig_Obj_t * pObj;
  int i;
  Aig_ManForEachCi( pAig, pObj, i ){
    sat_solver_add_buffer_enable(pSat, pCnf1->pVarNums[Aig_ManCi(pAig, i)->Id], pCnf2->pVarNums[Aig_ManCi(pAig, i)->Id], ctrlVars[i], 0);
    // Aig_Obj_t * pObj1;
    // int j;
    // Aig_ManForEachCi( pAig, pObj1, j){
    //     if(j == i)
    //       continue;
    //     mySat_solver_add_buffer_enable(pSat, pCnf1->pVarNums[Aig_ManCi(pAig, i)->Id], pCnf2->pVarNums[Aig_ManCi(pAig, j)->Id], ctrlVars[i], ctrlVars[j]);
    //     mySat_solver_add_buffer_enable(pSat, pCnf1->pVarNums[Aig_ManCi(pAig, j)->Id], pCnf2->pVarNums[Aig_ManCi(pAig, i)->Id], ctrlVars[i], ctrlVars[j]);
    // }
  }
  // std::vector<std::unordered_set<int>> symmGroups;
  dsForest<int> symmGroups;
  for(int j = 0, nj = Aig_ManCiNum(pAig); j < nj; ++j){
    symmGroups.makeSet(j);
  }
  for(int j = 0, nj = Aig_ManCiNum(pAig); j < nj; ++j){
    // symmGroups.push_back(std::unordered_set<int>({}));
    for(int k = j + 1, nk = Aig_ManCiNum(pAig); k < nk; ++k){
      // int existed = 0;
      // for(int l = 0, nl = symmGroups.size(); l< nl; ++l){
      //   if(symmGroups[l].count(j) == 1 && symmGroups[l].count(k) == 1){
      //     existed = 1; break;
      //   }
      // }
      // if(existed == 1)
      //   continue;;
      if(symmGroups.FindSet(symmGroups.getelement(j)) == symmGroups.FindSet(symmGroups.getelement(k))){
        printf("%d %d\n", j, k);
        continue;
      }
      assumps[j] = toLitCond(ctrlVars[j], 1);
      assumps[k] = toLitCond(ctrlVars[k], 1);
      assumps[Aig_ManCiNum(pAig) + 1] = toLitCond(pCnf1->pVarNums[Aig_ManCi(pAig, j)->Id], 0);
      assumps[Aig_ManCiNum(pAig) + 2] = toLitCond(pCnf1->pVarNums[Aig_ManCi(pAig, k)->Id], 1);
      assumps[Aig_ManCiNum(pAig) + 3] = toLitCond(pCnf2->pVarNums[Aig_ManCi(pAig, j)->Id], 1);
      assumps[Aig_ManCiNum(pAig) + 4] = toLitCond(pCnf2->pVarNums[Aig_ManCi(pAig, k)->Id], 0);
      int status = sat_solver_solve(pSat, assumps, assumps+ Aig_ManCiNum(pAig) + 5, 100000000, 0,0,0);
      if(status == l_False){
        symmGroups.Union(j, k);
        printf("%d %d\n", j, k);
        // symmGroups.back().insert(j);
        // symmGroups.back().insert(k);
      }        
      assumps[j] = toLitCond(ctrlVars[j], 0);
      assumps[k] = toLitCond(ctrlVars[k], 0);
    }
    // if(symmGroups.back().empty())
    //   symmGroups.pop_back();
  }
  // std::vector<std::vector<int>> symmGroupsVec;
  // for(int j = 0, nj = symmGroups.size(); j < nj; ++j){
  //   int placed = 0;
  //   for(int k = 0, nk = symmGroupsVec.size(); k < nk; ++k){
  //     if(symmGroups.FindSet(symmGroups.getelement(symmGroupsVec[k][0])) == symmGroups.FindSet(symmGroups.getelement(j))){
  //       symmGroupsVec[k].push_back(j);
  //       placed = 1;
  //     }
  //   }
  //   if(placed == 0)
  //     symmGroupsVec.push_back({j});
  // }
  // for(int j = 0, nj = symmGroupsVec.size(); j < nj; ++j){
  //   if(symmGroupsVec[j].size() <= 1)
  //     continue;
  //   printf("( ");
  //   for(auto k : symmGroupsVec[j])
  //     printf("%d, ", k);
  //   printf(")\n");

  // }
  // for(int pp = 0, npp = symmGroups.size(); pp < npp; ++pp){
  //   printf("( ");
  //   for(auto &s : symmGroups[pp])
  //     printf("%d, ", s);
  //   printf(")\n");
  // }
  // printf("===========\n");
  delete []assumps;
  delete []ctrlVars;
  Cnf_DataFree( pCnf1 );
  Cnf_DataFree( pCnf2 );
  sat_solver_delete( pSat );
  Aig_ManStop( pAig );

  return 0;
}
//====================================================================
//======================= Helper Function ============================
//====================================================================

void print_binary(unsigned int number, int bits)
{
    for(int i= 0; i < bits; ++i)
        putchar('0'+((number>>(i))&1));
    // for(int i= bits; i; i--)
    //     putchar('0'+((number>>(i-1))&1));
}

int readSimPattern(Abc_Ntk_t * pNtk ,Abc_Obj_t* pFanin, char ** argv){
    Abc_Obj_t* pNode;
    int i;
    Abc_NtkForEachPi(pNtk, pNode, i){
        if(Abc_ObjId(pNode) == Abc_ObjId(pFanin))
            return (argv[1][i] == '0' ? 1 : 0); // if pattern == '0' -> return 1 (means that the fanin's ddnode should be complemented) 
    }
    return -1;
}

int myStrtol(char* num, int base){
    char* endptr;
    int result = strtol(num, &endptr, base);
    // Check for conversion errors
    if (*endptr != '\0' && *endptr != '\n' || result < 0) {
        return -1;
    } else {
        return result;
    }
}
  
void myAddXorCNF(sat_solver * pSat, int vf, int va, int vb) {
    int lf = toLit(vf), la = toLit(va), lb = toLit(vb);
    int notlf = toLitCond(vf, 1), notla = toLitCond(va, 1), notlb = toLitCond(vb, 1);
    int lits[3];
    
    lits[0] = notla; lits[1] = lb ; lits[2] = lf;
    sat_solver_addclause(pSat, lits, lits + 3);
    lits[0] =  la; lits[1] = notlb; lits[2] =  lf;
    sat_solver_addclause(pSat, lits, lits + 3);
    lits[0] =  la; lits[1] = lb; lits[2] =  notlf;
    sat_solver_addclause(pSat, lits, lits + 3);
    lits[0] =  notla; lits[1] = notlb; lits[2] =  notlf;
    sat_solver_addclause(pSat, lits, lits + 3);
}
int  mySat_solver_add_buffer_enable( sat_solver * pSat, int iVarA, int iVarB, int iVarEn1, int iVarEn2){  // wants to test if xi & xj is symmetric -> enXI = 0 && enXJ = 0
      int Lits[4];
      int Cid;
      assert( iVarA >= 0 && iVarB >= 0 && iVarEn1 >= 0 && iVarEn2 >= 0);

      Lits[0] = toLitCond( iVarA, 0 );
      Lits[1] = toLitCond( iVarB, 1 );
      Lits[2] = toLitCond( iVarEn1, 0 );
      Lits[3] = toLitCond( iVarEn2, 0 );
      Cid = sat_solver_addclause( pSat, Lits, Lits + 4 );
      assert( Cid );

      Lits[0] = toLitCond( iVarA, 1 );
      Lits[1] = toLitCond( iVarB, 0 );
      Lits[2] = toLitCond( iVarEn1, 0 );
      Lits[3] = toLitCond( iVarEn2, 0 );
      Cid = sat_solver_addclause( pSat, Lits, Lits + 4 );
      assert( Cid );
      return 2;
}

