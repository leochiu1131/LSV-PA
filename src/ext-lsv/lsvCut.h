#include "stdlib.h"

typedef struct Cut_t_       Cut_t;
struct Cut_t_
{
    int              cut_size;
    int*             cut_nodes;
};

// Reference: https://www.geeksforgeeks.org/c-program-to-sort-an-array-in-ascending-order/
void selection_sort(int* nodes, int size){
    for(int i = 0; i < size-1; i++){
      int min_idx = i;
      // Find the minimum element in the remaining
      // unsorted array
      for (int j = i + 1; j < size; j++) {
          if (nodes[j] < nodes[min_idx]) {
              min_idx = j;
          }
      }
      // Swap the found minimum element with the first
      // element
      int temp = nodes[min_idx];
      nodes[min_idx] = nodes[i];
      nodes[i] = temp;
    }
    return;
}
Cut_t* cut_new(int* nodes, int size){
    Cut_t* cut = (Cut_t*)malloc(sizeof(Cut_t));
    cut->cut_nodes = (int*)malloc(sizeof(int)*size);
    
    int cutSize = 1;
    cut->cut_nodes[0] = nodes[0];
    selection_sort(nodes, size);
    for(int i = 1; i < size; i++){
        if(cut->cut_nodes[cutSize-1] == nodes[i]){
            continue;
        }
        else{
            cut->cut_nodes[cutSize] = nodes[i];
            cutSize++;
        }
    }
    cut->cut_size = cutSize;
    return cut;
}
Cut_t* cut_new(int* nodes_1, int size_1, int* nodes_2, int size_2){
    int* nodes = (int*)malloc(sizeof(int)*(size_1+size_2));
    for(int i = 0; i < size_1; i++){
        nodes[i] = nodes_1[i];
    }
    for(int i = 0; i < size_2; i++){
        nodes[size_1+i] = nodes_2[i];
    }
    Cut_t* cut = cut_new(nodes, size_1+size_2);
    printf("cut_size = %d\n", cut->cut_size);
    free(nodes);
    printf("free nodes\n");
    return cut;
}
Cut_t* cut_new(Cut_t* cut1, Cut_t* cut2){
    return cut_new(cut1->cut_nodes, cut1->cut_size, cut2->cut_nodes, cut2->cut_size);
}
Cut_t* cut_copy(Cut_t* cut){
    Cut_t* new_cut = (Cut_t*)malloc(sizeof(Cut_t));
    new_cut->cut_size = cut->cut_size;
    new_cut->cut_nodes = (int*)malloc(sizeof(int)*cut->cut_size);
    for(int i = 0; i < cut->cut_size; i++){
        new_cut->cut_nodes[i] = cut->cut_nodes[i];
    }
    return new_cut;
}   
void cut_free(Cut_t* cut){
    free(cut->cut_nodes);
    free(cut);
    return;
}
void cuts_free(Cut_t** cuts, int size){
    if(cuts == NULL){
        return;
    }
    for(int i = 0; i < size; i++){
        cut_free(cuts[i]);
    }
    // printf("free cuts\n");
    free(cuts);
    return;
}
int cut_equal(Cut_t* cut1, Cut_t* cut2){
    if(cut1->cut_size != cut2->cut_size){
        return 0;
    }
    for(int i = 0; i < cut1->cut_size; i++){
        if(cut1->cut_nodes[i] != cut2->cut_nodes[i]){
            return 0;
        }
    }
    return 1;
}
int cut_compare(Cut_t* cut1, Cut_t* cut2){
    if(cut1->cut_size != cut2->cut_size){
        return cut1->cut_size - cut2->cut_size;
    }
    for(int i = 0; i < cut1->cut_size; i++){
        if(cut1->cut_nodes[i] != cut2->cut_nodes[i]){
            return cut1->cut_nodes[i] - cut2->cut_nodes[i];
        }
    }
    return 0;
}
Cut_t** cut_sort_and_eliminate(Cut_t** cuts, int& size, int k_feasible){
    qsort(cuts, size, sizeof(Cut_t*), (int(*)(const void*, const void*))cut_compare);
    // eliminate duplicate cuts
    int i = 0;
    for(int j = 1; j < size; j++){
        if(cuts[j]->cut_size > k_feasible){
            cut_free(cuts[j]);
            continue;
        }
        if(cut_compare(cuts[i], cuts[j]) != 0){
            i++;
            cuts[i] = cuts[j];
        }
        else{
            cut_free(cuts[j]);
        }
    }
    if(cuts[0]->cut_size > k_feasible){
        cut_free(cuts[0]);
        size = 0;
        free(cuts);
        return NULL;
    }
    size = i+1;
    return cuts;
}
void print_cut(Cut_t* cut){
    for(int i = 0; i < cut->cut_size; i++){
        printf("%d ", cut->cut_nodes[i]);
    }
    printf("\n");
    return;
}