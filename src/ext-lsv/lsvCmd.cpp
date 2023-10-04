#include <stdlib.h>

#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "bdd/cudd/cudd.h"

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimBdd(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSimAIG(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
    Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", Lsv_CommandSimBdd, 0);
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

using namespace std;

int Lsv_CommandSimBdd(Abc_Frame_t* pAbc, int argc, char** argv) {
    Abc_Ntk_t* pNtk;
    Abc_Obj_t *pPo, *pPi;
    int ithPo, ithPi;
    int c = 0;

    pNtk = Abc_FrameReadNtk(pAbc);
    string inputPattern;
    unordered_map<string, int> piName2Value;

    // ensure Ntk exist
    if (pNtk == NULL) {
        Abc_Print(-1, "Empty network.\n");
        return 1;
    }
    if (!Abc_NtkIsBddLogic(pNtk)) {
        Abc_Print(-1, "Simulating BDDs can only be done for BDD networks (run \"collapse\").\n");
        return 1;
    }

    // get input pattern, assume input pattern = order
    if (argc != 2) goto usage;
    inputPattern = argv[1];

    // check if valid
    if (inputPattern.length() != Abc_NtkPiNum(pNtk)) goto usage;
    Abc_NtkForEachPi(pNtk, pPi, ithPi) {
        if (inputPattern[ithPi] != '1' && inputPattern[ithPi] != '0') goto usage;
        string piName = Abc_ObjName(pPi);
        piName2Value[piName] = inputPattern[ithPi] == '1' ? 1 : 0;
    }

    Extra_UtilGetoptReset();
    while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
        switch (c) {
            case 'h':
                goto usage;
            default:
                goto usage;
        }
    }

    // 1. To find the BDD node associated with each PO, use the codes below.
    Abc_NtkForEachPo(pNtk, pPo, ithPo) {
        Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo);
        assert(Abc_NtkIsBddLogic(pRoot->pNtk));
        DdManager* dd = (DdManager*)pRoot->pNtk->pManFunc;
        DdNode* ddnode = (DdNode*)pRoot->pData;  // origin

        // 2. To find the variable order of the BDD, you may use the following codes to find the variable name array.
        char** vNamesIn = (char**)Abc_NodeGetFaninNames(pRoot)->pArray;
        Abc_Obj_t* pFanin;
        DdNode* g;
        int j;
        Abc_ObjForEachFanin(pRoot, pFanin, j) {
            g = (piName2Value[vNamesIn[j]] == 1) ? Cudd_bddIthVar(dd, j) : (Cudd_Not(Cudd_bddIthVar(dd, j)));
            ddnode = Cudd_Cofactor(dd, ddnode, g);  // f with respect to g
        }

        // print result
        cout << Abc_ObjName(pPo) << ": ";
        if (ddnode == DD_ONE(dd))
            cout << "1";
        else if (ddnode == Cudd_Not(DD_ONE(dd)) || ddnode == DD_ZERO(dd))
            cout << "0";
        else
            assert(0);
        cout << endl;
    }
    return 0;

usage:
    Abc_Print(-2, "usage: lsv_sim_bdd <input_pattern>\n");
    Abc_Print(-2, "\t          do simulations for a given BDD and an input pattern\n");
    return 1;
}

void simulateAIG32times(Abc_Ntk_t* pNtk, vector<int>& inputPattern) {
    Abc_Obj_t *pObj, *pPi, *pPo, *pFanin;
    int ithObj, ithPi, ithPo, ithFanin;

    // assign inputpattern
    Abc_NtkForEachPi(pNtk, pPi, ithPi) {
        pPi->iTemp = inputPattern[ithPi];
    }

    // propogate value
    Abc_NtkForEachObj(pNtk, pObj, ithObj) {
        if (pObj->Type != 7) continue;
        vector<Abc_Obj_t*> fanin;
        Abc_ObjForEachFanin(pObj, pFanin, ithFanin) {
            fanin.emplace_back(pFanin);
        }
        pObj->iTemp = (pObj->fCompl0 ? ~fanin[0]->iTemp : fanin[0]->iTemp) & (pObj->fCompl1 ? ~fanin[1]->iTemp : fanin[1]->iTemp);
    }

    // output read
    Abc_NtkForEachPo(pNtk, pPo, ithPo) {
        Abc_Obj_t* fanin;
        Abc_ObjForEachFanin(pPo, pFanin, ithFanin) {
            fanin = pFanin;
        }
        pPo->iTemp = pPo->fCompl0 ? ~fanin->iTemp : fanin->iTemp;
    }

    return;
}

int Lsv_CommandSimAIG(Abc_Frame_t* pAbc, int argc, char** argv) {
    Abc_Ntk_t* pNtk;
    int c = 0,ithPo,restOfPattern;
    vector<int> inputPattern;
    string inputFileName;
    ifstream patternFile;
    Abc_Obj_t* pPo;

    // read Ntk
    pNtk = Abc_FrameReadNtk(pAbc);
    vector<string> outputPattern(Abc_NtkPoNum(pNtk));

    // ensure Ntk exist
    if (pNtk == NULL) {
        Abc_Print(-1, "Empty network.\n");
        return 1;
    }
    if (!Abc_NtkIsStrash(pNtk)) {
        Abc_Print(-1, "Simulating AIGs can only be done for AIG networks (run \"strash\").\n");
        return 1;
    }

    // get input name
    if (argc != 2) goto usage;
    inputFileName = argv[1];

    Extra_UtilGetoptReset();
    while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
        switch (c) {
            case 'h':
                goto usage;
            default:
                goto usage;
        }
    }

    // read inputFile
    patternFile.open(inputFileName);
    if (patternFile.is_open()) {
        string line;
        int count_32 = 0;
        inputPattern.clear();
        inputPattern = vector<int>(Abc_NtkPiNum(pNtk));

        while (patternFile >> line) {
            if (line.size() != Abc_NtkPiNum(pNtk)) {
                cout << "Error: Pattern(" << line << ") length(" << line.size() << ") does not match the number of inputs(" << Abc_NtkPiNum(pNtk) << ") in a circuit !!" << endl;
                return 0;
            }
            for (size_t i = 0; i < Abc_NtkPiNum(pNtk); ++i) {
                if (line[i] == '0')
                    inputPattern[i] = inputPattern[i] << 1;
                else if (line[i] == '1') {
                    inputPattern[i] = inputPattern[i] << 1;
                    inputPattern[i] += 1;
                } else {
                    cout << "  Error: Pattern(" << line << ") contains a non-0/1 character('" << line[i] << "')." << endl;
                    cout << "0 patterns simulated." << endl;
                    inputPattern.clear();
                    return 0;
                }
            }
            ++count_32;
            if (count_32 == 32) {
                count_32 = 0;
                simulateAIG32times(pNtk, inputPattern);  //<< TODO
                inputPattern.clear();
                inputPattern = vector<int>(Abc_NtkPiNum(pNtk));

                // record the output
                Abc_NtkForEachPo(pNtk, pPo, ithPo) {
                    for (int i = sizeof(int) * 8 - 1; i >= 0; i--) {
                        int bit = (pPo->iTemp >> i) & 1;
                        outputPattern[ithPo] += (bit) ? "1" : "0";
                    }
                }
            }
        }

        restOfPattern = count_32;
        // fill in 0
        while (count_32 != 32) {
            for (size_t i = 0; i < Abc_NtkPiNum(pNtk); ++i) {
                inputPattern[i] = inputPattern[i] << 1; 
            }
            ++count_32;
        }
        simulateAIG32times(pNtk, inputPattern);

        // record the output (restOfPattern)
        Abc_NtkForEachPo(pNtk, pPo, ithPo) {
            for (int i = sizeof(int) * 8 - 1; i >= sizeof(int) * 8 - restOfPattern; i--) {
                int bit = (pPo->iTemp >> i) & 1;
                outputPattern[ithPo] += (bit) ? "1" : "0";
            }
        }

        // print result
        Abc_NtkForEachPo(pNtk, pPo, ithPo) {
            cout << Abc_ObjName(pPo) << ": " << outputPattern[ithPo] << endl;
        }
    } else {
        cout << "   Error: file can't open" << endl;
        return 1;
    }

    return 0;

usage:
    Abc_Print(-2, "usage: lsv_sim_aig <input_pattern>\n");
    Abc_Print(-2, "\t          do simulations for a given AIG and an input pattern\n");
    return 1;
}
