/*
Quake GameCube port.
Copyright (C) 2007 Peter Mackay

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

// Standard includes.
#include <cstdio>

// OGC includes.
#include <ogc/lwp.h>
#include <ogc/lwp_watchdog.h>
#include <ogcsys.h>
#include <wiiuse/wpad.h>
#include "input_wiimote.h"

extern "C"
{
#include "../generic/quakedef.h"
}

int want_to_reset = 0;

void reset_system(void)
{
	want_to_reset = 1;
}

// Handy switches.
#define CONSOLE_DEBUG		0
#define TIME_DEMO		0
#define USE_THREAD		1
#define TEST_CONNECTION		0
#define DISABLE_NETWORK		0

namespace quake
{
	namespace main
	{
		// Types.
		typedef u32 pixel_pair;

		// Video globals.
		pixel_pair	(*xfb)[][640]	= 0;
		GXRModeObj*	rmode			= 0;

		// Set up the heap.
		static const size_t	heap_size	= 12 * 1024 * 1024;
		static char			heap[heap_size] __attribute__((aligned(8)));

		static void init()
		{
			// Initialise the video system.
			VIDEO_Init();

			rmode = VIDEO_GetPreferredMode(NULL);

			// Allocate the frame buffer.
			xfb = static_cast<pixel_pair (*)[][640]>(MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode)));

			// Set up the video system with the chosen mode.
			VIDEO_Configure(rmode);

			// Set the frame buffer.
			VIDEO_SetNextFramebuffer(xfb);

			// Show the screen.
			VIDEO_SetBlack(FALSE);
			VIDEO_Flush();
			VIDEO_WaitVSync();
			if (rmode->viTVMode & VI_NON_INTERLACE)
			{
				VIDEO_WaitVSync();
			}

			// Initialise the debug console.
			console_init(xfb, 20, 10, rmode->fbWidth, rmode->xfbHeight, rmode->fbWidth * 2);

			// Initialise the controller library.
			PAD_Init();
			WPAD_Shutdown();
			WPAD_Init();

			//WPAD_Disconnect(WPAD_CHAN_0);
			WPAD_Disconnect(WPAD_CHAN_1);
			WPAD_Disconnect(WPAD_CHAN_2);
			WPAD_Disconnect(WPAD_CHAN_3);

			wiimote_ir_res_x = rmode->fbWidth;
			wiimote_ir_res_y = rmode->xfbHeight;

			printf("\n\n\n\n\n\nIf the Nunchuk isn't detected, please reconnect it to the wiimote.\n\
					Oh, and don't forget to put your wrist wrap! :)\n\n");
			VIDEO_WaitVSync();
			struct timespec sleeptime = {3, 0};
			nanosleep(&sleeptime);
		}

		static void check_pak_file_exists()
		{
			int handle = -1;
			if (Sys_FileOpenRead("/id1/pak0.pak", &handle) < 0)
			{
				Sys_Error(
					"/ID1/PAK0.PAK was not found.\n"
					"\n"
					"This file comes with the full or demo version of Quake\n"
					"and is necessary for the game to run.\n"
					"\n"
					"Please make sure it is on your SD card in the correct\n"
					"location.\n"
					"\n"
					"If you are absolutely sure the file is correct, your SD\n"
					"card may not be compatible with the SD card lib which\n"
					"Quake uses, or the Wii. Please check the issue tracker.");
				return;
			}
			else
			{
				Sys_FileClose(handle);
			}
		}

		static void* main_thread_function(void*)
		{
			// Initialise.
			init();
			check_pak_file_exists();

			// Initialise the Common module.
			char* args[] =
			{
				"Quake",
#if CONSOLE_DEBUG
				"-condebug",
#endif
#if DISABLED_NETWORK
				"-noudp",
#endif
			};
			COM_InitArgv(sizeof(args) / sizeof(args[0]), args);

			// Initialise the Host module.
			quakeparms_t parms;
			memset(&parms, 0, sizeof(parms));
			parms.argc		= com_argc;
			parms.argv		= com_argv;
			parms.basedir	= "";
			parms.memsize	= heap_size;
			parms.membase	= heap;
			if (parms.membase == 0)
			{
				Sys_Error("Heap allocation failed");
			}
			memset(parms.membase, 0, parms.memsize);
			Host_Init(&parms);

#if TIME_DEMO
			Cbuf_AddText("map start\n");
			Cbuf_AddText("wait\n");
			Cbuf_AddText("timedemo demo1\n");
#endif
#if TEST_CONNECTION
			Cbuf_AddText("connect 192.168.0.2");
#endif

			// Run the main loop.
			u64 last_time = gettime();
			for (;;)
			{
				if (want_to_reset)
					SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);

				// Get the frame time in ticks.
				const u64		current_time	= gettime();
				const u64		time_delta		= current_time - last_time;
				const double	seconds	= time_delta * (0.001f / TB_TIMER_CLOCK);
				last_time = current_time;

				// Run the frame.
				Host_Frame(seconds);
			};

			// Quit (this code is never reached).
			Sys_Quit();
			return 0;
		}
	}
}

using namespace quake;
using namespace quake::main;

qboolean isDedicated = qfalse;

int main(int argc, char* argv[])
{
	__STM_Init();
	SYS_SetResetCallback(reset_system);

	// Start the main thread.
	lwp_t thread;
	LWP_CreateThread(&thread, &main_thread_function, 0, 0, 256 * 1024, 1);

	// Wait for it to finish.
	void* result;
	LWP_JoinThread(thread, &result);

	exit(0);
	return 0;
}
