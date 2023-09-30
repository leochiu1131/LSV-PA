#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
//add library for problem 4.1 and 4.2
#include <string>
#include <iostream>
#include <fstream>
#include <map>
using namespace std;

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
//add function for problem 4.1 and 4.2
static int Lsv_CommandSimBDD(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimAIG(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
    Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
    //add command for problem 4.1 and 4.2
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", Lsv_CommandSimBDD, 0);
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig", Lsv_CommandSimAIG, 0);
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

// problem 4.1 procedure part
void Lsv_NtkSimBDD(Abc_Ntk_t* pNtk, string input_pattern) {
    Abc_Obj_t* pPo;
    Abc_Obj_t* pPi;
    int i_po, i_pi;

    Abc_NtkForEachPo(pNtk, pPo, i_po) {
        Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo);
        DdManager* root_dd = (DdManager*)pRoot->pNtk->pManFunc;
        DdNode* root_ddnode = (DdNode*)pRoot->pData;

        char** bdd_pi_name_list = (char**)Abc_NodeGetFaninNames(pRoot)->pArray;

        DdNode* ans_node = root_ddnode;

        for (int i = 0; i < Abc_NtkPiNum(pNtk); i++) {
            if (bdd_pi_name_list[i] != NULL) {
                if (bdd_pi_name_list[i][0] != '\0') {
                    Abc_NtkForEachPi(pNtk, pPi, i_pi) {
                        string a = Abc_ObjName(pPi);
                        string b = bdd_pi_name_list[i];
                        if (a == b) {
                            DdNode* pi_now_node = root_dd->vars[i];
                            switch (input_pattern[i_pi]) {
                            case '0':
                                pi_now_node = Cudd_Not(pi_now_node);
                                ans_node = Cudd_Cofactor(root_dd, ans_node, pi_now_node);
                                break;
                            case '1':
                                ans_node = Cudd_Cofactor(root_dd, ans_node, pi_now_node);
                                break;
                            default:
                                break;
                            }
                        }
                    }
                }
            }
        }
        DdNode* one = DD_ONE(root_dd);
        if (ans_node == one) {
            printf("%s: 1\n", Abc_ObjName(pPo));
        }
        else {
            printf("%s: 0\n", Abc_ObjName(pPo));
        }
    }
}

int Lsv_CommandSimBDD(Abc_Frame_t* pAbc, int argc, char** argv) {
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    if (!pNtk) {
        Abc_Print(-1, "Empty network.\n");
        return 1;
    }

    string input_pattern = "";
    bool legal_input = false;
    if (argc == 2) {
        legal_input = true;
        std::string origin_input = argv[1];
        for (int i = 0; i < origin_input.size(); i++) {
            switch (origin_input[i]) {
            case '0':
                input_pattern += "0";
                break;
            case '1':
                input_pattern += "1";
                break;
            default:
                legal_input = false;
                break;
            }
        }
        if (Abc_NtkPiNum(pNtk) != input_pattern.size()) {
            legal_input = false;
        }
        //printf("now argv is:%s\n", input_pattern.c_str());
    }

    if (Abc_NtkIsBddLogic(pNtk)) {
        if (legal_input) {
            Lsv_NtkSimBDD(pNtk, input_pattern);
        }
        else {
            printf("illegal input.\n");
        }
    }
    else {
        printf("Not bdd now.\n");
    }

    return 0;
}

// problem 4.2 procedure part
void Lsv_NtkSimAIG(Abc_Ntk_t* pNtk, string* input_patterns) {
    Abc_Obj_t* pObj;
    int i;
    map<int, unsigned int> pattern_dict;

    int total_bit = input_patterns[0].size();
    string ans_32[Abc_NtkPoNum(pNtk)] = { "" };
    Abc_NtkForEachPo(pNtk, pObj, i) {
        ans_32[i] = "";
    }

    for (int k = 0; k <= total_bit / 32; k++) {
        Abc_NtkForEachPi(pNtk, pObj, i) {
            for (int v = 31; v >= 0; v--) {
                if (k * 32 + v < total_bit) {
                    pattern_dict[Abc_ObjId(pObj)] = pattern_dict[Abc_ObjId(pObj)] << 1;
                    if (input_patterns[i][k * 32 + v] == '1') {
                        pattern_dict[Abc_ObjId(pObj)] += 1;
                    }
                }
            }
        }

        Abc_NtkForEachNode(pNtk, pObj, i) {
            unsigned int p = pattern_dict[Abc_ObjFaninId0(pObj)];
            unsigned int q = pattern_dict[Abc_ObjFaninId1(pObj)];
            if (Abc_ObjFaninC0(pObj)) {
                p = ~p;
            }
            if (Abc_ObjFaninC1(pObj)) {
                q = ~q;
            }
            pattern_dict[Abc_ObjId(pObj)] = p & q;
        }

        Abc_NtkForEachPo(pNtk, pObj, i) {
            unsigned int ans_32_po = pattern_dict[Abc_ObjFaninId0(pObj)];
            if (Abc_ObjFaninC0(pObj)) {
                ans_32_po = ~ans_32_po;
            }
            pattern_dict[Abc_ObjId(pObj)] = ans_32_po;

            for (int v = 31; v >= 0; v--) {
                if (k * 32 + v < total_bit) {
                    if (ans_32_po % 2 == 0) {
                        ans_32[i] += '0';
                    }
                    else {
                        ans_32[i] += '1';
                    }
                    ans_32_po = ans_32_po >> 1;
                }
            }
        }
    }

    Abc_NtkForEachPo(pNtk, pObj, i) {
        printf("%s: %s\n", Abc_ObjName(pObj), ans_32[i].c_str());
    }
}

int Lsv_CommandSimAIG(Abc_Frame_t* pAbc, int argc, char** argv) {
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    if (!pNtk) {
        Abc_Print(-1, "Empty network.\n");
        return 1;
    }

    string input_patterns[Abc_NtkPiNum(pNtk)] = { "" };
    bool legal_input = false;
    if (argc == 2) {
        legal_input = true;
        string pattern_file = argv[1];
        ifstream ifs;
        ifs.open(pattern_file);
        if (!ifs.is_open()) {
            cout << "Failed to open file.\n";
            return 0;
        }
        string each_pattern;
        while (getline(ifs, each_pattern)) {
            if (Abc_NtkPiNum(pNtk) <= each_pattern.size()) {
                for (int i = 0; i < Abc_NtkPiNum(pNtk); i++) {
                    switch (each_pattern[i]) {
                    case '0':
                        input_patterns[i] += "0";
                        break;
                    case '1':
                        input_patterns[i] += "1";
                        break;
                    default:
                        legal_input = false;
                        break;
                    }
                }
            }
            else {
                legal_input = false;
            }
            if (!legal_input) {
                break;
            }
        }
        ifs.close();

        //for (int i = 0; i < Abc_NtkPiNum(pNtk); i++) {
        //    printf("input flow %d is:%s\n", i, input_patterns[i].c_str());
        //}
    }

    if (Abc_NtkIsStrash(pNtk)) {
        if (legal_input) {
            Lsv_NtkSimAIG(pNtk, input_patterns);
        }
        else {
            printf("illegal input.\n");
        }
    }
    else {
        printf("Not strash now.\n");
    }

    return 0;
}