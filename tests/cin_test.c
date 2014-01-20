#include <stdio.h>
#include <unistd.h> /* for sleep() */

#include "cin.h"


int main() {

/*Set directory for configuration files*/ 
  char fccd_config_dir[]="/home/jaimef/Desktop/FCCD Software Development/FCCD Qt/CINController_BNL_v0.1/config/Startup/";

/*Set CIN FPGA configuration file*/   
	char fpga_configfile[]="top_frame_fpga_r1004.bit";

/*Set CIN configuration file*/ 
	char cin_configfile_waveform[]="waveform_10ms_readout_timing_125MHz_frameStore.txt";

/*Create Path to files*/
	char cin_fpga_config[1024];
	char cin_waveform_config[1024];
	sprintf(cin_fpga_config,"%s%s", fccd_config_dir,fpga_configfile);
	sprintf(cin_waveform_config,"%s%s", fccd_config_dir,cin_configfile_waveform);

/*Set control ports*/	
	struct cin_port cp[2];
	
	cin_init_ctl_port(&cp[0], 0, 0);/* use default CIN control-port IP addr and IP port */
	cin_init_ctl_port(&cp[1], 0,CIN_DATA_CTL_PORT);/* use CIN control data port *

/*Test Functions*/	
/*  
  cin_off(&cp[0]);
	sleep(1);

  cin_on(&cp[0]);  
	sleep(4);

  cin_fp_on(&cp[0]);
	sleep(1);

 	cin_fp_off(&cp[0]);
	sleep(1);

	cin_load_firmware(&cp[1],cin_fpga_config);	//Load CIN Firmware Configuration
	sleep(3);

	cin_load_config(&cp[0],cin_waveform_config);	//Load FCCD clock configuration
	sleep(3);
*//*
*/
/*cin_set_fclk_125mhz(&cp[0]);
	sleep(1);

	cin_get_fclk_status(&cp[0]); //INCOMPLETE        
 	sleep(1);

	cin_get_cfg_fpga_status(&cp[0]);
	sleep(1);
	
	cin_get_power_status(&cp[0]);
	sleep(1);
        
	cin_set_trigger(&cp[0],3);  
	sleep(1);
	
	uint8_t status;
	status=cin_get_trigger_status (&cp[0]);
 	printf("getTriggerStatus returns:%u\n",status);
	sleep(1);

	cin_set_clocks(&cp[0],1);
	sleep(1);
        
	cin_set_clocks(&cp[0],0);
	sleep(1);
	
	cin_set_bias(&cp[0],1);
 	sleep(1);
  
  cin_set_bias(&cp[0],0);
 	sleep(1);
    
	cin_set_exposure_time(&cp[0],135.00);
	sleep(1);
	
	cin_set_trigger_delay(&cp[0],135.00);
	sleep(1);
	
	cin_set_cycle_time(&cp[0],135.00);
	sleep(1);
*/
	cin_set_frame_count_reset(&cp[0]);
/*
	cin_test_cfg_leds(&cp[0]);
  sleep(2);
*/
	//flashFrmLeds(&cp[0]);
	
	return 0;
}

