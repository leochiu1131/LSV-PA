#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimBdd(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv);



void init(Abc_Frame_t* pAbc) {
   Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
   Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", Lsv_CommandSimBdd, 0);
   Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig", Lsv_CommandSimAig, 0);
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

int Lsv_CommandSimBdd(Abc_Frame_t* pAbc, int argc, char** argv)
{
   if (argc != 2) {
      printf("invalid number of argument\n");
      return 0;
   }
   long long patternLen = strlen(argv[1]); 
   Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
   if (!Abc_NtkHasBdd(pNtk)) {
      printf("Please collapse first\n");
      return 0;
   }
   if (patternLen != Abc_NtkPiNum(pNtk)) {
      printf("invalid length of pattern\n");
      return 0;
   }

   int ithPo = 0;
   Abc_Obj_t* pPo = 0;

   Abc_NtkForEachPo(pNtk, pPo, ithPo) {
      Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo); // n9-n12
      assert( Abc_NtkIsBddLogic(pRoot->pNtk) );
      DdManager* dd = (DdManager *)pRoot->pNtk->pManFunc;  
      
      // Todo: do cofatoring
      Abc_Obj_t* pFanin = 0;
      DdNode* y = (DdNode*)pRoot->pData;
      int i = 0;
      int iFanin = 0;
      Abc_ObjForEachFanin(pRoot, pFanin, i) {
         if ((iFanin = Vec_IntFind(&pRoot->vFanins, pFanin->Id)) == -1) {
            printf( "Node %s should be among", Abc_ObjName(pFanin) );
            printf( " the fanins of node %s...\n", Abc_ObjName(pRoot) );
            return 0;
         }
         DdNode* bVar = Cudd_bddIthVar(dd, iFanin);
         int id = 0, j = 0;
         Abc_Obj_t* pPi = 0;
         Abc_NtkForEachPi(pNtk, pPi, j) {
            if (pPi->Id == pFanin->Id) {
               id = j;
               break;
            }
         }
         if (argv[1][id] == '0') 
            y = Cudd_Cofactor(dd, y, Cudd_Not(bVar));
         else if (argv[1][id] == '1')
            y = Cudd_Cofactor(dd, y, bVar);
         else {
            printf("invalid input!!!!\n");
            return 0;
         }   
         // delete buff; 
      }
      printf("%s: ", Abc_ObjName(pPo));
      printf("%d\n", y==dd->one);
   }
  
   return 1;
}

int zero2one(int n, int k) 
{
   return n | (1 << k);
}

int one2zero(int n, int k)
{
   return n & ~(1 << k);
}

void simulate(Abc_Ntk_t* pNtk)
{
   int ithGate = 0;
   Abc_Obj_t* gate = 0;
   Abc_NtkForEachObj(pNtk, gate, ithGate) {
      if (gate->Type == 7) {
         int fanin0 = gate->fCompl0==1 ? ~(Abc_ObjFanin0(gate)->iTemp) : Abc_ObjFanin0(gate)->iTemp;
         int fanin1 = gate->fCompl1==1 ? ~(Abc_ObjFanin1(gate)->iTemp) : Abc_ObjFanin1(gate)->iTemp;
         gate->iTemp = (fanin0 & fanin1);
      }
   }
   
}

int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv)
{
   if (argc != 2) {
      printf("invalid number of argument\n");
      return 0;
   }
   Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
   if (!Abc_NtkIsStrash(pNtk)) {
      printf("Please strash first\n");
      return 0;
   }
   FILE* fPtr = 0;
   fPtr = fopen(argv[1], "r");
   if (!fPtr) {
      printf("no such file\n");
      return 0;
   }

   int piNum = Abc_NtkPiNum(pNtk);
   int poNum = Abc_NtkPoNum(pNtk);
   long long maxPatternNum = 0, realPatternNum = 0;
   for (char c = getc(fPtr); c != EOF; c = getc(fPtr))
      if (c == '\n') // Increment count if this character is newline
         maxPatternNum = maxPatternNum + 1;
   fclose(fPtr);
   bool poOutput[poNum][maxPatternNum] = {0};
   char* pattern = (char*)malloc((piNum+3)*sizeof(char));
   int ithPi = 0, ithPo = 0, count = 0;
   Abc_Obj_t* pPi = 0;
   Abc_Obj_t* pPo = 0;
   
   fPtr = fopen(argv[1], "r");
   while (fgets(pattern, piNum+3, fPtr)) {
      if (strlen(pattern)-1 != piNum) {
         // printf("len = %ld\n", strlen(pattern)-1);
         printf("invalid pattern num\n");
         return 0;
      }
      Abc_NtkForEachPi(pNtk, pPi, ithPi) {
         if (pattern[ithPi] == '0')
            pPi->iTemp = one2zero(pPi->iTemp, count);
         else if (pattern[ithPi] == '1')
            pPi->iTemp = zero2one(pPi->iTemp, count);
         else {
            printf("invalid pattern!!!! \n");
            printf("%s", pattern);
            return 0;
         }
      }
      realPatternNum++; count++;
      if (count == 32) {
         simulate(pNtk);
         count = 0;
         Abc_NtkForEachPo(pNtk, pPo, ithPo) {
            int simVal = pPo->fCompl0 ==1 ? ~(Abc_ObjFanin0(pPo)->iTemp) : Abc_ObjFanin0(pPo)->iTemp;
            for (long long i = realPatternNum-32, j = 0; i < realPatternNum; i++, j++) {
               poOutput[ithPo][i] = (simVal >> j) & 1;
            }
         }
      }     
   }
   if (count > 0 && count < 32) {
      simulate(pNtk);
      Abc_NtkForEachPo(pNtk, pPo, ithPo) {
         int simVal = pPo->fCompl0 ==1 ? ~(Abc_ObjFanin0(pPo)->iTemp) : Abc_ObjFanin0(pPo)->iTemp;
         for (long long i = realPatternNum-count, j = 0; i < realPatternNum; i++, j++) {
            poOutput[ithPo][i] = (simVal >> j) & 1;
         }
      }
   }
   

   Abc_NtkForEachPo(pNtk, pPo, ithPo) {
      printf("%s: ", Abc_ObjName(pPo));
      for (int i = 0; i < realPatternNum; i++) {
         printf("%d", poOutput[ithPo][i]);
      }
      printf("\n");
   }

   fclose(fPtr);
   return 1;
}