/* unofficial gameplaySP kai
 *
 * Copyright (C) 2006 Exophase <exophase@gmail.com>
 * Copyright (C) 2007 takka <takka@tfact.net>
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

#ifndef MEMORY_H
#define MEMORY_H


#define SAVESTATE_SIZE  0x80000 // 512K Byte (524288 Byte)
#define SVS_HEADER_SIZE 12
extern const u8 SVS_HEADER_E[SVS_HEADER_SIZE];
extern const u8 SVS_HEADER_F[SVS_HEADER_SIZE];
#define SVS_FILE_SIZE (SAVESTATE_SIZE+SVS_HEADER_SIZE)

#define CONFIG_FILENAME "game_config.txt"

typedef enum
{
  DMA_START_IMMEDIATELY,
  DMA_START_VBLANK,
  DMA_START_HBLANK,
  DMA_START_SPECIAL,
  DMA_INACTIVE
} DMA_START_TYPE;

typedef enum
{
  DMA_16BIT,
  DMA_32BIT
} DMA_LENGTH_TYPE;

typedef enum
{
  DMA_NO_REPEAT,
  DMA_REPEAT
} DMA_REPEAT_TYPE;

typedef enum
{
  DMA_INCREMENT,
  DMA_DECREMENT,
  DMA_FIXED,
  DMA_RELOAD
} DMA_INCREMENT_TYPE;

typedef enum
{
  DMA_NO_IRQ,
  DMA_TRIGGER_IRQ
} DMA_IRQ_TYPE;

typedef enum
{
  DMA_DIRECT_SOUND_A,
  DMA_DIRECT_SOUND_B,
  DMA_NO_DIRECT_SOUND
} DMA_DS_TYPE;

typedef struct
{
  u32 dma_channel;
  u32 source_address;
  u32 dest_address;
  u32 length;
  DMA_REPEAT_TYPE repeat_type;
  DMA_DS_TYPE direct_sound_channel;
  DMA_INCREMENT_TYPE source_direction;
  DMA_INCREMENT_TYPE dest_direction;
  DMA_LENGTH_TYPE length_type;
  DMA_START_TYPE start_type;
  DMA_IRQ_TYPE irq;
} DmaTransferType;

typedef enum
{
  REG_DISPCNT     = 0x000,
  REG_DISPSTAT    = 0x002,
  REG_VCOUNT      = 0x003,
  REG_BG0CNT      = 0x004,
  REG_BG1CNT      = 0x005,
  REG_BG2CNT      = 0x006,
  REG_BG3CNT      = 0x007,
  REG_BG0HOFS     = 0x008,
  REG_BG0VOFS     = 0x009,
  REG_BG1HOFS     = 0x00A,
  REG_BG1VOFS     = 0x00B,
  REG_BG2HOFS     = 0x00C,
  REG_BG2VOFS     = 0x00D,
  REG_BG3HOFS     = 0x00E,
  REG_BG3VOFS     = 0x00F,
  REG_BG2PA       = 0x010,
  REG_BG2PB       = 0x011,
  REG_BG2PC       = 0x012,
  REG_BG2PD       = 0x013,
  REG_BG2X_L      = 0x014,
  REG_BG2X_H      = 0x015,
  REG_BG2Y_L      = 0x016,
  REG_BG2Y_H      = 0x017,
  REG_BG3PA       = 0x018,
  REG_BG3PB       = 0x019,
  REG_BG3PC       = 0x01A,
  REG_BG3PD       = 0x01B,
  REG_BG3X_L      = 0x01C,
  REG_BG3X_H      = 0x01D,
  REG_BG3Y_L      = 0x01E,
  REG_BG3Y_H      = 0x01F,
  REG_WIN0H       = 0x020,
  REG_WIN1H       = 0x021,
  REG_WIN0V       = 0x022,
  REG_WIN1V       = 0x023,
  REG_WININ       = 0x024,
  REG_WINOUT      = 0x025,
  REG_BLDCNT      = 0x028,
  REG_BLDALPHA    = 0x029,
  REG_BLDY        = 0x02A,
  REG_SOUNDCNT_X  = 0x042,
  REG_SOUNDBIAS   = 0x044,
  REG_TM0D        = 0x080,
  REG_TM0CNT      = 0x081,
  REG_TM1D        = 0x082,
  REG_TM1CNT      = 0x083,
  REG_TM2D        = 0x084,
  REG_TM2CNT      = 0x085,
  REG_TM3D        = 0x086,
  REG_TM3CNT      = 0x087,
  REG_SIOCNT      = 0x094,
  REG_SIOMLT_SEND = 0x095,
  REG_P1          = 0x098,
  REG_P1CNT       = 0x099,
  REG_RCNT        = 0x09A,
  REG_IE          = 0x100,
  REG_IF          = 0x101,
  REG_IME         = 0x104,
  REG_HALTCNT     = 0x180
} HARDWARE_REGISTER;

typedef enum
{
  TIMER_INACTIVE,
  TIMER_PRESCALE,
  TIMER_CASCADE
} TIMER_STATUS_TYPE;

typedef enum
{
  TIMER_NO_IRQ,
  TIMER_TRIGGER_IRQ
} TIMER_IRQ_TYPE;

typedef enum
{
  TIMER_DS_CHANNEL_NONE,
  TIMER_DS_CHANNEL_A,
  TIMER_DS_CHANNEL_B,
  TIMER_DS_CHANNEL_BOTH
} TIMER_DS_CHANNEL_TYPE;

typedef struct
{
  TIMER_STATUS_TYPE status;
  TIMER_IRQ_TYPE irq;

  TIMER_DS_CHANNEL_TYPE direct_sound_channels;
  FIXED08_24 frequency_step;

  s32 count;
  u32 reload;
  u32 reload_update;

  u32 prescale;

  u32 control_value;
  u32 control_update;
} TimerType;

// グローバル変数宣言

extern char gamepak_title[13];
extern char gamepak_code[5];
extern char gamepak_maker[3];
extern char CurrentGamePath[MAX_PATH];
extern bool IsGameLoaded;
extern u32 gamepak_crc32;
extern char backup_id[16];

extern u8 *gamepak_rom;
extern u32 gamepak_ram_buffer_size;
extern u32 oam_update;
extern DmaTransferType dma[4];

extern TimerType timer[4];
extern const u32 timer_prescale_table[];

extern u32 cpu_dma_hack;

extern u8 savestate_write_buffer[];
extern u8 *g_state_buffer_ptr;

struct BIOS_DATA
{
  // These need to be consecutive because separated areas break Lufia games.
  // Somehow, some code is accessing the metadata's first half as if it were
  // a "second half" of the BIOS, and having all-zeroes in that area breaks
  // everything. Also do not reorder the members, because the assembly files
  // expect "bios" to mean "bios.rom".
  u8  rom     [0x4000];
  u16 metadata[0x4000];
};

extern u16 palette_ram   [  0x200];
extern u16 oam_ram       [  0x200];
extern u16 io_registers  [  0x200];
extern u8  ewram_data    [0x40000];
extern u8  iwram_data    [ 0x8000];
extern u8  vram          [0x18000];
struct BIOS_DATA bios;
extern u8  gamepak_backup[0x20000];

extern u16 iwram_metadata[ 0x8000];
extern u16 ewram_metadata[0x40000];
extern u16 vram_metadata [0x18000];

extern u32 bios_read_protect;

extern u8 *memory_map_read[8 * 1024];
extern u32 reg[64];
extern u8 *memory_map_write[8 * 1024];

// This is the path to per-user files for ReGBA. This path does not have
// a trailing slash.
extern char main_path[MAX_PATH + 1];

extern FILE_TAG_TYPE gamepak_file_large;

extern u8 memory_waitstate[4][16];
extern u8 fetch_waitstate[4][16];

// SIO
extern u32 g_multi_mode;
extern u32 g_adhoc_transfer_flag;

// Tilt sensor on the GBA side. It's mapped... somewhere... in the GBA address
// space. See the read_backup function in memory.c for more information.
extern u32 tilt_sensor_x;
extern u32 tilt_sensor_y;

// 関数宣言

u8  read_memory8(u32 address);
u32 read_memory16(u32 address);
s16 read_memory16_signed(u32 address);
u32 read_memory32(u32 address);

u8  read_open_memory8(u32 address);
u16 read_open_memory16(u32 address);
u32 read_open_memory32(u32 address);

CPU_ALERT_TYPE write_memory8(u32 address, u8 value);
CPU_ALERT_TYPE write_memory16(u32 address, u16 value);
CPU_ALERT_TYPE write_memory32(u32 address, u32 value);

CPU_ALERT_TYPE write_io_register8(u32 address, u32 value);
CPU_ALERT_TYPE write_io_register16(u32 address, u32 value);
CPU_ALERT_TYPE write_io_register32(u32 address, u32 value);

CPU_ALERT_TYPE write_rtc(u32 address, u32 value);

u32 read_eeprom();
CPU_ALERT_TYPE write_eeprom(u32 address, u32 value);

u8 read_backup(u32 address);
CPU_ALERT_TYPE write_backup(u32 address, u32 value);

CPU_ALERT_TYPE dma_transfer(DmaTransferType *dma);

s32 load_bios(char *name);
u32 load_backup();

ssize_t load_gamepak(char *file_path);
u8 *load_gamepak_page(u16 physical_index);
void init_memory();
void init_gamepak_buffer();

void update_backup();
void update_backup_force();

u32 load_state(uint32_t SlotNumber);
u32 save_state(uint32_t SlotNumber, u16 *screen_capture);
void init_rewind(void);
void savestate_rewind(void);
void loadstate_rewind(void);

extern unsigned int rewind_queue_len;

#define GBA_IME_STATE  (io_registers[REG_IME] != 0)

#define pMEMORY_WS16N(resion) *(memory_waitstate[0] + (resion))
#define pMEMORY_WS16S(resion) *(memory_waitstate[2] + (resion))
#define pMEMORY_WS32N(resion) *(memory_waitstate[1] + (resion))
#define pMEMORY_WS32S(resion) *(memory_waitstate[3] + (resion))
#define pFETCH_WS16N(resion)  *(fetch_waitstate[0]  + (resion))
#define pFETCH_WS16S(resion)  *(fetch_waitstate[2]  + (resion))
#define pFETCH_WS32N(resion)  *(fetch_waitstate[1]  + (resion))
#define pFETCH_WS32S(resion)  *(fetch_waitstate[3]  + (resion))

#endif
