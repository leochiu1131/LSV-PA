#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <vector>
#include <string>
#include <fstream>

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandLsvSimBdd( Abc_Frame_t * pAbc, int argc, char ** argv );
static int Lsv_CommandLsvSimAig( Abc_Frame_t * pAbc, int argc, char ** argv );
static void Lsv_AigCalcNodeValueRecur(Abc_Obj_t* node, std::vector<std::vector<unsigned int>>& objValuations, std::vector<bool>& objInitialized, int inputLength);
// checks if is constant node 
// abc_NodeIsConst has assertion error
static inline int Lsv_IsConst(Abc_Obj_t* pNode) { return !Abc_ObjIsPi(pNode) && Abc_ObjFaninNum(pNode) == 0 && Abc_ObjFanoutNum(pNode) > 0 ;}

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", Lsv_CommandLsvSimBdd, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig", Lsv_CommandLsvSimAig, 0);
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


// my commands

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Lsv_CommandLsvSimBdd( Abc_Frame_t * pAbc, int argc, char ** argv )
{
    // get input
    char* pInputSeq;
    int c = 0;
    int i, j;
    int piNum;
    Abc_Obj_t* pPo, * pPi;
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    std::vector<std::string> piNames;
    std::vector<bool> inputSeq;

    Extra_UtilGetoptReset();
    while ( ( c = Extra_UtilGetopt( argc, argv, "h" ) ) != EOF )
    {
        switch ( c )
        {
            case 'h':
                goto usage;
            default:
                goto usage;
        }
    }
    if (!Abc_NtkIsBddLogic(pNtk))
    {
        Abc_Print( -1, "Convert to BDD first.\n");
        return 1;
    }
    if (argc != globalUtilOptind + 1){
        Abc_Print( -1, "Wrong number of arguments.\n");
        return 1;
    }

    pInputSeq = argv[globalUtilOptind];
    if (strlen(pInputSeq) != Abc_NtkPiNum(pNtk))
    {
        Abc_Print(-1, "Input size does not match.\n");
        return 1;
    }

#ifdef ABC_USE_CUDD
    // record PI names
    piNum = Abc_NtkPiNum(pNtk);
    Abc_NtkForEachPi(pNtk, pPi, i)
    {
        assert( Abc_NtkIsBddLogic(pPi->pNtk) );
        piNames.push_back(std::string(Abc_ObjName(pPi)));
        if (pInputSeq[i] == '0')
        {
            inputSeq.push_back(false);
        }
        else if (pInputSeq[i] == '1') 
        {
            inputSeq.push_back(true);
        }
        else {
            Abc_Print(-1, "Only 0 or 1 allowed.\n");
            return 1;
        }
    }

    // for (i = 0; i < inputSeq.size(); i++){
    //   printf("%s ", piNames[i].c_str());
    //     if (inputSeq[i]){
    //         printf("1\t");
    //     }
    //     else {
    //         printf("0\t");
    //     }
    // }
    // printf("\n");

    Abc_NtkForEachPo(pNtk, pPo, i) 
    {
        Abc_Obj_t* pPoBdd = Abc_ObjFanin0(pPo);
        // Abc_Print(1, "pPo: %s\npPoBdd: %s\n", Abc_ObjName(pPo), Abc_ObjName(pPoBdd));
         
        assert( Abc_NtkIsBddLogic(pPoBdd->pNtk) );
        DdManager * dd = (DdManager *)pPoBdd->pNtk->pManFunc;  
        DdNode* poDdNode = (DdNode *)pPoBdd->pData;
        
        Vec_Int_t* pFaninIds = Abc_ObjFaninVec(pPoBdd);
        // printf("POOO: %s: ", Abc_ObjName(pPo));
        // for (j = 0; j < pFaninIds->nSize; j++){
        //     printf("%d ", pFaninIds->pArray[j]);
        // }
        // printf("\n");


        DdNode* cof = poDdNode;
        for (j = 0; j < pFaninIds->nSize; j++){
            int inputIndex = pFaninIds->pArray[j];
            DdNode* inputDdNode = Cudd_bddIthVar(dd, j);

            // printf("faninId %d:", inputIndex);
            // Cudd_PrintDebug(dd, inputDdNode, 4, 4);
            // printf("cof %d:", j);
            // Cudd_PrintDebug(dd, cof, 4, 4);
            // printf("inputindex:%d \n", inputIndex);
            cof = inputSeq[inputIndex-1] ? Cudd_Cofactor(dd, cof, inputDdNode) : Cudd_Cofactor(dd, cof, Cudd_Not(inputDdNode));
        }
        
        
        // Cudd_PrintDebug(dd, cof, 4, 4);
        if (cof == Cudd_ReadOne(dd))
        {
            Abc_Print(1, "%s: %d\n", Abc_ObjName(pPo), 1);
        }
        else if (cof == Cudd_Not(Cudd_ReadOne(dd)))
        {
            Abc_Print(1, "%s: %d\n", Abc_ObjName(pPo), 0);
        }
        else {
            Abc_Print(1, "LMAO idiot\n");
        }
    }

#endif
    
    return 0;
usage:
    Abc_Print( -2, "usage: lsv_bdd_sim [-h] <input_pattern>\n" );
    Abc_Print( -2, "\t        Function simulation using BDD\n" );
    Abc_Print( -2, "\t-h    : print the command usage\n");
    return 1;
}

int Lsv_CommandLsvSimAig( Abc_Frame_t * pAbc, int argc, char ** argv )
{
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    char* pFileName;
    std::ifstream fin;
    std::string inputLineStr;
    int inputLength;
    int piNum = Abc_NtkPiNum(pNtk);
    std::vector<std::vector<unsigned int>> inputVarValuations;
    std::vector<std::vector<unsigned int>> objValuations;
    std::vector<bool> objInitialized;
    int c = 0;
    int i, j, k, l;
    Abc_Obj_t* pPo, * pPi, * pObj;
    
    std::vector<std::string> piNames;
    std::vector<bool> inputSeq;

    Extra_UtilGetoptReset();
    while ( ( c = Extra_UtilGetopt( argc, argv, "h" ) ) != EOF )
    {
        switch ( c )
        {
            case 'h':
                goto usage;
            default:
                goto usage;
        }
    }
    if (!Abc_NtkIsStrash(pNtk))
    {
        Abc_Print( -1, "Convert to AIG first.\n");
        return 1;
    }
    if (argc != globalUtilOptind + 1){
        Abc_Print( -1, "Wrong number of arguments.\n");
        return 1;
    }

    pFileName = argv[globalUtilOptind];
    fin = std::ifstream(pFileName, std::ifstream::in);
    if (!fin.is_open()){
        Abc_Print( -1, "Error opening file.\n");
        return 1;
    }

    // initialize input variable vector
    inputVarValuations = std::vector<std::vector<unsigned int>>(piNum, std::vector<unsigned int>(1,0));

    inputLength = 0;
    while (fin >> inputLineStr){
        // if (lineNum > 0){
        //     if (inputLineStr.length() != inputLength){
        //         Abc_Print(-1, "Input variable number mismatch.\n");
        //         return 1;
        //     }
        // }
        // else {
        //     inputLength = inputLineStr.length();
        // }

        if (inputLineStr.length() != piNum){
            Abc_Print(-1, "Input variable number mismatch.\n");
            return 1;
        }

        if (inputLength % 32 == 0 && inputLength != 0){
            for (i = 0; i < piNum; i++){
                inputVarValuations[i].push_back(0);
            }
        }

        int numIndex = inputLength / 32;
        int shift = inputLength % 32;
        for (int s = 0; s < inputLineStr.length(); s++){
            if (inputLineStr[s] == '0'){
                // do nothing
            }
            else if (inputLineStr[s] == '1'){
                inputVarValuations[s][numIndex] = inputVarValuations[s][numIndex] | (1 << shift);
            }
            else {
                Abc_Print(-1, "Only 0 or 1 allowed.\n");
                return 1;
            }
        }
        inputLength++;

        // printf("%s\n", inputLineStr.c_str());
    }

    assert(inputVarValuations.size() == piNum);
    for (i = 1; i < inputVarValuations.size(); i++){
        assert(inputVarValuations[i].size() == inputVarValuations[i-1].size());
    }
    
    // for (i = 0; i < inputVarValuations.size(); i++){
    //     for (j = 0; j < inputVarValuations[i].size(); j++){
    //         printf("%u ", inputVarValuations[i][j]);
    //     }
    //     printf("\n");
    // }

    // copy to vector with everything
    objValuations.resize(Abc_NtkObjNum(pNtk));
    objInitialized = std::vector<bool>(Abc_NtkObjNum(pNtk), false);
    Abc_NtkForEachPi(pNtk, pPi, i){
        int trueIndex = Abc_ObjId(pPi);
        objValuations[trueIndex] = inputVarValuations[i];
        objInitialized[trueIndex] = true;
    }

    // start propogating thru all internal nodes
    Abc_NtkForEachNode(pNtk, pObj, i){
        Lsv_AigCalcNodeValueRecur(pObj, objValuations, objInitialized, inputLength);
    }

    // Abc_NtkForEachObj(pNtk, pObj, i){
    //     printf("%s: ", Abc_ObjName(pObj));
    //     for (j = 0; j < objValuations[i].size(); j++){
    //         printf("%u ", objValuations[i][j]);
    //     }
    //     printf("\n");
    // }

    // find value of output
    Abc_NtkForEachPo(pNtk, pPo, i){
        Abc_Obj_t* outputNode = Abc_ObjFanin0(pPo);
        int outputNodeIndex = Abc_ObjId(outputNode);
        l = 0;
        printf("%s: ", Abc_ObjName(pPo));
        for (j = 0; j <= (inputLength - 1) / 32; j++){
            unsigned int outputValue;
            if (Lsv_IsConst(outputNode)){
                outputValue = 0xffffffff;
            }
            else {
                outputValue = objValuations[outputNodeIndex][j];
            }
            if (Abc_ObjFaninC0(pPo) == 1){
                outputValue = ~outputValue;
            }

            for (k = 0; k < 32 && l < inputLength; k++){
                if (((outputValue >> (l % 32)) & 1) == 1){
                    printf("1");
                }
                else {
                    printf("0");
                }
                l++;
            }
        }
        printf("\n");
    }

    return 0;
usage:
    Abc_Print( -2, "usage: lsv_bdd_aig [-h] <input_pattern_file>\n" );
    Abc_Print( -2, "\t        Parallel simulation using AIG\n" );
    Abc_Print( -2, "\t-h    : print the command usage\n");
    return 1;
}

// recursive step of finding the boolean value of a node
void Lsv_AigCalcNodeValueRecur(Abc_Obj_t* node, std::vector<std::vector<unsigned int>>& objValuations, std::vector<bool>& objInitialized, int inputLength){
    int trueIndex = Abc_ObjId(node);
    if (objInitialized[trueIndex]){
        return;
    }
    if (Lsv_IsConst(node)){
        objInitialized[trueIndex] = true;
        return;
    }

    Abc_Obj_t* pFanin0 = Abc_ObjFanin0(node);
    Abc_Obj_t* pFanin1 = Abc_ObjFanin1(node);
    // printf(">%d\t%d\t%d\n", Abc_ObjId(node), Abc_ObjId(pFanin0), Abc_ObjId(pFanin1));
    // printf(" %s\t%s\t%s\n", Abc_ObjName(node), Abc_ObjName(pFanin0), Abc_ObjName(pFanin1));
    Lsv_AigCalcNodeValueRecur(pFanin0, objValuations, objInitialized, inputLength);
    Lsv_AigCalcNodeValueRecur(pFanin1, objValuations, objInitialized, inputLength);

    std::vector<unsigned int> valuation;
    fflush(stdout);
    int arrayMaxIndex = (inputLength - 1) / 32;
    for (int i = 0; i <= arrayMaxIndex; i++){
        assert(objValuations[Abc_ObjId(pFanin0)].size() == arrayMaxIndex+1);
        assert(objValuations[Abc_ObjId(pFanin1)].size() == arrayMaxIndex+1);
        unsigned int leftValue;
        unsigned int rightValue;
        if (Lsv_IsConst(pFanin0)){
            leftValue = 0xffffffff;
        }
        else {
            leftValue = objValuations[Abc_ObjId(pFanin0)][i];
        }
        if (Abc_ObjFaninC0(node) == 1){
            leftValue = ~leftValue;
        }

        if (Lsv_IsConst(pFanin1)){
            rightValue = 0xffffffff;
        }
        else {
            rightValue = objValuations[Abc_ObjId(pFanin1)][i];
        }
        if (Abc_ObjFaninC1(node) == 1){
            rightValue = ~rightValue;
        }

        valuation.push_back(leftValue & rightValue); 
    }

    objValuations[trueIndex] = valuation;
    objInitialized[trueIndex] = true;
}

