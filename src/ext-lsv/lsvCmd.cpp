#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "bdd/cudd/cudd.h"
#include <unordered_map>
#include <fstream>
#include <math.h>
#include <string>

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
// y3: 2147516416
// 10000000000000001000000000000000
// y2: 1275087872
// 01001100000000000100110000000000
// y1: 1790995136
// 01101010110000000110101011000000
// y0: 2694881440
// 10100000101000001010000010100000

// void decToBinary(unsigned n)
// {
//     // array to store binary number
//     int binaryNum[32];
  
//     // counter for binary array
//     int i = 0;
//     while (n > 0) {
//         // storing remainder in binary array
//         binaryNum[i] = n % 2;
//         n = n / 2;
//         i++;
//     }
  
//     // printing binary array in reverse order
//     for (int j = i - 1; j >= 0; j--)
//         printf("%d", binaryNum[j]);
//     printf("\n");
// }


void Lsv_NtkSimAig(Abc_Ntk_t* pNtk, char* infile) {
  std::ifstream fin;  std::string input;  fin.open(infile);
  
  Abc_Obj_t *pPi, *pPo, *pNode;
  int numPi=0, numNode=0, numPo=0; 
  int iPi, iNode, iPo, iLine=0; 
  std::unordered_map<std::string , unsigned> m;
  std::unordered_map<std::string, std::string> ans;

  Abc_NtkForEachPi(pNtk, pPi, iPi) {
    m[Abc_ObjName(pPi)] = 0;
    numPi++;
  }
  Abc_NtkForEachNode(pNtk, pNode, iNode) {
    m[Abc_ObjName(pNode)] = 0;
    numNode++;
  }
  Abc_NtkForEachPo(pNtk, pPo, iPo) {
    m[Abc_ObjName(pPo)] = 0;
    ans[Abc_ObjName(pPo)] = "";
    numPo++;
  }

  while(std::getline(fin, input)) {
    Abc_NtkForEachPi(pNtk, pPi, iPi) {
      m[Abc_ObjName(pPi)] = m[Abc_ObjName(pPi)] + ((input.c_str()[iPi] == '1') ? unsigned(pow(2, (iLine % 32))) : 0) ;
      numPi++;
    }
    if(iLine % 32 == 31){
      // simulation
      Abc_NtkForEachNode(pNtk, pNode, iNode) {
        if (m[Abc_ObjName(pNode)] == 0){ // not present
          if (!Abc_ObjFaninC0(pNode) && !Abc_ObjFaninC1(pNode)) { // + +
            m[Abc_ObjName(pNode)] = m[Abc_ObjName(Abc_ObjFanin0(pNode))] & m[Abc_ObjName(Abc_ObjFanin1(pNode))];
          }
          else if (!Abc_ObjFaninC0(pNode) && Abc_ObjFaninC1(pNode)) { //  + -
            m[Abc_ObjName(pNode)] = m[Abc_ObjName(Abc_ObjFanin0(pNode))] & ~m[Abc_ObjName(Abc_ObjFanin1(pNode))];
          }
          else if (Abc_ObjFaninC0(pNode) && !Abc_ObjFaninC1(pNode)) { // - +
            m[Abc_ObjName(pNode)] = ~m[Abc_ObjName(Abc_ObjFanin0(pNode))] & m[Abc_ObjName(Abc_ObjFanin1(pNode))];
          }
          else if (Abc_ObjFaninC0(pNode) && Abc_ObjFaninC1(pNode)) { // - -
            m[Abc_ObjName(pNode)] = ~m[Abc_ObjName(Abc_ObjFanin0(pNode))] & ~m[Abc_ObjName(Abc_ObjFanin1(pNode))];
          }
        }
      }
      Abc_NtkForEachPo(pNtk, pNode, iNode) {
        if (m[Abc_ObjName(pNode)] == 0){ // not present
          if (!Abc_ObjFaninC0(pNode)) { // +
            m[Abc_ObjName(pNode)] = m[Abc_ObjName(Abc_ObjFanin0(pNode))];
          }
          else{ // -
            m[Abc_ObjName(pNode)] = ~m[Abc_ObjName(Abc_ObjFanin0(pNode))];
          }
        }

        for (int i = 0; i < 32; i++) {
          if(m[Abc_ObjName(pNode)] > 0) {
            if(m[Abc_ObjName(pNode)] % 2 == 1){
              ans[Abc_ObjName(pNode)] = ans[Abc_ObjName(pNode)] + "1";
            }
            else{
              ans[Abc_ObjName(pNode)] = ans[Abc_ObjName(pNode)] + "0";
            }
            m[Abc_ObjName(pNode)] = m[Abc_ObjName(pNode)] / 2;
          }
          else{
            ans[Abc_ObjName(pNode)] = ans[Abc_ObjName(pNode)] + "0";
          }
        }
      }

      // after 32 bits simulation, clear
      Abc_NtkForEachPi(pNtk, pPi, iPi) {
        m[Abc_ObjName(pPi)] = 0;
      }
      Abc_NtkForEachNode(pNtk, pNode, iNode) {
        m[Abc_ObjName(pNode)] = 0;
      }
      Abc_NtkForEachPo(pNtk, pPo, iPo) {
        m[Abc_ObjName(pPo)] = 0;
      }
    }

    iLine++;
  }

  if(iLine % 32 != 0){ // some of them not simulated yet
      Abc_NtkForEachNode(pNtk, pNode, iNode) {
        if (m[Abc_ObjName(pNode)] == 0){ // not present
          if (!Abc_ObjFaninC0(pNode) && !Abc_ObjFaninC1(pNode)) { // + +
            m[Abc_ObjName(pNode)] = m[Abc_ObjName(Abc_ObjFanin0(pNode))] & m[Abc_ObjName(Abc_ObjFanin1(pNode))];
          }
          else if (!Abc_ObjFaninC0(pNode) && Abc_ObjFaninC1(pNode)) { //  + -
            m[Abc_ObjName(pNode)] = m[Abc_ObjName(Abc_ObjFanin0(pNode))] & ~m[Abc_ObjName(Abc_ObjFanin1(pNode))];
          }
          else if (Abc_ObjFaninC0(pNode) && !Abc_ObjFaninC1(pNode)) { // - +
            m[Abc_ObjName(pNode)] = ~m[Abc_ObjName(Abc_ObjFanin0(pNode))] & m[Abc_ObjName(Abc_ObjFanin1(pNode))];
          }
          else if (Abc_ObjFaninC0(pNode) && Abc_ObjFaninC1(pNode)) { // - -
            m[Abc_ObjName(pNode)] = ~m[Abc_ObjName(Abc_ObjFanin0(pNode))] & ~m[Abc_ObjName(Abc_ObjFanin1(pNode))];
          }
        }
      }
      Abc_NtkForEachPo(pNtk, pNode, iNode) {
        if (m[Abc_ObjName(pNode)] == 0){ // not present
          if (!Abc_ObjFaninC0(pNode)) { // +
            m[Abc_ObjName(pNode)] = m[Abc_ObjName(Abc_ObjFanin0(pNode))];
          }
          else{ // -
            m[Abc_ObjName(pNode)] = ~m[Abc_ObjName(Abc_ObjFanin0(pNode))];
          }
        }
      
        for (int i = 0; i < (iLine % 32); i++) {
          if(m[Abc_ObjName(pNode)] > 0) {
            if(m[Abc_ObjName(pNode)] % 2 == 1){
              ans[Abc_ObjName(pNode)] = ans[Abc_ObjName(pNode)] + "1";
            }
            else{
              ans[Abc_ObjName(pNode)] = ans[Abc_ObjName(pNode)] + "0";
            }
            m[Abc_ObjName(pNode)] = m[Abc_ObjName(pNode)] / 2;
          }
          else{
            ans[Abc_ObjName(pNode)] = ans[Abc_ObjName(pNode)] + "0";
          }
        }
      }
  }

  Abc_NtkForEachPo(pNtk, pNode, iNode) {
    printf("%s: %s\n", Abc_ObjName(pNode), ans[Abc_ObjName(pNode)].c_str());
  }
}











void Lsv_NtkSimBdd(Abc_Ntk_t* pNtk, char* sim){
  Abc_Obj_t* pPi; Abc_Obj_t* pPo;
  int ithPi; int ithPo;

  Abc_NtkForEachPo(pNtk, pPo, ithPo) {
      Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo); 
      assert( Abc_NtkIsBddLogic(pRoot->pNtk) );
      DdManager * dd = (DdManager *)pRoot->pNtk->pManFunc;  
      DdNode* ddnode = (DdNode *)pRoot->pData;

      Abc_NtkForEachPi(pNtk, pPi, ithPi) {
        int id = Vec_IntFind( &pRoot->vFanins, pPi->Id );

        if(id != -1){
          if(sim[ithPi] == '0'){
            ddnode = Cudd_Cofactor(dd, ddnode,Cudd_Not(Cudd_bddIthVar( dd, id))); 
          }
          else{
            ddnode = Cudd_Cofactor(dd, ddnode,Cudd_bddIthVar( dd, id)); 
          }
        }

      }
      if(ddnode == dd->one){
        printf("%s: 1\n", Abc_ObjName(pPo));
      }
      else{
        printf("%s: 0\n", Abc_ObjName(pPo));
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

int Lsv_CommandSimBdd(Abc_Frame_t* pAbc, int argc, char** argv) {
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
  Lsv_NtkSimBdd(pNtk, argv[1]);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sim_bdd [-h] <sim pattern>\n");
  Abc_Print(-2, "\t        simulation with cofactor\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}


int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv) {
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
  Lsv_NtkSimAig(pNtk, argv[1]);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sim_bdd [-h] <sim pattern>\n");
  Abc_Print(-2, "\t        simulation with cofactor\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}