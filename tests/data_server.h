#ifndef _DATA_SERVER_H 
#define _DATA_SERVER_H 1

#define CIN_PACKET_LEN          8184
#define CIN_FRAME_HEIGHT        964
#define CIN_FRAME_WIDTH         1152
#define CIN_UDP_DATA_HEADER     8
#define CIN_FRAME_SIZE          2220744
#define CIN_MAX_MTU             9000

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  unsigned char *data;
  int len;
} udp_packet;

int setup_packets(udp_packet **packet, int packet_size,unsigned char* stream, unsigned int stream_len);
int start_server(udp_packet *packets, int num_packets, char* host, int port, long delay);

#ifdef __cplusplus
}
#endif


#endif
