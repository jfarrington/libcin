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

#include "listen.h"
#include "filters.h"

int net_set_promisc(int fd, const char* iface, int val){
  /* Set the network card promiscuos mode based on swicth*/

  struct ifreq ethreq;

  memset(&ethreq, 0, sizeof(ethreq));
  strncpy(ethreq.ifr_name, iface, IFNAMSIZ);
  if (ioctl(fd,SIOCGIFFLAGS,&ethreq) == -1) {
    return FALSE;
  } 

  if(val){
    ethreq.ifr_flags |= IFF_PROMISC;
  } else {
    ethreq.ifr_flags &= ~IFF_PROMISC;
  }

  if (ioctl(fd,SIOCSIFFLAGS,&ethreq) == -1) {
    return FALSE;
  }

  return TRUE;
}

int net_bind_to_interface(int fd, const char* iface){
  struct sockaddr_ll sll;
  struct ifreq ethreq;

  memset(&ethreq, 0, sizeof(ethreq));
  strncpy(ethreq.ifr_name, iface, IFNAMSIZ);

  if (ioctl(fd, SIOCGIFINDEX, &ethreq) == -1) {
    return FALSE;
  }

  sll.sll_family = AF_PACKET; 
  sll.sll_ifindex = ethreq.ifr_ifindex;
  sll.sll_protocol = htons(ETH_P_IP);
  if((bind(fd , (struct sockaddr *)&sll , sizeof(sll))) == -1){
    return FALSE;
  }

  return TRUE;
}

int net_open_socket(int *fd){
  if ((*fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_IP))) < 0) {
    return FALSE;
  }

  return TRUE;
}

int net_set_packet_filter(int fd){
  struct sock_fprog FILTER_CIN_SOURCE_IP_PORT;

  /* Defile filter */
  FILTER_CIN_SOURCE_IP_PORT.len = BPF_CIN_SOURCE_IP_PORT_LEN;
  FILTER_CIN_SOURCE_IP_PORT.filter = BPF_CIN_SOURCE_IP_PORT;

  /* Attach the filter to the socket */
  if(setsockopt(fd, SOL_SOCKET, SO_ATTACH_FILTER,
                &FILTER_CIN_SOURCE_IP_PORT, sizeof(FILTER_CIN_SOURCE_IP_PORT))<0){
    return FALSE;
  }

  return TRUE;
}

