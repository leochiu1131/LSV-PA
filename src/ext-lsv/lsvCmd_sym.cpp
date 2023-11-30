#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/cnf/cnf.h"
#include "lsvCmd.h"

#include <iostream>
#include <fstream>
#include <stdio.h>
#include <vector>
#include <string>
#include <unordered_map>
using namespace std;

extern "C"{
Aig_Man_t* Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
}


/*
#############################
###  lsv_sym_bdd Section  ###
#############################
*/
// command format:
// lsv_sym_bdd <i:output> <j:input> <k:input>
int Lsv_CommandSymBdd(Abc_Frame_t* pAbc, int argc, char** argv)
{
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

  if (pNtk == NULL) {
    cout<<"ERROR: No existing network.\n";
    return 1;
  }
  if (!Abc_NtkHasBdd(pNtk)) {
    cout<<"ERROR: No existing BDD network.\n";
    return 1;
  }
  if (argc < 4) {
    cout<<"ERROR: Incorrect input pattern format\n";
    return 1;
  }

  Lsv_SymBdd(pNtk, stoi(argv[1]), stoi(argv[2]), stoi(argv[3]));

  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sym_bdd [-h] <k> <i> <j>\n");
  Abc_Print(-2, "\t check output<k> is symmetric on input<i>,<j> or not\n");
  Abc_Print(-2, "\t-h            : print the command usage\n");
  Abc_Print(-2, "\t<k>, <i>, <j> : output, input, input pin index, start from 0\n");
  return 1;
}

int Lsv_SymBdd(Abc_Ntk_t* pNtk, int out, int in_a, int in_b)
{
  Abc_Obj_t *pRoot, *pPi, *pPo, *pFanin;
  int        i, in_a_var, in_b_var, minterm_size, result;
  char      *minterm, *input_pattern;
  DdManager *dd;
  DdNode    *ddnode, *cof0, *cof1, *cof01, *cof10, *asym;

  pPo = Abc_NtkPo(pNtk, out);
  pRoot = Abc_ObjFanin0(pPo);
  assert( Abc_NtkIsBddLogic(pRoot->pNtk) );

  // // print target output fanins
  // cout<<"Fanins: ";
  // Abc_ObjForEachFanin(pRoot, pFanin, i)
  // {
  //   cout<<Abc_ObjName(pFanin)<<" ";
  // }cout<<endl;
  

  // transform network input order to IthVar number
  in_a_var = Abc_ObjFaninNum(pRoot);
  in_b_var = Abc_ObjFaninNum(pRoot);
  Abc_ObjForEachFanin(pRoot, pFanin, i)
  {
    if (Abc_ObjId(pFanin) == in_a+1) in_a_var = i;
    if (Abc_ObjId(pFanin) == in_b+1) in_b_var = i;
  }

  // Verify symmetric, out_a'b == out_ab' ?
  dd = (DdManager *)pRoot->pNtk->pManFunc;
  ddnode = (DdNode *)pRoot->pData;
  Cudd_Ref(ddnode);
  cof0  = Cudd_Cofactor(dd, ddnode, Cudd_Not(Cudd_bddIthVar(dd, in_a_var)));
  Cudd_Ref(cof0);
  cof1  = Cudd_Cofactor(dd, ddnode, Cudd_bddIthVar(dd, in_a_var));
  Cudd_Ref(cof1);
  cof01 = Cudd_Cofactor(dd, cof0, Cudd_bddIthVar(dd, in_b_var));
  Cudd_Ref(cof01);
  cof10 = Cudd_Cofactor(dd, cof1, Cudd_Not(Cudd_bddIthVar(dd, in_b_var)));
  Cudd_Ref(cof10);
  asym  = Cudd_bddXor(dd, cof01, cof10);  
  Cudd_Ref(asym);

  if(asym == Cudd_ReadLogicZero(dd))
  {
    cout<<"symmetric\n";
  }
  else
  {
    cout<<"asymmetric\n";
    minterm_size = Abc_ObjFaninNum(pRoot);
    minterm = new char[minterm_size];
    // minterm init to 2
    for (i = 0; i < minterm_size; i++) minterm[i] = 2;
    result = Cudd_bddPickOneCube(dd, asym, minterm);
    
    // // check minterm content, don't need in submission
    // cout<<"onset minterm: ";
    // for (i = 0; i < minterm_size; i++) {
    //   cout<<int(minterm[i])<<" ";
    // }cout<<endl;

    input_pattern = new char[Abc_NtkPiNum(pNtk)];
    //set pattern all '0'
    for (i = 0; i < Abc_NtkPiNum(pNtk); i++) {
      input_pattern[i] = '0';
    }
    //update minterm into pattern
    for (i = 0; i < minterm_size; i++) {
      pFanin = Abc_ObjFanin(pRoot, i);
      input_pattern[Abc_ObjId(pFanin)-1] = '0' + (minterm[i]&1); //don't care is given '0'
    }
    //print ab = 01, 10 pattern
    input_pattern[in_a] = '0';
    input_pattern[in_b] = '1';
    cout<<input_pattern<<endl;
    input_pattern[in_a] = '1';
    input_pattern[in_b] = '0';
    cout<<input_pattern<<endl;

    // //check pattern correct, don't need in submission
    // result = Lsv_AsymCheckBdd(pNtk, out, in_a, in_b, input_pattern);

    delete [] minterm;
    delete [] input_pattern;
  }
  Cudd_RecursiveDeref(dd, ddnode);
  Cudd_RecursiveDeref(dd, cof0);
  Cudd_RecursiveDeref(dd, cof1);
  Cudd_RecursiveDeref(dd, cof01);
  Cudd_RecursiveDeref(dd, cof10);
  Cudd_RecursiveDeref(dd, asym);
  return 0;
}

int Lsv_AsymCheckBdd(Abc_Ntk_t* pNtk, int out, int in_a, int in_b, char* input_pattern)
{
  Abc_Obj_t *pRoot, *pPi, *pPo;
  int i, input_id;
  bool result[2];
  unordered_map<string, bool> input_map;
  
  pPo = Abc_NtkPo(pNtk, out);
  pRoot = Abc_ObjFanin0(pPo);
  assert( Abc_NtkIsBddLogic(pRoot->pNtk) );

  DdManager * dd = (DdManager *)pRoot->pNtk->pManFunc;  
  DdNode* ddnode = (DdNode *)pRoot->pData;
  
  //get the variable order of this bdd PO
  Vec_Ptr_t *vName = Abc_NodeGetFaninNames(pRoot);
  
  for (input_id = 0; input_id < 2; input_id++)
  {
    input_pattern[in_a] = '0' + (input_id&1);
    input_pattern[in_b] = '0' + !(input_id&1);
    //build map for input name and value
    Abc_NtkForEachPi(pNtk, pPi, i) {
      input_map[Abc_ObjName(pPi)] = input_pattern[i] == '1';
    }
    //do cofactor for assigning value to variable
    DdNode * Cof = ddnode;
    for(int i = 0; i < vName->nSize; i++) {
      if(Cudd_IsConstant(Cof)) {
        break;
        cout<<"current node is constant, break at "<<i<<"\n";
      }
      if (input_map[(char*)vName->pArray[i]]) {
        Cof = Cudd_Cofactor(dd, Cof, Cudd_bddIthVar(dd, i)); 
      } else {
        Cof = Cudd_Cofactor(dd, Cof, Cudd_Not(Cudd_bddIthVar(dd, i))); 
      }
    }

    if(Cof == Cudd_ReadOne(dd)) {
      result[input_id] = true;
    } else if(Cof == Cudd_ReadLogicZero(dd)) {
      result[input_id] = false;
    } else {
      cout<<Abc_ObjName(pPo)<<": non-constant, something wrong with asymm-check\n";
    }
  }

  if (result[0] == result[1]) {
    cout<<"[Asym Check] given patterns NOT asymmetric!!!\n";
    return 0;
  } else {
    cout<<"[Asym Check] Passed, patten1: "<<result[0]<<", pattern2: "<<result[1]<<endl;
    return 1;
  }
  
}

// /*
// #########################################
// ###  lsv_sym_sat/lsv_sym_all Section  ###
// #########################################
// */

int Lsv_CommandSymSat(Abc_Frame_t* pAbc, int argc, char** argv) {
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

  if (pNtk == NULL) {
    cout<<"ERROR: No existing network.\n";
    return 1;
  }
  if (!Abc_NtkHasAig(pNtk)) {
    cout<<"ERROR: Current network doesn't have AIG.\n";
    return 1;
  }
  if (argc < 4) {
    cout<<"ERROR: Incorrect input pattern format.\n";
    return 1;
  }

  Lsv_SymSat(pNtk, stoi(argv[1]), stoi(argv[2]), stoi(argv[3]));
  
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sym_aig [-h] <k> <i> <j>\n");
  Abc_Print(-2, "\t check output<k> is symmetric on input<i>,<j> or not\n");
  Abc_Print(-2, "\t-h            : print the command usage\n");
  Abc_Print(-2, "\t<k>, <i>, <j> : output, input, input pin index, start from 0\n");
  return 1;
}

int Lsv_CommandSymAll(Abc_Frame_t* pAbc, int argc, char** argv) {
  abctime clk;
  clk = Abc_Clock();
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

  if (pNtk == NULL) {
    cout<<"ERROR: No existing network.\n";
    return 1;
  }
  if (!Abc_NtkHasAig(pNtk)) {
    cout<<"ERROR: Current network doesn't have AIG.\n";
    return 1;
  }
  if (argc < 2) {
    cout<<"ERROR: Incorrect input pattern format.\n";
    return 1;
  }

  Lsv_SymAll(pNtk, stoi(argv[1]));
  
  // Abc_PrintTime(0, "lsv_sym_all time: ", Abc_Clock()-clk);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_sym_all [-h] <k>\n");
  Abc_Print(-2, "\t find all symmetric input pair(i,j) of output<k>, where i < j\n");
  Abc_Print(-2, "\t-h : print the command usage\n");
  Abc_Print(-2, "\t<k>: output pin index, start from 0\n");
  return 1;
}

int Lsv_SymSat(Abc_Ntk_t* pNtk, int out, int in_a, int in_b)
{
  int i, i_asym, status, input_num = Abc_NtkPiNum(pNtk);
  int copy_offset; //original input var - lifted input var
  int var_out;     //lifted output varnum of pCnf
  int bind_offset; //the first varnum of input binding enable var
  Cnf_Dat_t *pCnf;
  sat_solver *pSat;

  //check func support size of output
  // if 0 then sat solver will always asym, wrong, need deal with first
  Vec_Int_t *vec;
  int supp_size;
  vec = Abc_NtkNodeSupportInt(pNtk, out);
  supp_size = vec->nSize;
  Vec_IntFree(vec);
  if (supp_size == 0)
  {
    cout<<"symmetric\n";
    return 0;
  }
  
  Lsv_SymSat_Build(pNtk, out, pSat, pCnf, copy_offset, bind_offset, var_out);
  status = Lsv_SymSat_SolveInputPair(pSat, pCnf, in_a, in_b, input_num, copy_offset, bind_offset);
  
  if (status == 1) {
    cout<<"asymmetric\n";
    //print 2 asymmetric patterns, force (in_a, in_b) = (0,1), (1,0)
    for (i_asym = 0; i_asym < 2; i_asym++) {
      for (i = 0; i < input_num; i++) {
        if (i == in_a) {
          cout<<char('0' + i_asym);
        } else if (i == in_b)  {
          cout<<char('0' + (!i_asym));
        } else {
          cout<<char('0' + sat_solver_var_value(pSat, var_in(pCnf, i)));
        }
      }
      cout<<endl;
    }
  } else if (status == -1) {
    cout<<"symmetric\n";
  } else {
    cout<<"[ERROR] The result of sat is undetermined.\n";
  }
  return 0;
}

int Lsv_SymAll(Abc_Ntk_t* pNtk, int out)
{
  int i, j, k, status, sym_count;
  int input_num = Abc_NtkPiNum(pNtk);
  int copy_offset; //original input var - lifted input var
  int var_out;     //lifted output varnum of pCnf
  int bind_offset; //the first varnum of input binding enable var
  Cnf_Dat_t *pCnf;
  sat_solver *pSat;
  vector<vector<bool>> pair_sym(input_num - 1);
  
  //check func support size of output
  // if 0 then sat solver will always asym, wrong, need deal with first
  Vec_Int_t *vec;
  int supp_size;
  vec = Abc_NtkNodeSupportInt(pNtk, out);
  supp_size = vec->nSize;
  Vec_IntFree(vec);
  if (supp_size == 0)
  {
    for (i = 0; i < input_num; i++) {
      for (j = 0; j < input_num - i - 1; j++) {
        cout<<i<<" "<<(i+1+j)<<endl;
      }
    }
    return 0;
  }

  //init pair_sym to all false
  for (i = 0; i < input_num - 1; i++) {
    pair_sym[i].resize(input_num - i - 1, false);
  }
  Lsv_SymSat_Build(pNtk, out, pSat, pCnf, copy_offset, bind_offset, var_out);

  //Loop of finding all symmetric input pairs
  // i for input index
  for (i = 0; i < input_num - 1; i++)
  {
    sym_count = 0;
    //Do sat sym check for fix i, all i+1+j, skip already sym pair
    // j is partial offset of the second input index
    for (j = 0; j < input_num - i - 1; j++)
    {
      if (!pair_sym[i][j]) {
        status = Lsv_SymSat_SolveInputPair(pSat, pCnf, i, i+1+j, input_num, copy_offset, bind_offset);
        if (status == -1) pair_sym[i][j] = true; //-1 == UNSAT == Symmetric
      }
      if (pair_sym[i][j]) {
        sym_count++;
        cout<<i<<" "<<(i+1+j)<<endl;
      }
    }
    //Perform transitive symmetry propagate
    if (sym_count > 1) {
      for (j = 0; j < input_num - i - 2; j++)
      {
        if (pair_sym[i][j]) {
          for (k = 0; k < input_num - i - j - 2; k++) {
            if (pair_sym[i][j+1+k]) {
              // second index = k = input_index_of_bigger - input_index_of_smaller - 1 = (i+1+j+1+k)-(i+1+j)-1
              pair_sym[i+1+j][k] = true; 
            }
          }
        }
      }
    }
  } //end of find all sym input pair
  return 0;
}


// Build symmetric checking pSat from given Ntk and specified output pin index
// int copy_offset: original input var - lifted input var
// int bind_offset: the first varnum of input binding enable var
// int var_out    : lifted output varnum of pCnf
int Lsv_SymSat_Build(Abc_Ntk_t* pNtk, int out, sat_solver* &pSat, Cnf_Dat_t* &pCnf, int &copy_offset, int &bind_offset, int &var_out)
{
  int i, input_num = Abc_NtkPiNum(pNtk);
  Abc_Obj_t *pObj;
  Abc_Ntk_t *pNtkOut;
  Aig_Man_t *pAig;
  
  pObj = Abc_NtkPo(pNtk, out);
  //use all PIs for easy indexing and sym_all
  pNtkOut = Abc_NtkCreateCone( pNtk, Abc_ObjFanin0(pObj), Abc_ObjName(pObj), 1 );

  // transform to aig, add two cnf copy to sat
  pAig = Abc_NtkToDar( pNtkOut, 0, 1);
  // //check aig of cone only has one output
  // assert(Aig_ManCoNum(pAig) == 1);
  pCnf = Cnf_Derive(pAig, 1);
  pSat = sat_solver_new();
  Cnf_DataWriteIntoSolverInt( pSat, pCnf, 1, 0 );
  copy_offset = sat_solver_nvars(pSat);
  Cnf_DataLift(pCnf, copy_offset);
  Cnf_DataWriteIntoSolverInt( pSat, pCnf, 1, 0 );
  
  //bind two copy input with assumption clause
  bind_offset = sat_solver_nvars(pSat);
  sat_solver_setnvars(pSat, bind_offset + input_num);
  for (i = 0; i < input_num; i++)
  {
    sat_solver_add_buffer_enable(pSat, var_in(pCnf, i), var_in(pCnf, i)-copy_offset, bind_offset+i, 0);
  }

  //lifted output var num
  var_out = pCnf->pVarNums[ Aig_ManCo(pAig, 0)->Id ];

  //bind two output with !=, so SAT==Asymm, UNSAT==Symm
  sat_solver_add_buffer(pSat, var_out, var_out-copy_offset, 1);

  return 0;
}

//return: status, 1 for SAT, -1 for UNSAT, 0 for undetermined
int Lsv_SymSat_SolveInputPair(sat_solver* pSat, Cnf_Dat_t* pCnf, int in_a, int in_b, int input_num, int copy_offset, int bind_offset)
{
  int i, status;
  int *Lits; //Literals for assumption before sat solve
  
  //Assume (in_a,in_b) to (0,1), original
  //                      (1,0), copy
  //other input assume copy binding
  //Lits: 0~input_num-1, for binding or copy input_a,b assumption.
  //      input_num~input_num+1, for original input_a,b assumption. 
  Lits = new int[input_num + 2];
  
  for (i = 0; i < input_num; i++)
  {
    if (i == in_a) {
      Lits[i] = toLit(var_in(pCnf, i)); //copy
      Lits[input_num] = toLitCond(var_in(pCnf, i)-copy_offset, 1); //original
    } else if (i == in_b) {
      Lits[i] = toLitCond(var_in(pCnf, i), 1); //copy
      Lits[input_num + 1] = toLit(var_in(pCnf, i)-copy_offset); //original
    } else {
      Lits[i] = toLit(bind_offset + i);
    }
  }

  //solve assumption sat
  status = sat_solver_solve(pSat, Lits, Lits + input_num + 2, 0, 0, 0, 0);
  delete [] Lits;
  
  return status;
}

// Return var num of lifted Cnf
// original var can be obtained by lifted - offset
// in_index+1 == Abc_Ntk input id
// Abc_Ntk input id should == Aig_Man input id
inline int var_in(Cnf_Dat_t *pCnf, int in_index)
{
  return( pCnf->pVarNums[ in_index+1 ] );
}

int test(Abc_Ntk_t *pNtk)
{
  int i;
  Abc_Obj_t *pNtkObj, *pPo, *pPoFanin0;
  Aig_Obj_t *pAigObj;
  Vec_Int_t *vec;

  Abc_NtkForEachPo(pNtk, pPo, i)
  {
    pPoFanin0 = Abc_ObjFanin0(pPo);
    cout<<Abc_ObjName(pPo)<<", fanin: "<<Abc_ObjFaninNum(pPoFanin0)<<endl;
    vec = Abc_NtkNodeSupportInt(pNtk, i);
    cout<<"support number: "<<vec->nSize<<endl;
    Vec_IntFree(vec);
  }
  
  return 0;
}
