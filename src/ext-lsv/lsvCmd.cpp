#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <vector>
using namespace std;

static int Lsv_CommandPrintCut(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_CommandPrintCut, 0); 
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

//int Lsv_FindCut

int Lsv_CommandPrintCut(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  
  
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  if(argc != 2){
    printf("Wrong input command!\n");
    return 1;
  }
  
  if(argv[1][1] != '\0'){
    printf("Wrong input command!\n");
    return 1;
  }

  if((int)argv[1][0]-'0' > 6 || (int)argv[1][0]-'0' < 3){
    printf("Wrong input command!\n");
    return 1;
  }
  int CutNumMax = (int)argv[1][0]-'0';
  
  vector <int> CutNum(Abc_NtkObjNum(pNtk) + 1, -1);
  Abc_Obj_t* pObj, pObj2;
  vector<vector<vector<int>>>  cut(Abc_NtkObjNum(pNtk) + 1);
  vector<int> CutForm(Abc_NtkObjNum(pNtk) + 1);
  int i, OutputStartCheck = 0, OutputEndCheck = 0,pOutputStart,pOutputEnd;
  
  Abc_NtkForEachObj(pNtk, pObj, i) {
    cut[i].clear();
    
    int j, FaninCheck = 0;
    
    Abc_Obj_t* pFanin;
    if(Abc_ObjFaninNum(pObj) == 0 && Abc_ObjFanoutNum(pObj) > 0){
      CutNum[i] = 0; 
     
      cut[i].push_back({int(Abc_ObjId(pObj))});
    }
    
    else if(i > 0 && Abc_ObjFanoutNum(pObj) == 0 && OutputStartCheck == 0){
      pOutputStart = Abc_ObjId(pObj);
      OutputStartCheck = 1;
    }

    else if(Abc_ObjFanoutNum(pObj) > 0 && OutputEndCheck == 0){
      
      
      OutputEndCheck = 1;
      pOutputEnd = Abc_ObjId(pObj);
      i--;
    }
    else if(Abc_ObjFanoutNum(pObj) > 0 && OutputEndCheck == 1){
      vector<int> tempcut;
      tempcut.clear();
      tempcut.push_back(Abc_ObjId(pObj));
      cut[Abc_ObjId(pObj)].push_back(tempcut);
      
      vector<int> FaninCutNum;
      vector<int> FaninCutMax;
      FaninCutNum.resize(Abc_ObjFaninNum(pObj));
      FaninCutMax.resize(Abc_ObjFaninNum(pObj));
      Abc_ObjForEachFanin(pObj, pFanin, j){
        FaninCutMax[j] = CutNum[Abc_ObjId(pFanin)] - 1;
      }
      
      int CutCount = 0, k, CutNodeNum;
      while(1){
        
        for(j = 0; j <= Abc_NtkObjNum(pNtk); j++){
          CutForm[j] = 0;
        }
        Abc_ObjForEachFanin(pObj, pFanin, j){
          for(k = 0; k < cut[Abc_ObjId(pFanin)][FaninCutNum[j]].size(); k++){
            CutForm[cut[Abc_ObjId(pFanin)][FaninCutNum[j]][k]] = 1;
          }         
        }
        CutNodeNum = 0;
        tempcut.clear();
        for(j = 0; j < Abc_NtkObjNum(pNtk); j++){
          if(CutForm[j] == 1){
            CutNodeNum++;
            tempcut.push_back(j);
            if(CutNodeNum > CutNumMax){
              break;
            }
          }
        }
        if(!tempcut.empty() && CutNodeNum <= CutNumMax){
          int SameCutCheck = 0, k = 0, m = 0, CutCompareLarge = 0, CutCompareSmall = 0;
          for(j = 0; j < cut[Abc_ObjId(pObj)].size(); j++){
            k = 0;
            m = 0;
            CutCompareLarge = 0;
            CutCompareSmall = 0;
            if(j > 0){
              
              m = 0;
              for(k = 0; k < tempcut.size(); k++){
                while(m < cut[Abc_ObjId(pObj)][j].size() && cut[Abc_ObjId(pObj)][j][m] < tempcut[k]){
                  m++;
                }
                if(cut[Abc_ObjId(pObj)][j][m] > tempcut[k]){
                  CutCompareSmall = 0;
                  break;
                }
                
                else if(cut[Abc_ObjId(pObj)][j][m] == tempcut[k]){
                  CutCompareSmall++;
                  if(m + 1 < cut[Abc_ObjId(pObj)][j].size()){
                    m++;
                  }
                  else{
                    break;
                  }
                                    
                }
                
                
              }
              k = 0;
              for(m = 0; m < cut[Abc_ObjId(pObj)][j].size(); m++){
                while(k + 1 < tempcut.size() && cut[Abc_ObjId(pObj)][j][m] > tempcut[k]){
                  k++;
                }
                if(cut[Abc_ObjId(pObj)][j][m] < tempcut[k]){
                  CutCompareLarge = 0;
                  break;
                }
                else if(cut[Abc_ObjId(pObj)][j][m] == tempcut[k]){
                  CutCompareLarge++;
                  if(k + 1 < tempcut.size()){
                    k++;
                  }
                  else{
                    break;
                  }
                               
                }
                
                
              }
              if(CutCompareLarge == cut[Abc_ObjId(pObj)][j].size()){
                SameCutCheck = 1;
                break;
              }
              else if(CutCompareLarge != cut[Abc_ObjId(pObj)][j].size() && CutCompareSmall == tempcut.size()){
                cut[Abc_ObjId(pObj)].erase(cut[Abc_ObjId(pObj)].begin() + j);
                j--;
                CutCount--;
              }
            }
          }
          if(SameCutCheck == 0){
            
            CutCount++;
            cut[Abc_ObjId(pObj)].push_back(tempcut);
          }
          
        }
        int FindCutFinish = 0;
        Abc_ObjForEachFanin(pObj, pFanin, j){
          if(FaninCutNum[j] < FaninCutMax[j]){
            FaninCutNum[j]++;
            FindCutFinish = 1;
            break;
          }
          else{
            FaninCutNum[j] = 0;
          }
        }
        if(FindCutFinish == 0){
          break;
        }
      }
      CutNum[pObj->Id] = CutCount + 1;
    }
    
    for(j = 0; j < cut[Abc_ObjId(pObj)].size(); j++){
      printf("%d:", Abc_ObjId(pObj));
      for(int k = 0; k < cut[Abc_ObjId(pObj)][j].size(); k++){
        printf(" %d", cut[Abc_ObjId(pObj)][j][k]);
      }
      printf("\n");
    }
    
  }
  for(i = pOutputStart; i < pOutputEnd; i++){
    cut[i].clear();
    int j, FaninCheck = 0;
    
    Abc_Obj_t* pFanin;
    pObj = Abc_NtkObj(pNtk, i);
    
    vector<int> tempcut;
    tempcut.clear();
    tempcut.push_back(Abc_ObjId(pObj));
    cut[Abc_ObjId(pObj)].push_back(tempcut);
    
    vector<int> FaninCutNum;
    vector<int> FaninCutMax;
    FaninCutNum.resize(Abc_ObjFaninNum(pObj));
    FaninCutMax.resize(Abc_ObjFaninNum(pObj));
    Abc_ObjForEachFanin(pObj, pFanin, j){
      FaninCutMax[j] = CutNum[Abc_ObjId(pFanin)] - 1;
    }
    
    int CutCount = 0, k, CutNodeNum;
    while(1){
      
      for(j = 0; j <= Abc_NtkObjNum(pNtk); j++){
        CutForm[j] = 0;
      }
      Abc_ObjForEachFanin(pObj, pFanin, j){
        for(k = 0; k < cut[Abc_ObjId(pFanin)][FaninCutNum[j]].size(); k++){
          CutForm[cut[Abc_ObjId(pFanin)][FaninCutNum[j]][k]] = 1;
        }         
      }
      CutNodeNum = 0;
      tempcut.clear();
      for(j = 0; j < Abc_NtkObjNum(pNtk); j++){
        if(CutForm[j] == 1){
          CutNodeNum++;
          tempcut.push_back(j);
          if(CutNodeNum > CutNumMax){
            break;
          }
        }
      }
      if(!tempcut.empty() && CutNodeNum <= CutNumMax){
        int SameCutCheck = 0, k = 0, m = 0, CutCompareLarge = 1, CutCompareSmall = 1;

        

        for(j = 0; j < cut[Abc_ObjId(pObj)].size(); j++){
          k = 0;
          m = 0;
          CutCompareLarge = 0;
          CutCompareSmall = 0;
          if(j > 0){
            m = 0;
            for(k = 0; k < tempcut.size(); k++){
                while(m < cut[Abc_ObjId(pObj)][j].size() && cut[Abc_ObjId(pObj)][j][m] < tempcut[k]){
                  m++;
                }
                if(cut[Abc_ObjId(pObj)][j][m] > tempcut[k]){
                  CutCompareSmall = 0;
                  break;
                }
                
                else if(cut[Abc_ObjId(pObj)][j][m] == tempcut[k]){
                  CutCompareSmall++;
                  if(m + 1 < cut[Abc_ObjId(pObj)][j].size()){
                    m++;
                  }
                  else{
                    break;
                  }
                                    
                }
                
                
              }
              k = 0;
              for(m = 0; m < cut[Abc_ObjId(pObj)][j].size(); m++){
                while(k + 1 < tempcut.size() && cut[Abc_ObjId(pObj)][j][m] > tempcut[k]){
                  k++;
                }
                if(cut[Abc_ObjId(pObj)][j][m] < tempcut[k]){
                  CutCompareLarge = 0;
                  break;
                }
                else if(cut[Abc_ObjId(pObj)][j][m] == tempcut[k]){
                  CutCompareLarge++;
                  if(k + 1 < tempcut.size()){
                    k++;
                  }
                  else{
                    break;
                  }
                               
                }
                
                
              }
              if(CutCompareLarge == cut[Abc_ObjId(pObj)][j].size()){
                SameCutCheck = 1;
                break;
              }
              else if(CutCompareLarge != cut[Abc_ObjId(pObj)][j].size() && CutCompareSmall == tempcut.size()){
                cut[Abc_ObjId(pObj)].erase(cut[Abc_ObjId(pObj)].begin() + j);
                j--;
                CutCount--;
              }
          }
        }
        if(SameCutCheck == 0){
          
          CutCount++;
          cut[Abc_ObjId(pObj)].push_back(tempcut);
        }
        
      }
      int FindCutFinish = 0;
      Abc_ObjForEachFanin(pObj, pFanin, j){
        if(FaninCutNum[j] < FaninCutMax[j]){
          FaninCutNum[j]++;
          FindCutFinish = 1;
          break;
        }
        else{
          FaninCutNum[j] = 0;
        }
      }
      if(FindCutFinish == 0){
        break;
      }
    }
    CutNum[pObj->Id] = CutCount + 1;
    for(j = 0; j < cut[Abc_ObjId(pObj)].size(); j++){
      printf("%d:", Abc_ObjId(pObj));
      for(int k = 0; k < cut[Abc_ObjId(pObj)][j].size(); k++){
        printf(" %d", cut[Abc_ObjId(pObj)][j][k]);
      }
      printf("\n");
    }
  }
  return 0;

}

