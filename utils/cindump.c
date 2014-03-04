#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#include <tiffio.h>
#include <signal.h>

#include "cin.h"

#define YF_LOCAL_EDITS 1

void int_handler(int dummy){
  cin_data_stop_threads();
  exit(0);
}

int main(int argc, char *argv[]){

  /* Command line options */
  int c;

  /* For writing out */
  TIFF *tfp;
  int j;
  uint16_t *p;
  FILE *fp = NULL;
  char filename[256];
  int tiff_output = 1;

  /*   */
  struct cin_port port;

  uint16_t buffer[CIN_DATA_FRAME_WIDTH * CIN_DATA_FRAME_HEIGHT];
  uint16_t frame_number;

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

  fprintf(stderr, "\n\n\n\n");

#ifdef YF_LOCAL_EDITS
  if(cin_init_data_port(&port, "192.168.11.112", 49201, "192.168.11.112", 49203, 100)){
#else
  if(cin_init_data_port(&port, NULL, 0, NULL, 0, 1000)){
#endif
    exit(1);
  }

  printf("port.srvaddr = %s\n", port.srvaddr);
  
  /* Start the main routine */
  #ifdef YF_LOCAL_EDITS
  if(cin_data_init(CIN_DATA_MODE_PUSH_PULL, 200, 200)){
  #else
  if(cin_data_init(CIN_DATA_MODE_PUSH_PULL, 2000, 2000)){
  #endif
    exit(1);
  }

  signal(SIGINT, int_handler); 

  cin_data_start_monitor_output();

  while(1){
    // Load the buffer
    cin_data_load_frame(buffer, &frame_number);

    if(tiff_output){
      sprintf(filename, "frame%08d.tif", frame_number);
      tfp = TIFFOpen(filename, "w");

      TIFFSetField(tfp, TIFFTAG_IMAGEWIDTH, CIN_DATA_FRAME_WIDTH);
      TIFFSetField(tfp, TIFFTAG_BITSPERSAMPLE, 16);
      TIFFSetField(tfp, TIFFTAG_SAMPLESPERPIXEL, 1);
      TIFFSetField(tfp, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
      TIFFSetField(tfp, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
      TIFFSetField(tfp, TIFFTAG_ORIENTATION, ORIENTATION_BOTLEFT);

      p = buffer;
      for(j=0;j<CIN_DATA_FRAME_HEIGHT;j++){
        TIFFWriteScanline(tfp, p, j, 0);
        p += CIN_DATA_FRAME_WIDTH;
      }

      TIFFClose(tfp);

    } else {
      if(!fp || !(frame_number % 100)){
        if(fp){
          fclose(fp);
        }
        sprintf(filename, "frame%08d.bin", frame_number);
        fp = fopen(filename, "w");
      }
      fwrite(buffer, sizeof(uint16_t),
             CIN_DATA_FRAME_HEIGHT * CIN_DATA_FRAME_WIDTH, fp);
    }

  }

  cin_data_wait_for_threads();

  fprintf(stderr, "\n\n\n\n");

  return(0);
}

