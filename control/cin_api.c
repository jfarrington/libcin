#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "cin.h"
#include "cin_register_map.h"
#include "cin_api.h"

#define INFO(x) fprintf(stdout, (x) )

// Cache TriggerMode
static int m_TriggerMode = 0;

/**************************** UDP Socket ******************************/
static int cin_set_sock_timeout(struct cin_port* cp) {

   if (setsockopt(cp->sockfd, SOL_SOCKET, SO_RCVTIMEO,
      (void*)&cp->tv, sizeof(struct timeval)) < 0){
      perror("setsockopt(timeout)");
      return (-1);
   }
   return 0;
}

int cin_init_ctl_port(struct cin_port* cp, char* ipaddr, 
            uint16_t port) 
{

   if (cp == NULL)
   {
      perror("Parameter cp is NULL!");
      return (-1);
   }
   if(cp->sockfd) {
      perror("CIN control port was already initialized!!");
      // Does this need a return (-1) ?
   }

   if (ipaddr == 0) 
   { 
      cp->srvaddr = CIN_CTL_IP; 
   }
   else 
   { 
      cp->srvaddr = strndup(ipaddr, strlen(ipaddr)); 
   }
   
   if (port == 0) 
   { 
      cp->srvport = CIN_CTL_PORT; 
   }
   else 
   { 
      cp->srvport = port; 
   }

   cp->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
   if (cp->sockfd < 0) 
   {
      perror("CIN control port - socket() failed !!!");
      // Should this return (-1) ?
   }

   int i = 1;
   if (setsockopt(cp->sockfd, SOL_SOCKET, SO_REUSEADDR, 
                    (void *)&i, sizeof i) < 0) 
   {
      perror("CIN control port - setsockopt() failed !!!");
      // Should this return (-1) ?
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
   if(inet_aton(cp->srvaddr, &cp->sin_srv.sin_addr) == 0) 
   {
      perror("CIN control port - inet_aton() failed!!");
      // Should this return (-1) ?
   }
   return 0;
}

int cin_close_ctl_port(struct cin_port* cp) {

   if (cp->sockfd) 
   { 
      close(cp->sockfd); 
   }
   return 0;
}

/*************************** CIN Read/Write ***************************/

int cin_ctl_write(struct cin_port* cp, uint16_t reg, uint16_t val){

   uint32_t _valwr;
   int rc;

   if (cp == NULL)
   {
      perror("Parameter cp is NULL!");
      goto error;
   }
   
   _valwr = ntohl((uint32_t)(reg << 16 | val));
   rc = sendto(cp->sockfd, &_valwr, sizeof(_valwr), 0,
         (struct sockaddr*)&cp->sin_srv,
         sizeof(cp->sin_srv));
   if (rc != sizeof(_valwr) ) {
      perror("CIN control port - sendto( ) failure!!");
      goto error;
   }

   /*** TODO - Verify write verification procedure ***/
   return 0;
   
error:  
   perror("Write error control port");
   return (-1);
}

int cin_stream_write(struct cin_port* cp, char *val,int size) {
 
   int rc;
    
   if (cp == NULL)
   {
      perror("Parameter cp is NULL!");
      goto error;
   }
   
   rc = sendto(cp->sockfd, val, size, 0,
               (struct sockaddr*)&cp->sin_srv,
               sizeof(cp->sin_srv));
   if (rc != size ) {
      perror(" CIN control port - sendto( ) failure!!");
      goto error;
   }

   /*** TODO - implement write verification procedure ***/
   return 0;  
   
error:   
   perror("Write error to control data port");
   return -1;
}                                      

uint16_t cin_ctl_read(struct cin_port* cp, uint16_t reg) {
    
   int _status;
   uint32_t buf = 0;
   ssize_t n;

   if (cp == NULL)
   {
      perror("Parameter cp is NULL!");
      goto error;
   }
   
   _status=cin_ctl_write(cp, REG_READ_ADDRESS, reg);
   if (_status != 0)
   {
      goto error;
   }
   sleep(0.1); // YF Hard coded sleep - how to best handle this?

   _status=cin_ctl_write(cp, REG_COMMAND, CMD_READ_REG);
   if (_status != 0)
   {
      goto error;
   }

   /* set timeout to 1 sec */
   cp->tv.tv_sec = 1;
   cp->tv.tv_usec = 0;
   cin_set_sock_timeout(cp);

   n = recvfrom(cp->sockfd, (void*)&buf, sizeof(buf), 0,
         (struct sockaddr*)&cp->sin_cli, 
         (socklen_t*)&cp->slen);
   if (n != sizeof(buf)) 
   {
      if (n == 0)
      {
         perror(" CIN has shutdown control port connection");
      }
      else if (n < 0) 
      {
         perror(" CIN control port - recvfrom( ) failed!!");
      }
      else 
      {
         perror(" CIN control port - !!");
      }  
      return (-1);
   }

   /* reset socket timeout to 0.1s default */
   cp->tv.tv_sec = 0;
   cp->tv.tv_usec = 100000;
   cin_set_sock_timeout(cp);
   buf = ntohl(buf);

   return (uint16_t)(buf);
    
error:  
   perror("Read error");
   return (-1);
}

 
/******************* CIN PowerUP/PowerDown *************************/
int cin_on(struct cin_port* cp){

   int _status;

   INFO("  Powering ON CIN Board ........\n");

   _status=cin_ctl_write(cp,REG_PS_ENABLE, 0x000f);
   if (_status != 0)
   {
      goto error;
   }
   _status=cin_ctl_write(cp,REG_COMMAND, CMD_PS_ENABLE);
   if (_status != 0)
   {
      goto error;
   }
   return _status;
   
error:
   return _status;
}

int cin_off(struct cin_port* cp) {

   int _status;
     
   INFO("  Powering OFF CIN Board ........\n");
   _status=cin_ctl_write(cp,REG_PS_ENABLE, 0x0000);
   if (_status != 0)
      {goto error;}
   _status=cin_ctl_write(cp,REG_COMMAND, CMD_PS_ENABLE);
   if (_status != 0)
      {goto error;}

   return _status;
   
error:
   return _status;
     
}

int cin_fp_on(struct cin_port* cp){ 

   int _status;
   
   INFO("  Powering ON CIN Front Panel Boards ........\n");
   _status=cin_ctl_write(cp,REG_PS_ENABLE, 0x003f);
   if (_status != 0)
      {goto error;}
   _status=cin_ctl_write(cp,REG_COMMAND, CMD_PS_ENABLE);
   if (_status != 0)
      {goto error;}

   return _status;
   
error:
   return _status;
}

int cin_fp_off(struct cin_port* cp){

   int _status;
   
   INFO("  Powering OFF CIN Front Panel Boards ........\n");
   _status=cin_ctl_write(cp,REG_PS_ENABLE, 0x001f);
   if (_status != 0)
      {goto error;}
   _status=cin_ctl_write(cp,REG_COMMAND, CMD_PS_ENABLE);
   if (_status != 0)
      {goto error;}

   return _status;
   
error:
   return _status;
}

/******************* CIN Configuration/Status *************************/

int cin_load_config(struct cin_port* cp,char *filename){

   int _status;
   uint32_t _regul,_valul;
   char _regstr[12],_valstr[12],_line [1024];

   FILE *file = fopen ( filename, "r" );
   if (file != NULL) {  
      fprintf(stdout,"  Loading CIN configuration %s ........\n",filename);

      /* Read a line an filter out comments */     
      while(fgets(_line,sizeof _line,file)!= NULL){ 
         _line[strlen(_line)-1]='\0';   

         if ('#' == _line[0] || '\0' == _line[0]){
            //fprintf(stdout," Ignore line\n"); //DEBUG 
         }  
         else {
            sscanf (_line,"%s %s",_regstr,_valstr);
            _regul=strtoul(_regstr,NULL,16);
            _valul=strtoul(_valstr,NULL,16);          
            usleep(10000);   /*for flow control*/ 
            _status=cin_ctl_write(cp,_regul,_valul);
            if (_status != 0)
               {goto error;}     
         }   
      }     
      INFO("  CIN configuration loaded!\n");
      fclose(file);
   }
   else {
      perror(filename);
   } 
   return 0;
  
error:
   return -1;
}

int cin_load_firmware(struct cin_port* cp,struct cin_port* dcp,char *filename){
   
   uint32_t num_e;
   int _status; 
   char buffer[128];     
   
   FILE *file= fopen(filename, "rb");

   if (file != NULL) {              
               
      fprintf(stdout,"  Loading CIN FPGA firmware %s ........\n",filename);

      _status=cin_ctl_write(cp,REG_COMMAND,CMD_PROGRAM_FRAME); 
      if (_status != 0){goto error;}   
      sleep(1);
      
      /*Read file and send in 128 Byte chunks*/ 
      num_e=fread(buffer,1, sizeof(buffer), file);          
      while (num_e != 0 ){    
         _status=cin_stream_write(dcp, buffer,num_e);       
         if (_status != 0){goto error;}
         usleep(500);   /*for UDP flow control*/ 
         num_e=fread(buffer,1, sizeof(buffer), file);                
      }   
   }
   else {
      _status =  (-1);
      perror(filename);
      goto error; 
   }
   sleep(1);
   _status=cin_ctl_write(cp,REG_FRM_RESET,0x0001);
   if (_status != 0)
      {goto error;} 
   _status=cin_ctl_write(cp,REG_FRM_RESET,0x0000);
   if (_status != 0)
      {goto error;} 
      
   INFO("  CIN FPGA configuration loaded!!\n"); 
   fclose(file);
   return _status;
   
error:
   return _status;
}

static int dco_freeze (struct cin_port* cp){
   
   int _status;
   
   _status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xB089);
   if (_status != 0)
      {goto error;}

   _status=cin_ctl_write(cp,REG_FCLK_I2C_DATA_WR, 0xF010);
   if (_status != 0)
      {goto error;}

   _status=cin_ctl_write(cp,REG_FRM_COMMAND, CMD_FCLK_COMMIT);
   if (_status != 0)
      {goto error;}
   
   INFO("  Freeze Si570 DCO\n");
   return _status;

error:
   return _status;
}


static int dco_unfreeze (struct cin_port* cp){

   int _status;

   _status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xB089);
   if (_status != 0)
      {goto error;}

   _status=cin_ctl_write(cp,REG_FCLK_I2C_DATA_WR, 0xF000);
   if (_status != 0)
      {goto error;}

   _status=cin_ctl_write(cp,REG_FRM_COMMAND, CMD_FCLK_COMMIT);
   if (_status != 0)
      {goto error;}

   _status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xB087);
   if (_status != 0)
      {goto error;}

   _status=cin_ctl_write(cp,REG_FCLK_I2C_DATA_WR, 0xF040);
   if (_status != 0)
      {goto error;}

   _status=cin_ctl_write(cp,REG_FRM_COMMAND, CMD_FCLK_COMMIT);
   if (_status != 0)
      {goto error;}

   INFO("  UnFreeze Si570 DCO & Start Oscillator\n");   
   return _status;

error:
   return _status;
}

static int fclk_write(struct cin_port* cp, uint16_t reg,uint16_t val){

   int _status;
   
   _status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, reg);
   if (_status != 0)
      {goto error;}

   _status=cin_ctl_write(cp,REG_FCLK_I2C_DATA_WR, val);
   if (_status != 0)
      {goto error;}

   _status=cin_ctl_write(cp,REG_FRM_COMMAND, CMD_FCLK_COMMIT);
   if (_status != 0)
      {goto error;}
   
   return _status;

error:
   return _status;
}

/*TODO:-Check that clock is properly set*/
// YF hardcoded addresses :-(
int cin_set_fclk(struct cin_port* cp,uint16_t clkfreq){
   int _status;

   fprintf(stdout,"\n****Set CIN FCLK to %uMHz****\n",clkfreq);

   if (clkfreq == 125){
      _status=dco_freeze(cp);
      if (_status != 0)
         {goto error;} 
      
      _status=fclk_write(cp,0xB007,0xF002);
      if (_status != 0)
         {goto error;} 
      
      _status=fclk_write(cp,0xB008,0xF042);
      if (_status != 0)
         {goto error;} 
      
      _status=fclk_write(cp,0xB009,0xF0BC);
      if (_status != 0)
         {goto error;} 
      
      _status=fclk_write(cp,0xB00A,0xF019);
      if (_status != 0)
         {goto error;} 
      
      _status=fclk_write(cp,0xB00B,0xF06D);
      if (_status != 0)
         {goto error;} 
      
      _status=fclk_write(cp,0xB00C,0xF08F);
      if (_status != 0)
         {goto error;} 
      
      _status=dco_unfreeze(cp);
      if (_status != 0)
         {goto error;} 
   }

   else if (clkfreq == 180){

      _status=dco_freeze(cp);
      if (_status != 0)
         {goto error;}

      _status=fclk_write(cp,0xB007,0xF060);
      if (_status != 0)
         {goto error;}

      _status=fclk_write(cp,0xB008,0xF0C2);
      if (_status != 0)
         {goto error;}

      _status=fclk_write(cp,0xB009,0xF0C1);
      if (_status != 0)
         {goto error;}

      _status=fclk_write(cp,0xB00A,0xF0B9);
      if (_status != 0)
         {goto error;}

      _status=fclk_write(cp,0xB00B,0xF08A);
      if (_status != 0)
         {goto error;}

      _status=fclk_write(cp,0xB00C,0xF0EF);
      if (_status != 0)
         {goto error;}

      _status=dco_unfreeze(cp);
      if (_status != 0)
         {goto error;}
   }

   else if (clkfreq == 200){

      _status=dco_freeze(cp);
      if (_status != 0)
         {goto error;}

      _status=fclk_write(cp,0xB007,0xF060);
      if (_status != 0)
         {goto error;}

      _status=fclk_write(cp,0xB008,0xF0C3);
      if (_status != 0)
         {goto error;}

      _status=fclk_write(cp,0xB009,0xF010);
      if (_status != 0)
         {goto error;}

      _status=fclk_write(cp,0xB00A,0xF023);
      if (_status != 0)
         {goto error;}

      _status=fclk_write(cp,0xB00B,0xF07D);
      if (_status != 0)
         {goto error;}

      _status=fclk_write(cp,0xB00C,0xF0ED);
      if (_status != 0)
         {goto error;}

      _status=dco_unfreeze(cp);
      if (_status != 0)
         {goto error;}
   }
   
   else if (clkfreq == 250){

      _status=dco_freeze(cp);
      if (_status != 0)
         {goto error;}

      _status=fclk_write(cp,0xB007,0xF020);
      if (_status != 0)
         {goto error;}

      _status=fclk_write(cp,0xB008,0xF0C2);
      if (_status != 0)
         {goto error;}

      _status=fclk_write(cp,0xB009,0xF0BC);
      if (_status != 0)
         {goto error;}

      _status=fclk_write(cp,0xB00A,0xF019);
      if (_status != 0)
         {goto error;}

      _status=fclk_write(cp,0xB00B,0xF06D);
      if (_status != 0)
         {goto error;}
      
      _status=fclk_write(cp,0xB00C,0xF08F);
      if (_status != 0)
         {goto error;}

      _status=dco_unfreeze(cp);
      if (_status != 0)
         {goto error;}
   }
      
   else{
      perror("  Invalid FCLK Frequency!");
      perror("  Currently only 125MHz, 180MHz, 200MHz and 250MHz are supported\n");
      _status = (-1);
      goto error;      
   }
   return _status;
   
error:
   return  _status;
}

/*TODO:-Verify*/
int cin_get_fclk_status(struct cin_port* cp){ 

   uint16_t _val1,_val2;

   _val1= cin_ctl_read(cp,0xB007);
   _val2= cin_ctl_read(cp,0xB00C);
   
   INFO("\n****  CIN FCLK Configuration  ****\n\n"); 

   if((_val1 == 0xFFFF) || (_val2 == 0xFFFF)){
      perror("  ERROR:No Input\n\n");
      return -1;   
   }
   else if(((_val1 & 0xF002)==0xF002) && ((_val2 & 0xF08F)==0xF08F)){
      INFO("  FCLK Frequency = 125 MHz\n\n");
      return 0;    
   }
   else if(((_val1 & 0xF060)==0xF060) && ((_val2 & 0xF0EF)==0xF0EF)){
      INFO("  FCLK Frequency = 180 MHz\n\n");
      return 0;    
   }
   else if(((_val1 & 0xF060)==0xF060) && ((_val2 & 0xF0ED)==0xF0ED)){
      INFO("  FCLK Frequency = 200 MHz\n\n");
      return 0;    
   }  
   else if(((_val1 & 0xF060)==0xF060) && ((_val2 & 0xF08F)==0xF08F)){
      INFO("  FCLK Frequency = 250 MHz\n\n");
      return 0;    
   }
   else if((_val1 == 0xFFFF) || (_val2 == 0xFFFF)){
      INFO("  FCLK Frequency = 250 MHz\n\n");
      return 0;    
   }
   else{
      perror("  Unknown FCLK Frequency\n\n"); 
      return -1;
   }  
}
      
int cin_get_cfg_fpga_status(struct cin_port* cp){
      
   uint16_t _val;
   
   INFO("\n****  CFG FPGA Status Registers  ****\n\n"); 
   //# get Status Registers
   _val= cin_ctl_read(cp,REG_BOARD_ID);
   fprintf(stdout,"  CIN Board ID     :  %04X\n",_val);

   _val= cin_ctl_read(cp,REG_HW_SERIAL_NUM);
   fprintf(stdout,"  HW Serial Number :  %04X\n",_val);

   _val= cin_ctl_read(cp,REG_FPGA_VERSION);
   fprintf(stdout,"  CFG FPGA Version :  %04X\n\n",_val);

   _val= cin_ctl_read(cp,REG_FPGA_STATUS);
   fprintf(stdout,"  CFG FPGA Status  :  %04X\n",_val);
   /*
      # FPGA Status
      # 15 == FRM DONE
      # 14 == NOT FRM BUSY
      # 13 == NOT FRM INIT B
      # 12 >> 4 == 0
      # 3 >>0 == FP Config Control 3 == PS Interlock
   */
   _val= cin_ctl_read(cp,REG_FPGA_STATUS);   

   if(_val == 0xFFFF){
      perror("\n  ERROR:No Input\n\n");
      return -1;   
   }
   else if((_val & 0x8000)==0x8000){
      INFO("  ** Frame FPGA Configuration Done\n"); 
      return 0;
   }
   else{
      perror("  ** Frame FPGA NOT Configured\n");
      return -1;
   }
   if((_val & 0x0008)==0x0008){
      INFO("  ** FP Power Supply Unlocked\n"); 
   }
   else{
      INFO("  ** FP Power Supply Locked Off\n");
   }

   _val= cin_ctl_read(cp,REG_DCM_STATUS);
   fprintf(stdout,"\n  CFG DCM Status   :  %04X\n",_val);
   /*
   # DCM Status
   # 15 == 0
   # 14 >> 8 == CONF SW
   # 7 == ATCA 48V Alarm
   # 6 == tx2 src ready
   # 5 == tx1 src ready
   # 4 == DCM STATUS2
   # 3 == DCM STATUS1
   # 2 == DCM STATUS0
   # 1 == DCM PSDONE
   # 0 == DCM LOCKED
   */
   _val= cin_ctl_read(cp,REG_DCM_STATUS);
   if((_val & 0x0080)==0x0080){
      INFO("  ** ATCA 48V Alarm\n"); 
   }
   else{
      INFO("  ** ATCA 48V OK\n");
   }
   if((_val & 0x0001)==0x0001){
      INFO("  ** CFG Clock DCM Locked\n"); 
   }
   else{
      INFO("  ** CFG Clock DCM NOT Locked\n");
   }
   if((_val != 0x0800)==0x0000){
      INFO("  ** FP Power Supply Interlock Overide Enabled\n");  
   }
}

// YF hard coded constants :-(
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
   fprintf(stdout,"%s %0.2fV @ %0.3fA\n", desc, _voltage, _current);
}

int cin_get_power_status(struct cin_port* cp) {
    
   double _current, _voltage;
   uint16_t _val = cin_ctl_read(cp, REG_PS_ENABLE);
      
   INFO("****  CIN Power Monitor  ****\n\n");
   if(_val & 0x0001) {
      /* ADC == LT4151 */
      _val = cin_ctl_read(cp, REG_VMON_ADC1_CH1);
      _voltage = 0.025*_val;
      _val = cin_ctl_read(cp, REG_IMON_ADC1_CH0);
      _current = 0.00002*_val/0.003;
      fprintf(stdout,"  V12P_BUS Power  : %0.2fV @ %0.2fA\n\n", _voltage, _current);

      /* ADC == LT2418 */
      calcVIStatus(cp, REG_VMON_ADC0_CH5, REG_IMON_ADC0_CH5,
                0.00015258, "  V3P3_MGMT Power  :");
      calcVIStatus(cp, REG_VMON_ADC0_CH7, REG_IMON_ADC0_CH7,
                0.00015258, "  V2P5_MGMT Power  :");
      calcVIStatus(cp, REG_VMON_ADC0_CH2, REG_IMON_ADC0_CH2,
                0.00007629, "  V1P2_MGMT Power  :");
      calcVIStatus(cp, REG_VMON_ADC0_CH3, REG_IMON_ADC0_CH3,
                0.00007629, "  V1P0_ENET Power  :");
      fprintf(stdout,"\n");
      calcVIStatus(cp, REG_VMON_ADC0_CH4, REG_IMON_ADC0_CH4,
                0.00015258, "  V3P3_S3E Power   :");
      calcVIStatus(cp, REG_VMON_ADC0_CH8, REG_IMON_ADC0_CH8,
                0.00015258, "  V3P3_GEN Power   :");
      calcVIStatus(cp, REG_VMON_ADC0_CH9, REG_IMON_ADC0_CH9,
                0.00015258, "  V2P5_GEN Power   :");
      fprintf(stdout,"\n");
      calcVIStatus(cp, REG_VMON_ADC0_CHE, REG_IMON_ADC0_CHE,
                0.00007629, "  V0P9_V6 Power    :");
      calcVIStatus(cp, REG_VMON_ADC0_CHB, REG_IMON_ADC0_CHB,
                0.00007629, "  V1P0_V6 Power    :");
      calcVIStatus(cp, REG_VMON_ADC0_CHD, REG_IMON_ADC0_CHD,
                0.00015258, "  V2P5_V6 Power    :");
      fprintf(stdout,"\n");
      calcVIStatus(cp, REG_VMON_ADC0_CHF, REG_IMON_ADC0_CHF,
                0.00030516, "  V_FP Power       :");
      fprintf(stdout,"\n");
   }
   else {
      fprintf(stdout,"  12V Power Supply is OFF\n");
   }
   return 0;   
}

/******************* CIN Control *************************/
int cin_set_bias(struct cin_port* cp,int val){

   int _status;
   
   if (val==1){
      _status=cin_ctl_write(cp,REG_BIASCONFIGREGISTER0_REG, 0x0001);
      if (_status != 0)
         {goto error;}
      INFO("  Bias ON\n");
   }
   else if (val==0){
      _status=cin_ctl_write(cp,REG_BIASCONFIGREGISTER0_REG, 0x0000);
      if (_status != 0)
         {goto error;}
      INFO("  Bias OFF\n");
   }
   else{
      perror("Illegal Bias state: Only 0 or 1 allowed\n");
      _status= -1;
      goto error;
   }
   return _status;
   
error:
   return _status;
}

int cin_set_clocks(struct cin_port* cp,int val){

   int _status;   
   
   if (val==1){
      _status=cin_ctl_write(cp,REG_CLOCKCONFIGREGISTER0_REG, 0x0001);
      if (_status != 0)
         {goto error;}
      INFO("  Clocks ON\n");
   }
   else if (val==0){
      _status=cin_ctl_write(cp,REG_CLOCKCONFIGREGISTER0_REG, 0x0000);
      if (_status != 0)
         {goto error;}
      INFO("  Clocks OFF\n");
   }
   else{
      perror("  Illegal Clocks state: Only 0 or 1 allowed\n");
      _status= -1;
      goto error;
   }
   return _status;
   
error:
   return _status;
}

int cin_set_trigger(struct cin_port* cp,int val){

   int _status;
   
   if (val==0){
      _status=cin_ctl_write(cp,REG_TRIGGERMASK_REG, 0x0000);
      if (_status != 0)
         {goto error;}
      INFO("  Trigger set to Internal\n");
   }
   else if (val==1){
      _status=cin_ctl_write(cp,REG_TRIGGERMASK_REG, 0x0001);
      if (_status != 0)
         {goto error;}
      INFO("  Trigger set to External 1\n");
   }
   else if (val==2){
      _status=cin_ctl_write(cp,REG_TRIGGERMASK_REG, 0x0002);
      if (_status != 0)
         {goto error;}
      INFO("  Trigger set to External 2\n");
   }
   else if (val==3){
      _status=cin_ctl_write(cp,REG_TRIGGERMASK_REG, 0x0003);
      if (_status != 0)
         {goto error;}
      INFO("  Trigger set to External (1 or 2)\n");
   }
   else{
      perror("  Illegal Trigger state: Only values 0 to 3 allowed\n");
      _status= -1;
      goto error;
   }
   return _status;

error:
   return _status;
}


uint16_t cin_get_trigger_status (struct cin_port* cp){

   uint16_t _val,_state;
   
   _val =cin_ctl_read(cp,REG_TRIGGERMASK_REG); 
   
   if (_val == 0x0000){
      _state=0;
      INFO("  Trigger status is Internal\n");
   }
   else if (_val == 0x0001){
      _state=1;
      INFO("  Trigger status is External 1\n");
   }
   else if (_val == 0x0002){
      _state=2;   
      INFO("  Trigger status is External 2\n");
   }
   else if (_val == 0x0003){
      _state=3;
      INFO("  Trigger status is External (1 or 2)\n");
   }
   else{
      perror("  Unknown Trigger status\n");
      goto error; 
   }

   return _state;

error:
   return (-1);
}

static int clearFocusBit(struct cin_port* cp){

   uint16_t val1, _status;
   
   val1 =  cin_ctl_read(cp,REG_CLOCKCONFIGREGISTER0_REG);
   _status = cin_ctl_write(cp,REG_CLOCKCONFIGREGISTER0_REG, val1 & 0xFFFD);
   return _status;
}


static int setFocusBit(struct cin_port* cp){

   uint16_t val1, _status;

   val1 =  cin_ctl_read(cp,REG_CLOCKCONFIGREGISTER0_REG);
   _status = cin_ctl_write(cp,REG_CLOCKCONFIGREGISTER0_REG, val1 | 0x0002);
   return _status;
}

//
// val 0 = Single Trigger mode
//     1 = Continuous Trigger Mode
int cin_set_trigger_mode(struct cin_port* cp,int val){

   int _status;

   if (val == 0)
   {
      m_TriggerMode = 0;   // 0 = Single
      // From setTriggerModeSingle.py
      // Clear the Focus bit
      _status = clearFocusBit(cp);
      if (_status != 0) 
         {goto error;}

      _status=cin_ctl_write(cp,REG_NUMBEROFEXPOSURE_REG, 0x0001);
      if (_status != 0) 
         {goto error;}
   }

   else
   {    
      m_TriggerMode = 1;   // 1 = Continuous
      // From setContTriggers.py
      _status = clearFocusBit(cp);
      if (_status != 0) 
         {goto error;}
                                                                                          
      _status=cin_ctl_write(cp,REG_NUMBEROFEXPOSURE_REG, 0x0000);
      if (_status != 0) 
         {goto error;}  

      // YF Do not set Focus Bit here.  Set when call cin_trigger_start().
     // _status = setFocusBit(cp);
     // if (_status != 0) 
     //    {goto error;}
   }     
   return (0); 
   
   error:
      perror("Write error: cin_set_trigger_mode\n");
      return (-1);
}        


// Requires m_TriggerMode to be defined.
// Must call cin_set_trigger_mode before calling this function.   
int cin_trigger_start(struct cin_port* cp)
{
   int _status;
   
   if (m_TriggerMode == 0)    // Single Mode
   {
      _status=cin_ctl_write(cp,REG_FRM_COMMAND, 0x0100);
   }
   
   // Continuous Mode
   else
   {
      _status = setFocusBit(cp);
   }
   return _status;
 }     
      

//
// Stop the triggers
//      
int cin_trigger_stop(struct cin_port* cp)
{
   int _status;
   _status = clearFocusBit(cp);
   return _status;
}



//TODO:Malformed packet when MSB=0x0000*/
int cin_set_exposure_time(struct cin_port* cp,float ftime){  

   int _status;
   uint32_t _time, _msbval,_lsbval;
   float _fraction;

   fprintf(stdout,"  Exposure Time :%f ms\n", ftime);   //DEBUG
   ftime=ftime*100;
   _time=(uint32_t)ftime;  //Extract integer from decimal
   _fraction=ftime-_time;  //Extract fraction from decimal
   
   if(_fraction!=0){ //Check that there is no fractional value
      perror("ERROR:Smallest precision that can be specified is .01 ms\n");
      _status= -1;
   }
   else{ 
      //fprintf(stdout,"Hex value    :0x%08x\n",_time);     
      _msbval=(uint32_t)(_time>>16);
      _lsbval=(uint32_t)(_time & 0xffff);
      //fprintf(stdout,"MSB Hex value:0x%04x\n",_msbval);
      //fprintf(stdout,"LSB Hex value:0x%04x\n",_lsbval);
      _status=cin_ctl_write(cp,REG_EXPOSURETIMEMSB_REG,_msbval);
      if (_status != 0){goto error;}
      
      _status=cin_ctl_write(cp,REG_EXPOSURETIMELSB_REG,_lsbval);
      if (_status != 0){goto error;}
   }
   return _status;

error:
   return _status;
}

/*TODO:-Malformed packet when MSB=0x0000*/
int cin_set_trigger_delay(struct cin_port* cp,float ftime){  

   int _status;
   uint32_t _time, _msbval,_lsbval;
   float _fraction;

   fprintf(stdout,"  Trigger Delay Time:%f us\n", ftime);    //DEBUG
   _time=(uint32_t)ftime;                          //extract integer from decimal
   _fraction=ftime-_time;                       //extract fraction from decimal
   //fprintf(stdout,"Fraction    :%f\n",_fraction);         //DEBUG
    if(_fraction!=0){                           //Check that there is no fractional value
      perror("ERROR:Smallest precision that can be specified is 1 us\n");
      _status= 1;
      goto error;
    }
   else{ 
      //fprintf(stdout,"Hex value    :0x%08x\n",_time);     
      _msbval=(uint32_t)(_time>>16);
      _lsbval=(uint32_t)(_time & 0xffff);
      //fprintf(stdout,"MSB Hex value:0x%04x\n",_msbval);
      //fprintf(stdout,"LSB Hex value:0x%04x\n",_lsbval)

      _status=cin_ctl_write(cp,REG_DELAYTOEXPOSUREMSB_REG,_msbval);
      if (_status != 0){goto error;}
   
      _status=cin_ctl_write(cp,REG_DELAYTOEXPOSURELSB_REG,_lsbval);
      if (_status != 0){goto error;}
   }
   return _status;

error:
   return _status;
}

/*TODO:-Malformed packet when MSB=0x0000*/
int cin_set_cycle_time(struct cin_port* cp,float ftime){

   int _status;
   uint32_t _time, _msbval,_lsbval;
   float _fraction;
                                       
   fprintf(stdout,"  Cycle Time:%f ms\n", ftime);   //DEBUG
   _time=(uint32_t)ftime;                          //extract integer from decimal
   _fraction=ftime-_time;                       //extract fraction from decimal
   //fprintf(stdout,"Fraction    :%f\n",_fraction);         //DEBUG
   if(_fraction!=0){                         //Check that there is no fractional value
      perror("ERROR:Smallest precision that can be specified is 1 ms\n");
      _status= -1;
      goto error;
   }  
   else{ 
      //fprintf(stdout,"Hex value    :0x%08x\n",_time);     
      _msbval=(uint32_t)(_time>>16);
      _lsbval=(uint32_t)(_time & 0xffff);
      //fprintf(stdout,"MSB Hex value:0x%04x\n",_msbval);
      //fprintf(stdout,"LSB Hex value:0x%04x\n",_lsbval);
      _status=cin_ctl_write(cp,REG_TRIGGERREPETITIONTIMEMSB_REG,_msbval);
      if (_status != 0){goto error;}
   
      _status=cin_ctl_write(cp,REG_TRIGGERREPETITIONTIMELSB_REG,_lsbval);
      if (_status != 0){goto error;}   
   }
   return _status;

error:
   return _status;
}

/******************* Frame Acquisition *************************/
int cin_set_frame_count_reset(struct cin_port* cp){

   int _status;

   _status=cin_ctl_write(cp,REG_FRM_COMMAND, 0x0106);    

   if (_status != 0)
      {goto error;}
         
   INFO("  Frame count set to 0\n");
   return _status;

error:
   return _status;
}

/************************ Testing *****************************/
int cin_test_cfg_leds(struct cin_port* cp){
/* Test Front Panel LEDs */

   fprintf(stdout,"  \nFlashing CFG FP LEDs  ............\n");
   cin_ctl_write(cp, REG_SANDBOX_REG00, 0xAAAA);
   usleep(999999);
   cin_ctl_write(cp, REG_SANDBOX_REG00, 0x5555);
   usleep(999999);
   cin_ctl_write(cp, REG_SANDBOX_REG00, 0xFFFF);
   usleep(999999);
   cin_ctl_write(cp, REG_SANDBOX_REG00, 0x0001);
   usleep(400000);
   cin_ctl_write(cp, REG_SANDBOX_REG00, 0x0002);
   usleep(400000);
   cin_ctl_write(cp, REG_SANDBOX_REG00, 0x0004);
   usleep(400000);
   cin_ctl_write(cp, REG_SANDBOX_REG00, 0x0008);
   usleep(400000);
   cin_ctl_write(cp, REG_SANDBOX_REG00, 0x0010);
   usleep(400000);
   cin_ctl_write(cp, REG_SANDBOX_REG00, 0x0020);
   usleep(400000);
   cin_ctl_write(cp, REG_SANDBOX_REG00, 0x0040);
   usleep(400000);
   cin_ctl_write(cp, REG_SANDBOX_REG00, 0x0080);
   usleep(400000);
   cin_ctl_write(cp, REG_SANDBOX_REG00, 0x0100);
   usleep(400000);
   cin_ctl_write(cp, REG_SANDBOX_REG00, 0x0200);
   usleep(400000);
   cin_ctl_write(cp, REG_SANDBOX_REG00, 0x0400);
   usleep(400000);
   cin_ctl_write(cp, REG_SANDBOX_REG00, 0x0800);
   usleep(400000);
   cin_ctl_write(cp, REG_SANDBOX_REG00, 0x1000);
   usleep(400000);
   cin_ctl_write(cp, REG_SANDBOX_REG00, 0x2000);
   usleep(400000);
   cin_ctl_write(cp, REG_SANDBOX_REG00, 0x4000);
   usleep(400000);
   cin_ctl_write(cp, REG_SANDBOX_REG00, 0x8000);
   usleep(400000);
   cin_ctl_write(cp, REG_SANDBOX_REG00, 0x0000);
   return 0;
}

//void flashCfgLeds(struct CinCtlPort* cp) {
/* Test Front Panel LEDs */

/* fprintf(stdout,"\nFlashing CFG FP LEDs  ............ \n");
   cin_ctl_write(cp, REG_SANDBOX_REG00, 0xAAAA);
   usleep(999999);
   cin_ctl_write(cp, REG_SANDBOX_REG00, 0x5555);
   usleep(999999);
   cin_ctl_write(cp, REG_SANDBOX_REG00, 0xFFFF);
   usleep(999999);
   cin_ctl_write(cp, REG_SANDBOX_REG00, 0x0001);
   usleep(400000);
   cin_ctl_write(cp, REG_SANDBOX_REG00, 0x0002);
   usleep(400000);
   cin_ctl_write(cp, REG_SANDBOX_REG00, 0x0004);
   usleep(400000);
   cin_ctl_write(cp, REG_SANDBOX_REG00, 0x0008);
   usleep(400000);
   cin_ctl_write(cp, REG_SANDBOX_REG00, 0x0010);
   usleep(400000);
   cin_ctl_write(cp, REG_SANDBOX_REG00, 0x0020);
   usleep(400000);
   cin_ctl_write(cp, REG_SANDBOX_REG00, 0x0040);
   usleep(400000);
   cin_ctl_write(cp, REG_SANDBOX_REG00, 0x0080);
   usleep(400000);
   cin_ctl_write(cp, REG_SANDBOX_REG00, 0x0100);
   usleep(400000);
   cin_ctl_write(cp, REG_SANDBOX_REG00, 0x0200);
   usleep(400000);
   cin_ctl_write(cp, REG_SANDBOX_REG00, 0x0400);
   usleep(400000);
   cin_ctl_write(cp, REG_SANDBOX_REG00, 0x0800);
   usleep(400000);
   cin_ctl_write(cp, REG_SANDBOX_REG00, 0x1000);
   usleep(400000);
   cin_ctl_write(cp, REG_SANDBOX_REG00, 0x2000);
   usleep(400000);
   cin_ctl_write(cp, REG_SANDBOX_REG00, 0x4000);
   usleep(400000);
   cin_ctl_write(cp, REG_SANDBOX_REG00, 0x8000);
   usleep(400000);
   cin_ctl_write(cp, REG_SANDBOX_REG00, 0x0000);
}
*/
/* XXX - does not appear to have any effect
static void flashFrmLeds(CinCtlPort* cp) {
    //Test Front Panel LEDs 
   fprintf(stdout,"Flashing FRM FP LEDs  ............ \n");
	 cin_ctl_write(cp, REG_FRM_SANDBOX_REG00, 0x0004);    
   fprintf(stdout,"RED  ............ \n");
   usleep(500000);
   cin_ctl_write(cp, REG_FRM_SANDBOX_REG00, 0x0008);
   fprintf(stdout,"GRN  ............ \n");
   usleep(500000);
   cin_ctl_write(cp, REG_FRM_SANDBOX_REG00, 0x000C);
   fprintf(stdout,"YEL  ............ \n");
   usleep(500000);
   cin_ctl_write(cp, REG_FRM_SANDBOX_REG00, 0x0010);
   fprintf(stdout,"RED  ............ \n");
   usleep(500000);
   cin_ctl_write(cp, REG_FRM_SANDBOX_REG00, 0x0020);
   fprintf(stdout,"GRN  ............ \n");
   usleep(500000);
   cin_ctl_write(cp, REG_FRM_SANDBOX_REG00, 0x0030);
   fprintf(stdout,"YEL  ............ \n");
   usleep(500000);
   cin_ctl_write(cp, REG_FRM_SANDBOX_REG00, 0x0040);
   fprintf(stdout,"RED  ............ \n");
   usleep(500000);
   cin_ctl_write(cp, REG_FRM_SANDBOX_REG00, 0x0080);
   fprintf(stdout,"GRN  ............ \n");
   usleep(500000);
   cin_ctl_write(cp, REG_FRM_SANDBOX_REG00, 0x00C0);
   fprintf(stdout,"YEL  ............ \n");
   usleep(500000);
   cin_ctl_write(cp, REG_FRM_SANDBOX_REG00, 0x0100);
   fprintf(stdout,"RED  ............ \n");
   usleep(500000);
   cin_ctl_write(cp, REG_FRM_SANDBOX_REG00, 0x0200);
   fprintf(stdout,"GRN  ............ \n");
   usleep(500000);
   cin_ctl_write(cp, REG_FRM_SANDBOX_REG00, 0x0300);
   fprintf(stdout,"YEL  ............ \n");
   usleep(500000);
   cin_ctl_write(cp, REG_FRM_SANDBOX_REG00, 0x0400);
   fprintf(stdout,"RED  ............ \n");
   usleep(500000);
   cin_ctl_write(cp, REG_FRM_SANDBOX_REG00, 0x0800);
   fprintf(stdout,"GRN  ............ \n");
   usleep(500000);
   cin_ctl_write(cp, REG_FRM_SANDBOX_REG00, 0x0C00);
   fprintf(stdout,"YEL  ............ \n");
   usleep(500000);
   cin_ctl_write(cp, REG_FRM_SANDBOX_REG00, 0x0000);
   fprintf(stdout,"All OFF  ............ \n");
}
*/

/* -No longer used-
int cin_set_fclk_125mhz(struct cin_port* cp){
   
   int _status;
   
   fprintf(stdout,"\n****Set CIN FCLK to 125MHz****\n");
   //# Freeze DCO
   _status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xB089);
   if (_status != 0){goto error;}
   _status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xF010);
   if (_status != 0){goto error;}
   _status=cin_ctl_write(cp,REG_FRM_COMMAND, CMD_FCLK_COMMIT);
   if (_status != 0){goto error;}
   fprintf(stdout,"  Write to Reg 137 - Freeze DCO\n");
   fprintf(stdout,"  Set Si570 Oscillator Freq to 125MHz\n");

   //# WR Reg 7
   _status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xB007);
   if (_status != 0){goto error;}
   _status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xF002);
   if (_status != 0){goto error;}
   _status=cin_ctl_write(cp,REG_FRM_COMMAND, CMD_FCLK_COMMIT);
   if (_status != 0){goto error;}

   //# WR Reg 8
   _status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xB008);
   if (_status != 0){goto error;}
   _status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xF042);
   if (_status != 0){goto error;}
   _status=cin_ctl_write(cp,REG_FRM_COMMAND, CMD_FCLK_COMMIT);
   if (_status != 0){goto error;}

   //# WR Reg 9
   _status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xB009);
   if (_status != 0){goto error;}
   _status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xF0BC);
   if (_status != 0){goto error;}
   _status=cin_ctl_write(cp,REG_FRM_COMMAND, CMD_FCLK_COMMIT);
   if (_status != 0){goto error;}

   //# WR Reg 10
   _status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xB00A);
   if (_status != 0){goto error;}
   _status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xF019);
   if (_status != 0){goto error;}
   _status=cin_ctl_write(cp,REG_FRM_COMMAND, CMD_FCLK_COMMIT);
   if (_status != 0){goto error;}

   //# WR Reg 11
   _status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xB00B);
   if (_status != 0){goto error;}
   _status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xF06D);
   if (_status != 0){goto error;}
   _status=cin_ctl_write(cp,REG_FRM_COMMAND, CMD_FCLK_COMMIT);
   if (_status != 0){goto error;}

   //# WR Reg 12
   _status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xB00C);
   if (_status != 0){goto error;}
   _status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xF08F);
   if (_status != 0){goto error;}
   _status=cin_ctl_write(cp,REG_FRM_COMMAND, CMD_FCLK_COMMIT);
   if (_status != 0){goto error;}

   //# UnFreeze DCO
   _status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xB089);
   if (_status != 0){goto error;}
   _status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xF000);
   if (_status != 0){goto error;}
   _status=cin_ctl_write(cp,REG_FRM_COMMAND, CMD_FCLK_COMMIT);
   if (_status != 0){goto error;}
   _status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xB087);
   if (_status != 0){goto error;}
   _status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xF040);
   if (_status != 0){goto error;}
   _status=cin_ctl_write(cp,REG_FRM_COMMAND, CMD_FCLK_COMMIT);
   if (_status != 0){goto error;}
   
   fprintf(stdout,"  Write to Reg 137 - UnFreeze DCO & Start Oscillator\n");
   //# Set Clock&Bias Time Contant
   _status=cin_ctl_write(cp,REG_CCDFCLKSELECT_REG,0x0000);
   if (_status != 0){goto error;}
   return 0;
   
   error:{

      return -1;
   }  
}
*/
/*
int cin_get_fclk_status(struct cin_port* cp){ 

   int _status;
   uint32_t _reg,_reg7,_reg8,_reg9,_reg10,_reg11,_reg12,_val7,_val8;
   uint32_t _n1,interger,decimal;
   uint32_t _regfclksel,_valfclksel;

   fprintf(stdout,"\n**** CIN FCLK Configuration ****\n");
   _status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xB089);
   if (_status != 0){goto error;}      
   _reg= cin_ctl_read(cp,REG_FCLK_I2C_DATA_WR);//Is this an 8 element hex string??
   fprintf(stdout,"  FCLK OSC MUX SELECT : %04X \n",_reg);//Assumes 8 element hexstring
   //The statements below assume an 8 element string and use hex elements 4 to 8 of string
   if(_reg & 0x000F0000){ //if(regval[4:5] == "F") 
      //# Freeze DCO
      _status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xB189);
      if (_status != 0){goto error;}
      _status=cin_ctl_write(cp,REG_FRM_COMMAND, CMD_FCLK_COMMIT);
      if (_status != 0){goto error;}
      _reg= cin_ctl_read(cp,REG_FCLK_I2C_DATA_WR);
      if(_reg != 0x80000000){//if (reg_val[6:] != "08") 
         fprintf(stdout,"  Status Reg : %#08X",_reg);
      }
      _status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xB107);
      if (_status != 0){goto error;}
      _status=cin_ctl_write(cp,REG_FRM_COMMAND, CMD_FCLK_COMMIT);
      if (_status != 0){goto error;}
      _reg7= cin_ctl_read(cp,REG_FCLK_I2C_DATA_WR);
   
      _status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xB108);
      if (_status != 0){goto error;}
      _status=cin_ctl_write(cp,REG_FRM_COMMAND, CMD_FCLK_COMMIT);
      if (_status != 0){goto error;}
      _reg8= cin_ctl_read(cp,REG_FCLK_I2C_DATA_WR);

      _status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xB109);
      if (_status != 0){goto error;}
      _status=cin_ctl_write(cp,REG_FRM_COMMAND, CMD_FCLK_COMMIT);
      _reg9= cin_ctl_read(cp,REG_FCLK_I2C_DATA_WR);
      if (_status != 0){goto error;}

      _status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xB10A);
      if (_status != 0){goto error;}
      _status=cin_ctl_write(cp,REG_FRM_COMMAND, CMD_FCLK_COMMIT);
      if (_status != 0){goto error;}
      _reg10= cin_ctl_read(cp,REG_FCLK_I2C_DATA_WR);

      _status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xB10B);
      if (_status != 0){goto error;}
      _status=cin_ctl_write(cp,REG_FRM_COMMAND, CMD_FCLK_COMMIT);
      if (_status != 0){goto error;}
      _reg11= cin_ctl_read(cp,REG_FCLK_I2C_DATA_WR);

      _status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xB10C);
      if (_status != 0){goto error;}
      _status=cin_ctl_write(cp,REG_FRM_COMMAND, CMD_FCLK_COMMIT);
      if (_status != 0){goto error;}
      _reg12= cin_ctl_read(cp,REG_FCLK_I2C_DATA_WR);
   
      _val7 =(_reg7 &   0x000000FF);//bin_reg7 = bin(int(reg_val7[6:],16))[2:].zfill(8)
      _val8 =(_reg8 &   0x000000FF);//bin_reg8 = bin(int(reg_val8[6:],16))[2:].zfill(8)
   
      if((_val7 & 0x00000000)==0x00000000){//if (bin_reg7[0:3] == "000")
         fprintf(stdout,"  FCLK HS Divider = 4\n");
      }
    
      if((_val7 & 0x00000000)==0x40000000){//if (bin_reg7[0:3] == "001")
         fprintf(stdout,"  FCLK HS Divider = 5\n");
      }
   
      if((_val7 & 0x00000000)==0x40000000){//if (bin_reg7[0:3] == "010")
         fprintf(stdout,"  FCLK HS Divider = 6\n");
      } 

      _n1=(uint32_t)(_val7 & 0x0000001F)+(uint32_t)(_val8 & 0x000000C0);   //bin_n1       = bin_reg7[3:8] + bin_reg8[0:2]//dec_n1 = int(bin_n1,2)

      if (_n1%2 != 0){//if (dec_n1%2 != 0) 
         _n1 = _n1 + 1;//dec_n1 = dec_n1 + 1
      }
      fprintf(stdout,"  FCLK N1 Divider = %u",_n1);
   
      interger=(uint32_t)(_reg8 & 0x00000080) + (uint32_t)(_reg9 & 0x00000040);
      decimal=(uint32_t)(_reg9 & 0x00000080) + (uint32_t)(_reg10 & 0x000000C0)+ (uint32_t)(_reg11 & 0x000000C0)+ (uint32_t)(_reg12 & 0x000000C0);
      fprintf(stdout,"  FCLK RFREQ = %u.%u",interger,decimal);////print "  FCLK RFREQ = " + reg_val8[7:] + reg_val9[6:7] + "." + reg_val9[7:] + reg_val10[6:] + reg_val11[6:] + reg_val12[6:]
   
      if(((_reg7 & 0x00000000) == 0x00000000) && _n1==8){//if(bin_reg7[0:3] == "000" and dec_n1 ==  8)
         fprintf(stdout,"  FCLK Frequency = 156 MHz\n");
      }
      else if(((_reg7 & 0x20000000) == 0x20000000) && _n1==10){//elif (bin_reg7[0:3] == "000" and dec_n1 == 10)  
         fprintf(stdout, "  FCLK Frequency = 125 MHz\n");
      }
      else if(((_reg7 & 0x40000000) == 0x40000000) && _n1==4){//elif (bin_reg7[0:3] == "001" and dec_n1 ==  4) 
         fprintf(stdout, "  FCLK Frequency = 250 MHz\n");
      }
      else{ 
         fprintf(stdout,"  FCLK Frequency UNKNOWN\n");
      }
   }
      
   else if(((_reg & 0x00000010) & 0xD0) == 0x20){//elif (str(int(regval[4:5])&1110) == "2") 
      fprintf(stdout,"  FCLK Frequency = 250 MHz\n"); 
   }
   else if(((_reg & 0x00000010) & 0xD0) == 0x60){//elif (str(int(regval[4:5])&1110) == "6") 
      fprintf(stdout,"  FCLK Frequency = 200 MHz\n"); 
   }
   else if(((_reg & 0x00000010) & 0xD0) == 0xA0){//elif (str(int(regval[4:5])&1110) == "A") 
      fprintf(stdout,"  FCLK Frequency = 125 MHz\n"); 
   }
   _regfclksel= cin_ctl_read(cp,REG_CCDFCLKSELECT_REG);
   _valfclksel=(uint32_t)(_regfclksel & 0xFFFF);
   fprintf(stdout,"\n  CCD TIMING CONSTANT : %4X\n", _valfclksel);

  return 0;
  
  error:{
      return _status;
   }  
}
*/

