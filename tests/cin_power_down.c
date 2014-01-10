#include <stdio.h>
#include <unistd.h> /* for sleep() */

#include "cin.h"

int main (){
	
	struct cin_port cp[2];
	
	cin_init_ctl_port(&cp[0], 0, 0);/* use default CIN control-port IP addr and IP port */
	cin_init_ctl_port(&cp[1], 0,CIN_DATA_CTL_PORT);

	cin_set_bias(&cp[0],0);   		//Turn OFF camera CCD bias
	sleep(1);						

	cin_set_clocks(&cp[0],0);		//Turn OFF camera CCD bias
	sleep(1);

	cin_fp_off(&cp[0]);      		//Power OFF CIN front Panel
	sleep(2);	

	cin_off(&cp[0]);          	  //Power OFF CIN
	sleep(4);

	fprintf(stdout,"Closing ports.......\n");
	cin_close_ctl_port(&cp[0]);       //Close Control port
	cin_close_ctl_port(&cp[1]); 			//Close Stream-in port
	sleep(1);

	fprintf(stdout,"CIN shutdown complete!!\n");
	return(0);
}

