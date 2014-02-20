#include <stdio.h>
#include <unistd.h> /* for sleep() */
#include <string.h>
#include <stdlib.h>

#include "cin.h"

int main(){

	int ret_fclk,ret_fpga;
	/*Set directory for CIN configuration files*/ 
	char fccd_config_dir[]="../cin_config/";	

	/*Set CIN FPGA configuration file*/   
	char fpga_configfile[]="top_frame_fpga-v1019j.bit";

	/*Set CIN configuration file*/ 
	char cin_configfile_waveform[]="2013_Nov_30-200MHz_CCD_timing.txt";

	/*Create Path to files*/
	char cin_fpga_config[1024];
	char cin_waveform_config[1024];
	sprintf(cin_fpga_config,"%s%s", fccd_config_dir,fpga_configfile);
	sprintf(cin_waveform_config,"%s%s", fccd_config_dir,cin_configfile_waveform);

	/*Set control ports*/	
	struct cin_port cp[2];
	
	cin_init_ctl_port(&cp[0], 0, 0);/* Use default CIN control port */
	cin_init_ctl_port(&cp[1], 0,CIN_DATA_CTL_PORT);/* Use CIN control data port */
	
	cin_off(&cp[0]);
	sleep(5);

	cin_on(&cp[0]);
	sleep(5);

	cin_fp_on(&cp[0]);
	sleep(5);
	
	cin_get_cfg_fpga_status(&cp[0]);
	sleep(1);

	cin_load_firmware(&cp[0],&cp[1],cin_fpga_config);	
	sleep(5);

	cin_ctl_write(&cp[0],0x8013,0x057F);
	usleep(1000);
	cin_ctl_write(&cp[0],0x8014,0x0A17);
	usleep(1000);
	ret_fpga=cin_get_cfg_fpga_status(&cp[0]);
	sleep(1);

	ret_fclk=cin_get_fclk_status(&cp[0]);			
	sleep(1);
/************************* FCCD Configuration **************************/	

	cin_load_config(&cp[0],cin_waveform_config);		//Load FCCD clock configuration
	sleep(3);
/*	
	cin_load_config(&cp[0],cin_fcric_config);		//Load CIN fcric Configuration
	sleep(3);
	
	cin_load_config(&cp[0],cin_bias_config);		//Load FCCD bias Configuration
	sleep(3);
*/
/**********************************************************************/		
	fprintf(stdout,"\nCIN startup complete!!\n");

	if (ret_fpga==0){fprintf(stdout,"  *FPGA Status: OK\n");}
	else{fprintf(stdout,"  *FPGA Status: ERROR\n");}
	
	if (ret_fclk==0){fprintf(stdout,"  *FCLK Status: OK\n");}
	else{fprintf(stdout,"  *FCLK Status: ERROR\n");}	

	return 0;				
}
