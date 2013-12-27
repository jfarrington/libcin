#ifndef _CINDUMP_H 
#define _CINDUMP_H 1

typedef struct {
  fifo packet_fifo;  
  fifo frame_fifo;
  int socket_fd;
  pthread_mutex_t mutex;
  pthread_cond_t signal;
} cin_thread;

void *cin_listen_thread(cin_thread *data);
void *cin_write_thread(cin_thread *data);
void *cin_monitor_thread(cin_thread *data);
void *cin_assembler_thread(cin_thread *data);

#endif
