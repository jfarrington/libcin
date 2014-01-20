#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

#include "cin.h"
#include "cin_register_map.h"
#include "cin_api.h"

/**************************** UDP Socket ******************************/
static int cin_set_sock_timeout(struct cin_port* cp) {

    if(setsockopt(cp->sockfd, SOL_SOCKET, SO_RCVTIMEO,
                    (void*)&cp->tv, sizeof(struct timeval)) < 0) 
    {
        perror("setsockopt(timeout");
        return -1;
    }
    return 0;
}

int cin_init_ctl_port(struct cin_port* cp, char* ipaddr, 
				uint16_t port) 
{
	if(cp->sockfd) {
		perror("CIN control port was already initialized!!");
		return -1;
	}

	if(ipaddr == 0) { cp->srvaddr = CIN_CTL_IP; }
	else { cp->srvaddr = strndup(ipaddr, strlen(ipaddr)); }
	
	if(port == 0) { cp->srvport = CIN_CTL_PORT; }
  else { cp->srvport = port; }

	cp->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if(cp->sockfd < 0) {
  	perror("CIN control port - socket() failed !!!");
    return -1;
	}

	int i = 1;
 	if(setsockopt(cp->sockfd, SOL_SOCKET, SO_REUSEADDR, \
                    (void *)&i, sizeof i) < 0) {
		perror("CIN control port - setsockopt() failed !!!");
		return -1;
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
   	return -1;
 	}
 	return 0;
}

int cin_close_ctl_port(struct cin_port* cp) {

    if(cp->sockfd) { close(cp->sockfd); }
    return 0;
}
/*************************** CIN Read/Write ***************************/

int cin_ctl_write(struct cin_port* cp, uint16_t reg, uint16_t val) {

 	uint32_t _val;
  int rc;

  _val = ntohl((uint32_t)(reg << 16 | val));
  rc = sendto(cp->sockfd, &_val, sizeof(_val), 0, \
         						(struct sockaddr*)&cp->sin_srv,\ 
         						sizeof(cp->sin_srv));
  if(rc != sizeof(_val) ) {
   	perror("CIN control port - sendto( ) failure!!");
   	return -1;
 	}

 	/*** TODO - implement write verification procedure ***/
 	return 0;
}

int cin_stream_write(struct cin_port* cp, char *val,int size) {
 
 	int rc;
    
 	rc = sendto(cp->sockfd, &val, size, 0,\
    	(struct sockaddr*)&cp->sin_srv, sizeof(cp->sin_srv));
 	if(rc != size ) {
  	perror("CIN control port - sendto( ) failure!!");
   	return -1;
 	}

 	/*** TODO - implement write verification procedure ***/
 	return 0;
}													

uint16_t cin_ctl_read(struct cin_port* cp, uint16_t reg) {
	 
	int status;
	uint32_t buf = 0;
  ssize_t n;

 	status=cin_ctl_write(cp, REG_READ_ADDRESS, reg);
 	if (status != 0){goto error;}
 	sleep(0.1);

 	status=cin_ctl_write(cp, REG_COMMAND, CMD_READ_REG);
 	if (status != 0){goto error;}

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
    return -1;
 	}

  /* reset socket timeout to 0.1s default */
 	cp->tv.tv_sec = 0;
 	cp->tv.tv_usec = 100000;
 	cin_set_sock_timeout(cp);
 	buf = ntohl(buf);
 	return (uint16_t)(buf);
    
  error:{	
  	perror("Write error in cin_ctl_read\n");
		return -1;
	}	
}

/******************* CIN PowerUP/PowerDown *************************/
int cin_on(struct cin_port* cp){

	int status;
	 
	fprintf(stdout,"Powering ON CIN Board ........\n");
	status=cin_ctl_write(cp,REG_PS_ENABLE, 0x000f);
	if (status != 0){goto error;}
	status=cin_ctl_write(cp,REG_COMMAND, CMD_PS_ENABLE);
	if (status != 0){goto error;}
//	status=cin_ctl_write(cp,REG_PS_ENABLE, 0x001f); //This is like CIN_FP_PowerDown ???
	if (status != 0){goto error;}
//	status=cin_ctl_write(cp,REG_COMMAND, CMD_PS_ENABLE);
	if (status != 0){goto error;}
	return 0;
	
	error:{
		perror("Write error\n");
		return -1;
	}	
}

int cin_off(struct cin_port* cp) {

	int status;
	  
	fprintf(stdout,"Powering OFF CIN Board ........\n");
	status=cin_ctl_write(cp,REG_PS_ENABLE, 0x0000);
	if (status != 0){goto error;}
	status=cin_ctl_write(cp,REG_COMMAND, CMD_PS_ENABLE);
	if (status != 0){goto error;}
	return 0;
	
	error:{
		perror("Write error\n");
		return -1;
	}	
}

int cin_fp_on(struct cin_port* cp){	

 	int status;
 	
	fprintf(stdout,"Powering ON Front Panel Boards ........\n");
	status=cin_ctl_write(cp,REG_PS_ENABLE, 0x003f);
	if (status != 0){goto error;}
	status=cin_ctl_write(cp,REG_COMMAND, CMD_PS_ENABLE);
	if (status != 0){goto error;}
	return 0;
	
	error:{
		perror("Write error\n");
		return -1;
	}	
}

int cin_fp_off(struct cin_port* cp){

 	int status;
 	
	fprintf(stdout,"Powering OFF Front Panel Boards ........\n");
	status=cin_ctl_write(cp,REG_PS_ENABLE, 0x001f);
	if (status != 0){goto error;}
	status=cin_ctl_write(cp,REG_COMMAND, CMD_PS_ENABLE);
	if (status != 0){goto error;}
	return 0;
	
	error:{
		perror("Write error\n");
		return -1;
	}	
}

/******************* CIN Configuration/Status *************************/
/*TODO:-Check that file is loaded properly*/
int cin_load_config(struct cin_port* cp,char *filename){

  int status;
  unsigned int read_reg, read_val, REG, VAL;
//  uint16_t read_reg, read_val, REG, VAL;
  unsigned int i=1;//*DEBUG* 
  char reg_str[12],val_str[12],line [1024];

  FILE *file = fopen ( filename, "r" );
	if (file != NULL) {  
		fprintf(stdout,"\n****Send CIN configuration file****\n");
		while(fgets(line,sizeof line,file)!= NULL){ /* read a line from a file */     
			line[strlen(line)-1]='\0';   
      fprintf(stdout,"line%u:  %c%c%c%c ",i,line[0],line[1],line[2],line[3]);//*DEBUG* 
      i++;  //*DEBUG*
      if ('#' == line[0]||'\0' == line[0]){   
        fprintf(stdout,"Ignore line\n");//*DEBUG* 
      }
      else {
      	sscanf (line,"%04x %04x",&read_reg,&read_val);
        sprintf(reg_str,"0x%04x",read_reg);
        sprintf(val_str,"0x%04x",read_val);
        sscanf(reg_str, "%x", &REG);
        sscanf(val_str, "%x", &VAL);

        status=cin_ctl_write(cp,REG,VAL);//	WriteReg( read_addr, read_data, 0 ) 
        if (status != 0){goto error;}    

        fprintf(stdout," Get line: %04x %04x\n",REG,VAL);//*DEBUG*  
      }   
      memset(line,'\0',sizeof(line));
    }
    fprintf(stdout,"\nCIN Configuration sent!\n");//*DEBUG* 
    fclose(file);
  }
  else {
    perror(filename); 
  } 
  return 0;
  
  error:{
		perror("Write error\n");
		return -1;
	}	
}
/*TODO:-Check that file is loaded properly*/
int cin_load_firmware(struct cin_port* cp,char *filename){
	
	//uint32_t num_e, fileLen, pack_t_num,pack_size=128;
  int status;
	unsigned long num_e, fileLen, pack_t_num,pack_size=128;
	int pack_num=1;//*DEBUG*
	char *buffer;     
	
	FILE *file= fopen(filename, "rb");
	if (file != NULL) {	
		//Get file length
		fseek(file, 0, SEEK_END);
		fileLen=ftell(file);
		fseek(file, 0, SEEK_SET);
		//Allocate memory
   	buffer=(char *)malloc(pack_size+1);
   	if (!buffer){
   		perror("Memory error!");
   		fclose(file);
   	}
		pack_t_num=fileLen/pack_size;
		//Read file and send in packets       
		num_e=fread(buffer,pack_size, 1, file);	
		while (num_e!= 0  ){         	
			status=cin_stream_write(cp, buffer,sizeof(buffer));
			if (status != 0){goto error;} 		

			//	cp->write_bin(buffer,1);        //needs to write to CIN_STREAM_IN_PORT   
			fprintf(stdout,"Pack %u of %lu sent; Read:%lu \n ",pack_num,pack_t_num,num_e);//*DEBUG*
			num_e=fread(buffer,pack_size, 1, file);
			pack_num++;//*DEBUG*
		}  
		pack_num--;
		fprintf(stdout,"File size:%luB \n",fileLen); //*DEBUG*
		fprintf(stdout,"%u packs of size %luB sent!  \n ",pack_num,pack_size);//*DEBUG*
	}
	else {
		perror(filename); //print the error message on stderr.
	}
	fprintf(stdout,"\nFPGA Configuration sent!\n");
	free(buffer);  
	fclose(file);
	return 0;
	
	error:{
		perror("Write error\n");
		return -1;
	}
}
/*TODO:-Check that clock is properlly set*/
int cin_set_fclk_125mhz(struct cin_port* cp){
	
	int status;
	
	fprintf(stdout,"\n****Set CIN FCLK to 125MHz****\n");
	//# Freeze DCO
	status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xB089);
	if (status != 0){goto error;}
	status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xF010);
	if (status != 0){goto error;}
	status=cin_ctl_write(cp,REG_FRM_COMMAND, CMD_FCLK_COMMIT);
	if (status != 0){goto error;}
	fprintf(stdout,"  Write to Reg 137 - Freeze DCO\n");
	fprintf(stdout,"  Set Si570 Oscillator Freq to 125MHz\n");

	//# WR Reg 7
	status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xB007);
	if (status != 0){goto error;}
	status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xF002);
	if (status != 0){goto error;}
	status=cin_ctl_write(cp,REG_FRM_COMMAND, CMD_FCLK_COMMIT);
	if (status != 0){goto error;}

	//# WR Reg 8
	status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xB008);
	if (status != 0){goto error;}
	status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xF042);
	if (status != 0){goto error;}
	status=cin_ctl_write(cp,REG_FRM_COMMAND, CMD_FCLK_COMMIT);
	if (status != 0){goto error;}

	//# WR Reg 9
	status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xB009);
	if (status != 0){goto error;}
	status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xF0BC);
	if (status != 0){goto error;}
	status=cin_ctl_write(cp,REG_FRM_COMMAND, CMD_FCLK_COMMIT);
	if (status != 0){goto error;}

	//# WR Reg 10
	status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xB00A);
	if (status != 0){goto error;}
	status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xF019);
	if (status != 0){goto error;}
	status=cin_ctl_write(cp,REG_FRM_COMMAND, CMD_FCLK_COMMIT);
	if (status != 0){goto error;}

	//# WR Reg 11
	status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xB00B);
	if (status != 0){goto error;}
	status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xF06D);
	if (status != 0){goto error;}
	status=cin_ctl_write(cp,REG_FRM_COMMAND, CMD_FCLK_COMMIT);
	if (status != 0){goto error;}

	//# WR Reg 12
	status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xB00C);
	if (status != 0){goto error;}
	status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xF08F);
	if (status != 0){goto error;}
	status=cin_ctl_write(cp,REG_FRM_COMMAND, CMD_FCLK_COMMIT);
	if (status != 0){goto error;}

	//# UnFreeze DCO
	status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xB089);
	if (status != 0){goto error;}
	status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xF000);
	if (status != 0){goto error;}
	status=cin_ctl_write(cp,REG_FRM_COMMAND, CMD_FCLK_COMMIT);
	if (status != 0){goto error;}
	status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xB087);
	if (status != 0){goto error;}
	status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xF040);
	if (status != 0){goto error;}
	status=cin_ctl_write(cp,REG_FRM_COMMAND, CMD_FCLK_COMMIT);
	if (status != 0){goto error;}
	
	fprintf(stdout,"  Write to Reg 137 - UnFreeze DCO & Start Oscillator\n");
	//# Set Clock&Bias Time Contant
	status=cin_ctl_write(cp,REG_CCDFCLKSELECT_REG,0x0000);
	if (status != 0){goto error;}
	return 0;
	
	error:{
		perror("Write error\n");
		return -1;
	}	
}
/*TODO:-Check Boolean comparisons*/
int cin_get_fclk_status(struct cin_port* cp){ 

	int status;
	uint32_t _reg,_reg7,_reg8,_reg9,_reg10,_reg11,_reg12,_val7,_val8;
	uint32_t _n1,interger,decimal;
	uint32_t _regfclksel,_valfclksel;

	fprintf(stdout,"\n**** CIN FCLK Configuration ****\n");
	status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xB089);
	if (status != 0){goto error;}		
	_reg= cin_ctl_read(cp,REG_FCLK_I2C_DATA_WR);//Is this an 8 element hex string??
	fprintf(stdout,"  FCLK OSC MUX SELECT : %#08X \n",_reg);//Assumes 8 element hexstring

	//The statements below assume an 8 element string and use hex elements 4 to 8 of string
	if(_reg & 0x000F0000){ //if(regval[4:5] == "F") 
		//# Freeze DCO
		status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xB189);
		if (status != 0){goto error;}
		status=cin_ctl_write(cp,REG_FRM_COMMAND, CMD_FCLK_COMMIT);
		if (status != 0){goto error;}
		_reg= cin_ctl_read(cp,REG_FCLK_I2C_DATA_WR);
		if(_reg != 0x80000000){//if (reg_val[6:] != "08") 
			fprintf(stdout,"  Status Reg : %#08X",_reg);
		}
		status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xB107);
		if (status != 0){goto error;}
		status=cin_ctl_write(cp,REG_FRM_COMMAND, CMD_FCLK_COMMIT);
		if (status != 0){goto error;}
		_reg7= cin_ctl_read(cp,REG_FCLK_I2C_DATA_WR);
	
		status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xB108);
		if (status != 0){goto error;}
		status=cin_ctl_write(cp,REG_FRM_COMMAND, CMD_FCLK_COMMIT);
		if (status != 0){goto error;}
		_reg8= cin_ctl_read(cp,REG_FCLK_I2C_DATA_WR);

		status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xB109);
		if (status != 0){goto error;}
		status=cin_ctl_write(cp,REG_FRM_COMMAND, CMD_FCLK_COMMIT);
		_reg9= cin_ctl_read(cp,REG_FCLK_I2C_DATA_WR);
		if (status != 0){goto error;}

		status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xB10A);
		if (status != 0){goto error;}
		status=cin_ctl_write(cp,REG_FRM_COMMAND, CMD_FCLK_COMMIT);
		if (status != 0){goto error;}
		_reg10= cin_ctl_read(cp,REG_FCLK_I2C_DATA_WR);

		status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xB10B);
		if (status != 0){goto error;}
		status=cin_ctl_write(cp,REG_FRM_COMMAND, CMD_FCLK_COMMIT);
		if (status != 0){goto error;}
		_reg11= cin_ctl_read(cp,REG_FCLK_I2C_DATA_WR);

		status=cin_ctl_write(cp,REG_FCLK_I2C_ADDRESS, 0xB10C);
		if (status != 0){goto error;}
		status=cin_ctl_write(cp,REG_FRM_COMMAND, CMD_FCLK_COMMIT);
		if (status != 0){goto error;}
		_reg12= cin_ctl_read(cp,REG_FCLK_I2C_DATA_WR);
	
		_val7 =(_reg7 &	0x000000FF);//bin_reg7 = bin(int(reg_val7[6:],16))[2:].zfill(8)
		_val8 =(_reg8 &	0x000000FF);//bin_reg8 = bin(int(reg_val8[6:],16))[2:].zfill(8)
	
		if((_val7 & 0x00000000)==0x00000000){//if (bin_reg7[0:3] == "000")
			fprintf(stdout,"  FCLK HS Divider = 4\n");
		}
	 
		if((_val7 & 0x00000000)==0x40000000){//if (bin_reg7[0:3] == "001")
			fprintf(stdout,"  FCLK HS Divider = 5\n");
		}
	
		if((_val7 & 0x00000000)==0x40000000){//if (bin_reg7[0:3] == "010")
			fprintf(stdout,"  FCLK HS Divider = 6\n");
		} 

		_n1=(uint32_t)(_val7 & 0x0000001F)+(uint32_t)(_val8 & 0x000000C0);	//bin_n1 		= bin_reg7[3:8] + bin_reg8[0:2]//dec_n1 = int(bin_n1,2)

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
	fprintf(stdout,"\n  CCD TIMING CONSTANT : 0x%X", _valfclksel);

  return 0;
  
  error:{
		perror("Write error\n");
		return -1;
	}	
}
/*TODO:-Check Boolean comparisons*/
int cin_get_cfg_fpga_status(struct cin_port* cp){
		
	uint16_t _val;
	fprintf(stdout,"\n****  CFG FPGA Status Registers  ****\n");	
	//# get Status Registers
	_val= cin_ctl_read(cp,REG_BOARD_ID);
	fprintf(stdout,"  CIN Board ID     :  %u\n",_val);
	_val= cin_ctl_read(cp,REG_HW_SERIAL_NUM);
	fprintf(stdout,"  HW Serial Number :  %u\n",_val);
	_val= cin_ctl_read(cp,REG_FPGA_VERSION);
	fprintf(stdout,"  CFG FPGA Version :  %u\n",_val);
	_val= cin_ctl_read(cp,REG_FPGA_STATUS);
	fprintf(stdout,"  CFG FPGA Status  :  %u\n",_val);
	/*
	# FPGA Status
	# 15 == FRM DONE
	# 14 == NOT FRM BUSY
	# 13 == NOT FRM INIT B
	# 12 >> 4 == 0
	# 3 >>0 == FP Config Control 3 == PS Interlock
	*/
	_val= cin_ctl_read(cp,REG_FPGA_STATUS);	

	if(_val & 0x0008){//if (int(stats_vec[-16]) == 1) ?? *CHECK*
		fprintf(stdout,"  ** Frame FPGA Configuration Done\n"); 
	}
	else{
		fprintf(stdout,"  ** Frame FPGA NOT Configured\n");
	}
	if(_val & 0x8000){//i//if (int(stats_vec[-4]) == 1) ?? *CHECK*
		fprintf(stdout,"  ** FP Power Supply Unlocked\n"); 
	}
	else{
		fprintf(stdout,"  ** FP Power Supply Locked Off\n");
	}

	_val= cin_ctl_read(cp,REG_DCM_STATUS);
	fprintf(stdout,"\n  CFG DCM Status   :  %u\n",_val);
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
	_val= cin_ctl_read(cp,REG_DCM_STATUS);//reg_val = bin((int(cin_functions.ReadReg( cin_register_map.REG_DCM_STATUS)[4:8],16)))[2:].zfill(16)
//stats_vec = reg_val[:]
	if(_val & 0x0800){//if (int(stats_vec[-8]) == 1) ?? *CHECK*
		fprintf(stdout,"  ** ATCA 48V Alarm\n"); 
	}
	else{
		fprintf(stdout,"  ** ATCA 48V OK\n");
	}
	if(_val & 0x1000){//if (int(stats_vec[-1]) == 1) ?? *CHECK*
		fprintf(stdout,"  ** CFG Clock DCM Locked\n"); 
	}
	else{
		fprintf(stdout,"  ** CFG Clock DCM NOT Locked\n");
	}
	if(_val != 0x0080){//if (int(stats_vec[-12]) == 0) ?? *CHECK*
		fprintf(stdout,"  ** FP Power Supply Interlock Overide Enabled\n"); 	
	}
	return 0;
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
    fprintf(stdout,"%s %0.2fV @ %0.3fA\n", desc, _voltage, _current);
}

int cin_get_power_status(struct cin_port* cp) {
    
    double _current, _voltage;
    uint16_t _val = cin_ctl_read(cp, REG_PS_ENABLE);
		
		fprintf(stdout,"****  CIN Power Monitor  ****\n");
    if(_val & 0x0001) {
        /* ADC == LT4151 */
        _val = cin_ctl_read(cp, REG_VMON_ADC1_CH1);
        _voltage = 0.025*_val;
        _val = cin_ctl_read(cp, REG_IMON_ADC1_CH0);
        _current = 0.00002*_val/0.003;
        fprintf(stdout,"V12P_BUS Power  : %0.2fV @ %0.2fA\n\n", _voltage, _current);

        /* ADC == LT2418 */
        calcVIStatus(cp, REG_VMON_ADC0_CH5, REG_IMON_ADC0_CH5,
                0.00015258, "V3P3_MGMT Power  :");
        calcVIStatus(cp, REG_VMON_ADC0_CH7, REG_IMON_ADC0_CH7,
                0.00015258, "V2P5_MGMT Power  :");
        calcVIStatus(cp, REG_VMON_ADC0_CH2, REG_IMON_ADC0_CH2,
                0.00007629, "V1P2_MGMT Power  :");
        calcVIStatus(cp, REG_VMON_ADC0_CH3, REG_IMON_ADC0_CH3,
                0.00007629, "V1P0_ENET Power  :");
        fprintf(stdout,"\n");
        calcVIStatus(cp, REG_VMON_ADC0_CH4, REG_IMON_ADC0_CH4,
                0.00015258, "V3P3_S3E Power   :");
        calcVIStatus(cp, REG_VMON_ADC0_CH8, REG_IMON_ADC0_CH8,
                0.00015258, "V3P3_GEN Power   :");
        calcVIStatus(cp, REG_VMON_ADC0_CH9, REG_IMON_ADC0_CH9,
                0.00015258, "V2P5_GEN Power   :");
        fprintf(stdout,"\n");
        calcVIStatus(cp, REG_VMON_ADC0_CHE, REG_IMON_ADC0_CHE,
                0.00007629, "V0P9_V6 Power    :");
        calcVIStatus(cp, REG_VMON_ADC0_CHB, REG_IMON_ADC0_CHB,
                0.00007629, "V1P0_V6 Power    :");
        calcVIStatus(cp, REG_VMON_ADC0_CHD, REG_IMON_ADC0_CHD,
                0.00015258, "V2P5_V6 Power    :");
        fprintf(stdout,"\n");
        calcVIStatus(cp, REG_VMON_ADC0_CHF, REG_IMON_ADC0_CHF,
                0.00030516, "V_FP Power       :");
    }
    else {
        fprintf(stdout,"  12V Power Supply is OFF\n");
    }
    return 0;
    
}

/******************* CIN Control *************************/
int cin_set_bias(struct cin_port* cp,int val){

	int status;
	
	if (val==1){
		status=cin_ctl_write(cp,REG_BIASCONFIGREGISTER0_REG, 0x0001);
		if (status != 0){goto error;}
		fprintf(stdout,"Bias ON\n");
	}
	else if (val==0){
		status=cin_ctl_write(cp,REG_BIASCONFIGREGISTER0_REG, 0x0000);
		if (status != 0){goto error;}
		fprintf(stdout,"Bias OFF\n");
	}
	else{
		fprintf(stdout,"Illegal Bias state: Only 0 or 1 allowed\n");
	}
	return 0;
	
	error:{
		perror("Write error\n");
		return -1;
	}	
}

int cin_set_clocks(struct cin_port* cp,int val){

	int status;   
	
	if (val==1){
		status=cin_ctl_write(cp,REG_CLOCKCONFIGREGISTER0_REG, 0x0001);
		if (status != 0){goto error;}
		fprintf(stdout,"Clocks ON\n");
	}
	else if (val==0){
		status=cin_ctl_write(cp,REG_CLOCKCONFIGREGISTER0_REG, 0x0000);
		if (status != 0){goto error;}
		fprintf(stdout,"Clocks OFF\n");
	}
	else{
		fprintf(stdout,"Illegal Clocks state: Only 0 or 1 allowed\n");
	}
	return 0;
	
	error:{
		perror("Write error\n");
		return -1;
	}	
}
/*TODO:-Malformed packet when MSB=0x0000*/
int cin_set_trigger(struct cin_port* cp,int val){

	int status;
	
	if (val==0){
		status=cin_ctl_write(cp,REG_TRIGGERMASK_REG, 0x0000);//Internal trigger
		if (status != 0){goto error;}
		fprintf(stdout,"Trigger set to Internal\n");
	}
	else if (val==1){
		status=cin_ctl_write(cp,REG_TRIGGERMASK_REG, 0x0001);//External trigger chan 1
		if (status != 0){goto error;}
		fprintf(stdout,"Trigger set to External 1\n");
	}
	else if (val==2){
		status=cin_ctl_write(cp,REG_TRIGGERMASK_REG, 0x0002);//External trigger chan 2
		if (status != 0){goto error;}
		fprintf(stdout,"Trigger set to External 2\n");
	}
	else if (val==3){
		status=cin_ctl_write(cp,REG_TRIGGERMASK_REG, 0x0003);//External trigger chan 1 or 2
		if (status != 0){goto error;}
		fprintf(stdout,"Trigger set to External (1 or 2)\n");
	}
	else{
		fprintf(stdout,"Illegal Trigger state: Only values 0 to 3 allowed\n");
	}
	return 0;

	error:{
		perror("Write error\n");
		return -1;
	}		
}

int cin_get_trigger_status (struct cin_port* cp){

	uint16_t _val;
	int _state;
	
	_val =cin_ctl_read(cp,REG_TRIGGERMASK_REG); 
	//fprintf(stdout,"  TRIGGERMASK REG VALUE: 0x%04x\n",_val);//DEBUG
	if (_val == 0x0000){
		_state=0;
		fprintf(stdout,"Trigger status is Internal\n");
		return _state;
	}
	else if (_val == 0x0001){
		_state=1;
		fprintf(stdout,"Trigger status is External 1\n");
		return _state;
	}
	else if (_val == 0x0002){
		_state=2;	
		fprintf(stdout,"Trigger status is External 2\n");
		return _state;
	}
	else if (_val == 0x0003){
		_state=3;
		fprintf(stdout,"Trigger status is External (1 or 2)\n");
		return _state;
	}
	else{
		fprintf(stdout,"Unknown Trigger status\n");
	}
	return 0;
}
/*TODO:-Malformed packet when MSB=0x0000*/
int cin_set_exposure_time(struct cin_port* cp,float ftime){  //Set the Camera exposure time

 	int status;
  uint32_t _time, _msbval,_lsbval;
  float _fraction;

	fprintf(stdout,"ExposureTime :%f ms\n", ftime);	//DEBUG
	ftime=ftime*100;
	_time=(uint32_t)ftime;									//extract integer from decimal
	_fraction=ftime-_time;						      //extract fraction from decimal
	//fprintf(stdout,"Fraction	   :%f\n",_fraction);  //DEBUG
  if(_fraction!=0){									//Check that there is no fractional value
  	perror("ERROR:Smallest precision that can be specified is .01 ms\n");
  	return 0;
  }
  else{	
		//fprintf(stdout,"Hex value    :0x%08x\n",_time);		
 		_msbval=(uint32_t)(_time>>16);
 		_lsbval=(uint32_t)(_time & 0xffff);
		//fprintf(stdout,"MSB Hex value:0x%04x\n",_msbval);
		//fprintf(stdout,"LSB Hex value:0x%04x\n",_lsbval);
		status=cin_ctl_write(cp,REG_EXPOSURETIMEMSB_REG,_msbval);
		if (status != 0){goto error;}
		
		status=cin_ctl_write(cp,REG_EXPOSURETIMELSB_REG,_lsbval);
		if (status != 0){goto error;}

		return 0;
	}
	error:{
		perror("Write error\n");
		return -1;
	}		
}
/*TODO:-Malformed packet when MSB=0x0000*/
int cin_set_trigger_delay(struct cin_port* cp,float ftime){  //Set the trigger delay
	
	int status;
	uint32_t _time, _msbval,_lsbval;
  float _fraction;

	fprintf(stdout,"Trigger Delay Time:%f ms\n", ftime);	  //DEBUG
	_time=(uint32_t)ftime;									//extract integer from decimal
	_fraction=ftime-_time;						      //extract fraction from decimal
	//fprintf(stdout,"Fraction	   :%f\n",_fraction);  //DEBUG
  if(_fraction!=0){									//Check that there is no fractional value
  	perror("ERROR:Smallest precision that can be specified is 1 us\n");
  	return 0;
  }
  else{	
		//fprintf(stdout,"Hex value    :0x%08x\n",_time);		
 		_msbval=(uint32_t)(_time>>16);
 		_lsbval=(uint32_t)(_time & 0xffff);
		//fprintf(stdout,"MSB Hex value:0x%04x\n",_msbval);
		//fprintf(stdout,"LSB Hex value:0x%04x\n",_lsbval)

		status=cin_ctl_write(cp,REG_DELAYTOEXPOSUREMSB_REG,_msbval);
		if (status != 0){goto error;}
	
		status=cin_ctl_write(cp,REG_DELAYTOEXPOSURELSB_REG,_lsbval);
		if (status != 0){goto error;}
	
		return 0;
	}
	error:{
		perror("Write error\n");
		return -1;
	}		
}
/*TODO:-Malformed packet when MSB=0x0000*/
int cin_set_cycle_time(struct cin_port* cp,float ftime){

	int status;
	uint32_t _time, _msbval,_lsbval;
  float _fraction;
  													
	fprintf(stdout,"Cycle Time				:%f ms\n", ftime);	  //DEBUG
	_time=(uint32_t)ftime;									//extract integer from decimal
	_fraction=ftime-_time;						      //extract fraction from decimal
	//fprintf(stdout,"Fraction	   :%f\n",_fraction);  //DEBUG
  if(_fraction!=0){									//Check that there is no fractional value
  	perror("ERROR:Smallest precision that can be specified is 1 ms\n");
  	return -1;
  }
  else{	
		//fprintf(stdout,"Hex value    :0x%08x\n",_time);		
 		_msbval=(uint32_t)(_time>>16);
 		_lsbval=(uint32_t)(_time & 0xffff);
		//fprintf(stdout,"MSB Hex value:0x%04x\n",_msbval);
		//fprintf(stdout,"LSB Hex value:0x%04x\n",_lsbval);
		status=cin_ctl_write(cp,REG_TRIGGERREPETITIONTIMEMSB_REG,_msbval);
		if (status != 0){goto error;}
	
		status=cin_ctl_write(cp,REG_TRIGGERREPETITIONTIMELSB_REG,_lsbval);
		if (status != 0){goto error;}
		
		return 0;
	}
	
	error:{
		perror("Write error\n");
		return -1;
	}		
}

/******************* Frame Acquisition *************************/
int cin_set_frame_count_reset(struct cin_port* cp){
	int status;

	status=cin_ctl_write(cp,REG_FRM_COMMAND, 0x0106);		

	if (status != 0){goto error;}
			
	fprintf(stdout,"Frame count set to 0\n");
	return 0;

	error:{
		perror("Write error\n");
		return -1;
	}	
}
/************************ Testing *****************************/
int cin_test_cfg_leds(struct cin_port* cp){
/* Test Front Panel LEDs */
	fprintf(stdout,"\nFlashing CFG FP LEDs  ............\n");
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
/*	fprintf(stdout,"\nFlashing CFG FP LEDs  ............ \n");
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


