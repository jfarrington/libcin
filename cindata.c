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

#include <tiffio.h>

#include "cindata.h"
#include "descramble_block.h"
#include "bpfilter.h"

/* -----------------------------------------------------------------------------------------
 *
 * Network functions to read from fabric UDP port
 *
 * -----------------------------------------------------------------------------------------
 */

int net_connect(cin_fabric_iface *iface){
  /* Connect and bind to the interface */
  if(iface->mode == CIN_SOCKET_MODE_UDP){
    if(!net_open_socket_udp(iface)){
      perror("Unable to open socket!");
      return FALSE;
    }
    if(!net_bind_to_address(iface)){
      perror("Unable to bind to address");
      return FALSE;
    }
  } else if(iface->mode == CIN_SOCKET_MODE_BFP){
    if(!net_open_socket_bfp(iface)){
      perror("Unable to open socket!");
      return FALSE;
    }
    if(!net_set_packet_filter(iface)){
      perror("Unable to set packet filter!");
      return FALSE;
    }
    if(!net_set_promisc(iface, TRUE)){
      perror("Unable to set promisc mode on interface");
      return FALSE;
    }
    if(!net_bind_to_interface(iface)){
      perror("Unable to bind to ethernet interface");
      return FALSE;
    }
  } else {
    fprintf(stderr, "Unknown communication mode\n");
    return FALSE;
  }

  if(!net_set_buffers(iface)){
    perror("WARNING : Unable to set buffer size");
  }

  return TRUE;
}

void net_set_default(cin_fabric_iface *iface){

  iface->mode = CIN_SOCKET_MODE_UDP;
  iface->fd = -1;
  iface->svrport = CIN_SVRPORT;
  iface->header_len = 0;
  iface->recv_buffer = 0x3200000;
  strcpy(iface->iface_name, CIN_IFACE_NAME);
  strcpy(iface->svraddr,    CIN_SVRADDR);
}

int net_set_buffers(cin_fabric_iface *iface){
  /* Set the receieve buffers for the socket */
  unsigned int size;
  unsigned int size_len;

  size = (unsigned int)iface->recv_buffer;
  if(setsockopt(iface->fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size)) == -1){
    perror("Unable to set receive buffer :");
  } else {
    size_len = sizeof(size);
    if(getsockopt(iface->fd, SOL_SOCKET, SO_RCVBUF, &size, &size_len) == -1){
      perror("Unable to get receive buffer :");
    };
  }
  return TRUE;
}

int net_set_promisc(cin_fabric_iface *iface, int val){
  /* Set the network card promiscuos mode based on swicth*/

  struct ifreq ethreq;

  memset(&ethreq, 0, sizeof(ethreq));
  strncpy(ethreq.ifr_name, iface->iface_name, IFNAMSIZ);
  if (ioctl(iface->fd,SIOCGIFFLAGS,&ethreq) == -1) {
    return FALSE;
  } 

  if(val){
    ethreq.ifr_flags |= IFF_PROMISC;
  } else {
    ethreq.ifr_flags &= ~IFF_PROMISC;
  }

  if (ioctl(iface->fd,SIOCSIFFLAGS,&ethreq) == -1) {
    return FALSE;
  }

  return TRUE;
}

int net_bind_to_interface(cin_fabric_iface *iface){
  struct sockaddr_ll sll;
  struct ifreq ethreq;

  memset(&ethreq, 0, sizeof(ethreq));
  strncpy(ethreq.ifr_name, iface->iface_name, IFNAMSIZ);

  if (ioctl(iface->fd, SIOCGIFINDEX, &ethreq) == -1) {
    return FALSE;
  }

  sll.sll_family = AF_PACKET; 
  sll.sll_ifindex = ethreq.ifr_ifindex;
  sll.sll_protocol = htons(ETH_P_IP);
  if((bind(iface->fd, (struct sockaddr *)&sll , sizeof(sll))) == -1){
    return FALSE;
  }

  return TRUE;
}

int net_bind_to_address(cin_fabric_iface *iface) {
  struct sockaddr_in addr;
  struct in_addr inp;

  if(inet_aton(iface->svraddr, &inp) < 0){
    return FALSE;
  }

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inp.s_addr;
  addr.sin_port = htons(iface->svrport);

  if(bind(iface->fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    return FALSE;
  }

  return TRUE;
}

int net_open_socket_bfp(cin_fabric_iface *iface){
  if ((iface->fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_IP))) < 0) {
    return FALSE;
  }

  iface->header_len = CIN_UDP_PACKET_HEADER;
  return TRUE;

}

int net_open_socket_udp(cin_fabric_iface *iface){
  int i = 1;

  if ((iface->fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    return FALSE;
  }

  if(setsockopt(iface->fd, SOL_SOCKET, SO_REUSEADDR,
                (void *)&i, sizeof(i)) < 0){
    return FALSE;
  }

  iface->header_len = 0;
  return TRUE;
}

int net_set_packet_filter(cin_fabric_iface *iface){
  /* Set the packet filter for the BFP use of
     the UDP data stream */

  struct sock_fprog FILTER_CIN_SOURCE_IP_PORT;

  /* Defile filter */
  FILTER_CIN_SOURCE_IP_PORT.len = BPF_CIN_SOURCE_IP_PORT_LEN;
  FILTER_CIN_SOURCE_IP_PORT.filter = BPF_CIN_SOURCE_IP_PORT;

  /* Attach the filter to the socket */
  if(setsockopt(iface->fd, SOL_SOCKET, SO_ATTACH_FILTER,
                &FILTER_CIN_SOURCE_IP_PORT, sizeof(FILTER_CIN_SOURCE_IP_PORT))<0){
    return FALSE;
  }

  return TRUE;
}

int net_read(cin_fabric_iface *iface, unsigned char* buffer){
  /* Read from the UDP stream */  
  return recvfrom(iface->fd, buffer, CIN_MAX_MTU * sizeof(unsigned char), 0, NULL, NULL);
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

  if(bytes > (f->size * f->elem_size)){
    fprintf(stderr, "\n\n\n\n%p.%p,%p,%p\n\n\n\n", f->head, f->tail, f->end, f->data);
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

int cin_start_threads(cin_thread *data){
  /* Initialize and start all the threads to acquire data */

  pthread_t listener, writer, monitor, assembler;

  /* Setup FIFO elements */

  data->packet_fifo = malloc(sizeof(fifo));
  data->frame_fifo  = malloc(sizeof(fifo));

  if(!cin_initialize_fifo(data, 200, 200)){
    return FALSE;
  } 

  /* Set some defaults */

  data->mallformed_packets = 0;
  data->dropped_packets = 0;
  data->framerate = 0;

  /* Setup Mutexes */

  data->packet_mutex  = malloc(sizeof(pthread_mutex_t));
  data->frame_mutex   = malloc(sizeof(pthread_mutex_t));
  data->packet_signal = malloc(sizeof(pthread_cond_t));
  data->frame_signal  = malloc(sizeof(pthread_cond_t));

  pthread_mutex_init(data->packet_mutex, NULL);
  pthread_mutex_init(data->frame_mutex, NULL);
  pthread_cond_init(data->packet_signal, NULL);
  pthread_cond_init(data->frame_signal, NULL);

  /* Start threads */

  pthread_create(&listener, NULL, (void *)cin_listen_thread, (void *)data);
  pthread_create(&assembler, NULL, (void *)cin_assembler_thread, (void *)data);
  pthread_create(&writer, NULL, (void *)cin_write_thread, (void *)data);
  pthread_create(&monitor, NULL, (void *)cin_monitor_thread, (void *)data);
  pthread_join(listener, NULL);
  pthread_join(writer, NULL);
  pthread_join(monitor, NULL);
  pthread_join(assembler, NULL);

  return TRUE;
}

int cin_initialize_fifo(cin_thread *data, long int packet_size, long int frame_size){
  /* Inilialize the FIFO elements */

  cin_packet_fifo *p;
  cin_frame_fifo *q;
  long int i;

  /* Packet FIFO */

  if(fifo_init(data->packet_fifo, sizeof(cin_packet_fifo), 20000) == FALSE){
    return FALSE;
  }

  p = (cin_packet_fifo*)(data->packet_fifo->data);
  for(i=0;i<data->packet_fifo->size;i++){
    p->data = malloc(sizeof(unsigned char) * CIN_MAX_MTU);
    if(p->data == NULL){
      return FALSE;
    }
    p->size = 0;
    p++;
  }

  /* Frame FIFO */

  if(fifo_init(data->frame_fifo, sizeof(cin_frame_fifo), 200) == FALSE){
    return FALSE;
  }

  q = (cin_frame_fifo*)(data->frame_fifo->data);
  for(i=0;i<data->frame_fifo->size;i++){
    q->data = malloc(sizeof(uint16_t) * CIN_FRAME_WIDTH * CIN_FRAME_HEIGHT);
    if(!q->data){
      return FALSE;
    }
    q->number = 0;
    q->timestamp.tv_sec = 0;
    q->timestamp.tv_usec = 0;
    q++;
  }

  return TRUE; 
}

/* -----------------------------------------------------------------------------------------
 *
 * MAIN Thread Routines
 *
 * -----------------------------------------------------------------------------------------
 */

void *cin_assembler_thread(cin_thread *data){
  
  cin_packet_fifo *buffer = NULL;
  cin_frame_fifo *frame = NULL;
  unsigned int this_frame = 0;
  unsigned int last_frame = -1;
  unsigned int this_packet = 0;
  unsigned int last_packet = -1;
  unsigned int skipped_packets = -1;
  unsigned int next_packet;

  uint64_t last_packet_header = 0;
  uint64_t this_packet_header = 0;

  int buffer_len;
  unsigned char *buffer_p;

  uint64_t header;

  /* Descramble Map */

  uint32_t *ds_map; 
  uint32_t *ds_map_p;

  long int i;
  long int byte_count = 0;

  struct timeval last_frame_timestamp = {0,0};
  struct timeval this_frame_timestamp = {0,0};

  /* Set descramble map */

  ds_map = (uint32_t*)descramble_map;
  ds_map_p = ds_map;

  /* Get first frame pointer from stack */

  pthread_mutex_lock(data->frame_mutex);
  frame = (cin_frame_fifo*)fifo_get_head(data->frame_fifo);
  pthread_mutex_unlock(data->frame_mutex);

  while(1){

    /* Lock the mutex and get a packet from the fifo */

    pthread_mutex_lock(data->packet_mutex);
    buffer = (cin_packet_fifo*)fifo_get_tail(data->packet_fifo);
    pthread_mutex_unlock(data->packet_mutex);

    if(buffer != NULL){
      /* Start assebleing frame */

      buffer_p = buffer->data;
      buffer_len = buffer->size - data->iface->header_len - CIN_UDP_DATA_HEADER;

      /* Skip header (defined in interface */

      buffer_p += data->iface->header_len;

      /* Next lets check the magic number of the packet */
      header = *((uint64_t *)buffer_p) & CIN_MAGIC_PACKET_MASK; 

      if(header == CIN_MAGIC_PACKET) {; 
        
        /* First byte of frame header is the packet no*/
        /* Bytes 7 and 8 are the frame number */ 
    
        this_packet_header = *(uint64_t*)buffer_p;

        this_packet = *buffer_p; 
        buffer_p += 6;
        this_frame  = (*buffer_p << 8) + *(buffer_p + 1);
        buffer_p += 2;

        if(this_frame != last_frame){
          /* We have a new frame */

          /* Lock the mutex and get the next frame buffer */
          pthread_mutex_lock(data->frame_mutex);
          frame = (cin_frame_fifo*)fifo_get_head(data->frame_fifo);
          pthread_mutex_unlock(data->frame_mutex);
         
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
          if(this_packet > next_packet){
            skipped_packets = this_packet - next_packet;
          } else {
            skipped_packets = (256 - next_packet) + this_packet;
          }

          /* increment Dropped packet counter */
          data->dropped_packets += (unsigned long int)skipped_packets;
          fprintf(stderr, "\n\n\n\n\n %d, %d, %d, %d\n", skipped_packets, last_packet, this_packet, next_packet);
          fprintf(stderr, "%lx,%lx", last_packet_header, this_packet_header);
          fprintf(stderr, "\n\n\n\n\n");


          /* Write zero values to dropped packet regions */
          /* NOTE : We could use unused bits to do this better? */
          for(i=0;i<(CIN_PACKET_LEN * skipped_packets / 2);i++){
            *(frame->data + *ds_map_p) = CIN_DROPPED_PACKET_VAL;
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
        data->mallformed_packets++;
      } 

      last_packet_header = this_packet_header;

      /* Now we are done with the packet, we can advance the fifo */

      pthread_mutex_lock(data->packet_mutex);
      fifo_advance_tail(data->packet_fifo);
      pthread_mutex_unlock(data->packet_mutex);

      /* Now we can set the last packet to this packet */

      last_packet = this_packet;

      /* Check for full image */

      if(byte_count == CIN_FRAME_SIZE){
        /* Advance the frame fifo and signal that a new frame
           is avaliable. Perhaps there is a better way to do this? */
        frame->number = this_frame;
        frame->timestamp = this_frame_timestamp; /* taken from first packet */
        data->last_frame = this_frame;

        pthread_mutex_lock(data->frame_mutex);
        fifo_advance_head(data->frame_fifo);
        pthread_cond_signal(data->frame_signal);
        pthread_mutex_unlock(data->frame_mutex);
      }

    } else {
      /* Buffer is empty - wait for next packet */
      pthread_mutex_lock(data->packet_mutex);
      pthread_cond_wait(data->packet_signal, data->packet_mutex);
      pthread_mutex_unlock(data->packet_mutex);
    }
  }

  pthread_exit(NULL);
}

void *cin_write_thread(cin_thread *data){
  /* FILE *fp; */
  TIFF *fp;
  char filename[256];
  cin_frame_fifo *frame;
  uint16_t *p;
  int j;

  while(1){
    /* Lock mutex and wait for frame */
    /* once frame arrives, get the lail from the fifo */
    /* and save to disk */

    pthread_mutex_lock(data->frame_mutex);
    frame = (cin_frame_fifo*)fifo_get_tail(data->frame_fifo);
    pthread_mutex_unlock(data->frame_mutex);

    if(frame != NULL){

      /*
      sprintf(filename, "frame%08d.bin", frame->number);
    
      fp = fopen(filename, "w");
      if(fp){
        fwrite(frame->data, sizeof(uint16_t), 
               CIN_FRAME_HEIGHT * CIN_FRAME_WIDTH, fp);
        fclose(fp);
      } 
      */

      sprintf(filename, "frame%08d.tif", frame->number);
      fp = TIFFOpen(filename, "w");  

      TIFFSetField(fp, TIFFTAG_IMAGEWIDTH, CIN_FRAME_WIDTH);
      TIFFSetField(fp, TIFFTAG_BITSPERSAMPLE, 16);
      TIFFSetField(fp, TIFFTAG_SAMPLESPERPIXEL, 1);
      TIFFSetField(fp, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
      TIFFSetField(fp, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
      TIFFSetField(fp, TIFFTAG_ORIENTATION, ORIENTATION_BOTLEFT);

      p = frame->data;
      for(j=0;j<CIN_FRAME_HEIGHT;j++){
        TIFFWriteScanline(fp, p, j, 0);
        p += CIN_FRAME_WIDTH;
      }

      TIFFClose(fp);

      //sleep(1);

      /* Now that file is written, advance the fifo */
      pthread_mutex_lock(data->frame_mutex);
      fifo_advance_tail(data->frame_fifo);
      pthread_mutex_unlock(data->frame_mutex);
    } else {
      /* Nothing is in the buffer, wait for a signal */
      pthread_mutex_lock(data->frame_mutex);
      pthread_cond_wait(data->frame_signal, data->frame_mutex);
      pthread_mutex_unlock(data->frame_mutex);
    }
  }

  pthread_exit(NULL);
}

void *cin_monitor_thread(cin_thread *data){
  fprintf(stderr, "\033[?25l");

  while(1){
    fprintf(stderr, "Last frame %d\n", (unsigned int)data->last_frame);

    fprintf(stderr, "Packet buffer is %6.2f %% full. Spool buffer is %6.2f %% full.\n",
            fifo_percent_full(data->packet_fifo),
            fifo_percent_full(data->frame_fifo));
    
    fprintf(stderr, "Framerate = %4.1f s^-1 : Dropped packets %12ld : Mallformed packets %12ld\r",
            data->framerate, data->dropped_packets, data->mallformed_packets);
    fprintf(stderr, "\033[A\033[A"); /* Move up 2 lines */
    sleep(0.75);
  }

  pthread_exit(NULL);
}

void *cin_listen_thread(cin_thread *data){
  cin_packet_fifo *buffer = NULL;

  /* Open the socket for communications */

  if(!net_connect(data->iface)){
    perror("Unable to connect to CIN!");
    pthread_exit(NULL);
  }

  while(1){
    /* Get the next element in the fifo */
    pthread_mutex_lock(data->packet_mutex);
    buffer = (cin_packet_fifo*)fifo_get_head(data->packet_fifo);
    pthread_mutex_unlock(data->packet_mutex);

    buffer->size = net_read(data->iface, buffer->data);
    gettimeofday(&buffer->timestamp, NULL);

    //fprintf(stderr, "Buffer size = %d\n", buffer->size);

    if (buffer->size > 1024){
      pthread_mutex_lock(data->packet_mutex);
      fifo_advance_head(data->packet_fifo);
      pthread_cond_signal(data->packet_signal);
      pthread_mutex_unlock(data->packet_mutex);
    }
    
  }

  pthread_exit(NULL);
}
