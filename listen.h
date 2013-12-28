#ifndef _CIN_LISTEN_H 
#define _CIN_LISTEN_H 1

/* Definitions */

#define TRUE  1
#define FALSE 0


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
  unsigned char *data;
  int size;
  struct timeval timestamp;
} cin_packet_fifo;

typedef struct {
  uint16_t *data;
  uint16_t number;
  struct timeval timestamp;
} cin_frame_fifo;

typedef struct {
  int fd;
  char iface_name[256];
  char svraddr[16];
  int svrport;
  int mode;
  int header_len;
  int recv_buffer;
} cin_fabric_iface;

/* Templates for functions */

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

#endif
