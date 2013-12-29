#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>

#include "cindata.h"

int main(int argc, char *argv[]){

  cin_thread *thread_data;

  /* Command line options */
  char *eth_interface = NULL;
  char *odir = NULL;

  /* For command line processing */
  int c;

  while((c = getopt(argc, argv, "i:o:h")) != -1){
    switch(c){
      case 'h':
        fprintf(stderr,"cindump : Dump data from CIN\n\n");
        exit(1);
      case 'i':
        eth_interface = optarg;
        printf("**** Using interface %s\n", eth_interface);
        break;
      case 'o':
        odir = optarg;
        printf("**** Writing to directory : %s\n", odir);
        break;
    }
  }

  thread_data        = malloc(sizeof(cin_thread));
  thread_data->iface = malloc(sizeof(cin_fabric_iface));

  /* Load network defaults */
  net_set_default(thread_data->iface);

  /* Set the network interface if supplied */
  if(eth_interface){
    strncpy(thread_data->iface->iface_name, eth_interface, 256);
  }

  /* Start the main routine */
  cin_start_threads(thread_data);

  pthread_exit(NULL);
}

