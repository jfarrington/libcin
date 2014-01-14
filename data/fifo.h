#ifndef __CIN_DATA_FIFO__H 
#define __CIN_DATA_FIFO__H

#ifdef __cplusplus
extern "C" {
#endif

/* Datastructures */

typedef struct {
  void *data;
  void *head;
  void *tail;
  void *end;
  long int size;
  int elem_size;
  int full;
} fifo;

/* FIFO Functions */

void* fifo_get_head(fifo *f);
void* fifo_get_tail(fifo *f);
void fifo_advance_head(fifo *f);
void fifo_advance_tail(fifo *f);
int fifo_init(fifo *f, int elem_size, long int size);
long int fifo_used_bytes(fifo *f);
double fifo_percent_full(fifo *f);

#ifdef __cplusplus
}
#endif

#endif
