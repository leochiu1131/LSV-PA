#include <iostream>
#include <unordered_map>
#include <set>

using namespace std;

unordered_map<int, set<set<int>>> hash_table;

// 用於打印集合
void printSets(const set<int>& s) {
    for (int num : s) {
        cout << num << " ";
    }
    cout << endl;
}

int countTotalSets(const unordered_map<int, set<set<int>>>& hash_table) {
    int total_sets = 0;
    
    // 遍歷 hash_table
    for (const auto& pair : hash_table) {
        // 對每個 key 對應的 set<set<int>> 計數
        total_sets += pair.second.size();
    }
    
    return total_sets;
}

int main() {
    // 初始化哈希表
    hash_table[1] = { {1, 2}, {3}, {3} };
    hash_table[2] = {  };

    set<set<int>> result;  

    const auto& set1 = hash_table[1];
    const auto& set2 = hash_table[2];

    // Cross product
    for (const auto& s1 : set1) {
        for (const auto& s2 : set2) {
            set<int> temp_set;

            temp_set.insert(s1.begin(), s1.end());  
            temp_set.insert(s2.begin(), s2.end());  
            for (int elem : temp_set) {
                cout << elem << " ";
              }
            result.insert(temp_set);
        }
    }

    // 將結果存入 hash_table[3]
    hash_table[3] = result;

    // 打印 hash_table[3]
    cout << "hash_table[3]:" << endl;
    for (const auto& res_set : hash_table[3]) {
        printSets(res_set);
    }

    // 計算 hash_table 中共有多少個集合
    int total_sets = countTotalSets(hash_table);
    //cout << "Total number of sets in hash_table: " << total_sets << endl;

    return 0;
}
