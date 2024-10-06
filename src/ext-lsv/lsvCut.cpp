#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "aig/aig/aig.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <set>
#include <queue>
#include <stdio.h>
#include <stdlib.h>
#include "misc/vec/vec.h"
#include "misc/vec/vecInt.h"

// Déclaration de la fonction de commande pour afficher les coupes
static int Lsv_CommandPrintCuts(Abc_Frame_t* pAbc, int argc, char** argv);

// Initialisation de la commande dans ABC
void init_lsv_printcut(Abc_Frame_t* pAbc) {
    Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_CommandPrintCuts, 0);
}

// Fonction de nettoyage
void CleanupLsv(Abc_Frame_t* pAbc) {}

// Structure pour gérer l'enregistrement du paquet
Abc_FrameInitializer_t frameInit = {init_lsv_printcut, CleanupLsv};

struct PackageRegistrar {
    PackageRegistrar() { 
        Abc_FrameAddInitializer(&frameInit); 
    }
} registrar;

// Vérifie si une coupe donnée bloque tous les chemins vers les PI
bool isCutValid(const std::vector<int>& cut, Abc_Obj_t* pObj) {
    std::set<int> cutSet(cut.begin(), cut.end());
    std::queue<Abc_Obj_t*> queue;

    // Ajouter le nœud de départ à la queue
    queue.push(pObj);

    // Effectuer une recherche en largeur (BFS)
    while (!queue.empty()) {
        Abc_Obj_t* current = queue.front();
        queue.pop();

        // Vérifiez si c'est un PI (Primary Input)
        if (Abc_ObjIsPi(current)) {
            return false; // Un chemin vers un PI est trouvé, donc la coupe n'est pas valide
        }

        // Récupérer les fan-ins du nœud courant
        Abc_Obj_t* pFanin;
        int j;
        Abc_ObjForEachFanin(current, pFanin, j) {
            // Si le fan-in n'est pas dans la coupe, l'ajouter à la queue pour exploration
            if (cutSet.find(Abc_ObjId(pFanin)) == cutSet.end()) {
                queue.push(pFanin);
            }
        }
    }

    // Si tous les chemins vers les PIs ont été bloqués par la coupe
    return true; 
}

// Vérifie si une coupe est k-feasible
bool isKFeasibleCut(const std::vector<int>& cut, Abc_Obj_t* pObj) {
    int cutSize = cut.size();
    std::set<int> cutSet(cut.begin(), cut.end());

    // Parcourir tous les sous-ensembles possibles de la coupe
    for (size_t i = 1; i < (1 << cutSize); ++i) {
        std::vector<int> subset;

        // Générer un sous-ensemble à partir du bitmask
        for (size_t j = 0; j < cutSize; ++j) {
            if (i & (1 << j)) {
                subset.push_back(cut[j]);
            }
        }

        // Si le sous-ensemble est valide et sa taille est inférieure à celle de la coupe originale
        if (!subset.empty() && subset.size() < cutSize) {
            if (isCutValid(subset, pObj)) {
                // Un sous-ensemble valide a été trouvé, donc la coupe originale n'est pas k-feasible
                return false;
            }
        }
    }

    // Si aucun sous-ensemble valide n'a été trouvé, la coupe est k-feasible
    return true;
}

// Fonction pour calculer les coupes k-feasibles pour un nœud spécifique
Vec_Ptr_t* ComputeCuts(Abc_Obj_t* pNode, int k) {
    // Pour les nœuds d'entrée, retourne une coupe qui ne contient que ce nœud
    if (Abc_ObjIsCi(pNode)) {
        Vec_Ptr_t* cutsList = Vec_PtrAlloc(1);  
        Vec_Int_t* cut = Vec_IntAlloc(1);   
        Vec_IntPush(cut, Abc_ObjId(pNode)); 
        Vec_PtrPush(cutsList, cut);        
        return cutsList;
    }

    // Ignorer les nœuds avec moins de 2 entrées
    if (Abc_ObjFaninNum(pNode) < 2) {
        printf("Nœud %d a moins de 2 entrées. Ignoré.\n", Abc_ObjId(pNode));
        return NULL;
    }

    // Obtenir les nœuds d'entrée
    Abc_Obj_t* firstInput = Abc_ObjFanin0(pNode);
    Abc_Obj_t* secondInput = Abc_ObjFanin1(pNode);

    // Calculer les coupes pour chaque nœud d'entrée
    Vec_Ptr_t* cutsFromFirst = ComputeCuts(firstInput, k);
    Vec_Ptr_t* cutsFromSecond = ComputeCuts(secondInput, k);

    // Vérifier si les coupes ont été générées
    if (!cutsFromFirst || !cutsFromSecond) {
        return NULL;
    }

    // Vecteur pour stocker les coupes combinées
    Vec_Ptr_t* mergedCuts = Vec_PtrAlloc(100);  

    // Ajouter le nœud lui-même comme coupe
    Vec_Int_t* selfCut = Vec_IntAlloc(1);
    Vec_IntPush(selfCut, Abc_ObjId(pNode));
    Vec_PtrPush(mergedCuts, selfCut);  

    // Combinaison des coupes des nœuds d'entrée
    for (int i = 0; i < Vec_PtrSize(cutsFromFirst); i++) {
        Vec_Int_t* cutFromFirst = (Vec_Int_t*)Vec_PtrEntry(cutsFromFirst, i);
        for (int j = 0; j < Vec_PtrSize(cutsFromSecond); j++) {
            Vec_Int_t* cutFromSecond = (Vec_Int_t*)Vec_PtrEntry(cutsFromSecond, j);
            // Fusionner et trier les coupes
            Vec_Int_t* newCut = Vec_IntTwoMerge(cutFromFirst, cutFromSecond); 

            // Vérifier la taille de la coupe
            if (Vec_IntSize(newCut) > k) {
                Vec_IntFree(newCut);
                continue;
            }

            // Convertir Vec_Int_t* en std::vector<int> pour l'utilisation dans isKFeasibleCut
            std::vector<int> newCutVector(Vec_IntArray(newCut), Vec_IntArray(newCut) + Vec_IntSize(newCut));

            // Vérifier si la coupe est k-feasible
            if (isKFeasibleCut(newCutVector, pNode)) {
                // Ajouter le nœud actuel si absent de la coupe
                if (!Vec_IntFind(newCut, Abc_ObjId(pNode))) {
                    Vec_IntPush(newCut, Abc_ObjId(pNode));
                }

                // Trier la coupe avant de l'ajouter à la liste finale
                Vec_IntSort(newCut, 0);
                Vec_PtrPush(mergedCuts, newCut);
            } else {
                Vec_IntFree(newCut);
            }
        }
    }

    // Libération de la mémoire pour les coupes d'entrée
    Vec_PtrFree(cutsFromFirst);
    Vec_PtrFree(cutsFromSecond);

    return mergedCuts;
}

// Fonction pour afficher les coupes k-feasibles
void ShowCuts(Abc_Obj_t* pNode, Vec_Ptr_t* cuts) {
    printf("%d: ", Abc_ObjId(pNode));
    Vec_Int_t* selfCut = (Vec_Int_t*)Vec_PtrEntry(cuts, 0);
    for (int j = 0; j < Vec_IntSize(selfCut); j++) {
        printf("%d ", Vec_IntEntry(selfCut, j));
    }
    printf("\n");

    // Affichage des autres coupes
    for (int i = 1; i < Vec_PtrSize(cuts); i++) {
        Vec_Int_t* cut = (Vec_Int_t*)Vec_PtrEntry(cuts, i);
        printf("%d: ", Abc_ObjId(pNode));
        for (int j = 0; j < Vec_IntSize(cut); j++) {
            printf("%d ", Vec_IntEntry(cut, j));
        }
        printf("\n");
    }
}

// Fonction principale pour énumérer et afficher les coupes
void EnumerateCuts(Abc_Ntk_t* pNtk, int k) {
    Abc_Obj_t* pNode;
    int i;
    Abc_NtkForEachObj(pNtk, pNode, i) { 
        if (Abc_ObjIsNode(pNode) || Abc_ObjIsCi(pNode)) {
            Vec_Ptr_t* cuts = ComputeCuts(pNode, k);
            if (cuts) {
                ShowCuts(pNode, cuts);
                Vec_PtrFree(cuts);
            }
        }
    }
}

// Fonction appelée par la commande "lsv printcut <k>"
int Lsv_CommandPrintCuts(Abc_Frame_t* pAbc, int argc, char** argv) {
    // Vérifier le nombre d'arguments
    if (argc < 2) {
        printf("Usage: lsv printcut <k>\n");
        return 0;
    }

    // Extraire la valeur de k
    int k = atoi(argv[1]);
    if (k < 3 || k > 6) {
        printf("k doit être compris entre 3 et 6.\n");
        return 0;
    }

    // Obtenir le réseau courant
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    EnumerateCuts(pNtk, k);
    return 0;
}

