#ifndef _CIN_LISTEN_H 
#define _CIN_LISTEN_H 1

/* Definitions */

#define TRUE  1
#define FALSE 0


#define CIN_MAX_MTU         9000
#define CIN_SVRPORT         49201
#define CIN_SVRADDR         "10.0.5.22"
#define CIN_IFACE_NAME      "eth0"

/* Datastructures */

typedef struct {
  unsigned char *data;
  int size;
} cin_fifo_element;

typedef struct {
  int fd;
  char iface_name[256];
  char svraddr[16];
  int svrport;
} cin_fabric_iface;

/* Templates for functions */

void net_set_default(cin_fabric_iface *iface);
int net_set_promisc(cin_fabric_iface *iface, int val);
int net_set_packet_filter(cin_fabric_iface *iface);
int net_open_socket_udp(cin_fabric_iface *iface);
int net_open_socket_bfp(cin_fabric_iface *iface);
int net_bind_to_interface(cin_fabric_iface *iface);
int net_bind_to_address(cin_fabric_iface *iface);

#endif
