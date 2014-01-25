#include <stdio.h>
#include <unistd.h> /* for sleep() */

#include "cin.h"

char fccd_config_dir[]="/home/jaimef/Desktop/FCCD Software Development/FCCD Qt/CINController_BNL_v0.1/config/Startup/";

char fpga_configfile[]="top_frame_fpga_r1004.bit";
char cin_configfile_waveform[]="waveform_10ms_readout_timing_125MHz_frameStore.txt";
char cin_configfile_fcric[]="ARRA_fcrics_config_x8_11112011.txt";
char cin_configfile_bias[]="bias_setting_lbl_gold2.txt";

int main (){
	
	struct cin_port cp[2];
	
	cin_init_ctl_port(&cp[0], 0, 0);
	cin_init_ctl_port(&cp[1], 0,CIN_DATA_CTL_PORT);
	
	char cin_fpga_config[1024];
	char cin_waveform_config[1024];
	char cin_fcric_config[1024];
	char cin_bias_config[1024];

	sprintf(cin_fpga_config,"%s%s", fccd_config_dir,fpga_configfile);
	sprintf(cin_waveform_config,"%s%s", fccd_config_dir,cin_configfile_waveform);
	sprintf(cin_fcric_config,"%s%s", fccd_config_dir,cin_configfile_fcric);
	sprintf(cin_bias_config,"%s%s", fccd_config_dir,cin_configfile_bias);

	cin_off(&cp[0]);        										//Power OFF CIN
	sleep(1);

	cin_on(&cp[0]);          										//Power ON CIN
	sleep(4);
	
	cin_load_firmware(&cp[0],&cp[1],cin_fpga_config);	//Load CIN Firmware Configuration
	sleep(3);
	
	cin_get_cfg_fpga_status(&cp[0]);						//Get CIN FPGA status 
	sleep(1);
	
//	cin_set_fclk(&cp[0],200); 								//Set CIN clocks to 200MHz
//	sleep(1);																	//Included in latest binary
	
	cin_get_fclk_status(&cp[0]);								//Get CIN clock status 
	sleep(1);
	
	cin_fp_on(&cp[0]);      										//Power ON CIN front Panel
	sleep(2);																		//Wait to allow visual check
																																		 
/************************* FCCD Configuration **************************/	
	cin_load_config(&cp[0],cin_waveform_config);	//Load FCCD clock configuration
	sleep(3);
	
	cin_load_config(&cp[0],cin_fcric_config);			//Load CIN fcric Configuration
	sleep(3);
	
	cin_load_config(&cp[0],cin_bias_config);			//Load FCCD bias Configuration
	sleep(3);
/**********************************************************************/			
	return 0;
}

	




