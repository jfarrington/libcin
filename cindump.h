#ifndef _CINDUMP_H 
#define _CINDUMP_H 1

typedef struct {
  /* FIFO Elements */
  fifo packet_fifo;  
  fifo frame_fifo;

  /* Interface */
  cin_fabric_iface iface;

  /* Statistics */
  double framerate;

  /* Thread communications */
  pthread_mutex_t packet_mutex;
  pthread_cond_t packet_signal; 
  pthread_mutex_t frame_mutex;
  pthread_cond_t frame_signal;
} cin_thread;

void *cin_listen_thread(cin_thread *data);
void *cin_write_thread(cin_thread *data);
void *cin_monitor_thread(cin_thread *data);
void *cin_assembler_thread(cin_thread *data);

#endif
