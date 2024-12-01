#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <vector>
#include "sat/cnf/cnf.h"

using namespace std;

extern "C"{
Aig_Man_t* Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
Aig_Obj_t *  Aig_Not( Aig_Obj_t * p );
}

static int Lsv_CommandPrintCut(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSdc(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandOdc(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandOdcLoop(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_CommandPrintCut, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sdc", Lsv_CommandSdc, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_odc", Lsv_CommandOdc, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_odcloop", Lsv_CommandOdcLoop, 0);
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

//int Lsv_FindCut

int Lsv_CommandPrintCut(Abc_Frame_t* pAbc, int argc, char** argv) {

  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);


  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  if(argc != 2){
    printf("Wrong input command!\n");
    return 1;
  }

  if(argv[1][1] != '\0'){
    printf("Wrong input command!\n");
    return 1;
  }

  if((int)argv[1][0]-'0' > 6 || (int)argv[1][0]-'0' < 3){
    printf("Wrong input command!\n");
    return 1;
  }
  int CutNumMax = (int)argv[1][0]-'0';
  vector <int> CutNum(Abc_NtkObjNum(pNtk) + 1, -1);
  Abc_Obj_t* pObj, pObj2;
  vector<vector<vector<int>>>  cut(Abc_NtkObjNum(pNtk) + 1);
  vector<int> CutForm(Abc_NtkObjNum(pNtk) + 1);
  int i, OutputStartCheck = 0, OutputEndCheck = 0,pOutputStart,pOutputEnd;

  Abc_NtkForEachObj(pNtk, pObj, i) {
    cut[i].clear();

    int j, FaninCheck = 0;

    Abc_Obj_t* pFanin;
    if(Abc_ObjFaninNum(pObj) == 0 && Abc_ObjFanoutNum(pObj) > 0){
      CutNum[i] = 0;

      cut[i].push_back({int(Abc_ObjId(pObj))});
    }

    else if(i > 0 && Abc_ObjFanoutNum(pObj) == 0 && OutputStartCheck == 0){
      pOutputStart = Abc_ObjId(pObj);
      OutputStartCheck = 1;
    }

    else if(Abc_ObjFanoutNum(pObj) > 0 && OutputEndCheck == 0){


      OutputEndCheck = 1;
      pOutputEnd = Abc_ObjId(pObj);
      i--;
    }
    else if(Abc_ObjFanoutNum(pObj) > 0 && OutputEndCheck == 1){
      vector<int> tempcut;
      tempcut.clear();
      tempcut.push_back(Abc_ObjId(pObj));
      cut[Abc_ObjId(pObj)].push_back(tempcut);

      vector<int> FaninCutNum;
      vector<int> FaninCutMax;
      FaninCutNum.resize(Abc_ObjFaninNum(pObj));
      FaninCutMax.resize(Abc_ObjFaninNum(pObj));
      Abc_ObjForEachFanin(pObj, pFanin, j){
        FaninCutMax[j] = CutNum[Abc_ObjId(pFanin)] - 1;
      }

      int CutCount = 0, k, CutNodeNum;
      while(1){

        for(j = 0; j <= Abc_NtkObjNum(pNtk); j++){
          CutForm[j] = 0;
        }
        Abc_ObjForEachFanin(pObj, pFanin, j){
          for(k = 0; k < cut[Abc_ObjId(pFanin)][FaninCutNum[j]].size(); k++){
            CutForm[cut[Abc_ObjId(pFanin)][FaninCutNum[j]][k]] = 1;
          }
        }
        CutNodeNum = 0;
        tempcut.clear();
        for(j = 0; j <= Abc_NtkObjNum(pNtk); j++){
          if(CutForm[j] == 1){
            CutNodeNum++;
            tempcut.push_back(j);
            if(CutNodeNum > CutNumMax){
              break;
            }
          }
        }
        if(CutNodeNum <= CutNumMax){
          int SameCutCheck = 0, k = 0, m = 0, CutCompareLarge = 1, CutCompareSmall = 1;



          for(j = 0; j < cut[Abc_ObjId(pObj)].size(); j++){
            k = 0;
            m = 0;
            CutCompareLarge = 1;
            CutCompareSmall = 1;
            if(j > 0){

              m = 0;
              for(k = 0; k < tempcut.size(); k++){
                while(m < cut[Abc_ObjId(pObj)][j].size() && cut[Abc_ObjId(pObj)][j][m] < tempcut[k]){
                  m++;
                }

                if(cut[Abc_ObjId(pObj)][j][m] > tempcut[k]){
                  CutCompareSmall = 0;
                  break;
                }

                else if(cut[Abc_ObjId(pObj)][j][m] == tempcut[k]){
                  m++;
                  CutCompareSmall++;
                }


              }
              k = 0;
              for(m = 0; m < cut[Abc_ObjId(pObj)][j].size(); m++){
                while(k < tempcut.size() && cut[Abc_ObjId(pObj)][j][m] > tempcut[k]){
                  k++;
                }

                if(cut[Abc_ObjId(pObj)][j][m] < tempcut[k]){
                  CutCompareLarge = 0;
                  break;
                }
                else if(cut[Abc_ObjId(pObj)][j][m] == tempcut[k]){
                  k++;
                  CutCompareLarge++;
                }


              }
              if(CutCompareSmall == tempcut.size()){
                SameCutCheck = 1;
                break;
              }
              else if(CutCompareLarge == cut[Abc_ObjId(pObj)][j].size() && !CutCompareSmall == tempcut.size()){
                cut[Abc_ObjId(pObj)].erase(cut[Abc_ObjId(pObj)].begin() + j);
                CutCount--;
              }
            }
          }
          if(SameCutCheck == 0){

            CutCount++;
            cut[Abc_ObjId(pObj)].push_back(tempcut);
          }

        }
        int FindCutFinish = 0;
        Abc_ObjForEachFanin(pObj, pFanin, j){
          if(FaninCutNum[j] < FaninCutMax[j]){
            FaninCutNum[j]++;
            FindCutFinish = 1;
            break;
          }
          else{
            FaninCutNum[j] = 0;
          }
        }
        if(FindCutFinish == 0){
          break;
        }
      }
      CutNum[pObj->Id] = CutCount + 1;
    }

    for(j = 0; j < cut[Abc_ObjId(pObj)].size(); j++){
      printf("%d:", Abc_ObjId(pObj));
      for(int k = 0; k < cut[Abc_ObjId(pObj)][j].size(); k++){
        printf(" %d", cut[Abc_ObjId(pObj)][j][k]);
      }
      printf("\n");
    }

  }
  for(i = pOutputStart; i < pOutputEnd; i++){
    cut[i].clear();
    int j, FaninCheck = 0;

    Abc_Obj_t* pFanin;
    pObj = Abc_NtkObj(pNtk, i);

    vector<int> tempcut;
    tempcut.clear();
    tempcut.push_back(Abc_ObjId(pObj));
    cut[Abc_ObjId(pObj)].push_back(tempcut);

    vector<int> FaninCutNum;
    vector<int> FaninCutMax;
    FaninCutNum.resize(Abc_ObjFaninNum(pObj));
    FaninCutMax.resize(Abc_ObjFaninNum(pObj));
    Abc_ObjForEachFanin(pObj, pFanin, j){
      FaninCutMax[j] = CutNum[Abc_ObjId(pFanin)] - 1;
    }

    int CutCount = 0, k, CutNodeNum;
    while(1){

      for(j = 0; j <= Abc_NtkObjNum(pNtk); j++){
        CutForm[j] = 0;
      }
      Abc_ObjForEachFanin(pObj, pFanin, j){
        //printf("%d FaninCutNum:%d, Index: %d\n", Abc_ObjId(pFanin), FaninCutNum[j], j);
        for(k = 0; k < cut[Abc_ObjId(pFanin)][FaninCutNum[j]].size(); k++){
          CutForm[cut[Abc_ObjId(pFanin)][FaninCutNum[j]][k]] = 1;
        }
      }
      CutNodeNum = 0;
      tempcut.clear();
      for(j = 0; j <= Abc_NtkObjNum(pNtk); j++){
        //printf("j:%d %d\n", j, CutForm[j]);
        if(CutForm[j] == 1){
          CutNodeNum++;
          tempcut.push_back(j);
          if(CutNodeNum > CutNumMax){
            break;
          }
        }
      }
      if(CutNodeNum <= CutNumMax){
        int SameCutCheck = 0, k = 0, m = 0, CutCompareLarge = 1, CutCompareSmall = 1;



        for(j = 0; j < cut[Abc_ObjId(pObj)].size(); j++){
          k = 0;
          m = 0;
          CutCompareLarge = 1;
          CutCompareSmall = 1;
          if(j > 0){
            m = 0;
            for(k = 0; k < tempcut.size(); k++){
              while(m < cut[Abc_ObjId(pObj)][j].size() && cut[Abc_ObjId(pObj)][j][m] < tempcut[k]){
                m++;
              }

              if(cut[Abc_ObjId(pObj)][j][m] > tempcut[k]){
                CutCompareSmall = 0;
                break;
              }
              else if(cut[Abc_ObjId(pObj)][j][m] == tempcut[k]){
                m++;
                CutCompareSmall++;
              }


            }
            k = 0;
            for(m = 0; m < cut[Abc_ObjId(pObj)][j].size(); m++){
              while(k < tempcut.size() && cut[Abc_ObjId(pObj)][j][m] > tempcut[k]){
                k++;
              }

              if(cut[Abc_ObjId(pObj)][j][m] < tempcut[k]){
                CutCompareLarge = 0;
                break;
              }
              else if(cut[Abc_ObjId(pObj)][j][m] == tempcut[k]){
                k++;
                CutCompareLarge++;
              }


            }
            if(CutCompareSmall == tempcut.size()){
              SameCutCheck = 1;
              break;
            }
            else if(CutCompareLarge == cut[Abc_ObjId(pObj)][j].size() && !CutCompareSmall == tempcut.size()){
              cut[Abc_ObjId(pObj)].erase(cut[Abc_ObjId(pObj)].begin() + j);
              CutCount--;
            }
          }
        }
        if(SameCutCheck == 0){

          CutCount++;
          cut[Abc_ObjId(pObj)].push_back(tempcut);
        }

      }
      int FindCutFinish = 0;
      Abc_ObjForEachFanin(pObj, pFanin, j){
        if(FaninCutNum[j] < FaninCutMax[j]){
          FaninCutNum[j]++;
          FindCutFinish = 1;
          break;
        }
        else{
          FaninCutNum[j] = 0;
        }
      }
      if(FindCutFinish == 0){
        break;
      }
    }
    CutNum[pObj->Id] = CutCount + 1;
    for(j = 0; j < cut[Abc_ObjId(pObj)].size(); j++){
      printf("%d:", Abc_ObjId(pObj));
      for(int k = 0; k < cut[Abc_ObjId(pObj)][j].size(); k++){
        printf(" %d", cut[Abc_ObjId(pObj)][j][k]);
      }
      printf("\n");
    }
  }
  return 0;


}

int Lsv_CommandSdc(Abc_Frame_t* pAbc, int argc, char** argv) {

  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int i;

  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  if(argc != 2){
    printf("Wrong input command1!\n");
    return 1;
  }
  int node_num = 0;
  for(i =  0; i < strlen(argv[1]) ; i++){
    //printf("argv[1][i]: %c\n", argv[1][i], int(argv[1][i]));
    if(argv[1][i] < '0' || argv[1][i] > '9'){
      printf("Wrong input command2!\n");
      return 1;
    }
    node_num = node_num * 10 + (int)argv[1][i] - '0';
  }


  if(node_num < 0 || node_num >= Abc_NtkObjNum(pNtk)){
    printf("Node num out of range!\n");
    return 1;
  }

  Abc_Obj_t* pObj;
  Abc_Obj_t* pChosenNode = Abc_NtkObj(pNtk, node_num);
  Abc_Obj_t* pFanin;
  if(Abc_ObjFaninNum(pChosenNode) == 0 && Abc_ObjFanoutNum(pChosenNode) == 0){
    printf("The node is a Const.\n");
    return 1;
  }
  else if(Abc_ObjFaninNum(pChosenNode) == 0){
    printf("The node is a PI.\n");
    return 1;
  }
  else if(Abc_ObjFanoutNum(pChosenNode) == 0){
    printf("The node is a PO.\n");
    return 1;
  }

  Abc_Ntk_t *pCone;
  Vec_Ptr_t *vRoot;
  Abc_Obj_t *pleftfanin, *prightfanin;
  pleftfanin = Abc_ObjFanin0(pChosenNode);
  prightfanin = Abc_ObjFanin1(pChosenNode);
  vRoot = Vec_PtrAllocExact(2);
  Vec_PtrPush(vRoot, pleftfanin);
  Vec_PtrPush(vRoot, prightfanin);
  pCone = Abc_NtkCreateConeArray(pNtk, vRoot, 1);

  int pResult;
  int *pResultPtr;
  pResultPtr = Abc_NtkVerifySimulatePattern(pCone, Abc_NtkVerifyGetCleanModel(pNtk, 1));

  int y0, y1;
  Aig_Man_t *myAig;
  Cnf_Dat_t *myCnf;
  sat_solver *my_sat_solver = sat_solver_new();
  lit* assumption = new lit[2];
  myAig = Abc_NtkToDar(pCone, 0, 0);
  myCnf = Cnf_Derive(myAig, 2);
  Cnf_DataWriteIntoSolverInt(my_sat_solver, myCnf, 1, 1);

  lit leftfanin, rightfanin;
  leftfanin = myCnf->pVarNums[Abc_NtkPo(pCone, 0)->Id];
  rightfanin = myCnf->pVarNums[Abc_NtkPo(pCone, 1)->Id];
  int sdc_count = 0;
  int dc_type[4];
  dc_type[0] = 0;
  dc_type[1] = 0;
  dc_type[2] = 0;
  dc_type[3] = 0;
  for(y0 = 0; y0 < 2; y0++){
    for(y1 = 0; y1 < 2; y1++){
      if(pResultPtr[0] != y0 || pResultPtr[1] != y1){

        assumption[0] = leftfanin * 2 + (1 - y0);
        assumption[1] = rightfanin * 2 + (1 - y1);

        if(sat_solver_solve(my_sat_solver, assumption, assumption + 2, 0, 0, 0, 0)!= 1){
          dc_type[2 * (y0 ^ pChosenNode->fCompl0) + (y1 ^ pChosenNode->fCompl1)] = 1;
          sdc_count++;
        }

      }
    }
  }
  if(sdc_count == 0){
    printf("no sdc\n");
  }
  else{
    for(i = 0; i < 4; i++){
      if(dc_type[i] == 1){
        printf("%d%d ", i / 2, i % 2);
      }
    }
    printf("\n");
  }

  return 0;
}

Aig_Man_t * Abc_NtkToDarComp( Abc_Ntk_t * pNtk, int pCompObj, int fExors, int fRegisters )
{
    Vec_Ptr_t * vNodes;
    Aig_Man_t * pMan;
    Aig_Obj_t * pObjNew;
    Abc_Obj_t * pObj;
    int i, nNodes, nDontCares;
    // make sure the latches follow PIs/POs
    if ( fRegisters )
    {
        assert( Abc_NtkBoxNum(pNtk) == Abc_NtkLatchNum(pNtk) );
        Abc_NtkForEachCi( pNtk, pObj, i )
            if ( i < Abc_NtkPiNum(pNtk) )
            {
                assert( Abc_ObjIsPi(pObj) );
                if ( !Abc_ObjIsPi(pObj) )
                    Abc_Print( 1, "Abc_NtkToDar(): Temporary bug: The PI ordering is wrong!\n" );
            }
            else
                assert( Abc_ObjIsBo(pObj) );
        Abc_NtkForEachCo( pNtk, pObj, i )
            if ( i < Abc_NtkPoNum(pNtk) )
            {
                assert( Abc_ObjIsPo(pObj) );
                if ( !Abc_ObjIsPo(pObj) )
                    Abc_Print( 1, "Abc_NtkToDar(): Temporary bug: The PO ordering is wrong!\n" );
            }
            else
                assert( Abc_ObjIsBi(pObj) );
        // print warning about initial values
        nDontCares = 0;
        Abc_NtkForEachLatch( pNtk, pObj, i )
            if ( Abc_LatchIsInitDc(pObj) )
            {
                Abc_LatchSetInit0(pObj);
                nDontCares++;
            }
        if ( nDontCares )
        {
            Abc_Print( 1, "Warning: %d registers in this network have don't-care init values.\n", nDontCares );
            Abc_Print( 1, "The don't-care are assumed to be 0. The result may not verify.\n" );
            Abc_Print( 1, "Use command \"print_latch\" to see the init values of registers.\n" );
            Abc_Print( 1, "Use command \"zero\" to convert or \"init\" to change the values.\n" );
        }
    }
    // create the manager
    pMan = Aig_ManStart( Abc_NtkNodeNum(pNtk) + 100 );
    pMan->fCatchExor = fExors;
    pMan->nConstrs = pNtk->nConstrs;
    pMan->nBarBufs = pNtk->nBarBufs;
    pMan->pName = Extra_UtilStrsav( pNtk->pName );
    pMan->pSpec = Extra_UtilStrsav( pNtk->pSpec );
    // transfer the pointers to the basic nodes
    Abc_AigConst1(pNtk)->pCopy = (Abc_Obj_t *)Aig_ManConst1(pMan);
    Abc_NtkForEachCi( pNtk, pObj, i )
    {
        pObj->pCopy = (Abc_Obj_t *)Aig_ObjCreateCi(pMan);
        // initialize logic level of the CIs
        ((Aig_Obj_t *)pObj->pCopy)->Level = pObj->Level;
    }

    // complement the 1-values registers
    if ( fRegisters ) {
        Abc_NtkForEachLatch( pNtk, pObj, i )
            if ( Abc_LatchIsInit1(pObj) )
                Abc_ObjFanout0(pObj)->pCopy = Abc_ObjNot(Abc_ObjFanout0(pObj)->pCopy);
    }
    // perform the conversion of the internal nodes (assumes DFS ordering)
//    pMan->fAddStrash = 1;
    vNodes = Abc_NtkDfs( pNtk, 0 );
    Vec_PtrForEachEntry( Abc_Obj_t *, vNodes, pObj, i )
//    Abc_NtkForEachNode( pNtk, pObj, i )
    {
      if(pObj->vFanins.pArray[0] == pCompObj && pObj->vFanins.pArray[1] != pCompObj){
        pObj->pCopy = (Abc_Obj_t *)Aig_And( pMan, Aig_Not((Aig_Obj_t *)Abc_ObjChild0Copy(pObj)), (Aig_Obj_t *)Abc_ObjChild1Copy(pObj) );
      }
      else if (pObj->vFanins.pArray[0] == pCompObj && pObj->vFanins.pArray[1] == pCompObj)
      {
        pObj->pCopy = (Abc_Obj_t *)Aig_And( pMan, Aig_Not((Aig_Obj_t *)Abc_ObjChild0Copy(pObj)), Aig_Not((Aig_Obj_t *)Abc_ObjChild1Copy(pObj)) );
      }
      else if(pObj->vFanins.pArray[0] != pCompObj && pObj->vFanins.pArray[1] == pCompObj){
        pObj->pCopy = (Abc_Obj_t *)Aig_And( pMan, (Aig_Obj_t *)Abc_ObjChild0Copy(pObj), Aig_Not((Aig_Obj_t *)Abc_ObjChild1Copy(pObj)) );
      }
      else{
        pObj->pCopy = (Abc_Obj_t *)Aig_And( pMan, (Aig_Obj_t *)Abc_ObjChild0Copy(pObj), (Aig_Obj_t *)Abc_ObjChild1Copy(pObj) );
      }
//        Abc_Print( 1, "%d->%d ", pObj->Id, ((Aig_Obj_t *)pObj->pCopy)->Id );
    }
    Vec_PtrFree( vNodes );
    pMan->fAddStrash = 0;
    // create the POs
    Abc_NtkForEachCo( pNtk, pObj, i )
        if(pObj->vFanins.pArray[0] == pCompObj){
          Aig_ObjCreateCo( pMan, Aig_Not((Aig_Obj_t *)Abc_ObjChild0Copy(pObj)) );
        }
        else{
          Aig_ObjCreateCo( pMan, (Aig_Obj_t *)Abc_ObjChild0Copy(pObj) );
        }
    // complement the 1-valued registers
    Aig_ManSetRegNum( pMan, Abc_NtkLatchNum(pNtk) );
    if ( fRegisters )
        Aig_ManForEachLiSeq( pMan, pObjNew, i )
            if ( Abc_LatchIsInit1(Abc_ObjFanout0(Abc_NtkCo(pNtk,i))) )
                pObjNew->pFanin0 = Aig_Not(pObjNew->pFanin0);
    // remove dangling nodes
    nNodes = (Abc_NtkGetChoiceNum(pNtk) == 0)? Aig_ManCleanup( pMan ) : 0;
    if ( !fExors && nNodes )
        Abc_Print( 1, "Abc_NtkToDar(): Unexpected %d dangling nodes when converting to AIG!\n", nNodes );
//Aig_ManDumpVerilog( pMan, "test.v" );
    // save the number of registers
    if ( fRegisters )
    {
        Aig_ManSetRegNum( pMan, Abc_NtkLatchNum(pNtk) );
        pMan->vFlopNums = Vec_IntStartNatural( pMan->nRegs );
//        pMan->vFlopNums = NULL;
//        pMan->vOnehots = Abc_NtkConverLatchNamesIntoNumbers( pNtk );
        if ( pNtk->vOnehots )
            pMan->vOnehots = (Vec_Ptr_t *)Vec_VecDupInt( (Vec_Vec_t *)pNtk->vOnehots );
    }
    if ( !Aig_ManCheck( pMan ) )
    {
        Abc_Print( 1, "Abc_NtkToDar: AIG check has failed.\n" );
        Aig_ManStop( pMan );
        return NULL;
    }
    return pMan;
}

int Lsv_CommandOdc(Abc_Frame_t* pAbc, int argc, char** argv) {

  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int i;

  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  if(argc != 2){
    printf("Wrong input command1!\n");
    return 1;
  }
  int node_num = 0;
  for(i =  0; i < strlen(argv[1]) ; i++){
    //printf("argv[1][i]: %c\n", argv[1][i], int(argv[1][i]));
    if(argv[1][i] < '0' || argv[1][i] > '9'){
      printf("Wrong input command2!\n");
      return 1;
    }
    node_num = node_num * 10 + (int)argv[1][i] - '0';
  }


  if(node_num < 0 || node_num >= Abc_NtkObjNum(pNtk)){
    printf("Node num out of range!\n");
    return 1;
  }

  Abc_Obj_t* pObj;
  Abc_Obj_t* pChosenNode = Abc_NtkObj(pNtk, node_num);
  Abc_Obj_t* pFanin;
  if(Abc_ObjFaninNum(pChosenNode) == 0 && Abc_ObjFanoutNum(pChosenNode) == 0){
    printf("The node is a Const.\n");
    return 1;
  }
  else if(Abc_ObjFaninNum(pChosenNode) == 0){
    printf("The node is a PI.\n");
    return 1;
  }
  else if(Abc_ObjFanoutNum(pChosenNode) == 0){
    printf("The node is a PO.\n");
    return 1;
  }

  Abc_Ntk_t *pCone;
  Vec_Ptr_t *vRoot;
  Abc_Obj_t *pleftfanin, *prightfanin;
  pleftfanin = Abc_ObjFanin0(pChosenNode);
  prightfanin = Abc_ObjFanin1(pChosenNode);
  vRoot = Vec_PtrAllocExact(2);
  Vec_PtrPush(vRoot, pleftfanin);
  Vec_PtrPush(vRoot, prightfanin);
  pCone = Abc_NtkCreateConeArray(pNtk, vRoot, 1);

  int pResult;
  int *pResultPtr;
  pResultPtr = Abc_NtkVerifySimulatePattern(pCone, Abc_NtkVerifyGetCleanModel(pNtk, 1));

  int y0, y1;
  Aig_Man_t *myAig;
  Cnf_Dat_t *myCnf;
  sat_solver *my_sat_solver = sat_solver_new();
  lit* assumption = new lit[2];
  myAig = Abc_NtkToDar(pCone, 0, 0);
  myCnf = Cnf_Derive(myAig, 2);
  Cnf_DataWriteIntoSolverInt(my_sat_solver, myCnf, 1, 1);

  lit leftfanin, rightfanin;
  leftfanin = myCnf->pVarNums[Abc_NtkPo(pCone, 0)->Id];
  rightfanin = myCnf->pVarNums[Abc_NtkPo(pCone, 1)->Id];
  int dc_type[4] = {0};
  dc_type[0] = 0;
  dc_type[1] = 0;
  dc_type[2] = 0;
  dc_type[3] = 0;
  for(y0 = 0; y0 < 2; y0++){
    for(y1 = 0; y1 < 2; y1++){
      if(pResultPtr[0] != y0 || pResultPtr[1] != y1){

        assumption[0] = leftfanin * 2 + (1 - y0);
        assumption[1] = rightfanin * 2 + (1 - y1);

        if(sat_solver_solve(my_sat_solver, assumption, assumption + 2, 0, 0, 0, 0)!= 1){
          dc_type[2 * (y0 ^ pChosenNode->fCompl0) + (y1 ^ pChosenNode->fCompl1)] = 1;
        }

      }
    }
  }
  Aig_Man_t *aig, *aig_comp;
  Cnf_Dat_t *cnf, *cnf_comp;
  sat_solver *my_sat_solver2 = sat_solver_new();
  aig = Abc_NtkToDar(pNtk, 0, 0);
  aig_comp = Abc_NtkToDarComp(pNtk, Abc_ObjId(pChosenNode), 0, 0);
  cnf = Cnf_Derive(aig, Abc_NtkPoNum(pNtk));
  cnf_comp = Cnf_Derive(aig_comp, Abc_NtkPoNum(pNtk));
  Cnf_DataLift( cnf_comp, cnf->nVars );
  Cnf_DataWriteIntoSolverInt( my_sat_solver2, cnf, 1, 1 );
  Cnf_DataWriteIntoSolverInt( my_sat_solver2, cnf_comp, 1, 1 );
  //printf("begin connection\n");
  lit *inputconnect = new lit[2];
  for(i = 0; i < Abc_NtkPiNum(pNtk); i++){
    inputconnect[0] = (cnf->pVarNums[Aig_ManCi(aig, i)->Id]) * 2;
    inputconnect[1] = (cnf_comp->pVarNums[Aig_ManCi(aig_comp, i)->Id]) * 2 + 1;
    sat_solver_addclause(my_sat_solver2, inputconnect, inputconnect + 2);
    inputconnect[0] = (cnf->pVarNums[Aig_ManCi(aig, i)->Id]) * 2 + 1;
    inputconnect[1] = (cnf_comp->pVarNums[Aig_ManCi(aig_comp, i)->Id]) * 2;
    sat_solver_addclause(my_sat_solver2, inputconnect, inputconnect + 2);
  }
  int outputvariablebegin = sat_solver_nvars(my_sat_solver2);
  sat_solver_setnvars(my_sat_solver2, my_sat_solver2->size + Abc_NtkPoNum(pNtk) + 1);
  lit *outputxor = new lit[3];
  lit *outputor1 = new lit[Abc_NtkPoNum(pNtk) + 1];
  lit *outputor2 = new lit[2];
  for(i = 0; i < Abc_NtkPoNum(pNtk); i++){
    outputxor[0] = (cnf->pVarNums[Aig_ManCo(aig, i)->Id]) * 2 + 1;
    outputxor[1] = (cnf_comp->pVarNums[Aig_ManCo(aig_comp, i)->Id]) * 2 + 1;
    outputxor[2] = (outputvariablebegin + i) * 2 + 1;
    sat_solver_addclause(my_sat_solver2, outputxor, outputxor + 3);
    outputxor[0] = (cnf->pVarNums[Aig_ManCo(aig, i)->Id]) * 2;
    outputxor[1] = (cnf_comp->pVarNums[Aig_ManCo(aig_comp, i)->Id]) * 2;
    outputxor[2] = (outputvariablebegin + i) * 2 + 1;
    sat_solver_addclause(my_sat_solver2, outputxor, outputxor + 3);
    outputxor[0] = (cnf->pVarNums[Aig_ManCo(aig, i)->Id]) * 2;
    outputxor[1] = (cnf_comp->pVarNums[Aig_ManCo(aig_comp, i)->Id]) * 2 + 1;
    outputxor[2] = (outputvariablebegin + i) * 2;
    sat_solver_addclause(my_sat_solver2, outputxor, outputxor + 3);
    outputxor[0] = (cnf->pVarNums[Aig_ManCo(aig, i)->Id]) * 2 + 1;
    outputxor[1] = (cnf_comp->pVarNums[Aig_ManCo(aig_comp, i)->Id]) * 2;
    outputxor[2] = (outputvariablebegin + i) * 2;
    sat_solver_addclause(my_sat_solver2, outputxor, outputxor + 3);
  }
  for(i = 0; i < Abc_NtkPoNum(pNtk); i++){
    outputor1[i] = (outputvariablebegin + i) * 2;
    }
  outputor1[Abc_NtkPoNum(pNtk)] = (outputvariablebegin + Abc_NtkPoNum(pNtk)) * 2 + 1;
  sat_solver_addclause(my_sat_solver2, outputor1, outputor1 + Abc_NtkPoNum(pNtk) + 1);
  outputor2[0] = (outputvariablebegin + Abc_NtkPoNum(pNtk)) * 2;
  for(i = 0; i < Abc_NtkPoNum(pNtk); i++){
    outputor2[1] = (outputvariablebegin + i) * 2 + 1;
    sat_solver_addclause(my_sat_solver2, outputor2, outputor2 + 2);
  }
  lit *outcome;
  outcome = new lit[1];
  outcome[0] = (outputvariablebegin + Abc_NtkPoNum(pNtk)) * 2;
  lit *avoidrepeatsat = new lit[2];
  int *simulation = new int[Abc_NtkPiNum(pNtk)];
  Cnf_DataLift(myCnf, cnf->nVars + cnf_comp->nVars + Abc_NtkPoNum(pNtk) + 1);
  Cnf_DataWriteIntoSolverInt(my_sat_solver2, myCnf, 1, 1);
  for(i = 0; i < Abc_NtkPiNum(pNtk); i++){
    inputconnect[0] = (cnf->pVarNums[Aig_ManCi(aig, i)->Id]) * 2;
    inputconnect[1] = (myCnf->pVarNums[Aig_ManCi(myAig, i)->Id]) * 2 + 1;
    sat_solver_addclause(my_sat_solver2, inputconnect, inputconnect + 2);
    inputconnect[0] = (cnf->pVarNums[Aig_ManCi(aig, i)->Id]) * 2 + 1;
    inputconnect[1] = (myCnf->pVarNums[Aig_ManCi(myAig, i)->Id]) * 2;
    sat_solver_addclause(my_sat_solver2, inputconnect, inputconnect + 2);
  }
  /*printf("begin iteration\n");
  Cnf_DataPrint(cnf, 1);
  Cnf_DataPrint(cnf_comp, 1);
  Cnf_DataPrint(myCnf, 1);*/
  while(true){
    if(sat_solver_solve(my_sat_solver2, outcome, outcome + 1, 0, 0, 0, 0) == 1){
      y0 = sat_solver_var_value(my_sat_solver2, myCnf->pVarNums[Abc_NtkPo(pCone, 0)->Id]) ^ pChosenNode->fCompl0;
      y1 = sat_solver_var_value(my_sat_solver2, myCnf->pVarNums[Abc_NtkPo(pCone, 1)->Id]) ^ pChosenNode->fCompl1;
      /*for(i = 0; i < sat_solver_nvars(my_sat_solver2); i++){
        printf("%d: %d\n", i, sat_solver_var_value(my_sat_solver2, i));
      }*/
      if(dc_type[2 * y0 + y1] == 1){
        printf("wrong, solve an sdc %d%d\n", y0, y1);
      }
      else if(dc_type[2 * y0 + y1] == 0){
        dc_type[2 * y0 + y1] = 2;
      }
      avoidrepeatsat[0] = (myCnf->pVarNums[Abc_NtkPo(pCone, 0)->Id]) * 2 + (y0 ^ pChosenNode->fCompl0);
      avoidrepeatsat[1] = (myCnf->pVarNums[Abc_NtkPo(pCone, 1)->Id]) * 2 + (y1 ^ pChosenNode->fCompl1);

      sat_solver_addclause(my_sat_solver2, avoidrepeatsat, avoidrepeatsat + 2);
    }
    else{
      break;
    }
  }
  int dc_count = 0;
  for(i = 0; i < 4; i++){
    if(dc_type[i] == 0){
      printf("%d%d ", i / 2, i % 2);
      dc_count++;
    }
  }
  if(dc_count == 0){
    printf("no odc\n");
  }
  else{
    printf("\n");
  }
  return 0;
}

int Lsv_CommandOdcLoop(Abc_Frame_t* pAbc, int argc, char** argv) {

  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int i;

  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  if(argc != 2){
    printf("Wrong input command1!\n");
    return 1;
  }
  int node_num = 0;
  for(i =  0; i < strlen(argv[1]) ; i++){
    //printf("argv[1][i]: %c\n", argv[1][i], int(argv[1][i]));
    if(argv[1][i] < '0' || argv[1][i] > '9'){
      printf("Wrong input command2!\n");
      return 1;
    }
    node_num = node_num * 10 + (int)argv[1][i] - '0';
  }


  if(node_num < 0 || node_num >= Abc_NtkObjNum(pNtk)){
    printf("Node num out of range!\n");
    return 1;
  }
  int iteration = 100;
  while(iteration--){
    Abc_Obj_t* pObj;
    Abc_Obj_t* pChosenNode = Abc_NtkObj(pNtk, node_num);
    Abc_Obj_t* pFanin;
    if(Abc_ObjFaninNum(pChosenNode) == 0 && Abc_ObjFanoutNum(pChosenNode) == 0){
      printf("The node is a Const.\n");
      return 1;
    }
    else if(Abc_ObjFaninNum(pChosenNode) == 0){
      printf("The node is a PI.\n");
      return 1;
    }
    else if(Abc_ObjFanoutNum(pChosenNode) == 0){
      printf("The node is a PO.\n");
      return 1;
    }

    Abc_Ntk_t *pCone;
    Vec_Ptr_t *vRoot;
    Abc_Obj_t *pleftfanin, *prightfanin;
    pleftfanin = Abc_ObjFanin0(pChosenNode);
    prightfanin = Abc_ObjFanin1(pChosenNode);
    vRoot = Vec_PtrAllocExact(2);
    Vec_PtrPush(vRoot, pleftfanin);
    Vec_PtrPush(vRoot, prightfanin);
    pCone = Abc_NtkCreateConeArray(pNtk, vRoot, 1);

    int pResult;
    int *pResultPtr;
    pResultPtr = Abc_NtkVerifySimulatePattern(pCone, Abc_NtkVerifyGetCleanModel(pNtk, 1));

    int y0, y1;
    Aig_Man_t *myAig;
    Cnf_Dat_t *myCnf;
    sat_solver *my_sat_solver = sat_solver_new();
    lit* assumption = new lit[2];
    myAig = Abc_NtkToDar(pCone, 0, 0);
    myCnf = Cnf_Derive(myAig, 2);
    Cnf_DataWriteIntoSolverInt(my_sat_solver, myCnf, 1, 1);

    lit leftfanin, rightfanin;
    leftfanin = myCnf->pVarNums[Abc_NtkPo(pCone, 0)->Id];
    rightfanin = myCnf->pVarNums[Abc_NtkPo(pCone, 1)->Id];
    int dc_type[4] = {0};
    for(y0 = 0; y0 < 2; y0++){
      for(y1 = 0; y1 < 2; y1++){
        if(true || pResultPtr[0] != y0 || pResultPtr[1] != y1){

          assumption[0] = leftfanin * 2 + (1 - y0);
          assumption[1] = rightfanin * 2 + (1 - y1);

          if(sat_solver_solve(my_sat_solver, assumption, assumption + 2, 0, 0, 0, 0)!= 1){
            dc_type[2 * (y0 ^ pChosenNode->fCompl0) + (y1 ^ pChosenNode->fCompl1)] = 1;
          }

        }
      }
    }
    Aig_Man_t *aig, *aig_comp;
    Cnf_Dat_t *cnf, *cnf_comp;
    sat_solver *my_sat_solver2 = sat_solver_new();
    aig = Abc_NtkToDar(pNtk, 0, 0);
    aig_comp = Abc_NtkToDarComp(pNtk, Abc_ObjId(pChosenNode), 0, 0);
    cnf = Cnf_Derive(aig, Abc_NtkPoNum(pNtk));
    cnf_comp = Cnf_Derive(aig_comp, Abc_NtkPoNum(pNtk));
    Cnf_DataLift( cnf_comp, cnf->nVars );
    Cnf_DataWriteIntoSolverInt( my_sat_solver2, cnf, 1, 1 );
    Cnf_DataWriteIntoSolverInt( my_sat_solver2, cnf_comp, 1, 1 );
    //printf("begin connection\n");
    lit *inputconnect = new lit[2];
    for(i = 0; i < Abc_NtkPiNum(pNtk); i++){
      inputconnect[0] = (cnf->pVarNums[Aig_ManCi(aig, i)->Id]) * 2;
      inputconnect[1] = (cnf_comp->pVarNums[Aig_ManCi(aig_comp, i)->Id]) * 2 + 1;
      sat_solver_addclause(my_sat_solver2, inputconnect, inputconnect + 2);
      inputconnect[0] = (cnf->pVarNums[Aig_ManCi(aig, i)->Id]) * 2 + 1;
      inputconnect[1] = (cnf_comp->pVarNums[Aig_ManCi(aig_comp, i)->Id]) * 2;
      sat_solver_addclause(my_sat_solver2, inputconnect, inputconnect + 2);
    }
    int outputvariablebegin = sat_solver_nvars(my_sat_solver2);
    sat_solver_setnvars(my_sat_solver2, my_sat_solver2->size + Abc_NtkPoNum(pNtk) + 1);
    lit *outputxor = new lit[3];
    lit *outputor1 = new lit[Abc_NtkPoNum(pNtk) + 1];
    lit *outputor2 = new lit[2];
    for(i = 0; i < Abc_NtkPoNum(pNtk); i++){
      outputxor[0] = (cnf->pVarNums[Aig_ManCo(aig, i)->Id]) * 2 + 1;
      outputxor[1] = (cnf_comp->pVarNums[Aig_ManCo(aig_comp, i)->Id]) * 2 + 1;
      outputxor[2] = (outputvariablebegin + i) * 2 + 1;
      sat_solver_addclause(my_sat_solver2, outputxor, outputxor + 3);
      outputxor[0] = (cnf->pVarNums[Aig_ManCo(aig, i)->Id]) * 2;
      outputxor[1] = (cnf_comp->pVarNums[Aig_ManCo(aig_comp, i)->Id]) * 2;
      outputxor[2] = (outputvariablebegin + i) * 2 + 1;
      sat_solver_addclause(my_sat_solver2, outputxor, outputxor + 3);
      outputxor[0] = (cnf->pVarNums[Aig_ManCo(aig, i)->Id]) * 2;
      outputxor[1] = (cnf_comp->pVarNums[Aig_ManCo(aig_comp, i)->Id]) * 2 + 1;
      outputxor[2] = (outputvariablebegin + i) * 2;
      sat_solver_addclause(my_sat_solver2, outputxor, outputxor + 3);
      outputxor[0] = (cnf->pVarNums[Aig_ManCo(aig, i)->Id]) * 2 + 1;
      outputxor[1] = (cnf_comp->pVarNums[Aig_ManCo(aig_comp, i)->Id]) * 2;
      outputxor[2] = (outputvariablebegin + i) * 2;
      sat_solver_addclause(my_sat_solver2, outputxor, outputxor + 3);
    }
    for(i = 0; i < Abc_NtkPoNum(pNtk); i++){
      outputor1[i] = (outputvariablebegin + i) * 2;
      }
    outputor1[Abc_NtkPoNum(pNtk)] = (outputvariablebegin + Abc_NtkPoNum(pNtk)) * 2 + 1;
    sat_solver_addclause(my_sat_solver2, outputor1, outputor1 + Abc_NtkPoNum(pNtk) + 1);
    outputor2[0] = (outputvariablebegin + Abc_NtkPoNum(pNtk)) * 2;
    for(i = 0; i < Abc_NtkPoNum(pNtk); i++){
      outputor2[1] = (outputvariablebegin + i) * 2 + 1;
      sat_solver_addclause(my_sat_solver2, outputor2, outputor2 + 2);
    }
    lit *outcome;
    outcome = new lit[1];
    outcome[0] = (outputvariablebegin + Abc_NtkPoNum(pNtk)) * 2;
    lit *avoidrepeatsat = new lit[2];
    int *simulation = new int[Abc_NtkPiNum(pNtk)];
    Cnf_DataLift(myCnf, cnf->nVars + cnf_comp->nVars + Abc_NtkPoNum(pNtk) + 1);
    Cnf_DataWriteIntoSolverInt(my_sat_solver2, myCnf, 1, 1);
    for(i = 0; i < Abc_NtkPiNum(pNtk); i++){
      inputconnect[0] = (cnf->pVarNums[Aig_ManCi(aig, i)->Id]) * 2;
      inputconnect[1] = (myCnf->pVarNums[Aig_ManCi(myAig, i)->Id]) * 2 + 1;
      sat_solver_addclause(my_sat_solver2, inputconnect, inputconnect + 2);
      inputconnect[0] = (cnf->pVarNums[Aig_ManCi(aig, i)->Id]) * 2 + 1;
      inputconnect[1] = (myCnf->pVarNums[Aig_ManCi(myAig, i)->Id]) * 2;
      sat_solver_addclause(my_sat_solver2, inputconnect, inputconnect + 2);
    }
    /*printf("begin iteration\n");
    Cnf_DataPrint(cnf, 1);
    Cnf_DataPrint(cnf_comp, 1);
    Cnf_DataPrint(myCnf, 1);*/
    while(true){
      if(sat_solver_solve(my_sat_solver2, outcome, outcome + 1, 0, 0, 0, 0) == 1){
        y0 = sat_solver_var_value(my_sat_solver2, myCnf->pVarNums[Abc_NtkPo(pCone, 0)->Id]) ^ pChosenNode->fCompl0;
        y1 = sat_solver_var_value(my_sat_solver2, myCnf->pVarNums[Abc_NtkPo(pCone, 1)->Id]) ^ pChosenNode->fCompl1;
        /*for(i = 0; i < sat_solver_nvars(my_sat_solver2); i++){
          printf("%d: %d\n", i, sat_solver_var_value(my_sat_solver2, i));
        }*/
        if(dc_type[2 * y0 + y1] == 1){
          printf("wrong, solve an sdc %d%d\n", y0, y1);
        }
        else if(dc_type[2 * y0 + y1] == 0){
          dc_type[2 * y0 + y1] = 2;
        }
        avoidrepeatsat[0] = (myCnf->pVarNums[Abc_NtkPo(pCone, 0)->Id]) * 2 + (y0 ^ pChosenNode->fCompl0);
        avoidrepeatsat[1] = (myCnf->pVarNums[Abc_NtkPo(pCone, 1)->Id]) * 2 + (y1 ^ pChosenNode->fCompl1);

        sat_solver_addclause(my_sat_solver2, avoidrepeatsat, avoidrepeatsat + 2);
      }
      else{
        break;
      }
    }
    printf("%d: ", node_num);
    int dc_count = 0;
    for(i = 0; i < 4; i++){
      if(dc_type[i] == 0){
        printf("%d%d\n", i / 2, i % 2);
        dc_count++;
      }
    }
    if(dc_count == 0){
      printf("no odc\n");
    }
    node_num++;
  }
  return 0;
}
