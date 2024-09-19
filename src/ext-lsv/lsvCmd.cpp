#include <iostream>
#include <vector>
#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
using namespace std;

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_CommandPrintNodes, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

void Lsv_NtkPrintNodes(Abc_Ntk_t* pNtk, int &max) {
  Abc_Obj_t* pObj;
  int i;
  Abc_NtkForEachNode(pNtk, pObj, i) {
  	if(max<Abc_ObjId(pObj)){
  		max=Abc_ObjId(pObj);
	  }
  }
}
vector<int> Union(vector<int> a, vector<int> b){
	vector<int> c; int A=0; int B=0;
	while(A<a.size()&&B<b.size()){
		if(a[A]>b[B]){
			c.push_back(b[B]); B+=1;
		}
		else if(a[A]<b[B]){
			c.push_back(a[A]); A+=1;
		}
		else if(a[A]==b[B]){
			c.push_back(a[A]); A+=1; B+=1;
		}
	}
	while(A<a.size()){
		c.push_back(a[A]); A+=1;
	}
	while(B<b.size()){
		c.push_back(b[B]); B+=1;
	}
	return c;
}
bool include(vector<int> a, vector<int> b){
	int A=0; int B=0;
	if(a==b){
		return false;
	}
	while(A<a.size()&&B<b.size()){
		if(a[A]>b[B]){
			B+=1; return false;
		}
		else if(a[A]<b[B]){
			A+=1;
		}
		else if(a[A]==b[B]){
			A+=1; B+=1;
		}
	}
	if(B<b.size()){
		return false;
	}
	return true;
}
vector<vector<int>> redundant(vector<vector<int>> a){
	vector<bool> keep;
	for(int i=0;i<a.size();i++){
		keep.push_back(true);
	}
	for(int i=0;i<a.size();i++){
		for(int j=0;j<a.size();j++){
			if(include(a[i],a[j])){
				keep[j] = false;
			}
			if(a[i]==a[j] && i>j){
				keep[i] = false;
			}
		}
	}
	vector<vector<int>> result;
	for(int i=0;i<a.size();i++){
		if(keep[i]){
			result.push_back(a[i]);
		}
	}
	return result;
}
vector<vector<vector<int>>> baseCase(int length){
	vector<vector<vector<int>>> cuts;
	for(int i=0;i<length;i++){
		cuts.push_back({{i}});
	}
	return cuts;
}
void Incremental(Abc_Ntk_t* pNtk, vector<vector<vector<int>>> &possibleCuts, int limit) {
  Abc_Obj_t* pObj;
  int i; 
  Abc_NtkForEachNode(pNtk, pObj, i) {
  	vector<int> node;
    node.push_back(Abc_ObjId(pObj));
    Abc_Obj_t* pFanin;
    int j;
    Abc_ObjForEachFanin(pObj, pFanin, j) {
    	node.push_back(Abc_ObjId(pFanin));
    }
    for(int k=0;k<possibleCuts[node[1]].size();k++){
    	for(int l=0;l<possibleCuts[node[2]].size();l++){
    		vector<int> un = Union(possibleCuts[node[1]][k],possibleCuts[node[2]][l]);
    		if(un.size()<=limit){
    			possibleCuts[node[0]].push_back(un);
			}
		}
	}
	possibleCuts[node[0]] = redundant(possibleCuts[node[0]]);
  }
}
vector<bool> existance(Abc_Ntk_t* pNtk, int max){
	vector<bool> result;
	for(int i=0;i<max;i++){
		result.push_back(false);
	}
  Abc_Obj_t* pObj;
  int i; 
  Abc_NtkForEachNode(pNtk, pObj, i) {
    result[Abc_ObjId(pObj)] = true;
    Abc_Obj_t* pFanin;
    int j;
    Abc_ObjForEachFanin(pObj, pFanin, j) {
    	result[Abc_ObjId(pFanin)] = true;
    }
  }
  return result;
}
int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c; int max = 0; vector<vector<vector<int>>> possibleCuts; vector<bool> exist;
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
  Lsv_NtkPrintNodes(pNtk,max);
  exist = existance(pNtk,max);
  possibleCuts = baseCase(max);
  Incremental(pNtk,possibleCuts,atoi(argv[1]));
  for(int i=0;i<possibleCuts.size();i++){
  	if(exist[i]){
	    for(int j=0;j<possibleCuts[i].size();j++){
	    	cout<<i<<": ";
	  		for(int k=0;k<possibleCuts[i][j].size();k++){
	  			cout<<possibleCuts[i][j][k]<<" ";
			  	}
			  	cout<<endl;
		  	}		
	  	}
  	}
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_print_nodes [-h]\n");
  Abc_Print(-2, "\t        prints the nodes in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}
