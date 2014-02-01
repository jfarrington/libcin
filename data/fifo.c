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

int fifo_init(fifo *f, int elem_size, long int size, int readers){
  /* Initialize the fifo */
  /* NOTE : if this fails then it causes memory leaks !*/

  f->data = malloc(size * (long int)elem_size);
  if(f->data == NULL){
    return FIFO_ERR_MEMORY;
  } 

  /* set the initial parameters */
  f->head = f->data;
  int i;
  for(i=0;i<FIFO_MAX_READERS;i++){
    f->tail[i] = f->data;
  }
  f->end  = f->data + ((size - 1) * (long int)elem_size);
  f->size = size;
  f->elem_size = elem_size;

  f->full = 0;
  f->readers = readers;
  f->overruns = 0;

  /* Setup mutex */

  pthread_mutex_init(&f->mutex,NULL);
  pthread_cond_init(&f->signal, NULL);

  return FIFO_NOERR;
}

long int fifo_used_bytes(fifo *f){
  long int bytes = 0;

  int i;
  long int used;
  for(i=0;i<f->readers;i++){
    if(f->head >= f->tail[i]){
      used = (long int)(f->head - f->tail[i]);  
    } else {
      used = (long int)((f->end - f->tail[i]) + (f->head - f->data));
    }
    if(used > bytes){
      bytes = used;
    }
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

  return (percent * 100.0);
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

  /* Check all the tail pointers */
  
  int i;
  for(i=0;i<f->readers;i++){
    if((f->head == f->end) && (f->tail[i] == f->data)){
      f->full = 1;
      f->overruns++;
      goto cleanup;
    } else if((f->head + f->elem_size) == f->tail[i]){
      f->full = 1;
      f->overruns++;
      goto cleanup;
    }
  }

  if(f->head == f->end){
    f->head = f->data;
    f->full = 0;
  } else {
    f->head += f->elem_size;
    f->full = 0;
  }

cleanup:
  pthread_cond_broadcast(&f->signal);
  pthread_mutex_unlock(&f->mutex);
}

void* fifo_get_tail(fifo *f, int reader){
  /* Return the tail pointer or NULL if the FIFO is empty */

  void* tail;

  pthread_mutex_lock(&f->mutex);
  while(f->tail[reader] == f->head){
    pthread_cond_wait(&f->signal, &f->mutex);
  }

  tail = f->tail[reader];
  pthread_mutex_unlock(&f->mutex);

  return tail;
}

void fifo_advance_tail(fifo *f, int reader){
  /* Return the tail pointer and advance the FIFO */

  pthread_mutex_lock(&f->mutex);

  /* If the head and tail are the same, FIFO is empty */
  if(f->tail[reader] == f->head){
    return;
  }

  if(f->tail[reader] == f->end){
    f->tail[reader] = f->data;
  } else {
    f->tail[reader] += f->elem_size;
  }

  pthread_mutex_unlock(&f->mutex);
}

