#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

#include "../cin.h"
#include "cin_register_map.h"
#include "cin_api.h"


static int cin_set_sock_timeout(struct cin_port* cp) {

    if(setsockopt(cp->sockfd, SOL_SOCKET, SO_RCVTIMEO,
                    (void*)&cp->tv, sizeof(struct timeval)) < 0) 
    {
        perror("setsockopt(timeout");
        return 1;
    }
    return 0;
}

int cin_init_ctl_port(struct cin_port* cp, char* ipaddr, 
				uint16_t port) 
{
    if(cp->sockfd) {
        perror("CIN control port was already initialized!!");
        return 1;
    }

    if(ipaddr == 0) { cp->srvaddr = CIN_CTL_IP; }
    else { cp->srvaddr = strndup(ipaddr, strlen(ipaddr)); }

    if(port == 0) { cp->srvport = CIN_CTL_PORT; }
    else { cp->srvport = port; }

    cp->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(cp->sockfd < 0) {
        perror("CIN control port - socket() failed !!!");
        return 1;
    }

    int i = 1;
    if(setsockopt(cp->sockfd, SOL_SOCKET, SO_REUSEADDR, \
                    (void *)&i, sizeof i) < 0) {
        perror("CIN control port - setsockopt() failed !!!");
        return 1;
    }


    /* default timeout of 0.1s */
    cp->tv.tv_usec = 100000;
    cin_set_sock_timeout(cp);

    /* initialize CIN (server) and client (us!) sockaddr structs */
    memset(&cp->sin_srv, 0, sizeof(struct sockaddr_in));
    memset(&cp->sin_cli, 0, sizeof(struct sockaddr_in));
    cp->sin_srv.sin_family = AF_INET;
    cp->sin_srv.sin_port = htons(cp->srvport);
    cp->slen = sizeof(struct sockaddr_in);
    if(inet_aton(cp->srvaddr, &cp->sin_srv.sin_addr) == 0) {
        perror("CIN control port - inet_aton() failed!!");
        return 1;
    }
    
    return 0;
}

int cin_shutdown(struct cin_port* cp) {
    if(cp->sockfd) { close(cp->sockfd); }

    return 0;
}

int cin_ctl_write(struct cin_port* cp, uint16_t reg, uint16_t val) {
    uint32_t _val;
    int rc;

    _val = ntohl((uint32_t)(reg << 16 | val));
    rc = sendto(cp->sockfd, &_val, sizeof(_val), 0, \
                    (struct sockaddr*)&cp->sin_srv, sizeof(cp->sin_srv));
    if(rc != sizeof(_val) ) {
        perror("CIN control port - sendto( ) failure!!");
        return 1;
    }

    /*** TODO - implement write verification procedure ***/
    return 0;
}

uint16_t cin_ctl_read(struct cin_port* cp, uint16_t reg) {
    uint32_t buf = 0;
    ssize_t n;

    cin_ctl_write(cp, REG_READ_ADDRESS, reg);
    sleep(0.1);
    cin_ctl_write(cp, REG_COMMAND, CMD_READ_REG);

    /* set timeout to 1 sec */
    cp->tv.tv_sec = 1;
    cp->tv.tv_usec = 0;
    cin_set_sock_timeout(cp);

    n = recvfrom(cp->sockfd, (void*)&buf, sizeof(buf), 0,\
                            (struct sockaddr*)&cp->sin_cli, \
                            (socklen_t*)&cp->slen);
    if(n != sizeof(buf)) {
        if(n == 0)
            perror("CIN has shutdown control port connection");
        else if(n < 0) {
            perror("CIN control port - recvfrom( ) failed!!");
        }
        return 1;
    }

    /* reset socket timeout to 0.1s default */
    cp->tv.tv_sec = 0;
    cp->tv.tv_usec = 100000;
    cin_set_sock_timeout(cp);

    buf = ntohl(buf);
    return (uint16_t)(buf & 0x0000ffff);
}

void cin_power_up(struct cin_port* cp) {
    printf("Powering On CIN Board ........  \n");
    cin_ctl_write(cp, REG_PS_ENABLE, 0x000f);
    cin_ctl_write(cp, REG_COMMAND, CMD_PS_ENABLE);
    cin_ctl_write(cp, REG_PS_ENABLE, 0x001f);
    cin_ctl_write(cp, REG_COMMAND, CMD_PS_ENABLE);
}

void cin_power_down(struct cin_port* cp) {
    printf("Powering Off CIN Board ........ \n");
    cin_ctl_write(cp, REG_PS_ENABLE, 0x0000);
    cin_ctl_write(cp, REG_COMMAND, CMD_PS_ENABLE);
}

static double current_calc(uint16_t val) {
    double _current;
    if(val >= 0x8000) {
        _current = 0.000000476*(0x10000 - val)/0.003;
    }
    else { 
        _current = 0.000000476*val/0.003;
    }

    return _current;
}

static void calcVIStatus(struct cin_port* cp, uint16_t vreg, 
				uint16_t ireg, double vfact, char* desc)
{
    uint16_t _val;
    double _current, _voltage;

    _val = cin_ctl_read(cp, vreg);
    _voltage = vfact*_val;
    _val = cin_ctl_read(cp, ireg);
    _current = current_calc(_val);
    printf("%s %0.2fV @ %0.3fA\n", desc, _voltage, _current);
}

void cin_report_power_status(struct cin_port* cp) {
    printf("****  CIN Power Monitor  ****\n");
    double _current, _voltage;
    uint16_t _val = cin_ctl_read(cp, REG_PS_ENABLE);

    if(_val & 0x0001) {
        /* ADC == LT4151 */
        _val = cin_ctl_read(cp, REG_VMON_ADC1_CH1);
        _voltage = 0.025*_val;
        _val = cin_ctl_read(cp, REG_IMON_ADC1_CH0);
        _current = 0.00002*_val/0.003;
        printf("V12P_BUS Power  : %0.2fV @ %0.2fA\n\n", _voltage, _current);

        /* ADC == LT2418 */
        calcVIStatus(cp, REG_VMON_ADC0_CH5, REG_IMON_ADC0_CH5,
                0.00015258, "V3P3_MGMT Power  :");
        calcVIStatus(cp, REG_VMON_ADC0_CH7, REG_IMON_ADC0_CH7,
                0.00015258, "V2P5_MGMT Power  :");
        calcVIStatus(cp, REG_VMON_ADC0_CH2, REG_IMON_ADC0_CH2,
                0.00007629, "V1P2_MGMT Power  :");
        calcVIStatus(cp, REG_VMON_ADC0_CH3, REG_IMON_ADC0_CH3,
                0.00007629, "V1P0_ENET Power  :");
        printf("\n");
        calcVIStatus(cp, REG_VMON_ADC0_CH4, REG_IMON_ADC0_CH4,
                0.00015258, "V3P3_S3E Power   :");
        calcVIStatus(cp, REG_VMON_ADC0_CH8, REG_IMON_ADC0_CH8,
                0.00015258, "V3P3_GEN Power   :");
        calcVIStatus(cp, REG_VMON_ADC0_CH9, REG_IMON_ADC0_CH9,
                0.00015258, "V2P5_GEN Power   :");
        printf("\n");
        calcVIStatus(cp, REG_VMON_ADC0_CHE, REG_IMON_ADC0_CHE,
                0.00007629, "V0P9_V6 Power    :");
        calcVIStatus(cp, REG_VMON_ADC0_CHB, REG_IMON_ADC0_CHB,
                0.00007629, "V1P0_V6 Power    :");
        calcVIStatus(cp, REG_VMON_ADC0_CHD, REG_IMON_ADC0_CHD,
                0.00015258, "V2P5_V6 Power    :");
        printf("\n");
        calcVIStatus(cp, REG_VMON_ADC0_CHF, REG_IMON_ADC0_CHF,
                0.00030516, "V_FP Power       :");
    }
    else {
        printf("  12V Power Supply is OFF\n");
    }
}
