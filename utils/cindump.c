#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#include <tiffio.h>

#include "cindata.h"

int main(int argc, char *argv[]){

  cin_thread *thread_data;

  /* Command line options */
  char *eth_interface = NULL;
  char *odir = NULL;

  /* For command line processing */
  int c;

  /* For writing out */
  TIFF *tfp;
  int j;
  uint16_t *p;
  FILE *fp;
  char filename[256];
  cin_frame_fifo *frame;
  int tiff_output = 1;


  while((c = getopt(argc, argv, "i:hir")) != -1){
    switch(c){
      case 'h':
        fprintf(stderr,"cindump : Dump data from CIN\n\n");
        fprintf(stderr,"\t -i : Interface to bind to (eth5)\n");
        fprintf(stderr,"\t -r : Write files in raw (uint16_t format)\n");
        fprintf(stderr,"\n");
        exit(1);
      case 'i':
        eth_interface = optarg;
        break;
      case 'r':
        tiff_output =0;
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

  while(1){
    frame = cin_get_next_frame(thread_data);
    if(tiff_output){
      sprintf(filename, "frame%08d.tif", frame->number);
      tfp = TIFFOpen(filename, "w");

      TIFFSetField(tfp, TIFFTAG_IMAGEWIDTH, CIN_FRAME_WIDTH);
      TIFFSetField(tfp, TIFFTAG_BITSPERSAMPLE, 16);
      TIFFSetField(tfp, TIFFTAG_SAMPLESPERPIXEL, 1);
      TIFFSetField(tfp, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
      TIFFSetField(tfp, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
      TIFFSetField(tfp, TIFFTAG_ORIENTATION, ORIENTATION_BOTLEFT);

      p = frame->data;
      for(j=0;j<CIN_FRAME_HEIGHT;j++){
        TIFFWriteScanline(tfp, p, j, 0);
        p += CIN_FRAME_WIDTH;
      }

      TIFFClose(tfp);

    } else {
      sprintf(filename, "frame%08d.bin", frame->number);

      fp = fopen(filename, "w");
      if(fp){
        fwrite(frame->data, sizeof(uint16_t),
               CIN_FRAME_HEIGHT * CIN_FRAME_WIDTH, fp);
        fclose(fp);
      }

    }

    cin_release_frame(thread_data);
  }

  cin_wait_for_threads();

  return(0);
}

