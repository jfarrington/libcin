#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/time.h>

#include "fifo.h"
#include "listen.h"
#include "cindump.h"
#include "descramble_block.h"

int main(int argc, char *argv[]){
  long int i;

  /* Threads */
  pthread_t listener, writer, monitor, assembler;
  cin_thread thread_data;

  /* Command line options */
  char *eth_interface = NULL;
  char *ofile = NULL;

  /* FIFO elements */
  cin_packet_fifo *p;
  cin_frame_fifo *q;

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

  if(fifo_init(&thread_data.packet_fifo, sizeof(cin_packet_fifo), 200) == FALSE){
    printf("Unable to initialize FIFO\n");
    exit(1);
  }

  if(fifo_init(&thread_data.frame_fifo, sizeof(cin_frame_fifo), 200) == FALSE){
    printf("Unable to initialize FIFO\n");
    exit(1);
  }

  p = (cin_packet_fifo*)(thread_data.packet_fifo.data);
  for(i=0;i<thread_data.packet_fifo.size;i++){
    p->data = malloc(sizeof(unsigned char) * CIN_MAX_MTU);
    if(p->data == NULL){
      printf("Unable to initialize FIFO\n");
      exit(1);
    }
    p->size = 0;
    p++;
  }

  q = (cin_frame_fifo*)(thread_data.frame_fifo.data);
  for(i=0;i<thread_data.frame_fifo.size;i++){
    q->data = malloc(sizeof(uint16_t) * CIN_FRAME_WIDTH * CIN_FRAME_HEIGHT);
    if(!q->data){
      printf("Unable to initialize FIFO\n");
      exit(1);
    }
    q->number = 0;
    q++;
  }

  /* Setup Mutexes */

  pthread_mutex_init(&thread_data.packet_mutex, NULL);
  pthread_mutex_init(&thread_data.frame_mutex, NULL);
  pthread_cond_init(&thread_data.packet_signal, NULL);
  pthread_cond_init(&thread_data.frame_signal, NULL);

  /* Start threads */

  pthread_create(&listener, NULL, (void *)cin_listen_thread, (void *)&thread_data);
  pthread_create(&assembler, NULL, (void *)cin_assembler_thread, (void *)&thread_data);
  pthread_create(&writer, NULL, (void *)cin_write_thread, (void *)&thread_data);
  pthread_create(&monitor, NULL, (void *)cin_monitor_thread, (void *)&thread_data);
  pthread_join(listener, NULL);
  pthread_join(writer, NULL);
  pthread_join(monitor, NULL);
  pthread_join(assembler, NULL);

  fprintf(stderr, "Bye!\n\n");
  pthread_exit(NULL); 
}

void *cin_assembler_thread(cin_thread *data){
  
  cin_packet_fifo *buffer = NULL;
  cin_frame_fifo *frame = NULL;
  int this_frame = 0;
  int last_frame = -1;
  int this_packet = 0;
  int last_packet = -1;
  int skipped_packets = -1;
  int next_packet;

  int buffer_len;
  unsigned char *buffer_p;

  /* Descramble Map */

  uint32_t *ds_map; 
  uint32_t *ds_map_p;

  long int i;
  long int byte_count = 0;

  struct timeval last_frame_timestamp;
  struct timeval this_frame_timestamp;

  /* Set descramble map */

  ds_map = (uint32_t*)descramble_map;
  ds_map_p = ds_map;

  /* Allocate memory for an image and a descrambled image */

  pthread_mutex_lock(&data->frame_mutex);
  frame = (cin_frame_fifo*)fifo_get_head(&data->frame_fifo);
  pthread_mutex_unlock(&data->frame_mutex);

  while(1){

    /* Lock the mutex and get a packet from the fifo */

    pthread_mutex_lock(&data->packet_mutex);
    buffer = (cin_packet_fifo*)fifo_get_tail(&data->packet_fifo);
    pthread_mutex_unlock(&data->packet_mutex);

    if(buffer != NULL){
      /* Start assebleing frame */

      buffer_p = buffer->data;
      buffer_len = buffer->size - data->iface.header_len - CIN_UDP_DATA_HEADER;

      /* Skip header (defined in interface */

      buffer_p += data->iface.header_len;

      /* First byte of frame header is the packet no*/
      /* Bytes 7 and 8 are the frame number */ 
      this_packet = *buffer_p;
      buffer_p++;
     
      /* Next lets check the magic number of the packet */
      if(*((uint32_t *)buffer_p) == CIN_MAGIC_PACKET) {; 
        buffer_p+=5; 
        
        /* Get the frame number from header */

        this_frame = (*buffer_p << 8) + *(buffer_p + 1);
        buffer_p += 2;

        if(this_frame != last_frame){
          /* We have a new frame */

          /* Lock the mutex and get the next frame buffer */
          pthread_mutex_lock(&data->frame_mutex);
          frame = (cin_frame_fifo*)fifo_get_head(&data->frame_fifo);
          pthread_mutex_unlock(&data->frame_mutex);
         
          /* Reset the descramble map pointer */
          ds_map_p = ds_map;
         
          /* Set all the last frame stuff */
          last_frame = this_frame;
          last_packet = -1;
          byte_count = 0;
          
          /* The following is used for calculating rates */
          last_frame_timestamp.tv_sec  = this_frame_timestamp.tv_sec;
          last_frame_timestamp.tv_usec = this_frame_timestamp.tv_usec;
          this_frame_timestamp.tv_sec  = buffer->timestamp.tv_sec;
          this_frame_timestamp.tv_usec = buffer->timestamp.tv_usec; 

          data->framerate = this_frame_timestamp.tv_sec - last_frame_timestamp.tv_sec;
          data->framerate = ((double)(this_frame_timestamp.tv_usec - last_frame_timestamp.tv_usec) * 1e-6);
          if(data->framerate != 0){
            data->framerate = 1 / data->framerate;
          }
        }

        /* Predict the next packet number */
        next_packet = (last_packet + 1) & 0xFF;

        if(this_packet != next_packet){
          /* we have skipped packets! */
          skipped_packets = this_packet - next_packet;

          fprintf(stderr, "\nDropped %d packets at %d from frame %d\n\n", 
                  skipped_packets, this_packet, this_frame);

          /* Set this block to zero and advance the frame pointer */
          //memset(frame_p, 0, CIN_PACKET_LEN * skipped_packets / 2);
          for(i=0;i<(CIN_PACKET_LEN * skipped_packets / 2);i++){
            *(frame->data + *ds_map_p) = 0;
            ds_map_p++;
          }
          byte_count += CIN_PACKET_LEN * skipped_packets;
        } else {
          /* Swap endienness of packet and copy to frame */
          
          for(i=0;i<(buffer_len / 2);i++){
            *(frame->data + *ds_map_p) = (*buffer_p << 8) + *(buffer_p + 1);
            /* Advance descramble map by 1 and buffer by 2 */ 
            ds_map_p++;
            buffer_p += 2;
          }
          byte_count += buffer_len;
        }
      } else { 
        fprintf(stderr, "\nMallformed Packet Recieved!\n");
      } /* if CIN_MAGIC_PACKET */

      /* Now we are done with the packet, we can advance the fifo */

      pthread_mutex_lock(&data->packet_mutex);
      fifo_advance_tail(&data->packet_fifo);
      pthread_mutex_unlock(&data->packet_mutex);

      /* Now we can set the last packet to this packet */

      last_packet = this_packet;

      /* Check for full image */

      if(byte_count == CIN_FRAME_SIZE){
        /* Advance the frame fifo and signal that a new frame
           is avaliable */
        frame->number = this_frame;
        pthread_mutex_lock(&data->frame_mutex);
        fifo_advance_head(&data->frame_fifo);
        pthread_cond_signal(&data->frame_signal);
        pthread_mutex_unlock(&data->frame_mutex);
      }

    } else {
      /* Buffer is empty - wait for next packet */
      pthread_mutex_lock(&data->packet_mutex);
      pthread_cond_wait(&data->packet_signal, &data->packet_mutex);
      pthread_mutex_unlock(&data->packet_mutex);
    }
  }

  pthread_exit(NULL);
}

void *cin_write_thread(cin_thread *data){
  FILE *fp;
  char filename[256];
  cin_frame_fifo *frame;

  while(1){
    /* Lock mutex and wait for frame */
    /* once frame arrives, get the lail from the fifo */
    /* and save to disk */

    pthread_mutex_lock(&data->frame_mutex);
    pthread_cond_wait(&data->frame_signal, &data->frame_mutex);
    frame = (cin_frame_fifo*)fifo_get_tail(&data->frame_fifo);
    pthread_mutex_unlock(&data->frame_mutex);

    if(frame != NULL){
      sprintf(filename, "frame%06d.bin", frame->number);
    
      fp = fopen(filename, "w");
      if(fp){
        fwrite(frame->data, sizeof(uint16_t), 
               CIN_FRAME_HEIGHT * CIN_FRAME_WIDTH, fp);
        fclose(fp);
      } 

      /* Now that file is written, advance the fifo */
      pthread_mutex_lock(&data->frame_mutex);
      fifo_advance_tail(&data->frame_fifo);
      pthread_mutex_unlock(&data->frame_mutex);
    }
  }

  pthread_exit(NULL);
}

void *cin_monitor_thread(cin_thread *data){
  fprintf(stderr, "\n\n");

  while(1){
    fprintf(stderr, "Packet buffer is %9.3f %% full. Spool buffer is %9.3f %% full. Framerate = %6.1f s^-1\r",
            fifo_percent_full(&data->packet_fifo),
            fifo_percent_full(&data->frame_fifo),data->framerate);
    sleep(0.5);
  }

  pthread_exit(NULL);
}

void *cin_listen_thread(cin_thread *data){
  cin_packet_fifo *buffer = NULL;

  /* Open the socket for communications */

  if(!net_connect(&data->iface)){
    perror("Unable to connect to CIN!");
    pthread_exit(NULL);
  }

  while(1){
    /* Get the next element in the fifo */
    pthread_mutex_lock(&data->packet_mutex);
    buffer = (cin_packet_fifo*)fifo_get_head(&data->packet_fifo);
    pthread_mutex_unlock(&data->packet_mutex);

    buffer->size = net_read(&data->iface, buffer->data);
    gettimeofday(&buffer->timestamp, NULL);

    //fprintf(stderr, "Buffer size = %d\n", buffer->size);

    if (buffer->size > 1024){
      pthread_mutex_lock(&data->packet_mutex);
      fifo_advance_head(&data->packet_fifo);
      pthread_cond_signal(&data->packet_signal);
      pthread_mutex_unlock(&data->packet_mutex);
    }
    
  }

  pthread_exit(NULL);
}
