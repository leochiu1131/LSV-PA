#include <iostream>
#include <set>
#include <map>
#include <vector>
#include <algorithm>
#include <bits/stdc++.h>
#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"

class CutSet{
  CutSet(std::set<Abc_Obj_t *> Cset): 
    cset(std::make_pair(Cset.size(), Cset)){};
  public:
    std::pair<int, std::set<Abc_Obj_t *>> cset;
};

class SortSet{
  public:
    bool operator()(const std::set<Abc_Obj_t *> a, const std::set<Abc_Obj_t *> b) const {
      return a.size() <b.size();
    }
};

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


// bool operator < (const std::set<Abc_Obj_t*> a, const std::set<Abc_Obj_t*> b){
//   return a.size() <= b.size();
// }

void Lsv_NtkPrintCuts(Abc_Ntk_t* pNtk, int k_value) {
  Abc_Obj_t* pObj;
  std::map<Abc_Obj_t*, std::set<std::set<Abc_Obj_t*>/*, SortSet*/>> Obj2cut;

  for (int i = 0; (i < Vec_PtrSize((pNtk)->vObjs)) && (((pObj) = Abc_NtkObj(pNtk, i)), 1); i++ ){   \
    if (pObj == NULL)  continue;
    if (!Abc_ObjIsNode(pObj)){
      if (Abc_ObjFaninNum(pObj)) continue;

        // Abc_Print(-2, " * Fan ");
        std::set<Abc_Obj_t*> tmpcut{pObj};
        Obj2cut[pObj].insert(tmpcut);

    }else{
      Abc_Obj_t* pFanin;
      int t;
      std::vector<std::set<std::set<Abc_Obj_t*>>> Inset;
      Abc_ObjForEachFanin(pObj, pFanin, t) {
        Inset.push_back(Obj2cut[pFanin]);
      }
      if(Inset.size()>2){ Abc_Print(-2,"ERROR"); return; }
      else if(Inset.size()==1){
        Abc_Print(-2, Abc_ObjName(pObj));
        Abc_Print(-2, " is special\n");
        Obj2cut[pObj] = Inset[0];
      }else{
        std::set<Abc_Obj_t*> tmpcut{pObj};
        Obj2cut[pObj].insert(tmpcut);
        
        // printf("%s: %d, %d \n", Abc_ObjName(pObj), Inset[0].size(), Inset[1].size());
        for(const auto &s1 : Inset[0]){

          // Abc_Print(-2,"*s1 (");
          // for(const auto &s : s1){
          //   Abc_Print(-2, Abc_ObjName(s));
          //   Abc_Print(-2, ", ");
          // }
          // Abc_Print(-2,")\n");

          for(const auto &s2 : Inset[1]){

            // Abc_Print(-2," s2 (");
            // for(const auto &s : s2){
            //   Abc_Print(-2, Abc_ObjName(s));
            //   Abc_Print(-2, ", ");
            // }
            // Abc_Print(-2,")---");

            std::set<Abc_Obj_t*> tmpcut2;
            tmpcut2.insert(s1.begin(), s1.end());
            tmpcut2.insert(s2.begin(), s2.end());

            // Abc_Print(-2,"add (");
            // for(const auto &s : tmpcut2){
            //   Abc_Print(-2, Abc_ObjName(s));
            //   Abc_Print(-2, ", ");
            // }
            // Abc_Print(-2,")\n");

            if(tmpcut2.size() > k_value) continue;
            else Obj2cut[pObj].insert(tmpcut2);
          }
          // Abc_Print(-2,"\n");
        }

        // for(const auto &cut : Obj2cut[pObj]){
        //   Abc_Print(-2,"(");
        //   for(const auto &s : cut){
        //     Abc_Print(-2, Abc_ObjName(s));
        //     Abc_Print(-2, ", ");
        //   }
        //   Abc_Print(-2,"), ");
        // }
        // Abc_Print(-2, "\n");


        // for(std::set<std::set<Abc_Obj_t*>>::iterator si = Obj2cut[pObj].begin(); si != Obj2cut[pObj].end(); si++){
        //   for (std::set<std::set<Abc_Obj_t*>>::iterator sj = si; sj != Obj2cut[pObj].end(); sj++) {
        //     if(si==sj) continue;
        //     if(std::includes((*sj).begin(), (*sj).end(), (*si).begin(), (*si).end())){
        //       Obj2cut[pObj].erase(*si);
        //     }
        //   }
        // }


        bool swap_tag;
        std::vector<std::set<Abc_Obj_t*>> del;
        // std::reverse_iterator<std::set<std::set<Abc_Obj_t*>>::iterator> s1, s2;
        for(std::reverse_iterator<std::set<std::set<Abc_Obj_t*>>::iterator> si = Obj2cut[pObj].rbegin(); si != Obj2cut[pObj].rend(); si++){
          // Abc_Print(-2,"(");
          // for(const auto &s : *si){
          //   Abc_Print(-2, Abc_ObjName(s));
          //   Abc_Print(-2, ", ");
          // }
          // Abc_Print(-2,"), ");
          for(std::reverse_iterator<std::set<std::set<Abc_Obj_t*>>::iterator> sj = si; sj != Obj2cut[pObj].rend(); sj++) {
            if(si==sj) continue;
            swap_tag = 0;
            if((*si).size()<(*sj).size()){
              std::swap(si, sj);
              swap_tag = 1;
            }
            if(std::includes((*si).begin(), (*si).end(), (*sj).begin(), (*sj).end())){
              // auto k0 = Obj2cut[pObj].find(*si);
              // std::set<Abc_Obj_t*> del;
              // del.insert((*si).begin(), (*si).end());
              del.push_back(*si);

              // auto k = Obj2cut[pObj].erase(*si);
              
              // printf(" %d ", k);
              // Abc_Print(-2,"delete! // ");
              if(swap_tag){
                std::swap(si, sj);
                swap_tag = 0;
              }
              continue;
            }
            if(swap_tag)
                std::swap(si, sj);
          }
        }
        // Abc_Print(-2,"\n");
        for(auto d : del){
          auto k = Obj2cut[pObj].erase(d);
          // printf("delete %d ", k);
        }

        // if(Obj2cut.find(pObj) != Obj2cut.end()){
        //   int num = 0;
        //   for(std::reverse_iterator<std::set<std::set<Abc_Obj_t*>>::iterator> si = Obj2cut[pObj].rbegin(); si != Obj2cut[pObj].rend(); si++){
        //     printf("%d:", Abc_ObjId(pObj));
        //     for(const auto &s : *si){
        //       printf(" %s", Abc_ObjId(s));
        //     }
        //     printf("\n");
        //   }
        // }

      }

      

        // for(int l=0; l<Obj2cut[pObj].size(); l++){
        //   printf("%d: ", Abc_ObjId(pObj));

        // }
        // printf("%d   ", Obj2cut[pObj].size());
        // for(const auto &cut : Obj2cut[pObj]){
        //   Abc_Print(-2,"(");
        //   for(const auto &s : cut){
        //     Abc_Print(-2, Abc_ObjName(s));
        //     Abc_Print(-2, ", ");
        //   }
        //   Abc_Print(-2,"), ");
        // }
        // Abc_Print(-2, "\n");
      /////////////////////////////////////////////////////////////////////////////////
      // if(Obj2cut.find(pObj) != Obj2cut.end()){
      //   int num = 0;
      //   for(std::reverse_iterator<std::set<std::set<Abc_Obj_t*>>::iterator> si = Obj2cut[pObj].rbegin(); si != Obj2cut[pObj].rend(); si++){
      //     printf("%d:", Abc_ObjId(pObj));
      //     for(const auto &s : *si){
      //       printf(" %s", Abc_ObjId(s));
      //     }
      //     printf("\n");
      //   }
      // }
      /////////////////////////////////////////////////////////////////////////////////


    }
  }

  for (int i = 0; (i < Vec_PtrSize((pNtk)->vObjs)) && (((pObj) = Abc_NtkObj(pNtk, i)), 1); i++ ){   
    if (pObj == NULL)  continue;
    if (!Abc_ObjIsNode(pObj)){
      if (Abc_ObjFaninNum(pObj)) continue;
      else if(Obj2cut.find(pObj) != Obj2cut.end()){
            // printf("%d:", Abc_ObjId(pObj));
  //         // int num = 0;
          for(std::reverse_iterator<std::set<std::set<Abc_Obj_t*>>::iterator> si = Obj2cut[pObj].rbegin(); si != Obj2cut[pObj].rend(); si++){
            printf("%d:", Abc_ObjId(pObj));
            for(const auto &s : *si){
              printf(" %d", Abc_ObjId(s));
            }
            printf("\n");
          }
        }
    }else{
      // if (!Abc_ObjFaninNum(pObj)){
        if(Obj2cut.find(pObj) != Obj2cut.end()){
            // printf("%d:", Abc_ObjId(pObj));
  //         // int num = 0;
          for(std::reverse_iterator<std::set<std::set<Abc_Obj_t*>>::iterator> si = Obj2cut[pObj].rbegin(); si != Obj2cut[pObj].rend(); si++){
            printf("%d:", Abc_ObjId(pObj));
            for(const auto &s : *si){
              printf(" %d", Abc_ObjId(s));
            }
            printf("\n");
          }
        }
      // }
    }
  }

  // int tmp;
  // Abc_NtkForEachPo(pNtk, pObj, tmp) {
  //   Abc_Obj_t* pFanin = Abc_ObjFanin(pObj, 0);
  //   std::set<Abc_Obj_t*> tmpcut{pObj};
  //   Obj2cut[pObj].insert(tmpcut);
  //   std::set<Abc_Obj_t*> tmpcut2{pFanin};
  //   Obj2cut[pObj].insert(tmpcut2);
  //   for(const auto &s1 : Obj2cut[pFanin]){
  //     std::set<Abc_Obj_t*> tmpcut3(s1);
  //     if(tmpcut3.size() > k_value) continue;
  //     Obj2cut[pObj].insert(tmpcut3);
  //   }
  // }


  // for (int i = 0; (i < Vec_PtrSize((pNtk)->vObjs)) && (((pObj) = Abc_NtkObj(pNtk, i)), 1); i++ ){   
  //     if (pObj == NULL)  continue;
  //   // Abc_Print(-2, "It's : ");
  //   // if(!Abc_ObjIsNode(pObj)) Abc_Print(-2, "Node "); //printf("It's Node");
  //   // if(!Abc_ObjFaninNum(pObj)) Abc_Print(-2, "Fan ");
      
  //   printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));

  //     if(Obj2cut.find(pObj) != Obj2cut.end()){
  //       printf("%d   ", Obj2cut[pObj].size());
  //       for(const auto &cut : Obj2cut[pObj]){
  //         Abc_Print(-2,"(");
  //         for(const auto &s : cut){
  //           Abc_Print(-2, Abc_ObjName(s));
  //           Abc_Print(-2, ", ");
  //         }
  //         Abc_Print(-2,"), ");
  //       }
  //       Abc_Print(-2, "\n");
  //     }

  //   Abc_Obj_t* pFanin;
  //   int j;
  //   Abc_ObjForEachFanin(pObj, pFanin, j) {
  //     printf("  Fanin-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanin),
  //           Abc_ObjName(pFanin));
  //   }
  //   if (Abc_NtkHasSop(pNtk)) {
  //     printf("The SOP of this node:\n%s", (char*)pObj->pData);
  //   }
  // }
}

int Lsv_CommandPrintCuts(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  // printf("\n%s ", argv[1]);
  int c = std::stoi(argv[1]);
  // printf("\n %d\n\n", c);
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  Lsv_NtkPrintCuts(pNtk, c);
  return 0;

// usage:
//   Abc_Print(-2, "usage: lsv_print_cuts [-h]\n");
//   Abc_Print(-2, "\t        prints the nodes in the network\n");
//   Abc_Print(-2, "\t-h    : print the command usage\n");
//   return 1;
}


void Lsv_NtkPrintNodes(Abc_Ntk_t* pNtk) {
  Abc_Obj_t* pObj;
  int i;
  // Abc_NtkForEachNode(pNtk, pObj, i) {
  for ( i = 0; (i < Vec_PtrSize((pNtk)->vObjs)) && (((pObj) = Abc_NtkObj(pNtk, i)), 1); i++ )   \
        if ( (pObj) == NULL //  ||    !Abc_ObjIsNode(pObj)  
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
  ////////////////////////////////////////////////////////////////////////////////////////
  // printf("\n\n");
  // Abc_NtkForEachPi(pNtk, pObj, i) {
  //   printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
  //   Abc_Obj_t* pFanin;
  //   int j;
  //   Abc_ObjForEachFanin(pObj, pFanin, j) {
  //     printf("  Fanin-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanin),
  //            Abc_ObjName(pFanin));
  //   }
  //   if (Abc_NtkHasSop(pNtk)) {
  //     printf("The SOP of this node:\n%s", (char*)pObj->pData);
  //   }
  // }
  // printf("\n\n");
  // Abc_NtkForEachPo(pNtk, pObj, i) {
  //   printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
  //   Abc_Obj_t* pFanin;
  //   int j;
  //   Abc_ObjForEachFanin(pObj, pFanin, j) {
  //     printf("  Fanin-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanin),
  //            Abc_ObjName(pFanin));
  //   }
  //   if (Abc_NtkHasSop(pNtk)) {
  //     printf("The SOP of this node:\n%s", (char*)pObj->pData);
  //   }
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