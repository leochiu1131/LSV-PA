#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "bdd/cudd/cudd.h"
#include "bdd/cudd/cuddInt.h"
#include <map>
#include <string>
#include <iostream>
#include <fstream>
#include <bitset>

//static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int bdd_simulation(Abc_Frame_t* pAbc, int argc, char** argv);
static int aig_simulation(Abc_Frame_t* pAbc, int argc, char** argv);
//static int test (Abc_Frame_t* pAbc, int argc, char** argv);
/*
void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
}*/

void init(Abc_Frame_t* pAbc) 
{
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", bdd_simulation, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_aig", aig_simulation, 0);
  //Cmd_CommandAdd(pAbc, "LSV", "test", test, 0);
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
    Abc_Obj_t* pRoot = Abc_ObjFanin0(pPo);
    assert( Abc_NtkIsBddLogic(pRoot->pNtk) );
    DdManager * dd = (DdManager *)pRoot->pNtk->pManFunc;  //bddithvar under dd
    DdNode* ddnode = (DdNode *)pRoot->pData;
    char** vNamesIn = (char**) Abc_NodeGetFaninNames(pRoot)->pArray;
    int ithfanin;
    Abc_Obj_t* pFanin;

    Abc_ObjForEachFanin( pRoot, pFanin, ithfanin)
    {
      DdNode* ddsubnode = Cudd_bddIthVar(dd,ithfanin);
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




int aig_simulation(Abc_Frame_t* pAbc, int argc, char** argv)
{ 
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc); //convert frame to network
  Abc_NtkIsAigLogic(pNtk);
  std::ifstream infile; //create input file stream
  infile.open(argv[1],std::ios::in); //open the input file stream
  Abc_Obj_t* pPi; //primary input pointer
  int ithpi; //index of PI
  int nPi = Abc_NtkPiNum(pNtk); //total number of primary input 
  int** piarr = (int**)malloc(nPi * sizeof(int*)); //primary input array of each pi (2D)
  for (int i = 0; i < nPi; i++)
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
        if(len != 1) //if there is a new unsigned integer numeber, we need to allocate a new space 
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

/*
int test(Abc_Frame_t* pAbc, int argc, char** argv)
{
  int len = atoi(argv[1]);
  int row = atoi(argv[2]);
  std::ofstream opfile; //create input file stream
  opfile.open(argv[3],std::ios::out); //open the input file stream  
  int* arr = (int*)malloc(sizeof(int)*len);
  int N = 1;
  for(int i = 0; i < len; ++i)
  {
    N = N * 2;
  }
  N = N - 1;
  for(int i = 0; i < row; ++i)
  {
    int randnum = rand()% N;
    int times = 0;
    while (times <= len-1) //convert decimal number to binary and store it into binary array
    {
      // storing remainder in binary array
      arr[times] = randnum % 2;
      randnum = randnum / 2;
      times++;
    }
    for(int j = len-1; j>=0; --j)
    {
      opfile << arr[j];
    }
    opfile << std::endl;
    int a = arr[1]*2 + arr[3]*1;
    int b = arr[0]*2 + arr[2]*1;
    std::cout << a*b << " ";
  }
  std::cout << std::endl;
  return 0;
}
*/
