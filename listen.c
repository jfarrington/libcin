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

#include "listen.h"
#include "filters.h"

void net_set_default(cin_fabric_iface *iface){
  iface->fd = -1;
  iface->svrport = CIN_SVRPORT;
  strcpy(iface->iface_name, CIN_IFACE_NAME);
  strcpy(iface->svraddr,    CIN_SVRADDR);
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
  addr.sin_port = htons(iface->srvport);

  if(bind(iface->fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    return FALSE;
  }

  return TRUE;
}

int net_open_socket_udp(cin_fabric_iface *iface){
  if ((iface->fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_IP))) < 0) {
    return FALSE;
  }

  return TRUE;

}

int net_open_socket_bfp(cin_fabric_iface *iface){
  if ((iface->fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    return FALSE;
  }

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

