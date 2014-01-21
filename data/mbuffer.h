#ifndef __MBUFFER_H__
#define __MBUFFER_H__

#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CIN_DATA_MBUFFER_SIZE 2

#define CIN_DATA_MBUFFER_ERR_NONE       0
#define CIN_DATA_MBUFFER_ERR_MEMORY     1

typedef struct image_mbuffer {
  void* data[CIN_DATA_MBUFFER_SIZE];
  int active[CIN_DATA_MBUFFER_SIZE];
  int empty[CIN_DATA_MBUFFER_SIZE];
  int write_buffer;
  int read_buffer;
  pthread_mutex_t mutex;
  pthread_cond_t signal;
} mbuffer_t;

int mbuffer_init(mbuffer_t *buffer, int data_size);
void* mbuffer_get_write_buffer(mbuffer_t *buffer);
void mbuffer_write_done(mbuffer_t *buffer);
void* mbuffer_get_read_buffer(mbuffer_t *buffer);
void mbuffer_read_done(mbuffer_t *buffer);

#ifdef __cplusplus
}
#endif

#endif
