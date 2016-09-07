/*
	Vitamin
	Copyright (C) 2016, Team FreeK (TheFloW, Major Tom, mr. gas)

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <psp2/ctrl.h>
#include <psp2/power.h>
#include <psp2/io/dirent.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/appmgr.h>
#include <psp2/display.h>
#include <psp2/kernel/modulemgr.h>
#include <psp2/kernel/processmgr.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#include "stb_image_write.h"

#include "blit.h"

int ram_mode = 0;

#define GREEN 0x00007F00
#define BLUE 0x007F3F1F
#define PURPLE 0x007F1F7F

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
	
	// Getting title info
	char titleid[16], title[256];
	sceAppMgrAppParamGetString(0, 9, title , 256);
	sceAppMgrAppParamGetString(0, 12, titleid , 256);

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
		
		if (menu_open)
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

		sceDisplayWaitVblankStart();
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