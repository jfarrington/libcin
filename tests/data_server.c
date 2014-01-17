#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#include "cin.h"
#include "data_server.h"

#define MAX_PACKETS 4000

#include "descramble_map.h"

int main(int argc, char *argv[]){
  udp_packet *packet;
  int num_packets, c;
  char *host = NULL;
  int port = 49201;
  long delay = 3e6;

  while((c = getopt (argc, argv, "a:p:d:h")) != -1){
    switch(c){
      case 'a':
        host = optarg;
        break;
      case 'p':
        port = atoi(optarg);
        break;
      case 'd':
        delay = atol(optarg);
        break;
      case 'h':
      default:
        fprintf(stderr, "data_server:\n\n");
        fprintf(stderr, "\t-h : help (this page)\n");
        fprintf(stderr, "\t-a : host to send UDP stream to.\n");
        fprintf(stderr, "\t-p : port to send UDP stream to.\n");
        fprintf(stderr, "\t-d : delay in nanoseconds between frames\n");
        fprintf(stderr, "\n");  
        exit(1);
        break;
    }
  }

  uint16_t *frame;
  unsigned char *scrambled_frame;
  frame = malloc(sizeof(uint16_t) * CIN_DATA_FRAME_HEIGHT * CIN_DATA_FRAME_WIDTH);
  scrambled_frame = malloc(sizeof(unsigned char) * 2 * CIN_DATA_FRAME_HEIGHT * CIN_DATA_FRAME_WIDTH);
  if(!frame || !scrambled_frame){
    perror("Cannot allocate frame data.");
    exit(1);
  }

  make_test_pattern(frame, CIN_DATA_FRAME_HEIGHT, CIN_DATA_FRAME_WIDTH);
  scramble_image(scrambled_frame, frame, 
                 CIN_DATA_FRAME_HEIGHT * CIN_DATA_FRAME_WIDTH);

  fprintf(stderr, "image size = %d\n", (int)(CIN_DATA_FRAME_HEIGHT * CIN_DATA_FRAME_WIDTH));

  num_packets = setup_packets(&packet, CIN_PACKET_LEN, 
                              scrambled_frame, 
                              (CIN_DATA_FRAME_HEIGHT * CIN_DATA_FRAME_WIDTH * 2) - 312);

  fprintf(stderr, "num packets = %d\n", num_packets);

  start_server(packet, num_packets, host, port, delay);

  return(0);
}

int make_test_pattern(uint16_t *data, int height, int width){
  int i,j;
  for(i=0;i<width;i++){
    for(j=0;j<height;j++){
      data[(j * width) + i] = (uint16_t)((1000*j)+(1000*i));
    }
  }
  return(0);
}

int scramble_image(unsigned char* stream, uint16_t *image, int size){
  uint32_t *scramble;
  scramble = (uint32_t*)descramble_map_forward_bin;

  uint16_t *stream_p = (uint16_t*)stream;
  int i;
  for(i=0;i<size;i++){
    *stream_p = (uint16_t)((image[*scramble] << 8) | (image[*scramble] >> 8));
    stream_p++;
    scramble++;
  }
  return 0;
}

int setup_packets(udp_packet **packets, int packet_size, 
                  unsigned char* stream, unsigned int stream_len){
  unsigned char* stream_p;
  int packet_len;
  int bytes_left;
  int num_packets = 0;
  udp_packet *packet_p;

  *packets = (udp_packet *)malloc(MAX_PACKETS * sizeof(udp_packet));

  bytes_left = stream_len;
  stream_p = stream;
  packet_p = *packets;
  while(bytes_left > 0){
    packet_p->data = malloc(CIN_DATA_MAX_MTU * sizeof(unsigned char));
    if(!packet_p->data){
      return 0;
    }

    if((bytes_left - packet_size) >= 0){
      packet_len = packet_size;
    } else {
      packet_len = (unsigned int)bytes_left;
    }

    packet_p->len = packet_len + 8;

    *((uint64_t*)(packet_p->data)) = 0x0000F4F3F2F1F000;
    *(packet_p->data) = (unsigned char)num_packets;
    memcpy((packet_p->data + 8), stream_p, packet_len );
    stream_p += packet_len;
    bytes_left -= packet_len;
    num_packets++;
    packet_p++;
  }
  return num_packets;
}

int start_server(udp_packet *packets, int num_packets, char* host, int port, long delay){
  int s; /* socket */
  struct sockaddr_in dest_addr;
  udp_packet* packet_p;
  uint16_t frame_num;
  int i = 1;
  struct timespec delay_time = {.tv_sec = 0, .tv_nsec = 0};
  char buffer[256];

  delay_time.tv_nsec = delay;

  dest_addr.sin_family = AF_INET;
  dest_addr.sin_port   = htons(port);
  if(host == NULL){
    inet_pton(AF_INET, "10.23.5.1", &dest_addr.sin_addr);
  } else {
    inet_pton(AF_INET, host, &dest_addr.sin_addr);
  }

  s = socket(AF_INET, SOCK_DGRAM, 0);
  if(s < 0){
    perror("Unable to open socket :");
    return 1;
  }
  
  if(setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i)) < 0){
    perror("setsockopt() failed :");
    return 1;
  }

  long int sndbuf = 1024 * 1024 * 20;
  if(setsockopt(s, SOL_SOCKET, SO_SNDBUF,
                &sndbuf, sizeof(sndbuf)) == -1){
    perror("CIN data port - unable to set receive buffer :");
  }

  socklen_t sndbuf_len = sizeof(sndbuf);
  if(getsockopt(s, SOL_SOCKET, SO_SNDBUF,
                &sndbuf, &sndbuf_len) == -1){
    perror("CIN data port - unable to get receive buffer :");
  }

  fprintf(stderr, "Remote host = %s\n", inet_ntop(AF_INET,&dest_addr.sin_addr, buffer, 256));
  fprintf(stderr, "Remote port = %d\n", port);
  fprintf(stderr, "Delay = %ld ns\n", delay_time.tv_nsec); 
  fprintf(stderr, "Send Buffer = %ld Mb\n", sndbuf / (1024*1024));

  frame_num = 0;

  while(1){
    packet_p = packets;
    for(i=0;i<num_packets;i++){
      packet_p->data[6] = (char)(frame_num >> 8);
      packet_p->data[7] = (char)(frame_num & 0xFF);
      sendto(s, packet_p->data, packet_p->len, 0, 
             (struct sockaddr*) &dest_addr, sizeof(dest_addr));
      //printf("%d,%d,%d \n", i, packet_p->len, packet_p->data[0]);
      packet_p++;
      //nanosleep(&delay_time, NULL);
    }
    frame_num++;
    nanosleep(&delay_time,NULL);
  }

  close(s);
}
