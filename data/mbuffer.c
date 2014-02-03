#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "mbuffer.h"
#include "cin.h"

int mbuffer_init(mbuffer_t *buffer, int data_size){

  buffer->size = data_size;

  buffer->data = malloc(CIN_DATA_MBUFFER_SIZE * data_size);
  if(!buffer->data){
    return CIN_DATA_MBUFFER_ERR_MEMORY;
  }

  int i;
  for(i=0;i<CIN_DATA_MBUFFER_SIZE;i++){
    buffer->active[i] = 0;
    buffer->empty[i] = 1;
  }

  buffer->write_buffer = 0;
  buffer->read_buffer = 1;

  pthread_mutex_init(&buffer->mutex, NULL);
  pthread_cond_init(&buffer->signal, NULL);
  
  return CIN_DATA_MBUFFER_ERR_NONE;
}

void* mbuffer_get_write_buffer(mbuffer_t *buffer){
  /* Get the current buffer pointer and return */
  void* p;

  pthread_mutex_lock(&buffer->mutex);
  p = buffer->data + (buffer->size * buffer->write_buffer);
  buffer->active[buffer->write_buffer] = 1;
  pthread_mutex_unlock(&buffer->mutex);

  return p;
}

void mbuffer_write_done(mbuffer_t *buffer){
  /* Signal that we are done writing */
  pthread_mutex_lock(&buffer->mutex);

  // Set the current write buffer to active

  buffer->active[buffer->write_buffer] = 0;
  buffer->empty[buffer->write_buffer] = 0;

  // Now switch the write buffer to the read buffer

  buffer->write_buffer ^= buffer->read_buffer;
  buffer->read_buffer ^= buffer->write_buffer;
  buffer->write_buffer ^= buffer->read_buffer;

  pthread_cond_signal(&buffer->signal);
  pthread_mutex_unlock(&buffer->mutex);
}

void* mbuffer_get_read_buffer(mbuffer_t *buffer){
  /* Get the current read buffer */
  void* rtn;

  pthread_mutex_lock(&buffer->mutex);

  while(buffer->empty[buffer->read_buffer]){
    pthread_cond_wait(&buffer->signal, &buffer->mutex);
  }

  // return a pointer to the read buffer
  rtn = buffer->data + (buffer->read_buffer * buffer->size);

  // Set the buffer to active
  buffer->active[buffer->read_buffer] = 1;


  // If we are writing to the buffer then wait
  while(buffer->active[buffer->write_buffer]){
    pthread_cond_wait(&buffer->signal, &buffer->mutex);
  }

  pthread_mutex_unlock(&buffer->mutex);

  return rtn;
}

void mbuffer_read_done(mbuffer_t *buffer){
  /* Signal we are done with the read buffer */
  pthread_mutex_lock(&buffer->mutex);
  buffer->active[buffer->read_buffer] = 0;
  pthread_mutex_unlock(&buffer->mutex);
}

