#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"

#include <queue>
#include <list>
#include <unordered_set>
#include <algorithm>

#define DEBUG_MESG_ENABLE 1
#define DUMP_ANSWER 1




using namespace std;

static int Lsv_CommandPrintCuts(Abc_Frame_t *pAbc, int argc, char **argv);

void init_lsv(Abc_Frame_t *pAbc)
{
  Cmd_CommandAdd(pAbc, "LSV", "lsv_printcut", Lsv_CommandPrintCuts, 0);
}

void destroy_lsv(Abc_Frame_t *pAbc) {}

Abc_FrameInitializer_t frame_initializer_lsv = {init_lsv, destroy_lsv};

struct PackageRegistrationManager
{
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer_lsv); }
} lsvPackageRegistrationManager_lsv;

void Lsv_NtkPrintCuts(Abc_Ntk_t *pNtk,int CutsetsNumer)
{
  Abc_Obj_t *pObj;
  int i;






  Abc_NtkForEachNode(pNtk, pObj, i)
  {
    if(Abc_ObjId(pObj) != 10)
      continue;

    #if DEBUG_MESG_ENABLE
      printf("Host Id = %d \n", Abc_ObjId(pObj));
    #endif
    #if DUMP_ANSWER
      printf("%d: %d\n",Abc_ObjId(pObj) ,Abc_ObjId(pObj) );
    #endif

   


    std::list<std::list<Abc_Obj_t>> current_list_array;
    std::list<std::list<Abc_Obj_t>> next_list_array;
    std::list<Abc_Obj_t> *head_node = new std::list<Abc_Obj_t>;


    std::list<std::list<Abc_Obj_t>> ans_list_array;


    // initial to set head node
    head_node->push_front(*pObj);
    current_list_array.push_front(*head_node);
    delete head_node;



    // -> repeat until all nodes are primary inputs
    // ! start repeat here
    for (int ITERATION = 0; ITERATION < 10; ITERATION++)
    {
      if (current_list_array.empty()) {
        #if DEBUG_MESG_ENABLE
          printf("current_list_array is empty. Exiting loop.\n");
        #endif
        break;
      }



      // Dump current_list_array

      #if DEBUG_MESG_ENABLE
      printf("[INIT] Current list array at iteration %d:\n", ITERATION);
      for ( auto& list : current_list_array)
      {
        printf("[INIT] Current List: ");
        for ( auto& node : list)
        {
          printf("%d ", Abc_ObjId(&node));
        }
        printf("\n");
      }
      #endif




      int total_node_number = 0;
      int list_index = 0;
      int node_index = 0;
      std::vector<Abc_Obj_t*> temp_list;
      std::vector<std::vector<Abc_Obj_t *>> node_array(current_list_array.size());
      std::vector<int> list_node_number(current_list_array.size());







      list_index = 0;
      for (const auto& list : current_list_array)
      {
        node_index = 0;
        int list_size = list.size();
        node_array[list_index].resize(list_size);
        for (const auto& node : list)
        {
          node_array[list_index][node_index] = const_cast<Abc_Obj_t*>(&node);
          total_node_number++;
          node_index++;
        }
        list_node_number[list_index] = node_index;
        list_index++;
      }

















      for (int list_iteration_index = 0; list_iteration_index < node_array.size(); list_iteration_index++)
      {
        const int node_number = list_node_number[list_iteration_index];


    
        #if DEBUG_MESG_ENABLE
          printf("node_array size: %lu\n", node_array.size());
          for (size_t idx = 0; idx < node_array.size(); ++idx)
          {
            printf("node_array[%lu] size: %lu\n", idx, node_array[idx].size());
            printf("node_array[%lu] content: ", idx);
            for (auto node : node_array[idx])
            {
              printf("%d ", Abc_ObjId(node));
            }
            printf("\n");
          }
        #endif


        std::list<Abc_Obj_t> next_list [node_number];
        std::queue<Abc_Obj_t> resolve_queue [node_number];
        #if DEBUG_MESG_ENABLE
        printf("Declare resolve_queue with same as node_number: %d\n", node_number);
        #endif
       
        for (int i = 0; i < node_number; i++)
        {
       

          // to push all node from node array to the resolve_queue
          
          for (int j = 0; j < node_number; j++)
          {
            Abc_Obj_t *current_node = node_array[list_iteration_index][j];
            resolve_queue[i].push(*current_node);
          }

          // make node_array to rotate

        #if DEBUG_MESG_ENABLE
          printf("Before node rotation %d: ", i);
          for (int k = 0; k < node_number; ++k)
          {
            printf("%d ", Abc_ObjId(node_array[list_iteration_index][k]));
          }
          printf("\n");
        #endif
          Abc_Obj_t *temp_node;
          temp_node = node_array[list_iteration_index][0];
          for (int k = 0; k < node_number; ++k)
          {

            if (node_number - 1 == k)
              node_array[list_iteration_index][k] = temp_node;
            else
              node_array[list_iteration_index][k] = node_array[list_iteration_index][k + 1];
          }
        #if DEBUG_MESG_ENABLE
          printf("After node rotation %d: ", i);
          for (int k = 0; k < node_number; ++k)
          {
            printf("%d ", Abc_ObjId(node_array[list_iteration_index][k]));
          }
          printf("\n");
        #endif
        }

        for (int i = 0; i < node_number; ++i)
        {
          Abc_Obj_t current_node = resolve_queue[i].front();
          resolve_queue[i].pop();

          Abc_Obj_t *pFanin;
          int fanin_Id = 0;
          if(!Abc_ObjIsPi(&current_node))
          Abc_ObjForEachFanin(&current_node, pFanin, fanin_Id)
          {
            resolve_queue[i].push(*pFanin);
          }else
          {
            resolve_queue[i].push(current_node);
          }
        }

        for (int i = 0; i < node_number; ++i)
        {
          #if DEBUG_MESG_ENABLE
          printf("Queue %d: ", i);
          #endif
          std::queue<Abc_Obj_t> temp_queue = resolve_queue[i];
          while (!temp_queue.empty())
          {
            Abc_Obj_t node = temp_queue.front();
            temp_queue.pop();
            #if DEBUG_MESG_ENABLE
            printf("%d ", Abc_ObjId(&node));
            #endif
          }
          #if DEBUG_MESG_ENABLE
          printf("\n");
          #endif
        }

        // !  find is there any dumplicate node in the resolve_queue

        for (int i = 0; i < node_number; ++i)
        {
          std::queue<Abc_Obj_t> unique_queue;
          std::unordered_set<int> seen_ids;

          while (!resolve_queue[i].empty())
          {
            Abc_Obj_t node = resolve_queue[i].front();
            resolve_queue[i].pop();

            if (seen_ids.find(Abc_ObjId(&node)) == seen_ids.end())
            {
              unique_queue.push(node);
              seen_ids.insert(Abc_ObjId(&node));
            }
          }

          resolve_queue[i] = unique_queue;
        }

        // ! check is there cutset larger than specific number

        // ! check is all nodes are primary input
        // Check if all nodes in the resolve_queue are primary inputs without modifying the queue
        current_list_array.clear();
        for (int i = 0; i < node_number; ++i)
        {
          bool allPrimaryInputs = true;
          std::queue<Abc_Obj_t> temp_queue = resolve_queue[i];

          while (!temp_queue.empty())
          {
            Abc_Obj_t *node = &temp_queue.front();
            next_list[i].push_front(*node);
            temp_queue.pop();
            if (!Abc_ObjIsPi(node))
            {
              allPrimaryInputs = false;
              // break;
            }
          }
          // ! make sure next_list is unique
          
            next_list[i].sort([]( Abc_Obj_t &a,  Abc_Obj_t &b) {
              return Abc_ObjId(&a) < Abc_ObjId(&b);
            });

            next_list[i].unique([](Abc_Obj_t& a, Abc_Obj_t& b) {
              return Abc_ObjId(&a) == Abc_ObjId(&b);
            });






          // Save output IDs
          std::vector<int> output_ids;
          std::list<Abc_Obj_t> ans_list;



          for (auto node : next_list[i])
          {
            output_ids.push_back(Abc_ObjId(&node));
            ans_list.push_back(node);
          }

            

            // Ensure every list in ans_list_array is unique


          ans_list_array.push_front(ans_list);


            // Ensure every list in ans_list_array is unique
            ans_list_array.sort([]( std::list<Abc_Obj_t>& a,  std::list<Abc_Obj_t>& b) {
              if (a.size() != b.size()) return a.size() < b.size();
              return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(), []( Abc_Obj_t& x,  Abc_Obj_t& y) {
                return Abc_ObjId(&x) < Abc_ObjId(&y);
              });
            });
            ans_list_array.unique([]( std::list<Abc_Obj_t>& a,  std::list<Abc_Obj_t>& b) {
              return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), []( Abc_Obj_t& x,  Abc_Obj_t& y) {
                return Abc_ObjId(&x) == Abc_ObjId(&y);
              });
            });




            #if DUMP_ANSWER
            // Dump ans_list_array
              #if DEBUG_MESG_ENABLE
              printf("[ANSWER]ans_list_array: \n");
              for ( auto& list : ans_list_array) {
                printf("[ANSWER] \tList: ");
                for ( auto& node : list) {
                printf("%d ", Abc_ObjId(&node));
                }
                printf("\n");
              }
              #endif
            #endif
          // stop here
          // ! check is there cutset larger than specific number

          if (!allPrimaryInputs && next_list[i].size()< CutsetsNumer)
          {
            next_list_array.push_front(next_list[i]);
          }
        }
       

        //delete[] next_list;
        //delete[] resolve_queue;
      }
       
      current_list_array.clear(); 
      /* printf("[FINAL] Next list array at iteration %d:\n", ITERATION); */
      // Ensure every list in next_list_array is unique
      next_list_array.sort([](std::list<Abc_Obj_t>& a, std::list<Abc_Obj_t>& b) {
        if (a.size() != b.size()) return a.size() < b.size();
        return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(), [](Abc_Obj_t& x, Abc_Obj_t& y) {
          return Abc_ObjId(&x) < Abc_ObjId(&y);
        });
      });
      next_list_array.unique([](std::list<Abc_Obj_t>& a, std::list<Abc_Obj_t>& b) {
        return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](Abc_Obj_t& x, Abc_Obj_t& y) {
          return Abc_ObjId(&x) == Abc_ObjId(&y);
        });
      });




      for (auto& list : next_list_array)
      { 
        #if DEBUG_MESG_ENABLE
         printf("[FINAL] Next List: ");
        #endif
        for (auto& node : list)
        {
        #if DEBUG_MESG_ENABLE
          printf("%d ", Abc_ObjId(&node));
        #endif
        } 
        current_list_array.push_front(list);
        #if DEBUG_MESG_ENABLE
          printf("\n"); 
        #endif
      }
      next_list_array.clear();

    


      #if DEBUG_MESG_ENABLE
      printf("--------------------------------------------\n");
      #endif



    }

    //show answer here
    #if DUMP_ANSWER
    for ( auto& list : ans_list_array) {
      printf("%d: ", Abc_ObjId(pObj));
      for ( auto& node : list) {
        printf("%d ", Abc_ObjId(&node));
      }
      printf("\n");
    }
    #endif

    // list_length indecate the list number after resolution

  }
}

int Lsv_CommandPrintCuts(Abc_Frame_t *pAbc, int argc, char **argv)
{

  int numberCutset = 3;
  Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
  int c;
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF)
  {
    switch (c)
    {
    case 'h':
      goto usage;
    default:
      goto usage;
    }
  }
  if (!pNtk)
  {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  if (argc > 1) {
    #if DEBUG_MESG_ENABLE
      printf("First argument: %s\n", argv[1]);
    #endif
  }else
  {
    printf("Please type number of cutset 3~6 ");
  }
  numberCutset = atoi(argv[1]);
  Lsv_NtkPrintCuts(pNtk, numberCutset);
  
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_printcut [-h]\n");
  Abc_Print(-2, "\t        prints the nodes in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}