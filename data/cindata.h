#ifndef _CIN_LISTEN_H 
#define _CIN_LISTEN_H 

/* Definitions */

#ifndef TRUE
#define TRUE  1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#define MAX_THREADS             5

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

struct cin_data_thread_data {
  /* FIFO Elements */
  fifo *packet_fifo;  
  fifo *frame_fifo;

  /* Interface */
  struct cin_port* dp; 

  /* Statistics */
  double framerate;
  unsigned long int dropped_packets;
  unsigned long int mallformed_packets;
  uint16_t last_frame;

};

struct cin_data_packet {
  unsigned char *data;
  int size;
  struct timeval timestamp;
};

/* Templates for functions */

/* FIFO Functions */

void* fifo_get_head(fifo *f);
void* fifo_get_tail(fifo *f);
void fifo_advance_head(fifo *f);
void fifo_advance_tail(fifo *f);
int fifo_init(fifo *f, int elem_size, long int size);
long int fifo_used_bytes(fifo *f);
double fifo_percent_full(fifo *f);

/* Threads for processing stream */

void *cin_data_listen_thread(void);
void *cin_data_monitor_thread(void);
void *cin_data_assembler_thread(void);

/* Profiling Functions */
struct timespec timespec_diff(struct timespec start, struct timespec end);
void timespec_copy(struct timespec *dest, struct timespec *src);

#endif
