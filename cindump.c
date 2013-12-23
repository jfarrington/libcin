#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "listen.h"

int main(int argc, char *argv[]){
  int fd;

  int byte_count;
  unsigned char buf[CIN_MAX_MTU];
  unsigned char *data = NULL;
  unsigned char *databuf = NULL;

  FILE *fp;

  /* Command line options */
  char *eth_interface = NULL;
  char *ofile = NULL;
  long int length = 1*1024*1024;
  long int recvd = 0;

  /* For command line processing */
  int c;

  while((c = getopt(argc, argv, "i:o:l:")) != -1){
    switch(c){
      case 'i':
        eth_interface = optarg;
        printf("Using interface %s\n", eth_interface);
        break;
      case 'o':
        ofile = optarg;
        printf("Writing to : %s\n", ofile);
        break;
      case 'l':
        length = atoi(optarg) * 1024 * 1024;
        printf("Reading %ld bytes\n", length);
        break;
    }
  }

  if(ofile == NULL){
    perror("You have to specify a filename");
    exit(1);
  }

  if(!net_open_socket(&fd)){
    perror("cannot create socket for cin communications");
    exit(1);
  }

  if(!net_set_packet_filter(fd)){
    perror("unable to set packet filtering");
    exit(1);
  }

  if(eth_interface){
    if(!net_set_promisc(fd, eth_interface, TRUE)){
      perror("unable to set interface to promisc mode");
      exit(1);
    }
    if(!net_bind_to_interface(fd, eth_interface)){
      perror("unable to bine to interface");
      exit(1);
    }
  }

  data = malloc(length * sizeof(unsigned char));
  if(!data){
    perror("cannot allocate memory");
    exit(1);
  }

  databuf = data;
  while((length - recvd) > CIN_MAX_MTU){
    byte_count = recvfrom(fd, buf, sizeof(buf), 0, NULL, NULL);
    if(byte_count != -1) {
      byte_count -= 42;
      memcpy(databuf, buf + 42, byte_count);
      databuf+=(byte_count);
      recvd+=(byte_count);
    }
  }

  close(fd);

  /* Now write out file */

  fp = fopen(ofile, "wb");
  if(fp){
    fwrite(data, sizeof(unsigned char), recvd, fp);
    fclose(fp);
  } else {
    perror("Unable to open output file");
  }

  return(0);
}
