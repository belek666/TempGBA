/* gameplaySP
 *
 * Copyright (C) 2006 Exophase <exophase@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "common.h"
#include <sys/time.h>
#include <stdlib.h>

TimerType timer[4];

frameskip_type current_frameskip_type = auto_frameskip;
u32 frameskip_value = 4;
u32 random_skip = 0;
u32 global_cycles_per_instruction = 3;
u32 skip_next_frame = 0;

u32 frameskip_counter = 0;

u32 cpu_ticks_ = 0;
u32 frame_ticks = 0;

u32 execute_cycles = 960;
s32 video_count = 960;
u32 ticks;

u32 irq_ticks = 0;
u8 cpu_init_state = 0;

u32 dma_cycle_count = 0;

u32 arm_frame = 0;
u32 thumb_frame = 0;
u32 last_frame = 0;

u32 synchronize_flag = 1;

#define check_count(count_var)                                                \
	if(count_var < execute_cycles)                                            \
		execute_cycles = count_var;                                           \

#define check_timer(timer_number)                                             \
	if(timer[timer_number].status == TIMER_PRESCALE)                          \
		check_count(timer[timer_number].count);                               \

#define update_timer(timer_number)                                            \
  if(timer[timer_number].status != TIMER_INACTIVE)                            \
  {                                                                           \
    if(timer[timer_number].status != TIMER_CASCADE)                           \
    {                                                                         \
      timer[timer_number].count -= execute_cycles;                            \
    }                                                                         \
                                                                              \
    if(timer[timer_number].count <= 0)                                        \
    {                                                                         \
      if(timer[timer_number].irq == TIMER_TRIGGER_IRQ)                        \
        irq_raised |= IRQ_TIMER##timer_number;                                \
                                                                              \
      if((timer_number != 3) &&                                               \
       (timer[timer_number + 1].status == TIMER_CASCADE))                     \
      {                                                                       \
        timer[timer_number + 1].count--;                                      \
      }                                                                       \
                                                                              \
      u32 timer_reload = timer[timer_number].reload << timer[timer_number].prescale; \
                                                                              \
      if(timer_number < 2)                                                    \
      {                                                                       \
        if(timer[timer_number].direct_sound_channels & 0x01)                  \
          sound_timer(timer[timer_number].frequency_step, 0);                 \
                                                                              \
        if(timer[timer_number].direct_sound_channels & 0x02)                  \
          sound_timer(timer[timer_number].frequency_step, 1);                 \
      }                                                                       \
                                                                              \
      if (timer[timer_number].reload_update != 0)                             \
      {                                                                       \
        SOUND_UPDATE_FREQUENCY_STEP(timer_number);                            \
        timer[timer_number].reload_update = 0;                                \
      }                                                                       \
                                                                              \
      timer[timer_number].count += timer_reload;                              \
    }                                                                         \
                                                                              \
    io_registers[REG_TM##timer_number##D] =                                   \
      -(timer[timer_number].count >> timer[timer_number].prescale);           \
  }                                                                           \
                                                                              \
  if (timer[timer_number].control_update != 0)                                \
  {                                                                           \
    timer_control(timer_number, timer[timer_number].control_value);           \
    timer[timer_number].control_update = 0;                                   \
  }                                                                           \

char *file_ext[] = { ".gba", ".bin", ".zip", NULL };

static bool caches_inited = false;

void init_main()
{
	u32 i;

	skip_next_frame = 0;

	for(i = 0; i < 4; i++)
	{
		memset(&dma[i], 0, sizeof(DmaTransferType));

		dma[i].start_type = DMA_INACTIVE;
		dma[i].direct_sound_channel = DMA_NO_DIRECT_SOUND;

		memset(&timer[i], 0, sizeof(TimerType));

		timer[i].status = TIMER_INACTIVE;
		timer[i].reload = 0x10000;
		timer[i].direct_sound_channels = TIMER_DS_CHANNEL_NONE;
	}

	cpu_ticks_ = 0;
	frame_ticks = 0;

	execute_cycles = ResolveSetting(BootFromBIOS, PerGameBootFromBIOS) ? 960 : 272;
	video_count = execute_cycles;

	dma_cycle_count = 0;

	irq_ticks = 0;
	cpu_init_state = 0;

	if (!caches_inited)
	{
		flush_translation_cache(TRANSLATION_REGION_READONLY, FLUSH_REASON_INITIALIZING);
		flush_translation_cache(TRANSLATION_REGION_WRITABLE, FLUSH_REASON_INITIALIZING);
	}
	else
	{
		flush_translation_cache(TRANSLATION_REGION_READONLY, FLUSH_REASON_LOADING_ROM);
		clear_metadata_area(METADATA_AREA_EWRAM, CLEAR_REASON_LOADING_ROM);
		clear_metadata_area(METADATA_AREA_IWRAM, CLEAR_REASON_LOADING_ROM);
		clear_metadata_area(METADATA_AREA_VRAM, CLEAR_REASON_LOADING_ROM);
	}

	caches_inited = true;

	StatsInitGame();
}

int main(int argc, char *argv[])
{
	char load_filename[MAX_PATH];
	char file[MAX_PATH];

	ps2init();
	init_video();
	init_input();
	init_audio();

#ifdef HOST
	getcwd(main_path, MAX_PATH);
#else
	if(!ps2GetMainPath(main_path, (char *)argv[0]))
	{
		ShowErrorScreen("Wrong Path: %s \n", main_path);
		error_quit();
	}
#endif

	ReGBA_LoadSettings("global_config", false);
	reload_video();

	ReGBA_ProgressInitialise(FILE_ACTION_LOAD_BIOS);
	sprintf(file, "%s/gba_bios.bin", main_path);
	ReGBA_ProgressUpdate(1, 2);
	
	if(load_bios(file) == -1)
	{
		ReGBA_ProgressUpdate(2, 2);
		ReGBA_ProgressFinalise();

		ShowErrorScreen("The GBA BIOS was not found in location: "
			"\n%s\n The file needs "
			"to be named gba_bios.bin.", main_path);

		error_quit();
	}
	else
	{
		ReGBA_ProgressUpdate(2, 2);
		ReGBA_ProgressFinalise();
	}

	init_main();
	init_sound();

#ifdef HOST
	argc = 2;
	argv[1] = "host:rom/rom.gba";
#endif

	if(argc > 1)
	{
		if(load_gamepak(argv[1]) == -1)
		{
			if(errno != 0)
				ShowErrorScreen("Loading ROM failed: %s", strerror(errno));
			else
				ShowErrorScreen("Loading ROM failed: File format invalid");
			
			//error_quit();
			goto loadrom;
		}

		if (IsGameLoaded)
		{
			char FileNameNoExt[MAX_PATH + 1];
			GetFileNameNoExtension(FileNameNoExt, CurrentGamePath);
			ReGBA_LoadSettings(FileNameNoExt, true);
		}

		init_cpu(ResolveSetting(BootFromBIOS, PerGameBootFromBIOS) /* in port.c */);
	}
	else
	{
		printf("No ROM was specified was on the command line.\n");
			
loadrom:
	SetMenuResolution();
	
		if(load_file(file_ext, load_filename) == -1)
		{
		if(ReGBA_Menu(REGBA_MENU_ENTRY_REASON_NO_ROM))
		goto loadrom;
		}
		else
		{
			if(load_gamepak(load_filename) == -1)
			{
				ShowErrorScreen("Failed to load gamepak %s, exiting.\n", load_filename);
				ps2delay(5);
				goto loadrom;
			}

			init_cpu(ResolveSetting(BootFromBIOS, PerGameBootFromBIOS) /* in port.c */);
		}
	}

	// We'll never actually return from here.
	SetGameResolution();

	execute_arm_translate(execute_cycles);
	return 0;
}

static void timer_control(u8 timer_number, u32 value)
{
	TimerType *tm = timer + timer_number;

	if ((value & 0x80) != 0)
	{
		if (tm->status == TIMER_INACTIVE)
		{
			if ((value & 0x04) != 0)
			{
				tm->status = TIMER_CASCADE;
				tm->prescale = 0;
			}
			else
			{
				tm->status = TIMER_PRESCALE;
				tm->prescale = timer_prescale_table[value & 0x03];
			}

			tm->irq = (value >> 6) & 0x01;

			u32 timer_reload = tm->reload;

			io_registers[REG_TM0D + (timer_number << 1)] = 0x10000 - timer_reload;

			timer_reload <<= tm->prescale;
			tm->count = timer_reload;

			if (timer_number < 2)
			{
				tm->frequency_step = FLOAT_TO_FP08_24((SYS_CLOCK / SOUND_FREQUENCY) / timer_reload);
				tm->reload_update = 0;

				if ((tm->direct_sound_channels & 0x01) != 0)
					adjust_direct_sound_buffer(0, cpu_ticks_ + timer_reload);

				if ((tm->direct_sound_channels & 0x02) != 0)
					adjust_direct_sound_buffer(1, cpu_ticks_ + timer_reload);
			}
		}
	}
	else
	{
		tm->status = TIMER_INACTIVE;
	}
}

#define SOUND_CLOCK_TICKS (167772)	// 1/100 second
int sound_ticks = SOUND_CLOCK_TICKS;

u32 update_gba()
{
	IRQ_TYPE irq_raised = IRQ_NONE;

	do
	{
		cpu_dma_hack = 0;

		during_dma_transfer_loop:

		cpu_ticks_ += execute_cycles;

		sound_ticks -= execute_cycles;

		if (sound_ticks <= 0)
		{
			update_gbc_sound(cpu_ticks_);
			sound_ticks += SOUND_CLOCK_TICKS;
		}

		update_timer(0);
		update_timer(1);
		update_timer(2);
		update_timer(3);

		video_count -= execute_cycles;

		if(video_count <= 0)
		{
			u32 vcount = io_registers[REG_VCOUNT];
			u32 dispstat = io_registers[REG_DISPSTAT];

			if((dispstat & 0x02) == 0)
			{
				// Transition from hrefresh to hblank
				video_count += (272);
				dispstat |= 0x02;

				if((dispstat & 0x01) == 0)
				{
					u32 i;

					update_scanline();

					// If in visible area also fire HDMA
					for(i = 0; i < 4; i++)
					{
						if(dma[i].start_type == DMA_START_HBLANK)
							dma_transfer(dma + i);
					}
				}

				if(dispstat & 0x10)
					irq_raised |= IRQ_HBLANK;
			}
			else
			{
				// Transition from hblank to next line
				video_count += 960;
				dispstat &= ~0x02;

				vcount++;

				if(vcount == 160)
				{
					// Transition from vrefresh to vblank
					u32 i;

					dispstat |= 0x01;

					if(update_input())
						continue;

					if(dispstat & 0x8)
					{
						irq_raised |= IRQ_VBLANK;
					}

					affine_reference_x[0] =
					 (s32)(ADDRESS32(io_registers, 0x28) << 4) >> 4;
					affine_reference_y[0] =
					 (s32)(ADDRESS32(io_registers, 0x2C) << 4) >> 4;
					affine_reference_x[1] =
					 (s32)(ADDRESS32(io_registers, 0x38) << 4) >> 4;
					affine_reference_y[1] =
					 (s32)(ADDRESS32(io_registers, 0x3C) << 4) >> 4;

					for(i = 0; i < 4; i++)
					{
						if(dma[i].start_type == DMA_START_VBLANK)
							dma_transfer(dma + i);
					}
				}
				else

				if(vcount == 228)
				{
					// Transition from vblank to next screen
					dispstat &= ~0x01;
					frame_ticks++;
#if 0
					process_cheats();
#endif
					update_backup();

					update_gbc_sound(cpu_ticks_);
					ReGBA_AudioUpdate();


					Stats.EmulatedFrames++;
					Stats.TotalEmulatedFrames++;
					ReGBA_RenderScreen();

					vcount = 0;
				}

				if(vcount == (dispstat >> 8))
				{
					// vcount trigger
					dispstat |= 0x04;
					if(dispstat & 0x20)
					{
						irq_raised |= IRQ_VCOUNT;
					}
				}
				else
				{
					dispstat &= ~0x04;
				}

				io_registers[REG_VCOUNT] = vcount;
			}
			io_registers[REG_DISPSTAT] = dispstat;
		}

		execute_cycles = video_count;

		check_count((u32)sound_ticks);

		check_timer(0);
		check_timer(1);
		check_timer(2);
		check_timer(3);

		if (dma_cycle_count != 0)
		{
			check_count(dma_cycle_count);
			dma_cycle_count -= execute_cycles;

			goto during_dma_transfer_loop;
		}

		if (irq_raised != IRQ_NONE)
		{
			ADDRESS16(io_registers, 0x202) |= irq_raised;
			irq_raised = IRQ_NONE;
		}

		if ((io_registers[REG_IF] != 0) && GBA_IME_STATE && ARM_IRQ_STATE)
		{
			u16 irq_mask = (reg[CPU_HALT_STATE] == CPU_STOP) ? 0x3080 : 0x3FFF;

			if ((io_registers[REG_IE] & io_registers[REG_IF] & irq_mask) != 0)
			{
				if (cpu_init_state != 0)
				{
					if (irq_ticks == 0)
					{
						cpu_interrupt();
						cpu_init_state = 0;
					}
				}
				else
				{
					if (reg[CPU_HALT_STATE] == CPU_HALT)
					{
						cpu_interrupt();
					}
					else
					{
						// IRQ delay - Tsyncmax=3, Texc=3, Tirq=2, Tldm=20
						//						 Tsyncmin=2
						irq_ticks = 9;
						cpu_init_state = 1;
					}
				}
			}
		}

		if (irq_ticks != 0)
		{
			check_count(irq_ticks);
			irq_ticks -= execute_cycles;
		}

	} while(reg[CPU_HALT_STATE] != CPU_ACTIVE);

	return execute_cycles;
}

static void quit_common()
{
	if(IsGameLoaded)
		update_backup_force();

	ps2quit();
}

void quit()
{
	quit_common();
	exit(0);
}

void error_quit()
{
	quit_common();
	exit(1);
}

void reset_gba()
{
	init_main();
	init_memory();
	init_cpu(ResolveSetting(BootFromBIOS, PerGameBootFromBIOS) /* in port.c */);
	reset_sound();
}

size_t FILE_LENGTH(FILE_TAG_TYPE fp)
{
	size_t length;
	
	//should save file pos?
	length = FILE_SEEK(fp, 0, SEEK_END);
	FILE_SEEK(fp, 0, SEEK_SET);
	
	return length;
}

void delay_us(u32 us_count)
{
	//SDL_Delay(us_count / 1000); // for dingux
	// sleep(0);
}

void get_ticks_us(u64 *ticks_return)
{
	 struct timeval current_time;
	 ps2time_gettimeofday(&current_time, NULL);

	 *ticks_return =
	 (u64)current_time.tv_sec * 1000000 + current_time.tv_usec;
}

void change_ext(const char *src, char *buffer, char *extension)
{
	char *position;

	strcpy(buffer, main_path);
	strcat(buffer, "/");

	position = strrchr(src, '/');
	if (position)
	src = position+1;

	strcat(buffer, src);
	position = strrchr(buffer, '.');

	if(position)
		strcpy(position, extension);
}

// type = READ / WRITE_MEM
#define MAIN_SAVESTATE_BODY(type)															\
{																							\
	FILE_##type##_VARIABLE(g_state_buffer_ptr, cpu_ticks_);									\
	FILE_##type##_VARIABLE(g_state_buffer_ptr, execute_cycles);								\
	FILE_##type##_VARIABLE(g_state_buffer_ptr, video_count);								\
																							\
	FILE_##type##_VARIABLE(g_state_buffer_ptr, cpu_init_state);								\
	FILE_##type##_VARIABLE(g_state_buffer_ptr, irq_ticks);									\
																							\
	FILE_##type##_VARIABLE(g_state_buffer_ptr, dma_cycle_count);							\
																							\
	FILE_##type##_ARRAY(g_state_buffer_ptr, timer);											\
}																							\

void main_read_mem_savestate()
MAIN_SAVESTATE_BODY(READ_MEM);

void main_write_mem_savestate()
MAIN_SAVESTATE_BODY(WRITE_MEM);
