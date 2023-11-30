#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/cnf/cnf.h"
#include <vector>
#include <string>
#include <fstream>
#include <cstdlib>

extern "C"{
    Aig_Man_t* Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
}

// PA1
static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandLsvSimBdd( Abc_Frame_t * pAbc, int argc, char ** argv );
static int Lsv_CommandLsvSimAig( Abc_Frame_t * pAbc, int argc, char ** argv );
static void Lsv_AigCalcNodeValueRecur(Abc_Obj_t* node, std::vector<std::vector<unsigned int>>& objValuations, std::vector<bool>& objInitialized, int inputLength);
// checks if is constant node 
// abc_NodeIsConst has assertion error
static inline int Lsv_IsConst(Abc_Obj_t* pNode) { return !Abc_ObjIsPi(pNode) && Abc_ObjFaninNum(pNode) == 0 && Abc_ObjFanoutNum(pNode) > 0 ;}

// PA2
static int Lsv_CommandLsvSymBdd( Abc_Frame_t * pAbc, int argc, char ** argv );
static bool Lsv_BddFindPathRecur( DdManager* dd, int* pathTaken, int* inputPinLookup, DdNode* node);
static int Lsv_CommandLsvSymSat( Abc_Frame_t * pAbc, int argc, char ** argv );
static int Lsv_CommandLsvSymAll( Abc_Frame_t * pAbc, int argc, char ** argv );

void init(Abc_Frame_t* pAbc) {
    Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", Lsv_CommandLsvSimBdd, 0);
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig", Lsv_CommandLsvSimAig, 0);
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_bdd", Lsv_CommandLsvSymBdd, 0);
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_sat", Lsv_CommandLsvSymSat, 0);
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_all", Lsv_CommandLsvSymAll, 0);
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


// PA1

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

// PA2
int Lsv_CommandLsvSymBdd(Abc_Frame_t* pAbc, int argc, char** argv)
{
    // get input
    int c = 0;
    int i;
    int inputPin1, inputPin2;
    int outputPin;
    int piNum, poNum;
    Abc_Obj_t* pPo, * pPi1, * pPi2;
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);

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
    if (argc != globalUtilOptind + 3){
        Abc_Print( -1, "Wrong number of arguments.\n");
        return 1;
    }

    outputPin = strtol(argv[globalUtilOptind], NULL, 10);
    inputPin1 = strtol(argv[globalUtilOptind+1], NULL, 10);
    inputPin2 = strtol(argv[globalUtilOptind+2], NULL, 10);
    piNum = Abc_NtkPiNum(pNtk);
    poNum = Abc_NtkPoNum(pNtk);
    if (!(outputPin < poNum && 0 <= outputPin)){
        Abc_Print( -1, "Output pin out of range.\n");
        return 1;
    } 
    
    if (!(inputPin1 < piNum && 0 <= inputPin1) || !(inputPin2 < piNum && 0 <= inputPin2)){
        Abc_Print( -1, "Input pin out of range.\n");
        return 1;
    } 
  
    if (inputPin1 == inputPin2){
        Abc_Print( -1, "Input pins must be different.\n");
        return 1;
    } 

#ifdef ABC_USE_CUDD
    pPo = Abc_NtkPo(pNtk, outputPin);
    pPi1 = Abc_NtkPi(pNtk, inputPin1);
    pPi2 = Abc_NtkPi(pNtk, inputPin2);

    if (Abc_NtkIsBddLogic(pPo->pNtk)){
        Abc_Obj_t* pPoBdd = Abc_ObjFanin0(pPo);
        DdManager* dd = (DdManager*)pPoBdd->pNtk->pManFunc;
        DdNode* poDdNode = (DdNode*)pPoBdd->pData;
        DdNode* pi1DdNode = Cudd_ReadOne(dd), *pi2DdNode = Cudd_ReadOne(dd);
        int pi1BddOrder = -1, pi2BddOrder = -1;

        Vec_Int_t* pFaninIds = Abc_ObjFaninVec(pPoBdd);

        // find bdd nodes corresponding to inputs
        bool pi1Found = false, pi2Found = false;
        for (i = 0; i < pFaninIds->nSize; i++){
            int inputIndex = pFaninIds->pArray[i] - 1;
            if (inputIndex == inputPin1 && !pi1Found){
                pi1Found = true;
                pi1DdNode = Cudd_bddIthVar(dd, i);
                pi1BddOrder = i;
            }
            if (inputIndex == inputPin2 && !pi2Found){
                pi2Found = true;
                pi2DdNode = Cudd_bddIthVar(dd, i);
                pi2BddOrder = i;
            }
            if (pi1Found && pi2Found){
                break;
            }
        }

        // find f(1,0) and f(0,1) using cofactor
        DdNode* cof1DdNode, *cof2DdNode, *temp;
        DdNode* cof;
        temp = Cudd_Cofactor(dd, poDdNode, pi1DdNode);
        Cudd_Ref(temp);
        cof = pi2Found ? Cudd_Not(pi2DdNode) : Cudd_ReadOne(dd);
        cof1DdNode = Cudd_Cofactor(dd, temp, cof);
        Cudd_RecursiveDeref(dd, temp);
        Cudd_Ref(cof1DdNode);

        cof = pi1Found ? Cudd_Not(pi1DdNode) : Cudd_ReadOne(dd);
        temp = Cudd_Cofactor(dd, poDdNode, cof);
        Cudd_Ref(temp);
        cof2DdNode = Cudd_Cofactor(dd, temp, pi2DdNode);
        Cudd_RecursiveDeref(dd, temp);
        Cudd_Ref(cof2DdNode);

        // check equivalence
        if (cof1DdNode == cof2DdNode){
            Abc_Print(1, "symmetric\n");
            Cudd_RecursiveDeref(dd, cof1DdNode);
            Cudd_RecursiveDeref(dd, cof2DdNode);
            return 0;
        }

        // construct XOR
        DdNode* xorDdNode = Cudd_bddXor(dd, cof1DdNode, cof2DdNode);
        Cudd_Ref(xorDdNode);

        // find counter example, valuations for PIs: 
        // -1: pi from inputs
        // 0: 0, 1: 1
        // 2: dont care
        int* piValuations = new int[piNum];
        for (i = 0; i < piNum; i++){
            piValuations[i] = 1;
        }
        piValuations[inputPin1] = -1;
        piValuations[inputPin2] = -1;

        // recursively traverse bdd, find one path to the one node
        bool found = Lsv_BddFindPathRecur(dd, piValuations, pFaninIds->pArray, xorDdNode);
        if (!found){
            Abc_Print(-1, "DEAREST KARTHUS, I HOPE THIS ERROR FINDS YOU WELL.\n");
            return 1;
        }

        Abc_Print(1, "asymmetric\n");
        piValuations[inputPin1] = 0;
        piValuations[inputPin2] = 1;
        for (i = 0; i < piNum; i++){
            Abc_Print(1, "%d", piValuations[i]);
        }
        Abc_Print(1,"\n");
        piValuations[inputPin1] = 1;
        piValuations[inputPin2] = 0;
        for (i = 0; i < piNum; i++){
            Abc_Print(1, "%d", piValuations[i]);
        }
        Abc_Print(1,"\n");
        delete [] piValuations;
    }

#endif
    
    return 0;
usage:
    Abc_Print( -2, "usage: lsv_sym_bdd [-h] <output pin> <input pin 1> <input pin 2>\n" );
    Abc_Print( -2, "\t        Tests whether the output pin is symmertric wrt the two input pins.\n" );
    Abc_Print( -2, "\t-h    : print the command usage\n");
    return 1;
}

bool Lsv_BddFindPathRecur( DdManager* dd, int* pathTaken, int* inputPinLookup, DdNode* node){
    DdNode *thisNode, *thenNode, *elseNode;
    int index;
    
    if (node == Cudd_ReadOne(dd)){
        // printf("ONE FOUND\n");
        return true;
    }
    if (node == Cudd_Not(Cudd_ReadOne(dd))){
        return false;
    }
    
    thisNode = Cudd_Regular(node);
    thenNode = cuddT(thisNode);
    elseNode = cuddE(thisNode);
    if (Cudd_IsComplement(node)) {
        thenNode = Cudd_Not(thenNode); 
        elseNode = Cudd_Not(elseNode);
    }
    index = inputPinLookup[Cudd_NodeReadIndex(thisNode)] - 1;

    if (pathTaken[index] == -1){
        printf("DEAREST KAYN, I HOPE THIS ERROR FINDS YOU WELL\n");
    }

    pathTaken[index] = 1;
    bool thenResult = Lsv_BddFindPathRecur(dd, pathTaken, inputPinLookup, thenNode);
    if (thenResult) return true;
    pathTaken[index] = 0;
    bool elseResult = Lsv_BddFindPathRecur(dd, pathTaken, inputPinLookup, elseNode);
    if (elseResult) return true;

    pathTaken[index] = 2; 
    return false;
}

int Lsv_CommandLsvSymSat(Abc_Frame_t* pAbc, int argc, char** argv)
{
    // get input
    int c = 0;
    int i;
    int inputPin1, inputPin2;
    int outputPin;
    int piNum, poNum;
    Abc_Obj_t* pPo, * pPi1, * pPi2;
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);

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
    if (argc != globalUtilOptind + 3){
        Abc_Print( -1, "Wrong number of arguments.\n");
        return 1;
    }

    outputPin = strtol(argv[globalUtilOptind], NULL, 10);
    inputPin1 = strtol(argv[globalUtilOptind+1], NULL, 10);
    inputPin2 = strtol(argv[globalUtilOptind+2], NULL, 10);
    piNum = Abc_NtkPiNum(pNtk);
    poNum = Abc_NtkPoNum(pNtk);
    if (!(outputPin < poNum && 0 <= outputPin)){
        Abc_Print( -1, "Output pin out of range.\n");
        return 1;
    } 
    
    if (!(inputPin1 < piNum && 0 <= inputPin1) || !(inputPin2 < piNum && 0 <= inputPin2)){
        Abc_Print( -1, "Input pin out of range.\n");
        return 1;
    } 
  
    if (inputPin1 == inputPin2){
        Abc_Print( -1, "Input pins must be different.\n");
        return 1;
    } 

    pPo = Abc_NtkPo(pNtk, outputPin);
    pPi1 = Abc_NtkPi(pNtk, inputPin1);
    pPi2 = Abc_NtkPi(pNtk, inputPin2);

    // aig
    if (Abc_NtkIsStrash(pPo->pNtk)){
        Abc_Ntk_t* pConeNtk = Abc_NtkCreateCone(pNtk, Abc_ObjFanin0(pPo), Abc_ObjName(pPo), 1);
        Aig_Man_t* pAig = Abc_NtkToDar(pConeNtk, 0, 0);

        sat_solver* pSatSolver = sat_solver_new();
        Cnf_Dat_t* pCnf1 = Cnf_Derive(pAig, 1);
        Cnf_DataWriteIntoSolverInt(pSatSolver, pCnf1, 1, 0);

        Cnf_Dat_t* pCnf2 = Cnf_Derive(pAig, 1);
        Cnf_DataLift(pCnf2, pCnf1->nVars);
        Cnf_DataWriteIntoSolverInt(pSatSolver, pCnf2, 1, 0);

        Aig_Obj_t* pAigCi;
        lit Lits[2];
        Aig_ManForEachCi(pAig, pAigCi, i){
            int cnf1Var = pCnf1->pVarNums[pAigCi->Id];
            int cnf2Var = pCnf2->pVarNums[pAigCi->Id];
            if (inputPin1 == i){
                // constraint: assign inputPin1 0 and 1
                Lits[0] = toLitCond(cnf1Var, 0);
                sat_solver_addclause(pSatSolver, Lits, Lits + 1);
                Lits[0] = toLitCond(cnf2Var, 1);
                sat_solver_addclause(pSatSolver, Lits, Lits + 1);
            }
            else if (inputPin2 == i){
                // constraint: assign inputPin2 1 and 0
                Lits[0] = toLitCond(cnf1Var, 1);
                sat_solver_addclause(pSatSolver, Lits, Lits + 1);
                Lits[0] = toLitCond(cnf2Var, 0);
                sat_solver_addclause(pSatSolver, Lits, Lits + 1);
            }
            else {
                // constraint: other inputs should be equal
                Lits[0] = toLitCond(cnf1Var, 0);
                Lits[1] = toLitCond(cnf2Var, 1);
                sat_solver_addclause(pSatSolver, Lits, Lits + 2);
                Lits[0] = toLitCond(cnf1Var, 1);
                Lits[1] = toLitCond(cnf2Var, 0);
                sat_solver_addclause(pSatSolver, Lits, Lits + 2);
            }
        }

        // constraint: output must be different values (to test asymmetry)
        int outputAigId = Aig_ManCo(pAig, 0)->Id; 
        int cnf1Output = pCnf1->pVarNums[outputAigId];
        int cnf2Output = pCnf2->pVarNums[outputAigId];
        Lits[0] = toLitCond(cnf1Output, 0);
        Lits[1] = toLitCond(cnf2Output, 0);
        sat_solver_addclause(pSatSolver, Lits, Lits + 2);
        Lits[0] = toLitCond(cnf1Output, 1);
        Lits[1] = toLitCond(cnf2Output, 1);
        sat_solver_addclause(pSatSolver, Lits, Lits + 2);
        
        // solve the cnf
        lbool result = sat_solver_solve(pSatSolver, Lits, Lits, 0, 0, 0, 0);

        if (result == l_True){
            int* piValuations = new int[piNum];
            Aig_ManForEachCi(pAig, pAigCi, i){
                int cnf1Var = pCnf1->pVarNums[pAigCi->Id];
                piValuations[i] = sat_solver_var_value(pSatSolver, cnf1Var);
            }

            Abc_Print(1, "asymmetric\n");
            piValuations[inputPin1] = 0;
            piValuations[inputPin2] = 1;
            for (i = 0; i < piNum; i++){
                Abc_Print(1, "%d", piValuations[i]);
            }
            Abc_Print(1,"\n");
            piValuations[inputPin1] = 1;
            piValuations[inputPin2] = 0;
            for (i = 0; i < piNum; i++){
                Abc_Print(1, "%d", piValuations[i]);
            }
            Abc_Print(1,"\n");
            delete [] piValuations;
        }
        else if (result == l_False){
            Abc_Print(1, "symmetric\n");
        }
        else {
            Abc_Print(-1, "ERROR\n");
            return 1;
        }

    }

    return 0;
usage:
    Abc_Print( -2, "usage: lsv_sym_sat [-h] <output pin> <input pin 1> <input pin 2>\n" );
    Abc_Print( -2, "\t        Tests whether the output pin is symmertric wrt the two input pins.\n" );
    Abc_Print( -2, "\t-h    : print the command usage\n");
    return 1;
}

int Lsv_CommandLsvSymAll( Abc_Frame_t * pAbc, int argc, char ** argv ){
    // get input
    int c = 0;
    int i, j;
    int outputPin;
    int piNum, poNum;
    Abc_Obj_t* pPo;
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);

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

    outputPin = strtol(argv[globalUtilOptind], NULL, 10);
    piNum = Abc_NtkPiNum(pNtk);
    poNum = Abc_NtkPoNum(pNtk);
    if (!(outputPin < poNum && 0 <= outputPin)){
        Abc_Print( -1, "Output pin out of range.\n");
        return 1;
    } 

    pPo = Abc_NtkPo(pNtk, outputPin);

    // aig
    if (Abc_NtkIsStrash(pPo->pNtk)){
        Abc_Ntk_t* pConeNtk = Abc_NtkCreateCone(pNtk, Abc_ObjFanin0(pPo), Abc_ObjName(pPo), 1);
        Aig_Man_t* pAig = Abc_NtkToDar(pConeNtk, 0, 0);

        sat_solver* pSatSolver = sat_solver_new();
        Cnf_Dat_t* pCnf1 = Cnf_Derive(pAig, 1);
        Cnf_DataWriteIntoSolverInt(pSatSolver, pCnf1, 1, 0);

        Cnf_Dat_t* pCnf2 = Cnf_Derive(pAig, 1);
        Cnf_DataLift(pCnf2, pCnf1->nVars);
        Cnf_DataWriteIntoSolverInt(pSatSolver, pCnf2, 1, 0);

        int* controlVars = new int[piNum];
        for (i = 0; i < piNum; i++){
            controlVars[i] = sat_solver_addvar(pSatSolver);
        }

        Aig_Obj_t* pAigCi, *pAigCj;
        lit Lits[4];
        // constraint 2
        Aig_ManForEachCi(pAig, pAigCi, i){
            int cnf1Var = pCnf1->pVarNums[pAigCi->Id];
            int cnf2Var = pCnf2->pVarNums[pAigCi->Id];
            int controlVar = controlVars[i];
            Lits[0] = toLitCond(cnf1Var, 0);
            Lits[1] = toLitCond(cnf2Var, 1);
            Lits[2] = toLitCond(controlVar, 1);
            sat_solver_addclause(pSatSolver, Lits, Lits + 3);
            Lits[0] = toLitCond(cnf1Var, 1);
            Lits[1] = toLitCond(cnf2Var, 0);
            Lits[2] = toLitCond(controlVar, 1);
            sat_solver_addclause(pSatSolver, Lits, Lits + 3);
        }

        Aig_ManForEachCi(pAig, pAigCi, i){
            for (j = i+1; j < piNum; j++){
                pAigCj = (Aig_Obj_t*) Vec_PtrEntry(pAig->vCis, j);
                int cnf1Vari = pCnf1->pVarNums[pAigCi->Id];
                int cnf2Varj = pCnf2->pVarNums[pAigCj->Id];
                int controlVari = controlVars[i];
                int controlVarj = controlVars[j];
                // constraint 3
                Lits[0] = toLitCond(cnf1Vari, 0);
                Lits[1] = toLitCond(cnf2Varj, 1);
                Lits[2] = toLitCond(controlVari, 0);
                Lits[3] = toLitCond(controlVarj, 0);
                sat_solver_addclause(pSatSolver, Lits, Lits + 4);
                Lits[0] = toLitCond(cnf1Vari, 1);
                Lits[1] = toLitCond(cnf2Varj, 0);
                Lits[2] = toLitCond(controlVari, 0);
                Lits[3] = toLitCond(controlVarj, 0);
                sat_solver_addclause(pSatSolver, Lits, Lits + 4);

                int cnf1Varj = pCnf1->pVarNums[pAigCj->Id];
                int cnf2Vari = pCnf2->pVarNums[pAigCi->Id];
                // constraint 4
                Lits[0] = toLitCond(cnf1Varj, 0);
                Lits[1] = toLitCond(cnf2Vari, 1);
                Lits[2] = toLitCond(controlVari, 0);
                Lits[3] = toLitCond(controlVarj, 0);
                sat_solver_addclause(pSatSolver, Lits, Lits + 4);
                Lits[0] = toLitCond(cnf1Varj, 1);
                Lits[1] = toLitCond(cnf2Vari, 0);
                Lits[2] = toLitCond(controlVari, 0);
                Lits[3] = toLitCond(controlVarj, 0);
                sat_solver_addclause(pSatSolver, Lits, Lits + 4);
            }
        }

        // constraint 1
        int outputAigId = Aig_ManCo(pAig, 0)->Id; 
        int cnf1Output = pCnf1->pVarNums[outputAigId];
        int cnf2Output = pCnf2->pVarNums[outputAigId];
        Lits[0] = toLitCond(cnf1Output, 0);
        Lits[1] = toLitCond(cnf2Output, 0);
        sat_solver_addclause(pSatSolver, Lits, Lits + 2);
        Lits[0] = toLitCond(cnf1Output, 1);
        Lits[1] = toLitCond(cnf2Output, 1);
        sat_solver_addclause(pSatSolver, Lits, Lits + 2);
        
        // incremental sat solving 
        int* equivGroups = new int[piNum];
        lit* controlLits = new lit[piNum];
        for (i = 0; i < piNum; i++){
            equivGroups[i] = -1;
            controlLits[i] = toLitCond(controlVars[i], 0);
        }        

        for (i = 0; i < piNum; i++){
            if (equivGroups[i] != -1){
                continue;
            }
            equivGroups[i] = i;
            for (j = i+1; j < piNum; j++){
                if (equivGroups[j] != -1){
                    continue;
                }
                int controlVari = controlVars[i];
                int controlVarj = controlVars[j];
                controlLits[i] = toLitCond(controlVari, 1);
                controlLits[j] = toLitCond(controlVarj, 1);

                lbool result = sat_solver_solve(pSatSolver, controlLits, controlLits+piNum, 0, 0, 0, 0);
                if (result == l_False){
                    // symmetric
                    equivGroups[j] = i;
                }
                else if (result == l_Undef){
                    Abc_Print(-1, "ERROR\n");
                    return 1;
                }
                controlLits[i] = toLitCond(controlVari,0);
                controlLits[j] = toLitCond(controlVarj,0);
            }
        }

        for (i = 0; i < piNum; i++){
            for (j = i+1; j < piNum; j++){
                if (equivGroups[i] == equivGroups[j]){
                    Abc_Print(1, "%d %d\n", i, j);
                }
            }
        }

        delete[] controlVars;
        delete[] controlLits;
        delete[] equivGroups;
    }

    return 0;
usage:
    Abc_Print( -2, "usage: lsv_sym_all [-h] <output pin>\n" );
    Abc_Print( -2, "\t        Outputs which input pin pairs are symmetric wrt the output pin.\n" );
    Abc_Print( -2, "\t-h    : print the command usage\n");
    return 1;
}