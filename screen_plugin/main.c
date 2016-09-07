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

#define ip_server "192.168.0.12"
#define port_server 18194
int ret;

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#include "stb_image_write.h"

#include "blit.h"

int ram_mode = 0;

#define GREEN 0x00007F00
#define BLUE 0x007F3F1F
#define PURPLE 0x007F1F7F

#define NET_INIT_SIZE 1*1024*1024

#define NONE 0
#define INFO 1
#define ERROR 2
#define DEBUG 3	


static int debugnet_initialized=0;
int SocketFD = -1;
static void *net_memory = NULL;
static SceNetInAddr vita_addr;
struct SceNetSockaddrIn stSockAddr;
int sceClibVsnprintf(char *, SceSize, const char *, va_list); 
int logLevel=INFO;

void debugNetSendData(unsigned char* buffer,int size)
{
  sceNetSendto(SocketFD, buffer, size, 0, (struct SceNetSockaddr *)&stSockAddr, sizeof stSockAddr);
}

void debugNetUDPPrintf(const char* fmt, ...)
{
  char buffer[0x800];
  va_list arg;
  va_start(arg, fmt);
  sceClibVsnprintf(buffer, sizeof(buffer), fmt, arg);
  va_end(arg);
  sceNetSendto(SocketFD, buffer, strlen(buffer), 0, (struct SceNetSockaddr *)&stSockAddr, sizeof stSockAddr);
}

void debugNetPrintf(int level, char* format, ...) 
{
	char msgbuf[0x800];
	va_list args;
	
		if (level>logLevel)
		return;
       
	va_start(args, format);
       
	sceClibVsnprintf(msgbuf,2048, format, args);
	msgbuf[2047] = 0;
	va_end(args);
	switch(level)
	{
		case INFO:
	    	debugNetUDPPrintf("[INFO]: %s",msgbuf);  
	        break;
	   	case ERROR: 
	    	debugNetUDPPrintf("[ERROR]: %s",msgbuf);
	        break;
		case DEBUG:
	        debugNetUDPPrintf("[DEBUG]: %s",msgbuf);
	        break;
		case NONE:
			break;
	    default:
		    debugNetUDPPrintf("%s",msgbuf);
       
	}
}

void debugNetSetLogLevel(int level)
{
	logLevel=level;	
}

int debugNetInit(char *serverIp, int port, int level)
{
    int ret;
    SceNetInitParam initparam;
    SceNetCtlInfo info;

    sceSysmoduleLoadModule(SCE_SYSMODULE_NET);

	debugNetSetLogLevel(level);
    if (debugnet_initialized) {
        return debugnet_initialized;
    }

    /*net initialazation code from xerpi at https://github.com/xerpi/FTPVita/blob/master/ftp.c*/
    /* Init Net */
    if (sceNetShowNetstat() == SCE_NET_ERROR_ENOTINIT) {
        net_memory = malloc(NET_INIT_SIZE);

        initparam.memory = net_memory;
        initparam.size = NET_INIT_SIZE;
        initparam.flags = 0;

        ret = sceNetInit(&initparam);
        //printf("sceNetInit(): 0x%08X\n", ret);
    } else {
        //printf("Net is already initialized.\n");
    }

    /* Init NetCtl */
    ret = sceNetCtlInit();
    //printf("sceNetCtlInit(): 0x%08X\n", ret);
   

    /* Get IP address */
    ret = sceNetCtlInetGetInfo(SCE_NETCTL_INFO_GET_IP_ADDRESS, &info);
    //printf("sceNetCtlInetGetInfo(): 0x%08X\n", ret);


    /* Save the IP of PSVita to a global variable */
    sceNetInetPton(SCE_NET_AF_INET, info.ip_address, &vita_addr);

	/* Create datagram udp socket*/
    SocketFD = sceNetSocket("debugnet_socket",
        SCE_NET_AF_INET , SCE_NET_SOCK_DGRAM, SCE_NET_IPPROTO_UDP);
   
    memset(&stSockAddr, 0, sizeof stSockAddr);

    int broadcast = 1;
    sceNetSetsockopt(SocketFD, SCE_NET_SOL_SOCKET, SCE_NET_SO_BROADCAST, &broadcast, sizeof(broadcast));
	
	/*Populate SceNetSockaddrIn structure values*/
    stSockAddr.sin_family = SCE_NET_AF_INET;
    stSockAddr.sin_port = sceNetHtons(port);
    sceNetInetPton(SCE_NET_AF_INET, serverIp, &stSockAddr.sin_addr);

	/*Show log on pc/mac side*/
	debugNetUDPPrintf("debugnet initialized\n");

	/*library debugnet initialized*/
    debugnet_initialized = 1;

    return debugnet_initialized;
}

static uint32_t current_buttons = 0, pressed_buttons = 0;
SceDisplayFrameBuf framebuf;

void _free_vita_newlib()
{
	
}

volatile int term_stubs = 0;
int dummy_thread(SceSize args, void *argp)
{
	for (;;)
	{
		if (term_stubs) sceKernelExitDeleteThread(0);
	}
}
void pauseMainThread()
{
	sceKernelChangeThreadPriority(0, 0x0);
	int i;
	term_stubs = 0;
	for (i=0;i<2;i++)
	{
		SceUID thid = sceKernelCreateThread("dummy thread", dummy_thread, 0x0, 0x40000, 0, 0, NULL);
		if (thid >= 0)
			sceKernelStartThread(thid, 0, NULL);
	}
	
	SceKernelThreadInfo main_thread;
	for(;;)
	{
		main_thread.size = sizeof(SceKernelThreadInfo);
		sceKernelGetThreadInfo(0x40010003, &main_thread);
		sceKernelChangeThreadPriority(0x40010003, 0x7F);
		if (main_thread.status == SCE_THREAD_RUNNING)
		{
			term_stubs = 1;
			sceKernelDelayThread(1000);
			term_stubs = 0;
			for (i=0;i<2;i++)
			{
				SceUID thid = sceKernelCreateThread("dummy thread", dummy_thread, 0x0, 0x40000, 0, 0, NULL);
				if (thid >= 0)
					sceKernelStartThread(thid, 0, NULL);
			}
		}else break;
	}
}
void resumeMainThread()
{
	term_stubs = 1;
	sceKernelChangeThreadPriority(0, 0x40);
	SceKernelThreadInfo main_thread;
	main_thread.size = sizeof(SceKernelThreadInfo);
	sceKernelGetThreadInfo(0x40010003, &main_thread);
	sceKernelChangeThreadPriority(0x40010003, main_thread.initPriority);
}

int holdButtons(SceCtrlData *pad, uint32_t buttons, uint64_t time)
 {
	if ((pad->buttons & buttons) == buttons)
	{
		uint64_t time_start = sceKernelGetProcessTimeWide();

		while ((pad->buttons & buttons) == buttons)
		{
			sceKernelDelayThread(10 * 1000);
			sceCtrlPeekBufferPositive(0, pad, 1);

			pressed_buttons = pad->buttons & ~current_buttons;
			current_buttons = pad->buttons;

			if ((sceKernelGetProcessTimeWide() - time_start) >= time) {
				return 1;
			}
		}
	}

	return 0;
}

unsigned char temp[65536];
int tempCounter = 0;

void stbi_write_func_test(void *context, void *data, int size)
{
	SceUID _filBuf = (SceUID)context;
	
	int i;
	for(i = 0;i < size;i++)
	{
		unsigned char* dat = (unsigned char*)data;
		temp[tempCounter] = dat[i];
		tempCounter++;
		
		if(tempCounter >= 65536)
		{
			if (_filBuf >= 0)
			{
				sceIoWrite(_filBuf, temp, 65536);
			}
			
			tempCounter = 0;
		}
	}	
}

int screenshots_thread(SceSize args, void *argp)
{
	sceKernelDelayThread(5 * 1000 * 1000);

	int menu_open = 0;
	int screen = 0;
	
	sceIoMkdir("ux0:/data/screenshots", 0777);
	
	// Attaching game main thread
	SceKernelThreadInfo status;
	status.size = sizeof(SceKernelThreadInfo);
	sceKernelGetThreadInfo(0x40010003, &status);
	
	// Getting title info
	char titleid[16], title[256];
	sceAppMgrAppParamGetString(0, 9, title , 256);
	sceAppMgrAppParamGetString(0, 12, titleid , 256);
	
	ret = debugNetInit(ip_server, port_server, DEBUG);
	
	while (1)
	{
		SceCtrlData pad;
		memset(&pad, 0, sizeof(SceCtrlData));
		sceCtrlPeekBufferPositive(0, &pad, 1);

		pressed_buttons = pad.buttons & ~current_buttons;
		current_buttons = pad.buttons;

		if (!menu_open && holdButtons(&pad, SCE_CTRL_SELECT, 1 * 1000 * 1000))
		{
			menu_open = 1;
		}
		else if (menu_open)
		{
			if (pressed_buttons & SCE_CTRL_SELECT)
				menu_open = 0;

			if ((pad.buttons & SCE_CTRL_UP) && (pad.buttons & SCE_CTRL_LTRIGGER) && (pad.buttons & SCE_CTRL_RTRIGGER))
			{
				memset(&framebuf, 0x00, sizeof(SceDisplayFrameBuf));
				framebuf.size = sizeof(SceDisplayFrameBuf);
				
				sceDisplayWaitVblankStart();
				screen = sceDisplayGetFrameBuf(&framebuf, 1);
				
				if (screen == 0)
				{
					pauseMainThread();

					SceDateTime time;
					sceRtcGetCurrentClockLocalTime(&time);
					char filename[256];
					
					sprintf(filename,"ux0:/data/screenshots/%s_%d%d_%d%d%d.tga",titleid,time.month,time.day,time.hour,time.minute,time.second);
					
					debugNetPrintf(DEBUG,filename,ret);
					
					SceUID _filBuf = sceIoOpen(filename, SCE_O_WRONLY | SCE_O_CREAT, 0777);					

					stbi_write_tga_to_func(stbi_write_func_test, _filBuf, framebuf.pitch, framebuf.height, 4, framebuf.base);
					
					if(tempCounter >= 0)
					{
						if (_filBuf >= 0)
						{
							sceIoWrite(_filBuf, temp, tempCounter);
						}
						
						tempCounter = 0;
					}
			
					if (_filBuf >= 0)
					{
						sceIoClose(_filBuf);
					}

					screen = 1;
					
					resumeMainThread();
				}

			}

			blit_setup();
			blit_set_color(0x00FFFFFF, 0x00007F00);
			blit_stringf(336, 128, "Screenshot Plugin");			
		}

		//sceDisplayWaitVblankStart();
	}

	return 0;
}

int _start(SceSize args, void *argp) 
{
	SceUID thid = sceKernelCreateThread("screenshots_thread", screenshots_thread, 0x40, 0x600000, 0, 0, NULL);
	if (thid >= 0)
		sceKernelStartThread(thid, 0, NULL);

	return 0;
}