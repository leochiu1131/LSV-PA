#include <iostream>
#include <vector>
#include <string>
#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/cnf/cnf.h"

#include "base/abc/abcShow.c"
extern "C"{
    Aig_Man_t* Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
}
using namespace std;

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintSDC(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintODC(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sdc", Lsv_CommandPrintSDC, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_odc", Lsv_CommandPrintODC, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

// Fonction pour créer un CNF basé sur les nœuds d'entrée d'un nœud donné.
void CreateCnfForFaninNodes(Abc_Ntk_t * pNtk, int n, Cnf_Dat_t ** ppCnf, sat_solver ** ppSolver, int * pVarY0, int * pVarY1, int * pComplY0, int * pComplY1) {
    // Récupération des nœuds fan-in du nœud cible.
    Abc_Obj_t * pNodeA = Abc_NtkObj(pNtk, n);
    Abc_Obj_t * pFanin0 = Abc_ObjFanin0(pNodeA);
    Abc_Obj_t * pFanin1 = Abc_ObjFanin1(pNodeA);

    // Création d'un vecteur contenant les nœuds fan-in.
    Vec_Ptr_t * vNodes = Vec_PtrAlloc(2);
    Vec_PtrPush(vNodes, pFanin0);
    Vec_PtrPush(vNodes, pFanin1);

    // Création d'un sous-réseau (cone) contenant uniquement ces nœuds.
    Abc_Ntk_t * pConeNtk = Abc_NtkCreateConeArray(pNtk, vNodes, 1);
    Vec_PtrFree(vNodes);

    // Conversion du sous-réseau en un graphe AIG.
    Aig_Man_t * pAig = Abc_NtkToDar(pConeNtk, 0, 0);

    // Génération du CNF associé au graphe AIG.
    *ppCnf = Cnf_Derive(pAig, 2);

    // Création du solveur SAT.
    *ppSolver = sat_solver_new();

    // Ajout des clauses CNF dans le solveur.
    Cnf_DataWriteIntoSolverInt(*ppSolver, *ppCnf, 1, 0);

    // Récupération des variables du CNF correspondant aux sorties du graphe AIG.
    *pVarY0 = (*ppCnf)->pVarNums[Aig_ObjId(Aig_ManCo(pAig, 0))];
    *pVarY1 = (*ppCnf)->pVarNums[Aig_ObjId(Aig_ManCo(pAig, 1))];
    *pComplY0 = Abc_ObjFaninC0(pNodeA);
    *pComplY1 = Abc_ObjFaninC1(pNodeA);

    // Libération des ressources.
    Aig_ManStop(pAig);
    Abc_NtkDelete(pConeNtk);
}

// Vérification d'un modèle donné dans le solveur SAT.
int CheckPattern(sat_solver * pSolver, Cnf_Dat_t * pCnf, int y0, int y1, int v0, int v1, int complY0, int complY1) {
    // Réinitialisation du solveur SAT.
    sat_solver_restart(pSolver);

    // Ajout des clauses CNF au solveur.
    Cnf_DataWriteIntoSolverInt(pSolver, pCnf, 1, 0);

    // Création des littéraux pour les variables.
    int lit_y0 = Abc_Var2Lit(y0, !(v0 ^ complY0));
    int lit_y1 = Abc_Var2Lit(y1, !(v1 ^ complY1));

    // Ajout des clauses correspondantes au solveur.
    sat_solver_addclause(pSolver, &lit_y0, &lit_y0 + 1);
    sat_solver_addclause(pSolver, &lit_y1, &lit_y1 + 1);

    // Résolution SAT pour le modèle donné.
    int status = sat_solver_solve(pSolver, NULL, NULL, 0, 0, 0, 0);
    return status;
}

Vec_Int_t * Lsv_NtkPrint_SDC(Abc_Ntk_t* pNtk, int n, bool print) {
  Abc_Obj_t * nodeN = Abc_NtkObj(pNtk, n);
  if(nodeN == NULL){
    cout<< "ERROR: NULL node\n";
    return NULL;
  }
  if(Abc_ObjFaninNum(nodeN) == 0){
    printf("no sdc\n");
    return NULL;
  }
    Cnf_Dat_t * pCnf;
    sat_solver * pSolver;
    int y0, y1, complY0, complY1;
    Vec_Int_t * vSDC = Vec_IntAlloc(4);

    CreateCnfForFaninNodes(pNtk, n, &pCnf, &pSolver, &y0, &y1, &complY0, &complY1);

    int patterns[4][2] = {{0, 0}, {0, 1}, {1, 0}, {1, 1}};
    for (int i = 0; i < 4; i++) {
        int v0 = patterns[i][0];
        int v1 = patterns[i][1];
        int status = CheckPattern(pSolver, pCnf, y0, y1, v0, v1, complY0, complY1);
        if (status == l_False) {
            Vec_IntPush(vSDC, (v0 << 1) | v1);
        } else {
        }
    }

    if(print){
      if (Vec_IntSize(vSDC) == 0) {
          printf("no sdc\n");
      } else {
          int Entry, i;
          Vec_IntForEachEntry(vSDC, Entry, i) {
              printf("%d%d ", (Entry >> 1) & 1, Entry & 1);
          }
          printf("\n");
      }
    }
    Cnf_DataFree(pCnf);
    sat_solver_delete(pSolver);
    return vSDC;
}

Vec_Int_t *  SolveAllSat(sat_solver * pSolver, int y0, int y1, Abc_Obj_t * nodeN) {
    Vec_Int_t * vCareSet = Vec_IntAlloc(4);
    int count=0;
    while (1) {
      count++;
        int status = sat_solver_solve(pSolver, NULL, NULL, 0, 0, 0, 0);
        if (status != l_True) {
            break;
        }
        int v0 = sat_solver_var_value(pSolver, y0);
        int v1 = sat_solver_var_value(pSolver, y1);
        int lit_y0 = Abc_Var2Lit(y0, v0);
        int lit_y1 = Abc_Var2Lit(y1, v1);
        v0 = v0 ^ Abc_ObjFaninC0(nodeN);
        v1 = v1 ^ Abc_ObjFaninC1(nodeN);
        Vec_IntPush(vCareSet, (v0 << 1) | v1);
        int clause[2] = {lit_y0, lit_y1};
        if(!sat_solver_addclause(pSolver, clause, clause + 2)) break;
    }
    return vCareSet;
}


void Lsv_NtkPrint_ODC(Abc_Ntk_t* pNtk, int n) {
  Abc_Obj_t * nodeN = Abc_NtkObj(pNtk, n);
  if(nodeN == NULL){
    cout<< "ERROR: NULL node\n";
    return;
  }
  if(Abc_ObjFaninNum(nodeN) == 0 || Abc_ObjFaninNum(nodeN) == 1){
    printf("no odc\n");
    return;
  }
  if(Abc_ObjIsPo(nodeN)){
    nodeN = Abc_NtkObj(pNtk, n);
  }

  Vec_Int_t * vSDC = Lsv_NtkPrint_SDC(pNtk, n, false);
    Abc_Ntk_t * pNtkNeg = Abc_NtkDup(pNtk);
    Abc_Ntk_t * pBig = Abc_NtkDup(pNtk);
    Abc_Obj_t * pNodeA = Abc_NtkObj(pNtkNeg, n);
    Abc_Obj_t* pFanout;
    int i;
    Abc_ObjForEachFanout(pNodeA, pFanout, i) {

        Abc_ObjXorFaninC(pFanout, Abc_ObjFanoutFaninNum(pFanout, pNodeA));
    }
    Abc_Ntk_t * pMiter = Abc_NtkMiter(pNtk, pNtkNeg, 1, 0, 0, 0);
    Abc_NtkAppend(pBig, pMiter, 1);
    pMiter = pBig;
    int pVarY0, pVarY1;
    Abc_Obj_t * NpMiter[2] = {NULL, NULL};
    if(Abc_ObjFanin0(nodeN) == NULL || Abc_ObjFanin1(nodeN) == NULL){
      cout<<"ERROR_fanin!!!!!!\n";
      return;
    }
    NpMiter[0] = Abc_NtkObj(pMiter, Abc_ObjId(Abc_ObjFanin0(nodeN)));
    NpMiter[1] = Abc_NtkObj(pMiter, Abc_ObjId(Abc_ObjFanin1(nodeN)));
    if(NpMiter[0] == NULL || NpMiter[1] == NULL){
      cout<<"ERROR_fanin_NpMiter\n";
      return;
    }
    Vec_Ptr_t * vNodes = Vec_PtrAlloc(3);
    Vec_PtrPush(vNodes, NpMiter[0]);
    Vec_PtrPush(vNodes, NpMiter[1]);
    Abc_Ntk_t * pConeIn = Abc_NtkCreateConeArray(pMiter, vNodes, 1);
    Abc_Obj_t * Conefanin[3] = {NULL, NULL, NULL};
    Conefanin[0] = Abc_ObjFanin0(Abc_NtkPo(pConeIn, 0));
    Conefanin[1] = Abc_ObjFanin0(Abc_NtkPo(pConeIn, 1));
    Vec_PtrPush(vNodes, Abc_NtkPo(pMiter, 1));
    Abc_Ntk_t * pCone = Abc_NtkCreateConeArray(pMiter, vNodes, 1);
    Conefanin[2] = Abc_NtkPo(pCone, 2);
    Vec_PtrFree(vNodes);
    Aig_Man_t * pAig = Abc_NtkToDar(pCone, 0, 0);
    Aig_Obj_t * NpAig[3] = {NULL, NULL, NULL};
    if(Conefanin[0] == NULL ||Conefanin[1] == NULL ||Conefanin[2] == NULL){
      cout<<"ERROR: Conefanin NULL!!!\n";
    }

    NpAig[0] = Aig_ManObj(pAig, Abc_ObjId(Conefanin[0]));
    NpAig[1] = Aig_ManObj(pAig, Abc_ObjId(Conefanin[1]));
    NpAig[2] = Aig_ManObj(pAig, Abc_ObjId(Conefanin[2]));
    if(NpAig[0] == NULL ||  NpAig[1] == NULL ||  NpAig[2] == NULL){
      cout<<"ERROR_fanin_NpAig\n";
      return;
    }

    Cnf_Dat_t * pCnf = Cnf_Derive(pAig, Aig_ManCoNum(pAig));

    int NpCnf[3] = {-1, -1, -1};
    if(Aig_ObjId(NpAig[0]) == -1 || Aig_ObjId(NpAig[1]) == -1){
      cout<<"ERROR_fanin_0_NpCnf!!!!!!\n";
      return;
    }

    NpCnf[0] = pCnf->pVarNums[ Aig_ObjId(NpAig[0]) ];
    NpCnf[1] = pCnf->pVarNums[ Aig_ObjId(NpAig[1]) ];
    NpCnf[2] = pCnf->pVarNums[ Aig_ObjId(NpAig[2]) ];
    if(NpCnf[0] == -1 ||  NpCnf[1] == -1 ||  NpCnf[2] == -1){
      cout<<"ERROR_fanin_NpCnf\n";
      return;
    }
    pVarY0 = NpCnf[0];
    pVarY1 = NpCnf[1];

    sat_solver * pSolver = sat_solver_new();

    Cnf_DataWriteIntoSolverInt(pSolver, pCnf, 1, 0);

    if(pVarY0==-1 || pVarY1==-1){
      cout<<"NULL ERROR return\n";
      return;
    }

    int lit_miter = Abc_Var2Lit(NpCnf[2], 0);
    sat_solver_addclause(pSolver, &lit_miter, &lit_miter + 1);

    Vec_Int_t * vCareSet = SolveAllSat(pSolver, pVarY0, pVarY1, nodeN);
    Vec_Int_t * vODC = Vec_IntAlloc(4);

    int patterns[4] = {0, 1, 2, 3};

    for (int i = 0; i < 4; i++) {
        if (Vec_IntFind(vCareSet, patterns[i]) == -1) {
            Vec_IntPush(vODC, patterns[i]);
        }
    }

    for (int i = 0; i < Vec_IntSize(vSDC); i++) {
        int sdc = Vec_IntEntry(vSDC, i);
        Vec_IntRemove(vODC, sdc);
    }

    if (Vec_IntSize(vODC) == 0) {
        printf("no odc\n");
    } else {
        int Entry, i;
        Vec_IntForEachEntry(vODC, Entry, i) {
            printf("%d%d ", (Entry >> 1) & 1, Entry & 1);
        }
        printf("\n");
    }

    Cnf_DataFree(pCnf);
    sat_solver_delete(pSolver);
    Aig_ManStop(pAig);

    Abc_NtkDelete(pNtkNeg);
    Abc_NtkDelete(pMiter);
}

void Lsv_NtkPrintNodes(Abc_Ntk_t* pNtk) {
  Abc_Obj_t* pObj;
  int i;
  for ( i = 0; (i < Vec_PtrSize((pNtk)->vObjs)) && (((pObj) = Abc_NtkObj(pNtk, i)), 1); i++ )   \
        if ( (pObj) == NULL
        ) {} else{
    
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

// Commande pour afficher les coupes SDC (structural don't-care)
int Lsv_CommandPrintSDC(Abc_Frame_t* pAbc, int argc, char** argv) {
    // Récupération du réseau actuel depuis le cadre ABC
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);

    // Conversion de l'argument en entier (paramètre c)
    int c = std::stoi(argv[1]);

    // Réinitialisation des options d'utils (utile pour les commandes suivantes)
    Extra_UtilGetoptReset();

    // Vérification si le réseau est vide
    if (!pNtk) {
        // Message d'erreur si aucun réseau n'est chargé
        Abc_Print(-1, "Réseau vide.\n");
        return 1; // Retourne 1 pour indiquer une erreur
    }

    // Appelle la fonction d'affichage des coupes SDC pour le réseau
    Lsv_NtkPrint_SDC(pNtk, c, true);

    // Retourne 0 pour indiquer un succès
    return 0;
}

// Commande pour afficher les coupes ODC (observability don't-care)
int Lsv_CommandPrintODC(Abc_Frame_t* pAbc, int argc, char** argv) {
    // Récupération du réseau actuel depuis le cadre ABC
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);

    // Conversion de l'argument en entier (paramètre c)
    int c = std::stoi(argv[1]);

    // Réinitialisation des options d'utils (utile pour les commandes suivantes)
    Extra_UtilGetoptReset();

    // Vérification si le réseau est vide
    if (!pNtk) {
        // Message d'erreur si aucun réseau n'est chargé
        Abc_Print(-1, "Réseau vide.\n");
        return 1; // Retourne 1 pour indiquer une erreur
    }

    // Appelle la fonction d'affichage des coupes ODC pour le réseau
    Lsv_NtkPrint_ODC(pNtk, c);

    // Retourne 0 pour indiquer un succès
    return 0;
}

