#include "base/main/main.h"

ABC_NAMESPACE_IMPL_START

int TestFirst_FirstFunction(Abc_Ntk_t * pNtk);

int TestFirst_FirstFunctionAbc(Abc_Frame_t * pAbc) {

    Abc_Ntk_t * pNtk;
    int result;

    pNtk = Abc_FrameReadNtk(pAbc);

    if (pNtk == NULL) {
        Abc_Print(-1, "TestFirst_FirstFunctionAbc: Getting the Target network has failed.\n");
        return 0;
    }

    result = TestFirst_FirstFunction(pNtk);

    return result;
}

int TestFirst_FirstFunction(Abc_Ntk_t * pNtk) {
    
    if (!Abc_NtkIsStrash(pNtk)) {
        Abc_Print(-1, "TestFirst_FirstFunctionAbc: Network is not strashed.\n");
        return 0;
    }

    Abc_Print(1, "The network with name %s has:\n", Abc_NtkName(pNtk));
    Abc_Print(1, "\t- %d primary inputs;\n", Abc_NtkPiNum(pNtk));
    Abc_Print(1, "\t- %d primary outputs;\n", Abc_NtkPoNum(pNtk));
    Abc_Print(1, "\t- %d AND gates;\n", Abc_NtkNodeNum(pNtk));

    int i;
    Abc_Obj_t * pObj;

    Abc_NtkForEachObj( pNtk, pObj, i )
        Abc_Print(1, "\t- object type: %d \n", Abc_ObjType( pObj ));

    return 1;
}

ABC_NAMESPACE_IMPL_END