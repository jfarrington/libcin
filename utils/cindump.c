#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#include <tiffio.h>
#include <signal.h>

#include "cin.h"

static int keep_running = 1;

void int_handler(int dummy){
  keep_running = 0;
}

int main(int argc, char *argv[]){

  /* Command line options */
  int c;

  /* For writing out */
  TIFF *tfp;
  int j;
  uint16_t *p;
  FILE *fp;
  char filename[256];
  int tiff_output = 1;

  /*   */
  struct cin_port port;
  struct cin_data_frame *frame;

  while((c = getopt(argc, argv, "hr")) != -1){
    switch(c){
      case 'h':
        fprintf(stderr,"cindump : Dump data from CIN\n\n");
        fprintf(stderr,"\t -r : Write files in raw (uint16_t format)\n");
        fprintf(stderr,"\n");
        exit(1);
      case 'r':
        tiff_output =0;
        break;
    }
  }

  if(cin_init_data_port(&port, NULL, 0, NULL, 0, 0)){
    exit(1);
  }

  /* Start the main routine */
  if(cin_data_init(2000000, 20000)){
    exit(1);
  }

  signal(SIGINT, int_handler);

  while(1){
    frame = cin_data_get_next_frame();
    if(tiff_output){
      sprintf(filename, "frame%08d.tif", frame->number);
      tfp = TIFFOpen(filename, "w");

      TIFFSetField(tfp, TIFFTAG_IMAGEWIDTH, CIN_DATA_FRAME_WIDTH);
      TIFFSetField(tfp, TIFFTAG_BITSPERSAMPLE, 16);
      TIFFSetField(tfp, TIFFTAG_SAMPLESPERPIXEL, 1);
      TIFFSetField(tfp, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
      TIFFSetField(tfp, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
      TIFFSetField(tfp, TIFFTAG_ORIENTATION, ORIENTATION_BOTLEFT);

      p = frame->data;
      for(j=0;j<CIN_DATA_FRAME_HEIGHT;j++){
        TIFFWriteScanline(tfp, p, j, 0);
        p += CIN_DATA_FRAME_WIDTH;
      }

      TIFFClose(tfp);

    } else {
      sprintf(filename, "frame%08d.bin", frame->number);
      fp = fopen(filename, "w");
      if(fp){
        /* Compress stream */
        fwrite(frame->data, sizeof(uint16_t),
               CIN_DATA_FRAME_HEIGHT * CIN_DATA_FRAME_WIDTH, fp);
        fclose(fp);
      }

    }

    cin_data_release_frame(1);

    if(!keep_running){
      cin_data_stop_threads();
      break;
    }
  }

  cin_data_wait_for_threads();

  return(0);
}

