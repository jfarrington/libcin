

uint16_t* dbl_buffer_getbuffer(struct dbl_buffer *buffer){
  /* Get the current buffer pointer and return */
  return buffer->image[current];
}


