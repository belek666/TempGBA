/* unofficial gameplaySP kai
 *
 * Copyright (C) 2006 Exophase <exophase@gmail.com>
 * Copyright (C) 2007 takka <takka@tfact.net>
 * Copyright (C) 2007 NJ
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "common.h"

u32 skip_next_frame_flag = 0;
u32 frameskip_0_hack_flag = 0;

u32 cpu_ticks = 0;
u32 frame_ticks = 0;

u32 execute_cycles = 960;
s32 video_count = 960;

u32 irq_ticks = 0;
u8 cpu_init_state = 0;

u32 dma_cycle_count = 0;

unsigned int pen = 0;
unsigned int frame_interval = 60; // For in-memory saved states used in rewinding

u32 to_skip= 0;

char *file_ext[] = { ".gba", ".bin", ".zip", NULL };

static u8 caches_inited = 0;

void init_main()
{
  u32 i;

  skip_next_frame_flag = 0;
  frameskip_0_hack_flag = 0;

  for (i = 0; i < 4; i++)
  {
    memset(&dma[i], 0, sizeof(DmaTransferType));

    dma[i].start_type = DMA_INACTIVE;
    dma[i].direct_sound_channel = DMA_NO_DIRECT_SOUND;

    memset(&timer[i], 0, sizeof(TimerType));

    timer[i].status = TIMER_INACTIVE;
    timer[i].reload = 0x10000;
    timer[i].direct_sound_channels = TIMER_DS_CHANNEL_NONE;
  }

  cpu_ticks = 0;
  frame_ticks = 0;

  execute_cycles = gpsp_persistent_config.BootFromBIOS ? 960 : 272;
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

  caches_inited = 1;

  StatsInitGame();
}

void quit(void)
{
/*
  u32 reg_ra;

  __asm__ __volatile__("or %0, $0, $ra"
                        : "=r" (reg_ra)
                        :);

  dbg_printf("return address= %08x\n", reg_ra);
*/

#ifdef USE_DEBUG
	fclose(g_dbg_file);
#endif

	ds2_plug_exit();
	while(1);
}

int gpsp_main(int argc, char *argv[])
{
	char load_filename[MAX_FILE];

	if(gui_init(0) < 0)
		quit();
	// Initialidse paths
	initial_gpsp_config();

    init_video();

	// 初始化
	init_game_config();

	init_main();
	init_sound();

	// BIOS的读入
	char bios_filename[MAX_FILE];
	sprintf(bios_filename, "%s/%s", main_path, "gba_bios.bin");
	u32 bios_ret = load_bios(bios_filename);
	if(bios_ret == -1) // 当读取失败
	{
		err_msg(DOWN_SCREEN, "The GBA BIOS is not present\nPlease see README.md for\nmore information\n\nLe BIOS GBA n'est pas present\nLisez README.md pour plus\nd'information (en anglais)");
		ds2_flipScreen(DOWN_SCREEN, DOWN_SCREEN_UPDATE_METHOD);
		wait_Anykey_press(0);
		quit();
	}

	init_cpu(gpsp_persistent_config.BootFromBIOS);
	init_memory();
	reset_sound();

	ReGBA_Menu(REGBA_MENU_ENTRY_REASON_NO_ROM);

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
          adjust_direct_sound_buffer(0, cpu_ticks + timer_reload);

        if ((tm->direct_sound_channels & 0x02) != 0)
          adjust_direct_sound_buffer(1, cpu_ticks + timer_reload);
      }
    }
  }
  else
  {
    tm->status = TIMER_INACTIVE;
  }
}

#define CHECK_COUNT(count_var)                                                \
  if ((count_var) < execute_cycles)                                           \
  {                                                                           \
    execute_cycles = count_var;                                               \
  }                                                                           \

#define CHECK_TIMER(timer_number)                                             \
  if (timer[timer_number].status == TIMER_PRESCALE)                           \
  {                                                                           \
    CHECK_COUNT(timer[timer_number].count);                                   \
  }                                                                           \

#define UPDATE_TIMER(timer_number)                                            \
{                                                                             \
  TimerType *tm = timer + timer_number;                                       \
                                                                              \
  if (tm->status != TIMER_INACTIVE)                                           \
  {                                                                           \
    if (tm->status != TIMER_CASCADE)                                          \
    {                                                                         \
      tm->count -= execute_cycles;                                            \
    }                                                                         \
                                                                              \
    if (tm->count <= 0)                                                       \
    {                                                                         \
      if (tm->irq == TIMER_TRIGGER_IRQ)                                       \
      {                                                                       \
        irq_raised |= IRQ_TIMER##timer_number;                                \
      }                                                                       \
                                                                              \
      if (timer_number != 3)                                                  \
      {                                                                       \
        if (tm[1].status == TIMER_CASCADE)                                    \
          tm[1].count--;                                                      \
      }                                                                       \
                                                                              \
      u32 timer_reload = tm->reload << tm->prescale;                          \
                                                                              \
      if (timer_number < 2)                                                   \
      {                                                                       \
        if ((tm->direct_sound_channels & 0x01) != 0)                          \
          sound_timer(tm->frequency_step, 0);                                 \
                                                                              \
        if ((tm->direct_sound_channels & 0x02) != 0)                          \
          sound_timer(tm->frequency_step, 1);                                 \
                                                                              \
        if (tm->reload_update != 0)                                           \
        {                                                                     \
          tm->frequency_step = FLOAT_TO_FP08_24((SYS_CLOCK / SOUND_FREQUENCY) / timer_reload); \
          tm->reload_update = 0;                                              \
        }                                                                     \
     }                                                                        \
                                                                              \
      tm->count += timer_reload;                                              \
    }                                                                         \
                                                                              \
    io_registers[REG_TM##timer_number##D] = 0x10000 - (tm->count >> tm->prescale); \
  }                                                                           \
                                                                              \
  if (tm->control_update != 0)                                                \
  {                                                                           \
    timer_control(timer_number, tm->control_value);                           \
    tm->control_update = 0;                                                   \
  }                                                                           \
}                                                                             \

#define START_DMA_TRANSFER(channel, start_timing)                             \
  if (dma[channel].start_type == DMA_START_##start_timing)                    \
  {                                                                           \
    dma_transfer(dma + channel);                                              \
  }                                                                           \


#define SOUND_CLOCK_TICKS (167772)  // 1/100 second
int sound_ticks = SOUND_CLOCK_TICKS;

u32 update_gba(void)
{
  s32 i;
  IRQ_TYPE irq_raised = IRQ_NONE;

  do
  {
    cpu_dma_hack = 0;

    during_dma_transfer_loop:

    cpu_ticks += execute_cycles;

    sound_ticks -= execute_cycles;

    if (sound_ticks <= 0)
    {
      update_gbc_sound(cpu_ticks);
      sound_ticks += SOUND_CLOCK_TICKS;
    }

    UPDATE_TIMER(0);
    UPDATE_TIMER(1);
    UPDATE_TIMER(2);
    UPDATE_TIMER(3);

    video_count -= execute_cycles;

    if (video_count <= 0)
    {
      u32 vcount = io_registers[REG_VCOUNT];
      u32 dispstat = io_registers[REG_DISPSTAT];

      if (!(dispstat & 0x02))
      {
        // Transition from hrefresh to hblank
        video_count += 272;
        dispstat |= 0x02;

        if (!(dispstat & 0x01))
        {
          update_scanline();

          // If in visible area also fire HDMA
          for (i = 0; i < 4; i++)
          {
            START_DMA_TRANSFER(i, HBLANK);
          }
        }

        // H-blank interrupts do occur during v-blank (unlike hdma, which does not)
        if ((dispstat & 0x10) != 0)
          irq_raised |= IRQ_HBLANK;
      }
      else
      {
        // Transition from hblank to next line
        video_count += 960;
        dispstat &= ~0x02;

        vcount++;

        if (vcount == 160)
        {
          // Transition from vrefresh to vblank
          dispstat |= 0x01;

          if (update_input() != 0)
            continue;

          affine_reference_x[0] = (s32)(ADDRESS32(io_registers, 0x28) << 4) >> 4;
          affine_reference_y[0] = (s32)(ADDRESS32(io_registers, 0x2C) << 4) >> 4;
          affine_reference_x[1] = (s32)(ADDRESS32(io_registers, 0x38) << 4) >> 4;
          affine_reference_y[1] = (s32)(ADDRESS32(io_registers, 0x3C) << 4) >> 4;

          for (i = 0; i < 4; i++)
          {
            START_DMA_TRANSFER(i, VBLANK);
          }

          if ((dispstat & 0x08) != 0)
            irq_raised |= IRQ_VBLANK;
       }
       else if (vcount == 228)
       {
          // Transition from vblank to next screen
          dispstat &= ~0x01;
          frame_ticks++;
			if(frame_ticks >= frame_interval)
				frame_ticks = 0;

			if(game_config.backward)
			{
				if(fast_backward) // Rewinding requested
				{
					fast_backward = 0;
					if(rewind_queue_len > 0)
					{
						if(frame_ticks > 3)
						{
							if(pen)
								mdelay(500);

							loadstate_rewind();
							pen = 1;
							frame_ticks = 0;
							continue;
						}
					}
					else if(frame_ticks > 3)
					{
						u32 HotkeyRewind = game_persistent_config.HotkeyRewind != 0 ? game_persistent_config.HotkeyRewind : gpsp_persistent_config.HotkeyRewind;

						struct key_buf inputdata;
						ds2_getrawInput(&inputdata);

						while (inputdata.key & HotkeyRewind)
						{
							ds2_getrawInput(&inputdata);
						}
					}
				}
				else if(frame_ticks ==0)
				{
					savestate_rewind();
				}
			}

          if (gpsp_persistent_config.UpdateBackup != 0)
            update_backup();

          process_cheats();

          update_gbc_sound(cpu_ticks);
          ReGBA_AudioUpdate();

          vcount = 0;

          Stats.EmulatedFrames++;
          Stats.TotalEmulatedFrames++;
          ReGBA_RenderScreen();

//printf("SKIP_RATE %d %d\n", SKIP_RATE, to_skip);
        } //(vcount == 228)

        if (vcount == (dispstat >> 8))
        {
          // vcount trigger
          dispstat |= 0x04;

          if ((dispstat & 0x20) != 0)
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

    CHECK_COUNT((u32)sound_ticks);

    for (i = 0; i < 4; i++)
    {
      CHECK_TIMER(i);
    }

    if (dma_cycle_count != 0)
    {
      CHECK_COUNT(dma_cycle_count);
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
            //             Tsyncmin=2
            irq_ticks = 9;
            cpu_init_state = 1;
          }
        }
      }
    }

    if (irq_ticks != 0)
    {
      CHECK_COUNT(irq_ticks);
      irq_ticks -= execute_cycles;
    }
  }
  while(reg[CPU_HALT_STATE] != CPU_ACTIVE);

  return execute_cycles;
}


void reset_gba(void)
{
  init_main();
  init_memory();
  init_cpu(gpsp_persistent_config.BootFromBIOS);
  reset_sound();
}

void change_ext(char *src, char *buffer, char *extension)
{
  char *dot_position;
  strcpy(buffer, src);
  dot_position = strrchr(buffer, '.');

  if(dot_position)
    strcpy(dot_position, extension);
}

// type = READ / WRITE_MEM
#define MAIN_SAVESTATE_BODY(type)                                             \
{                                                                             \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, cpu_ticks);                      \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, execute_cycles);                 \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, video_count);                    \
                                                                              \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, cpu_init_state);                 \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, irq_ticks);                      \
                                                                              \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, dma_cycle_count);                \
                                                                              \
  FILE_##type##_ARRAY(g_state_buffer_ptr, timer);                             \
}                                                                             \

void main_read_mem_savestate(void)
MAIN_SAVESTATE_BODY(READ_MEM);

void main_write_mem_savestate(void)
MAIN_SAVESTATE_BODY(WRITE_MEM);

void error_msg(char *text)
{
    gui_action_type gui_action = CURSOR_NONE;

    printf(text);

    while(gui_action == CURSOR_NONE)
    {
      gui_action = get_gui_input();
//      sceKernelDelayThread(15000); /* 0.0015s */
    }
}


char* FS_FGets(char *buffer, int num, FILE_TAG_TYPE stream)
{
	int m;
	char *s;

//    printf("In fgets\n");

	if(num <= 0)
		return (NULL);

	num--;
	m= fread(buffer, 1, num, stream);
	*(buffer +m) = '\0';

//    printf("fread= %s\n", buffer);

    if(m == 0)
      return (NULL);

	s = strchr(buffer, '\n');

	if(m < num)						//at the end of file
	{
		if(s == NULL)
			return (buffer);

		*(++s)= '\0';				//string include '\n'
		m -= s - buffer;
		fseek(stream, -m, SEEK_CUR);//fix fread pointer
		return (buffer);
	}
	else
	{
		if(s)
		{
			*(++s)= '\0';				//string include '\n'
			m -= s - buffer;
			fseek(stream, -m, SEEK_CUR);//fix fread pointer
		}

		return (buffer);
	}
}
