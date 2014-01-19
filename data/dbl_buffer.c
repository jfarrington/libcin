#include <stdint.h>

#include "dbl_buffer.h"

uint16_t* dbl_buffer_getbuffer(dbl_buffer_t *buffer){
  /* Get the current buffer pointer and return */
  return buffer->image[buffer->active];
}


