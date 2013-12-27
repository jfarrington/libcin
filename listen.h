#ifndef _CIN_LISTEN_H 
#define _CIN_LISTEN_H 1

/* Definitions */

#define TRUE  1
#define FALSE 0
#define CIN_MAX_MTU 9000

/* Datastructures */

typedef struct {
  unsigned char *data;
  int size;
} cin_fifo_element;

/* Templates for functions */

int net_set_promisc(int fd, const char* iface, int val);
int net_set_packet_filter(int fd);
int net_open_socket(int *fd);
int net_bind_to_interface(int fd, const char* iface);

#endif
