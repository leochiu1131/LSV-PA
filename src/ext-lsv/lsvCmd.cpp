#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"

static int Lsv_CommandSimBDD(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimAIG(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", Lsv_CommandSimBDD, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig", Lsv_CommandSimAIG, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

void Lsv_SimBDD(Abc_Ntk_t* pNtk, char* pInput) {
  Abc_Obj_t *pObj, *pI, *pPo, *pFanin;
  int ithPo, ithFI;
  bool ans = 0;
  struct inseq{
    bool value;
    char *name;
  };
  inseq inarr[Abc_NtkPiNum(pNtk)];
  for(ithFI = 0; ithFI<Abc_NtkPiNum(pNtk); ithFI++){
    if (pInput[ithFI] == '\0') {
      Abc_Print(-1, "Wrong input length.\n");
      return;
    }
    inarr[ithFI].value = (bool)(pInput[ithFI] - '0' > 0);
	inarr[ithFI].name = Abc_ObjName(Abc_NtkPi(pNtk, ithFI));
	printf("%s = %d\n", inarr[ithFI].name, inarr[ithFI].value);
  }
/*   Abc_NtkForEachNode(pNtk, pObj, i) {
    printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
    Abc_Obj_t* pFanin;
    int j;
    Abc_ObjForEachFanin(pObj, pFanin, j) {
      printf("  Fanin-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanin), Abc_ObjName(pFanin));
    }
    if (Abc_NtkHasSop(pNtk)) {
      printf("The SOP of this node:\n%s", (char*)pObj->pData);
    }
  }  */
  
  Abc_NtkForEachPo(pNtk, pPo, ithPo) {
    Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo); 
    
    assert( Abc_NtkIsBddLogic(pRoot->pNtk) );
    DdManager * dd = (DdManager *)pRoot->pNtk->pManFunc;  
    DdNode* ddnode = (DdNode *)pRoot->pData;
	
	char** vNamesIn = (char**) Abc_NodeGetFaninNames(pRoot)->pArray;
    Abc_Print(1, "%s\n", *vNamesIn);
	
	Abc_ObjForEachFanin( pRoot, pI, ithFI ){
      //Abc_Print(1, "%s: %i\n", Abc_ObjName(pI), inarr[ithFI].value);
      if(*vNamesIn + ithFI == inarr[ithFI].name){
        Abc_Print(1, "match%s\n", *vNamesIn+ ithFI);//inarr[ithFI].value
	  }
	}
    //Abc_ObjForEachFanin( pRoot, pFanin, ithFI ){
      
      //printf("%s\n", *vNamesIn);
      //ddnode = Cudd_Cofactor(dd, ddnode, );
    //}
    Abc_Print(1, "%s: %i\n", Abc_ObjName(pPo), ans);
  }
}

int Lsv_CommandSimBDD(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  char * pInput = argv[1];
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
  if ( !Abc_NtkIsBddLogic( pNtk ) ){
    Abc_Print( -1, "This command is only applicable to logic BDD networks (run \"collapse\").\n" );
    return 1;
  }
  if (argc != 2){
    Abc_Print(-1, "One input pattern required.\n");
    return 1;
  }
  Lsv_SimBDD( pNtk, pInput);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sim_bdd <input_pattern>\n");
  Abc_Print(-2, "\t        print function simulation with BDD\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}
void Lsv_SimAIG(Abc_Ntk_t* pNtk, char* pInput) {
  Abc_Obj_t *pObj, *pI, *pPo, *pFanin;
  int ithPo, ithFI;
  bool ans = 0;
  Abc_Print(1, "%s\n", pInput);
  Abc_NtkForEachPo(pNtk, pPo, ithPo) {
    Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo); 
    assert( Abc_NtkIsBddLogic(pRoot->pNtk) );
    DdManager * dd = (DdManager *)pRoot->pNtk->pManFunc;  
    DdNode* ddnode = (DdNode *)pRoot->pData;
    Abc_ObjForEachFanin( pRoot, pFanin, ithFI ){
      
    }
    Abc_Print(1, "%s: %i\n", Abc_ObjName(pPo), ans);
  }
}
int Lsv_CommandSimAIG(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  char * pInput = argv[1];
  char **pInputSet;
  FILE *fp;
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
  if ( !Abc_NtkIsStrash(pNtk) )
  {
    Abc_Print( -1, "This command works only for AIGs (run \"strash\").\n" );
    return 1;
  }
  if (argc != 2){
    Abc_Print(-1, "One input file required.\n");
    return 1;
  }
  fp = fopen(pInput, "r");
  if(fp != NULL){
    char* buff;
    int len;
    fseek(fp, 0, SEEK_END);
    len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    buff = (char*)malloc(len * sizeof(char));

    while(fgets(buff, len * sizeof(char), fp) != NULL){
      
      printf("%s", buff);
      //Lsv_SimAIG( pNtk, buff);
    }
    fclose(fp);
  }

  return 0;
  
usage:
  Abc_Print(-2, "usage: lsv_sim_aig [-h]\n");
  Abc_Print(-2, "\t        print function simulation with AIG\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}