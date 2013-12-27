#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <unistd.h>
#include <stdint.h>

#include "fifo.h"
#include "listen.h"
#include "cindump.h"
#include "descramble.h"

int main(int argc, char *argv[]){
  int fd;

  long int i;

  /* Threads */
  pthread_t listener, writer, monitor, assembler;
  cin_thread thread_data;

  FILE *fp;

  /* Command line options */
  char *eth_interface = NULL;
  char *ofile = NULL;
  long int length = 1*1024*1024;

  cin_fifo_element *p;

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
      perror("unable to bind to interface");
      exit(1);
    }
  }

  if(fifo_init(&thread_data.packet_fifo, sizeof(cin_fifo_element), 1024 * 1024) == FALSE){
    printf("Unable to initialize FIFO\n");
    exit(1);
  }

  p = (cin_fifo_element*)(thread_data.packet_fifo.data);
  for(i=0;i<thread_data.packet_fifo.size;i++){
    p->data = malloc(sizeof(unsigned char) * CIN_MAX_MTU);
    if(p->data == NULL){
      printf("Unable to initialize FIFO\n");
      exit(1);
    }
    p->size = 0;
    p++;
  }

  thread_data.socket_fd = fd;

  /* Setup Mutexes */

  pthread_mutex_init(&thread_data.mutex, NULL);
  pthread_cond_init(&thread_data.signal, NULL);

  /* Start threads */

  pthread_create(&listener, NULL, (void *)cin_listen_thread, (void *)&thread_data);
  /* pthread_create(&assembler, NULL, (void *)cin_assembler_thread, (void *)&thread_data);*/
  pthread_create(&writer, NULL, (void *)cin_write_thread, (void *)&thread_data);
  pthread_create(&monitor, NULL, (void *)cin_monitor_thread, (void *)&thread_data);
  pthread_join(listener, NULL);
  pthread_join(writer, NULL);
  pthread_join(monitor, NULL);
  /* pthread_join(assembler, NULL); */

  fprintf(stderr, "Bye!\n\n");
  pthread_exit(NULL); 
}

void *cin_write_thread(cin_thread *data){
  cin_fifo_element *buffer = NULL;
  int this_frame = 0;
  int last_frame = -1;
  int this_packet = 0;
  int last_packet = -1;
  int skipped_packets = -1;
  int next_packet;

  uint16_t *frame;
  uint16_t *ds_frame;
  uint16_t *framep;

  /* used for byteswap */
  uint16_t *_framep;
  uint16_t *_bufferp;

  int i;

  FILE *fp;
  char filename[1024];

  long int image_size;
  long int image_height = 1000, image_width = 1152;

  image_size = image_height * image_width;

  /* Allocate memory for an image and a descrambled image */

  frame = malloc(sizeof(uint16_t) * image_size);
  if(frame == NULL){
    return NULL;
  }
  ds_frame = malloc(sizeof(uint16_t) * image_size);
  if(ds_frame == NULL){
    return NULL;
  }

  /* Use framep as the pointer to where we are in a frame */

  framep = frame;

  while(1){

    /* Lock the mutex and get a packet from the fifo */

    pthread_mutex_lock(&data->mutex);
    buffer = (cin_fifo_element*)fifo_get_tail(&data->packet_fifo);
    pthread_mutex_unlock(&data->mutex);

    if(buffer != NULL){
      /* Start assebleing frame */

      /* First byte of frame header is the packet no*/
      /* Bytes 7 and 8 are the frame number */ 
      this_packet = buffer->data[42];
      this_frame = (buffer->data[48] << 8) + buffer->data[49];

      if(this_frame != last_frame){
        /* We have a new frame, reset the buffer */
        framep = frame;
        last_frame = this_frame;
        last_packet = -1;
      }

      /* Predict the next packet number */
      next_packet = (last_packet + 1) & 0xFF;

      if(this_packet != next_packet){
        /* we have skipped packets! */
        skipped_packets = this_packet - next_packet;

        fprintf(stderr, "\nDropped %d packets at %d from frame %d\n\n", 
                skipped_packets, this_packet, this_frame);

        /* Set this block to zero and advance the frame pointer */
        memset(framep, 0, 8184 * skipped_packets / 2);
        framep += (8184 * skipped_packets / 2);
      }

      /* Now copy packet to buffer
         and swap the endieness */
      
      _framep = framep;
      _bufferp = (uint16_t*)(buffer->data + 50);

      for(i=0;i<((buffer->size-50)/2);i++){
        *_framep = (*_bufferp << 8) | (*_bufferp >> 8);
        _framep++;
        _bufferp++;
      }

      /* Now we are done with the packet, we can advance the fifo */

      pthread_mutex_lock(&data->mutex);
      fifo_advance_tail(&data->packet_fifo);
      pthread_mutex_unlock(&data->mutex);

      /* Advance the frame pointer by the packet length */
      framep += ((buffer->size - 50) / 2);
      last_packet = this_packet;

      /* Check for full image */
      /* NOTE : There has to be a better way to do this! */

      if((int)(framep - frame) == (2220744 / 2)){
        /* Descramble Image */

        if(descramble_image(ds_frame, frame, image_size, image_height, image_width)){
          /* Write out image */
          /*
          sprintf(filename, "frame%010d.bin", this_frame);
          fp = fopen(filename, "w");
          if(fp != NULL){
            fwrite(ds_frame, sizeof(uint16_t), image_size, fp);
            fclose(fp);
          }
          */
        }
      }
    } else {
      /* Buffer is empty - wait for next packet */
      pthread_mutex_lock(&data->mutex);
      pthread_cond_wait(&data->signal, &data->mutex);
      pthread_mutex_unlock(&data->mutex);
    }
  }

  pthread_exit(NULL);
}

void *cin_monitor_thread(cin_thread *data){
  while(1){
    fprintf(stderr, "Packet buffer has %12ld elements (head = %p, tail = %p)\r",
            fifo_used_bytes(&data->packet_fifo),
            data->packet_fifo.head,
            data->packet_fifo.tail); 
    sleep(0.25);
  }

  pthread_exit(NULL);
}

void *cin_listen_thread(cin_thread *data){
  cin_fifo_element *buffer = NULL;

  while(1){
    /* Get the next element in the fifo */
    pthread_mutex_lock(&data->mutex);
    buffer = (cin_fifo_element*)fifo_get_head(&data->packet_fifo);
    pthread_mutex_unlock(&data->mutex);

    buffer->size = recvfrom(data->socket_fd, buffer->data, 
                    sizeof(unsigned char) * CIN_MAX_MTU, 
                    0, NULL, NULL);

    if (buffer->size > 1024){
      pthread_mutex_lock(&data->mutex);
      fifo_advance_head(&data->packet_fifo);
      pthread_cond_signal(&data->signal);
      pthread_mutex_unlock(&data->mutex);
    }
    
  }

  pthread_exit(NULL);
}
