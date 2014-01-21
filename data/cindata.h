#ifndef __CIN_DATA__H 
#define __CIN_DATA__H

#include "mbuffer.h"
#include "fifo.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Definitions */

#define MAX_THREADS  5

/* Datastructures */

struct cin_data_thread_data {
  /* FIFO Elements */
  fifo *packet_fifo;  
  fifo *frame_fifo;
  fifo *image_fifo;

  /* Image double buffer */
  mbuffer_t *image_dbuffer;

  /* Interface */
  struct cin_port* dp; 

  /* Statistics */
  struct timespec framerate;
  unsigned long int dropped_packets;
  unsigned long int mallformed_packets;
  uint16_t last_frame;

  struct cin_data_stats stats;
  pthread_mutex_t stats_mutex;
};

struct cin_data_packet {
  unsigned char data[CIN_DATA_MAX_MTU];
  int size;
  struct timespec timestamp;
};

/* Templates for functions */

/* Threads for processing stream */

void *cin_data_listen_thread(void);
void *cin_data_monitor_thread(void);
void *cin_data_assembler_thread(void);
void *cin_data_descramble_thread(void);
void *cin_data_monitor_output_thread(void);

/* Profiling Functions */
struct timespec timespec_diff(struct timespec start, struct timespec end);
void timespec_copy(struct timespec *dest, struct timespec *src);

#ifdef __cplusplus
}
#endif

#endif
