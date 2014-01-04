#ifndef __CIN_H__
#define __CIN_H__

#include <stdint.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>

#define CIN_CTL_IP                   "192.168.1.207"
#define CIN_CTL_PORT                 49200

#define CIN_DATA_IP                  "10.0.5.207"
#define CIN_DATA_PORT                49201
#define CIN_DATA_CTL_PORT            49203
#define CIN_DATA_MAX_MTU             9000
#define CIN_DATA_UDP_PACKET_HEADER   48
#define CIN_DATA_UDP_HEADER          8
#define CIN_DATA_MAGIC_PACKET        0x0000F4F3F2F1F000
#define CIN_DATA_MAGIC_PACKET_MASK   0x0000FFFFFFFFFF00
#define CIN_DATA_PACKET_LEN          8184
#define CIN_DATA_FRAME_HEIGHT        964
#define CIN_DATA_FRAME_WIDTH         1152
#define CIN_DATA_FRAME_SIZE          2220744
#define CIN_DATA_DROPPED_PACKET_VAL  0x0
#define CIN_DATA_RCVBUF              0x200000

struct cin_port {
    char *srvaddr;
    char *cliaddr;
    uint16_t srvport;
    uint16_t cliport;
    int sockfd;
    struct timeval tv;
    struct sockaddr_in sin_srv; /* server info */
    struct sockaddr_in sin_cli; /* client info (us!) */
    socklen_t slen; /* for recvfrom() */
    unsigned int rcvbuf; /* For setting data recieve buffer */
};

struct cin_data_frame {
  uint16_t *data;
  uint16_t number;
  struct timeval timestamp;
};


/* prototypes */
int cin_init_ctl_port(struct cin_port* cp, char* ipaddr, uint16_t port);
uint16_t cin_ctl_read(struct cin_port* cp, uint16_t reg);
int cin_ctl_write(struct cin_port* cp, uint16_t reg, uint16_t val);
int cin_shutdown(struct cin_port* cp);

void cin_power_up();
void cin_power_down();
void cin_report_power_status();


/* cindata prototypes */

int cin_init_data_port(struct cin_port* dp,
                       char* ipaddr, uint16_t port,
                       char* cin_ipaddr, uint16_t cin_port,
                       unsigned int rcvbuf);
int cin_data_read(struct cin_port* dp, unsigned char* buffer);
int cin_data_write(struct cin_port* dp, unsigned char* buffer, int buffer_len);

int cin_data_init(void);
void cin_data_wait_for_threads(void);
int cin_data_stop_threads(void);

struct cin_data_frame* cin_data_get_next_frame(void);
void cin_data_release_frame(void);

#endif
