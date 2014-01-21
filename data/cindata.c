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
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "cin.h"
#include "fifo.h"
#include "cindata.h"
#include "mbuffer.h"
#include "descramble_map.h"

/* -------------------------------------------------------------------------------
 *
 * Thread communication global variables
 *
 * -------------------------------------------------------------------------------
 */

static pthread_t threads[MAX_THREADS];
static struct cin_data_thread_data thread_data;

/* -------------------------------------------------------------------------------
 *
 * Network functions to read from fabric UDP port
 *
 * -------------------------------------------------------------------------------
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
  dp->rcvbuf = dp->rcvbuf * 1024 * 1024; // Convert to Mb

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

  socklen_t rcvbuf_rb_len = sizeof(dp->rcvbuf_rb);
  if(getsockopt(dp->sockfd, SOL_SOCKET, SO_RCVBUF,
                &dp->rcvbuf_rb, &rcvbuf_rb_len) == -1){
    perror("CIN data port - unable to get receive buffer :");
  }

  DEBUG_PRINT("Recieve buffer = %ld Mb\n", dp->rcvbuf_rb / (1024 * 1024));

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

/* -------------------------------------------------------------------------------
 *
 * Main thread functions
 *
 * -------------------------------------------------------------------------------
 */

int cin_data_init(int packet_buffer_len, int frame_buffer_len, int show_stats){
  /* Initialize and start all the threads to acquire data */
  /* This does not block, just start threads */
  /* Setup FIFO elements */

  /* Packet FIFO */

  thread_data.packet_fifo = malloc(sizeof(fifo));
  if(fifo_init(thread_data.packet_fifo, sizeof(struct cin_data_packet), packet_buffer_len)){
    return 1;
  }

  /* Frame FIFO */

  thread_data.frame_fifo = malloc(sizeof(fifo));
  if(fifo_init(thread_data.frame_fifo, sizeof(struct cin_data_frame), frame_buffer_len)){
    return 1;
  }

  /* Image FIFO */

  thread_data.image_fifo = malloc(sizeof(fifo));
  if(fifo_init(thread_data.image_fifo, sizeof(struct cin_data_frame), frame_buffer_len)){
    return 1;
  }

  /* Image Double Buffer */

  thread_data.image_dbuffer = malloc(sizeof(mbuffer_t));
  if(mbuffer_init(thread_data.image_dbuffer, sizeof(struct cin_data_frame))){
    return 1;
  }

  /* Set some defaults */

  thread_data.mallformed_packets = 0;
  thread_data.dropped_packets = 0;

  /* Try to nice process */

  if(setpriority(PRIO_PROCESS, 0, -20)){
    perror("Unable to renice process");
  }

  /* Setup the needed mutexes */
  pthread_mutex_init(&thread_data.stats_mutex, NULL);

  pthread_create(&threads[0], NULL, (void *)cin_data_listen_thread, NULL);
  pthread_create(&threads[1], NULL, (void *)cin_data_assembler_thread, NULL);
  pthread_create(&threads[2], NULL, (void *)cin_data_descramble_thread, NULL);
  pthread_create(&threads[3], NULL, (void *)cin_data_monitor_thread, NULL);
  pthread_create(&threads[4], NULL, (void *)cin_data_monitor_output_thread, NULL);
  return 0;
}

void cin_data_wait_for_threads(void){
  /* This routine waits for the threads to stop 
     NOTE : This blocks until all threads complete */
  int i;
  for(i=0;i<5;i++){
    pthread_join(threads[i], NULL);
  }
}

int cin_data_stop_threads(void){
  int i;
  for(i=0;i<5;i++){
    pthread_cancel(threads[i]);
  }
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
  int this_frame = 0;
  int last_frame = -1;
  int this_packet = 0;
  int last_packet = 0;
  int this_packet_msb = 0;
  int last_packet_msb = 0;
  int skipped;

  int buffer_len;
  unsigned char *buffer_p;

  uint16_t *frame_p;
  int i;

  uint64_t header;

  struct timespec last_frame_timestamp = {0,0};
  struct timespec this_frame_timestamp = {0,0};

  /* Get first frame pointer from the buffer */

  frame = (struct cin_data_frame*)fifo_get_head(thread_data.frame_fifo);

  pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

  while(1){

    /* Get a packet from the fifo */

    buffer = (struct cin_data_packet*)fifo_get_tail(thread_data.packet_fifo);

    if(buffer == NULL){
      /* We don't have a frame, continue */
      continue;
    }

    /* Start assebleing frame */

    buffer_p = buffer->data;
    buffer_len = buffer->size - CIN_DATA_UDP_HEADER;
    if(buffer_len > CIN_DATA_PACKET_LEN){
      /* Dump the frame and continue */
      fifo_advance_tail(thread_data.packet_fifo);
      thread_data.mallformed_packets++;
      continue;
    }

    /* Next lets check the magic number of the packet */
    header = *((uint64_t *)buffer_p) & CIN_DATA_MAGIC_PACKET_MASK; 

    if(header != CIN_DATA_MAGIC_PACKET) {
      /* Dump the packet and continue */
      fifo_advance_tail(thread_data.packet_fifo);
      thread_data.mallformed_packets++;
    }
    
    /* First byte of frame header is the packet no*/
    /* Bytes 7 and 8 are the frame number */ 

    this_packet = *buffer_p; 
    buffer_p += 6;
    this_frame  = (*buffer_p << 8) + *(buffer_p + 1);
    buffer_p += 2;

    if(this_frame != last_frame){
      /* We have a new frame */

      if(frame){
        frame->number = this_frame;
        frame->timestamp = this_frame_timestamp;
        thread_data.last_frame = this_frame;

        fifo_advance_head(thread_data.frame_fifo);

        frame = NULL;
      }

      /* Get the next frame buffer */

      frame = (struct cin_data_frame*)fifo_get_head(thread_data.frame_fifo);

      /* Set all the last frame stuff */
      last_frame = this_frame;
      last_packet = -1;
      this_packet_msb = 0;
      last_packet_msb = 0;
        
      last_frame_timestamp  = this_frame_timestamp;
      this_frame_timestamp  = buffer->timestamp;
      thread_data.framerate = timespec_diff(last_frame_timestamp,this_frame_timestamp);
    } // this_frame != last_frame 

    if(this_packet <= last_packet){
      this_packet_msb += 0x100;
    }

    skipped = (this_packet + this_packet_msb) - (last_packet + last_packet_msb + 1);
    if(skipped){
      thread_data.dropped_packets += skipped;
    }

    /* Do some bounds checking */

    if((this_packet + this_packet_msb) < CIN_DATA_MAX_PACKETS){
      // Copy the data and swap the endieness
      frame_p = frame->data;
      frame_p += (this_packet + this_packet_msb) * CIN_DATA_PACKET_LEN / 2;
      for(i=0;i<buffer_len;i++){
        *frame_p = *buffer_p << 8;
        buffer_p++;
        *frame_p += *buffer_p;
        buffer_p++;
        frame_p++;
      }
    } 

    /* Now we can set the last packet to this packet */

    last_packet = this_packet;
    last_packet_msb = this_packet_msb;

    /* Now we are done with the packet, we can advance the fifo */

    fifo_advance_tail(thread_data.packet_fifo);

  } // while(1)

  pthread_exit(NULL);
}

void *cin_data_monitor_thread(void){
  //fprintf(stderr, "\033[?25l");
  double framerate = 0;
  double framerate_smoothed =0;
  double f;

  unsigned int last_frame = 0;

  while(1){

    if((unsigned int)thread_data.last_frame != last_frame){
      // Compute framerate

      f = ((double)thread_data.framerate.tv_nsec * 1e-9);
      if(f == 0){
        f = 0;
      } else {
        f = 1 / f;
      }

      // Provide some smoothing to the data

      framerate_smoothed -= framerate_smoothed / 2.0;
      framerate_smoothed += f / 2.0;
      framerate = framerate_smoothed;
    } else {
      framerate = 0; // we are idle 
    }

    /* We should probably wrap this in a mutex */
  
    pthread_mutex_lock(&thread_data.stats_mutex);

    thread_data.stats.last_frame = (int)thread_data.last_frame;
    thread_data.stats.framerate = framerate;
    thread_data.stats.packet_percent_full = fifo_percent_full(thread_data.packet_fifo);
    thread_data.stats.frame_percent_full = fifo_percent_full(thread_data.frame_fifo);
    thread_data.stats.image_percent_full = fifo_percent_full(thread_data.image_fifo);
    thread_data.stats.dropped_packets = thread_data.dropped_packets;
    thread_data.stats.mallformed_packets = thread_data.mallformed_packets;

    pthread_mutex_unlock(&thread_data.stats_mutex);

    sleep(0.1);
  }

  pthread_exit(NULL);
}

void *cin_data_monitor_output_thread(void){
   /* Output to screen monitored values */
  struct cin_data_stats stats;

  while(1){
    pthread_mutex_lock(&thread_data.stats_mutex);
    stats = thread_data.stats;
    pthread_mutex_unlock(&thread_data.stats_mutex);

    fprintf(stderr, "Last frame %-12d\n", stats.last_frame);

    fprintf(stderr, "Packet buffer %8.3f%%.", 
            stats.packet_percent_full);
    fprintf(stderr, " Image buffer %8.3f%%.",
            stats.frame_percent_full);
    fprintf(stderr, " Spool buffer %8.3f%%.\n",
            stats.image_percent_full);
    
    fprintf(stderr, "Framerate = %6.1f s^-1 : Dropped packets %10ld : Mallformed packets %6ld\r",
            stats.framerate, stats.dropped_packets, stats.mallformed_packets);
    fprintf(stderr, "\033[A\033[A"); // Move up 2 lines 

    sleep(0.5);
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
    buffer = (struct cin_data_packet*)fifo_get_head(thread_data.packet_fifo);
   
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

    buffer->size = cin_data_read(thread_data.dp, buffer->data);
    clock_gettime(CLOCK_REALTIME, &buffer->timestamp);

    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

    fifo_advance_head(thread_data.packet_fifo);
  }

  pthread_exit(NULL);
}


void* cin_data_descramble_thread(void){
  /* This routine gets the next frame and descrambles is */
  struct cin_data_frame *frame = NULL;
  struct cin_data_frame *image = NULL;
  struct cin_data_frame *buffer = NULL;
  int i;
  uint32_t *dsmap = (uint32_t*)descramble_map_forward_bin;
  uint32_t *dsmap_p;
  uint16_t *data_p;

  while(1){
    // Get a frame 
    
    frame = (struct cin_data_frame*)fifo_get_tail(thread_data.frame_fifo);
    image = (struct cin_data_frame*)fifo_get_head(thread_data.image_fifo);
    buffer = (struct cin_data_frame*)mbuffer_get_write_buffer(thread_data.image_dbuffer);

    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

    dsmap_p = dsmap;
    data_p  = frame->data;
    for(i=0;i<(CIN_DATA_FRAME_HEIGHT * CIN_DATA_FRAME_WIDTH);i++){
      image->data[*dsmap_p] = *data_p;
      buffer->data[*dsmap_p] = *data_p;
      dsmap_p++;
      data_p++;
    }

    image->timestamp = frame->timestamp;
    image->number = frame->number;
    buffer->timestamp = frame->timestamp;
    buffer->number = frame->number;
  
    // Release the frame and the image

    fifo_advance_tail(thread_data.frame_fifo);
    fifo_advance_head(thread_data.image_fifo);
    mbuffer_write_done(thread_data.image_dbuffer);

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
  }

  pthread_exit(NULL);
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

/* -------------------------------------------------------------------------------
 *
 * Routines for accessing the image buffer
 *
 * -------------------------------------------------------------------------------
 */

struct cin_data_frame* cin_data_get_next_frame(void){
  /* This routine gets the next frame. This will block until a frame is avaliable */
  struct cin_data_frame *frame = NULL;

  frame = (struct cin_data_frame*)fifo_get_tail(thread_data.image_fifo);
  return frame;
}

void cin_data_release_frame(int free_mem){
  /* Advance the fifo */
  fifo_advance_tail(thread_data.image_fifo);
}

/* -------------------------------------------------------------------------------
 *
 * Routines for accessing the double buffer
 *
 * -------------------------------------------------------------------------------
 */

struct cin_data_frame* cin_data_get_buffered_frame(void){
  /* This routine gets the buffered frame. 
     This will block until a frame is avaliable */
  struct cin_data_frame *frame = NULL;

  frame = (struct cin_data_frame*)mbuffer_get_read_buffer(thread_data.image_dbuffer);
  return frame;
}

void cin_data_release_buffered_frame(void){
  mbuffer_read_done(thread_data.image_dbuffer);
}

/* -------------------------------------------------------------------------------
 *
 * Routines for accessing the statistics
 *
 * -------------------------------------------------------------------------------
 */

struct cin_data_stats cin_data_get_stats(void){
  /* Return the stats on the data */
  struct cin_data_stats s;
  
  pthread_mutex_lock(&thread_data.stats_mutex);
  s = thread_data.stats;
  pthread_mutex_unlock(&thread_data.stats_mutex);

  return s;
}
