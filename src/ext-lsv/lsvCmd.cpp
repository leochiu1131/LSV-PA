#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "bdd/cudd/cudd.h" // Founded by `grep -rni "cudd"
#include <string>
#include <math.h>
#include <fstream>
#include <unordered_map>
// The text data in demo.in is a set about `lsv/pa1/mlif.blif`

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandBDD_Sim(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandAIG_Sim(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", Lsv_CommandBDD_Sim, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig", Lsv_CommandAIG_Sim, 0);
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

void Lsv_NtkAIGSim(Abc_Ntk_t* pNtk, char* pFile)
{
  // declaration
  std::ifstream file_in(pFile);
  std::string input;
  Abc_Obj_t *p_Pi, *p_Po, *p_Node;
  int index_pi, index_po, index_node;
  int index_edge=0; // index
  std::unordered_map<std::string, unsigned> map;
  std::unordered_map<std::string, std::string> ans; 
  //Network Initialization

  Abc_NtkForEachPi(pNtk, p_Pi, index_pi) 
  {
    map[Abc_ObjName(p_Pi)] = 0; //initialization
  }

  Abc_NtkForEachNode(pNtk, p_Node, index_node)
  {
    map[Abc_ObjName(p_Node)] = 0; //initialization
  }
  
  Abc_NtkForEachPo(pNtk, p_Po, index_po)
  {
    map[Abc_ObjName(p_Po)] = 0; // initialization
    ans[Abc_ObjName(p_Po)] = ""; // init for string.
  }

  while (std::getline(file_in, input))
  {
    Abc_NtkForEachPi(pNtk, p_Pi, index_pi)
    {
      map[Abc_ObjName(p_Pi)] = map[Abc_ObjName(p_Pi)] + ( (input.c_str()[index_pi]== '1') ? unsigned(pow(2, index_edge%32))  : 0 );
    }
    if(index_edge%32==31) // 32-bit checker
    {
      Abc_NtkForEachNode(pNtk, p_Node, index_node) // AIG branching
      {
        if(map[Abc_ObjName(p_Node)] == 0) // if pi is equal to 0, no value
        {
          if(!Abc_ObjFaninC0(p_Node) && !Abc_ObjFaninC1(p_Node)) //!(00)
          {
            map[Abc_ObjName(p_Node)] = map[Abc_ObjName(Abc_ObjFanin0(p_Node))] & map[Abc_ObjName(Abc_ObjFanin1(p_Node))];
          }
          else if (!Abc_ObjFaninC0(p_Node) && Abc_ObjFaninC1(p_Node)) //!(01)
          {
            map[Abc_ObjName(p_Node)] = map[Abc_ObjName(Abc_ObjFanin0(p_Node))] & ~map[Abc_ObjName(Abc_ObjFanin1(p_Node))];
          }
          else if (Abc_ObjFaninC0(p_Node) && !Abc_ObjFaninC1(p_Node)) //!(10)
          {
            map[Abc_ObjName(p_Node)] = ~map[Abc_ObjName(Abc_ObjFanin0(p_Node))] & map[Abc_ObjName(Abc_ObjFanin1(p_Node))];
          }
          else if (Abc_ObjFaninC0(p_Node) && Abc_ObjFaninC1(p_Node)) //!(11)
          {
            map[Abc_ObjName(p_Node)] = ~map[Abc_ObjName(Abc_ObjFanin0(p_Node))] & ~map[Abc_ObjName(Abc_ObjFanin1(p_Node))];
          }
        }
      }
      Abc_NtkForEachPo(pNtk, p_Node, index_node)
      {
        if(map[Abc_ObjName(p_Node)] == 0)
        {
          if(!Abc_ObjFaninC0(p_Node))
          {
            map[Abc_ObjName(p_Node)] = map[Abc_ObjName(Abc_ObjFanin0(p_Node))];
          }
          else
          {
            map[Abc_ObjName(p_Node)] = ~map[Abc_ObjName(Abc_ObjFanin0(p_Node))];
          }
        }

        for (int i=0; i < 32; i++)
        {
          if(map[Abc_ObjName(p_Node)] > 0)
          {
            if(map[Abc_ObjName(p_Node)] % 2 == 1)
            {
              ans[Abc_ObjName(p_Node)] = ans[Abc_ObjName(p_Node)] + "1";
            }
            else
            {
              ans[Abc_ObjName(p_Node)] = ans[Abc_ObjName(p_Node)] + "0"; 
            }
            map[Abc_ObjName(p_Node)] = map[Abc_ObjName(p_Node)] / 2;
          }
          else
            ans[Abc_ObjName(p_Node)] = ans[Abc_ObjName(p_Node)] + "0";
        }
      }

      // Clear data
      Abc_NtkForEachPi(pNtk, p_Pi, index_pi)
      {
        map[Abc_ObjName(p_Pi)] = 0;
      }
      Abc_NtkForEachNode(pNtk, p_Node, index_node)
      {
        map[Abc_ObjName(p_Node)] = 0;
      }
      Abc_NtkForEachPo(pNtk, p_Po, index_po) 
      {
        map[Abc_ObjName(p_Po)] = 0;
      }
    }
    index_edge++;
  }
  if(index_edge % 32 != 0 )
  {
    Abc_NtkForEachNode(pNtk, p_Node, index_node) // AIG branching
    {
      if(map[Abc_ObjName(p_Node)] == 0) // if pi is equal to 0, no value
      {
        if(!Abc_ObjFaninC0(p_Node) && !Abc_ObjFaninC1(p_Node)) //!(00)
        {
          map[Abc_ObjName(p_Node)] = map[Abc_ObjName(Abc_ObjFanin0(p_Node))] & map[Abc_ObjName(Abc_ObjFanin1(p_Node))];
        }
        else if (!Abc_ObjFaninC0(p_Node) && Abc_ObjFaninC1(p_Node)) //!(01)
        {
          map[Abc_ObjName(p_Node)] = map[Abc_ObjName(Abc_ObjFanin0(p_Node))] & ~map[Abc_ObjName(Abc_ObjFanin1(p_Node))];
        }
        else if (Abc_ObjFaninC0(p_Node) && !Abc_ObjFaninC1(p_Node)) //!(10)
        {
          map[Abc_ObjName(p_Node)] = ~map[Abc_ObjName(Abc_ObjFanin0(p_Node))] & map[Abc_ObjName(Abc_ObjFanin1(p_Node))];
        }
        else if (Abc_ObjFaninC0(p_Node) && Abc_ObjFaninC1(p_Node)) //!(11)
        {
          map[Abc_ObjName(p_Node)] = ~map[Abc_ObjName(Abc_ObjFanin0(p_Node))] & ~map[Abc_ObjName(Abc_ObjFanin1(p_Node))];
        }
        }
    }  
    Abc_NtkForEachPo(pNtk, p_Node, index_node)
    {
      if(map[Abc_ObjName(p_Node)] == 0)
      {
        if(!Abc_ObjFaninC0(p_Node))
        {
          map[Abc_ObjName(p_Node)] = map[Abc_ObjName(Abc_ObjFanin0(p_Node))];
        }
        else
        {
          map[Abc_ObjName(p_Node)] = ~map[Abc_ObjName(Abc_ObjFanin0(p_Node))];
        }
      }
      for (int i=0; i < (index_edge) % 32; i++)
      {
        if(map[Abc_ObjName(p_Node)] > 0)
        {
          if(map[Abc_ObjName(p_Node)] % 2 == 1)
          {
            ans[Abc_ObjName(p_Node)] = ans[Abc_ObjName(p_Node)] + "1";
          }
          else
          {
            ans[Abc_ObjName(p_Node)] = ans[Abc_ObjName(p_Node)] + "0"; 
          }
          map[Abc_ObjName(p_Node)] = map[Abc_ObjName(p_Node)] / 2;
        }
        else
          ans[Abc_ObjName(p_Node)] = ans[Abc_ObjName(p_Node)] + "0";
      } 
    }
    Abc_NtkForEachPo(pNtk, p_Node, index_node)
    {
      printf("%s: %s\n", Abc_ObjName(p_Node), ans[Abc_ObjName(p_Node)].c_str());
    }
  }
}

void Lsv_NtkBDDsim(Abc_Ntk_t* pNtk, char* pFile)
{
  Abc_Obj_t *p_Po, *p_Pi;
  int index_Pi, index_Po;
  bool debug_flag=1;

  Abc_NtkForEachPo(pNtk, p_Po, index_Po) 
  {
    Abc_Obj_t* pRoot = Abc_ObjFanin0(p_Po); 
    assert( Abc_NtkIsBddLogic(pRoot->pNtk) );
    DdManager * dd = (DdManager *)pRoot->pNtk->pManFunc;  
    DdNode* ddnode = (DdNode *)pRoot->pData;
    Abc_NtkForEachPi(pNtk, p_Pi, index_Pi)
    {
      int id = Vec_IntFind(&pRoot -> vFanins, p_Pi->Id);

      if(id != -1)
      {
        if(pFile[index_Pi] == '0')
        {
          ddnode = Cudd_Cofactor(dd, ddnode, Cudd_Not(Cudd_bddIthVar(dd, id)));
        }
        else 
          ddnode = Cudd_Cofactor(dd, ddnode, Cudd_bddIthVar(dd, id));
      }
      // if (debug_flag) {
      //   if(ddnode = (dd->one))
      //     printf("%s %s: 1\n", Abc_ObjName(p_Pi), Abc_ObjName(p_Po));
      //   else
      //     printf("%s %s: 0\n", Abc_ObjName(p_Pi), Abc_ObjName(p_Po));
      // }
    }
    if(ddnode == (dd->one))
      printf("%s: 1\n", Abc_ObjName(p_Po));
    else
      printf("%s: 0\n", Abc_ObjName(p_Po));
  }
}

int Lsv_CommandAIG_Sim(Abc_Frame_t* pAbc, int argc, char** argv) {
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
  Lsv_NtkAIGSim(pNtk, argv[1]);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sim_aig [-h] <input_pattern_file>\n");
  Abc_Print(-2, "\t        enable cofactor func.\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

int Lsv_CommandBDD_Sim(Abc_Frame_t* pAbc, int argc, char** argv) {
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
  Lsv_NtkBDDsim(pNtk,argv[1]);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sim_bdd [-h] <input_pattern_file>\n");
  Abc_Print(-2, "\t        enable cofactor func.\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
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
