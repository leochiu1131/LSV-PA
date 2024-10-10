#include "base/abc/abc.h"
#include "aig/aig/aig.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <iostream>
#include <vector>
#include <bits/stdc++.h>
#include <algorithm>

extern "C" {
  Aig_Man_t * Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
} 

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintCuts(Abc_Frame_t* pAbc, int argc, char** argv);


void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_CommandPrintCuts, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

//Aig_Man_t* NtktoAig(Abc_Ntk_t * pNtk);

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

bool isSubset(const std::vector<int>& a, const std::vector<int>& b) {
    return std::includes(a.begin(), a.end(), b.begin(), b.end());
}

void removeDuplicates(std::vector<std::vector<int>>& vecs) {
    for (auto it = vecs.begin(); it != vecs.end(); ++it) {
        bool subset = false;
        for (const auto& v : vecs) {
            if (it->size()>v.size() && isSubset(*it, v)) {
                subset = true;
                break;
            }
        }
        if (subset) {
            it = vecs.erase(it);
            --it;
        }
    }
}


void Lsv_NtkPrintCuts(Abc_Ntk_t* pNtk, int k) {
  //extern Aig_Man_t * Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
  Aig_Obj_t* pObj;
  Abc_Ntk_t* pNtkAig;
  //Abc_Ntk_t* p=pNtk;
  int i;  
   //std::cout<< k<< std::endl;
  pNtkAig    = Abc_NtkStrash( pNtk, 0, 1, 0 );
  Aig_Man_t* p=Abc_NtkToDar(pNtkAig, 0, 1);
  //Abc_NtkToAig(pNtk);

  // Aig_ManForEachPiSeq( p, pObj, i )   printf("PI node: %d\n", Aig_ObjId(pObj));
  // Aig_ManForEachNode( p, pObj, i ) printf("node: %d\n", Aig_ObjId(pObj));
  // Aig_ManForEachPoSeq( p, pObj, i ) printf("PO node: %d\n", Aig_ObjId(pObj));
  std::vector<std::vector<std::vector<int>>> V;
  
  
  Aig_ManForEachPiSeq( p, pObj, i ) {
    V.push_back({{Aig_ObjId(pObj)}});
    printf("%d: %d\n", Aig_ObjId(pObj), V[Aig_ObjId(pObj)-1][0][0]);
  }
  //printf("------");


  Aig_ManForEachNode( p, pObj, i ){
    Aig_Obj_t *pFanin0=Aig_ObjFanin0(pObj), *pFanin1=Aig_ObjFanin1(pObj);
    std::vector<std::vector<int>> Vtemp;

    Vtemp.push_back({Aig_ObjId(pObj)});
    //printf("here i: %d, node: %d, cut: %d\n", i, Aig_ObjId(pObj), Vtemp[0][0]);
    //printf("\nnode %d fanin %d and %d \n", Aig_ObjId(pObj), Aig_ObjId(pFanin0), Aig_ObjId(pFanin1));
    for(auto P: V[Aig_ObjId(pFanin0)-1]){
      for(auto Q: V[Aig_ObjId(pFanin1)-1]){
        std::vector<int> Vtemp1;
        if(k>P.size()&&k>Q.size()){
          for(auto i1: P) {Vtemp1.push_back(i1);}
          for(auto i2: Q) {if(std::count(Vtemp1.begin(), Vtemp1.end(), i2)==0) Vtemp1.push_back(i2); }
          //printf("\ncut "); 
          //for(auto i3: Vtemp1) printf("%d ", i3);
          std::sort(Vtemp1.begin(), Vtemp1.end());
          //bool inc=0;
          // for(auto j:Vtemp){
          //   if(std::includes(Vtemp1.begin(), Vtemp1.end(), j.begin(), j.end())) {
          //     inc=1; break;
          //   }
          // }
          if(Vtemp1.size()<=k) {
            Vtemp.push_back(Vtemp1);
            //for(auto i3: Vtemp1) printf("%d ", i3);
            //printf("\n");
          }
        }
        
      }
      //printf("cut2: %d ", Vtemp[0][0]);
      // for(auto p: Vtemp) {
      //   printf("cut of %d: ", Aig_ObjId(pObj));
      //   for(auto q: p)      printf("%d ", q);
      //   printf("\n");

      // }
    }
    //printf("-----\n");
    removeDuplicates(Vtemp);
    V.push_back(Vtemp);
    for(auto p: V[Aig_ObjId(pObj)-1]) {
      printf("%d: ", Aig_ObjId(pObj));
      for(auto q: p)      printf("%d ", q);
      printf("\n");

    }
  }

  // Aig_ManForEachPoSeq( p, pObj, i ){
  //   Aig_Obj_t *pFanin0=Aig_ObjFanin0(pObj), *pFanin1=Aig_ObjFanin1(pObj);
  //   Vtemp.push_back({Aig_ObjId(pObj)});
  //   for(auto P: V[Aig_ObjId(pFanin0)-1]){
  //     for(auto Q: V[Aig_ObjId(pFanin1)-1]){
  //       std::vector<int> Vtemp1;
  //       for(auto i1: P) Vtemp1.push_back(i1);
  //       for(auto i2: Q) if(std::count(Vtemp1.begin(), Vtemp1.end(), i2)==0) Vtemp1.push_back(i2);
  //       std::sort(Vtemp1.begin(), Vtemp1.end());
  //       if(Vtemp1.size()<k) Vtemp.push_back(Vtemp1);
  //     }
  //   }
  //   V.push_back(Vtemp);
  // }

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

int Lsv_CommandPrintCuts(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  char *output;
  int k=strtol(argv[argc-1], &output, 10);
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
  // if(output){    
  //   std::cout<< output<< "\n"<< k;
  //   printf("\ninvalid k.\n");    
  //   return 1;
  // }
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  if ( !Abc_NtkIsStrash(pNtk) ){
    Abc_Print( 1, "Main AIG: The current network is not an AIG.\n");
    return 1;
  }
            
  
  Lsv_NtkPrintCuts(pNtk, k);
  
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_printcut <k> [-h]\n");
  Abc_Print(-2, "\t        prints k-feasible cuts of AIG\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

