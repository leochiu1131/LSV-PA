#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <queue>
#include <string>
#include "sat/cnf/cnf.h"
#include "sat/cnf/cnf.h"
#include <iostream>
#include <vector>
extern "C"{
Aig_Man_t* Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
}

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandLSVSIMBDD              ( Abc_Frame_t * pAbc, int argc, char ** argv );
static int Lsv_CommandLSVSIMAIG(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandLSVSYMBDD(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandLSVSYMSAT(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandLSVSYMALL(Abc_Frame_t* pAbc, int argc, char** argv);


void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd( pAbc, "LSV",     "lsv_sim_bdd",   Lsv_CommandLSVSIMBDD,       0 );
  Cmd_CommandAdd( pAbc, "LSV",     "lsv_sim_aig",   Lsv_CommandLSVSIMAIG,       0 );
  Cmd_CommandAdd( pAbc, "LSV",     "lsv_sym_bdd",   Lsv_CommandLSVSYMBDD,       0 );
  Cmd_CommandAdd( pAbc, "LSV",     "lsv_sym_sat",   Lsv_CommandLSVSYMSAT,       0 );
  Cmd_CommandAdd( pAbc, "LSV",     "lsv_sym_all",   Lsv_CommandLSVSYMALL,       0 );
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

/**My implement*/

int Lsv_CommandLSVSIMBDD( Abc_Frame_t * pAbc, int argc, char ** argv )
{
    Abc_Ntk_t * pNtk = Abc_FrameReadNtk(pAbc);
    int c, ithPo=0;
    Abc_Obj_t * pPo;
    // set defaults
    Extra_UtilGetoptReset();
    while ( ( c = Extra_UtilGetopt( argc, argv, "h" ) ) != EOF )
    {
        switch ( c )
        {
        
        case 'h':
            goto usage;
        default:
            goto usage;
        }
    }

    if ( pNtk == NULL )
    {
        Abc_Print( -1, "Empty network.\n" );
        return 1;
    }

    
    Abc_NtkForEachPo(pNtk, pPo, ithPo) {
    Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo); 
    
    assert( Abc_NtkIsBddLogic(pRoot->pNtk) );
    DdManager * dd = (DdManager *)pRoot->pNtk->pManFunc;  
    DdNode* ddnode = (DdNode *)pRoot->pData;
    printf("%s: ", Abc_ObjName(pPo));
    char** vNamesIn = (char**) Abc_NodeGetFaninNames(pRoot)->pArray;

    DdNode* temp2[pRoot->vFanins.nSize+1];
    temp2[0]=ddnode;
    for(int i=0; i<pRoot->vFanins.nSize; i++){
        // printf("m是：%s\n", vNamesIn[i]);
        int ithPi = 0;
        Abc_Obj_t * pPi;
            Abc_NtkForEachPi(pNtk, pPi, ithPi){

                int result = strcmp(Abc_ObjName(pPi), vNamesIn[i]);
                if(result==0){
 
                        if(argv[1][ithPi]=='0'){
                            temp2[i+1] = Cudd_Cofactor(dd, temp2[i], Cudd_Not(Cudd_bddIthVar(dd, i)));
                           
                        }
                        else{
                            temp2[i+1] = Cudd_Cofactor(dd, temp2[i], Cudd_bddIthVar(dd, i));
                            
                        }
                        
                   
                    
                }
        }
    }
    
    DdNode* temp3 = Cudd_ReadOne(dd);
    if (temp2[pRoot->vFanins.nSize]==temp3)
    {
        printf("%d\n", 1);
    }
    else
    {
        printf("%d\n", 0);
    }

    }
    

    return 0;

usage:
    Abc_Print( -2, "usage: lsv_sim_bdd [-h] assignment\n" );
    Abc_Print( -2, "\t-h    : print the command usage\n");
    return 1;
}




int Lsv_CommandLSVSIMAIG(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);

  Extra_UtilGetoptReset();
  
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  // Abc_Obj_t* pObjs;
  // int j;
  // Abc_NtkForEachPi(pNtk, pObjs, j){
  //   printf("Object Id = %d, name = %s\n", Abc_ObjId(pObjs), Abc_ObjName(pObjs));
  // }
  // printf("%d %d\n\n", Abc_NtkObjNum( pNtk ), Abc_NtkPiNum( pNtk ));

  // printf("Size of int: %zu bytes\n\n", sizeof(int));

  FILE *filek = fopen(argv[1], "r"); // 打开文件以供读取

  if (filek == NULL) {
      perror("cannot open the file.");
      return 1;
  }
  bool running = true;
  int rem = 0;
  std::queue<int>* vecarr;
  vecarr = new std::queue<int>[Abc_NtkPoNum(pNtk )];
  char** names;
  names = new char*[Abc_NtkPoNum(pNtk )];
  while(running){
    unsigned int* result; // 用于存储结果的32位整数
    result = new unsigned int[Abc_NtkObjNum( pNtk )];
    for(int i=0; i<Abc_NtkObjNum( pNtk ); i++){
      result[i]=0;
    }

    for (int i = 0; i < 32; i++) {
      for(int k=1; k<=Abc_NtkPiNum(pNtk ); k++){
        char ch = fgetc(filek);
        // printf("%c ", ch);
        if (ch == '1') {
            // 如果读到字符 '1'，将对应位设置为1
            result[k] |= (1);
        } else if (ch != '0') {
            // 处理错误：如果不是 '0' 或 '1'，则退出
            // printf("有餘數 %d\n", i);
            fclose(filek);
            rem = i;
            running = false;
            break;
        }
        if(i!=31 && running){
          result[k]=result[k]<<1;
        }
            
      }
      if(!running){
        for(int k=1; k<=Abc_NtkPiNum(pNtk ); k++){
          result[k]=result[k]>>1;
        }
        break;
      }
      fgetc(filek);
      // printf("\n");
        
    }
    // printf("看這裡\n");
    // for(int i=1; i<=Abc_NtkPiNum(pNtk ); i++){
    //   printf("%u ", result[i]);
    // }
    // printf("\n\n");

    


    Abc_Obj_t* pObj;
    int i;
    Abc_NtkForEachNode(pNtk, pObj, i) {
      // printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
      // if(Abc_NtkIsStrash(pNtk)){
      //   printf("fCompl0 = %d, id: %d, fCompl1 = %d, id: %d\n", Abc_ObjFaninC0(pObj),Abc_ObjId(Abc_ObjFanin0( pObj )), Abc_ObjFaninC1(pObj), Abc_ObjId(Abc_ObjFanin1( pObj )));
      // }
      if(Abc_ObjFaninC0(pObj)==0 && Abc_ObjFaninC1(pObj)==0){
        result[Abc_ObjId(pObj)] = result[Abc_ObjId(Abc_ObjFanin0( pObj ))]&result[Abc_ObjId(Abc_ObjFanin1( pObj ))];
      }
      else if(Abc_ObjFaninC0(pObj)==0 && Abc_ObjFaninC1(pObj)==1){
        result[Abc_ObjId(pObj)] = result[Abc_ObjId(Abc_ObjFanin0( pObj ))]&(~result[Abc_ObjId(Abc_ObjFanin1( pObj ))]);
      }
      else if(Abc_ObjFaninC0(pObj)==1 && Abc_ObjFaninC1(pObj)==0){
        result[Abc_ObjId(pObj)] = (~result[Abc_ObjId(Abc_ObjFanin0( pObj ))])&result[Abc_ObjId(Abc_ObjFanin1( pObj ))];
      }
      else if(Abc_ObjFaninC0(pObj)==1 && Abc_ObjFaninC1(pObj)==1){
        result[Abc_ObjId(pObj)] = (~result[Abc_ObjId(Abc_ObjFanin0( pObj ))])&(~result[Abc_ObjId(Abc_ObjFanin1( pObj ))]);
      }
    }

      Abc_Obj_t* pObju;
    int u;
    Abc_NtkForEachPo(pNtk, pObju, u) {
      // printf("Object Id = %d, name = %s\n", Abc_ObjId(pObju), Abc_ObjName(pObju));
      names[u] = Abc_ObjName(pObju);
      // if(Abc_NtkIsStrash(pNtk)){
      //   printf("fCompl0 = %d, id: %d\n", Abc_ObjFaninC0(pObju),Abc_ObjId(Abc_ObjFanin0( pObju )));
      // }
      if(Abc_ObjFaninC0(pObju)==0){
        result[Abc_ObjId(pObju)] = result[Abc_ObjId(Abc_ObjFanin0( pObju ))];
      }
      else if(Abc_ObjFaninC0(pObju)==1){
        result[Abc_ObjId(pObju)] = (~result[Abc_ObjId(Abc_ObjFanin0( pObju ))]);
      }
      // printf("result is %u\n", result[Abc_ObjId(pObju)]);
      if(running){
        int temparr[32]={0};
        for(int i=31; i>=0; i--){
          temparr[i]=result[Abc_ObjId(pObju)]&1;
          // printf("%d", temparr[i]);
          result[Abc_ObjId(pObju)] = result[Abc_ObjId(pObju)]>>1;
        }
        // printf("\n");
        for(int i=0; i<32; i++){
          vecarr[u].push(temparr[i]);
        }
        // printf("喔嗨嗨呦%d\n", (int)(vecarr[0].size()));
      }
      else{
        int temparr[32]={0};
        for(int i=rem-1; i>=0; i--){
          temparr[i]=result[Abc_ObjId(pObju)]&1;
          // printf("%d", temparr[i]);
          result[Abc_ObjId(pObju)] = result[Abc_ObjId(pObju)]>>1;
        }
        // printf("\n");
        for(int i=0; i<rem; i++){
          vecarr[u].push(temparr[i]);
        }
        // printf("喔嗨呦優%d\n", (int)(vecarr[0].size()));
      }
    }
  }
  // printf("喔嗨呦%d\n", (int)(vecarr[0].size()));
  for(int i=0; i<Abc_NtkPoNum(pNtk ); i++){
    int size = vecarr[i].size();
    printf("%s: ",names[i]);
    for (int j = 0; j < size; j++) {
        printf( "%d", vecarr[i].front());
        vecarr[i].pop();
    }
    printf("\n");
  }
  // 关闭文件
  // fclose(filek);

  
  return 0;


}

/////////////// PA2

bool onePath(DdManager* dd, DdNode* nd, std::string& path, int now){
  if(Cudd_IsConstant(nd)){
    if(nd==Cudd_ReadOne(dd)){
      return true;
    }
    else{
      printf("no path, error.\n");
      return false;
    }
  }
  else{
    DdNode* temp1 = Cudd_Cofactor(dd, nd, Cudd_Not(Cudd_bddIthVar(dd, now)));
    
    Cudd_Ref(temp1);
    if(temp1!=Cudd_Not(Cudd_ReadOne(dd))){
      onePath(dd, temp1, path, now+1);
      path[now] = '0';
      Cudd_RecursiveDeref(dd, temp1);
      return true;
    }
    else {
      DdNode* temp2 = Cudd_Cofactor(dd, nd, Cudd_bddIthVar(dd, now));
      Cudd_Ref(temp2);
      onePath(dd, temp2, path, now+1);
      path[now] = '1';
      Cudd_RecursiveDeref(dd, temp1);
      Cudd_RecursiveDeref(dd, temp2);
      return true;
    }
    
  }
  return true;
}

int Lsv_CommandLSVSYMBDD( Abc_Frame_t * pAbc, int argc, char ** argv )
{
    Abc_Ntk_t * pNtk = Abc_FrameReadNtk(pAbc);
    int c, ithPo=0;
    Abc_Obj_t * pPo;
    // set defaults
    // Extra_UtilGetoptReset();
    // while ( ( c = Extra_UtilGetopt( argc, argv, "h" ) ) != EOF )
    // {
    //     switch ( c )
    //     {
        
    //     case 'h':
    //         goto usage;
    //     default:
    //         goto usage;
    //     }
    // }

    if ( pNtk == NULL )
    {
        Abc_Print( -1, "Empty network.\n" );
        return 1;
    }
    // printf("zeroth argvs: %s\n", argv[1]);
    int num1 = std::atoi(argv[1]);
    int num2 = std::atoi(argv[2]);
    int num3 = std::atoi(argv[3]);
    // printf("zeroth argvs: %d\n", num1);
    // printf("first argvs: %d\n", num2);
    // printf("second argvs: %d\n", num3);
    
    pPo = Abc_NtkPo(pNtk, num1);
    Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo); 
    
    assert( Abc_NtkIsBddLogic(pRoot->pNtk) );
    DdManager * dd = (DdManager *)pRoot->pNtk->pManFunc;  
    DdNode* ddnode = (DdNode *)pRoot->pData;
    Cudd_Ref(ddnode);
    // printf("%s: \n", Abc_ObjName(pPo));
    char** vNamesIn = (char**) Abc_NodeGetFaninNames(pRoot)->pArray;

    DdNode* temp2[3];
    temp2[0]=ddnode;
    Cudd_Ref(temp2[0]);
    DdNode* temp22[3];
    temp22[0]=ddnode;
    Cudd_Ref(temp22[0]);
    int ifirst = -1;
    int isecond = -1;
    int pinum = Abc_NtkPiNum(pNtk);
    // DdNode* tempevery[pinum];
    // printf( "number: %d\n", pinum);
    int* temparr = new int[pinum];
    int* temparr2 = new int[pinum];
    for(int i=0; i<pinum; i++){
      temparr[i]=0;
      temparr2[i]=-1;
    }
    int* bdd2abc = new int [pRoot->vFanins.nSize];
    for(int i=0; i<pRoot->vFanins.nSize; i++){
        // printf("m是：%s\n", vNamesIn[i]);
        int ithPi = 0;
        Abc_Obj_t * pPi;
        pPi = Abc_NtkPi(pNtk, num2);
        // printf("first argv: %s\n", argv[1][1]);
        Abc_Obj_t * pPi2;
        pPi2 = Abc_NtkPi(pNtk, num3);
        int inputsnum = std::atoi(vNamesIn[i]+3);
        // printf("second argv: %d\n", std::atoi(vNamesIn[i]+3));
        temparr2[inputsnum]=i;
        bdd2abc[i]=inputsnum;
        int result = strcmp(Abc_ObjName(pPi), vNamesIn[i]);
        int result2 = strcmp(Abc_ObjName(pPi2), vNamesIn[i]);
        if(result==0){
          ifirst = i;
          // printf("first: %s\n", Abc_ObjName(pPi));
        } 
        if(result2==0){
          isecond = i;
          // printf("second: %s\n", Abc_ObjName(pPi2));
        }   
    }
    // printf("if: %d\n", ifirst);
    // printf("is: %d\n", isecond);
    if(ifirst != -1){
      temp2[1] = Cudd_Cofactor(dd, temp2[0], Cudd_Not(Cudd_bddIthVar(dd, ifirst)));
      temp22[1] = Cudd_Cofactor(dd, temp22[0], Cudd_bddIthVar(dd, ifirst));
      Cudd_Ref(temp2[1]);
      Cudd_Ref(temp22[1]);
    }
    else{
      temp2[1] = temp2[0];
      temp22[1] = temp2[0];
      Cudd_RecursiveDeref(dd, temp2[0]);
      Cudd_RecursiveDeref(dd, temp22[0]);
      Cudd_Ref(temp2[1]);
      Cudd_Ref(temp22[1]);
    }
    if(isecond != -1){
      temp2[2] = Cudd_Cofactor(dd, temp2[1], Cudd_bddIthVar(dd, isecond));
      temp22[2] = Cudd_Cofactor(dd, temp22[1], Cudd_Not(Cudd_bddIthVar(dd, isecond)));
      Cudd_RecursiveDeref(dd, temp2[1]);
      Cudd_RecursiveDeref(dd, temp22[1]);
      Cudd_Ref(temp2[2]);
      Cudd_Ref(temp22[2]);
    }
    else{
      temp2[2] = temp2[1];
      temp22[2] = temp22[1];
      Cudd_RecursiveDeref(dd, temp2[1]);
      Cudd_RecursiveDeref(dd, temp22[1]);
      Cudd_Ref(temp2[2]);
      Cudd_Ref(temp22[2]);
    }
    
    
    if (temp2[2]==temp22[2])
    {
        // printf("%d\n", 1);
        printf("symmetric\n");
    }
    else
    {
        // printf("%d\n", 0);
        printf("asymmetric\n");
        DdNode* temp3;
        temp3 = Cudd_bddXor(dd, temp2[2], temp22[2]);
        Cudd_Ref(temp3);
        //do DFS and see if there is a path to 1
        //DFS
        std::string path = "";
        std::string abc_path = "";
        std::string abc_path_01 = "";
        std::string abc_path_10 = "";
        for(int i = 0; i<pinum; i++){
          abc_path += "0";
          abc_path_01 += "0";
          abc_path_10 += "0";
          if(temparr2[i]!=-1){
            path += "0";
          }
        }
        if(onePath(dd, temp3, path, 0)){
          // printf("path: %s\n", path.c_str());
          for(int i=0; i<pRoot->vFanins.nSize; i++){
            abc_path[bdd2abc[i]] = path[i];
            abc_path_01[bdd2abc[i]] = path[i];
            abc_path_10[bdd2abc[i]] = path[i];
          }
          abc_path_01[num2] = '0';
          abc_path_10[num2] = '1';
          abc_path_01[num3] = '1';
          abc_path_10[num3] = '0';
          // printf("abc_path: %s\n", abc_path.c_str());
          printf("%s\n", abc_path_01.c_str());
          printf("%s\n", abc_path_10.c_str());
          
        }
        else{
          printf("no path\n");
        }
        Cudd_RecursiveDeref(dd, temp3);
        Cudd_RecursiveDeref(dd, temp2[2]);
        Cudd_RecursiveDeref(dd, temp22[2]);

    }
    Cudd_RecursiveDeref(dd, ddnode);
    

    return 0;

// usage:
//     Abc_Print( -2, "usage: lsv_sim_bdd [-h] assignment\n" );
//     Abc_Print( -2, "\t-h    : print the command usage\n");
//     return 1;
}

int Lsv_CommandLSVSYMSAT(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);

  Extra_UtilGetoptReset();
  
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  sat_solver* pSat;
  int y = std::atoi(argv[1]);
  int x1 = std::atoi(argv[2]);
  int x2 = std::atoi(argv[3]);
  printf("x1: %d\n", x1);
  printf("x2: %d\n", x2);

  Abc_Obj_t* pPo = Abc_NtkPo(pNtk, y);
  Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo); 
  Abc_Ntk_t* pNtk2 = Abc_NtkCreateCone(pNtk, pRoot, Abc_ObjName(pPo), 1);
  Aig_Man_t* aigman = Abc_NtkToDar(pNtk2, 0, 1);
  pSat = sat_solver_new();
  Cnf_Dat_t* pCnf = Cnf_Derive(aigman, Aig_ManCoNum(aigman));
  Cnf_DataWriteIntoSolverInt(pSat, pCnf, 1, 0);
  Cnf_DataLift(pCnf, pCnf->nVars);
  Cnf_DataWriteIntoSolverInt(pSat, pCnf, 2, 0);
  Cnf_DataLift(pCnf, -(pCnf->nVars));
  int u;
  Aig_Obj_t* pObj;
  int pLits[2];
  int nVars = pCnf->nVars; 
  Aig_ManForEachCi(aigman, pObj, u){
    if(u==x1){
      pLits[0] = toLitCond(pCnf->pVarNums[pObj->Id], 0);
      pLits[1] = toLitCond(pCnf->pVarNums[pObj->Id]+nVars, 0);
      if(!sat_solver_addclause(pSat, pLits, pLits+2)){
        printf("something wrong: %d\n", x1);
      }
      pLits[0] = toLitCond(pCnf->pVarNums[pObj->Id], 1);
      pLits[1] = toLitCond(pCnf->pVarNums[pObj->Id]+nVars, 1);
      if(!sat_solver_addclause(pSat, pLits, pLits+2)){
        printf("something wrong: %d\n", x1);
      }
    }
    else if(u==x2){
      pLits[0] = toLitCond(pCnf->pVarNums[pObj->Id], 0);
      pLits[1] = toLitCond(pCnf->pVarNums[pObj->Id]+nVars, 0);
      if(!sat_solver_addclause(pSat, pLits, pLits+2)){
        printf("something wrong: %d\n", x2);
      }
      pLits[0] = toLitCond(pCnf->pVarNums[pObj->Id], 1);
      pLits[1] = toLitCond(pCnf->pVarNums[pObj->Id]+nVars, 1);
      if(!sat_solver_addclause(pSat, pLits, pLits+2)){
        printf("something wrong: %d\n", x2);
      }
    }
    else{
      pLits[0] = toLitCond(pCnf->pVarNums[pObj->Id], 0);
      pLits[1] = toLitCond(pCnf->pVarNums[pObj->Id]+nVars, 1);
      if(!sat_solver_addclause(pSat, pLits, pLits+2)){
        printf("something wrong: %d\n", u);
      }
      pLits[0] = toLitCond(pCnf->pVarNums[pObj->Id], 1);
      pLits[1] = toLitCond(pCnf->pVarNums[pObj->Id]+nVars, 0);
      if(!sat_solver_addclause(pSat, pLits, pLits+2)){
        printf("something wrong: %d\n", u);
      }
    }
  }

  pLits[0] = toLitCond(pCnf->pVarNums[Aig_ManCi(aigman, x1)->Id], 0);
  pLits[1] = toLitCond(pCnf->pVarNums[Aig_ManCi(aigman, x2)->Id], 0);
  if(!sat_solver_addclause(pSat, pLits, pLits+2)){
    printf("something wrong1: %d\n", pCnf->pVarNums[Aig_ManCi(aigman, x1)->Id]);
  }
  pLits[0] = toLitCond(pCnf->pVarNums[Aig_ManCi(aigman, x1)->Id], 1);
  pLits[1] = toLitCond(pCnf->pVarNums[Aig_ManCi(aigman, x2)->Id], 1);
  if(!sat_solver_addclause(pSat, pLits, pLits+2)){
    printf("something wrong2: %d\n", pCnf->pVarNums[Aig_ManCi(aigman, x2)->Id]);
  }
  pLits[0] = toLitCond(pCnf->pVarNums[Aig_ManCi(aigman, x1)->Id]+nVars, 0);
  pLits[1] = toLitCond(pCnf->pVarNums[Aig_ManCi(aigman, x2)->Id]+nVars, 0);
  if(!sat_solver_addclause(pSat, pLits, pLits+2)){
    printf("something wrong3: %d\n", pCnf->pVarNums[Aig_ManCi(aigman, x2)->Id]);
  }
  pLits[0] = toLitCond(pCnf->pVarNums[Aig_ManCi(aigman, x1)->Id]+nVars, 1);
  pLits[1] = toLitCond(pCnf->pVarNums[Aig_ManCi(aigman, x2)->Id]+nVars, 1);
  if(!sat_solver_addclause(pSat, pLits, pLits+2)){
    printf("something wrong4: %d\n", pCnf->pVarNums[Aig_ManCi(aigman, x2)->Id]);
  }

  Aig_ManForEachCo(aigman, pObj, u){
    pLits[0] = toLitCond(pCnf->pVarNums[pObj->Id], 0);
    pLits[1] = toLitCond(pCnf->pVarNums[pObj->Id]+nVars, 0);
    if(!sat_solver_addclause(pSat, pLits, pLits+2)){
      printf("something wrong5: %d\n", u);
    }
    pLits[0] = toLitCond(pCnf->pVarNums[pObj->Id], 1);
    pLits[1] = toLitCond(pCnf->pVarNums[pObj->Id]+nVars, 1);
    if(!sat_solver_addclause(pSat, pLits, pLits+2)){
      printf("something wrong6: %d\n", u);
    }
  }
  int status = sat_solver_solve(pSat, NULL, NULL, 0, 0, 0, 0);
  if(status==l_True){
    printf("asymmetric\n");
    Aig_ManForEachCi(aigman, pObj, u){
      if(sat_solver_var_value(pSat, pCnf->pVarNums[pObj->Id])==1){
        printf("1");
      }
      else{
        printf("0");
      }
    }
    printf("\n");
    Aig_ManForEachCi(aigman, pObj, u){
      if(sat_solver_var_value(pSat, pCnf->pVarNums[pObj->Id]+nVars)==1){
        printf("1");
      }
      else{
        printf("0");
      }
    }
    printf("\n");
  }
  else if(status==l_False){
    printf("symmetric\n");
  }
  else{
    printf("INDET\n");
  }
  return 0;
}


int Lsv_CommandLSVSYMALL(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);

  Extra_UtilGetoptReset();
  
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  sat_solver* pSat;
  int y = std::atoi(argv[1]);
  // int x1 = std::atoi(argv[2]);
  // int x2 = std::atoi(argv[3]);

  Abc_Obj_t* pPo = Abc_NtkPo(pNtk, y);
  Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo); 
  Abc_Ntk_t* pNtk2 = Abc_NtkCreateCone(pNtk, pRoot, Abc_ObjName(pPo), 1);
  Aig_Man_t* aigman = Abc_NtkToDar(pNtk2, 0, 1);
  pSat = sat_solver_new();
  // printf("Aig_ManCoNum(aigman): %d\n", Aig_ManCoNum(aigman));
  Cnf_Dat_t* pCnf = Cnf_Derive(aigman, Aig_ManCoNum(aigman));
  Cnf_DataWriteIntoSolverInt(pSat, pCnf, 1, 0);
  Cnf_DataLift(pCnf, pCnf->nVars);
  Cnf_DataWriteIntoSolverInt(pSat, pCnf, 2, 0);
  Cnf_DataLift(pCnf, (pCnf->nVars));
  Cnf_DataWriteIntoSolverInt(pSat, pCnf, 3, 0);
  Cnf_DataLift(pCnf, -2*(pCnf->nVars));
  int u;
  Aig_Obj_t* pObj;
  int pLits[4];
  int nVars = pCnf->nVars; 
  // printf("nVars: %d\n", nVars);
  Aig_ManForEachCi(aigman, pObj, u){
    // (A'+B+H)(A+B'+H) = (A=B)+H
      // printf("u: %d\n", u);
      pLits[0] = toLitCond(pCnf->pVarNums[pObj->Id], 1);
      pLits[1] = toLitCond(pCnf->pVarNums[pObj->Id]+nVars, 0);
      pLits[2] = toLitCond(pCnf->pVarNums[pObj->Id]+2*nVars, 0);
      if(!sat_solver_addclause(pSat, pLits, pLits+3)){
        printf("something wrong\n");
      }
      pLits[0] = toLitCond(pCnf->pVarNums[pObj->Id], 0);
      pLits[1] = toLitCond(pCnf->pVarNums[pObj->Id]+nVars, 1);
      pLits[2] = toLitCond(pCnf->pVarNums[pObj->Id]+2*nVars, 0);
      if(!sat_solver_addclause(pSat, pLits, pLits+3)){
        printf("something wrong\n");
      }
    
  }
  
  //(A(t1) = B(t2))+H'(t1)+H'(t2) = (A(t1)+B'(t2)+H'(t1)+H'(t2))(A'(t1)+B(t2)+H'(t1)+H'(t2))
  for(int i=0; i<Aig_ManCiNum(aigman)-1; i++){
    for(int j=i+1; j<Aig_ManCiNum(aigman); j++){
      // printf("i: %d, j: %d\n", i, j);
      pLits[0] = toLitCond(pCnf->pVarNums[Aig_ManCi(aigman, i)->Id], 0);
      pLits[1] = toLitCond(pCnf->pVarNums[Aig_ManCi(aigman, j)->Id]+nVars, 1);
      pLits[2] = toLitCond(pCnf->pVarNums[Aig_ManCi(aigman, i)->Id]+2*nVars, 1);
      pLits[3] = toLitCond(pCnf->pVarNums[Aig_ManCi(aigman, j)->Id]+2*nVars, 1);
      if(!sat_solver_addclause(pSat, pLits, pLits+4)){
        printf("something wrong\n");
      }
      pLits[0] = toLitCond(pCnf->pVarNums[Aig_ManCi(aigman, i)->Id], 1);
      pLits[1] = toLitCond(pCnf->pVarNums[Aig_ManCi(aigman, j)->Id]+nVars, 0);
      pLits[2] = toLitCond(pCnf->pVarNums[Aig_ManCi(aigman, i)->Id]+2*nVars, 1);
      pLits[3] = toLitCond(pCnf->pVarNums[Aig_ManCi(aigman, j)->Id]+2*nVars, 1);
      if(!sat_solver_addclause(pSat, pLits, pLits+4)){
        printf("something wrong\n");
      }
    }
  }


  for(int i=0; i<Aig_ManCiNum(aigman)-1; i++){
    for(int j=i+1; j<Aig_ManCiNum(aigman); j++){
      // printf("i: %d, j: %d\n", i, j);
      pLits[0] = toLitCond(pCnf->pVarNums[Aig_ManCi(aigman, i)->Id], 0);
      pLits[1] = toLitCond(pCnf->pVarNums[Aig_ManCi(aigman, j)->Id], 0);
      pLits[2] = toLitCond(pCnf->pVarNums[Aig_ManCi(aigman, i)->Id]+2*nVars, 1);
      pLits[3] = toLitCond(pCnf->pVarNums[Aig_ManCi(aigman, j)->Id]+2*nVars, 1);
      if(!sat_solver_addclause(pSat, pLits, pLits+4)){
        printf("something wrong\n");
      }
      pLits[0] = toLitCond(pCnf->pVarNums[Aig_ManCi(aigman, i)->Id], 1);
      pLits[1] = toLitCond(pCnf->pVarNums[Aig_ManCi(aigman, j)->Id], 1);
      pLits[2] = toLitCond(pCnf->pVarNums[Aig_ManCi(aigman, i)->Id]+2*nVars, 1);
      pLits[3] = toLitCond(pCnf->pVarNums[Aig_ManCi(aigman, j)->Id]+2*nVars, 1);
      if(!sat_solver_addclause(pSat, pLits, pLits+4)){
        printf("something wrong\n");
      }

      pLits[0] = toLitCond(pCnf->pVarNums[Aig_ManCi(aigman, i)->Id]+nVars, 0);
      pLits[1] = toLitCond(pCnf->pVarNums[Aig_ManCi(aigman, j)->Id]+nVars, 0);
      pLits[2] = toLitCond(pCnf->pVarNums[Aig_ManCi(aigman, i)->Id]+2*nVars, 1);
      pLits[3] = toLitCond(pCnf->pVarNums[Aig_ManCi(aigman, j)->Id]+2*nVars, 1);
      if(!sat_solver_addclause(pSat, pLits, pLits+4)){
        printf("something wrong\n");
      }
      pLits[0] = toLitCond(pCnf->pVarNums[Aig_ManCi(aigman, i)->Id]+nVars, 1);
      pLits[1] = toLitCond(pCnf->pVarNums[Aig_ManCi(aigman, j)->Id]+nVars, 1);
      pLits[2] = toLitCond(pCnf->pVarNums[Aig_ManCi(aigman, i)->Id]+2*nVars, 1);
      pLits[3] = toLitCond(pCnf->pVarNums[Aig_ManCi(aigman, j)->Id]+2*nVars, 1);
      if(!sat_solver_addclause(pSat, pLits, pLits+4)){
        printf("something wrong\n");
      }
    }
  }

  Aig_ManForEachCo(aigman, pObj, u){
    pLits[0] = toLitCond(pCnf->pVarNums[pObj->Id], 0);
    pLits[1] = toLitCond(pCnf->pVarNums[pObj->Id]+nVars, 0);
    if(!sat_solver_addclause(pSat, pLits, pLits+2)){
      printf("something wrong\n");
    }
    pLits[0] = toLitCond(pCnf->pVarNums[pObj->Id], 1);
    pLits[1] = toLitCond(pCnf->pVarNums[pObj->Id]+nVars, 1);
    if(!sat_solver_addclause(pSat, pLits, pLits+2)){
      printf("something wrong\n");
    }
  }
  // printf("Aig_ManCiNum(aigman): %d\n", Aig_ManCiNum(aigman));
  
  //H's conditions
  int *pLits2 = new int[Aig_ManCiNum(aigman)];
  for(int i=0; i<Aig_ManCiNum(aigman); i++){
    pLits2[i] = toLitCond(pCnf->pVarNums[Aig_ManCi(aigman, i)->Id]+2*nVars, 1);
  }
  int status;
  bool** result1;
  result1 = new bool*[Aig_ManCiNum(aigman)];
  for(int i=0; i<Aig_ManCiNum(aigman); i++){
    result1[i] = new bool[Aig_ManCiNum(aigman)];
    for(int j=0; j<Aig_ManCiNum(aigman); j++){
      result1[i][j] = false;
    }
  }

  for(int i=0; i<Aig_ManCiNum(aigman)-1; i++){
    for(int j = i+1; j<Aig_ManCiNum(aigman); j++){
      // printf("I1: %d, J1: %d, result: %d\n", i, j, result1[i][j]);
      if(result1[i][j]==true){
        printf("%d %d\n", i, j);
        // printf("KKKKK\n");
        continue;
      }
      // printf("I11: %d, J11: %d\n", i, j);
      // printf("GGGGG\n");
      pLits2[i] = toLitCond(pCnf->pVarNums[Aig_ManCi(aigman, i)->Id]+2*nVars, 0);
      pLits2[j] = toLitCond(pCnf->pVarNums[Aig_ManCi(aigman, j)->Id]+2*nVars, 0);
      // pLits[0] = toLitCond(pCnf->pVarNums[Aig_ManCi(aigman, i)->Id]+2*nVars, 0);
      // pLits[1] = toLitCond(pCnf->pVarNums[Aig_ManCi(aigman, j)->Id]+2*nVars, 0);
      // printf("MMMMM\n");
      status = sat_solver_solve(pSat, pLits2, pLits2+Aig_ManCiNum(aigman), (ABC_INT64_T)0, (ABC_INT64_T)0, (ABC_INT64_T)0, (ABC_INT64_T)0);
      if(status==l_False){
        printf("%d %d\n", i, j);
        result1[i][j] = true;
        // printf("%d, %d\n", i, j);
      }
      // printf("I12: %d, J12: %d\n", i, j);
      pLits2[i] = toLitCond(pCnf->pVarNums[Aig_ManCi(aigman, i)->Id]+2*nVars, 1);
      pLits2[j] = toLitCond(pCnf->pVarNums[Aig_ManCi(aigman, j)->Id]+2*nVars, 1);
      // printf("I: %d, J: %d\n", i, j);
    }
    for(int k = 0; k<Aig_ManCiNum(aigman)-1; k++){
      for(int s=k+1; s<Aig_ManCiNum(aigman); s++){
        // printf("k: %d, s: %d\n", k, s);
        if(result1[i][k]&&result1[i][s]){
          result1[k][s]=true;
        }
      }
    }
    // printf("here\n");
  }
  return 0;
}
