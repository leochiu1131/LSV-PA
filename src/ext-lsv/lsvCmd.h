#ifndef __BMATCH_H__
#define __BMATCH_H__

#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/cnf/cnf.h"

#include <iostream>
#include <fstream>
#include <stdio.h>
#include <vector>
#include <string>
#include <unordered_map>
using namespace std;

void init(Abc_Frame_t* pAbc);
void destroy(Abc_Frame_t* pAbc);

/*
#################################
###  lsv_print_nodes Section  ###
#################################
*/
int  Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
void Lsv_NtkPrintNodes(Abc_Ntk_t* pNtk);


/*
#############################
###  lsv_sim_bdd Section  ###
#############################
*/
int Lsv_CommandSimBdd(Abc_Frame_t* pAbc, int argc, char** argv);
int Lsv_SimBdd(Abc_Ntk_t* pNtk, char* input_pattern);

/*
#############################
###  lsv_sim_aig Section  ###
#############################
*/
int  Lsv_CommandSimAig(Abc_Frame_t* pAbc, int argc, char** argv);
int  Lsv_SimAig_ParallelSim(Abc_Ntk_t* pNtk, Vec_Ptr_t* vNode, vector<vector<int>> &result);
void Lsv_SimAig_Output(Abc_Ntk_t* pNtk, vector<vector<int>> &result, int residual);
int  Lsv_SimAig(Abc_Ntk_t* pNtk, char* input_file);


/*
#############################
###  lsv_sym_bdd Section  ###
#############################
*/
int Lsv_CommandSymBdd(Abc_Frame_t* pAbc, int argc, char** argv);
int Lsv_SymBdd(Abc_Ntk_t* pNtk, int out, int in_a, int in_b);
int Lsv_AsymCheckBdd(Abc_Ntk_t* pNtk, int out, int in_a, int in_b, char* input_pattern);


/*
#########################################
###  lsv_sym_sat/lsv_sym_all Section  ###
#########################################
*/
int Lsv_CommandSymSat(Abc_Frame_t* pAbc, int argc, char** argv);
int Lsv_CommandSymAll(Abc_Frame_t* pAbc, int argc, char** argv);

int Lsv_SymSat(Abc_Ntk_t* pNtk, int out, int in_a, int in_b);
int Lsv_SymAll(Abc_Ntk_t* pNtk, int out);

int Lsv_SymSat_Build(Abc_Ntk_t* pNtk, int out, sat_solver* &pSat, Cnf_Dat_t* &pCnf, int &copy_offset, int &bind_offset, int &var_out);
int Lsv_SymSat_SolveInputPair(sat_solver* pSat, Cnf_Dat_t* pCnf, int in_a, int in_b, int input_num, int copy_offset, int bind_offset);

int test(Abc_Ntk_t *pNtk);
int var_in(Cnf_Dat_t *pCnf, int in_index);

#endif
