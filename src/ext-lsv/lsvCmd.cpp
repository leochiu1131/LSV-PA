#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"

#include "bdd/extrab/extraBdd.h"
#include "base/abc/abcShow.c"
#include "sat/cnf/cnf.h"

#include <unordered_map>
#include <fstream>
#include <vector>

extern "C" {
    Aig_Man_t *Abc_NtkToDar(Abc_Ntk_t *pNtk, int fExors, int fRegisters);
}

static int Lsv_CommandPrintNodes(Abc_Frame_t *pAbc, int argc, char **argv);
static int Lsv_CommandSimulateBDD(Abc_Frame_t *pAbc, int argc, char **argv);
static int Lsv_CommandSimulateAIG(Abc_Frame_t *pAbc, int argc, char **argv);
static int Lsv_CommandCheckSymmetryBDD(Abc_Frame_t *pAbc, int argc, char **argv);
static int Lsv_CommandCheckSymmetrySAT(Abc_Frame_t *pAbc, int argc, char **argv);

void init(Abc_Frame_t *pAbc) {
    Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", Lsv_CommandSimulateBDD, 0);
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig", Lsv_CommandSimulateAIG, 0);
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_bdd", Lsv_CommandCheckSymmetryBDD, 0);
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_sat", Lsv_CommandCheckSymmetrySAT, 0);
}

void destroy(Abc_Frame_t *pAbc) {}

Abc_FrameInitializer_t frame_initializer = { init, destroy };

struct PackageRegistrationManager {
    PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

void Lsv_NtkPrintNodes(Abc_Ntk_t *pNtk) {
    Abc_Obj_t *pObj;
    int i;
    Abc_NtkForEachNode(pNtk, pObj, i) {
        printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
        Abc_Obj_t *pFanin;
        int j;
        Abc_ObjForEachFanin(pObj, pFanin, j) {
            printf("  Fanin-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanin),
                Abc_ObjName(pFanin));
        }
        if ( Abc_NtkHasSop(pNtk) ) {
            printf("The SOP of this node:\n%s", ( char * ) pObj->pData);
        }
    }
}

int Lsv_CommandPrintNodes(Abc_Frame_t *pAbc, int argc, char **argv) {
    Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
    int c;
    Extra_UtilGetoptReset();
    while ( ( c = Extra_UtilGetopt(argc, argv, "h") ) != EOF ) {
        switch ( c ) {
        case 'h':
            goto usage;
        default:
            goto usage;
        }
    }
    if ( !pNtk ) {
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

void Lsv_NtkSimulateBdd(Abc_Ntk_t *pNtk, char *inputPattern) {
    int i;
    Abc_Obj_t *pNode;
    std::unordered_map <std::string, bool> PIname2Value;
    DdManager *dd = ( DdManager * ) pNtk->pManFunc;
    if ( !Abc_NtkIsBddLogic(pNtk) )
    {
        Abc_Print(-1, "BDD simulation can only be done for logic BDD networks (run \"bdd\").\n");
        return;
    }
    // printf("PIs:\n");
    Abc_NtkForEachPi(pNtk, pNode, i) {
        PIname2Value[Abc_ObjName(pNode)] = ( int ) ( inputPattern[i] - 48 );
        // printf("%s (ID=%d): %d\n", Abc_ObjName(pNode), Abc_ObjId(pNode), ( int ) ( inputPattern[i] - 48 ));
    }

    // printf("Bdd Variables Count = %d\n", Cudd_ReadSize(dd));
    // printf("POs:\n");
    Abc_NtkForEachNode(pNtk, pNode, i) {
        Abc_Obj_t *pFanin, *pFanout;
        int j;
        pFanout = Abc_ObjFanout(pNode, 0);
        // printf("  PO-%d: Id = %d, name = %s\n", i, Abc_ObjId(pFanout), Abc_ObjName(pFanout));
        printf("%s: ", Abc_ObjName(pFanout));

        DdNode *funcNode = ( DdNode * ) pNode->pData;
        Abc_ObjForEachFanin(pNode, pFanin, j) {
            bool patternValue = PIname2Value[Abc_ObjName(pFanin)];
            // printf("  Fanin-%d: Id = %d, name = %s, value = %d\n", j, Abc_ObjId(pFanin), Abc_ObjName(pFanin), patternValue);
            DdNode *curDD = Cudd_bddIthVar(dd, j);
            if ( patternValue ) {
                funcNode = Cudd_Cofactor(dd, funcNode, curDD);
            }
            else {
                funcNode = Cudd_Cofactor(dd, funcNode, Cudd_Not(curDD));
            }
        }
        if ( funcNode == Cudd_ReadLogicZero(dd) ) {
            printf("0\n");
        }
        else if ( funcNode == Cudd_ReadOne(dd) ) {
            printf("1\n");
        }
        else {
            printf("Unexpected result.\n");
        }
    }
}

int Lsv_CommandSimulateBDD(Abc_Frame_t *pAbc, int argc, char **argv) {
    Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
    int c;
    Extra_UtilGetoptReset();
    while ( ( c = Extra_UtilGetopt(argc, argv, "h") ) != EOF ) {
        switch ( c ) {
        case 'h':
            goto usage;
        default:
            goto usage;
        }
    }

    if ( !pNtk ) {
        Abc_Print(-1, "Empty network.\n");
        return 1;
    }
    Lsv_NtkSimulateBdd(pNtk, argv[1]);

    return 0;

usage:
    Abc_Print(-2, "usage: I don't know how to use it too.\n");
    return 1;
}

void Lsv_NtkSimulateAIG(Abc_Ntk_t *pNtk, char *inputFileName) {
    std::ifstream infile(inputFileName);
    if ( !infile.is_open() ) {
        Abc_Print(-1, "File not found.\n");
        return;
    }

    const unsigned int mask = 4294967295;

    int i;
    Abc_Obj_t *pNode;
    std::string line;
    int simulationCount = 0;
    // int PICount = Abc_NtkPiNum(pNtk);
    // int POCount = Abc_NtkPoNum(pNtk);
    std::unordered_map<std::string, unsigned int> NodeName2val;
    std::unordered_map<std::string, std::string> POName2val;

    Abc_NtkForEachPi(pNtk, pNode, i) {
        NodeName2val[Abc_ObjName(pNode)] = 0;
    }
    Abc_NtkForEachPo(pNtk, pNode, i) {
        POName2val[Abc_ObjName(pNode)] = "";
    }

    while ( std::getline(infile, line) ) {
        ++simulationCount;
        Abc_NtkForEachPi(pNtk, pNode, i) {
            ( NodeName2val[Abc_ObjName(pNode)] ) <<= 1;
            NodeName2val[Abc_ObjName(pNode)] += ( line[i] == '0' ) ? 0 : 1;
        }
        if ( simulationCount == 32 ) {
            // do simulation
            Abc_NtkForEachObj(pNtk, pNode, i) {
                if ( Abc_AigNodeIsConst(pNode) ) {
                    NodeName2val[Abc_ObjName(pNode)] = mask;
                }
            }

            Abc_NtkForEachNode(pNtk, pNode, i) {  // * topological order
                Abc_Obj_t *pNodeR = Abc_ObjRegular(pNode);
                unsigned int child0 = NodeName2val[Abc_ObjName(Abc_ObjFanin0(pNodeR))];
                unsigned int child1 = NodeName2val[Abc_ObjName(Abc_ObjFanin1(pNodeR))];
                if ( Abc_ObjFaninC0(pNodeR) ) {
                    child0 ^= mask;
                }
                if ( Abc_ObjFaninC1(pNodeR) ) {
                    child1 ^= mask;
                }
                unsigned int parent = child0 & child1;
                NodeName2val[Abc_ObjName(pNodeR)] = parent;
            }

            Abc_NtkForEachPo(pNtk, pNode, i) {
                Abc_Obj_t *pNodeR = Abc_ObjRegular(pNode);
                unsigned int child0 = NodeName2val[Abc_ObjName(Abc_ObjFanin0(pNodeR))];
                if ( Abc_ObjFaninC0(pNodeR) ) {
                    child0 ^= mask;
                }
                for ( int b = simulationCount - 1; b >= 0; --b ) {
                    POName2val[Abc_ObjName(pNodeR)] += ( char ) ( ( child0 >> b ) % 2 + 48 );
                }
            }
            simulationCount = 0;
        }
    }
    infile.close();

    // do simulation
    if ( simulationCount > 0 ) {
        Abc_NtkForEachObj(pNtk, pNode, i) {
            if ( Abc_AigNodeIsConst(pNode) ) {
                NodeName2val[Abc_ObjName(pNode)] = mask;
            }
        }

        Abc_NtkForEachNode(pNtk, pNode, i) {  // * topological order
            Abc_Obj_t *pNodeR = Abc_ObjRegular(pNode);
            unsigned int child0 = NodeName2val[Abc_ObjName(Abc_ObjFanin0(pNodeR))];
            unsigned int child1 = NodeName2val[Abc_ObjName(Abc_ObjFanin1(pNodeR))];
            if ( Abc_ObjFaninC0(pNodeR) ) {
                child0 ^= mask;
            }
            if ( Abc_ObjFaninC1(pNodeR) ) {
                child1 ^= mask;
            }
            unsigned int parent = child0 & child1;
            NodeName2val[Abc_ObjName(pNodeR)] = parent;
        }

        Abc_NtkForEachPo(pNtk, pNode, i) {
            Abc_Obj_t *pNodeR = Abc_ObjRegular(pNode);
            unsigned int child0 = NodeName2val[Abc_ObjName(Abc_ObjFanin0(pNodeR))];
            if ( Abc_ObjFaninC0(pNodeR) ) {
                child0 ^= mask;
            }
            for ( int b = simulationCount - 1; b >= 0; --b ) {
                POName2val[Abc_ObjName(pNodeR)] += ( char ) ( ( child0 >> b ) % 2 + 48 );
            }
        }
    }

    // print simulation result
    Abc_NtkForEachPo(pNtk, pNode, i) {
        Abc_Obj_t *pNodeR = Abc_ObjRegular(pNode);
        printf("%s: %s\n", Abc_ObjName(pNodeR), POName2val[Abc_ObjName(pNodeR)].c_str());
    }
}

int Lsv_CommandSimulateAIG(Abc_Frame_t *pAbc, int argc, char **argv) {
    Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
    int c;
    Extra_UtilGetoptReset();
    while ( ( c = Extra_UtilGetopt(argc, argv, "h") ) != EOF ) {
        switch ( c ) {
        case 'h':
            goto usage;
        default:
            goto usage;
        }
    }

    if ( !pNtk ) {
        Abc_Print(-1, "Empty network.\n");
        return 1;
    }
    Lsv_NtkSimulateAIG(pNtk, argv[1]);

    return 0;

usage:
    Abc_Print(-2, "usage: I don't know how to use it too.\n");
    return 1;
}


//================================================
//                     PA 2
//================================================

bool traverseBDD(DdManager *dd, DdNode *node, std::unordered_map<unsigned int, bool> &bddID2assignedValue) {
    int varIndex = Cudd_NodeReadIndex(node);

    if ( Cudd_IsConstant(node) ) {
        // printf("const %d\n", node == Cudd_ReadOne(dd));
        return node == Cudd_ReadOne(dd);  // Return true if terminal node is 1
    }

    // printf("var %d\n", varIndex);
    // printf("is complemented: %d\n", Cudd_IsComplement(node));

    // Try the high child first.
    bddID2assignedValue[varIndex] = ( Cudd_IsComplement(node) ) ? false : true;

    if ( traverseBDD(dd, Cudd_T(node), bddID2assignedValue) ) {
        return true;
    }

    // If the high child did not work, try the low child.
    bddID2assignedValue[varIndex] = ( Cudd_IsComplement(node) ) ? true : false;

    if ( traverseBDD(dd, Cudd_E(node), bddID2assignedValue) ) {
        return true;
    }
    return false;
}

void write_dd(DdManager *gbm, DdNode *dd, char *filename)
{
    FILE *outfile; // output file pointer for .dot file
    outfile = fopen(filename, "w");
    DdNode **ddnodearray = ( DdNode ** ) malloc(sizeof(DdNode *)); // initialize the function array
    ddnodearray[0] = dd;
    Cudd_DumpDot(gbm, 1, ddnodearray, NULL, NULL, outfile); // dump the function to .dot file
    free(ddnodearray);
    fclose(outfile); // close the file */
}

void Lsv_CheckSymmetryBDD(Abc_Ntk_t *pNtk, int yk, int xi_pos, int xj_pos) {
    int i;
    Abc_Obj_t *pNode, *pFanin;
    DdManager *dd = ( DdManager * ) pNtk->pManFunc;
    Abc_NtkForEachPo(pNtk, pNode, i) {
        // printf("PO-%d: Id = %d, name = %s\n", i, Abc_ObjId(pNode), Abc_ObjName(pNode));
    }
    pNode = Abc_ObjFanin0(Abc_NtkPo(pNtk, yk));
    int PINum = Abc_NtkPiNum(pNtk);
    std::vector<int> initPos2bddPos(PINum, -1);
    std::vector<int> bddPos2initPos(PINum, -1);
    std::string initPos2PIName[PINum];
    std::unordered_map<std::string, int> PIName2initPos;
    std::unordered_map<std::string, int> PIName2bddPos;

    Abc_NtkForEachPi(pNtk, pFanin, i) {
        initPos2PIName[i] = Abc_ObjName(pFanin);
        PIName2initPos[Abc_ObjName(pFanin)] = i;
        // printf("PI-%d: Id = %d, name = %s\n", i, Abc_ObjId(pFanin), Abc_ObjName(pFanin));
    }

    Abc_ObjForEachFanin(pNode, pFanin, i) {
        PIName2bddPos[Abc_ObjName(pFanin)] = i;
        char *PIName = Abc_ObjName(pFanin);
        int bddPos = i;
        int initPos = PIName2initPos[PIName];
        initPos2bddPos[initPos] = bddPos;
        bddPos2initPos[bddPos] = initPos;
        // printf("Fanin-%d: Id = %d, name = %s\n", i, Abc_ObjId(pFanin), Abc_ObjName(pFanin));
    }

    int xi = initPos2bddPos[xi_pos];
    int xj = initPos2bddPos[xj_pos];

    // print initPos2bddPos
    // for ( int i = 0; i < PINum; ++i ) {
    //     printf("%d: %d\n", i, initPos2bddPos[i]);
    // }


    // check symmetry
    DdNode *funcNode = ( DdNode * ) pNode->pData;
    Cudd_Ref(funcNode);
    DdNode *xiNode;
    DdNode *xjNode;
    DdNode *funcNodeCofactor1;
    DdNode *funcNodeCofactor2;

    bool is_symmetric = false;
    if ( xi != -1 && xj != -1 ) {
        xiNode = Cudd_bddIthVar(dd, xi);
        xjNode = Cudd_bddIthVar(dd, xj);
        Cudd_Ref(xiNode);
        Cudd_Ref(xjNode);
        funcNodeCofactor1 = Cudd_Cofactor(dd, funcNode, xiNode);
        funcNodeCofactor1 = Cudd_Cofactor(dd, funcNodeCofactor1, Cudd_Not(xjNode));
        funcNodeCofactor2 = Cudd_Cofactor(dd, funcNode, Cudd_Not(xiNode));
        funcNodeCofactor2 = Cudd_Cofactor(dd, funcNodeCofactor2, xjNode);
        Cudd_RecursiveDeref(dd, xiNode);
        Cudd_RecursiveDeref(dd, xjNode);
        Cudd_Ref(funcNodeCofactor1);
        Cudd_Ref(funcNodeCofactor2);
        is_symmetric = ( funcNodeCofactor1 == funcNodeCofactor2 );
    }
    else if ( xi == -1 && xj != -1 ) {
        xjNode = Cudd_bddIthVar(dd, xj);
        Cudd_Ref(xjNode);
        funcNodeCofactor1 = Cudd_Cofactor(dd, funcNode, xjNode);
        funcNodeCofactor2 = Cudd_Cofactor(dd, funcNode, Cudd_Not(xjNode));
        Cudd_RecursiveDeref(dd, xjNode);
        Cudd_Ref(funcNodeCofactor1);
        Cudd_Ref(funcNodeCofactor2);
        is_symmetric = ( funcNodeCofactor1 == funcNodeCofactor2 );
    }
    else if ( xi != -1 && xj == -1 ) {
        xiNode = Cudd_bddIthVar(dd, xi);
        Cudd_Ref(xiNode);
        funcNodeCofactor1 = Cudd_Cofactor(dd, funcNode, xiNode);
        funcNodeCofactor2 = Cudd_Cofactor(dd, funcNode, Cudd_Not(xiNode));
        Cudd_RecursiveDeref(dd, xiNode);
        Cudd_Ref(funcNodeCofactor1);
        Cudd_Ref(funcNodeCofactor2);
        is_symmetric = ( funcNodeCofactor1 == funcNodeCofactor2 );
    }
    else {
        is_symmetric = true;
    }

    Cudd_RecursiveDeref(dd, funcNode);

    if ( is_symmetric ) {
        printf("symmetric\n");
        if ( xi != -1 || xj != -1 ) {
            Cudd_RecursiveDeref(dd, funcNodeCofactor1);
            Cudd_RecursiveDeref(dd, funcNodeCofactor2);
        }
        return;
    }
    else {
        printf("asymmetric\n");
    }

    // find counterexample
    std::unordered_map<unsigned int, bool> bddID2assignedValue;

    DdNode *diffNode = Cudd_bddXor(dd, funcNodeCofactor1, funcNodeCofactor2);

    Cudd_RecursiveDeref(dd, funcNodeCofactor1);
    Cudd_RecursiveDeref(dd, funcNodeCofactor2);
    Cudd_Ref(diffNode);

    if ( traverseBDD(dd, diffNode, bddID2assignedValue) ) {
        // printf("find counterexample\n");
        // print bddID2assignedValue
        // for ( std::unordered_map<unsigned int, bool>::iterator it = bddID2assignedValue.begin(); it != bddID2assignedValue.end(); ++it ) {
        //     printf("%d: %d\n", it->first, it->second);
        // }
        std::string counterexampleBDD1(PINum, '0');
        std::string counterexampleBDD2(PINum, '0');
        std::string counterexample1(PINum, '0');
        std::string counterexample2(PINum, '0');

        for ( std::unordered_map<unsigned int, bool>::iterator it = bddID2assignedValue.begin(); it != bddID2assignedValue.end(); ++it ) {
            if ( it->second ) {
                counterexampleBDD1[it->first] = '1';
                counterexampleBDD2[it->first] = '1';
            }
        }


        for ( int pos = 0; pos < PINum; ++pos ) {
            if ( pos == xi_pos ) {
                counterexample1[pos] = '1';
                counterexample2[pos] = '0';
            }
            else if ( pos == xj_pos ) {
                counterexample1[pos] = '0';
                counterexample2[pos] = '1';
            }
            else {
                int bddPos = initPos2bddPos[pos];
                if ( bddPos == -1 ) {
                    counterexample1[pos] = '0';
                    counterexample2[pos] = '0';
                }
                else {
                    counterexample1[pos] = counterexampleBDD1[bddPos];
                    counterexample2[pos] = counterexampleBDD2[bddPos];
                }
            }
        }
        printf("%s\n", counterexample1.c_str());
        printf("%s\n", counterexample2.c_str());
    }
    else {
        printf("[ERROR] no counterexample\n");
    }

    // draw BDD node
    // DdNode *add = Cudd_BddToAdd(dd, diffNode);
    // Cudd_Ref(add);
    // write_dd(dd, add, "diff.dot");

    Cudd_RecursiveDeref(dd, diffNode);
}

int Lsv_CommandCheckSymmetryBDD(Abc_Frame_t *pAbc, int argc, char **argv) {
    Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
    int c;
    int yk = 0, xi = 0, xj = 0;
    int i;
    Extra_UtilGetoptReset();
    while ( ( c = Extra_UtilGetopt(argc, argv, "h") ) != EOF ) {
        switch ( c ) {
        case 'h':
            goto usage;
        default:
            goto usage;
        }
    }
    if ( !pNtk ) {
        Abc_Print(-1, "Empty network.\n");
        return 1;
    }
    if ( argc != 4 ) {
        Abc_Print(-1, "Wrong argument format.\n");
        return 1;
    }

    // extract yk, xi, xj from arguments
    i = 0;
    while ( argv[1][i] != '\0' ) {
        yk = yk * 10 + ( argv[1][i] - 48 );
        ++i;
    }
    i = 0;
    while ( argv[2][i] != '\0' ) {
        xi = xi * 10 + ( argv[2][i] - 48 );
        ++i;
    }
    i = 0;
    while ( argv[3][i] != '\0' ) {
        xj = xj * 10 + ( argv[3][i] - 48 );
        ++i;
    }
    Lsv_CheckSymmetryBDD(pNtk, yk, xi, xj);
    return 0;

usage:
    Abc_Print(-2, "usage: lsv_sym_bdd [-h]\n");
    Abc_Print(-2, "\t        check bdd is symmetric or not\n");
    Abc_Print(-2, "\t-h    : print the command usage\n");
    return 1;
}

void printAIG(Aig_Man_t *pMan) {
    int i;
    Aig_Obj_t *pObj;

    printf("Number of Nodes: %d\n", Aig_ManObjNum(pMan));

    Aig_ManForEachObj(pMan, pObj, i) {
        // Print information for each node, e.g., pObj->Type, fanins, etc.
        printf("Node-%d: ID = %d\n", i, pObj->Id);
        printf("PI: %d, PO: %d\n", Aig_ObjIsCi(pObj), Aig_ObjIsCo(pObj));
        if ( Aig_ObjFanin0(pObj) ) {
            printf("Fanin0: ID = %d\n", Aig_ObjFanin0(pObj)->Id);
        }
        if ( Aig_ObjFanin1(pObj) ) {
            printf("Fanin1: ID = %d\n", Aig_ObjFanin1(pObj)->Id);
        }
        printf("\n");
    }
}

void printCNF(Cnf_Dat_t *pCnf) {
    printf("Number of variables: %d\n", pCnf->nVars);
    for ( int i = 0; i < pCnf->nClauses; i++ ) {
        int *pLit, *pStop;
        for ( pLit = pCnf->pClauses[i], pStop = pCnf->pClauses[i + 1]; pLit < pStop; pLit++ ) {
            printf("%d ", ( *pLit & 1 ) ? -( *pLit >> 1 ) : ( *pLit >> 1 ));
        }
        printf("\n");
    }
}

void Lsv_CheckSymmetrySAT(Abc_Ntk_t *pNtk, int yk, int xi_pos, int xj_pos) {
    int i;
    Abc_Obj_t *pObj;
    // printf("%s\n", Abc_ObjName(Abc_NtkPo(pNtk, yk)));
    Abc_Ntk_t *pNtkTar = Abc_NtkCreateCone(pNtk, Abc_ObjFanin0(Abc_NtkPo(pNtk, yk)), Abc_ObjName(Abc_NtkPo(pNtk, yk)), 0);

    char *xiName = Abc_ObjName(Abc_NtkPi(pNtk, xi_pos));
    char *xjName = Abc_ObjName(Abc_NtkPi(pNtk, xj_pos));

    Abc_Obj_t *pXi = nullptr;
    Abc_Obj_t *pXj = nullptr;
    Abc_Obj_t *pYk = Abc_ObjFanin0(Abc_NtkPo(pNtkTar, 0));

    Abc_NtkForEachPi(pNtkTar, pObj, i) {
        if ( strcmp(Abc_ObjName(pObj), xiName) == 0 ) {
            pXi = pObj;
        }
        else if ( strcmp(Abc_ObjName(pObj), xjName) == 0 ) {
            pXj = pObj;
        }
    }

    if ( pXi == nullptr && pXj == nullptr ) {
        printf("symmetric\n");
        return;
    }

    Aig_Man_t *pMan = Abc_NtkToDar(pNtkTar, 0, 0);

    Cnf_Dat_t *pCnf = Cnf_Derive(pMan, 1);

    Cnf_Dat_t *pCnf2 = Cnf_Derive(pMan, 1);
    Cnf_DataLift(pCnf2, pCnf->nVars);

    sat_solver *pSat = sat_solver_new();
    Cnf_DataWriteIntoSolverInt(pSat, pCnf, 1, 0);
    Cnf_DataWriteIntoSolverInt(pSat, pCnf2, 1, 0);

    // pSat->fPrintClause = 1;

    //  (a == b) => (a + b') * (a' + b)
    Abc_NtkForEachPi(pNtkTar, pObj, i) {
        int Var1, Var2;

        if ( pObj == pXi ) {
            if ( pXj == nullptr ) {
                continue;
            }
            Var1 = pCnf->pVarNums[pXi->Id];
            Var2 = pCnf->pVarNums[pXj->Id] + pCnf->nVars;
        }
        else if ( pObj == pXj ) {
            if ( pXi == nullptr ) {
                continue;
            }
            Var1 = pCnf->pVarNums[pXj->Id];
            Var2 = pCnf->pVarNums[pXi->Id] + pCnf->nVars;
        }
        else {
            Var1 = pCnf->pVarNums[pObj->Id];
            Var2 = Var1 + pCnf->nVars;
        }

        lit Lits[2];

        Lits[0] = toLitCond(Var1, 1);
        Lits[1] = toLitCond(Var2, 0);
        sat_solver_addclause(pSat, Lits, Lits + 2);

        Lits[0] = toLitCond(Var1, 0);
        Lits[1] = toLitCond(Var2, 1);
        sat_solver_addclause(pSat, Lits, Lits + 2);
    }

    // handle xi or xj does not exist
    if ( pXi == nullptr ) {
        int Var1 = pCnf->pVarNums[pXj->Id];
        int Var2 = Var1 + pCnf->nVars;
        printf("Xi not included\n");
        lit Lit;

        Lit = toLitCond(Var1, 0);
        sat_solver_addclause(pSat, &Lit, &Lit + 1);

        Lit = toLitCond(Var2, 1);
        sat_solver_addclause(pSat, &Lit, &Lit + 1);
    }
    else if ( pXj == nullptr ) {
        int Var1 = pCnf->pVarNums[pXi->Id];
        int Var2 = Var1 + pCnf->nVars;
        printf("Xj not included\n");
        lit Lit;

        Lit = toLitCond(Var1, 0);
        sat_solver_addclause(pSat, &Lit, &Lit + 1);

        Lit = toLitCond(Var2, 1);
        sat_solver_addclause(pSat, &Lit, &Lit + 1);
    }

    // Var(y1) xor Var(y2)
    int VarY1 = pCnf->pVarNums[pYk->Id];
    int VarY2 = VarY1 + pCnf->nVars;
    int xorVar = sat_solver_addvar(pSat);
    sat_solver_add_xor(pSat, xorVar, VarY1, VarY2, 0);
    lit xorLit = toLitCond(xorVar, 0);
    sat_solver_addclause(pSat, &xorLit, &xorLit + 1);

    // run SAT solver
    int status = sat_solver_solve(pSat, NULL, NULL, 0, 0, 0, 0);

    if ( status == l_True ) {
        printf("asymmetric\n");
    }
    else if ( status == l_False ) {
        printf("symmetric\n");
        return;
    }

    std::string counterexample1(Abc_NtkPiNum(pNtk), '0');
    std::string counterexample2(Abc_NtkPiNum(pNtk), '0');

    int tarNtk_i = 0;
    Abc_NtkForEachPi(pNtk, pObj, i) {
        // printf("%d %d\n", i, tarNtk_i);
        if ( tarNtk_i >= Abc_NtkPiNum(pNtkTar) ) {
            if ( i == xi_pos ) {
                counterexample1[i] = '1';
                counterexample2[i] = '0';
            }
            else if ( i == xj_pos ) {
                counterexample1[i] = '0';
                counterexample2[i] = '1';
            }
            continue;
        }

        Abc_Obj_t *pObjTar = Abc_NtkPi(pNtkTar, tarNtk_i);
        if ( i == xi_pos ) {
            counterexample1[i] = '1';
            counterexample2[i] = '0';
            if ( strcmp(Abc_ObjName(pObj), Abc_ObjName(pObjTar)) == 0 ) {
                ++tarNtk_i;
            }
        }
        else if ( i == xj_pos ) {
            counterexample1[i] = '0';
            counterexample2[i] = '1';
            if ( strcmp(Abc_ObjName(pObj), Abc_ObjName(pObjTar)) == 0 ) {
                ++tarNtk_i;
            }
        }
        else if ( strcmp(Abc_ObjName(pObj), Abc_ObjName(pObjTar)) == 0 ) {
            int SAT_ID = pCnf->pVarNums[pObjTar->Id];
            counterexample1[i] = '0' + sat_solver_var_value(pSat, SAT_ID);
            counterexample2[i] = '0' + sat_solver_var_value(pSat, SAT_ID);
            ++tarNtk_i;
        }
    }

    printf("%s\n", counterexample1.c_str());
    printf("%s\n", counterexample2.c_str());

    // Abc_NtkForEachObj(pNtkTar, pObj, i) {
    //     printf("Obj-%d: SAT ID = %d, name = %s\n", i, pCnf->pVarNums[pObj->Id], Abc_ObjName(pObj));
    // }

    // for ( int i = 0; i < sat_solver_nvars(pSat); ++i ) {
    //     printf("%d: %d\n", i, sat_solver_var_value(pSat, i));
    // }

    // Abc_NtkShow(pNtkTar, 0, 0, 1, 0);
    // printf("Number of variables: %d\n", sat_solver_nvars(pSat));
    // printf("Number of clauses: %d\n", sat_solver_nclauses(pSat));

    // printf("%s\n", pMan->pName);
}

int Lsv_CommandCheckSymmetrySAT(Abc_Frame_t *pAbc, int argc, char **argv) {
    Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
    int c;
    int yk = 0, xi = 0, xj = 0;
    int i;
    Extra_UtilGetoptReset();
    while ( ( c = Extra_UtilGetopt(argc, argv, "h") ) != EOF ) {
        switch ( c ) {
        case 'h':
            goto usage;
        default:
            goto usage;
        }
    }
    if ( !pNtk ) {
        Abc_Print(-1, "Empty network.\n");
        return 1;
    }
    if ( argc != 4 ) {
        Abc_Print(-1, "Wrong argument format.\n");
        return 1;
    }

    // extract yk, xi, xj from arguments
    i = 0;
    while ( argv[1][i] != '\0' ) {
        yk = yk * 10 + ( argv[1][i] - 48 );
        ++i;
    }
    i = 0;
    while ( argv[2][i] != '\0' ) {
        xi = xi * 10 + ( argv[2][i] - 48 );
        ++i;
    }
    i = 0;
    while ( argv[3][i] != '\0' ) {
        xj = xj * 10 + ( argv[3][i] - 48 );
        ++i;
    }
    Lsv_CheckSymmetrySAT(pNtk, yk, xi, xj);
    return 0;

usage:
    Abc_Print(-2, "usage: lsv_sym_sat [-h]\n");
    Abc_Print(-2, "\t        check aig is symmetric or not\n");
    Abc_Print(-2, "\t-h    : print the command usage\n");
    return 1;
}