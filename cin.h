#ifndef __CIN_H__
#define __CIN_H__

#include <stdint.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>

#define CIN_CTL_IP      "192.168.1.207"
#define CIN_CTL_PORT    49200


struct cin_port {
    char *srvaddr;
    uint16_t srvport;
    int sockfd;
    struct timeval tv;
    struct sockaddr_in sin_srv; /* server info */
    struct sockaddr_in sin_cli; /* client info (us!) */
    socklen_t slen; /* for recvfrom() */
};

/* prototypes */
int cin_init_ctl_port(struct cin_port* cp, char* ipaddr, uint16_t port);
uint16_t cin_ctl_read(struct cin_port* cp, uint16_t reg);
int cin_ctl_write(struct cin_port* cp, uint16_t reg, uint16_t val);
int cin_shutdown(struct cin_port* cp);

void cin_power_up();
void cin_power_down();
void cin_report_power_status();

#endif
