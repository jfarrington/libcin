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
  struct timespec timestamp;
};

/* prototypes */
/**************************** UDP Socket ******************************/
int cin_init_ctl_port(struct cin_port* cp, char* ipaddr, uint16_t port);
int cin_close_ctl_port(struct cin_port* cp);

/************************* CIN Read/Write *****************************/
uint16_t cin_ctl_read(struct cin_port* cp, uint16_t reg);
int cin_ctl_write(struct cin_port* cp, uint16_t reg, uint16_t val);
											/*TODO - implement write verification procedure */
int cin_stream_write(struct cin_port* cp, char* val,int size);
											/*TODO - implement write verification procedure */
/********************** CIN PowerUP/PowerDown *************************/
int cin_on(struct cin_port* cp);          			//Power ON CIN
int cin_off(struct cin_port* cp);        				//Power OFF CIN
int cin_fp_on(struct cin_port* cp);      				//Power ON CIN front Panel
int cin_fp_off(struct cin_port* cp);    				//Power OFF CIN front Panel

/******************* CIN Configuration/Status *************************/
int cin_load_config(struct cin_port* cp,char *filename);		//Load CIN Configuration File 
																					/*TODO:-Check that file is loaded properly*/
int cin_load_firmware(struct cin_port* cp,char *filename);  //Load CIN Firmware Configuration
																					/*TODO:-Check that file is loaded properly*/
int cin_set_fclk_125mhz(struct cin_port* cp); 	     				//Set CIN clocks to 125MHz
																					/*TODO:-Check that clock is properlly set*/
int cin_get_fclk_status(struct cin_port* cp);   						//Get CIN clock status  		
																					/*TODO:-Check Boolean comparisons*/
int cin_get_cfg_fpga_status(struct cin_port* cp);				//Get CIN FPGA status 		
																					/*TODO:-Check Boolean comparisons*/
int cin_get_power_status(struct cin_port* cp);					//Get Camera/CIN power Status

/**************************** CIN Control *****************************/
int cin_set_bias(struct cin_port* cp,int val);   		//Turn on/off camera CCD bias
										//Input:val={1-ON,0-OFF}
																				
int cin_set_clocks(struct cin_port* cp,int val);  	//Turn on/off camera clocks
					    			//Input:val={1-ON,0-OFF}

int cin_set_trigger(struct cin_port* cp,int val); 	//Set trigger source
//Tested						//Input:val={0-Internal, 1-External1, 2-External2, 3-External 1 or 2}

int cin_get_trigger_status (struct cin_port* cp);//Tested	//Get trigger source status
//Tested						//Return:{0-Internal, 1-External1, 2-External2, 3-External 1 or 2}*/

int cin_set_exposure_time(struct cin_port* cp,float e_time);  //Set the Camera exposure time
				 	 					//Input:e_time (ms)					/*TODO:-Malformed packet when MSB=0x0000*/

int cin_set_trigger_delay(struct cin_port* cp,float t_time);  //Set the trigger delay time
					    			//Input:t_time (us)					/*TODO:-Malformed packet when MSB=0x0000*/

int cin_set_cycle_time(struct cin_port* cp,float c_time);	    //Set the Camera cyle time time
					    			//Input:c_time (ms)					/*TODO:-Malformed packet when MSB=0x0000*/

/************************* Frame Acquistion *****************************/
int cin_set_frame_count_reset(struct cin_port* cp); 			//Sets CIN frame counter to 0

/****************************** Testing *********************************/
int cin_test_cfg_leds(struct cin_port* cp); 	        		//Flash configuration Leds in sequence	


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
void cin_data_release_frame(int free_mem);

#endif
