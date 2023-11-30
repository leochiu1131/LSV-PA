#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "bdd/cudd/cudd.h"
#include "sat/cnf/cnf.h"
#include "aig/gia/giaAig.h"
#include "sat/bsat/satSolver.h"
#include "base/io/ioAbc.h"
extern "C"{
/*=== base/abci/abcDar.c ==============================================*/
extern Aig_Man_t * Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
extern Abc_Ntk_t * Abc_NtkDC2( Abc_Ntk_t * pNtk, int fBalance, int fUpdateLevel, int fFanout, int fPower, int fVerbose );

/*=== aig/gia/giaAig.c ================================================*/
extern Gia_Man_t * Gia_ManFromAig( Aig_Man_t * p );
}

#include <string>
#include <fstream>
#include <unordered_map>
#include <algorithm>

#define S_ONE  (~(0))
#define S_ZERO (0)

static int Lsv_CommandPrintNodes(Abc_Frame_t *pAbc, int argc, char **argv);
static int Lsv_CommandSimBdd(Abc_Frame_t *pAbc, int argc, char **argv);
static int Lsv_CommandSimAig(Abc_Frame_t *pAbc, int argc, char **argv);
static int Lsv_CommandSymBdd(Abc_Frame_t *pAbc, int argc, char **argv);
static int Lsv_CommandSymSat(Abc_Frame_t *pAbc, int argc, char **argv);
static int Lsv_CommandSymSatAll(Abc_Frame_t *pAbc, int argc, char **argv);

void init(Abc_Frame_t *pAbc) {
    Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", Lsv_CommandSimBdd, 0);
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig", Lsv_CommandSimAig, 0);
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_bdd", Lsv_CommandSymBdd, 0);
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_sat", Lsv_CommandSymSat, 0);
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_all", Lsv_CommandSymSatAll, 0);
}

void destroy(Abc_Frame_t *pAbc) {
}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
    PackageRegistrationManager() {
        Abc_FrameAddInitializer(&frame_initializer);
    }
} lsvPackageRegistrationManager;

static Abc_Obj_t *Abc_NtkCreateMultiplexer(Abc_Aig_t *pMan, std::vector<Abc_Obj_t *> &pControl, std::vector<Abc_Obj_t *> &pSignal) {
    int j;
    std::vector<Abc_Obj_t *> prevSignal = pSignal;
    std::vector<Abc_Obj_t *> currSignal;

    for (int i = 0; i < pControl.size(); ++i) {
        Abc_Obj_t *pC = pControl[i];
        for (j = 0; j < prevSignal.size(); j += 2) {
            Abc_Obj_t *p0 = prevSignal[j];
            Abc_Obj_t *p1 = ((j + 1) == prevSignal.size()) ? prevSignal[j] : prevSignal[j + 1];
            currSignal.push_back(Abc_AigMux(pMan, pC, p1, p0));
        }
        prevSignal = currSignal;
        currSignal.clear();
    }

    return prevSignal[0];
}

static void Abc_NtkMiterAddOne( Abc_Ntk_t * pNtk, Abc_Ntk_t * pNtkMiter ) {
    Abc_Obj_t * pNode;
    int i;
    assert( Abc_NtkIsDfsOrdered(pNtk) );
    Abc_AigForEachAnd( pNtk, pNode, i ) {
        pNode->pCopy = Abc_AigAnd( (Abc_Aig_t *)pNtkMiter->pManFunc, Abc_ObjChild0Copy(pNode), Abc_ObjChild1Copy(pNode) );
    }
}

Abc_Ntk_t *Lsv_NtkSymMiterAll(Abc_Ntk_t *pNtk, int pk) {
    char Buffer[1000];
    Abc_Ntk_t *pNtk1 = pNtk;
    Abc_Ntk_t *pNtk2 = Abc_NtkDup(pNtk);
    Abc_Ntk_t *pNtkMiter;
    pNtkMiter = Abc_NtkAlloc(ABC_NTK_STRASH, ABC_FUNC_AIG, 1);

    Abc_NtkSetName(pNtkMiter, "miter");
    Abc_Aig_t *pMan = (Abc_Aig_t *)pNtkMiter->pManFunc;
    int i, j;
    Abc_Obj_t *pObj, *pObjTemp, *pObjNew;

    Abc_AigConst1(pNtk1)->pCopy = Abc_AigConst1(pNtkMiter);
    Abc_AigConst1(pNtk2)->pCopy = Abc_AigConst1(pNtkMiter);

    // Control Signal
    int nControlPi = (int)(std::ceil(std::log2((Abc_NtkPiNum(pNtk1) + 1))));
    std::vector<std::vector<Abc_Obj_t *> > controlPis;
    Abc_NtkForEachPi(pNtk2, pObj, i) {
        std::vector<Abc_Obj_t *> controlPi;
        for (int b = 0; b < nControlPi; ++b) {
            pObjNew = Abc_NtkCreatePi(pNtkMiter);
            sprintf(Buffer, "%d_%d", i, b);
            Abc_ObjAssignName(pObjNew, Buffer, 0);
            controlPi.push_back(pObjNew);
        }
        controlPis.emplace_back(controlPi);
    }

    // Ntk1 PI
    std::vector<Abc_Obj_t *> Ntk1_Pis;
    Abc_NtkForEachPi(pNtk1, pObj, i) {
        pObjNew = Abc_NtkCreatePi(pNtkMiter);
        pObj->pCopy = pObjNew;
        Ntk1_Pis.push_back(pObjNew);
    }

    Abc_NtkForEachPi(pNtk2, pObj, i) {
        pObj->pCopy = Abc_NtkCreateMultiplexer(pMan, controlPis[i], Ntk1_Pis);
    }

    Abc_NtkMiterAddOne(pNtk1, pNtkMiter);
    Abc_NtkMiterAddOne(pNtk2, pNtkMiter);
    
    Abc_Obj_t *pOut1 = Abc_ObjChild0Copy(Abc_NtkPo(pNtk1, pk));
    Abc_Obj_t *pOut2 = Abc_ObjChild0Copy(Abc_NtkPo(pNtk2, pk));
    // PO
    pObjNew = Abc_NtkCreatePo(pNtkMiter);
    Abc_ObjAssignName(pObjNew, "miter", Abc_ObjName(pObjNew));
    Abc_ObjAddFanin(Abc_NtkPo(pNtkMiter, 0), Abc_AigXor(pMan, pOut1, pOut2));
    
    // Cleanup
    Abc_AigCleanup((Abc_Aig_t *)pNtkMiter->pManFunc);

    // DC2 (Here ML)
    pNtkMiter = Abc_NtkDC2(pNtkMiter, 0, 0, 1, 0, 0);

    // Cleanup
    Abc_AigCleanup((Abc_Aig_t *)pNtkMiter->pManFunc);
    Abc_NtkDelete(pNtk2);

    return pNtkMiter;
}

void Lsv_SatAllEncode(int offset, int nPi, int nBit, int i, int j, int *controls) {
    for (int pi = 0; pi < nPi; ++pi) {
        int encode_idx = (pi == i) ? j : (pi == j) ? i : pi;

        for (int code = encode_idx, b = 0; b < nBit; ++b, code >>= 1) {
            controls[pi*nBit + b] = toLitCond(pi*nBit + b + offset, !(code & 1));
        }
    }
}

void Lsv_NtkSymSatAll(Abc_Ntk_t *pNtk, int k) {
    if (k < 0 || k >= Abc_NtkPoNum(pNtk)) {
        Abc_Print(1, "Index out of range!\n");
        return;
    }
    Abc_Ntk_t *pNtkMiter = Lsv_NtkSymMiterAll(pNtk, k);
    Aig_Man_t *pAig = Abc_NtkToDar(pNtkMiter, 0, 0);
    Gia_Man_t *pGia = Gia_ManFromAig(pAig);

    Cnf_Dat_t * pCnf;
    pCnf = (Cnf_Dat_t *)Mf_ManGenerateCnf( pGia, 8, 0, 1, 0, 0 );
    sat_solver *solver = (sat_solver *)Cnf_DataWriteIntoSolver( pCnf, 1, 0 );
    int offset = pCnf->pVarNums[Abc_NtkPi(pNtkMiter, 0)->Id];

    if (solver) {
        int nControlPi = (int)(std::ceil(std::log2((Abc_NtkPiNum(pNtk) + 1))));
        int *controls = ABC_ALLOC(int, Abc_NtkPiNum(pNtk) * nControlPi);

        for (int i = 0; i < Abc_NtkPiNum(pNtk)-1; ++i) {
            for (int j = i+1; j < Abc_NtkPiNum(pNtk); ++j) {
                // TODO: skip trivial case

                // encode
                Lsv_SatAllEncode(offset, Abc_NtkPiNum(pNtk), nControlPi, i, j, controls);
            
                int status = sat_solver_solve(solver, controls, controls + Abc_NtkPiNum(pNtk) * nControlPi, 0, 0, 0, 0);
                if (status == l_True) {
                    // printf("asymmetric\n");
                } else if (status == l_False) {
                    Abc_Print(1, "%d %d\n", i, j);
                }
            }
        }
        ABC_FREE(controls);
    } else {
        Abc_Print(1, "symmetric\n");
    }

    Gia_ManStop(pGia);
    Aig_ManStop(pAig);
    Abc_NtkDelete(pNtkMiter);
}

int Lsv_CommandSymSatAll(Abc_Frame_t *pAbc, int argc, char **argv) {
    Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
    Vec_Ptr_t *pModel = NULL;
    int c, fGlobal = 0, fVerbose = 0, nArgc;
    char **nArgv;
    // set defaults
    Extra_UtilGetoptReset();
    while ((c = Extra_UtilGetopt(argc, argv, "vh")) != EOF) {
        switch (c) {
        case 'v':
            fVerbose ^= 1;
            break;
        case 'h':
            goto usage;
        default:
            goto usage;
        }
    }

    nArgc = argc - globalUtilOptind;
    nArgv = argv + globalUtilOptind;

    if (nArgc != 1) {
        goto usage;
    }

    if (pNtk == NULL) {
        Abc_Print(-1, "Empty network.\n");
        return 1;
    }

    if (!Abc_NtkIsStrash(pNtk)) {
        Abc_Print(-1, "Cannot lsv_sym_aig a network that is not an AIG (run \"strash\").\n");
        return 1;
    }

    Lsv_NtkSymSatAll(pNtk, atoi(nArgv[0]));

	return 0;
usage:
    Abc_Print(-2, "usage: lsv_sym_all [-h] <k>\n");
    Abc_Print(-2, "\t        symmetry checking with output k\n");
    Abc_Print(-2, "\t-h    : print the command usage\n");
    return 1;
}

Abc_Ntk_t *Lsv_NtkSymMiter(Abc_Ntk_t *pNtk, int pk, int pi, int pj) {
    char Buffer[1000];
    Abc_Ntk_t *pNtk1 = pNtk;
    Abc_Ntk_t *pNtk2 = Abc_NtkDup(pNtk);
    Abc_Ntk_t *pNtkMiter;
    pNtkMiter = Abc_NtkAlloc(ABC_NTK_STRASH, ABC_FUNC_AIG, 1);

    Abc_NtkSetName(pNtkMiter, "miter");
    Abc_Aig_t *pMan = (Abc_Aig_t *)pNtkMiter->pManFunc;
    int i;
    Abc_Obj_t *pObj, *pObjTemp, *pObjNew;

    Abc_AigConst1(pNtk1)->pCopy = Abc_AigConst1(pNtkMiter);
    Abc_AigConst1(pNtk2)->pCopy = Abc_AigConst1(pNtkMiter);
    Abc_NtkForEachPi(pNtk1, pObj, i) {
        pObjNew = Abc_NtkCreatePi(pNtkMiter);
        pObj->pCopy = pObjNew;
    }

    Abc_NtkForEachPi(pNtk2, pObj, i) {
        int index = (i == pi) ? pj : (i == pj) ? pi : i;
        pObj->pCopy = Abc_NtkPi(pNtk1, index)->pCopy;
    }

    Abc_NtkMiterAddOne(pNtk1, pNtkMiter);
    Abc_NtkMiterAddOne(pNtk2, pNtkMiter);
    
    Abc_Obj_t *pOut1 = Abc_ObjChild0Copy(Abc_NtkPo(pNtk1, pk));
    Abc_Obj_t *pOut2 = Abc_ObjChild0Copy(Abc_NtkPo(pNtk2, pk));
    // PO
    pObjNew = Abc_NtkCreatePo(pNtkMiter);
    Abc_ObjAssignName(pObjNew, "miter", Abc_ObjName(pObjNew));
    Abc_ObjAddFanin(Abc_NtkPo(pNtkMiter, 0), Abc_AigXor(pMan, pOut1, pOut2));
    
    // Cleanup
    Abc_AigCleanup((Abc_Aig_t *)pNtkMiter->pManFunc);

    // DC2 (Here ML)
    pNtkMiter = Abc_NtkDC2(pNtkMiter, 0, 0, 1, 0, 0);

    // Cleanup
    Abc_AigCleanup((Abc_Aig_t *)pNtkMiter->pManFunc);
    Abc_NtkDelete(pNtk2);

    return pNtkMiter;
}

void Lsv_NtkSymSat(Abc_Ntk_t *pNtk, int pk, int pi, int pj) {
    if (pi < 0 || pi >= Abc_NtkPiNum(pNtk) || pj < 0 || pj >= Abc_NtkPiNum(pNtk) || pk < 0 || pk >= Abc_NtkPoNum(pNtk)) {
        Abc_Print(1, "Index out of range!\n");
        return;
    }

    Abc_Ntk_t *pNtkMiter = Lsv_NtkSymMiter(pNtk, pk, pi, pj);
    Aig_Man_t *pAig = Abc_NtkToDar(pNtkMiter, 0, 0);
    Gia_Man_t *pGia = Gia_ManFromAig(pAig);

    Cnf_Dat_t * pCnf;
    pCnf = (Cnf_Dat_t *)Mf_ManGenerateCnf( pGia, 8, 0, 1, 0, 0 );
    sat_solver *solver = (sat_solver *)Cnf_DataWriteIntoSolver( pCnf, 1, 0 );
    int offset = pCnf->pVarNums[Abc_NtkPi(pNtkMiter, 0)->Id];

    if (solver) {
        int status = sat_solver_solve(solver, 0, 0, 0, 0, 0, 0);
        if (status == l_True) {
            printf("asymmetric\n");
            std::vector<int> counter_example;
            for (int i = 0; i < Abc_NtkPiNum(pNtk); ++i) {
                counter_example.push_back(sat_solver_var_value(solver, i+offset));
            }
            for (int i = 0; i < Abc_NtkPiNum(pNtk); ++i) {
                Abc_Print(1, "%d", counter_example[i]);
            }
            Abc_Print(1, "\n");
            std::swap(counter_example[pi], counter_example[pj]);
            for (int i = 0; i < Abc_NtkPiNum(pNtk); ++i) {
                Abc_Print(1, "%d", counter_example[i]);
            }
            Abc_Print(1, "\n");
        } else if (status == l_False) {
            Abc_Print(1, "symmetric\n");
        }
        sat_solver_delete(solver);
    } else {
        Abc_Print(1, "symmetric\n");
    }

    Gia_ManStop(pGia);
    Aig_ManStop(pAig);
    Abc_NtkDelete(pNtkMiter);
}

int Lsv_CommandSymSat(Abc_Frame_t *pAbc, int argc, char **argv) {
    Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
    Vec_Ptr_t *pModel = NULL;
    int c, fGlobal = 0, fVerbose = 0, nArgc;
    char **nArgv;
    // set defaults
    Extra_UtilGetoptReset();
    while ((c = Extra_UtilGetopt(argc, argv, "vh")) != EOF) {
        switch (c) {
        case 'v':
            fVerbose ^= 1;
            break;
        case 'h':
            goto usage;
        default:
            goto usage;
        }
    }

    nArgc = argc - globalUtilOptind;
    nArgv = argv + globalUtilOptind;

    if (nArgc != 3) {
        goto usage;
    }

    if (pNtk == NULL) {
        Abc_Print(-1, "Empty network.\n");
        return 1;
    }

    if (!Abc_NtkIsStrash(pNtk)) {
        Abc_Print(-1, "Cannot lsv_sym_aig a network that is not an AIG (run \"strash\").\n");
        return 1;
    }

    Lsv_NtkSymSat(pNtk, atoi(nArgv[0]), atoi(nArgv[1]), atoi(nArgv[2]));

	return 0;
usage:
    Abc_Print(-2, "usage: lsv_sym_sat [-h] <k> <i> <j>\n");
    Abc_Print(-2, "\t        symmetry checking with output k and input i and j using sat\n");
    Abc_Print(-2, "\t-h    : print the command usage\n");
    return 1;
}

typedef struct Path {
    int Id;
    int T;
} Path;

int Lsv_NtkBddPath2One_rec(DdNode *target1, DdNode *target2, DdNode *root, st__table * visited, std::vector<Path> &path) {
    DdNode *my_root = Cudd_Regular(root);
    int complement = Cudd_IsComplement(root);

    if (root == target1 || root == target2) {
        // printf("Get Const 1!\n");
        return 1;
    } else if (root == Cudd_Not(target1) || root == Cudd_Not(target2)) {
        return 0;
    } else {
        DdNode *T = Cudd_T(my_root);
        DdNode *E = Cudd_E(my_root);

        if (Lsv_NtkBddPath2One_rec(target1, target2, T, visited, path)) {
            // printf("%d -> T\n", my_root->Id);
            Path p;
            p.Id = my_root->Id;
            p.T = 1;
            path.push_back(p);
            return 1;
        }
        if (Lsv_NtkBddPath2One_rec(target1, target2, E, visited, path)) {
            // printf("%d -> E\n", my_root->Id);
            Path p;
            p.Id = my_root->Id;
            p.T = 0;
            path.push_back(p);
            return 1;
        }
        return 0;
    }
    return 0;
}

std::vector<Path> Lsv_NtkBddPath2One(DdManager *dd, DdNode *root) {
    st__table *visited = st__init_table( st__ptrcmp, st__ptrhash );
    DdNode *target1 = Cudd_IsComplement(root) ? DD_ZERO(dd) : DD_ONE(dd);
    DdNode *target2 = Cudd_IsComplement(root) ? Cudd_Not(DD_ONE(dd)) : Cudd_Not(DD_ZERO(dd));
    std::vector<Path> path;
    Lsv_NtkBddPath2One_rec(target1, target2, root, visited, path);
    st__free_table(visited);

    return path;
}

void Lsv_NtkSymBdd(Abc_Ntk_t *pNtk, int k, int i, int j) {
    if (i < 0 || i >= Abc_NtkPiNum(pNtk) || j < 0 || j >= Abc_NtkPiNum(pNtk) || k < 0 || k >= Abc_NtkPoNum(pNtk)) {
        Abc_Print(1, "Index out of range!\n");
        return;
    }
    Abc_Obj_t *pOut = Abc_NtkPo(pNtk, k);
    Abc_Obj_t *pRoot = Abc_ObjFanin0(pOut);
    DdManager *dd = (DdManager *)pRoot->pNtk->pManFunc;

    int l, iFanin, count = 0;
    Abc_Obj_t *pFanin;
    DdNode* cof_pos = (DdNode*)pRoot->pData, *cof_pos_new;
    DdNode* cof_neg = (DdNode*)pRoot->pData, *cof_neg_new;
    Cudd_Ref(cof_pos);
    Cudd_Ref(cof_neg);

    Vec_Ptr_t * vNamesIn;
    vNamesIn = Abc_NodeGetFaninNames(pRoot);
    int i_index = Vec_PtrFindStr(vNamesIn, Abc_ObjName(Abc_NtkPi(pNtk, i)));
    int j_index = Vec_PtrFindStr(vNamesIn, Abc_ObjName(Abc_NtkPi(pNtk, j)));
    if (i_index != -1) {
        DdNode *dd_i = Cudd_bddIthVar(dd, i_index);
        Cudd_Ref(dd_i);
        cof_pos_new = Cudd_Cofactor(dd, cof_pos, dd_i);
        Cudd_Ref(cof_pos_new);
        Cudd_RecursiveDeref(dd, cof_pos);
        cof_pos = cof_pos_new;
        
        cof_neg_new = Cudd_Cofactor(dd, cof_neg, Cudd_Not(dd_i));
        Cudd_Ref(cof_neg_new);
        Cudd_RecursiveDeref(dd, cof_neg);
        cof_neg = cof_neg_new;
    }
    if (j_index != -1) {
        DdNode *dd_j = Cudd_bddIthVar(dd, j_index);
        Cudd_Ref(dd_j);
        cof_pos_new = Cudd_Cofactor(dd, cof_pos, Cudd_Not(dd_j));
        Cudd_Ref(cof_pos_new);
        Cudd_RecursiveDeref(dd, cof_pos);
        cof_pos = cof_pos_new;

        cof_neg_new = Cudd_Cofactor(dd, cof_neg, dd_j);
        Cudd_Ref(cof_neg_new);
        Cudd_RecursiveDeref(dd, cof_neg);
        cof_neg = cof_neg_new;
    }

    DdNode *counter_example = Cudd_bddXor(dd, cof_pos, cof_neg);
    Cudd_Ref(counter_example);
    Cudd_RecursiveDeref(dd, cof_pos);
    Cudd_RecursiveDeref(dd, cof_neg);

    if (counter_example == Cudd_Not(DD_ONE(dd)) || counter_example == DD_ZERO(dd)) {
        Abc_Print(1, "symmetric\n");
    } else {
        Abc_Print(1, "asymmetric\n");
        std::vector<Path> path = Lsv_NtkBddPath2One(dd, counter_example);

        std::vector<int> cex;
        Abc_NtkForEachPi(pNtk, pFanin, l) {
            int index = Vec_PtrFindStr(vNamesIn, Abc_ObjName(pFanin));
            DdNode* bVar = Cudd_bddIthVar(dd, index);

            int find = 0;
            if (index != -1) {
                for (int i = 0; i < path.size(); ++i) {
                    if (path[i].Id == bVar->Id) {
                        cex.push_back(path[i].T);
                        find = 1;
                        break;
                    }
                }
            }
            if (!find) {
                if (l == j) {
                    cex.push_back(1);
                } else {
                    cex.push_back(0);
                }
            }
        }
        for (int i = 0; i < Abc_NtkPiNum(pNtk); ++i) {
            Abc_Print(1, "%d", cex[i]);
        }
        Abc_Print(1, "\n");
        std::swap(cex[i], cex[j]);
        for (int i = 0; i < Abc_NtkPiNum(pNtk); ++i) {
            Abc_Print(1, "%d", cex[i]);
        }
        Abc_Print(1, "\n");
    }
    Cudd_RecursiveDeref(dd, counter_example);
}

int Lsv_CommandSymBdd(Abc_Frame_t *pAbc, int argc, char **argv) {
    Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
    Abc_Obj_t *pObj;
    int c, nArgc;
    char **nArgv;
    // set defaults
    Extra_UtilGetoptReset();
    while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
        switch (c) {
        case 'h':
            goto usage;
        default:
            goto usage;
        }
    }

    nArgc = argc - globalUtilOptind;
    nArgv = argv + globalUtilOptind;

    if (nArgc != 3) {
        goto usage;
    }

    if (pNtk == NULL) {
        Abc_Print(-1, "Empty network.\n");
        return 1;
    }

	if (!Abc_NtkIsBddLogic(pNtk)) {
		Abc_Print(-1, "Cannot lsv_sym_bdd a network that is not an BDD (run \"collapse\").\n");
        return 1;
	}

    Lsv_NtkSymBdd(pNtk, atoi(nArgv[0]), atoi(nArgv[1]), atoi(nArgv[2]));

	return 0;
usage:
    Abc_Print(-2, "usage: lsv_sym_bdd [-h] <k> <i> <j>\n");
    Abc_Print(-2, "\t        symmetry checking with output k and input i and j using bdd\n");
    Abc_Print(-2, "\t-h    : print the command usage\n");
    return 1;
}

void Lsv_NtkSimAig(Abc_Ntk_t *pNtk, Vec_Ptr_t *pModel_bit) {
    assert(Abc_NtkIsStrash(pNtk));
    assert(Abc_NtkPiNum(pNtk) == Vec_PtrSize(pModel_bit));
    int i, j, v0, v1, nBits, batchSize;
    Abc_Obj_t *pObj;

    int *pModel = ABC_ALLOC(int, Abc_NtkPiNum(pNtk));
    Vec_Ptr_t *pValue = Vec_PtrAlloc(Abc_NtkPoNum(pNtk));
    for (int i = 0; i < Abc_NtkPoNum(pNtk); ++i) {
        Vec_PtrPush(pValue, Vec_IntAlloc(0));
    }

    nBits = Vec_BitSize((Vec_Bit_t *)Vec_PtrEntry(pModel_bit, 0));
    batchSize = (nBits - 1) / 32 + 1;
    for (int b = 0; b < batchSize; ++b) {
        for (i = 0; i < Abc_NtkPiNum(pNtk); ++i) {
            pModel[i] = ((Vec_Bit_t *)Vec_PtrEntry(pModel_bit, i))->pArray[b];
        }

        Abc_AigConst1(pNtk)->iTemp = S_ONE;
        Abc_NtkForEachCi(pNtk, pObj, i) {
            pObj->iTemp = pModel[i];
        }
        Abc_NtkForEachNode(pNtk, pObj, i) {
            v0 = Abc_ObjFanin0(pObj)->iTemp ^ ((Abc_ObjFaninC0(pObj)) ? S_ONE : S_ZERO);
            v1 = Abc_ObjFanin1(pObj)->iTemp ^ ((Abc_ObjFaninC1(pObj)) ? S_ONE : S_ZERO);
            pObj->iTemp = v0 & v1;
        }
        Abc_NtkForEachPo(pNtk, pObj, i) {
            Vec_IntPush((Vec_Int_t *)Vec_PtrEntry(pValue, i), Abc_ObjFanin0(pObj)->iTemp ^ ((Abc_ObjFaninC0(pObj)) ? S_ONE : S_ZERO));
        }
    }
    Abc_NtkForEachPo(pNtk, pObj, i) {
        printf("%s: ", Abc_ObjName(pObj));
        for (int b = 0; b < batchSize; ++b) {
            int res = Vec_IntEntry((Vec_Int_t *)Vec_PtrEntry(pValue, i), b);
            for (j = 0; j < nBits - 32 * b && j < 32; ++j) {
                printf("%d", (res >> j) & 1);
            }
        }
        printf("\n");
    }
    ABC_FREE(pModel);
    for (int i = 0; i < Abc_NtkPoNum(pNtk); ++i) {
        Vec_IntFree((Vec_Int_t *)Vec_PtrEntry(pValue, i));
    }
    Vec_PtrFree(pValue);
}

Vec_Ptr_t *Lsv_GetPattern(const char *filepath, int nInputs) {
    std::fstream fin(filepath);
    if (fin.fail()) {
        return NULL;
    }

    Vec_Ptr_t *pModel = Vec_PtrAlloc(nInputs);
    for (int i = 0; i < nInputs; ++i) {
        Vec_PtrPush(pModel, Vec_BitAlloc(0));
    }

    std::string line;
    while (std::getline(fin, line)) {
		if (line.size() != nInputs) {
			printf("Inconsistent input size %d vs %d.\n", line.size(), nInputs);
			printf("Stop at bit %d.\n", Vec_BitSize((Vec_Bit_t *)Vec_PtrEntry(pModel, 0)));
			fin.close();
			return pModel;
		}
        for (int i = 0; i < nInputs; ++i) {
            Vec_BitPush((Vec_Bit_t *)Vec_PtrEntry(pModel, i), line[i] == '1');
        }
    }

    fin.close();

    return pModel;
}

int Lsv_CommandSimAig(Abc_Frame_t *pAbc, int argc, char **argv) {
    Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
    Vec_Ptr_t *pModel = NULL;
    int c, fGlobal = 0, fVerbose = 0, nArgc;
    char **nArgv;
    // set defaults
    Extra_UtilGetoptReset();
    while ((c = Extra_UtilGetopt(argc, argv, "gvh")) != EOF) {
        switch (c) {
        case 'g':
            fGlobal ^= 1;
            break;
        case 'v':
            fVerbose ^= 1;
            break;
        case 'h':
            goto usage;
        default:
            goto usage;
        }
    }

    nArgc = argc - globalUtilOptind;
    nArgv = argv + globalUtilOptind;

    if (nArgc != 1) {
        goto usage;
    }

    if (pNtk == NULL) {
        Abc_Print(-1, "Empty network.\n");
        return 1;
    }

    if (!Abc_NtkIsStrash(pNtk)) {
        Abc_Print(-1, "Cannot lsv_sim_aig a network that is not an AIG (run \"strash\").\n");
        return 1;
    }

    pModel = Lsv_GetPattern(nArgv[0], Abc_NtkPiNum(pNtk));

    if (!pModel) {
        Abc_Print(-1, "Cannot open the input pattern file \"%s\"\n", nArgv[0]);
        return 1;
    }

    Lsv_NtkSimAig(pNtk, pModel);

final:
    for (c = 0; c < Abc_NtkPiNum(pNtk); ++c) {
        Vec_BitFree((Vec_Bit_t *)Vec_PtrEntry(pModel, c));
    }
    Vec_PtrFree(pModel);
    return 0;
usage:
    Abc_Print(-2, "usage: lsv_sim_aig [-gh] <input pattern file>\n");
    Abc_Print(-2, "\t        parallel simulation using aig\n");
    Abc_Print(-2, "\t-g    : parallel simulation of all primary outputs [default = %s].\n", fGlobal ? "yes" : "no");
    Abc_Print(-2, "\t-v    : verbose [default = %s].\n", fVerbose ? "yes" : "no");
    Abc_Print(-2, "\t-h    : print the command usage\n");
    return 1;
}

void Lsv_NtkSimBdd(Abc_Ntk_t *pNtk, const char *pattern) {
	int i;
    Abc_Obj_t *pNode;
    Vec_Ptr_t * vNamesIn;
    char * pNameOut;
    int comp;
    DdNode *ptr;

    if (strlen(pattern) != Abc_NtkPiNum(pNtk)) {
        printf("Inconsistent input size %d vs %d\n", strlen(pattern), Abc_NtkPiNum(pNtk));
        return;
    }

    std::unordered_map<std::string, int> map;
    Abc_NtkForEachPi(pNtk, pNode, i) {
        map[Abc_ObjName(pNode)] = pattern[i] == '1';
    }

    Abc_NtkForEachPo(pNtk, pNode, i) {
        printf("%s: ", Abc_ObjName(pNode));
        pNode = Abc_ObjFanin0(pNode);
        DdManager * dd = (DdManager *)pNode->pNtk->pManFunc;
        vNamesIn = Abc_NodeGetFaninNames( pNode );
        pNameOut = Abc_ObjName(pNode);

        int *model = ABC_ALLOC(int, Vec_PtrSize(vNamesIn));
        for (int i = 0; i < Vec_PtrSize(vNamesIn); ++i) {
            model[i] = map[(char *)Vec_PtrEntry(vNamesIn, i)];
        }

        // root node
        DdNode *root = (DdNode *)pNode->pData;
        comp = Cudd_IsComplement(root);
        ptr = Cudd_Regular(root);
        for (int i = 0; i < Vec_PtrSize(vNamesIn); ++i) {
            if (Cudd_IsConstant(ptr)) break;
            if (model[ptr->index] == 1) {
                ptr = cuddT(ptr);
            } else {
                comp ^= Cudd_IsComplement(cuddE(ptr));
                ptr = Cudd_Regular(cuddE(ptr));
            }
        }
        DdNode *target = Cudd_NotCond(ptr, comp);
        int constant = Cudd_IsConstant(target);
        printf("%d\n", constant ^ comp);

        ABC_FREE(model);
        Abc_NodeFreeNames( vNamesIn );
    }
}

int Lsv_CommandSimBdd(Abc_Frame_t *pAbc, int argc, char **argv) {
    Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
    Abc_Obj_t *pObj;
    int c, nArgc;
    char **nArgv;
    // set defaults
    Extra_UtilGetoptReset();
    while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
        switch (c) {
        case 'h':
            goto usage;
        default:
            goto usage;
        }
    }

    nArgc = argc - globalUtilOptind;
    nArgv = argv + globalUtilOptind;

    if (nArgc != 1) {
        goto usage;
    }

    if (pNtk == NULL) {
        Abc_Print(-1, "Empty network.\n");
        return 1;
    }

	if (!Abc_NtkIsBddLogic(pNtk)) {
		Abc_Print(-1, "Cannot lsv_sim_bdd a network that is not an BDD (run \"collapse\").\n");
        return 1;
	}

    Lsv_NtkSimBdd(pNtk, nArgv[0]);

	return 0;
usage:
    Abc_Print(-2, "usage: lsv_sim_bdd [-h] <input pattern>\n");
    Abc_Print(-2, "\t        simulation using bdd\n");
    Abc_Print(-2, "\t-h    : print the command usage\n");
    return 1;
}

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
        if (Abc_NtkHasSop(pNtk)) {
            printf("The SOP of this node:\n%s", (char *)pObj->pData);
        }
    }
}

int Lsv_CommandPrintNodes(Abc_Frame_t *pAbc, int argc, char **argv) {
    Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
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