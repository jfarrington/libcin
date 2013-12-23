#ifndef _CIN_LISTEN_H 
#define _CIN_LISTEN_H 1

/* Definitions */

#define TRUE  1
#define FALSE 0
#define CIN_MAX_MTU 9000

/* Templates for functions */

int net_set_promisc(int fd, const char* iface, int val);
int net_set_packet_filter(int fd);
int net_open_socket(int *fd);
int net_bind_to_interface(int fd, const char* iface);

/* Datastructures */

struct cin_packet_entry {
  unsigned char *data[CIN_MAX_MTU];
  int size;
};

struct cin_packet_ringbuffer {
  struct cin_packet_array *data;
  size_t capacity;
  size_t count;
  struct cin_packet_array *head;
  struct cin_packet_array *tail;
};

#endif
