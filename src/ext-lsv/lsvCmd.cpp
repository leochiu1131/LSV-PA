#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/cnf/cnf.h"
extern "C"{
Aig_Man_t* Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
}

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSymCheckBdd(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSymCheckSat(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSymIncrSat(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_bdd", Lsv_CommandSymCheckBdd, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_sat", Lsv_CommandSymCheckSat, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_all", Lsv_CommandSymIncrSat, 0);
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

int BddCofactor(Abc_Obj_t* pNode, DdManager* dd, DdNode* node, int* iValue){
  if(!node){
    printf("error:!node\n");
    return -1;
  }
  if( node == Cudd_ReadLogicZero(dd)){
    return 0;
  }
  if( node == Cudd_ReadOne(dd)){
    return 1;
  }
  int idx = Cudd_NodeReadIndex(node);
  DdNode *nCopy = Cudd_bddIthVar(dd, idx);
  int vIdx = Abc_ObjId(Abc_ObjFanin(pNode, idx)) - 1;
  DdNode *nNext = iValue[vIdx] ? Cudd_Cofactor(dd,node,nCopy): Cudd_Cofactor(dd,node,Cudd_Not(nCopy));
  return BddCofactor(pNode,dd, nNext, iValue);
}
int SimSinglePatternBdd(Abc_Obj_t* pNode, DdManager *dd, int iIdx[], int* iValue){
  DdNode *node = ( DdNode * ) pNode->pData;
  int p1 = BddCofactor(pNode,dd,node,iValue);
  iValue[iIdx[0]]=1;
  iValue[iIdx[1]]=0;
  int p2 = BddCofactor(pNode,dd,node,iValue);
  if(p1==p2){
    return 1;
  }else{
    return 0;
  }
}
void SimAllPatternBdd(Abc_Ntk_t* pNtk, int oIdx, int iIdx[]){
  Abc_Obj_t* pNode;
  int i,k;
  k=0;
  Abc_NtkForEachNode(pNtk, pNode, i) {
    if(k==oIdx){
      break;
    }else{
      k++;
    }
  }
  DdManager *dd = ( DdManager * ) pNtk->pManFunc;
  int iNum = Abc_NtkPiNum(pNtk);
  long total = pow(2,iNum-2);
  long iCopy;
  int *iValue = ABC_ALLOC(int, iNum);
  for( long i=0;i<total; i++){
    iCopy = i;
    for( int j=0;j<iNum;j++){
      if(j==iIdx[0] || j==iIdx[1]){
        iValue[j]=(j==iIdx[0])?0:1;
      }else{
        iValue[j]=iCopy%2;
        iCopy/=2;
      }
    }
    // test if asymmetre
    int val = SimSinglePatternBdd(pNode, dd, iIdx, iValue);
    if(!val){
      // asymmetre
      printf("asymmetric\n");  
      for(int loop = 0; loop < iNum; loop++)
        printf("%d", iValue[loop]);
      printf("\n");
      for(int loop = 0; loop < iNum; loop++){
        if(loop==iIdx[0]){
          printf("%d",iValue[iIdx[1]]);
        }else if(loop==iIdx[1]){
          printf("%d",iValue[iIdx[0]]);
        }else{
          printf("%d", iValue[loop]);
        }
      }
      printf("\n");
      break;
    }else if(i==total-1){
      printf("symmetric\n");
    }
  }
  
  ABC_FREE(iValue);
}
int Lsv_CommandSymCheckBdd(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  Extra_UtilGetoptReset();
  if ( pNtk == NULL )
  {
      Abc_Print( -1, "Empty network.\n" );
      return 1;
  }
  if ( !Abc_NtkIsBddLogic(pNtk) )
  {
      Abc_Print( -1, "Simulation only for BDD. Try collapse.\n" );
      return 1;
  }
  if ( argc < 4 ) // TODO: check if there is input missing
  {
      Abc_Print( -1, "Missing input parameter lsv sym bdd <k> <i> <j> .\n" );
      return 1;
  }
  int oIdx, iIdx[2];
  oIdx = atoi(argv[1]);
  iIdx[0] = atoi(argv[2]);
  iIdx[1] = atoi(argv[3]);
  int iNum = Abc_NtkPiNum(pNtk);
  int oNum = Abc_NtkPoNum(pNtk);
  if (oIdx>=oNum || iIdx[0] >= iNum ||iIdx[1] >=iNum){
    Abc_Print( -1, "Input parameter mismatch circuit.\n" );
    return 1;
  } else {
    SimAllPatternBdd(pNtk, oIdx, iIdx);
    return 0;
  }
}

int EquCnf(int iVarA, int iVarB, sat_solver* solver){
  lit Lits[2];
  int Cid;
  Lits[0] = toLitCond( iVarA, 0 );
  Lits[1] = toLitCond( iVarB, 1 );
  Cid = sat_solver_addclause( solver, Lits, Lits + 2 );
  assert( Cid );
  Lits[0] = toLitCond( iVarA, 1 );
  Lits[1] = toLitCond( iVarB, 0 );
  return sat_solver_addclause( solver, Lits, Lits + 2 );
}
void SatCheckSym(Abc_Ntk_t* pNtk, int oIdx, int iIdx[]){
  int i;
  int k=0;
  Abc_Obj_t* pNode = Abc_ObjFanin0((Abc_Obj_t*)Abc_NtkPo(pNtk, oIdx)) ;
  if(pNode==NULL || !Abc_ObjIsNode(pNode)){
    Abc_Print(-1,"Empty output node.\n");
  }
  char pNodeName[7] = "output";
  int iNum = Abc_NtkPiNum(pNtk);
  Abc_Ntk_t* p = Abc_NtkCreateCone(pNtk, pNode,pNodeName, 1);
  Aig_Man_t* pAig = Abc_NtkToDar(p,0,0);
  sat_solver* solver = sat_solver_new();
  // solver->fPrintClause = 1;
  Cnf_Dat_t* pCnfA = Cnf_Derive(pAig,1);
  Cnf_DataWriteIntoSolverInt(solver,pCnfA,1,0);
  Cnf_DataLift(pCnfA,pCnfA->nVars);
  Cnf_DataWriteIntoSolverInt(solver,pCnfA,1,0);
  Cnf_DataLift(pCnfA, -pCnfA->nVars);

  Aig_Obj_t* pObj,*pObjhold;
  pObjhold=NULL;
  lit Lits[2];
  int Cid, iVarA, iVarB;
  
  Aig_ManForEachCi( pAig, pObj, i ){
    if (i==iIdx[0] || i == iIdx[1]){
      if(pObjhold==NULL){
        pObjhold=pObj;
        continue;
      }else{
        iVarA = pCnfA->pVarNums[pObj->Id];
        iVarB = pCnfA->pVarNums[pObjhold->Id];
        Lits[0] = toLitCond( iVarA, 0 );
        Lits[1] = toLitCond( iVarB, 0 );
        Cid = sat_solver_addclause( solver, Lits, Lits + 2 );
        assert(Cid);
        Lits[0] = toLitCond( iVarA,1 );
        Lits[1] = toLitCond( iVarB,1 );
        Cid = sat_solver_addclause( solver, Lits, Lits + 2 );
        assert(Cid);
        iVarA = pCnfA->pVarNums[pObj->Id];
        Cnf_DataLift(pCnfA, pCnfA->nVars);
        iVarB = pCnfA->pVarNums[pObjhold->Id];
        Cnf_DataLift(pCnfA, -pCnfA->nVars);
        Cid = EquCnf(iVarA,iVarB,solver);
        assert(Cid);
        iVarA = pCnfA->pVarNums[pObjhold->Id];
        Cnf_DataLift(pCnfA, pCnfA->nVars);
        iVarB = pCnfA->pVarNums[pObj->Id];
        Cnf_DataLift(pCnfA, -pCnfA->nVars);
      }
    }else{
      iVarA = pCnfA->pVarNums[pObj->Id];
      Cnf_DataLift(pCnfA, pCnfA->nVars);
      iVarB = pCnfA->pVarNums[pObj->Id];
      Cnf_DataLift(pCnfA, -pCnfA->nVars);
    }
    Cid = EquCnf(iVarA,iVarB,solver);
    assert(Cid);
  }
    Aig_ManForEachCo( pAig, pObj, i ){
    }
    int satVarA = pCnfA->pVarNums[pObj->Id];
    Cnf_DataLift(pCnfA, pCnfA->nVars);
    int satVarB = pCnfA->pVarNums[pObj->Id];
    Cnf_DataLift(pCnfA, -pCnfA->nVars);
    Lits[0] = toLitCond( satVarA, 0 );
    Lits[1] = toLitCond( satVarB, 0 );
    Cid = sat_solver_addclause( solver, Lits, Lits + 2 );
    assert(Cid);
    Lits[0] = toLitCond( satVarA, 1 );
    Lits[1] = toLitCond( satVarB, 1 );
    Cid = sat_solver_addclause( solver, Lits, Lits + 2 );
    assert(Cid);
    int sat = sat_solver_solve(solver,NULL,NULL,0,0,0,0);
    if(sat==1){ // satisfied
      printf("asymmetric\n");
      int v1,v2;
      v1=-1;
      v2=-1;
      Aig_ManForEachCi( pAig, pObj, i ){
        if (i==iIdx[0] || i==iIdx[1]){
          if(v1==-1){
            v1=pObj->Id;
          }else if(v2==-1){
            v2=pObj->Id;
          }
        }
        printf("%d",sat_solver_var_value(solver,pCnfA->pVarNums[pObj->Id]));
      }
      if (sat_solver_var_value(solver,pCnfA->pVarNums[v1]) == sat_solver_var_value(solver,pCnfA->pVarNums[v2])){
        printf("warning asymmetric not trigger\n");
        printf("s size = %d\n",solver->size);
      }
      printf("\n");
      Aig_ManForEachCi( pAig, pObj, i ){
          if (i==iIdx[0] || i==iIdx[1]){
            if(v2==-1)
              printf("%d",sat_solver_var_value(solver,pCnfA->pVarNums[v1]));
            else{
              printf("%d",sat_solver_var_value(solver,pCnfA->pVarNums[v2]));
              v2=-1;
            }
          }
          else
              printf("%d",sat_solver_var_value(solver,pCnfA->pVarNums[pObj->Id]));
      }
      printf("\n");
    }else{ // not satisfied
      printf("symmetric\n");
    }
}
int Lsv_CommandSymCheckSat(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  Extra_UtilGetoptReset();
  if ( pNtk == NULL )
  {
      Abc_Print( -1, "Empty network.\n" );
      return 1;
  }
  if ( !Abc_NtkIsStrash(pNtk) )
  {
      Abc_Print( -1, "Simulation only for Aig. Try strash.\n" );
      return 1;
  }
  if ( argc < 4 ) // TODO: check if there is input missing
  {
      Abc_Print( -1, "Missing input parameter lsv sym bdd <k> <i> <j> .\n" );
      return 1;
  }
  int oIdx, iIdx[2];
  oIdx = atoi(argv[1]);
  iIdx[0] = atoi(argv[2]);
  iIdx[1] = atoi(argv[3]);
  int iNum = Abc_NtkPiNum(pNtk);
  int oNum = Abc_NtkPoNum(pNtk);
  if (oIdx>=oNum || iIdx[0] >= iNum ||iIdx[1] >=iNum){
    Abc_Print( -1, "Input parameter mismatch circuit.\n" );
    return 1;
  } else {
    SatCheckSym(pNtk, oIdx, iIdx);
    return 0;
  }
}

int BufEnCnfCmpl(sat_solver* solver, int iVarA, int iVarB, int iVarEn){
  lit Lits[3];
  int Cid;
  assert( iVarA >= 0 && iVarB >= 0 && iVarEn >= 0 );

  Lits[0] = toLitCond( iVarA, 0 );
  Lits[1] = toLitCond( iVarB, 1 );
  Lits[2] = toLitCond( iVarEn, 0 );
  Cid = sat_solver_addclause( solver, Lits, Lits + 3 );
  assert( Cid );

  Lits[0] = toLitCond( iVarA, 1 );
  Lits[1] = toLitCond( iVarB, 0 );
  Lits[2] = toLitCond( iVarEn, 0 );
  Cid = sat_solver_addclause( solver, Lits, Lits + 3 );
  assert( Cid );
  return 2;
}
int EnCnf(int i, int j, int* objId, sat_solver* solver, int nVar, int psize){
  lit Lits[4];
  int Cid;
  Lits[0] = toLitCond(objId[i],0);
  Lits[1] = toLitCond(objId[j]+nVar,1);
  Lits[2] = toLitCond(psize+i, 1);
  Lits[3] = toLitCond(psize+j, 1);
  Cid = sat_solver_addclause( solver, Lits, Lits + 4 );
  assert( Cid );
  Lits[0] = toLitCond(objId[i],1);
  Lits[1] = toLitCond(objId[j]+nVar,0);
  Lits[2] = toLitCond(psize+i, 1);
  Lits[3] = toLitCond(psize+j, 1);
  Cid = sat_solver_addclause( solver, Lits, Lits + 4 );
  assert( Cid );
  Lits[0] = toLitCond(objId[j],0);
  Lits[1] = toLitCond(objId[i]+nVar,1);
  Lits[2] = toLitCond(psize+i, 1);
  Lits[3] = toLitCond(psize+j, 1);
  Cid = sat_solver_addclause( solver, Lits, Lits + 4 );
  assert( Cid );
  Lits[0] = toLitCond(objId[j],1);
  Lits[1] = toLitCond(objId[i]+nVar,0);
  Lits[2] = toLitCond(psize+i, 1);
  Lits[3] = toLitCond(psize+j, 1);
  Cid = sat_solver_addclause( solver, Lits, Lits + 4 );
  assert( Cid );
  Lits[0] = toLitCond(objId[j],1);
  Lits[1] = toLitCond(objId[i],1);
  Lits[2] = toLitCond(psize+i, 1);
  Lits[3] = toLitCond(psize+j, 1);
  Cid = sat_solver_addclause( solver, Lits, Lits + 4 );
  assert( Cid );
  Lits[0] = toLitCond(objId[j],0);
  Lits[1] = toLitCond(objId[i],0);
  Lits[2] = toLitCond(psize+i, 1);
  Lits[3] = toLitCond(psize+j, 1);
  return sat_solver_addclause( solver, Lits, Lits + 4 );
}
void IncrSatSym(Abc_Ntk_t* pNtk, int oIdx){
  Abc_Obj_t* pNode = Abc_ObjFanin0((Abc_Obj_t*)Abc_NtkPo(pNtk, oIdx)) ;
  int iNum = Abc_NtkPiNum(pNtk);
  if(pNode==NULL || !Abc_ObjIsNode(pNode)){
    Abc_Print(-1,"Empty output node.\n");
  }
  char pNodeName[7] = "output";
  Abc_Ntk_t* p = Abc_NtkCreateCone(pNtk, pNode,pNodeName, 1);
  Aig_Man_t* pAig = Abc_NtkToDar(p,0,0);
  sat_solver* solver = sat_solver_new();
  // solver->fPrintClause = 1;
  Cnf_Dat_t* pCnf = Cnf_Derive(pAig,1);
  Cnf_DataWriteIntoSolverInt(solver,pCnf,1,0);
  Cnf_DataLift(pCnf,pCnf->nVars);
  Cnf_DataWriteIntoSolverInt(solver,pCnf,1,0);
  Cnf_DataLift(pCnf, -pCnf->nVars);
  sat_solver_setnvars(solver,iNum);
  lit Lits[2];
  int iVarA,iVarB,iVarEn,i,k,psize,Cid,sat;
  psize = solver->size;
  Aig_Obj_t* pObj;
  k=0;
  int* objId= ABC_ALLOC(int, iNum);
  Aig_ManForEachCo( pAig, pObj, i ){
  }
  int satVarA = pCnf->pVarNums[pObj->Id];
  Cnf_DataLift(pCnf, pCnf->nVars);
  int satVarB = pCnf->pVarNums[pObj->Id];
  Cnf_DataLift(pCnf, -pCnf->nVars);
  Lits[0] = toLitCond( satVarA, 0 );
  Lits[1] = toLitCond( satVarB, 0 );
  Cid = sat_solver_addclause( solver, Lits, Lits + 2 );
  assert(Cid);
  Lits[0] = toLitCond( satVarA, 1 );
  Lits[1] = toLitCond( satVarB, 1 );
  Cid = sat_solver_addclause( solver, Lits, Lits + 2 );
  assert(Cid);
  Aig_ManForEachCi( pAig, pObj, i ){
    objId[k] = pCnf->pVarNums[pObj->Id];
    iVarA = pCnf->pVarNums[pObj->Id];
    Cnf_DataLift(pCnf, pCnf->nVars);
    iVarB = pCnf->pVarNums[pObj->Id];
    Cnf_DataLift(pCnf, -pCnf->nVars);
    iVarEn = psize + k;
    // iVarEn = psize;
    // psize++;
    BufEnCnfCmpl(solver,iVarA,iVarB,iVarEn);
    // sat_solver_add_buffer_enable(solver,iVarA,iVarB,iVarEn+1,0);
    k++;
  }
  // psize -=iNum;
  for (int i=0;i<iNum;i++){
    for(int j=i+1;j<iNum;j++){
      EnCnf(i,j,objId,solver,pCnf->nVars, psize);
      // sat_solver_add_buffer_enable(solver,objId[i],objId[j]+pCnf->nVars,psize+i+1,0);
      // sat_solver_add_buffer_enable(solver,objId[j],objId[i]+pCnf->nVars,psize+i+1,0);
    }
  }
  lit* Ctrl= ABC_ALLOC(lit, iNum);
  for(int i=0;i<iNum;i++){
    for(int j=i+1;j<iNum;j++){
      // Lits[0] = toLitCond(psize+i,0);
      // Lits[1] = toLitCond(psize+j,0);
      for(int k=0;k<iNum;k++){
        if(k==i || k==j)
          Ctrl[k] = toLitCond(psize+k,0);
          // Ctrl[k] = toLitCond(objId[k],0);
        else
          Ctrl[k] = toLitCond(psize+k,1);
      }
      // // sat_solver_addclause(solver,Ctrl,Ctrl+iNum);
      // // sat = sat_solver_solve(solver,NULL,NULL,0,0,0,0);
      sat = sat_solver_solve(solver,Ctrl,Ctrl+iNum,0,0,0,0);
      // sat = sat_solver_solve(solver,Lits,Lits+2,0,0,0,0);
      if(sat==-1)
        printf("%d %d\n",i,j);
      // else
      //   printf("no %d %d\n",i,j);
      // break;
    }
  }
  ABC_FREE(objId);
}
int Lsv_CommandSymIncrSat(Abc_Frame_t* pAbc, int argc, char** argv){
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  Extra_UtilGetoptReset();
  if ( pNtk == NULL )
  {
      Abc_Print( -1, "Empty network.\n" );
      return 1;
  }
  if ( !Abc_NtkIsStrash(pNtk) )
  {
      Abc_Print( -1, "Simulation only for Aig. Try strash.\n" );
      return 1;
  }
  if ( argc < 2 ) // TODO: check if there is input missing
  {
      Abc_Print( -1, "Missing input parameter lsv_sym_all <k> .\n" );
      return 1;
  }
  int oIdx;
  oIdx = atoi(argv[1]);
  int oNum = Abc_NtkPoNum(pNtk);
  if (oIdx>=oNum ){
    Abc_Print( -1, "Input parameter <k> mismatch.\n" );
    return 1;
  } else {
    IncrSatSym(pNtk, oIdx);
    return 0;
  }
}
