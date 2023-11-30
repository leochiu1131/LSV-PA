#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "bdd/cudd/cudd.h"
#include "bdd/cudd/cuddInt.h"
#include "sat/cnf/cnf.h"
#include <map>
#include <string>
#include <iostream>
#include <fstream>
#include <bitset>
#include <utility>
#include <vector>
extern "C"
{
  Aig_Man_t* Abc_NtkToDar(Abc_Ntk_t* pNtk, int fExors, int fRegisters);
}
//static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int bdd_simulation(Abc_Frame_t* pAbc, int argc, char** argv);
static int aig_simulation(Abc_Frame_t* pAbc, int argc, char** argv);
static int bdd_symmetric(Abc_Frame_t* pAbc, int argc, char** argv);
static int aig_symmetric(Abc_Frame_t* pAbc, int argc, char** argv);
static int all_symmetric(Abc_Frame_t* pAbc, int argc, char** argv);
static int test (Abc_Frame_t* pAbc, int argc, char** argv);
/*
void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
}*/

void init(Abc_Frame_t* pAbc) 
{
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", bdd_simulation, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig", aig_simulation, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_bdd", bdd_symmetric, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_sat", aig_symmetric, 0); 
  Cmd_CommandAdd(pAbc, "LSV", "test", test, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_all", all_symmetric, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager 
{
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

void Lsv_NtkPrintNodes(Abc_Ntk_t* pNtk) 
{
  Abc_Obj_t* pObj;
  int i;
  Abc_NtkForEachNode(pNtk, pObj, i) {
    printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
    Abc_Obj_t* pFanin;
    int j;
    Abc_ObjForEachFanin(pObj, pFanin, j) 
    {
      printf("  Fanin-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanin),
             Abc_ObjName(pFanin));
    }
    if (Abc_NtkHasSop(pNtk)) 
    {
      printf("The SOP of this node:\n%s", (char*)pObj->pData);
    }
  }
}

int test(Abc_Frame_t* pAbc, int argc, char** argv)
{
  int a = atoi(argv[1]);
  std::cout << a << std::endl;
  
  return 0;
  

  //assert(ddnode == Cudd_ReadOne(dd) );
  //cudd_readone 
  //std::cout << tmpstr << std::endl;

}

/*
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
}*/


int bdd_simulation(Abc_Frame_t* pAbc, int argc, char** argv)
{
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc); //convert frame to network
  std::map <std::string,int> inputht; //create input hashtable
  Abc_Obj_t* pPi; //temporary pi object pointer
  int ithpi; //index of the pi
  std::string pistr; //temporary string of each pi
  Abc_NtkForEachPi( pNtk, pPi, ithpi) //
  {
    pistr = Abc_ObjName(pPi); //get the string of each pi
    inputht[pistr] = argv[1][ithpi] - 48; //set value of each pi name
  }
  //Abc_NtkForEachPi( pNtk, pPi, ithpi) //
  //{
    //std::cout << Abc_ObjName(pPi) << ": " << argv[1][ithpi] - 48 << std::endl; //get the string of each pi
    //inputht[pistr] = argv[1][ithpi] - 48; //set value of each pi name
  //}
  //test if the stored hash table value is correct
  //for (std::map<std::string,int>::iterator it= inputht.begin(); it!=inputht.end(); ++it)
    //std::cout << it->first << " => " << it->second << '\n';

  //printf("\n");

  //print name of primary output
  Abc_Obj_t* pPo;
  int ithpo;
  int nPo = Abc_NtkPoNum(pNtk);
  int* res = (int*)malloc(sizeof(int)*nPo);

  Abc_NtkForEachPo(pNtk, pPo, ithpo)
  {
    Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo); //reverse to the fanin from the primary output
    assert( Abc_NtkIsBddLogic(pRoot->pNtk) ); 
    DdManager * dd = (DdManager *)pRoot->pNtk->pManFunc;  //bddithvar under dd
    DdNode* ddnode = (DdNode *)pRoot->pData; //value of ithpo
    char** vNamesIn = (char**) Abc_NodeGetFaninNames(pRoot)->pArray; //all fanin of the single primary output
    int ithfanin; //
    Abc_Obj_t* pFanin; //

    Abc_ObjForEachFanin( pRoot, pFanin, ithfanin) //corresponding input network
    {
      DdNode* ddsubnode = Cudd_bddIthVar(dd,ithfanin); //
      if(inputht[vNamesIn[ithfanin]] == 0)
        ddsubnode = Cudd_Not(ddsubnode);
      assert(ddnode != NULL);
      ddnode = Cudd_Cofactor(dd,ddnode,ddsubnode); //?
      //std::cout << "ithfanin: " << ithfanin << std::endl;
    }
    //assert(ddnode == Cudd_ReadOne(dd) );
    //cudd_readone
    if(ddnode == Cudd_ReadOne(dd))
      res[ithpo] = 1;
    else
      res[ithpo] = 0;
  }
  
  Abc_NtkForEachPo(pNtk, pPo, ithpo)
  {
    std::cout << Abc_ObjName(pPo) << ": " << res[ithpo] << std::endl;
  }


  return 0;
}

int bdd_symmetric(Abc_Frame_t* pAbc, int argc, char** argv)
{
  int kindex = atoi(argv[1]); //get number of index k
  int iindex = atoi(argv[2]); //get number of index i 
  int jindex = atoi(argv[3]); //get number of index j
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc); //convert frame to network
  int pinum = Abc_NtkPiNum(pNtk); //number of the primary input
  std::map <std::string,int> pip1; //create asymmetrical first pattern input hashtable
  std::map <std::string,int> pip2; //create asymmetrical second pattern input hashtable
  Abc_Obj_t* pPi; //temporary pi object pointer
  int ithpi; //index of the pi
  std::string pistr; //temporary string of each pi
  Abc_NtkForEachPi( pNtk, pPi, ithpi) //initialize value of each primary input to 0
  {
    pistr = Abc_ObjName(pPi); //get the string of each pi
    if(ithpi == iindex) //primary input index = index of i
    {
      pip1[pistr] = 1; //first pattern i = 1
      pip2[pistr] = 0; //second pattern i = 0
    }
    else if(ithpi == jindex) //primary input index = index of j
    {
      pip1[pistr] = 0; //first pattern j = 0
      pip2[pistr] = 1; //second pattern j = 1
    }
    else
    {
      pip1[pistr] = 0; //set value of other pis to 0 
      pip2[pistr] = 0; //set value of other pis to 0
    }
  }
  std::string kstr; //var name of k
  std::string istr; //var name of i
  std::string jstr; //var name of j
  kstr = Abc_ObjName(Abc_NtkPo(pNtk,kindex)); //set var name of k
  istr = Abc_ObjName(Abc_NtkPi(pNtk,iindex)); //set var name of i
  jstr = Abc_ObjName(Abc_NtkPi(pNtk,jindex)); //set var name of j
  Abc_Obj_t* pRoot = Abc_ObjFanin0(Abc_NtkPo(pNtk,kindex)); //fanin of the kth primary output node
  assert( Abc_NtkIsBddLogic(pRoot->pNtk) ); //check whether the network is a BDD network
  DdManager * dd = (DdManager *)pRoot->pNtk->pManFunc;  //get dd manager of BDD 
  DdNode* ddnodecmpleft = (DdNode *)pRoot->pData; //function on the left side of equation
  Cudd_Ref(ddnodecmpleft); //reference the new created Ddnode
  DdNode* ddnodecmpright = (DdNode *)pRoot->pData; //function on the right side of equation
  Cudd_Ref(ddnodecmpright); //reference the new created Ddnode
  char** vNamesIn = (char**) Abc_NodeGetFaninNames(pRoot)->pArray; //name of all fanin of the single primary output
  int ithfanin; //index of fanin
  Abc_Obj_t* pFanin; //pointer of fanin
  int faninnum = Abc_ObjFaninNum(pRoot); //fanin number of the root
  char* cubestr = (char*)malloc(sizeof(char)*faninnum); //onset cube string (number of literals)
  DdNode* tmpleft;
  DdNode* tmpright; //temporary ddnode of the left and right side
  DdNode* nottmpleft;
  DdNode* nottmpright; //temporary not ddnode of the left and right side
  Abc_ObjForEachFanin( pRoot, pFanin, ithfanin) //corresponding input network
  {
    DdNode* ddsubnode = Cudd_bddIthVar(dd,ithfanin); //get the pointer of ddnode
    Cudd_Ref(ddsubnode); //reference
    //symmetric function
    if(vNamesIn[ithfanin] == istr) //ith primary input
    {
      tmpleft = Cudd_Cofactor(dd,ddnodecmpleft,ddsubnode);
      Cudd_Ref(tmpleft);
      Cudd_RecursiveDeref(dd,ddnodecmpleft); 
      ddnodecmpleft = tmpleft; 
      //Cudd_RecursiveDeref(dd,tmpleft);

      nottmpright = Cudd_Not(ddsubnode);
      Cudd_Ref(nottmpright);
      tmpright = Cudd_Cofactor(dd,ddnodecmpright,nottmpright);
      Cudd_Ref(tmpright);
      Cudd_RecursiveDeref(dd,nottmpright);    
      Cudd_RecursiveDeref(dd,ddnodecmpright);
      ddnodecmpright = tmpright; 
      //Cudd_RecursiveDeref(dd,tmpright);                          
      //ddnodecmpleft = Cudd_Cofactor(dd,ddnodecmpleft,ddsubnode); //positive cofactor
      //ddsubnode = Cudd_Not(ddsubnode); //negative edge of the node
      //ddnodecmpright = Cudd_Cofactor(dd,ddnodecmpright,ddsubnode); //negative cofactor        
    }
    if(vNamesIn[ithfanin] == jstr) //jth primary input 
    {
      tmpright = Cudd_Cofactor(dd,ddnodecmpright,ddsubnode);
      Cudd_Ref(tmpright);
      Cudd_RecursiveDeref(dd,ddnodecmpright); 
      ddnodecmpright = tmpright;
      //Cudd_RecursiveDeref(dd,tmpright);

      nottmpleft = Cudd_Not(ddsubnode);
      Cudd_Ref(nottmpleft);      
      tmpleft = Cudd_Cofactor(dd,ddnodecmpleft,nottmpleft);
      Cudd_Ref(tmpleft);
      Cudd_RecursiveDeref(dd,nottmpleft); 
      Cudd_RecursiveDeref(dd,ddnodecmpleft);          
      ddnodecmpleft = tmpleft; 
      //Cudd_RecursiveDeref(dd,tmpleft); 
      //ddnodecmpright = Cudd_Cofactor(dd,ddnodecmpright,ddsubnode);
      //ddsubnode = Cudd_Not(ddsubnode);
      //ddnodecmpleft = Cudd_Cofactor(dd,ddnodecmpleft,ddsubnode);
    }
    Cudd_RecursiveDeref(dd, ddsubnode);
    //std::cout << "ithfanin: " << ithfanin << std::endl;
  }
  if(ddnodecmpleft == ddnodecmpright) //compare whether two BDDs are equivalent
  {
    Cudd_RecursiveDeref(dd,ddnodecmpleft); //node dereference
    Cudd_RecursiveDeref(dd,ddnodecmpright); //node dereference 
    std::cout << "symmetric" << std::endl;       
    return 0;
  }
  DdNode* bddxor = Cudd_bddXor(dd,ddnodecmpleft,ddnodecmpright); //get xor bdd diagram
  Cudd_Ref(bddxor); //reference the bddxor graph
  Cudd_RecursiveDeref(dd, ddnodecmpleft); //dereference left ddnode graph
  Cudd_RecursiveDeref(dd, ddnodecmpright); //dereference right ddnode graph
  assert(cubestr != NULL);
  assert(bddxor != NULL); 
  int suc = Cudd_bddPickOneCube(dd,bddxor,cubestr);
  assert(suc == 1);
  Cudd_RecursiveDeref(dd,bddxor);

  Abc_ObjForEachFanin( pRoot, pFanin, ithfanin) //corresponding input network
  {
    if(vNamesIn[ithfanin] == istr || vNamesIn[ithfanin] == jstr || (int)cubestr[ithfanin] == 2)
      continue;
    else if((int)cubestr[ithfanin] == 1)
    {
      pip1[vNamesIn[ithfanin]] = 1;
      pip2[vNamesIn[ithfanin]] = 1;
    }
    else
    {
      pip1[vNamesIn[ithfanin]] = 0;
      pip2[vNamesIn[ithfanin]] = 0;      
    }
  }
  std::cout << "asymmetric" << std::endl;
  Abc_NtkForEachPi( pNtk, pPi, ithpi)
  {
    std::cout << pip1[Abc_ObjName(pPi)];
  }
  std::cout << std::endl;
  Abc_NtkForEachPi( pNtk, pPi, ithpi)
  {
    std::cout << pip2[Abc_ObjName(pPi)];
  }
  std::cout << std::endl;
  free(cubestr);  
  return 0;
}

//time ./abc -c "read ./lsv/pa2/demo.blif" -c "strash" -c "lsv_sym_all 50 "
int aig_simulation(Abc_Frame_t* pAbc, int argc, char** argv)
{ 
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc); //convert frame to network
  Abc_NtkIsAigLogic(pNtk); //verify whether the network is an aig network
  std::ifstream infile; //create input file stream
  infile.open(argv[1],std::ios::in); //open the input file stream
  Abc_Obj_t* pPi; //primary input pointer
  int ithpi; //index of PI
  int nPi = Abc_NtkPiNum(pNtk); //total number of primary input 
  int** piarr = (int**)malloc(nPi * sizeof(int*)); //primary input array of each pi (2D) input data
  for (int i = 0; i < nPi; i++) //initialize the array with one integer
      piarr[i] = (int*)malloc(sizeof(int));
  std::string tmpstr; //temporary string 
  std::map <Abc_Obj_t*,unsigned int> objvmap; //hash map: value of each object node
  int len = 0; //total number of rows in input file 
  int intlen = 0; //total number of unsigned integers
  int intindex = 0; //temporary index of each pi array
  int* tmppiptr; //temporary malloc pointer
  while(infile >> tmpstr) //get each input line and store as a string
  {
    ++len; //total number of row of bits of the input file
    if(len % 32 == 1) //if a new unsigned number is read
    {
      ++intlen; //increase the number of unsigned integer
      intindex = intlen - 1; //the unsigned integer index in array at present
    }
    assert(intlen == intindex+1);
    Abc_NtkForEachPi(pNtk, pPi, ithpi) //set value to each PI array
    {
      if(len % 32 == 1) //if it is a new unsigned integer number, we should initialize the value
      {
        if(len != 1) //if there is a new unsigned integer number, we need to allocate a new space 
        {
          tmppiptr = (int*)realloc(piarr[ithpi],intlen);
          assert(tmppiptr != NULL);
          piarr[ithpi] = tmppiptr;
        }
        piarr[ithpi][intindex] = 0; //set initial number as zero   
      }
      piarr[ithpi][intindex] = piarr[ithpi][intindex] * 2 + (tmpstr[ithpi]-48); //read the number of the bit and update the value     
    }
  }
  infile.close(); //close the input file stream
  int nPo =  Abc_NtkPoNum(pNtk); //number of primary output
  int** resarr = (int**)malloc(nPo * sizeof(int*)); //output 2D array
  for (int i = 0; i < nPo; i++)
      resarr[i] = (int*)malloc(intlen * sizeof(int));
  //int* resarr = (int*)malloc(sizeof(int)*len);
  //Abc_NtkForEachPi(pNtk, pPi, ithpi) //set value to each PI
  //{
    //std::cout << Abc_ObjName(pPi) << ": " << objvmap[pPi] << std::endl; 
  //}
  //infile.close(); //close the input file stream
  Abc_Obj_t* pObj; //object pointer
  int ithobj; //index of each object
  Abc_Obj_t* pPo; //primary output pointer
  int ithpo; //index of primary output
  for(int i = 0; i < intlen; ++i) //computing for loop
  {
    Abc_NtkForEachPi(pNtk, pPi, ithpi) //give the input value to the hash map
    {
      objvmap[pPi] = piarr[ithpi][i];
    }
    Abc_NtkForEachObj( pNtk, pObj, ithobj) //compute the value of each and node in AIG
    {
      if(Abc_ObjIsPi(pObj) || pObj->Type == ABC_OBJ_CONST1 || Abc_ObjIsPo(pObj))
        continue;
      else //if the node is an and node
      {
        Abc_Obj_t* fanin0;
        Abc_Obj_t* fanin1;
        fanin0 = Abc_ObjFanin0(pObj);
        fanin1 = Abc_ObjFanin1(pObj);
        unsigned int num0;
        unsigned int num1;
        if(fanin0->Type == ABC_OBJ_CONST1)
          num0 = 1;
        else
          num0 = objvmap[fanin0];
        if(fanin1->Type == ABC_OBJ_CONST1)
          num1 = 1;
        else
          num1 = objvmap[fanin1];
        if(Abc_ObjFaninC0(pObj))
          num0 = ~num0;
        if(Abc_ObjFaninC1(pObj))
          num1 = ~num1;
        unsigned int multinum;
        multinum = num0 & num1;
        objvmap[pObj] = multinum;
        //std::cout << "the mid and node ID is: " << pObj->Id << std::endl;
        //std::cout << "for this and node: " << multinum << std::endl; 
      }
    }
    Abc_NtkForEachPo(pNtk, pPo, ithpo)
    {
      Abc_Obj_t* fanin0;
      fanin0 = Abc_ObjFanin0(pPo);
      unsigned int num0;
      if(fanin0->Type == ABC_OBJ_CONST1)
        num0 = 1;
      else
        num0 = objvmap[fanin0];
      if(Abc_ObjFaninC0(pPo))
        num0 = ~num0;
      resarr[ithpo][i] = num0;
      //std::cout << "the po node ID is: " << pPo->Id << std::endl;
      //std::cout << "output node number is: " << num0 << std::endl;    
    }
    objvmap.clear();
  }  
  unsigned int quo = len/32;
  unsigned int rem = len%32;
  unsigned int lastmulti = 1;
  for(int j = 0; j < rem; ++j)
    lastmulti *= 2;
  lastmulti = lastmulti - 1;
  Abc_NtkForEachPo(pNtk, pPo, ithpo)
  {
    std::cout << Abc_ObjName(pPo) << ": ";
    for(int i = 0; i < intlen; ++i)
    {
      unsigned int resint = resarr[ithpo][i]; //each number in the array
      // array to store binary number
      int binaryNum[32];
      int endindex; //end of the index
      if((i == quo) && (rem!=0)) //if there is a number less than 32 bits 
      {
        resarr[ithpo][i] = resarr[ithpo][i] & lastmulti;
        resint = resarr[ithpo][i];
        endindex = rem - 1;
      }
      else
        endindex = 31;      
      //counter for binary array
      int times = 0;
      while (times <= endindex) //convert decimal number to binary and store it into binary array
      {
        // storing remainder in binary array
        binaryNum[times] = resint % 2;
        resint = resint / 2;
        times++;
      }
      // printing binary array in reverse order
      for (int j = endindex; j >= 0; j--)
        std::cout << binaryNum[j];
    }
    std::cout << std::endl;
  }
  return 0; 
}

int aig_symmetric(Abc_Frame_t* pAbc, int argc, char** argv)
{
  //1k 2i 3j
  int kindex = atoi(argv[1]);
  int iindex = atoi(argv[2]);
  int jindex = atoi(argv[3]);
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc); //convert frame to network
  Abc_NtkIsAigLogic(pNtk); //verify whether the network is an aig network
  //pointer of k
  Abc_Obj_t * kpo = Abc_NtkCo(pNtk,kindex); //get kth primary output node pointer
  Abc_Obj_t * kfaninobj = Abc_ObjFanin0(kpo); //get fanin node of kth primary output node
  Abc_Obj_t * pPo;
  int ithpo;  
  char* kstr = Abc_ObjName(kfaninobj); //get the name of kth primary output
  assert(Abc_ObjIsNode(kfaninobj));  
  Abc_Ntk_t* kNtk = Abc_NtkCreateCone(pNtk,kfaninobj,kstr,1); //derive the cone of k
  Aig_Man_t * kAig = Abc_NtkToDar( kNtk,0,0); //convert the k boolean network to an aig abcDar.c
  sat_solver * kSat = sat_solver_new(); //initialize a new sat solver
  Cnf_Dat_t *kCnfA = Cnf_Derive( kAig, Aig_ManCoNum(kAig)); //derive cnf formula CA cnfCore.c
  kSat = (sat_solver *)Cnf_DataWriteIntoSolverInt(kSat,kCnfA,1,0); //fInit timeframe = 0 cnfMan.c pdrCnf.c
  Cnf_Dat_t *kCnfB = Cnf_Derive( kAig, Aig_ManCoNum(kAig)); //derive cnf formula CB cnfCore.c
  Cnf_DataLift(kCnfB,kCnfB->nVars); //cnfMan.c create cnf formula B kcnf->nVars
  kSat = (sat_solver *)Cnf_DataWriteIntoSolverInt(kSat,kCnfB,1,0); //write formula B into the SAT Solver
  Aig_Obj_t_* CiObj; //combinatorial input object pointer
  int ithci; //index of each combinatorial input
  int nsatVar = kSat->size; //calculate the number of variables in the sat solver

  //std::cout << kSat->size << std::endl; //check the number of variables in the sat solver
  Vec_Int_t * tempVararr = Vec_IntAlloc(nsatVar); //two literals
  int iCnfA,iCnfB,jCnfA,jCnfB;
  Aig_ManForEachCi( kAig,CiObj,ithci) //input the corresponding index of each variable in CNF formula
  {
    if(ithci == iindex || ithci == jindex)
    {
      if(ithci == iindex)
      {
        iCnfA = kCnfA->pVarNums[CiObj->Id];
        iCnfB = kCnfB->pVarNums[CiObj->Id];
      }
      else
      {
        jCnfA = kCnfA->pVarNums[CiObj->Id];
        jCnfB = kCnfB->pVarNums[CiObj->Id];      
      }
      continue;
    }
    tempVararr->nSize = 0;
    Vec_IntPush( tempVararr, toLitCond(kCnfA->pVarNums[CiObj->Id],1) ); //cnfindex complement
    Vec_IntPush( tempVararr, toLitCond(kCnfB->pVarNums[CiObj->Id],0) );
    sat_solver_addclause( kSat, tempVararr->pArray, tempVararr->pArray + tempVararr->nSize ); //verification
    tempVararr->nSize = 0;
    Vec_IntPush( tempVararr, toLitCond(kCnfA->pVarNums[CiObj->Id],0) );
    Vec_IntPush( tempVararr, toLitCond(kCnfB->pVarNums[CiObj->Id],1) );
    sat_solver_addclause( kSat, tempVararr->pArray, tempVararr->pArray + tempVararr->nSize );  
  }
  //A[i] = B[j]
  tempVararr->nSize = 0;
  Vec_IntPush( tempVararr, toLitCond(iCnfA,1) );
  Vec_IntPush( tempVararr, toLitCond(jCnfB,0) );
  sat_solver_addclause( kSat, tempVararr->pArray, tempVararr->pArray + tempVararr->nSize );
  tempVararr->nSize = 0;
  Vec_IntPush( tempVararr, toLitCond(iCnfA,0) );
  Vec_IntPush( tempVararr, toLitCond(jCnfB,1) );
  sat_solver_addclause( kSat, tempVararr->pArray, tempVararr->pArray + tempVararr->nSize );
  //A[j] = B[i]
  tempVararr->nSize = 0;
  Vec_IntPush( tempVararr, toLitCond(jCnfA,1) );
  Vec_IntPush( tempVararr, toLitCond(iCnfB,0) );
  sat_solver_addclause( kSat, tempVararr->pArray, tempVararr->pArray + tempVararr->nSize );
  tempVararr->nSize = 0;
  Vec_IntPush( tempVararr, toLitCond(jCnfA,0) );
  Vec_IntPush( tempVararr, toLitCond(iCnfB,1) );
  sat_solver_addclause( kSat, tempVararr->pArray, tempVararr->pArray + tempVararr->nSize );
  Aig_Obj_t_* CoObj;
  int ithco;
  int kCnfAId,kCnfBId;
  Aig_ManForEachCo( kAig,CoObj,ithco)
  {
    kCnfAId = kCnfA->pVarNums[CoObj->Id];
    kCnfBId = kCnfB->pVarNums[CoObj->Id];
  }
  //A[k] 
  tempVararr->nSize = 0;
  Vec_IntPush( tempVararr, toLitCond(kCnfAId,0) );
  Vec_IntPush( tempVararr, toLitCond(kCnfBId,0) );
  sat_solver_addclause( kSat, tempVararr->pArray, tempVararr->pArray + tempVararr->nSize );
  tempVararr->nSize = 0;
  Vec_IntPush( tempVararr, toLitCond(kCnfAId,1) );
  Vec_IntPush( tempVararr, toLitCond(kCnfBId,1) );
  sat_solver_addclause( kSat, tempVararr->pArray, tempVararr->pArray + tempVararr->nSize );
  Vec_Int_t * solvarr = Vec_IntAlloc(nsatVar);  
  int retvalue = sat_solver_solve(kSat,NULL,NULL,0,0,0,0); 
  if(retvalue == -1)
  {
    std::cout << "symmetric" << std::endl;
    return 0;
  }
  std::cout << "asymmetric" << std::endl;
  Aig_ManForEachCi( kAig,CiObj,ithci) //input the corresponding index of each variable in CNF formula
  {
    std::cout << sat_solver_var_value(kSat, kCnfA->pVarNums[CiObj->Id]);
  }
  std::cout << std::endl;
  Aig_ManForEachCi( kAig,CiObj,ithci) //input the corresponding index of each variable in CNF formula
  {
    std::cout << sat_solver_var_value(kSat, kCnfB->pVarNums[CiObj->Id]);
  }
  std::cout << std::endl;
  return 0;
}

int all_symmetric(Abc_Frame_t* pAbc, int argc, char** argv)
{
  int kindex = atoi(argv[1]); //get kindex
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc); //convert frame to network
  int pinum = Abc_NtkPiNum(pNtk); //primary input number
  Abc_NtkIsAigLogic(pNtk); //verify whether the network is an aig network

  Abc_Obj_t * kpo = Abc_NtkCo(pNtk,kindex); //get kth primary output node pointer
  Abc_Obj_t * kfaninobj = Abc_ObjFanin0(kpo); //get fanin node of kth primary output node
  Abc_Obj_t * pPo; //primary output pointer
  int ithpo;  //ith primary output index
  char* kstr = Abc_ObjName(kfaninobj); //get the name of kth primary output
  assert(Abc_ObjIsNode(kfaninobj)); //ensure the node is an object node
  Abc_Ntk_t* kNtk = Abc_NtkCreateCone(pNtk,kfaninobj,kstr,1); //derive the cone of k
  Aig_Man_t * kAig = Abc_NtkToDar( kNtk,0,0); //convert the k boolean network to an aig abcDar.c
  sat_solver * kSat = sat_solver_new(); //initialize a new sat solver
  Cnf_Dat_t *kCnfA = Cnf_Derive( kAig, Aig_ManCoNum(kAig)); //derive cnf formula CA cnfCore.c
  kSat = (sat_solver *)Cnf_DataWriteIntoSolverInt(kSat,kCnfA,1,0); //fInit timeframe = 0 cnfMan.c pdrCnf.c
  Cnf_Dat_t *kCnfB = Cnf_Derive( kAig, Aig_ManCoNum(kAig)); //derive cnf formula CB cnfCore.c
  Cnf_DataLift(kCnfB,kCnfB->nVars); //cnfMan.c create cnf formula B kcnf->nVars
  kSat = (sat_solver *)Cnf_DataWriteIntoSolverInt(kSat,kCnfB,1,0); //write formula B into the SAT Solver
  int nsatVar = kSat->size; //calculate the number of variables in the sat solver
  Vec_Int_t * tempVararr = Vec_IntAlloc(4); //temporary variable array
  Aig_Obj_t_* CoObj; //
  int ithco;
  int kCnfAId,kCnfBId;
  Aig_ManForEachCo( kAig,CoObj,ithco)
  {
    kCnfAId = kCnfA->pVarNums[CoObj->Id];
    kCnfBId = kCnfB->pVarNums[CoObj->Id];
  }
  //A[k] xor B[k]
  tempVararr->nSize = 0;
  Vec_IntPush( tempVararr, toLitCond(kCnfAId,0) );
  Vec_IntPush( tempVararr, toLitCond(kCnfBId,0) );
  sat_solver_addclause(kSat,tempVararr->pArray,tempVararr->pArray+tempVararr->nSize );
  tempVararr->nSize = 0;
  Vec_IntPush( tempVararr, toLitCond(kCnfAId,1) );
  Vec_IntPush( tempVararr, toLitCond(kCnfBId,1) );
  sat_solver_addclause(kSat,tempVararr->pArray,tempVararr->pArray+tempVararr->nSize);
  Aig_Obj_t_* CiObjouter;
  int ithciouter;
  Aig_Obj_t_* CiObjinner;
  int ithciinner;
  int* asVarindex = (int*)malloc(sizeof(int)*pinum);
  for(int i = 0; i < pinum; ++i)
  {
    asVarindex[i] = nsatVar;
    ++nsatVar;
  }
  Vec_Int_t * c1 = Vec_IntAlloc(4); //temporary variable array
  Vec_Int_t * c2 = Vec_IntAlloc(4); //temporary variable array
  Vec_Int_t * c3 = Vec_IntAlloc(4); //temporary variable array
  Vec_Int_t * c4 = Vec_IntAlloc(4); //temporary variable array
  Vec_Int_t * c5 = Vec_IntAlloc(4); //temporary variable array
  Vec_Int_t * c6 = Vec_IntAlloc(4); //temporary variable array
  //add clause
  Aig_ManForEachCi(kAig,CiObjouter,ithciouter) //input the corresponding index of each variable in CNF formula
  {
    c1->nSize = 0;
    Vec_IntPush(c1,toLitCond(kCnfA->pVarNums[CiObjouter->Id],1) ); //at'
    Vec_IntPush(c1,toLitCond(kCnfB->pVarNums[CiObjouter->Id],0) ); //bt
    Vec_IntPush(c1,toLitCond(asVarindex[ithciouter],0) ); //ht
    sat_solver_addclause( kSat, c1->pArray, c1->pArray + c1->nSize ); //verification
    c2->nSize = 0;
    Vec_IntPush(c2,toLitCond(kCnfA->pVarNums[CiObjouter->Id],0) ); //at'
    Vec_IntPush(c2,toLitCond(kCnfB->pVarNums[CiObjouter->Id],1) ); //bt
    Vec_IntPush(c2,toLitCond(asVarindex[ithciouter],0) ); //ht
    sat_solver_addclause( kSat, c2->pArray, c2->pArray + c2->nSize ); //verification
    Aig_ManForEachCi(kAig,CiObjinner,ithciinner)
    {
      if(ithciinner <= ithciouter)
        continue;
      c3->nSize = 0;
      Vec_IntPush(c3,toLitCond(kCnfA->pVarNums[CiObjouter->Id],1)); //at1'
      Vec_IntPush(c3,toLitCond(asVarindex[ithciouter],1)); //ht1'
      Vec_IntPush(c3,toLitCond(kCnfB->pVarNums[CiObjinner->Id],0)); //bt2
      Vec_IntPush(c3,toLitCond(asVarindex[ithciinner],1)); //ht2'
      sat_solver_addclause(kSat,c3->pArray,c3->pArray+c3->nSize); //push c3
      c4->nSize = 0;
      Vec_IntPush(c4,toLitCond(kCnfA->pVarNums[CiObjouter->Id],0) ); //at1'
      Vec_IntPush(c4,toLitCond(asVarindex[ithciouter],1) ); //ht1'
      Vec_IntPush(c4,toLitCond(kCnfB->pVarNums[CiObjinner->Id],1) ); //bt2'
      Vec_IntPush(c4,toLitCond(asVarindex[ithciinner],1) ); //ht2'
      sat_solver_addclause(kSat,c4->pArray,c4->pArray+c4->nSize ); //push c4
      c5->nSize = 0;
      Vec_IntPush(c5,toLitCond(kCnfB->pVarNums[CiObjouter->Id],0) ); //bt1
      Vec_IntPush(c5,toLitCond(asVarindex[ithciouter],1) ); //ht1'
      Vec_IntPush(c5,toLitCond(kCnfA->pVarNums[CiObjinner->Id],1) ); //at2'
      Vec_IntPush(c5,toLitCond(asVarindex[ithciinner],1) ); //ht2'
      sat_solver_addclause(kSat,c5->pArray,c5->pArray+c5->nSize ); //push c5
      c6->nSize = 0;
      Vec_IntPush(c6,toLitCond(kCnfB->pVarNums[CiObjouter->Id],1) ); //bt1'
      Vec_IntPush(c6,toLitCond(asVarindex[ithciouter],1) ); //ht1'
      Vec_IntPush(c6,toLitCond(kCnfA->pVarNums[CiObjinner->Id],0) ); //at2
      Vec_IntPush(c6,toLitCond(asVarindex[ithciinner],1) ); //ht2'
      sat_solver_addclause(kSat,c6->pArray,c6->pArray+c6->nSize ); //push c6
    }
  }
  std::vector<std::vector<int> > symmap(pinum);
  Aig_Obj_t_* CiObj; //
  int ithci;
  bool ilist = false;
  bool jlist = false;
  std::vector<int> :: iterator symmapiter;
  std::vector<std::pair<int,int> > res;
  std::vector<std::pair<int,int> > :: iterator resiter; 
  nsatVar = kSat->size;
  Vec_Int_t * asvec = Vec_IntAlloc(nsatVar); //temporary variable array  
  Aig_ManForEachCi(kAig,CiObjouter,ithciouter) //input the corresponding index of each variable in CNF formula
  {
    Aig_ManForEachCi(kAig,CiObjinner,ithciinner)//input the corresponding index of each variable in CNF formula
    {
      if(ithciinner <= ithciouter) //if the index of the inner loop is smaller than the index of outer loop, they can be omitted and 
        continue;
      for(int i = 0;i < ithciouter; ++i) //check if it has transtivity properties
      {
        for(symmapiter = symmap[i].begin(); symmapiter != symmap[i].end(); ++symmapiter)
        {
          if(*symmapiter == ithciouter)
            ilist = true;
          if(*symmapiter == ithciinner)
            jlist = true;
          if(ilist && jlist)
            break;
        }
        if(ilist && jlist)
          break;
        else
        {
          ilist = false;
          jlist = false;
        } 
      }
      if(ilist && jlist) //push the pair of the symmetric indexs into the array
      {
        res.push_back(std::make_pair(ithciouter,ithciinner));
        symmap[ithciouter].push_back(ithciinner);
      }
      else
      {
        assert((ilist && jlist) == 0 );
        asvec->nSize = 0;
        for(int j = 0; j < pinum; ++j)
        {
          if(j == ithciouter || j == ithciinner)
            Vec_IntPush(asvec,toLitCond(asVarindex[j],0));            
          else
            Vec_IntPush(asvec,toLitCond(asVarindex[j],1));           
        }
        int retValue = sat_solver_solve(kSat,asvec->pArray,asvec->pArray+asvec->nSize,0,0,0,0);
        if(retValue == -1)
        {
          res.push_back(std::make_pair(ithciouter,ithciinner));
          symmap[ithciouter].push_back(ithciinner);
        }      
      }
      ilist = false;
      jlist = false;
    }    
  }
  if(!res.empty())
  {
    for(resiter = res.begin(); resiter != res.end(); ++resiter)
      std::cout << resiter->first << " " << resiter->second << std::endl;    
  }

}

//how to create new variables and how to add extra variables?
//provide new index except the index in two aigs
//set variables to 0 or 1 using sat_solver_solve?
//put vec_int_push and sat solver solve initial and end
//if 2 and 3 in the same list