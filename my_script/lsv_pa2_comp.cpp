#include<iostream>
#include<fstream>
#include<sstream>
#include<string>
using namespace std;

int main(int argc, char **argv){
    ifstream in1(argv[1]);
    ifstream in2(argv[2]);
    stringstream ss1;
    stringstream ss2;
    string line1, line2;
    bool is_diff = false;
    string curr_inst;
    while (getline(in1, line1)){
        // cout << line1 << endl;
        string abc, inst;
        ss1 << line1;
        ss1 >> abc;
        // cout << abc << endl;
        if (abc != "abc"){
            // cout << "next\n";
            ss1.clear();
            ss1.str("");
            continue;
        }
        ss1 >> abc >> inst;
        // cout << "inst: " <<  inst << endl;
        bool keep_find = false;
        if (inst == "lsv_sdc" || inst == "lsv_odc"){
            // compare with the other file
            keep_find = true;
            if (in2.eof()){
                cout << "file length is different\n";
                is_diff = true;
                break;
            }
            while (keep_find && !in2.eof()){
                getline(in2, line2);
                string abc2, inst2;
                ss2 << line2;
                ss2 >> abc2;
                if (abc2 != "abc"){
                    ss2.clear();
                    ss2.str("");
                    continue;
                }
                ss2 >> abc2 >> inst2;
                if (inst2 == "lsv_sdc" || inst2 == "lsv_odc"){
                    
                    keep_find = false;
                    if (line1 != line2){
                        cout << "warning inst1 != inst2\n";
                        cout << "inst1: " << line1 << endl;
                        cout << "inst2: " << line2 << endl;
                    }
                    curr_inst = line1;
                    // get the answers
                    getline(in1, line1);
                    ss1.clear();
                    ss1.str("");
                    ss1 << line1;
                    getline(in2, line2);
                    ss2.clear();
                    ss2.str("");
                    ss2 << line2;
                    // compare the result
                    while (ss1 >> abc){
                        ss2 >> abc2;
                        if (abc != abc2){
                            is_diff = true;
                            cout << "different inst: " << curr_inst << endl;
                            cout << "file 1:" << line1 << endl;
                            cout << "file 2:" << line2 << endl;
                            break;
                        }
                    }
                }
                ss2.clear();
                ss2.str("");
            }
        }
        ss1.clear();
        ss1.str("");
    }

    string abc, inst;
    while (getline(in2, line2)){
        ss2 << line2;
        ss2 >> abc;
        if (abc != "abc"){
            ss2.clear();
            ss2.str("");
            continue;
        }
        ss2 >> abc >> inst;
        if (inst == "lsv_sdc" || inst == "lsv_odc"){
            is_diff = true;
            cout << "file length is not the same\n";
            cout << "file1 last inst: " << curr_inst << endl;
            cout << "file2 inst: " << line2 << endl;
            break;
        }
    }
    if (!is_diff){
        cout << "the same\n";
    }
    else {
        cout << "different!\n";
    }
    return 0;
}
