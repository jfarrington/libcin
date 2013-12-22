#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>

#include "filters.h"

main(){
  int fd;
  struct sockaddr_ll myaddr;
  struct sockaddr_ll cli_addr;
  socklen_t slen=sizeof(cli_addr);
  int byte_count;
  char buf[9000];
  char *image;
  char ipstr[INET6_ADDRSTRLEN];

  if ((fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_IP))) < 0) {
      perror("cannot create socket");
      return 0;
  }

  image = malloc(100*1024*2048*2 * sizeof(char));
  if(!image){
    perror("cannot allocate memory");
    return 0;
  }

  /* Define Filter */

  struct sock_fprog FILTER_CIN_SOURCE_IP_PORT;
  FILTER_CIN_SOURCE_IP_PORT.len = BPF_CIN_SOURCE_IP_PORT_LEN;
  FILTER_CIN_SOURCE_IP_PORT.filter = BPF_CIN_SOURCE_IP_PORT;

  /* Attach the filter to the socket */
  if(setsockopt(fd, SOL_SOCKET, SO_ATTACH_FILTER, 
                &FILTER_CIN_SOURCE_IP_PORT, sizeof(FILTER_CIN_SOURCE_IP_PORT))<0){
     perror("setsockopt");
     close(fd);
     exit(1);
  }

  char *imagebuf = image;
  long int total = 10 * 2048*2048*2;
  while(total > 0){
    byte_count = recvfrom(fd, buf, sizeof(buf), 0, NULL, NULL) - 42;
    if (byte_count) { // We have a proper ethernet header
      memcpy(imagebuf,buf + 42,byte_count);
      imagebuf+=byte_count;
      total-=byte_count;
    }
  }

  FILE *fp = fopen("image.bin", "wb");
  if(fp){
    fwrite(image, sizeof(char), 10 * 2048 * 2048 * 2,fp);
    fclose(fp);
  }
  close(fd);
}
