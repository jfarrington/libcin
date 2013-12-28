#ifndef _CIN_LISTEN_H 
#define _CIN_LISTEN_H 1

/* Definitions */

#ifndef TRUE
#define TRUE  1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#define CIN_MAX_MTU             9000
#define CIN_SVRPORT             49201
#define CIN_SVRADDR             "10.0.5.22"
#define CIN_IFACE_NAME          "eth0"
#define CIN_SOCKET_MODE_UDP     1
#define CIN_SOCKET_MODE_BFP     2
#define CIN_UDP_PACKET_HEADER   48
#define CIN_UDP_DATA_HEADER     8
#define CIN_MAGIC_PACKET        0xF3F2F1F0
#define CIN_PACKET_LEN          8184
#define CIN_FRAME_HEIGHT        964
#define CIN_FRAME_WIDTH         1152
#define CIN_FRAME_SIZE          2220744

/* Datastructures */

typedef struct {
  void *data;
  void *head;
  void *tail;
  void *end;
  long int size;
  int elem_size;
} fifo;

typedef struct {
  int fd;
  char iface_name[256];
  char svraddr[16];
  int svrport;
  int mode;
  int header_len;
  int recv_buffer;
} cin_fabric_iface;

typedef struct {
  /* FIFO Elements */
  fifo packet_fifo;  
  fifo frame_fifo;

  /* Interface */
  cin_fabric_iface iface;

  /* Statistics */
  double framerate;

  /* Thread communications */
  pthread_mutex_t packet_mutex;
  pthread_cond_t packet_signal; 
  pthread_mutex_t frame_mutex;
  pthread_cond_t frame_signal;
} cin_thread;

typedef struct {
  unsigned char *data;
  int size;
  struct timeval timestamp;
} cin_packet_fifo;

typedef struct {
  uint16_t *data;
  uint16_t number;
  struct timeval timestamp;
} cin_frame_fifo;

/* Templates for functions */

/* UDP Network Functions */

void net_set_default(cin_fabric_iface *iface);
int net_set_promisc(cin_fabric_iface *iface, int val);
int net_set_packet_filter(cin_fabric_iface *iface);
int net_open_socket_udp(cin_fabric_iface *iface);
int net_open_socket_bfp(cin_fabric_iface *iface);
int net_bind_to_interface(cin_fabric_iface *iface);
int net_bind_to_address(cin_fabric_iface *iface);
int net_read(cin_fabric_iface *iface, unsigned char* buffer);
int net_connect(cin_fabric_iface *iface);
int net_set_buffers(cin_fabric_iface *iface);

/* FIFO Functions */

void* fifo_get_head(fifo *f);
void* fifo_get_tail(fifo *f);
void fifo_advance_head(fifo *f);
void fifo_advance_tail(fifo *f);
int fifo_init(fifo *f, int elem_size, long int size);
long int fifo_used_bytes(fifo *f);
double fifo_percent_full(fifo *f);

/* Threads for processing stream */

void *cin_listen_thread(cin_thread *data);
void *cin_write_thread(cin_thread *data);
void *cin_monitor_thread(cin_thread *data);
void *cin_assembler_thread(cin_thread *data);

/* Thread Functions */

int cin_initialize_fifo(cin_thread *data, long int packet_size, long int frame_size);
int cin_start_threads(cin_thread *data);
#endif
