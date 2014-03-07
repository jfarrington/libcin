#include <stdio.h>
#include <unistd.h> /* for sleep() */
#include <string.h>
#include <stdlib.h>

#include "cin.h"

int main(int argc, char *argv[]){

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
	cin_init_ctl_port(&cp[1], 0,CIN_CTL_FRMW_PORT);/* Use CIN firmware configuration port*/
	printf("*%s*\n",argv[1]);

	if (argv[1]==NULL){
		printf("No option selected!\n\n");
		goto options;
	}	
	else if (strcmp(argv[1],"cin_on") == 0){
		cin_on(&cp[0]);
	}
	
	else if (strcmp(argv[1],"cin_off") == 0){
		int _status;
		_status=cin_off(&cp[0]);
		printf("%u",_status);
	}

	else if (strcmp(argv[1],"cin_fp_on") == 0){
		cin_fp_on(&cp[0]);
	}

	else if (strcmp(argv[1],"cin_fp_off") == 0){
		cin_fp_off(&cp[0]);
	}

	else if (strcmp(argv[1],"cin_load_config") == 0){
		cin_load_config(&cp[0],cin_waveform_config);	
	}

	else if (strcmp(argv[1],"cin_load_firmware") == 0){
		cin_load_firmware(&cp[0],&cp[1],cin_fpga_config);	
	}

	else if (strcmp(argv[1],"cin_set_fclk") == 0){
		uint16_t clkfreq;	
		clkfreq= atoi(argv[2]);
		cin_set_fclk(&cp[0],clkfreq);
	}

	else if (strcmp(argv[1],"cin_get_fclk_status") == 0){
		cin_get_fclk_status(&cp[0]);	
	}

	else if (strcmp(argv[1],"cin_get_cfg_fpga_status") == 0){
		cin_get_cfg_fpga_status(&cp[0]);    
	}

	else if (strcmp(argv[1],"cin_get_power_status") == 0){
		cin_get_power_status(&cp[0]);	
	}	

	else if (strcmp(argv[1],"cin_set_bias") == 0){
		int val; 
		val= atoi(argv[2]);
		cin_set_bias(&cp[0],val);	
	}

	else if (strcmp(argv[1],"cin_set_clocks") == 0){
		int val; 
		val= atoi(argv[2]);
		cin_set_clocks(&cp[0],val);	
	}

	else if (strcmp(argv[1],"cin_set_trigger") == 0){
		int val; 
		val = atoi(argv[2]);
		cin_set_trigger(&cp[0],val);	
	}

	else if (strcmp(argv[1],"cin_get_trigger_status") == 0){
		int val; 
		val = cin_get_trigger_status (&cp[0]);
 		printf("getTriggerStatus returns:%u\n",val);
	}

	else if (strcmp(argv[1],"cin_set_trigger_mode") == 0){
		int val;
		val = atoi(argv[2]);
		cin_set_trigger_mode(&cp[0],val);
	}
	
	else if (strcmp(argv[1],"cin_set_exposure_time") == 0){
		float val;
		sscanf(argv[2], "%f", &val);
		cin_set_exposure_time(&cp[0],val);
	}

	else if (strcmp(argv[1],"cin_set_trigger_delay") == 0){
		float val;
		sscanf(argv[2], "%f", &val); 
		cin_set_trigger_delay(&cp[0],val);
	}

	else if (strcmp(argv[1],"cin_set_cycle_time") == 0){
		float val;
		sscanf(argv[2], "%f", &val); 
		cin_set_cycle_time(&cp[0],val);
	}
	else if (strcmp(argv[1],"cin_set_frame_count_reset") == 0){
		cin_set_frame_count_reset(&cp[0]);	
	}	
	
	else if (strcmp(argv[1],"cin_test_cfg_leds") == 0){
		cin_test_cfg_leds(&cp[0]);	
	}	

	else if (strcmp(argv[1],"-h") == 0){
		goto options;
	}
	
	else{
		printf("Invalid function!\n\n");
		goto options;
	}	
	sleep(1);
	return 0;	

	options:{
		printf("Format:$./cintest (Option)\nOptions:\
		\n cin_on\
		\n cin_off\
		\n cin_fp_on\
		\n cin_fp_off\
		\n cin_load_config\
		\n cin_load_firmware\
		\n cin_set_fclk (uint16_t clkfreq) ;clkfreq={125, 200 and 250}(MHz)\
		\n cin_get_fclk_status\
		\n cin_get_cfg_fpga_status\
		\n cin_get_power_status\
		\n cin_set_bias (int val)    ;val={1-ON,0-OFF}\
		\n cin_set_clocks (int val)  ;val={1-ON,0-OFF}\
		\n cin_set_trigger (int val) ;val={0-Int,1-Ext1,2-Ext2,3-Ext 1 or 2}\
		\n cin_get_trigger_status\
		\n cin_set_trigger_mode      ;val={0-Stop,1-Continuous}\
		\n cin_set_exposure_time (float e_time)   ;(ms)\
		\n cin_set_trigger_delay (float d_time)  	;(us)\
		\n cin_set_cycle_time (float c_time)      ;(ms)\
		\n cin_set_frame_count_reset\
		\n cin_test_cfg_leds\n\n");
	}
			
}
