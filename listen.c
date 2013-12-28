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

#include "listen.h"

struct sock_filter BPF_CIN_SOURCE_IP_PORT[]= {
  { 0x28, 0, 0, 0x0000000c },
  { 0x15, 0, 16, 0x00000800 },
  { 0x20, 0, 0, 0x0000001a },
  { 0x15, 0, 14, 0x0a0005cf },
  { 0x20, 0, 0, 0x0000001e },
  { 0x15, 0, 12, 0x0a000516 },
  { 0x30, 0, 0, 0x00000017 },
  { 0x15, 2, 0, 0x00000084 },
  { 0x15, 1, 0, 0x00000006 },
  { 0x15, 0, 8, 0x00000011 },
  { 0x28, 0, 0, 0x00000014 },
  { 0x45, 6, 0, 0x00001fff },
  { 0xb1, 0, 0, 0x0000000e },
  { 0x48, 0, 0, 0x0000000e },
  { 0x15, 0, 3, 0x0000c033 },
  { 0x48, 0, 0, 0x00000010 },
  { 0x15, 0, 1, 0x0000c031 },
  { 0x6, 0, 0, 0x0000ffff },
  { 0x6, 0, 0, 0x00000000 },
};

#define BPF_CIN_SOURCE_IP_PORT_LEN 19


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
  iface->recv_buffer = 65535;
  strcpy(iface->iface_name, CIN_IFACE_NAME);
  strcpy(iface->svraddr,    CIN_SVRADDR);
}

int net_set_buffers(cin_fabric_iface *iface){
  /* Set the receieve buffers for the socket */
  unsigned int size;
  unsigned int size_len;

  
  size = 0x3FFFFFFF;
  if(setsockopt(iface->fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size)) == -1){
    perror("Unable to set receive buffer :");
  } else {
    size_len = sizeof(size);
    if(getsockopt(iface->fd, SOL_SOCKET, SO_RCVBUF, &size, &size_len) == -1){
      perror("Unable to get receive buffer :");
    };
    fprintf(stderr,"==== Recv Buffer set to %x bytes\n", size);
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
  //addr.sin_addr.s_addr = htonl(INADDR_ANY);
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
  if ((iface->fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    return FALSE;
  }

  iface->header_len = 0;
  return TRUE;
}

int net_set_packet_filter(cin_fabric_iface *iface){
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
  int r;
  
  r = recvfrom(iface->fd, buffer, CIN_MAX_MTU * sizeof(unsigned char), 0, NULL, NULL);

  return r;
}

