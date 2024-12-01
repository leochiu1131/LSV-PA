#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/cnf/cnf.h"
#include <vector>
#include <string>
#include <map>
#include <iostream>
#include <fstream>
#include <unordered_set>
#include <cstdlib>
#include <random>
#include <ctime>

extern "C"{
  Aig_Man_t* Abc_NtkToDar(Abc_Ntk_t* pNtk, int fExors, int fRegisters);
}

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandCutEnum(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandComputeSDC(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandComputeODC(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSDC(Abc_Frame_t* pAbc, int argc, char** argv);


void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_CommandCutEnum, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sdc", Lsv_CommandComputeSDC, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_odc", Lsv_CommandComputeODC, 0);
  Cmd_CommandAdd(pAbc, "LSV", "sdc", Lsv_CommandSDC, 0);
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

void Lsv_NtkPrintCut(Abc_Ntk_t* pNtk, int i){
  Abc_Obj_t* pObj;
  int j;

  std::vector<std::vector<int> > cuts;
  std::map<int, std::vector<std::vector<int> > > cut_list;

  Abc_NtkForEachPi(pNtk, pObj, j){
    int id = Abc_ObjId(pObj);
    printf("%d: %d\n", id, id);
  }
  j=0;

  Abc_NtkForEachNode(pNtk, pObj, j){
    int id = Abc_ObjId(pObj);
    //string name = Abc_ObjName(pObj);
    int fanin0 = Abc_ObjId(Abc_ObjFanin0(pObj));
    int fanin1 = Abc_ObjId(Abc_ObjFanin1(pObj));

    for(auto &s1 : cut_list[fanin0]){
      for(auto &s2 : cut_list[fanin1]){
        std::vector<int> cut = s1;
        for(auto &s3 : s2){
          cut.push_back(s3);
        }
        //if(cut.size() > k)
        //  continue;

        
      }
    }
  }
}

int Lsv_CommandCutEnum(Abc_Frame_t* pAbc, int argc, char** argv){
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  char* c = argv[1];
  int i = std::atoi(c);

  if(!pNtk){
    Abc_Print(-1, "Empty network.\n");
    return -1; 
  }
  if(i == 0){
    Abc_Print(-1, "Invalid k.\n");
    return -1; 
  }
  Lsv_NtkPrintCut(pNtk,i);
  return 0;
}

//Compute SDC
std::unordered_set<int> RandomSimulation(Abc_Ntk_t *pNtk, int i, int pattern_num){
  srand(time(NULL));
  std::unordered_set<int> observe_patterns;
  Abc_Obj_t* pTarget = Abc_NtkObj(pNtk, i);
  
  Abc_Obj_t* pTarget_fanin0 = Abc_ObjFanin0(pTarget);
  Abc_Obj_t* pTarget_fanin1 = Abc_ObjFanin1(pTarget);
  
  for(int j = 0; j < pattern_num; ++j){
    Abc_Obj_t* pObj;
    int k;
    Abc_NtkForEachPi(pNtk, pObj, k){
      pObj -> pData = (void*) (rand() % 2);
    }

    Abc_NtkForEachNode(pNtk, pObj, k){
      int v0 = (int) (intptr_t) Abc_ObjFanin0(pObj) -> pData;
      int v1 = (int) (intptr_t) Abc_ObjFanin1(pObj) -> pData;
      
      if(Abc_ObjFaninC0(pObj)){
        v0 = 1 - v0;
      }
      if(Abc_ObjFaninC1(pObj)){
        v1 = 1 - v1;
      }

      int value = v0 & v1;
      pObj -> pData = (void*) (intptr_t) value;
    }

    int y0 = (int) (intptr_t) pTarget_fanin0 -> pData;
    int y1 = (int) (intptr_t) pTarget_fanin1 -> pData;

    if(Abc_ObjFaninC0(pTarget)){
      y0 = 1 - y0;
    }
    if(Abc_ObjFaninC1(pTarget)){
      y1 = 1 - y1;
    }
    int observepattern = 2 * y0 + y1;
    observe_patterns.insert(observepattern);
  }

  return observe_patterns;
}

bool SATbaseCheck(Abc_Ntk_t* pNtk, int i, int pattern){
  Abc_Obj_t* pTarget = Abc_NtkObj(pNtk,i);
  //std::cout << "fanin: " << Abc_ObjId(Abc_ObjFanin0(pTarget)) << " " << Abc_ObjId(Abc_ObjFanin1(pTarget)) << "\n";
  int v0 = pattern / 2;
  int v1 = pattern % 2;

  Abc_Ntk_t* pConeNtk = Abc_NtkCreateCone(pNtk, pTarget, "cone", 0);
  Aig_Man_t* pAig = Abc_NtkToDar(pConeNtk, 0, 0);
  Cnf_Dat_t* pCnf = Cnf_Derive(pAig, Abc_NtkPoNum(pConeNtk)); //nOutputs
  sat_solver* pSat = sat_solver_new();
  Cnf_DataWriteIntoSolverInt(pSat, pCnf, 1, 0);

  int var0 = pCnf -> pVarNums[Aig_ObjId(Aig_ManCi(pAig, 0))];
  int var1 = pCnf -> pVarNums[Aig_ObjId(Aig_ManCi(pAig, 1))];
  int assumptions[2] = {Abc_Var2Lit(var0, v0), Abc_Var2Lit(var1, v1)};

  int status = sat_solver_solve(pSat, assumptions, assumptions + 2, 100, 100, 100, 100);
  sat_solver_delete(pSat);
  Cnf_DataFree(pCnf);
  Aig_ManStop(pAig);
  Abc_NtkDelete(pConeNtk);

  return (status == l_True);
}

void Lsv_NtkComputeSDC(Abc_Ntk_t* pNtk, int i){
  int num_pattern = 5000;
  std::unordered_set<int> patterns;
  patterns = RandomSimulation(pNtk, i, num_pattern);
  

  std::vector<int> allPatterns = {0,1,2,3};
  std::vector<int> missingPatterns;
  for(int p : allPatterns){
    if(patterns.find(p) == patterns.end()){
      missingPatterns.push_back(p);
    }
  }

  bool flag = 0;
  for(int p : missingPatterns){
    bool isUNSAT = SATbaseCheck(pNtk, i, p);
    //std::cout << "isUNSAT: " << isUNSAT << "\n";
    if(isUNSAT){
      int v0 = p / 2;
      int v1 = p % 2;
      std::cout << v0 << v1 << "\n";
      flag = 1;
    }
  }
  if(!flag){
    std::cout << "no sdc\n";
  }
}

int Lsv_CommandComputeSDC(Abc_Frame_t* pAbc, int argc, char** argv){
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  char* c = argv[1];
  int i = std::atoi(c);

  if(!pNtk){
    Abc_Print(-1, "Empty network.\n");
    return -1; 
  }
/**/
  Abc_Obj_t* pObj;
  int j;
  Abc_NtkForEachPi(pNtk, pObj, j){
    int id = Abc_ObjId(pObj);
    if(i == id){
      Abc_Print(-1, "Primary input is not internal node.\n");
      return -1; 
    }
  }
  Abc_NtkForEachPo(pNtk, pObj, j){
    int id = Abc_ObjId(pObj);
    if(i == id){
      Abc_Print(-1, "Primary output is not internal node.\n");
      return -1; 
    }
  }

  Lsv_NtkComputeSDC(pNtk,i);
  return 0;
}
//

//Compute ODC

std::unordered_set<int> Lsv_NodeSDC(Abc_Ntk_t* pNtk, int i){
  std::unordered_set<int> o;
  int num_pattern = 5000;
  std::unordered_set<int> patterns;
  patterns = RandomSimulation(pNtk, i, num_pattern);
  

  std::vector<int> allPatterns = {0,1,2,3};
  std::vector<int> missingPatterns;
  for(int p : allPatterns){
    if(patterns.find(p) == patterns.end()){
      missingPatterns.push_back(p);
    }
  }

  //bool flag = 0;
  for(int p : missingPatterns){
    bool isUNSAT = SATbaseCheck(pNtk, i, p);
    //std::cout << "isUNSAT: " << isUNSAT << "\n";
    if(isUNSAT){
      o.insert(p);
      int v0 = p / 2;
      int v1 = p % 2;
      //std::cout << v0 << v1 << "\n";
      //flag = 1;
    }
  }
  return o;
}

void Lsv_NtkComputeODC(Abc_Ntk_t* pNtk, int k){
  //std::cout << "Compute ODC.\n";
  Abc_Ntk_t *pNegNtk = Abc_NtkDup(pNtk);
    Abc_Obj_t *pCompNode = Abc_NtkObj(pNegNtk, k);
    Abc_Obj_t *pFanout;
    Abc_Obj_t *pNode = Abc_NtkObj(pNtk,k);

    int i;
    Abc_ObjForEachFanout(pCompNode, pFanout, i){
        Abc_Obj_t *pFanin;
        int j;
        Abc_ObjForEachFanin(pFanout, pFanin, j){
            if (pFanin == pCompNode){
                Abc_ObjXorFaninC(pFanout, j);
            }
        }
    }

    Abc_Ntk_t *pMiterNtk = Abc_NtkMiter(pNtk, pNegNtk, 1, 0, 0, 0);
    Abc_Obj_t *pFanin0 = Abc_ObjFanin0(pNode);
    Abc_Obj_t *pFanin1 = Abc_ObjFanin1(pNode);

    Vec_Ptr_t *vfanin = Vec_PtrAlloc(2);
    Vec_PtrPush(vfanin, pFanin0);
    Vec_PtrPush(vfanin, pFanin1);
    Abc_Ntk_t *pConeNtk = Abc_NtkCreateConeArray(pNtk, vfanin, 1);

    Abc_NtkAppend(pMiterNtk, pConeNtk, 1);
    Aig_Man_t *pAig = Abc_NtkToDar(pMiterNtk, 0, 0);

    sat_solver *pSat = sat_solver_new();
    Cnf_Dat_t *pCnf = Cnf_Derive(pAig, Abc_NtkPoNum(pMiterNtk));
    Cnf_DataWriteIntoSolverInt(pSat, pCnf, 1, 0);
    int var1 = pCnf->pVarNums[Aig_ManCo(pAig, 0)->Id];
    int var2 = pCnf->pVarNums[Aig_ManCo(pAig, 1)->Id];
    int var3 = pCnf->pVarNums[Aig_ManCo(pAig, 2)->Id];

    lit assumption[1];
    assumption[0] = Abc_Var2Lit(var1, 0);
    sat_solver_addclause(pSat, assumption, assumption + 1);
    int status = sat_solver_solve(pSat, NULL, NULL, 0, 0, 0, 0);

    std::vector<int> dontcare;
    dontcare.resize(4);
    for(int x = 0;x < dontcare.size();++x){
      dontcare[x] = 0;
    }
  /*
    for(int x = 0;x < dontcare.size();++x){
      std::cout << dontcare[x] << "\n";
    }*/

    while (status == l_True){
        int value1 = sat_solver_var_value(pSat, var2);
        int value2 = sat_solver_var_value(pSat, var3);
        int assumptions[2] = {Abc_Var2Lit(var2, value1), Abc_Var2Lit(var3, value2)};
        int v0 = value1, v1 = value2;
        if (Abc_ObjFaninC0(pNode))
            v0 = 1-v0;
        if (Abc_ObjFaninC1(pNode))
            v1 = 1-v1;

        dontcare[2*v0 + v1] = 1;
        if (!sat_solver_addclause(pSat, assumptions, assumptions + 2)){
          break;
        }
            
        status = sat_solver_solve(pSat, NULL, NULL, 0, 0, 0, 0);
    }

 /*
    for(int x = 0;x < dontcare.size();++x){
      std::cout << dontcare[x] << "\n";
    }*/

    bool flag = false;
    for (int x = 0; x < dontcare.size(); x++)
    {
        if (dontcare[x] == 0)
        {
            int first = x / 2;
            int second = x % 2;
            std::unordered_set<int> sdc_set;
            sdc_set = Lsv_NodeSDC(pNtk,k); 
            if (sdc_set.find(x) == sdc_set.end())
            {
                std::cout << first << second << " ";
                flag = true;
            }
        }
    }
    // cout << count;
    if (!flag)
        std::cout << "no odc";
    std::cout << std::endl;
}

int Lsv_CommandComputeODC(Abc_Frame_t* pAbc, int argc, char** argv){
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  char* c = argv[1];
  int i = std::atoi(c);

  if(!pNtk){
    Abc_Print(-1, "Empty network.\n");
    return -1; 
  }

  Abc_Obj_t* pObj;
  int j;
  Abc_NtkForEachPi(pNtk, pObj, j){
    int id = Abc_ObjId(pObj);
    if(i == id){
      Abc_Print(-1, "Primary input is not internal node.\n");
      return -1; 
    }
  }
  Abc_NtkForEachPo(pNtk, pObj, j){
    int id = Abc_ObjId(pObj);
    if(i == id){
      Abc_Print(-1, "Primary output is not internal node.\n");
      return -1; 
    }
  }

  Lsv_NtkComputeODC(pNtk,i);
  return 0;
}


void ComputeSDC(Abc_Ntk_t* pNtk, std::ofstream& fout){
  //std::cout << "Compute ODC.\n";
  int i, cnt;
  int tcnt = 0;
  Abc_Obj_t* pObj;
  Abc_NtkForEachNode(pNtk, pObj, i){
    int num_pattern = 5000;
    cnt = 0;
    /*if(Abc_ObjIsPi(Abc_ObjFanin0(pObj)) || Abc_ObjIsPi(Abc_ObjFanin1(pObj))){
      continue;
    }*/

    std::unordered_set<int> patterns;
    patterns = RandomSimulation(pNtk, i, num_pattern);
    

    std::vector<int> allPatterns = {0,1,2,3};
    std::vector<int> missingPatterns;
    for(int p : allPatterns){
      if(patterns.find(p) == patterns.end()){
        missingPatterns.push_back(p);
      }
    }

    //bool flag = 0;
    std::cout << "node: " << i << "\n";
    fout << "node: " << i << "\n";
    for(int p : missingPatterns){
      bool isUNSAT = SATbaseCheck(pNtk, i, p);
      if(isUNSAT){
        int v0 = p / 2;
        int v1 = p % 2;
        std::cout << v0 << v1 << "\n";
        fout << v0 << v1 << "\n";
        //flag = 1;
        cnt++;
        
      }
    }
    //std::cout << "Total don't cares: " << cnt << "\n";
    tcnt += cnt;
    //missingPatterns.clear();
  }
  std::cout << "Ntk total don't care: " << tcnt << "\n";
  fout << "Ntk total don't care: " << tcnt << "\n";
}

//total dc in the circuit
int Lsv_CommandSDC(Abc_Frame_t* pAbc, int argc, char** argv){
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  std::ofstream fout;
  fout.open(argv[1]);
  //char* c = argv[1];
  //int i = std::atoi(c);

  if(!pNtk){
    Abc_Print(-1, "Empty network.\n");
    return -1; 
  }
/*
  Abc_Obj_t* pObj;
  int j;
  Abc_NtkForEachPi(pNtk, pObj, j){
    int id = Abc_ObjId(pObj);
    if(i == id){
      Abc_Print(-1, "Primary input is not internal node.\n");
      return -1; 
    }
  }
  Abc_NtkForEachPo(pNtk, pObj, j){
    int id = Abc_ObjId(pObj);
    if(i == id){
      Abc_Print(-1, "Primary output is not internal node.\n");
      return -1; 
    }
  }
*/
  ComputeSDC(pNtk,fout);
  return 0;
}

