#ifndef __CIN_DATA__H 
#define __CIN_DATA__H

#include "mbuffer.h"
#include "fifo.h"
#include "pthread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Definitions */

#define MAX_THREADS             10
#define CIN_DATA_MONITOR_UPDATE 100000 // in usec

/* Datastructures */

typedef struct cin_data_threads {
  pthread_t thread_id;
  int started;
} cin_data_threads_t;

typedef struct image_buffer {
  cin_data_frame_t *data;
  int waiting;
  pthread_mutex_t mutex;
  pthread_cond_t signal_push;
  pthread_cond_t signal_pop;
} image_buffer_t;

typedef struct cin_data_thread_data {
  /* FIFO Elements */
  fifo *packet_fifo;  
  fifo *frame_fifo;
  fifo *image_fifo;

  /* Image double buffer */
  mbuffer_t *image_dbuffer;

  /* Buffer for images */

  image_buffer_t *image_buffer;

  /* Interface */
  struct cin_port* dp; 

  /* Statistics */
  struct timespec framerate;
  unsigned long int dropped_packets;
  unsigned long int mallformed_packets;
  uint16_t last_frame;

  struct cin_data_stats stats;
  pthread_mutex_t stats_mutex;
  pthread_cond_t stats_signal;

  /* Mutex for sequential frame access */
} cin_data_thread_data_t;

typedef struct cin_data_packet {
  unsigned char data[CIN_DATA_MAX_MTU];
  int size;
  struct timespec timestamp;
} cin_data_packet_t;

typedef struct cin_data_proc {
  void* (*input_get) (void*);
  void* (*input_put) (void*);
  void* input_args;

  void* (*output_put) (void*);
  void* (*output_get) (void*);
  void* output_args;
} cin_data_proc_t;

/* Templates for functions */

int cin_data_thread_start(cin_data_threads_t *thread, 
                          void *(*func) (void *),
                          void *arg, int policy, int priority);

/* Threads for processing stream */

void *cin_data_listen_thread(void *args);
void *cin_data_monitor_thread(void);
void *cin_data_assembler_thread(void *args);
void *cin_data_descramble_thread(void *args);
void *cin_data_monitor_output_thread(void);

/* Buffer Routines */

void* cin_data_buffer_push(void *arg);
void cin_data_buffer_pop(void *arg);

/* Profiling Functions */
struct timespec timespec_diff(struct timespec start, struct timespec end);
void timespec_copy(struct timespec *dest, struct timespec *src);

#ifdef __cplusplus
}
#endif

#endif
