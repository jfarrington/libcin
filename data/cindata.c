#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <linux/filter.h>
#include <pthread.h>
#include <sys/time.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>

#include "cin.h"
#include "fifo.h"
#include "cindata.h"
#include "descramble_block.h"

/* -----------------------------------------------------------------------------------------
 *
 * Thread communication global variables
 *
 * -----------------------------------------------------------------------------------------
 */

static pthread_t threads[MAX_THREADS];
static pthread_mutex_t *packet_mutex;
static pthread_cond_t *packet_signal;
static pthread_mutex_t *frame_mutex;
static pthread_cond_t *frame_signal;
static struct cin_data_thread_data thread_data;

/* -----------------------------------------------------------------------------------------
 *
 * Network functions to read from fabric UDP port
 *
 * -----------------------------------------------------------------------------------------
 */

int cin_init_data_port(struct cin_port* dp, 
                       char* ipaddr, uint16_t port,
                       char* cin_ipaddr, uint16_t cin_port,
                       unsigned int rcvbuf) {

  if(ipaddr == NULL){
    dp->cliaddr = "0.0.0.0";
  } else {
    dp->cliaddr = ipaddr;
  }

  if(port == 0){
    dp->cliport = CIN_DATA_PORT;
  } else {
    dp->cliport = port;
  }

  if(cin_ipaddr == NULL){
    dp->srvaddr = CIN_DATA_IP;
  } else {
    dp->srvaddr = cin_ipaddr;
  }

  if(cin_port == 0){
    dp->srvport = CIN_DATA_CTL_PORT;
  } else {
    dp->srvport = cin_port;
  }

  if(rcvbuf == 0){
    dp->rcvbuf = CIN_DATA_RCVBUF;
  } else {
    dp->rcvbuf = rcvbuf;
  }

  dp->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if(dp->sockfd < 0) {
    perror("CIN data port - socket() failed !!!");
    return 1;
  }

  int i = 1;
  if(setsockopt(dp->sockfd, SOL_SOCKET, SO_REUSEADDR, \
                (void *)&i, sizeof i) < 0) {
    perror("CIN data port - setsockopt() failed !!!");
    return 1;
  }

  /* initialize CIN (server) and client (us!) sockaddr structs */

  memset(&dp->sin_srv, 0, sizeof(struct sockaddr_in));
  memset(&dp->sin_cli, 0, sizeof(struct sockaddr_in));

  dp->sin_srv.sin_family = AF_INET;
  dp->sin_srv.sin_port = htons(dp->srvport);
  dp->sin_cli.sin_family = AF_INET;
  dp->sin_cli.sin_port = htons(dp->cliport);
  dp->slen = sizeof(struct sockaddr_in);

  if(inet_aton(dp->srvaddr, &dp->sin_srv.sin_addr) == 0) {
    perror("CIN data port - inet_aton() failed!!");
    return 1;
  }

  if(inet_aton(dp->cliaddr, &dp->sin_cli.sin_addr) == 0) {
    perror("CIN data port - inet_aton() failed!!");
    return 1;
  }

  /* Bind to the socket to get CIN traffic */

  if((bind(dp->sockfd, (struct sockaddr *)&dp->sin_cli , sizeof(dp->sin_cli))) == -1){
    perror("CIN data port - cannot bind");
    return 1;
  }

  /* Set the receieve buffers for the socket */

  if(setsockopt(dp->sockfd, SOL_SOCKET, SO_RCVBUF, 
                &dp->rcvbuf, sizeof(dp->rcvbuf)) == -1){
    perror("CIN data port - unable to set receive buffer :");
  } 

  thread_data.dp = dp;
  return 0;
}

int cin_data_read(struct cin_port* dp, unsigned char* buffer){
  /* Read from the UDP stream */  
  return recvfrom(dp->sockfd, buffer, 
                  CIN_DATA_MAX_MTU * sizeof(unsigned char), 0,
                  (struct sockaddr*)&dp->sin_cli, 
                  (socklen_t*)&dp->slen);
}

int cin_data_write(struct cin_port* dp, unsigned char* buffer, int buffer_len){
  return sendto(dp->sockfd, buffer, buffer_len, 0,
                (struct sockaddr*)&dp->sin_srv, sizeof(dp->sin_srv));
}

/* -----------------------------------------------------------------------------------------
 *
 * FIFO Functions
 *
 * -----------------------------------------------------------------------------------------
 */

int fifo_init(fifo *f, int elem_size, long int size){
  /* Initialize the fifo */
  /* NOTE : if this fails then it causes memory leaks !*/

  f->data = malloc(size * (long int)elem_size);
  if(f->data == NULL){
    return FALSE;
  } 

  /* set the initial parameters */
  f->head = f->data;
  f->tail = f->data;
  f->end  = f->data + ((size - 1) * (long int)elem_size);
  f->size = size;
  f->elem_size = elem_size;

  /* store the pointers of the start in both
     head and tail */

  f->head = f->data;
  f->tail = f->data;

  f->full = FALSE;

  return TRUE;
}

long int fifo_used_bytes(fifo *f){
  long int bytes;

  if(f->head >= f->tail){
    bytes = (long int)(f->head - f->tail);  
  } else {
    bytes = (long int)((f->end - f->tail) + (f->head - f->data));
  }

  return bytes;
}

double fifo_percent_full(fifo *f){
  long int bytes;
  double percent;
  
  bytes = fifo_used_bytes(f);
  percent = (double)bytes / (double)(f->elem_size * f->size);

  return percent * 100.0;
}

void *fifo_get_head(fifo *f){
  return f->head;
}

void fifo_advance_head(fifo *f){
  /* Increment the head pointet */

  /* Check if we have hit the end before emptying any data */
  if((f->head == f->end) && (f->tail == f->data)){
    f->full = TRUE;
    return;
  }

  if((f->head + f->elem_size) == f->tail){
    /* FIFO is full. Don't increment */
    f->full = TRUE;
    return;
  }

  if(f->head == f->end){
    f->head = f->data;
  } else {
    f->head += f->elem_size;
  }

  f->full = FALSE;
}

void* fifo_get_tail(fifo *f){
  /* Return the tail pointer or NULL if the FIFO is empty */

  if(f->tail == f->head){
    return NULL;
  }

  return f->tail;
}

void fifo_advance_tail(fifo *f){
  /* Return the tail pointer and advance the FIFO */

  /* If the head and tail are the same, FIFO is empty */
  if(f->tail == f->head){
    return;
  }

  if(f->tail == f->end){
    f->tail = f->data;
  } else {
    f->tail += f->elem_size;
  }
}

/* -----------------------------------------------------------------------------------------
 *
 * Main thread functions
 *
 * -----------------------------------------------------------------------------------------
 */

int cin_data_init(void){
  /* Initialize and start all the threads to acquire data */
  /* This does not block, just start threads */
  /* Setup FIFO elements */

  thread_data.packet_fifo = malloc(sizeof(fifo));
  if(!thread_data.packet_fifo){
    return 1;
  }
  thread_data.frame_fifo  = malloc(sizeof(fifo));
  if(!thread_data.frame_fifo){
    return 1;
  }

  /* Packet FIFO */

  if(fifo_init(thread_data.packet_fifo, sizeof(struct cin_data_packet), 20) == FALSE){
    return 1;
  }

  struct cin_data_packet *p;
  p = (struct cin_data_packet*)(thread_data.packet_fifo->data);
  long int i;
  for(i=0;i<thread_data.packet_fifo->size;i++){
    p->data = malloc(sizeof(unsigned char) * CIN_DATA_MAX_MTU);
    if(p->data == NULL){
      return 1;
    }
    p->size = 0;
    p++;
  }

  /* Frame FIFO */

  if(fifo_init(thread_data.frame_fifo, sizeof(struct cin_data_frame), 200) == FALSE){
    return 1;
  }

  struct cin_data_frame *q;
  q = (struct cin_data_frame*)(thread_data.frame_fifo->data);
  for(i=0;i<thread_data.frame_fifo->size;i++){
    q->data = malloc(sizeof(uint16_t) * CIN_DATA_FRAME_WIDTH * CIN_DATA_FRAME_HEIGHT);
    if(!q->data){
      return 1;
    }
    q->number = 0;
    q->timestamp.tv_sec = 0;
    q->timestamp.tv_usec = 0;
    q++;
  }

  /* Set some defaults */

  thread_data.mallformed_packets = 0;
  thread_data.dropped_packets = 0;
  thread_data.framerate = 0;

  /* Setup Mutexes */

  packet_mutex  = malloc(sizeof(pthread_mutex_t));
  frame_mutex   = malloc(sizeof(pthread_mutex_t));
  packet_signal = malloc(sizeof(pthread_cond_t));
  frame_signal  = malloc(sizeof(pthread_cond_t));

  pthread_mutex_init(packet_mutex, NULL);
  pthread_mutex_init(frame_mutex, NULL);
  pthread_cond_init(packet_signal, NULL);
  pthread_cond_init(frame_signal, NULL);

  /* Start threads */

  pthread_create(&threads[0], NULL, (void *)cin_data_listen_thread, NULL);
  pthread_create(&threads[1], NULL, (void *)cin_data_assembler_thread, NULL);
  pthread_create(&threads[2], NULL, (void *)cin_data_monitor_thread, NULL);

  return 0;
}

void cin_data_wait_for_threads(void){
  /* This routine waits for the threads to stop 
     NOTE : This blocks until all threads complete */
  pthread_join(threads[0], NULL);
  pthread_join(threads[1], NULL);
  pthread_join(threads[2], NULL);
}

int cin_data_stop_threads(void){
  /* To be implemented */
  return 0;
}

/* -----------------------------------------------------------------------------------------
 *
 * MAIN Thread Routines
 *
 * -----------------------------------------------------------------------------------------
 */

void *cin_data_assembler_thread(void){
  
  struct cin_data_packet *buffer = NULL;
  struct cin_data_frame *frame = NULL;
  unsigned int this_frame = 0;
  unsigned int last_frame = -1;
  unsigned int this_packet = 0;
  unsigned int last_packet = -1;
  unsigned int skipped_packets = -1;
  unsigned int next_packet;

#ifdef __DEBUG__
  uint64_t last_packet_header = 0;
  uint64_t this_packet_header = 0;
#endif

  int buffer_len;
  unsigned char *buffer_p;

  uint64_t header;

#ifdef __DESCRAMBLE__
  /* Descramble Map */
  uint32_t *ds_map; 
  uint32_t *ds_map_p;
  long int i;
#else
  /* Pointer to frame */
  uint16_t *frame_p;
#endif

  long int byte_count = 0;

  struct timeval last_frame_timestamp = {0,0};
  struct timeval this_frame_timestamp = {0,0};

#ifdef __DESCRAMBLE__
  /* Set descramble map */
  ds_map = (uint32_t*)descramble_map;
  ds_map_p = ds_map;
#endif

  /* Get first frame pointer from the buffer */

  pthread_mutex_lock(frame_mutex);
  frame = (struct cin_data_frame*)fifo_get_head(thread_data.frame_fifo);
  pthread_mutex_unlock(frame_mutex);

#ifndef __DESCRAMBLE__
  /* set frame_p to the start of the frame */
  frame_p = frame->data;
#endif

  while(1){

    /* Lock the mutex and get a packet from the fifo */

    pthread_mutex_lock(packet_mutex);
    buffer = (struct cin_data_packet*)fifo_get_tail(thread_data.packet_fifo);
    pthread_mutex_unlock(packet_mutex);

    if(buffer != NULL){
      /* Start assebleing frame */

      buffer_p = buffer->data;
      buffer_len = buffer->size - CIN_DATA_UDP_HEADER;

      /* Next lets check the magic number of the packet */
      header = *((uint64_t *)buffer_p) & CIN_DATA_MAGIC_PACKET_MASK; 

      if(header == CIN_DATA_MAGIC_PACKET) {; 
        
        /* First byte of frame header is the packet no*/
        /* Bytes 7 and 8 are the frame number */ 
#ifdef __DEBUG__ 
        this_packet_header = *(uint64_t*)buffer_p;
#endif
        this_packet = *buffer_p; 
        buffer_p += 6;
        this_frame  = (*buffer_p << 8) + *(buffer_p + 1);
        buffer_p += 2;

        if(this_frame != last_frame){
          /* We have a new frame */

          /* Lock the mutex and get the next frame buffer */
          pthread_mutex_lock(frame_mutex);
          frame = (struct cin_data_frame*)fifo_get_head(thread_data.frame_fifo);
          pthread_mutex_unlock(frame_mutex);
         
#ifdef __DESCRAMBLE__
          /* Reset the descramble map pointer */
          ds_map_p = ds_map;
#else
          /* Set the frame pointer to the start of the frame */
          frame_p = frame->data;
#endif

          /* Set all the last frame stuff */
          last_frame = this_frame;
          last_packet = -1;
          byte_count = 0;
          
          /* The following is used for calculating rates */
          last_frame_timestamp.tv_sec  = this_frame_timestamp.tv_sec;
          last_frame_timestamp.tv_usec = this_frame_timestamp.tv_usec;
          this_frame_timestamp.tv_sec  = buffer->timestamp.tv_sec;
          this_frame_timestamp.tv_usec = buffer->timestamp.tv_usec; 

          thread_data.framerate = this_frame_timestamp.tv_sec - last_frame_timestamp.tv_sec;
          thread_data.framerate = ((double)(this_frame_timestamp.tv_usec - last_frame_timestamp.tv_usec) * 1e-6);
          if(thread_data.framerate != 0){
            thread_data.framerate = 1 / thread_data.framerate;
          }
        }

        /* Predict the next packet number */
        next_packet = (last_packet + 1) & 0xFF;

        if(this_packet != next_packet){
          /* we have skipped packets! */
          if(this_packet > next_packet){
            skipped_packets = this_packet - next_packet;
          } else {
            skipped_packets = (256 - next_packet) + this_packet;
          }

          /* increment Dropped packet counter */
          thread_data.dropped_packets += (unsigned long int)skipped_packets;
          
          /* Write zero values to dropped packet regions */
          /* NOTE : We could use unused bits to do this better? */
#ifdef __DESCRAMBLE__
          for(i=0;i<(CIN_DATA_PACKET_LEN * skipped_packets / 2);i++){
            *(frame->data + *ds_map_p) = CIN_DATA_DROPPED_PACKET_VAL;
            ds_map_p++;
          }
#else
          memset(frame_p, CIN_DATA_DROPPED_PACKET_VAL, 
                 CIN_DATA_PACKET_LEN * skipped_packets / 2);
          frame_p += CIN_DATA_PACKET_LEN * skipped_packets / 2;
#endif

          byte_count += CIN_DATA_PACKET_LEN * skipped_packets;

        } else {

#ifdef __DESCRAMBLE__
          /* Swap endienness of packet and copy to frame */
          for(i=0;i<(buffer_len / 2);i++){
            *(frame->data + *ds_map_p) = (*buffer_p << 8) + *(buffer_p + 1);
            /* Advance descramble map by 1 and buffer by 2 */ 
            ds_map_p++;
            buffer_p += 2;
          }
#else
          memcpy(frame_p, (uint16_t*)buffer_p, buffer_len);
          frame_p += buffer_len/2;
#endif
          byte_count += buffer_len;
        }
      } else { 
        thread_data.mallformed_packets++;
      } 

#ifdef __DEBUG__ 
      last_packet_header = this_packet_header;
#endif

      /* Now we are done with the packet, we can advance the fifo */

      pthread_mutex_lock(packet_mutex);
      fifo_advance_tail(thread_data.packet_fifo);
      pthread_mutex_unlock(packet_mutex);

      /* Now we can set the last packet to this packet */

      last_packet = this_packet;

      /* Check for full image */

      if(byte_count == CIN_DATA_FRAME_SIZE){
        /* Advance the frame fifo and signal that a new frame
           is avaliable. Perhaps there is a better way to do this? */
        frame->number = this_frame;
        frame->timestamp = this_frame_timestamp; /* taken from first packet */
        thread_data.last_frame = this_frame;

        pthread_mutex_lock(frame_mutex);
        fifo_advance_head(thread_data.frame_fifo);
        pthread_cond_signal(frame_signal);
        pthread_mutex_unlock(frame_mutex);
      }

    } else {
      /* Buffer is empty - wait for next packet */
      pthread_mutex_lock(packet_mutex);
      pthread_cond_wait(packet_signal, packet_mutex);
      pthread_mutex_unlock(packet_mutex);
    }
  }

  pthread_exit(NULL);
}

void *cin_data_monitor_thread(void){
  fprintf(stderr, "\033[?25l");

  while(1){
    fprintf(stderr, "Last frame %d\n", (unsigned int)thread_data.last_frame);

    fprintf(stderr, "Packet buffer is %6.2f %% full. Spool buffer is %6.2f %% full.\n",
            fifo_percent_full(thread_data.packet_fifo),
            fifo_percent_full(thread_data.frame_fifo));
    
    fprintf(stderr, "Framerate = %6.1f s^-1 : Dropped packets %10ld : Mallformed packets %6ld\r",
            thread_data.framerate, thread_data.dropped_packets, thread_data.mallformed_packets);
    fprintf(stderr, "\033[A\033[A"); /* Move up 2 lines */
    sleep(0.75);
  }

  pthread_exit(NULL);
}

void *cin_data_listen_thread(void){
  struct cin_data_packet *buffer = NULL;
  char* dummy = "DUMMY DATA";
  
  /* Send a packet to initialize the CIN */

  cin_data_write(thread_data.dp, dummy, sizeof(dummy));

  while(1){
    /* Get the next element in the fifo */
    pthread_mutex_lock(packet_mutex);
    buffer = (struct cin_data_packet*)fifo_get_head(thread_data.packet_fifo);
    pthread_mutex_unlock(packet_mutex);
    
    buffer->size = cin_data_read(thread_data.dp, buffer->data);
    gettimeofday(&buffer->timestamp, NULL);

    if (buffer->size > 1024){
      pthread_mutex_lock(packet_mutex);
      fifo_advance_head(thread_data.packet_fifo);
      pthread_cond_signal(packet_signal);
      pthread_mutex_unlock(packet_mutex);
    }
  }

  pthread_exit(NULL);
}

struct cin_data_frame* cin_data_get_next_frame(void){
  /* This routine gets the next frame. This will block until a frame is avaliable */
  struct cin_data_frame *frame = NULL;

  pthread_mutex_lock(frame_mutex);
  frame = (struct cin_data_frame*)fifo_get_tail(thread_data.frame_fifo);
  while(frame == NULL){
    frame = (struct cin_data_frame*)fifo_get_tail(thread_data.frame_fifo);
    /* block until frame is avaliable */
    pthread_cond_wait(frame_signal, frame_mutex);
  }
  pthread_mutex_unlock(frame_mutex);
  
  return frame;
}

void cin_data_release_frame(void){
  /* Advance the fifo */
  pthread_mutex_lock(frame_mutex);
  fifo_advance_tail(thread_data.frame_fifo);
  pthread_mutex_unlock(frame_mutex);
}

/* -----------------------------------------------------------------------------------------
 *
 * Routines for benchmarking
 *
 * -----------------------------------------------------------------------------------------
 */

struct timespec timespec_diff(struct timespec start, struct timespec end){
  /* Calculte the difference between two times */
  struct timespec temp;
  if ((end.tv_nsec-start.tv_nsec)<0) {
    temp.tv_sec = end.tv_sec-start.tv_sec-1;
    temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
  } else {
    temp.tv_sec = end.tv_sec-start.tv_sec;
    temp.tv_nsec = end.tv_nsec-start.tv_nsec;
  }
  return temp;
}

void timespec_copy(struct timespec *dest, struct timespec *src){
  dest->tv_sec = src->tv_sec;
  dest->tv_sec = src->tv_nsec;
}
