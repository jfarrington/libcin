#include <stdint.h>
#include <stdio.h>

#include "descramble_map_forward.h"
#include "descramble_map_reverse.h"

int main(){

  int i, index;

  uint32_t *forward, *reverse;

  forward = (uint32_t*)descramble_map_forward_bin;
  reverse = (uint32_t*)descramble_map_reverse_bin;

  for(i =0; i<(descramble_map_forward_bin_len/4); i++){
    if(forward[reverse[i]] != i){
      fprintf(stderr, "%d : %d : %d : ", i, forward[i], reverse[i]);
      index = forward[i];
      fprintf(stderr, "%d\n", forward[reverse[i]]);
    }
  }

}
