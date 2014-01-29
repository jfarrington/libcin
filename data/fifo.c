#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#include "cin.h"
#include "fifo.h"

/* -----------------------------------------------------------------------------------------
 *
 * FIFO Functions
 *
 * -----------------------------------------------------------------------------------------
 */

int fifo_init(fifo *f, int elem_size, long int size){
  /* Initialize the fifo */
  /* NOTE : if this fails then it causes memory leaks !*/

  f->data = malloc(size * (long int)elem_size);
  if(f->data == NULL){
    return FIFO_ERR_MEMORY;
  } 

  /* set the initial parameters */
  f->head = f->data;
  f->tail = f->data;
  f->end  = f->data + ((size - 1) * (long int)elem_size);
  f->size = size;
  f->elem_size = elem_size;

  /* store the pointers of the start in both
     head and tail */

  f->head = f->data;
  f->tail = f->data;

  f->full = 0;

  /* Setup mutex */

  pthread_mutex_init(&f->mutex,NULL);
  pthread_cond_init(&f->signal, NULL);

  return FIFO_NOERR;
}

long int fifo_used_bytes(fifo *f){
  long int bytes;

  if(f->head >= f->tail){
    bytes = (long int)(f->head - f->tail);  
  } else {
    bytes = (long int)((f->end - f->tail) + (f->head - f->data));
  }

  return bytes;
}

long int fifo_used_elements(fifo *f){
  return fifo_used_bytes(f) / f->elem_size;
}

double fifo_percent_full(fifo *f){
  long int bytes;
  double percent;
  
  bytes = fifo_used_bytes(f);
  percent = (double)bytes / (double)(f->elem_size * f->size);

  return percent * 100.0;
}

void *fifo_get_head(fifo *f){
  void *head;

  pthread_mutex_lock(&f->mutex);
  head = f->head;
  pthread_mutex_unlock(&f->mutex);

  return head;
}

void fifo_advance_head(fifo *f){
  /* Increment the head pointet */

  pthread_mutex_lock(&f->mutex);

  if((f->head == f->end) && (f->tail == f->data)){
    f->full = 1;
  } else if((f->head + f->elem_size) == f->tail){
    /* FIFO is full. Don't increment */
    f->full = 1;
  } else if(f->head == f->end){
    f->head = f->data;
    f->full = 0;
  } else {
    f->head += f->elem_size;
    f->full = 0;
  }

  pthread_cond_signal(&f->signal);
  pthread_mutex_unlock(&f->mutex);
}

void* fifo_get_tail(fifo *f){
  /* Return the tail pointer or NULL if the FIFO is empty */

  void* tail;

  pthread_mutex_lock(&f->mutex);
  while(f->tail == f->head){
    pthread_cond_wait(&f->signal, &f->mutex);
  }

  tail = f->tail;
  pthread_mutex_unlock(&f->mutex);

  return tail;
}

void fifo_advance_tail(fifo *f){
  /* Return the tail pointer and advance the FIFO */

  pthread_mutex_lock(&f->mutex);

  /* If the head and tail are the same, FIFO is empty */
  if(f->tail == f->head){
    return;
  }

  if(f->tail == f->end){
    f->tail = f->data;
  } else {
    f->tail += f->elem_size;
  }

  pthread_mutex_unlock(&f->mutex);
}

