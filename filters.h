#ifndef _udp_listener_filters
#define _udp_listener_filters 1

#include <linux/filter.h>

/* CIN FILTER

  (000) ldh      [12]
  (001) jeq      #0x800           jt 2  jf 18
  (002) ld       [26]
  (003) jeq      #0xa0005cf       jt 6  jf 4
  (004) ld       [30]
  (005) jeq      #0xa0005cf       jt 6  jf 18
  (006) ldb      [23]
  (007) jeq      #0x84            jt 10 jf 8
  (008) jeq      #0x6             jt 10 jf 9
  (009) jeq      #0x11            jt 10 jf 18
  (010) ldh      [20]
  (011) jset     #0x1fff          jt 18 jf 12
  (012) ldxb     4*([14]&0xf)
  (013) ldh      [x + 14]
  (014) jeq      #0xc033          jt 17 jf 15
  (015) ldh      [x + 16]
  (016) jeq      #0xc033          jt 17 jf 18
  (017) ret      #65535
  (018) ret      #0

*/

struct sock_filter BPF_CIN_SOURCE_IP_PORT[]= {
  { 0x28, 0, 0, 0x0000000c },
  { 0x15, 15, 0, 0x000086dd },
  { 0x15, 0, 14, 0x00000800 },
  { 0x30, 0, 0, 0x00000017 },
  { 0x15, 0, 12, 0x00000011 },
  { 0x20, 0, 0, 0x0000001a },
  { 0x15, 2, 0, 0x0a0005cf },
  { 0x20, 0, 0, 0x0000001e },
  { 0x15, 0, 8, 0x0a0005cf },
  { 0x28, 0, 0, 0x00000014 },
  { 0x45, 6, 0, 0x00001fff },
  { 0xb1, 0, 0, 0x0000000e },
  { 0x48, 0, 0, 0x0000000e },
  { 0x15, 2, 0, 0x0000c033 },
  { 0x48, 0, 0, 0x00000010 },
  { 0x15, 0, 1, 0x0000c033 },
  { 0x6, 0, 0, 0x0000ffff },
  { 0x6, 0, 0, 0x00000000 }
};

#define BPF_CIN_SOURCE_IP_PORT_LEN 18

#endif
