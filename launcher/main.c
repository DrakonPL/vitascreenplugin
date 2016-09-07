#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <psp2/ctrl.h>
#include <psp2/power.h>
#include <psp2/io/dirent.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/sysmodule.h>
#include <psp2/appmgr.h>
#include <psp2/display.h>
#include <psp2/kernel/modulemgr.h>
#include <psp2/kernel/processmgr.h>

#include "blit.h"


int launcher_thread(SceSize args, void *argp)
{
	sceKernelDelayThread(5 * 1000 * 1000);

	// Loading net module
	sceKernelLoadStartModule("ux0:/data/amphetamin.suprx", 0, NULL, 0, NULL, NULL);
	sceKernelDelayThread(1 * 1000 * 1000); // Wait till net module did its stuffs	

	for (;;)
	{
		sceKernelDelayThread(1000); // Just let VITA scheduler do its work
	}
	
	return 0;
}

int _start(SceSize args, void *argp) 
{
	SceUID thid = sceKernelCreateThread("launcher_thread", launcher_thread, 0x40, 0x600000, 0, 0, NULL);
	if (thid >= 0)
		sceKernelStartThread(thid, 0, NULL);

	return 0;
}