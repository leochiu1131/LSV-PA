#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"

#include <unordered_map>
#include <vector>
#include <iostream>
#include <fstream>
#include <map>
#include <string>
using namespace std;

static int Lsv_CommandSimBdd(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", Lsv_CommandSimBdd, 0);
    Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig", Lsv_CommandSimAig, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;


void Lsv_NtkSimBdd(Abc_Ntk_t* pNtk, string pattern) {
  Abc_Obj_t* pPo;
  int i_po;

  Abc_NtkForEachPo(pNtk, pPo, i_po) {
    Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo);
    DdManager * dd = (DdManager*)pRoot->pNtk->pManFunc;
    DdNode* ddnode = (DdNode*)pRoot->pData;


    DdNode* cofactor = ddnode;

    int i ;
    Abc_Obj_t* pFanin;
    Abc_ObjForEachFanin(pRoot, pFanin, i) {
      char * name_fanin = Abc_ObjName(pFanin);
      Abc_Obj_t* pPi;
      int i_pi;
      Abc_NtkForEachPi(pNtk, pPi, i_pi) {
        char * name_pi = Abc_ObjName(pPi);
        if (name_pi == name_fanin) {
          if ( pattern[i_pi] == '0' ) {
            cofactor = Cudd_Cofactor(dd, cofactor, Cudd_Not(Cudd_bddIthVar(dd, i)));
            break;
          }
          else if ( pattern[i_pi] == '1' ) {
            cofactor = Cudd_Cofactor(dd, cofactor, Cudd_bddIthVar(dd, i));
            break;
          }
          else {
            printf("pattern exist invalid number\n");
            break;
          }
        }
      }
    }

    DdNode* compare = DD_ONE(dd);
    int resultValue;
    printf("%s",Abc_ObjName(pPo));
    if (cofactor == compare) {
      resultValue = 1;
      printf(": %d\n", resultValue);
    }
    else {
      resultValue = 0;
      printf(": %d\n", resultValue);
    }
  }
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
  Abc_Print(-2, "usage: lsv_sim_bdd [-h]\n");
  Abc_Print(-2, "\t        prints the nodes in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

void Lsv_NtkSimAig(Abc_Ntk_t* pNtk, ifstream& infile) {
  // printf("start now\n");
  string line;
  vector<string> pattern;
  while (getline(infile, line)) {
    pattern.push_back(line);
  }
  infile.close();

  Abc_Obj_t* pObj;
  int i;
  map<Abc_Obj_t*, string> obj2pattern;
  Abc_NtkForEachPi(pNtk, pObj, i) {
  for (int ii = 0; ii < pattern.size() ; ii++)
    {
      obj2pattern[pObj] += pattern[ii][i];
    }
  }

  Abc_NtkForEachObj(pNtk, pObj, i) {
    if(Abc_ObjIsPi(pObj) || pObj->Type == ABC_OBJ_CONST1 || Abc_ObjIsPo(pObj))
    {
      continue;
    }
    string ff = "";
    for (int j = 0; j < pattern.size(); j++) {
      int num0, num1;
      if (obj2pattern[Abc_ObjFanin0(pObj)][j] == '1')
      {
        num0 = 1;
      }
      else
      {
        num0 = 0;
      }

      if (obj2pattern[Abc_ObjFanin1(pObj)][j] == '1')
      {
        num1 = 1;
      }
      else
      {
        num1 = 0;
      }

      if (Abc_ObjFaninC0(pObj))
      {
        if (num0 == 1)
        {
          num0 = 0;
        }
        else
        {
          num0 = 1;
        }
      }

      if (Abc_ObjFaninC1(pObj)) {
        if (num1 == 1)
        {
          num1 = 0;
        }
        else
        {
          num1 = 1;
        }
      }
          
      if (num0 == 1 && num1 == 1)
      {
        ff+= '1';
      }
      else {
        ff+= '0';
      }
    }
    obj2pattern[pObj] = ff;
  }

  Abc_NtkForEachObj(pNtk, pObj, i) {
    if(Abc_ObjIsPo(pObj)){
      string final = "";
      int num0;
      for (int j = 0; j < pattern.size(); j++) {
        if ( obj2pattern[Abc_ObjFanin0(pObj)][j] == '1' )
        {
          num0 = 1;
          if (Abc_ObjFaninC0(pObj))
          {
            num0 = 0;
          }
        }
        else
        {
          num0 = 0;
          if (Abc_ObjFaninC0(pObj))
          {
            num0 = 1;
          }
        }
        if (num0 == 0) 
        {
          final+= '0';
        }
        else 
        {
          final+= '1';
        }
      }
      obj2pattern[pObj] = final;

      printf("%s", Abc_ObjName(pObj));
      printf(": %s\n", final.c_str());
    }
  }
}

int Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  Extra_UtilGetoptReset();
  // while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
  //   switch (c) {
  //     case 'h':
  //       goto usage;
  //     default:
  //       goto usage;
  //   }
  // }
  if (!pNtk) {
      Abc_Print(-1, "Empty network.\n");
      return 1;
  }
  ifstream infile;
  infile.open(argv[1]);

  if (Abc_NtkIsStrash(pNtk)) {
    Lsv_NtkSimAig(pNtk, infile);
  }
  else {
    printf("Need to be strashed first\n");
  }
  return 0;

// usage:
//   Abc_Print(-2, "usage: lsv_sim_bdd [-h]\n");
//   Abc_Print(-2, "\t        prints the nodes in the network\n");
//   Abc_Print(-2, "\t-h    : print the command usage\n");
//   return 1;
} 



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