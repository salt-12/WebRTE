// golden
// 6/12/2018
//

#include <ps4.h>
#include "server.h"

int _main(void) {
    initKernel();
    initLibc();
    initPthread();
    initNetwork();
    initSysUtil();

    sceKernelSleep(1);

    // just a little notify
    sceSysUtilSendSystemNotificationWithText(222, "PS4 Trainer");
    sceSysUtilSendSystemNotificationWithText(222, "Visit ps4trainer.com then sit back, relax, and enjoy some mods!");
    
    // jailbreak current thread
    sys_console_cmd(SYS_CONSOLE_CMD_JAILBREAK, NULL);

    // start the server, this will block
    return start_http_server();
}