#include <stdlib.h>
#include <stdio.h>
#include "fifo.h"

int fifo_init(fifo *f, int elem_size, long int size){
  /* Initialize the fifo */
  /* NOTE : if this fails then it causes memory leaks !*/

  f->data = malloc(size * (long int)elem_size);
  if(f->data == NULL){
    return FALSE;
  } 

  /* set the initial parameters */
  f->head = f->data;
  f->tail = f->data;
  f->end  = f->data + ((size - 1) * elem_size);
  f->size = size;
  f->elem_size = elem_size;

  /* store the pointers of the start in both
     head and tail */

  f->head = f->data;
  f->tail = f->data;
  fprintf(stderr, "==== FIFO initialized with %ld bytes at %p\n", 
          size * (long int)elem_size, f->data);
  return TRUE;
}

long int fifo_used_bytes(fifo *f){
  if(f->head >= f->tail){
    return (long int)(f->head - f->tail);  
  } else {
    return (long int)((f->end - f->head) + f->tail);
  }
}

void *fifo_get_head(fifo *f){
  return f->head;
}

void fifo_advance_head(fifo *f){
  /* Increment the head pointet */

  f->head += f->elem_size;

  if(f->head == f->end){
    f->head = f->data;
  }
}

void* fifo_get_tail(fifo *f){
  /* Return the tail pointer or NULL if the FIFO is empty */

  if(f->tail == f->head){
    return NULL;
  }

  return f->tail;
}

void fifo_advance_tail(fifo *f){
  /* Return the tail pointer and advance the FIFO */

  /* If the head and tail are the same, FIFO is empty */
  if(f->tail == f->head){
    return;
  }

  if(f->tail == f->end){
    f->tail = f->data;
  } else {
    f->tail += f->elem_size;
  }
}

