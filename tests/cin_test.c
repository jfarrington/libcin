#include <unistd.h> /* for sleep() */
#include "../cin.h"


int main() {
    struct cin_port cp;

    /* use default CIN control-port IP addr and IP port */
    cin_init_ctl_port(&cp, 0, 0);
    
    cin_power_down(&cp);
    sleep(1);
    cin_power_up(&cp);
    sleep(4);

    cin_report_power_status(&cp);
    sleep(1);

    cin_shutdown(&cp);
    return 0;
}
