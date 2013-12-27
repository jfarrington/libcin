#ifndef _FIFO_H 
#define _FIFO_H 1

#define TRUE 1
#define FALSE 0

/* Datastructures */

typedef struct {
  void *data;
  void *head;
  void *tail;
  void *end;
  long int size;
  int elem_size;
} fifo;

/* Templates for functions */

void* fifo_get_head(fifo *f);
void* fifo_get_tail(fifo *f);

void fifo_advance_head(fifo *f);
void fifo_advance_tail(fifo *f);

int fifo_init(fifo *f, int elem_size, long int size);
long int fifo_used_bytes(fifo *f);

#endif
