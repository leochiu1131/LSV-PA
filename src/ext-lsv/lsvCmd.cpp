#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <vector>
#include <set>
#include <algorithm>
#include "sat/cnf/cnf.h"
using std::vector;
using std::set;
using std::includes;

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintCut(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSDC(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandODC(Abc_Frame_t* pAbc, int argc, char** argv);
extern "C"{
Aig_Man_t* Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
}
void Lsv_NtkPrintODC(Abc_Ntk_t* pNtk,Abc_Obj_t* pObj);
void Lsv_NtkPrintSDC(Abc_Ntk_t* pNtk,Abc_Obj_t* pObj);
void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_CommandPrintCut, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sdc", Lsv_CommandSDC, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_odc", Lsv_CommandODC, 0);
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
    // printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
    // Abc_Obj_t* pFanin;
    // int j;
    // Abc_ObjForEachFanin(pObj, pFanin, j) {
    //   printf("  Fanin-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanin), Abc_ObjName(pFanin));
    // }
    // if (Abc_NtkHasSop(pNtk)) {
    //   printf("The SOP of this node:\n%s", (char*)pObj->pData);
    // }
    // if(Abc_ObjFaninNum(pObj)!=0){
      printf("sdc of node %d : \n",i);
      Lsv_NtkPrintSDC(pNtk,pObj);
    // }
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

void printcutmsg(){
  Abc_Print(-2, "usage: lsv_printcut <k> [-h]\n");
  Abc_Print(-2, "\t        prints the <k> feasable cuts in the network,3<=k<=6\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
}

void printsdcmsg(){
  Abc_Print(-2, "usage: lsv_sdc <n> [-h]\n");
  Abc_Print(-2, "\t        prints the SDCs of internal node <n> in terms of the fanins\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
}

void printodcmsg(){
  Abc_Print(-2, "usage: lsv_odc <n> [-h]\n");
  Abc_Print(-2, "\t        prints the ODCs of internal node <n> with respect to all primary outputs in terms of its fanins\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
}
int Abc_NtkMyAppend( Abc_Ntk_t * pNtk1, Abc_Ntk_t * pNtk2, int fAddPos )
{
    Abc_Obj_t * pObj;
    char * pName;
    int i, nNewCis;
    // the first network should be an AIG
    assert( Abc_NtkIsStrash(pNtk1) );
    assert( Abc_NtkIsLogic(pNtk2) || Abc_NtkIsStrash(pNtk2) ); 
    if ( Abc_NtkIsLogic(pNtk2) && !Abc_NtkToAig(pNtk2) )
    {
        printf( "Converting to AIGs has failed.\n" );
        return 0;
    }
    // check that the networks have the same PIs
    // reorder PIs of pNtk2 according to pNtk1
    // if ( !Abc_NtkCompareSignals( pNtk1, pNtk2, 1, 1 ) )
    //     printf( "Abc_NtkAppend(): The union of the network PIs is computed (warning).\n" );
    // perform strashing
    nNewCis = 0;
    Abc_NtkCleanCopy( pNtk2 );
    if ( Abc_NtkIsStrash(pNtk2) )
        Abc_AigConst1(pNtk2)->pCopy = Abc_AigConst1(pNtk1);
    Abc_NtkForEachCi( pNtk2, pObj, i )
    {
        pName = Abc_ObjName(pObj);
        pObj->pCopy = Abc_NtkFindCi(pNtk1, Abc_ObjName(pObj));
        if ( pObj->pCopy == NULL )
        {
            pObj->pCopy = Abc_NtkDupObj(pNtk1, pObj, 1);
            nNewCis++;
        }
    }
    if ( nNewCis )
        printf( "Warning: Procedure Abc_NtkAppend() added %d new CIs.\n", nNewCis );
    // add pNtk2 to pNtk1 while strashing
    Abc_NtkForEachNode( pNtk2, pObj, i )
        pObj->pCopy = Abc_AigAnd( (Abc_Aig_t *)pNtk1->pManFunc, Abc_ObjChild0Copy(pObj), Abc_ObjChild1Copy(pObj) );
    // add the COs of the second network
    if ( fAddPos )
    {
        Abc_NtkForEachPo( pNtk2, pObj, i )
        {
            Abc_NtkDupObj( pNtk1, pObj, 0 );
            Abc_ObjAddFanin( pObj->pCopy, Abc_ObjChild0Copy(pObj) );
            Abc_ObjAssignName( pObj->pCopy, Abc_ObjName(pObj), NULL );
        }
    }
    else
    {
        Abc_Obj_t * pObjOld, * pDriverOld, * pDriverNew;
        int fCompl, iNodeId;
        // OR the choices
        Abc_NtkForEachCo( pNtk2, pObj, i )
        {
            iNodeId = Nm_ManFindIdByNameTwoTypes( pNtk1->pManName, Abc_ObjName(pObj), ABC_OBJ_PO, ABC_OBJ_BI );
//            if ( iNodeId < 0 )
//                continue;
            assert( iNodeId >= 0 );
            pObjOld = Abc_NtkObj( pNtk1, iNodeId );
            // derive the new driver
            pDriverOld = Abc_ObjChild0( pObjOld );
            pDriverNew = Abc_ObjChild0Copy( pObj );
            pDriverNew = Abc_AigOr( (Abc_Aig_t *)pNtk1->pManFunc, pDriverOld, pDriverNew );
            if ( Abc_ObjRegular(pDriverOld) == Abc_ObjRegular(pDriverNew) )
                continue;
            // replace the old driver by the new driver
            fCompl = Abc_ObjRegular(pDriverOld)->fPhase ^ Abc_ObjRegular(pDriverNew)->fPhase;
            Abc_ObjPatchFanin( pObjOld, Abc_ObjRegular(pDriverOld), Abc_ObjNotCond(Abc_ObjRegular(pDriverNew), fCompl) );
        }
    }
    // make sure that everything is okay
    if ( !Abc_NtkCheck( pNtk1 ) )
    {
        printf( "Abc_NtkAppend: The network check has failed.\n" );
        return 0;
    }
    return 1;
}
void Lsv_NtkPrintSDC(Abc_Ntk_t* pNtk,Abc_Obj_t* pObj){
  int minterms[4]={0,0,0,0};//00 01 10 11
  Abc_Obj_t* pFanin;
  int j;
  Vec_Ptr_t * vNodes;
  vNodes = Vec_PtrAlloc( 0 );
  Abc_Ntk_t* cone;
  Aig_Man_t* man;
  Cnf_Dat_t* cnf;
  int value[4][2];
  Abc_ObjForEachFanin(pObj, pFanin, j) {
    Vec_PtrPush( vNodes, pFanin );
  }
  Abc_Obj_t* pObjtemp;
  int ii;
  cone=Abc_NtkCreateConeArray(pNtk,vNodes,1);
  man=Abc_NtkToDar(cone,0,0);
  cnf=Cnf_Derive( man, Aig_ManCoNum(man) );
  int * pattern = new int[Abc_NtkCiNum(cone)];
  for (int ii=0;ii<Abc_NtkCiNum(cone);ii++){
    pattern[ii]=0;
  }
  value[0][0] = Abc_NtkVerifySimulatePattern(cone, pattern)[0];
  value[0][1] = Abc_NtkVerifySimulatePattern(cone, pattern)[1];
  for (int ii=0;ii<Abc_NtkCiNum(cone);ii++){
    pattern[ii]=1;
  }
  value[1][0] = Abc_NtkVerifySimulatePattern(cone, pattern)[0];
  value[1][1] = Abc_NtkVerifySimulatePattern(cone, pattern)[1];
  int temp = 0;
  for (int ii=0;ii<Abc_NtkCiNum(cone);ii++){
    pattern[ii]=temp;
    temp = 1 - temp;
  }
  value[2][0] = Abc_NtkVerifySimulatePattern(cone, pattern)[0];
  value[2][1] = Abc_NtkVerifySimulatePattern(cone, pattern)[1];
  temp = 1;
  for (int ii=0;ii<Abc_NtkCiNum(cone);ii++){
    pattern[ii]=temp;
    temp = 1 - temp;
  }
  value[3][0] = Abc_NtkVerifySimulatePattern(cone, pattern)[0];
  value[3][1] = Abc_NtkVerifySimulatePattern(cone, pattern)[1];
  for(int iii=0;iii<4;iii++){
    value[iii][0] ^= Abc_ObjFaninC0(pObj);
    value[iii][1] ^= Abc_ObjFaninC1(pObj);
    minterms[value[iii][0]*2+value[iii][1]]=1;
  }
  int sum=0;
  for(int iii=0;iii<4;iii++){
    sum+=minterms[iii];
  }
  if(sum==4){
    printf("no sdc\n");
    return;
  }
  sat_solver* mysolver=sat_solver_new();
  Cnf_DataWriteIntoSolverInt(mysolver,cnf,1,1);
  int assumption[2];
  int ids[2];
  Abc_NtkForEachCo(cone, pObjtemp, ii){
    ids[ii]=pObjtemp->Id;
  }
  for(int ii=0;ii<2;ii++){
    for(int iii=0;iii<2;iii++){
      if (minterms[ii*2+iii]==1)continue;
      assumption[0]=2*(cnf->pVarNums[ids[0]])+1^ii^Abc_ObjFaninC0(pObj);
      assumption[1]=2*(cnf->pVarNums[ids[1]])+1^iii^Abc_ObjFaninC1(pObj);
      if(sat_solver_solve(mysolver,assumption,assumption+2,0,0,0,0)==1){
        minterms[ii*2+iii]=1;
      }
      else{
        printf("%d%d ",ii,iii);
      }
    }
  }
  sum=0;
  for(int iii=0;iii<4;iii++){
    sum+=minterms[iii];
  }
  if(sum==4){
    printf("no sdc\n");
  }
  else printf("\n");
}

void Lsv_NtkPrintODC(Abc_Ntk_t* pNtk,Abc_Obj_t* pObj) {
  Abc_Obj_t* pObjtemp;
  Aig_Obj_t* pAigObj;
  Abc_Ntk_t* miter;
  Abc_Ntk_t* pNtk2=Abc_NtkDup(pNtk);
  int i;
  // Abc_ObjForEachFanout(Abc_NtkObj(pNtk2,Abc_ObjId(pObj)),pObjtemp,i){
  //   Abc_Obj_t* pFanin;
  //   int j;
  //   Abc_ObjForEachFanin(pObjtemp, pFanin, j) {
  //     if(Abc_ObjId(pFanin)==Abc_ObjId(pObj))Abc_ObjXorFaninC(pObjtemp,j);
  //   }
  // }
  std::string name1=Abc_ObjName(pObj);
  std::string name2;
  Abc_NtkForEachObj(pNtk2, pObjtemp, i) {
    Abc_Obj_t* pFanin;
    int j;
    Abc_ObjForEachFanin(pObjtemp, pFanin, j) {
      name2=Abc_ObjName(pFanin);
      if(name1==name2){
        Abc_ObjXorFaninC(pObjtemp,j);
        // printf("%u %u %u\n",Abc_ObjId(pFanin),Abc_ObjId(pObj),Abc_ObjId(pObjtemp));
      }
    }
  }
  miter=Abc_NtkMiter(pNtk,pNtk2,1,0,0,0);
  Abc_Ntk_t* cone;
  Vec_Ptr_t * vNodes;
  Abc_Obj_t* pFanin;
  // Abc_Obj_t* pCone1;
  // Abc_Obj_t* pCone2;
  Aig_Man_t* manmiter;
  Aig_Man_t* mancone;
  Cnf_Dat_t* cnfmiter;
  Cnf_Dat_t* cnfcone;
  std::string name3;
  int ii;
  vNodes = Vec_PtrAlloc( 2 );
  Abc_ObjForEachFanin(pObj, pFanin, ii) {
    name1=Abc_ObjName(pFanin);
    // printf("%s\n",name1.c_str());
    Vec_PtrPush( vNodes, pFanin );
  }
  cone=Abc_NtkCreateConeArray(pNtk,vNodes,1);
  // name3=Abc_ObjName(Abc_NtkCo(cone,0));
  // printf("%s\n",name3.c_str());
  // name3=Abc_ObjName(Abc_NtkCo(cone,1));
  // printf("%s\n",name3.c_str());
  Abc_NtkMyAppend(miter,cone,1);
  // name3=Abc_ObjName(Abc_NtkCo(cone,0));
  // printf("%s\n",name3.c_str());
  // name3=Abc_ObjName(Abc_NtkCo(cone,1));
  // printf("%s\n",name3.c_str());
  // pCone1=Abc_NtkCreateObj(pNtk,ABC_OBJ_PO);
  // Abc_ObjAssignName( pCone1, "ConeCo1", NULL );
  manmiter=Abc_NtkToDar(miter,0,0);
  cnfmiter=Cnf_Derive( manmiter, Aig_ManCoNum(manmiter) );
  mancone=Abc_NtkToDar(cone,0,0);
  cnfcone=Cnf_Derive( mancone, Aig_ManCoNum(mancone) );
  sat_solver* mitersolver=sat_solver_new();
  sat_solver* conesolver=sat_solver_new();
  Cnf_DataWriteIntoSolverInt(mitersolver,cnfmiter,1,1);
  Cnf_DataWriteIntoSolverInt(conesolver,cnfcone,1,1);
  int miterassumption[3];
  int coneassumption[2];
  int miterids[3];
  int coneids[2];
  // int ciids[2];
  // Cnf_DataPrint(cnfmiter, 1);
  Aig_ManForEachCo(manmiter, pAigObj, i) {
    miterids[i]=cnfmiter->pVarNums[ Aig_ObjId(pAigObj)];
    // printf("%d\n", cnfmiter->pVarNums[ Aig_ObjId(pAigObj)]);
  }
  Aig_ManForEachCo(mancone, pAigObj, i) {
    coneids[i]=cnfcone->pVarNums[ Aig_ObjId(pAigObj)];
  }
  // printf("%s : %d\n",Abc_ObjName(Abc_ObjFanin0(pObj)),Abc_ObjFaninId0(pObj)/*,Abc_ObjName(Abc_ObjFanin1(pObj)),Abc_ObjFaninId1(pObj)*/);
  // printf("%s : %d\n",/*Abc_ObjName(Abc_ObjFanin0(pObj)),Abc_ObjFaninId0(pObj),*/Abc_ObjName(Abc_ObjFanin1(pObj)),Abc_ObjFaninId1(pObj));
  // printf("%s : %d, %s : %d\n",Abc_ObjName(Abc_ObjFanin0(pObj)),Abc_ObjFaninId0(pObj),Abc_ObjName(Abc_ObjFanin1(pObj)),Abc_ObjFaninId1(pObj));
  // printf("%s : %d, %s : %d\n",/*Abc_ObjName(Abc_NtkCo(miter,1)),Abc_ObjFaninId0(Abc_NtkCo(miter,1)),Abc_ObjName(Abc_NtkCo(miter,2)),Abc_ObjFaninId0(Abc_NtkCo(miter,2)),*/Abc_ObjName(Abc_ObjFanin0(pObj)),Abc_ObjFaninId0(pObj),Abc_ObjName(Abc_ObjFanin1(pObj)),Abc_ObjFaninId1(pObj));
  // if(Abc_ObjFaninId0(Abc_NtkCo(miter,1))>Abc_ObjFaninId0(Abc_NtkCo(miter,2))){
  //   int temp=miterids[1];
  //   miterids[1]=miterids[2];
  //   miterids[2]=temp;
  //   printf("%d %d %d %d\n",Abc_ObjFaninId0(Abc_NtkCo(miter,1)),Abc_ObjFaninId0(Abc_NtkCo(miter,2)),Abc_ObjFaninId0(pObj),Abc_ObjFaninId1(pObj));
  // }
  // name1=Abc_ObjName(Abc_ObjFanin0(pObj));
  // name2=Abc_ObjName(Abc_NtkCo(miter,1));
  // if(name1!=name2)printf("fanin0 of target is %s, not equal to fanin of output, which is %s\n",name1.c_str(),name2.c_str());
  // name1=Abc_ObjName(Abc_ObjFanin1(pObj));
  // name2=Abc_ObjName(Abc_NtkCo(miter,2));
  // if(name1!=name2)printf("fanin1 of target is %s, not equal to fanin of output, which is %s\n",name1.c_str(),name2.c_str());
  // Abc_NtkForEachObj(pNtk, pObjtemp, i) {
  //   printf("Object Id = %d, name = %s, cnf Id = %d\n", Abc_ObjId(pObjtemp), Abc_ObjName(pObjtemp),cnfmiter->pVarNums[Abc_ObjId(pObjtemp)]);
  //   Abc_Obj_t* pFanin;
  //   int j;
  //   Abc_ObjForEachFanin(pObjtemp, pFanin, j) {
  //     printf("  Fanin-%d: Id = %d, name = %s, comp = %d\n", j, Abc_ObjId(pFanin), Abc_ObjName(pFanin), Abc_ObjFaninC(pObjtemp,j));
  //   }
  // }
  // Abc_NtkForEachObj(pNtk2, pObjtemp, i) {
  //   printf("Object Id = %d, name = %s, cnf Id = %d\n", Abc_ObjId(pObjtemp), Abc_ObjName(pObjtemp),cnfmiter->pVarNums[Abc_ObjId(pObjtemp)]);
  //   Abc_Obj_t* pFanin;
  //   int j;
  //   Abc_ObjForEachFanin(pObjtemp, pFanin, j) {
  //     printf("  Fanin-%d: Id = %d, name = %s, comp = %d\n", j, Abc_ObjId(pFanin), Abc_ObjName(pFanin), Abc_ObjFaninC(pObjtemp,j));
  //   }
  // }
  // Abc_NtkForEachObj(miter, pObjtemp, i) {
  //   printf("Object Id = %d, name = %s, cnf Id = %d\n", Abc_ObjId(pObjtemp), Abc_ObjName(pObjtemp),cnfmiter->pVarNums[Abc_ObjId(pObjtemp)]);
  //   Abc_Obj_t* pFanin;
  //   int j;
  //   Abc_ObjForEachFanin(pObjtemp, pFanin, j) {
  //     printf("  Fanin-%d: Id = %d, name = %s, comp = %d\n", j, Abc_ObjId(pFanin), Abc_ObjName(pFanin), Abc_ObjFaninC(pObjtemp,j));
  //   }
  // }
  // Abc_NtkForEachObj(cone, pObjtemp, i) {
  //   printf("Object Id = %d, name = %s, cnf Id = %d\n", Abc_ObjId(pObjtemp), Abc_ObjName(pObjtemp),cnfmiter->pVarNums[Abc_ObjId(pObjtemp)]);
  //   Abc_Obj_t* pFanin;
  //   int j;
  //   Abc_ObjForEachFanin(pObjtemp, pFanin, j) {
  //     printf("  Fanin-%d: Id = %d, name = %s, comp = %d\n", j, Abc_ObjId(pFanin), Abc_ObjName(pFanin), Abc_ObjFaninC(pObjtemp,j));
  //   }
  // }
  // Cnf_DataPrint(cnfmiter,1);
  // Abc_NtkForEachCo(miter, pObjtemp, ii){
  //   miterids[ii]=pObjtemp->Id;
  //   printf("miter Co #%d : name = %s, Obj Id = %d, cnf Id = %d\n",ii,Abc_ObjName(pObjtemp),pObjtemp->Id,cnfmiter->pVarNums[miterids[ii]]);
  // }
  // Aig_ManForEachCi(manmiter, pAigObj, ii){
  //   ciids[ii]=cnfmiter->pVarNums[Aig_ObjId(pAigObj)];
  // }
  // Abc_NtkForEachCo(cone, pObjtemp, ii){
  //   coneids[ii]=pObjtemp->Id;
  //   printf("cone Co #%d : name = %s, Obj Id = %d, cnf Id = %d\n",ii,Abc_ObjName(pObjtemp),pObjtemp->Id,cnfcone->pVarNums[coneids[ii]]);
  // }
  int notodc[4]={0,0,0,0};
  miterassumption[0]=2*(miterids[0]);
  // int pattern[2]={0,0};
  // printf("0 0 Ntk: %d\n",Abc_NtkVerifySimulatePattern(pNtk, pattern)[0]);
  // printf("0 0 Ntk2: %d\n",Abc_NtkVerifySimulatePattern(pNtk2, pattern)[0]);
  // printf("0 0 miter: %d %d %d\n",Abc_NtkVerifySimulatePattern(miter, pattern)[0],Abc_NtkVerifySimulatePattern(miter, pattern)[1],Abc_NtkVerifySimulatePattern(miter, pattern)[2]);
  // pattern[1]=1;
  // printf("0 1 Ntk: %d\n",Abc_NtkVerifySimulatePattern(pNtk, pattern)[0]);
  // printf("0 1 Ntk2: %d\n",Abc_NtkVerifySimulatePattern(pNtk2, pattern)[0]);
  // printf("0 1 miter: %d %d %d\n",Abc_NtkVerifySimulatePattern(miter, pattern)[0],Abc_NtkVerifySimulatePattern(miter, pattern)[1],Abc_NtkVerifySimulatePattern(miter, pattern)[2]);
  // pattern[1]=0;
  // pattern[0]=1;
  // printf("1 0 Ntk: %d\n",Abc_NtkVerifySimulatePattern(pNtk, pattern)[0]);
  // printf("1 0 Ntk2: %d\n",Abc_NtkVerifySimulatePattern(pNtk2, pattern)[0]);
  // printf("1 0 miter: %d %d %d\n",Abc_NtkVerifySimulatePattern(miter, pattern)[0],Abc_NtkVerifySimulatePattern(miter, pattern)[1],Abc_NtkVerifySimulatePattern(miter, pattern)[2]);
  // pattern[1]=1;
  // printf("1 1 Ntk: %d\n",Abc_NtkVerifySimulatePattern(pNtk, pattern)[0]);
  // printf("1 1 Ntk2: %d\n",Abc_NtkVerifySimulatePattern(pNtk2, pattern)[0]);
  // printf("1 1 miter: %d %d %d\n",Abc_NtkVerifySimulatePattern(miter, pattern)[0],Abc_NtkVerifySimulatePattern(miter, pattern)[1],Abc_NtkVerifySimulatePattern(miter, pattern)[2]);
  for(int ii=0;ii<2;ii++){
    for(int iii=0;iii<2;iii++){
      miterassumption[1]=2*(miterids[1])+1^ii^Abc_ObjFaninC0(pObj);
      miterassumption[2]=2*(miterids[2])+1^iii^Abc_ObjFaninC1(pObj);
      coneassumption[0]=2*(coneids[0])+1^ii^Abc_ObjFaninC0(pObj);
      coneassumption[1]=2*(coneids[1])+1^iii^Abc_ObjFaninC1(pObj);
      int satans=sat_solver_solve(mitersolver,miterassumption,miterassumption+3,0,0,0,0);
      int coneans=-2;
      // printf("%d %d : miter %d %d %d , cone %d %d\n",ii,iii,miterassumption[0],miterassumption[1],miterassumption[2],coneassumption[0],coneassumption[1]);
      if(satans==-1){
        coneans=sat_solver_solve(conesolver,coneassumption,coneassumption+2,0,0,0,0);
        if (coneans==1)printf("%d%d ",ii,iii);
        else notodc[ii*2+iii]=1;
      }
      else notodc[ii*2+iii]=1;
      // printf("%d %d : %d %d\n",ii,iii,satans,coneans);
      // if(sat_solver_solve(mitersolver,miterassumption,miterassumption+3,1000000,1000000,1000000,1000000)==1)printf("miter solution : %d %d\n",sat_solver_var_value(mitersolver,ciids[0]),sat_solver_var_value(mitersolver,ciids[1]));
    }
  }
  int sum=0;
  for(int iii=0;iii<4;iii++){
    sum+=notodc[iii];
  }
  if(sum==4){
    printf("no odc\n");
  }
  else printf("\n");
}

void find_cut(Abc_Ntk_t* pNtk, Abc_Obj_t* pObj, vector<vector<set<int>>>& cuts,vector<int>& cuts_checked, int k){
  int id=Abc_ObjId(pObj);
  if(Abc_ObjFanoutNum(pObj)==0&&Abc_ObjFaninNum(pObj)==0)return;
  if(Abc_ObjFaninNum(pObj)==0&&cuts_checked[id]!=1){
    cuts[id].push_back(set<int>{id});
    cuts_checked[id]=1;
    return;
  }
  cuts[id].push_back(set<int>{id});
  Abc_Obj_t* pFanin;
  int j;
  vector<Abc_Obj_t*> pins;
  Abc_ObjForEachFanin(pObj, pFanin, j) {
    if(cuts_checked[Abc_ObjId(pFanin)]!=1)find_cut(pNtk,pFanin,cuts,cuts_checked,k);
    pins.push_back(pFanin);
  }
  if(pins.size()==1){
    for(auto i=cuts[Abc_ObjId(pins[0])].begin();i!=cuts[Abc_ObjId(pins[0])].end();i++)cuts[id].push_back(*i);
    return;
  }
  set<int> new_cut;
  for(int i=0;i<cuts[Abc_ObjId(pins[0])].size();i++){
    for(int ii=0;ii<cuts[Abc_ObjId(pins[1])].size();ii++){
      new_cut.clear();
      new_cut.insert(cuts[Abc_ObjId(pins[0])][i].begin(),cuts[Abc_ObjId(pins[0])][i].end());
      new_cut.insert(cuts[Abc_ObjId(pins[1])][ii].begin(),cuts[Abc_ObjId(pins[1])][ii].end());
      int add=1;
      for(int iii=0;iii<cuts[id].size();iii++){
        if(new_cut.size()>k||includes(new_cut.begin(),new_cut.end(),cuts[id][iii].begin(),cuts[id][iii].end())){
          add=0;
          break;
        }
        if(includes(cuts[id][iii].begin(),cuts[id][iii].end(),new_cut.begin(),new_cut.end())){
          cuts[id].erase(cuts[id].begin()+iii);
          iii--;
        }
      }
      if(add==1){
        cuts[id].push_back(new_cut);
      }
    }
  }
  cuts_checked[id]=1;
}

void Lsv_NtkPrintCut(Abc_Ntk_t* pNtk,int k) {
  Abc_Obj_t* pObj;
  int i;
  vector<vector<set<int>>> cuts(Abc_NtkObjNum(pNtk)+1);
  vector<int> cuts_checked(Abc_NtkObjNum(pNtk)+1,0);
  Abc_NtkForEachObj(pNtk, pObj, i) {
    if(cuts_checked[Abc_ObjId(pObj)]!=1)find_cut(pNtk,pObj,cuts,cuts_checked,k);
    int id=Abc_ObjId(pObj);
    for(int ii=0;ii<cuts[id].size();ii++){
      printf("%d:",id);
      for(auto iii=cuts[id][ii].begin();iii!=cuts[id][ii].end();iii++){
        printf(" %d", *iii);
      }
      printf("\n");
    }
  }
}

int Lsv_CommandPrintCut(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c;
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
    switch (c) {
      case 'h':
        printcutmsg();
        return 1;
      default:
        printcutmsg();
        return 1;
    }
  }
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  if(argc==1){
    printf("please provide k\n");
    printcutmsg();
    return 1;
  }
  if(argc>2){
    printf("too much assign value of k\n");
    printcutmsg();
    return 1;
  }
  if(strlen(argv[1])>1){
    printf("k out of range\n");
    printcutmsg();
    return 1;
  }
  int k=argv[1][0]-'0';
  if(k<3||k>6){
    printf("k out of range\n");
    printcutmsg();
    return 1;
  }
  Lsv_NtkPrintCut(pNtk,k);
  return 0;
}

int Lsv_CommandSDC(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c;
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
    switch (c) {
      case 'h':
        printsdcmsg();
        return 1;
      default:
        printsdcmsg();
        return 1;
    }
  }
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  if(argc==1){
    printf("please provide n\n");
    printsdcmsg();
    return 1;
  }
  if(argc>2){
    printf("too much assign value of n\n");
    printsdcmsg();
    return 1;
  }
  int n=0;
  for(int i=0;i<strlen(argv[1]);i++){
    n*=10;
    n+=argv[1][i]-'0';
  }
  if(n>=pNtk->nObjs){
    printf("n out of range\n");
    printsdcmsg();
    return 1;
  }
  Abc_Obj_t* pObj = Abc_NtkObj(pNtk, n);
  if(Abc_ObjFaninNum(pObj)==0 && Abc_ObjFanoutNum(pObj)==0){
    printf("node n not a internal node\n");
    printsdcmsg();
    return 1;
  }
  Lsv_NtkPrintSDC(pNtk,pObj);
  return 0;
}

int Lsv_CommandODC(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c;
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
    switch (c) {
      case 'h':
        printodcmsg();
        return 1;
      default:
        printodcmsg();
        return 1;
    }
  }
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  if(argc==1){
    printf("please provide n\n");
    printodcmsg();
    return 1;
  }
  if(argc>2){
    printf("too much assign value of n\n");
    printodcmsg();
    return 1;
  }
  int n=0;
  for(int i=0;i<strlen(argv[1]);i++){
    n*=10;
    n+=argv[1][i]-'0';
  }
  if(n>=pNtk->nObjs){
    printf("n out of range\n");
    printodcmsg();
    return 1;
  }
  Abc_Obj_t* pObj = Abc_NtkObj(pNtk, n);
  if(Abc_ObjFaninNum(pObj)==0){
    printf("node n has no fanin\n");
    printodcmsg();
    return 1;
  }
  if(Abc_ObjFanoutNum(pObj)==0){
    printf("no odc\n");
    return 1;
  }
  Lsv_NtkPrintODC(pNtk,pObj);
  return 0;
}