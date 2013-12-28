#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>

#include "listen.h"

int main(int argc, char *argv[]){

  cin_thread thread_data;

  /* Command line options */
  char *eth_interface = NULL;
  char *ofile = NULL;

  /* For command line processing */
  int c;

  while((c = getopt(argc, argv, "i:o:")) != -1){
    switch(c){
      case 'i':
        eth_interface = optarg;
        printf("**** Using interface %s\n", eth_interface);
        break;
      case 'o':
        ofile = optarg;
        printf("**** Writing to : %s\n", ofile);
        break;
    }
  }

  /*
  if(ofile == NULL){
    perror("You have to specify a filename");
    exit(1);
  }
  */

  net_set_default(&thread_data.iface);

  if(eth_interface){
    strncpy(thread_data.iface.iface_name, eth_interface, 256);
  }

  cin_start_threads(&thread_data);

  pthread_exit(NULL);
}

