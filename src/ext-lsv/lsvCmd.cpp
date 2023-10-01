#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <map>
using namespace std;

static int simulation_bdd(Abc_Frame_t *pAbc, int argc, char **argv );
static int simulation_aig(Abc_Frame_t *pAbc, int argc, char **argv );
// settings ----------------------------------------------------------------
void init(Abc_Frame_t* pAbc) {
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", simulation_bdd, 0);
    Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig", simulation_aig, 0);
}
void destroy(Abc_Frame_t* pAbc) {}
Abc_FrameInitializer_t frame_initializer = {init, destroy};
struct PackageRegistrationManager {
    PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;
// find the BDD node associated with each PO
void BDDCofactor(char **argv, int inputIndex, DdManager* dd, DdNode* ddnode, DdNode* ddnode_bddIthVar){
    bool f_appearance = (argv[1][inputIndex] == '1') ? true : false;
    ddnode = Cudd_Cofactor(dd, ddnode, f_appearance?ddnode_bddIthVar:Cudd_Not(ddnode_bddIthVar));
}
// lsv_sim_bdd ------------------------------------------------------------
static int simulation_bdd(Abc_Frame_t* pAbc, int argc, char** argv) {
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);;
    Abc_Obj_t *pPo, *pPi;
    int ithPo, ithPi;
    map<string, int> inHashTable;
    Abc_NtkForEachPi(pNtk, pPi, ithPi){
        string piName = Abc_ObjName(pPi);
        if(argv[1][ithPi] == '1'){
            inHashTable[piName] = 1;
        }else{
          inHashTable[piName] = 0;
        }
    }

    Extra_UtilGetoptReset();
    // find the BDD node associated with each PO
    Abc_NtkForEachPo(pNtk, pPo, ithPo) {
        Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo);
        Abc_Obj_t* pFanin;
        int ithFanin;
        DdManager* dd = (DdManager*)pRoot->pNtk->pManFunc;
        DdNode* ddnode = (DdNode*)pRoot->pData;
        char** vNamesIn = (char**)Abc_NodeGetFaninNames(pRoot)->pArray;
        
        Abc_ObjForEachFanin(pRoot, pFanin, ithFanin) {
            DdNode* subnode = (inHashTable[vNamesIn[ithFanin]] == 0) ? Cudd_Not(Cudd_bddIthVar(dd, ithFanin)): Cudd_bddIthVar(dd, ithFanin);
            ddnode = Cudd_Cofactor(dd, ddnode, subnode); 
        }
        cout << Abc_ObjName(pPo) << ": " << ((ddnode == Cudd_ReadLogicZero(dd)) ? "0" : "1") << "\n";
    }
    return 0;
}
// lsv_sim_aig ------------------------------------------------------------
class aigSim{
    private:
        Abc_Ntk_t* pNtk;
        Abc_Obj_t* pPo;
        Abc_Obj_t* pObj;
        Abc_Obj_t* pPi;
        Abc_Obj_t* pFanin;
        Abc_Obj_t* faninObj;
        vector<int> inPatterns;
        vector<string> outPatterns;
        ifstream inFile;
        string fileStr;
        size_t PiNum, PoNum;
        int ithPi, ithPo;
        int ithObj, ithFanin;
        int parallel32_size = 0;
        int pPoBegin = sizeof(int) * 8 - 1;
        int pPoBoundary;
        void PIinPattern();
        void ObjNetwork();
        void PoNetwork(const string&);
    public:
        void aigInit(Abc_Frame_t*);
        void readInFile(int, char**);
        void inPatternLshift();
        void parallel32bitSimulator();
        void appendOutPattern();
        void printOutPattern();
};
void aigSim::aigInit(Abc_Frame_t* pAbc){
    pNtk = Abc_FrameReadNtk(pAbc);
    PiNum = Abc_NtkPiNum(pNtk);
    PoNum = Abc_NtkPoNum(pNtk);
    inPatterns.clear();
    inPatterns = vector<int>(PiNum);
    outPatterns = vector<string>(PoNum);
}
void aigSim::readInFile(int argc, char** argv){
    inFile.open(argv[1],ios::in);
    if(!inFile.is_open()){
        cout << "Failed to open file\n" << endl;
    }
    while(inFile >> fileStr){
        ++parallel32_size;
        for(size_t i = 0; i < PiNum; ++i){
            inPatterns[i] = (fileStr[i] == '0')?(inPatterns[i] << 1):((inPatterns[i] << 1)+1);
        }
        if(parallel32_size == 32){
            parallel32bitSimulator();
            inPatterns.clear();
            inPatterns = vector<int>(PiNum);
            pPoBoundary = 0;
            parallel32_size = 0;
            appendOutPattern();
        }
    }
}
void aigSim::inPatternLshift(){
    for(int i= parallel32_size; i!= 32; ++i) {
        for(size_t j = 0; j < Abc_NtkPiNum(pNtk); ++j){
            inPatterns[j] <<= 1; 
        }
    }
}
void aigSim::PIinPattern(){
    Abc_NtkForEachPi(pNtk, pPi, ithPi) {
        pPi->iTemp = inPatterns[ithPi];
    }
}
void aigSim::ObjNetwork(){
    Abc_NtkForEachObj(pNtk, pObj, ithObj){
        if(pObj->Type==7){
            vector<Abc_Obj_t*> fanin;
            int fCompare10Temp, fCompare11Temp;
            Abc_ObjForEachFanin(pObj, pFanin, ithFanin){
                fanin.push_back(pFanin);
            }
            fCompare10Temp = pObj->fCompl0 ? ~fanin[0]->iTemp : fanin[0]->iTemp;
            fCompare11Temp = pObj->fCompl1 ? ~fanin[1]->iTemp : fanin[1]->iTemp;
            pObj->iTemp = fCompare10Temp & fCompare11Temp;
        }else;
    }
}
void aigSim::PoNetwork(const string& instruction){
    Abc_NtkForEachPo(pNtk, pPo, ithPo) {
        if(instruction == "pPo iTemp"){
            Abc_ObjForEachFanin(pPo, pFanin, ithFanin) {
                faninObj = pFanin;
            }
            if(pPo->fCompl0){
                pPo->iTemp = ~faninObj->iTemp;
            }else{
                pPo->iTemp = faninObj->iTemp;
            }
        
        }else if(instruction == "append OutPattern"){
            for(int i = pPoBegin; i >= pPoBoundary; i--) {
                outPatterns[ithPo] = ((pPo->iTemp >> i) & 1) ? (outPatterns[ithPo]+"1") : (outPatterns[ithPo]+"0");
            }
        }else if(instruction == "print OutPattern"){
            printf("%s: %s\n", Abc_ObjName(pPo), outPatterns[ithPo].c_str());
        }
    }
}
void aigSim::parallel32bitSimulator(){
    PIinPattern();
    ObjNetwork();
    PoNetwork("pPo iTemp");
}
void aigSim::appendOutPattern(){
    pPoBoundary = pPoBegin - parallel32_size + 1;
    PoNetwork("append OutPattern");
}
void aigSim::printOutPattern(){
    PoNetwork("print OutPattern");
}
static int simulation_aig(Abc_Frame_t* pAbc, int argc, char** argv) {
    aigSim aig;  
    aig.aigInit(pAbc);
    aig.readInFile(argc, argv);
    aig.inPatternLshift();
    aig.parallel32bitSimulator();
    aig.appendOutPattern();
    aig.printOutPattern();
    return 0;
}
