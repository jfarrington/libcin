#ifndef _DESCRAMBLE_H
#define _DESCRAMBLE_H 1

#ifdef __cplusplus
extern "C" {
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

int descramble_image(uint16_t *image_out, uint16_t *image_in,
                     uint64_t image_size_pixels, uint64_t image_height, uint64_t image_width);

#ifdef __cplusplus
}
#endif

#endif
