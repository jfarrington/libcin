#ifndef __CIN_DATA_FIFO__H 
#define __CIN_DATA_FIFO__H

#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Macro Definitions */

#define FIFO_ERR_MEMORY 1
#define FIFO_NOERR      0

/* Datastructures */

typedef struct {
  void *data;
  void *head;
  void *tail;
  void *end;
  long int size;
  int elem_size;
  int full;
  pthread_mutex_t mutex;
  pthread_cond_t signal;
} fifo;

/* FIFO Functions */

void* fifo_get_head(fifo *f);
/* 
 * Return the head of the FIFO element pointed to by f. 
 * This routine will signal that new data is avaliable in
 * the fifo using "pthread_cond_signal"
 */
void* fifo_get_tail(fifo *f);
/* 
 * Return the tail of the FIFO element pointed to by f.
 * This routine will block until data is avaliable, waiting
 * on the signal sent by "fifo_get_head". If data is on the
 * fifo then it will immediately return
 */
void fifo_advance_head(fifo *f);
/*
 * Advance the head pointer, signalling we are done filling
 * the fifo with an element.
 */
void fifo_advance_tail(fifo *f);
/*
 * Advance the tail pointer, signalling we have processed a fifo
 * element and this can be returned
 */
int fifo_init(fifo *f, int elem_size, long int size);
long int fifo_used_bytes(fifo *f);
double fifo_percent_full(fifo *f);
long int fifo_used_elements(fifo *f);

#ifdef __cplusplus
}
#endif

#endif
