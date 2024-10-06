#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <stack>
#include <map>
#include <set>
#include <iostream>
#include <algorithm>  
#include <iterator>   

using namespace std;
static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintcut (Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_CommandPrintcut, 0);
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


struct MapCompare {
    bool operator()(Abc_Obj_t* a, Abc_Obj_t* b) const {
        return Abc_ObjId(a) < Abc_ObjId(b); 
    }
};

struct SetCompare {
    // 重載 operator() 來作為比較器
    bool operator()(const std::set<Abc_Obj_t*,MapCompare>& a, const std::set<Abc_Obj_t*,MapCompare>& b) const {
        return a.size() < b.size(); 
    }
};





 map<Abc_Obj_t*,  std::set<std::set<Abc_Obj_t*,MapCompare>>,MapCompare > isVisited_map;


bool isSubset(const std::set<Abc_Obj_t*, MapCompare>& set1, const std::set<Abc_Obj_t*, MapCompare>& set2)
{
    for (const auto& elem : set1)
    {
        if (set2.find(elem) == set2.end()) 
        {
            return false;
        }
    }
    return true;  
}




void Mergeset(Abc_Obj_t* ptr1 , Abc_Obj_t* ptr2 , Abc_Obj_t* Mergeresult, int k)
{
    std::set<std::set<Abc_Obj_t*, MapCompare>> com_mergeset;
    for (const auto& elem1 : isVisited_map[ptr1]) 
    {
     for (const auto& elem2 : isVisited_map[ptr2]) 
      {            
        std::set<Abc_Obj_t*,MapCompare> mergeset;
        std::set_union(elem1.begin(), elem1.end(), elem2.begin(), elem2.end(), std::inserter(mergeset, mergeset.begin()));
        if(mergeset.size() <= k )
        {
          com_mergeset.insert(mergeset);
        }
        
      }
    }

    auto& dest = com_mergeset;
    std::set<std::set<Abc_Obj_t*, MapCompare>> to_erase;  

    for (auto it = dest.begin(); it != dest.end(); ++it)
    {
        for (auto itin = dest.begin(); itin != dest.end(); ++itin)
        {
            if(itin == it)
            {
              continue;
            }
            if (isSubset(*itin, *it))
            {
                to_erase.insert(*it);  
            }
            if (isSubset(*it, *itin))
            {
                to_erase.insert(*itin); 
            }
        }
    }
    for (const auto& elem : to_erase)
    {
        com_mergeset.erase(elem);
    }
    for (const auto& setelem : com_mergeset)
    {
        isVisited_map[Mergeresult].insert(setelem);
    }

}
 






void findNodes(Abc_Obj_t* topNode, int k) {
    if (topNode == nullptr) {
        std::cout << "Top node is null." << std::endl;
        return;
    }

    std::stack<Abc_Obj_t*> q;
    q.push(topNode); 

    while (!q.empty()) {
        Abc_Obj_t* current = q.top();
        // 將左右子節點放入隊列，層次 +1
        if(isVisited_map.find(current)!= isVisited_map.end())
        {
          q.pop();
        }
        else
        {
            if(Abc_ObjFaninNum(current)==2)
            {
                if((isVisited_map.find(Abc_ObjFanin0(current)) != isVisited_map.end() ) && (isVisited_map.find(Abc_ObjFanin1(current)) != isVisited_map.end() ))
                { 
                  
                  set<Abc_Obj_t*,MapCompare> initial ;
                  initial.insert(current);
                  isVisited_map[current].insert(initial) ;
                  Mergeset(Abc_ObjFanin0(current),Abc_ObjFanin1(current),current,k);
                  q.pop();
                }
                if (isVisited_map.find(Abc_ObjFanin0(current)) == isVisited_map.end() ) {
                  q.push(Abc_ObjFanin0(current));
                }
                if (isVisited_map.find(Abc_ObjFanin1(current)) == isVisited_map.end() ) {
                  q.push(Abc_ObjFanin1(current));
                }
            } 
            else if( Abc_ObjFaninNum(current)==1)
            {
               if((isVisited_map.find(Abc_ObjFanin0(current)) != isVisited_map.end() ) )
               {
                  set<Abc_Obj_t*,MapCompare> initial ;
                  initial.insert(current);
                  isVisited_map[current] =  isVisited_map[Abc_ObjFanin0(current)];
                  isVisited_map[current].insert(initial);
                  q.pop();
               }
               else
               {
                   q.push(Abc_ObjFanin0(current));
               }
            }
            else if(Abc_ObjFaninNum(current)==0)
            {
              set<Abc_Obj_t*,MapCompare> initial ;
              initial.insert(current);
              isVisited_map[current].insert(initial);
              q.pop();
            }
            else
            {
              q.pop();
            }
            
        }
    }
}



int Lsv_CommandPrintcut(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  if (argc != 2) {
    Abc_Print(-1, "Invalid number of arguments. Usage: lsv printcut <k>\n");
    return 1;
  }

  int k = atoi(argv[1]);
 
  if (k <= 0) {
    Abc_Print(-1, "Invalid k value. It should be a positive integer.\n");
    return 1;
  }
  Abc_Obj_t* pObj;
  int z;
  Abc_NtkForEachObj(pNtk, pObj, z)
  {
     if(Abc_ObjIsPi(pObj)==1)
     {
       
       set<Abc_Obj_t*,MapCompare> initial ;
       initial.insert(pObj);
       isVisited_map[pObj].insert(initial) ;
     }
  }
  Abc_NtkForEachObj(pNtk, pObj, z)
  {
      findNodes(pObj,k);
  }
  int count = 0;
  for (map<Abc_Obj_t*,  std::set<std::set<Abc_Obj_t*,MapCompare>>,MapCompare> ::iterator it = isVisited_map.begin(); it != isVisited_map.end(); ++it) {
  std::set<std::set<Abc_Obj_t*,MapCompare>> ttt = it->second;
  std::vector<std::set<Abc_Obj_t*,MapCompare>> tttVec(ttt.begin(), ttt.end());
  std::sort(tttVec.begin(), tttVec.end(), SetCompare());
  // cout<<Abc_ObjId(it->first)<<": "<<it->second.size()<<endl;
  for (const auto& uuu : tttVec) 
  {
        cout<<Abc_ObjId(it->first)<<": ";
        for (const auto& hello : uuu) 
        {
            cout<<Abc_ObjId(hello)<<" ";
        }
        cout<<endl;
   }
    }


   return 0;
  }
