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

static cin_data_threads_t *threads;
static cin_data_thread_data_t thread_data;

/* -------------------------------------------------------------------------------
 *
 * Network functions to read from fabric UDP port
 *
 * -------------------------------------------------------------------------------
 */

int cin_init_data_port(struct cin_port* dp, 
                       char* ipaddr, uint16_t port,
                       char* cin_ipaddr, uint16_t cin_port,
                       int rcvbuf) {

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
  
  // Do debur prints

  DEBUG_PRINT("Client address = %s:%d\n", dp->cliaddr, dp->cliport);
  DEBUG_PRINT("Server address = %s:%d\n", dp->srvaddr, dp->srvport);

  dp->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if(dp->sockfd < 0) {
    perror("CIN data port - socket() failed !!!");
    return 1;
  }

  DEBUG_PRINT("Opened socket :%d\n", dp->sockfd);

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

  DEBUG_PRINT("Requesting recieve buffer of %d Mb\n", dp->rcvbuf / (1024*1024));

  if(setsockopt(dp->sockfd, SOL_SOCKET, SO_RCVBUF, 
                &dp->rcvbuf, sizeof(dp->rcvbuf)) == -1){
    perror("CIN data port - unable to set receive buffer :");
  } 

  socklen_t rcvbuf_rb_len = sizeof(dp->rcvbuf_rb);
  if(getsockopt(dp->sockfd, SOL_SOCKET, SO_RCVBUF,
                &dp->rcvbuf_rb, &rcvbuf_rb_len) == -1){
    perror("CIN data port - unable to get receive buffer :");
  }

  DEBUG_PRINT("Recieve buffer = %d Mb\n", dp->rcvbuf_rb / (1024 * 1024));

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

int cin_data_write(struct cin_port* dp, char* buffer, int buffer_len){
  return sendto(dp->sockfd, buffer, buffer_len, 0,
                (struct sockaddr*)&dp->sin_srv, sizeof(dp->sin_srv));
}

/* -------------------------------------------------------------------------------
 *
 * Initialization Functions
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
  struct cin_data_frame *q;
  long int i;
  q = (struct cin_data_frame*)(thread_data.frame_fifo->data);
  for(i=0;i<thread_data.frame_fifo->size;i++){
    //q->data              = (uint16_t *)NULL;  
    q->data = malloc(sizeof(uint16_t) * CIN_DATA_FRAME_WIDTH * CIN_DATA_FRAME_HEIGHT);
    if(q->data == NULL){
      return 1;
    }
    q->number            = 0;
    q->timestamp.tv_sec  = 0;
    q->timestamp.tv_nsec = 0;
    q++;
  } 

  /* Image FIFO */

  thread_data.image_fifo = malloc(sizeof(fifo));
  if(fifo_init(thread_data.image_fifo, sizeof(struct cin_data_frame), frame_buffer_len)){
    return 1;
  }
  q = (struct cin_data_frame*)(thread_data.image_fifo->data);
  for(i=0;i<thread_data.image_fifo->size;i++){
    q->data = malloc(sizeof(uint16_t) * CIN_DATA_FRAME_WIDTH * CIN_DATA_FRAME_HEIGHT);
    if(q->data == NULL){
      return 1;
    }
    q->number            = 0;
    q->timestamp.tv_sec  = 0;
    q->timestamp.tv_nsec = 0;
    q++;
  }

  /* Image Double Buffer */

  thread_data.image_dbuffer = malloc(sizeof(mbuffer_t));
  if(mbuffer_init(thread_data.image_dbuffer, sizeof(struct cin_data_frame))){
    return 1;
  }

  /* Push Pull Buffer */

  thread_data.image_buffer = malloc(sizeof(image_buffer_t));
  thread_data.image_buffer->data = malloc(sizeof(cin_data_frame_t));
  thread_data.image_buffer->waiting = 0;

  /* Set some defaults */

  thread_data.mallformed_packets = 0;
  thread_data.dropped_packets = 0;

  /* Setup the needed mutexes */
  pthread_mutex_init(&thread_data.stats_mutex, NULL);

  /* Setup threads */

  threads = malloc(sizeof(cin_data_threads_t) * MAX_THREADS);
  if(!threads){
    return 1;
  }

  for(i=0;i<MAX_THREADS;i++){
    threads[i].started = 0;
  }

  /* Setup connections between processes */

  cin_data_proc_t *listen = malloc(sizeof(cin_data_proc_t));
  listen->output_get = (void*)&fifo_get_head;
  listen->output_put = (void*)&fifo_advance_head;
  listen->output_args = (void*)thread_data.packet_fifo;

  cin_data_proc_t *assemble = malloc(sizeof(cin_data_proc_t));
  assemble->input_get = (void*)&fifo_get_tail;
  assemble->input_put = (void*)&fifo_advance_tail;
  assemble->input_args = (void*)thread_data.packet_fifo;
  assemble->output_get = (void*)&fifo_get_head;
  assemble->output_put = (void*)&fifo_advance_head;
  assemble->output_args = (void*)thread_data.frame_fifo;

  cin_data_proc_t *descramble = malloc(sizeof(cin_data_proc_t));
  descramble->input_get = (void*)&fifo_get_tail;
  descramble->input_put = (void*)&fifo_advance_tail;
  descramble->input_args = (void*)thread_data.frame_fifo;
  //descramble->output_get = (void*)&fifo_get_head;
  //descramble->output_put = (void*)&fifo_advance_head;
  //descramble->output_args = (void*)thread_data.image_fifo;

  descramble->output_get = (void*)&cin_data_buffer_push;
  descramble->output_put = (void*)&cin_data_buffer_pop;
  descramble->output_args = (void*)thread_data.image_buffer;

  cin_data_thread_start(&threads[0], (void *)cin_data_listen_thread, (void *)listen,
                        SCHED_RR, 1); 
  cin_data_thread_start(&threads[1], (void *)cin_data_assembler_thread, (void *)assemble, 
                        SCHED_RR, 1);
  cin_data_thread_start(&threads[2], (void *)cin_data_descramble_thread, (void *)descramble,
                        SCHED_RR, 1);
  cin_data_thread_start(&threads[3], (void *)cin_data_monitor_thread, NULL,
                        SCHED_RR, 10);
  if(show_stats){
    cin_data_thread_start(&threads[4], (void *)cin_data_monitor_output_thread, NULL,
                          SCHED_RR, 10);
  }
  return 0;
}


int cin_data_thread_start(cin_data_threads_t *thread, void *(*func) (void *), 
                          void *arg, int policy, int priority){
  int rtn;
#ifdef _POSIX_THREAD_PRIORITY_SCHEDULING
  struct sched_param thread_param;
  int min, max;
  max = sched_get_priority_max(policy);
  DEBUG_PRINT("Max priority : %d\n", max);
  min = sched_get_priority_min(policy);
  DEBUG_PRINT("Min priority : %d\n", min);
  thread_param.sched_priority = (max - min) / priority;

  DEBUG_PRINT("Setting priority to %d\n", thread_param.sched_priority);

  pthread_attr_t thread_attr;
  pthread_attr_init(&thread_attr);
  pthread_attr_setschedpolicy(&thread_attr, policy);
  pthread_attr_setschedparam(&thread_attr, &thread_param);
  pthread_attr_setinheritsched(&thread_attr, PTHREAD_EXPLICIT_SCHED);
 
  rtn = pthread_create(&thread->thread_id, &thread_attr, func, arg);
  if(rtn == 0){
    DEBUG_COMMENT("Started thread\n");
    thread->started = 1;
    return 0;
  } else {
    DEBUG_COMMENT("Cannot set scheduling policy .. setting default.\n");
    rtn = pthread_create(&thread->thread_id, NULL, func, arg);
    if(rtn == 0){
      thread->started = 1;
      return 0;
    } else {
      thread->started = 0;
      return 1;
    }
  }

#else
  DEBUG_COMMENT("Priority Scheduling Not Supported\n");
  rtn = pthread_create(&thread->thread_id, NULL, func, arg);
  if(rtn == 0){
    thread->started = 1;
    return 0;
  }
  thread->started = 0;
  return 1;
#endif
}

void cin_data_wait_for_threads(void){
  /* This routine waits for the threads to stop 
     NOTE : This blocks until all threads complete */
  int i;
  for(i=0;i<MAX_THREADS;i++){
    if(threads[i].started){
      pthread_join(threads[i].thread_id, NULL);
      DEBUG_PRINT("Thread %d joined\n", i);
    }
  }
}

int cin_data_stop_threads(void){
  int i;
  DEBUG_COMMENT("Cancel Requested\n");
  for(i=0;i<MAX_THREADS;i++){
    if(threads[i].started){
      pthread_cancel(threads[i].thread_id);
      DEBUG_PRINT("Sending cancel to thread: %d\n", i);
    }
  }
  return 0;
}

/* -----------------------------------------------------------------------------------------
 *
 * MAIN Thread Routines
 *
 * -----------------------------------------------------------------------------------------
 */

void *cin_data_assembler_thread(void *args){
  
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

  cin_data_proc_t *proc = (cin_data_proc_t*)args;

  /* Get first frame pointer from the buffer */

  frame = (cin_data_frame_t*)(*proc->input_get)(proc->input_args);

  while(1){

    /* Get a packet from the fifo */

    buffer = (cin_data_packet_t*)(*proc->input_get)(proc->input_args);

    if(buffer == NULL){
      /* We don't have a frame, continue */
      continue;
    }

    /* Start assebleing frame */

    buffer_p = buffer->data;
    buffer_len = buffer->size - CIN_DATA_UDP_HEADER;
    if(buffer_len > CIN_DATA_PACKET_LEN){
      /* Dump the frame and continue */
      (*proc->input_put)(proc->input_args);
      thread_data.mallformed_packets++;
      continue;
    }

    /* Next lets check the magic number of the packet */
    header = *((uint64_t *)buffer_p) & CIN_DATA_MAGIC_PACKET_MASK; 

    if(header != CIN_DATA_MAGIC_PACKET) {
      /* Dump the packet and continue */
      (*proc->input_put)(proc->input_args);
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

        (*proc->output_put)(proc->output_args);

        frame = NULL;
      }

      /* Get the next frame buffer */

      frame = (cin_data_frame_t*)(*proc->output_get)(proc->output_args);

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

    (*proc->input_put)(proc->input_args);

  } // while(1)

  pthread_exit(NULL);
}

void *cin_data_monitor_thread(void){
  //fprintf(stderr, "\033[?25l");
  double framerate = 0;
  double framerate_smoothed =0;
  double alpha = 0.25; // Smooth over 4 readings
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

      framerate_smoothed = (alpha * f) + ((1.0 - alpha) * framerate_smoothed);
      framerate = framerate_smoothed;
    } else {
      framerate = 0; // we are idle 
    }

    /* We should probably wrap this in a mutex */
  
    pthread_mutex_lock(&thread_data.stats_mutex);

    last_frame = (int)thread_data.last_frame;
    thread_data.stats.last_frame = last_frame;
    thread_data.stats.framerate = framerate;
    thread_data.stats.packet_percent_full = fifo_percent_full(thread_data.packet_fifo);
    thread_data.stats.frame_percent_full = fifo_percent_full(thread_data.frame_fifo);
    thread_data.stats.image_percent_full = fifo_percent_full(thread_data.image_fifo);
    thread_data.stats.dropped_packets = thread_data.dropped_packets;
    thread_data.stats.mallformed_packets = thread_data.mallformed_packets;

    pthread_mutex_unlock(&thread_data.stats_mutex);

    usleep(CIN_DATA_MONITOR_UPDATE);
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

    usleep(CIN_DATA_MONITOR_UPDATE);
  }

  pthread_exit(NULL);
}

void *cin_data_listen_thread(void *args){
  
  struct cin_data_packet *buffer = NULL;
  char* dummy = "DUMMY DATA";
  cin_data_proc_t *proc = (cin_data_proc_t*)args;
 
  /* Send a packet to initialize the CIN */

  cin_data_write(thread_data.dp, dummy, sizeof(dummy));

  while(1){
    /* Get the next element in the fifo */
    buffer = (cin_data_packet_t*)(*proc->output_get)(proc->output_args);
   
    buffer->size = cin_data_read(thread_data.dp, buffer->data);
    clock_gettime(CLOCK_REALTIME, &buffer->timestamp);

    (*proc->output_put)(proc->output_args);
  }

  pthread_exit(NULL);
}


void* cin_data_descramble_thread(void *args){
  /* This routine gets the next frame and descrambles is */
  struct cin_data_frame *frame = NULL;
  struct cin_data_frame *image = NULL;
  //struct cin_data_frame *buffer = NULL;
  int i;
  uint32_t *dsmap = (uint32_t*)descramble_map_forward_bin;
  uint32_t *dsmap_p;
  uint16_t *data_p;

  cin_data_proc_t *proc = (cin_data_proc_t*)args;

  while(1){
    // Get a frame 
    
    frame = (cin_data_frame_t*)(*proc->input_get)(proc->input_args);
    image = (cin_data_frame_t*)(*proc->output_get)(proc->output_args);
    //buffer = (struct cin_data_frame*)mbuffer_get_write_buffer(thread_data.image_dbuffer);

    dsmap_p = dsmap;
    data_p  = frame->data;
    for(i=0;i<(CIN_DATA_FRAME_HEIGHT * CIN_DATA_FRAME_WIDTH);i++){
      image->data[*dsmap_p] = *data_p;
      //buffer->data[*dsmap_p] = *data_p;
      dsmap_p++;
      data_p++;
    }

    image->timestamp = frame->timestamp;
    image->number = frame->number;
    //buffer->timestamp = frame->timestamp;
    //buffer->number = frame->number;
  
    // Release the frame and the image

    (*proc->input_put)(proc->input_args);
    (*proc->output_put)(proc->output_args);
    //mbuffer_write_done(thread_data.image_dbuffer);

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


/* -------------------------------------------------------------------------------
 *
 * Routines for sequential access to data
 *
 * -------------------------------------------------------------------------------
 */

int cin_data_load_frame(uint16_t *buffer, uint16_t *frame_num){
  //DEBUG_PRINT("Called with %p\n", buffer);
  pthread_mutex_lock(&thread_data.image_buffer->mutex);

  // Load the buffer pointer into the buffer
  thread_data.image_buffer->data->data = buffer;
  //DEBUG_PRINT("Data = %p\n", thread_data.image_buffer->data->data);
  thread_data.image_buffer->waiting = 1;
  pthread_cond_signal(&thread_data.image_buffer->signal_push);
  //DEBUG_COMMENT("Sent Signal\n");

  // Now wait for the data to be copied
  pthread_cond_wait(&thread_data.image_buffer->signal_pop, 
                    &thread_data.image_buffer->mutex);

  //DEBUG_COMMENT("Got Signal\n");
  *frame_num = thread_data.image_buffer->data->number;

  // We are no longer waiting
  thread_data.image_buffer->waiting = 0;

  // Now we have a valid image, we can return
  pthread_mutex_unlock(&thread_data.image_buffer->mutex);

  //DEBUG_COMMENT("Returning\n");

  return 0;
}

void* cin_data_buffer_push(void *arg){
  void *rtn; 
  image_buffer_t *buffer = (image_buffer_t*)arg;

  pthread_mutex_lock(&buffer->mutex);
  //DEBUG_COMMENT("START\n");
  if(!buffer->waiting){
    // If the waiting flag has gone up
    // we signaled but this routine was not listening
    //DEBUG_COMMENT("Waiting\n");
    pthread_cond_wait(&buffer->signal_push,
                      &buffer->mutex);
  }
  rtn = (void *)(buffer->data);
  //DEBUG_COMMENT("END\n");
  pthread_mutex_unlock(&buffer->mutex);
  return rtn;
}

void cin_data_buffer_pop(void *arg){
  image_buffer_t *buffer = (image_buffer_t*)arg;

  //DEBUG_COMMENT("START\n");
  pthread_mutex_lock(&buffer->mutex);
  pthread_cond_signal(&buffer->signal_pop);
  pthread_mutex_unlock(&buffer->mutex);
  //DEBUG_COMMENT("END\n");
}
