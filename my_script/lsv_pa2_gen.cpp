#include<iostream>
#include<fstream>
#include<string>
#include<cstdlib>
using namespace std;

int main(int argc, char **argv){
    ofstream out(argv[1]);
    string benchs[8] = {"adder.blif", "div.blif", "int2float.blif", "log2.blif", "mem_ctrl.blif", "router.blif", "sqrt.blif", "square.blif"};
    string dir = "./lsv/pa1/benchmarks/";
    int mins[8] = {258, 130, 19, 34, 1206, 91, 193, 193};
    int maxs[8] = {476, 28866, 278, 30957, 1208, 347, 24810, 18676};
    // for (auto b : benchs){
    //     cout << b << " ";
    // }
    // cout << endl;
    for (int i=0; i<8; i++){
        out << "read " << dir << benchs[i] << endl;
        out << "strash" << endl;
        for (int j=0; j<8; j++){
            int node_id = (rand() % (maxs[i] - mins[i] + 1)) + mins[i];
            out << "lsv_sdc " << node_id << endl;
            out << "lsv_odc " << node_id << endl;
        }
    }
    out << "quit" << endl;
    return 0;
}
