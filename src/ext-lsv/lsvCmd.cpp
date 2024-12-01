#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
/*
//static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
//void registercutCommand(Abc_Frame_t* pAbc);
void init(Abc_Frame_t* pAbc) {
  //Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  //registercutCommand(pAbc);
}
*/
int Lsv_CommandSDC(Abc_Frame_t* pAbc, int argc, char** argv);
int Lsv_CommandODC(Abc_Frame_t* pAbc, int argc, char** argv);


void init(Abc_Frame_t* pAbc) {
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sdc", Lsv_CommandSDC, 0);
    Cmd_CommandAdd(pAbc, "LSV", "lsv_odc", Lsv_CommandODC, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

void Lsv_NtkPrintNodes(Abc_Ntk_t* pNtk) {
  Abc_Obj_t* pObj;
  int i;
  printf("Hello world!!!\n");
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


/*
Abc_NtkForEachAnd(): iterate through all and gates
Abc_NtkPoNum(): get the number of primary outputs
Abc_ObjIsPi(): check if an object is PI
Abc_ObjFanin0(): get the first fanin object of the object
*/