#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/cnf/cnf.h"

extern "C"{
Aig_Man_t* Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
}

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandSDC(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandODC(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sdc", Lsv_CommandSDC, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_odc", Lsv_CommandODC, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

Vec_Ptr_t* recursive_count(Abc_Obj_t* pObj, int n) {
  Vec_Ptr_t* res = Vec_PtrAlloc(8);
  int te[1] = {Abc_ObjId(pObj)};
  Vec_Int_t* tte = Vec_IntAllocArrayCopy(te, 1);
  Vec_PtrPush(res, tte);
  if(Abc_AigNodeIsAnd(pObj)){
    Vec_Ptr_t* res0 = recursive_count(Abc_ObjFanin0(pObj), n);
    //printf("\nres0 %d parr: %d\n", Abc_ObjId(Abc_ObjFanin0(pObj)), Vec_PtrSize(res0));
    Vec_Ptr_t* res1 = recursive_count(Abc_ObjFanin1(pObj), n);
    //printf("\nres1%d parr: %d\n", Abc_ObjId(Abc_ObjFanin1(pObj)), Vec_PtrSize(res1));

    for(int i=0; i<Vec_PtrSize(res0); i++) {
      for(int j=0; j<Vec_PtrSize(res1); j++) {
        Vec_Int_t* l0 =(Vec_Int_t*) Vec_PtrEntry(res0, i);
        Vec_Int_t* l1 =(Vec_Int_t*) Vec_PtrEntry(res1, j);
        Vec_Int_t* temp = Vec_IntTwoMerge(l0, l1);
        //printf("\ntemparr: %d\n", Vec_IntSize(temp));
        if(Vec_IntSize(temp) <= n) {
          Vec_PtrPush(res, temp);
        }
      }
    }
    Vec_PtrFree(res0);
    Vec_PtrFree(res1);
  }
  return res;
}

void Lsv_NtkPrintNodes(Abc_Ntk_t* pNtk, int n) {
  Abc_Obj_t* pObj;
  int i;
  Abc_NtkForEachPi(pNtk, pObj, i) {
    printf("%d: %d\n", Abc_ObjId(pObj), Abc_ObjId(pObj));
  }

  int m;
  Abc_NtkForEachNode(pNtk, pObj, m) {
    //printf("Node:  %d", Abc_ObjId(pObj));
    Vec_Ptr_t* kcut = recursive_count(pObj, n);
    for(int i=0; i<Vec_PtrSize(kcut); i++) {
      printf("%d:", Abc_ObjId(pObj));
      Vec_Int_t* kc =(Vec_Int_t*) Vec_PtrEntry(kcut, i);
      for (int j=0; j<Vec_IntSize(kc); j++) {
        printf(" %d", Vec_IntEntry(kc, j));
      }
      printf("\n");
    }
    Vec_PtrFree(kcut);
  }
}

int Sat_SDC(Cnf_Dat_t * f, int n, int id1, int id2) {
  sat_solver* s1 = (sat_solver*) Cnf_DataWriteIntoSolverInt(sat_solver_new(), f, 1, 1);
  int y0 = n / 2;
  int y1 = n % 2;
  int c1[2];
  c1[0] = toLitCond(f->pVarNums[id1], 1-y0);
  c1[1] = toLitCond(f->pVarNums[id2], 1-y1);
  int res = sat_solver_solve(s1, c1, c1+2, 0, 0, 0, 0);
  sat_solver_delete(s1);
  return res;
}

void Lsv_NtkODC(Abc_Ntk_t* pNtk, int n) {
 
}


void Lsv_NtkSDC(Abc_Ntk_t* pNtk, int n) {
  Vec_Ptr_t* inputs = Vec_PtrAlloc(8);
  Abc_Obj_t* pObj = Abc_NtkObj(pNtk, n);
  
  Abc_Obj_t* y0 = Abc_ObjFanin0(pObj);
  Abc_Obj_t* y1 = Abc_ObjFanin1(pObj);
  int yc0 = Abc_ObjFaninC0(pObj);
  int yc1 = Abc_ObjFaninC1(pObj);
  Vec_PtrPush(inputs, y0);
  Vec_PtrPush(inputs, y1);
  int arr[4] = {0, 1, 2, 3};
  Abc_Ntk_t* cone_arr = Abc_NtkCreateConeArray(pNtk, inputs, 1);
  int i1 = Abc_ObjId(Abc_NtkFindNode(cone_arr, Abc_ObjName(y0)));
  int i2 = Abc_ObjId(Abc_NtkFindNode(cone_arr, Abc_ObjName(y1)));
  bool is_empty = true;
  Cnf_Dat_t* f1 = Cnf_Derive(Abc_NtkToDar(cone_arr, 0, 0), 2);
  Vec_Int_t* p0 = Vec_IntAlloc(8);
  Vec_Int_t* p1 = Vec_IntAlloc(8);
  for (int e1: arr) {
    int sat = Sat_SDC(f1, e1, i1, i2);
    if (sat == l_False) {
      is_empty = false;
      if (yc0 == 1) {
        Vec_IntPush(p0, 1-(e1 / 2));
      } else {
        Vec_IntPush(p0, e1 / 2);
      }
       if (yc1 == 1) {
        Vec_IntPush(p1, 1-(e1 / 2));
      } else {
        Vec_IntPush(p1, e1 % 2);
      }
    }
  }
  if (is_empty) {
    printf("%s\n", "no sdc");
  } else {
    printf("%d", Vec_IntEntry(p0, 0));
    printf("%d", Vec_IntEntry(p1, 0));
    for (int j=1; j<Vec_IntSize(p0); j++) {
      printf("%s", " ");
      printf("%d", Vec_IntEntry(p0, j));
      printf("%d", Vec_IntEntry(p1, j));
    }
    printf("\n");
  }
  Vec_PtrFree(inputs);
  Vec_IntFree(p0);
  Vec_IntFree(p1);
}

int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c;
  if (argc != 2) {
    Abc_Print(-1, "Wrong usage\n");
    return 1;
  }
  int n = atoi(argv[1]);
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
  if (!Abc_NtkIsStrash(pNtk)) {
    Abc_Print(-1, "Not yet strash.\n");
    return 1;
  }
  if(n == 0) {
    return 0;
  }
  Lsv_NtkPrintNodes(pNtk, n);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_print [n]\n");
  Abc_Print(-2, "\t        prints the k-feasible cuts in the network\n");
  return 1;
}

int Lsv_CommandSDC(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c;
  if (argc != 2) {
    Abc_Print(-1, "Wrong usage\n");
    return 1;
  }
  int n = atoi(argv[1]);
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
  if (!Abc_NtkIsStrash(pNtk)) {
    Abc_Print(-1, "Not yet strash.\n");
    return 1;
  }
  if(!Abc_ObjIsNode(Abc_NtkObj(pNtk, n))) {
    Abc_Print(-1, "Wrong node number.\n");
    return 1;
  }
  Lsv_NtkSDC(pNtk, n);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sdc [n]\n");
  Abc_Print(-2, "\t   print sdc\n");
  return 1;
}

int Lsv_CommandODC(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c;
  if (argc != 2) {
    Abc_Print(-1, "Wrong usage\n");
    return 1;
  }
  int n = atoi(argv[1]);
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
  if (!Abc_NtkIsStrash(pNtk)) {
    Abc_Print(-1, "Not yet strash.\n");
    return 1;
  }
  if(!Abc_ObjIsNode(Abc_NtkObj(pNtk, n))) {
    Abc_Print(-1, "Wrong node number.\n");
    return 1;
  }
  Lsv_NtkODC(pNtk, n);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_odc [n]\n");
  Abc_Print(-2, "\t   print odc\n");
  return 1;
}