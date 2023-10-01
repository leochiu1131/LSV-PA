#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <queue>

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandLSVSIMBDD              ( Abc_Frame_t * pAbc, int argc, char ** argv );
static int Lsv_CommandLSVSIMAIG(Abc_Frame_t* pAbc, int argc, char** argv);


void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd( pAbc, "LSV",     "lsv_sim_bdd",   Lsv_CommandLSVSIMBDD,       0 );
  Cmd_CommandAdd( pAbc, "LSV",     "lsv_sim_aig",   Lsv_CommandLSVSIMAIG,       0 );
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