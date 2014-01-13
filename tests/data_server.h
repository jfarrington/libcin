#ifndef _DATA_SERVER_H 
#define _DATA_SERVER_H 1

#include <stdint.h>

#define CIN_PACKET_LEN          8184
#define CIN_UDP_DATA_HEADER     8

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  unsigned char *data;
  int len;
} udp_packet;

int make_test_pattern(uint16_t *data, int height, int width);
int scramble_image(unsigned char* stream, uint16_t *image, int size);
int setup_packets(udp_packet **packet, int packet_size,unsigned char* stream, unsigned int stream_len);
int start_server(udp_packet *packets, int num_packets, char* host, int port, long delay);

#ifdef __cplusplus
}
#endif


#endif
