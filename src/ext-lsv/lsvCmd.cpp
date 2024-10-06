#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"


static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_CommandPrintNodes, 0);
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
/**/


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