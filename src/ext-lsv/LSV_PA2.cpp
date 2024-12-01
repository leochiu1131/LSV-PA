#include "base/abc/abc.h"
#include "base/abc/abcShow.c"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include<vector>
#include<algorithm> 
#include<iostream>
#include<random>
#include<limits>
#include<cstddef>
#include<string.h>
#include "sat/cnf/cnf.h"

#define sizeSize_t 63 
extern "C"{Aig_Man_t* Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );}

static int Lsv_CommandPrintSDC(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintODC(Abc_Frame_t* pAbc, int argc, char** argv);


void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sdc", Lsv_CommandPrintSDC, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_odc", Lsv_CommandPrintODC, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

void setBit(int &num, size_t position, bool value) {
  if(value)
    num |= (1UL << position);
  else
    num &= ~(1UL << position);
  return;
}


// Function to check if a bit at a specific position is set
inline bool getBit(size_t num, size_t position) {
    return (num & (1UL << position)) != 0;
}

void simulation(Abc_Ntk_t* pNtk, std::vector<size_t>& simulatedPatterns, std::vector<bool>& simulated, int nodeId)
{
  Abc_Obj_t* pObj=Abc_NtkObj(pNtk, nodeId);
  if(simulated[nodeId])
    return;
  if(Abc_ObjIsPi(pObj))
  {
    static std::mt19937 eng(445);
    std::uniform_int_distribution<size_t> distr(std::numeric_limits<size_t>::min(), std::numeric_limits<size_t>::max());
    simulatedPatterns[Abc_ObjId(pObj)]=distr(eng);
    simulated[Abc_ObjId(pObj)]=1;
    return;
  }
  if(Abc_AigNodeIsConst(pObj))
  {
    simulatedPatterns[Abc_ObjId(pObj)]=~static_cast<size_t>(0);
    simulated[Abc_ObjId(pObj)]=1;
    return;
  }
  if(!Abc_ObjIsNode(pObj))  
    return;  
  if (!simulated[Abc_ObjFaninId0(pObj)])
    simulation(pNtk, simulatedPatterns, simulated, Abc_ObjFaninId0(pObj));
  if (!simulated[Abc_ObjFaninId1(pObj)])
    simulation(pNtk, simulatedPatterns, simulated, Abc_ObjFaninId1(pObj));
  
  simulatedPatterns[nodeId]=(Abc_ObjFaninC(pObj, 0) ? ~simulatedPatterns[Abc_ObjFaninId0(pObj)]:simulatedPatterns[Abc_ObjFaninId0(pObj)])& 
                            (Abc_ObjFaninC(pObj, 1) ? ~simulatedPatterns[Abc_ObjFaninId1(pObj)]:simulatedPatterns[Abc_ObjFaninId1(pObj)]);
  simulated[nodeId]=1;
  return;
}

bool checkSDC(Abc_Ntk_t* pNtk, std::vector<size_t> nodeId, std::vector<bool> SDCCandidate)
{
  Vec_Ptr_t *pEndPointsArray=Vec_PtrAlloc(2); //nCap???
  Vec_PtrPush(pEndPointsArray, Abc_NtkObj(pNtk, nodeId[0]));
  Vec_PtrPush(pEndPointsArray, Abc_NtkObj(pNtk, nodeId[1]));
  Abc_Ntk_t *pNtkSDC=Abc_NtkCreateConeArray(pNtk, pEndPointsArray, 1); //fUseAllCis=1
  Aig_Man_t *pAigSDC=Abc_NtkToDar(pNtkSDC, 0, 0);
  sat_solver *pSat=sat_solver_new();
  Cnf_Dat_t *pCnfSDC=Cnf_Derive(pAigSDC, 2);
  Cnf_DataWriteIntoSolverInt(pSat, pCnfSDC, 1, 0);
  lit clause0[1], clause1[1];
  clause0[0]=Abc_Var2Lit(pCnfSDC->pVarNums[Aig_ObjId(Aig_ManCo(pAigSDC, 0))], !SDCCandidate[0]);
  clause1[0]=Abc_Var2Lit(pCnfSDC->pVarNums[Aig_ObjId(Aig_ManCo(pAigSDC, 1))], !SDCCandidate[1]);
  sat_solver_addclause(pSat, clause0, clause0+1);
  sat_solver_addclause(pSat, clause1, clause1+1);
  if(sat_solver_solve( pSat, NULL, NULL, 0, 0, 0, 0)<0)
    return 1;
  return 0;
}

void Lsv_NtkGetSDC(Abc_Ntk_t* pNtk, std::vector<bool>& SDCCandidates, int n)
{
  std::vector<size_t> simulatedPatterns(Vec_PtrSize((pNtk)->vObjs));
  std::vector<bool> simulated(Vec_PtrSize((pNtk)->vObjs), 0);
  Abc_Obj_t * pObj=Abc_NtkObj(pNtk, n);
  simulation(pNtk, simulatedPatterns, simulated, Abc_ObjFaninId0(pObj));
  simulation(pNtk, simulatedPatterns, simulated, Abc_ObjFaninId1(pObj));
  //00 01 10 11 
  for(int i=0;i<sizeSize_t;++i)
  {
    int care=getBit(simulatedPatterns[Abc_ObjId(Abc_ObjFanin0(pObj))], i)+2*getBit(simulatedPatterns[Abc_ObjId(Abc_ObjFanin1(pObj))], i);
    SDCCandidates[care]=0;
  }
  for(int i=0;i<4;++i)
  {
    if(!SDCCandidates[i])
      continue;
    if(!checkSDC(pNtk, std::vector<size_t> {Abc_ObjId(Abc_ObjFanin0(pObj)), Abc_ObjId(Abc_ObjFanin1(pObj))}, std::vector<bool> {getBit(i, 0), getBit(i, 1)}))
      SDCCandidates[i]=0;
  }
  return;
}

void Lsv_NtkPrintSDC(Abc_Ntk_t* pNtk, int n)
{
  std::vector<bool> SDC(4, 1);
  Lsv_NtkGetSDC(pNtk, SDC, n);
  bool noSDC=1;
  for(int i=0;i<4;++i)
  {
    if(SDC[i])
    {
      noSDC=0;
      std::cout<<(Abc_ObjFaninC(Abc_NtkObj(pNtk, n), 0) ? !getBit(i, 0):getBit(i, 0));
      std::cout<<(Abc_ObjFaninC(Abc_NtkObj(pNtk, n), 1) ? !getBit(i, 1):getBit(i, 1));
      std::cout<<" "; 
    }
  }
  if(noSDC)
    std::cout<<"no sdc";
  std::cout<<"\n";
}

int Lsv_CommandPrintSDC( Abc_Frame_t * pAbc, int argc, char ** argv )
{
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  size_t sizeArgv=strlen(argv[1]);
  // int c, k;
  // std::cout<<argv[1]<<std::endl;
  // int n=argv[1][0]-'0';
  // if(argv[1][0])
  int n=0;
  for(int i=0;i<sizeArgv;++i)
    n=10*n+argv[1][i]-'0';
  // std::cout<<"argv= "<<strlen(argv[1])<<std::endl;
  // std::cout<<"n= "<<n<<std::endl;
  if(!pNtk) 
  {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }


  //if(!Abc_ObjIsNode(Abc_NtkObj(pNtk, n)))
  //  std::cout<<"fuck you cheater fuck n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"<<std::endl;

  Lsv_NtkPrintSDC(pNtk, n);
  return 0;
}

void checkODC(Abc_Ntk_t* pNtk, int n, std::vector<bool> &ODCCandidate)
{
  Abc_Ntk_t *pNtkWithnInver=Abc_NtkDup(pNtk);
  //Abc_Obj_t * pObjn=Abc_NtkObj(pNtkWithnInver, n);
  Abc_Obj_t * pFanout;
  //std::cout<<"sdvfsdvbsrbgfsd="<<Abc_ObjFaninId(Abc_NtkObj(pNtk, 6), 0)<<std::endl;
  //std::cout<<"sdvfsdvbsrbgfsd="<<Abc_ObjFaninId(Abc_NtkObj(pNtk, 7), 0)<<std::endl;
  for (int i = 0; (i < Abc_ObjFanoutNum(Abc_NtkObj(pNtkWithnInver, n))) && (((pFanout) = Abc_ObjFanout(Abc_NtkObj(pNtkWithnInver, n), i)), 1); i++ )
  {
    //std::cout<<"Abc_ObjId(pFanout)"<<Abc_ObjId(pFanout)<<std::endl;
    if(Abc_ObjFaninId(pFanout, 0)==n)
    {
      //std::cout<<"FaninId="<<n<<std::endl;
      //Abc_ObjFaninC(pFanout, 0) ? pFanout->fCompl0=0: pFanout->fCompl0=1; 
      Abc_ObjXorFaninC(pFanout, 0); 
    }
      
    else if(Abc_ObjFaninId(pFanout, 1)==n)
    {
      //std::cout<<"FaninId="<<n<<std::endl;
     // Abc_ObjFaninC(pFanout, 1) ? pFanout->fCompl1=0: pFanout->fCompl1=1; 
      Abc_ObjXorFaninC(pFanout, 1);
    }
       

      //Abc_ObjSetFaninC
  }
  //std::cout<<"before Abc_NtkMiter"<<std::endl;
  Abc_Ntk_t *pNtkMiter=Abc_NtkMiter(pNtk, pNtkWithnInver, 1, 0, 0, 0);
  //std::cout<<"Abc_NtkPoNum(pNtkMiter)="<<Abc_NtkPoNum(pNtkMiter)<<std::endl;

  Vec_Ptr_t *pEndPointsArray=Vec_PtrAlloc(2); //nCap???
  Vec_PtrPush(pEndPointsArray, Abc_ObjFanin(Abc_NtkObj(pNtk, n), 0));
  Vec_PtrPush(pEndPointsArray, Abc_ObjFanin(Abc_NtkObj(pNtk, n), 1));
  Abc_Ntk_t *pNtkConstructFanins=Abc_NtkCreateConeArray(pNtk, pEndPointsArray, 1); //fUseAllCis=1
  
  Abc_NtkAppend(pNtkMiter, pNtkConstructFanins, 1);

  //Abc_NtkShow( pNtkMiter, 0, 0, 1, 0, 1 );
  Aig_Man_t *pAigMiter=Abc_NtkToDar(pNtkMiter, 0, 0);
  sat_solver *pSat=sat_solver_new();
  //std::cout<<"before Cnf_Derive"<<std::endl;
  Cnf_Dat_t *pCnf=Cnf_Derive(pAigMiter, Aig_ManCoNum(pAigMiter));
  //std::cout<<"after Cnf_Derive"<<std::endl;
  Cnf_DataWriteIntoSolverInt(pSat, pCnf, 1, 0);
  lit clauseSAT[1];
  clauseSAT[0]=Abc_Var2Lit(pCnf->pVarNums[Aig_ObjId(Aig_ManCo(pAigMiter, 0))], 0);
  
  //std::fill(ODCCandidate.begin(), ODCCandidate.end(), 0);
  std::vector<bool> careSet(4, 0);
  if(sat_solver_addclause(pSat, clauseSAT, clauseSAT+1))
  {
    //std::cout<<"Abc_NtkPoNum(pNtkMiter)="<<Abc_NtkPoNum(pNtkMiter)<<std::endl;
    int Fanin0PoId=Abc_NtkPoNum(pNtkMiter)-2; 
    //std::cout<<"start sat_solver_solve"<<std::endl;
    //std::cout<<"sat="<<sat_solver_solve( pSat, NULL, NULL, 0, 0, 0, 0)<<std::endl;
    for(int i=0;sat_solver_solve( pSat, NULL, NULL, 0, 0, 0, 0)>=0 && i<4;++i)
    {
      //std::cout<<"i="<<i<<std::endl;
      bool ODC0=sat_solver_var_value(pSat, pCnf->pVarNums[Aig_ObjId(Aig_ManCo(pAigMiter, 1))]);
      bool ODC1=sat_solver_var_value(pSat, pCnf->pVarNums[Aig_ObjId(Aig_ManCo(pAigMiter, 2))]);
      //std::cout<<"ODC0+2*ODC1="<<ODC0+2*ODC1<<std::endl;
      careSet[ODC0+2*ODC1]=1;
      lit clauseConstrains[2];
      clauseConstrains[0]=Abc_Var2Lit(pCnf->pVarNums[Aig_ObjId(Aig_ManCo(pAigMiter, Fanin0PoId))], ODC0);
      clauseConstrains[1]=Abc_Var2Lit(pCnf->pVarNums[Aig_ObjId(Aig_ManCo(pAigMiter, Fanin0PoId+1))], ODC1);
      //std::cout<<"ODC0="<<ODC0<<" ODC1="<<ODC1<<std::endl;
      if(!sat_solver_addclause(pSat, clauseConstrains, clauseConstrains+2))
      {
        //std::cout<<"out"<<std::endl;
        //std::cout<<"ODC0="<<ODC0<<" ODC1="<<ODC1<<std::endl;
        break;
      }
        
    }
  }
  
  for(int i=0;i<4;++i)
    ODCCandidate[i]=!careSet[i];

  
  return;
}

void Lsv_NtkPrintODC(Abc_Ntk_t* pNtk, int n)
{
  std::vector<bool> ODC(4), SDC(4, 1);
  
  checkODC(pNtk, n, ODC);
  Lsv_NtkGetSDC(pNtk, SDC, n);
  for(int i=0;i<4;++i)
  {
    if(SDC[i]==1)
      ODC[i]=0;
  }

  bool noODC=1;
  for(int i=0;i<4;++i)
  {
    //std::cout<<"ODC[i]="<<ODC[i]<<std::endl;
    if(ODC[i])
    {
      noODC=0;
      std::cout<<(Abc_ObjFaninC(Abc_NtkObj(pNtk, n), 0) ? !getBit(i, 0):getBit(i, 0));
      std::cout<<(Abc_ObjFaninC(Abc_NtkObj(pNtk, n), 1) ? !getBit(i, 1):getBit(i, 1));
      std::cout<<" "; 
    }
  }
  if(noODC)
    std::cout<<"no odc";
  std::cout<<"\n";
}

int Lsv_CommandPrintODC( Abc_Frame_t * pAbc, int argc, char ** argv )
{
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  size_t sizeArgv=strlen(argv[1]);
  // int c, k;
  // std::cout<<argv[1]<<std::endl;
  // int n=argv[1][0]-'0';
  // if(argv[1][0])
  int n=0;
  for(int i=0;i<sizeArgv;++i)
    n=10*n+argv[1][i]-'0';
  // std::cout<<"argv= "<<strlen(argv[1])<<std::endl;
  // std::cout<<"n= "<<n<<std::endl;
  if(!pNtk) 
  {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }


  //if(!Abc_ObjIsNode(Abc_NtkObj(pNtk, n)))
  //  std::cout<<"fuck you cheater fuck n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"<<std::endl;
  
  //for(int n=4;n<8;++n)
  //  Lsv_NtkPrintODC(pNtk, n);

  //std::cout<<"n="<<n<<std::endl;
  Lsv_NtkPrintODC(pNtk, n);
  return 0;
}
  