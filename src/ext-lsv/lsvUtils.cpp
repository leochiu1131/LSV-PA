#include "lsvCmd.h"

ABC_NAMESPACE_IMPL_START

/**
 * auxiliary function: printBits()
*/
void printBits(unsigned int num, int size) {
    unsigned int maxPow = 1<<(size-1);
    // printf("MAX POW : %u\n",maxPow);
    for(int i=0; i<size; ++i){
      // print last bit and shift left.
      printf("%u", !!(num&maxPow));
      num = num<<1;
    }
    // printf("\n");
}

ABC_NAMESPACE_IMPL_END