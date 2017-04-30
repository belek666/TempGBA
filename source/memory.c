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

#include "common.h" 

u8 savestate_write_buffer[SAVESTATE_SIZE];
u8 *g_state_buffer_ptr;

#define SAVESTATE_REWIND_SIZE (SAVESTATE_REWIND_LEN*SAVESTATE_REWIND_NUM)	//~5MB
#define SAVESTATE_REWIND_LEN (0x69040)
#define SAVESTATE_REWIND_NUM (10)
u8 SAVESTATE_REWIND_MEM[ SAVESTATE_REWIND_SIZE ] __attribute__ ((aligned (4))) ;

const u8 SVS_HEADER_E[SVS_HEADER_SIZE] = {'R','E', 'G', 'B', 'A', 'R', 'T', 'S', '0', '.', '1', 'e'};
const u8 SVS_HEADER_F[SVS_HEADER_SIZE] = {'R','E', 'G', 'B', 'A', 'R', 'T', 'S', '0', '.', '1', 'f'};

void memory_read_mem_savestate();
void memory_write_mem_savestate();


u8 ALIGN_DATA memory_waitstate[4][16] =
{
  // Non-sequential
  { 0, 0, 2, 0, 0, 0, 0, 0, 4, 4, 4, 4, 4, 4, 4, 0 }, // 8,16bit accesses
  { 0, 0, 5, 0, 0, 1, 1, 1, 7, 7, 9, 9,13,13, 4, 0 }, // 32bit accesses

  // Sequential
  { 0, 0, 2, 0, 0, 0, 0, 0, 2, 2, 4, 4, 8, 8, 4, 0 },
  { 0, 0, 5, 0, 0, 1, 1, 1, 5, 5, 9, 9,17,17, 4, 0 }
};

u8 ALIGN_DATA fetch_waitstate[4][16] =
{
  // Non-sequential
  { 0, 0, 2, 0, 0, 0, 0, 0, 4, 4, 4, 4, 4, 4, 4, 0 }, // 8,16bit accesses
  { 0, 0, 5, 0, 0, 1, 1, 1, 7, 7, 9, 9,13,13, 4, 0 }, // 32bit accesses

  // Sequential
  { 0, 0, 2, 0, 0, 0, 0, 0, 2, 2, 4, 4, 8, 8, 4, 0 },
  { 0, 0, 5, 0, 0, 1, 1, 1, 5, 5, 9, 9,17,17, 4, 0 }
};

// GBA memory areas.

u16 ALIGN_DATA palette_ram   [  0x200]; // Palette RAM             (05000000h)      1 KiB
u16 ALIGN_DATA oam_ram       [  0x200]; // Object Attribute Memory (07000000h)      1 KiB
u16 ALIGN_DATA io_registers  [  0x200]; // I/O Registers           (04000000h)      1 KiB
u8  ALIGN_DATA ewram_data    [0x40000]; // External Working RAM    (02000000h)    256 KiB
u8  ALIGN_DATA iwram_data    [ 0x8000]; // Internal Working RAM    (03000000h)     32 KiB
u8  ALIGN_DATA vram          [0x18000]; // Video RAM               (06000000h)     96 KiB
struct BIOS_DATA ALIGN_DATA bios;       // BIOS ROM and code tags  (00000000h)     48 KiB
u8  ALIGN_DATA gamepak_backup[0x20000]; // Backup flash/EEPROM...  (0E000000h)    128 KiB
                                        // ----------------------------------------------
                                        // Total                                  594 KiB
/*
 * These are Metadata Areas corresponding to the Data Areas above. They
 * contain information about the native code compilation status of each
 * Data Word in that Data Area. For more information about these, see
 * "doc/partial flushing of RAM code.txt" in your source tree.
 */
u16 ALIGN_DATA ewram_metadata[0x40000]; // External Working RAM code metadata     512 KiB
u16 ALIGN_DATA iwram_metadata[ 0x8000]; // Internal Working RAM code metadata      64 KiB
u16 ALIGN_DATA vram_metadata [0x18000]; // Video RAM code metadata                192 KiB
                                        // ----------------------------------------------
                                        // Total                                  768 KiB

u32 bios_read_protect;

TimerType ALIGN_DATA timer[4];
const u32 ALIGN_DATA timer_prescale_table[] = { 0, 6, 8, 10 };

DmaTransferType ALIGN_DATA dma[4];
const s32 ALIGN_DATA dma_addr_control[4] = { 2, -2, 0, 2 };

// DMA Prefetch
u32 cpu_dma_hack;
u32 cpu_dma_last;

// Keeps us knowing how much we have left.
u8 *gamepak_rom = NULL;
u32 gamepak_size;

u32 gamepak_next_swap;

u32 gamepak_ram_buffer_size;
u32 gamepak_ram_pages;

char gamepak_title[13];
char gamepak_code[5];
char gamepak_maker[3];
char CurrentGamePath[MAX_PATH];
bool IsGameLoaded = false;
bool IsZippedROM = false; // true if the current ROM came directly from a
                          // zip file (false if it was extracted to temp)

bool IsNintendoBIOS = false;

// Enough to map the gamepak RAM space.
u16 ALIGN_DATA gamepak_memory_map[1024];

// This is global so that it can be kept open for large ROMs to swap
// pages from, so there's no slowdown with opening and closing the file
// a lot.

FILE_TAG_TYPE gamepak_file_large = FILE_TAG_INVALID;

char main_path[MAX_PATH + 1];

// Writes to these respective locations should trigger an update
// so the related subsystem may react to it.

// If OAM is written to:
u32 oam_update = 1;


// RTC
typedef enum
{
  RTC_DISABLED,
  RTC_IDLE,
  RTC_COMMAND,
  RTC_OUTPUT_DATA,
  RTC_INPUT_DATA
} RTC_STATE_TYPE;

typedef enum
{
  RTC_COMMAND_RESET            = 0x60,
  RTC_COMMAND_WRITE_STATUS     = 0x62,
  RTC_COMMAND_READ_STATUS      = 0x63,
  RTC_COMMAND_OUTPUT_TIME_FULL = 0x65,
  RTC_COMMAND_OUTPUT_TIME      = 0x67
} RTC_COMMAND_TYPE;

typedef enum
{
  RTC_WRITE_TIME,
  RTC_WRITE_TIME_FULL,
  RTC_WRITE_STATUS
} RTC_WRITE_MODE_TYPE;

RTC_STATE_TYPE rtc_state = RTC_DISABLED;
RTC_WRITE_MODE_TYPE rtc_write_mode;

u16 ALIGN_DATA rtc_registers[3];
u32 ALIGN_DATA rtc_data[12];
u32 rtc_command;
u32 rtc_status = 0x40;
u32 rtc_data_bytes;
s32 rtc_bit_count;

static u32 encode_bcd(u8 value);


// Write out backup file this many cycles after the most recent
// backup write.
#define WRITE_BACKUP_DELAY  (10)

// If the backup space is written (only update once this hits 0)
u32 backup_update = WRITE_BACKUP_DELAY + 1;

typedef enum
{
  BACKUP_SRAM,
  BACKUP_FLASH,
  BACKUP_EEPROM,
  BACKUP_NONE
} BACKUP_TYPE_TYPE;

BACKUP_TYPE_TYPE backup_type = BACKUP_NONE;

typedef enum
{
  SRAM_SIZE_32KB = 0x08000,
  SRAM_SIZE_64KB = 0x10000
} SRAM_SIZE_TYPE;

// Keep it 32KB until the upper 64KB is accessed, then make it 64KB.
SRAM_SIZE_TYPE sram_size = SRAM_SIZE_32KB;

typedef enum
{
  FLASH_BASE_MODE,
  FLASH_ERASE_MODE,
  FLASH_ID_MODE,
  FLASH_WRITE_MODE,
  FLASH_BANKSWITCH_MODE
} FLASH_MODE_TYPE;

typedef enum
{
  FLASH_SIZE_64KB  = 0x10000,
  FLASH_SIZE_128KB = 0x20000
} FLASH_SIZE_TYPE;

typedef enum
{
  FLASH_DEVICE_MACRONIX_64KB   = 0x1C,
  FLASH_DEVICE_ATMEL_64KB      = 0x3D,
  FLASH_DEVICE_SST_64KB        = 0xD4,
  FLASH_DEVICE_PANASONIC_64KB  = 0x1B,
  FLASH_DEVICE_SANYO_128KB     = 0x13,
  FLASH_DEVICE_MACRONIX_128KB  = 0x09
} FLASH_DEVICE_ID_TYPE;

typedef enum
{
  FLASH_MANUFACTURER_MACRONIX  = 0xC2,
  FLASH_MANUFACTURER_ATMEL     = 0x1F,
  FLASH_MANUFACTURER_PANASONIC = 0x32,
  FLASH_MANUFACTURER_SST       = 0xBF, // sanyo or sst
  FLASH_MANUFACTURER_SANYO     = 0x62
} FLASH_MANUFACTURER_ID_TYPE;

FLASH_MODE_TYPE flash_mode = FLASH_BASE_MODE;
u32 flash_command_position = 0;
u32 flash_bank_offset = 0;

FLASH_DEVICE_ID_TYPE flash_device_id = FLASH_DEVICE_PANASONIC_64KB;
FLASH_MANUFACTURER_ID_TYPE flash_manufacturer_id = FLASH_MANUFACTURER_PANASONIC;
FLASH_SIZE_TYPE flash_size = FLASH_SIZE_64KB;

typedef enum
{
  EEPROM_512_BYTE = 0x0200,
  EEPROM_8_KBYTE  = 0x2000
} EEPROM_SIZE_TYPE;

typedef enum
{
  EEPROM_BASE_MODE,
  EEPROM_READ_MODE,
  EEPROM_READ_HEADER_MODE,
  EEPROM_ADDRESS_MODE,
  EEPROM_WRITE_MODE,
  EEPROM_WRITE_ADDRESS_MODE,
  EEPROM_ADDRESS_FOOTER_MODE,
  EEPROM_WRITE_FOOTER_MODE
} EEPROM_MODE_TYPE;

EEPROM_SIZE_TYPE eeprom_size = EEPROM_512_BYTE;
EEPROM_MODE_TYPE eeprom_mode = EEPROM_BASE_MODE;
u32 eeprom_address_length;
u32 eeprom_address = 0;
u32 eeprom_counter = 0;
u8 ALIGN_DATA eeprom_buffer[8];

char backup_filename[MAX_FILE];
static u32 save_backup(void);

char backup_id[16];
static void load_backup_id(void);

// Tilt sensor on the GBA side. It's mapped... somewhere... in the GBA address
// space. See the read_backup function in memory.c for more information.
u32 tilt_sensor_x;
u32 tilt_sensor_y;


// SIO
typedef enum
{
  NORMAL8,
  NORMAL32,
  MULTIPLAYER,
  UART,
  GP,
  JOYBUS
} SIO_MODE_TYPE;

static SIO_MODE_TYPE sio_mode(u16 reg_sio_cnt, u16 reg_rcnt);
static CPU_ALERT_TYPE sio_control(u32 value);

u32 send_adhoc_multi();
u32 g_multi_mode;


static void waitstate_control(u32 value);

static char *skip_spaces(char *line_ptr);
static s32 parse_config_line(char *current_line, char *current_variable, char *current_value);
static s32 load_game_config(char *gamepak_title, char *gamepak_code, char *gamepak_maker);
static bool lookup_game_config(char *gamepak_title, char *gamepak_code, char *gamepak_maker, FILE_TAG_TYPE config_file);

static void init_memory_gamepak(void);

static ssize_t load_gamepak_raw(char *name);
static u32 evict_gamepak_page(void);


u8 *read_rom_block = NULL;
u8 *read_ram_block = NULL;

u32 read_rom_region = 0xFFFFFFFF;
u32 read_ram_region = 0xFFFFFFFF;

inline static CPU_ALERT_TYPE check_smc_write(u16 *metadata, u32 offset, u8 region);

static u32 read_null(u32 address);

static u32 read8_ram(u32 address);
static u32 read16_ram(u32 address);
static u32 read32_ram(u32 address);

static u32 read8_bios(u32 address);
static u32 read8_io_registers(u32 address);
static u32 read8_palette_ram(u32 address);
static u32 read8_oam_ram(u32 address);
static u32 read8_gamepak(u32 address);
static u32 read8_backup(u32 address);
static u32 read8_open(u32 address);

static u32 read16_bios(u32 address);
static u32 read16_io_registers(u32 address);
static u32 read16_palette_ram(u32 address);
static u32 read16_oam_ram(u32 address);
static u32 read16_eeprom(u32 address);
static u32 read16_gamepak(u32 address);
static u32 read16_backup(u32 address);
static u32 read16_open(u32 address);

static u32 read32_bios(u32 address);
static u32 read32_io_registers(u32 address);
static u32 read32_palette_ram(u32 address);
static u32 read32_oam_ram(u32 address);
static u32 read32_gamepak(u32 address);
static u32 read32_backup(u32 address);
static u32 read32_open(u32 address);

static u32 (*mem_read8[16])(u32) =
{
  read8_bios,          // 0
  read8_open,          // 1
  read8_ram,           // 2
  read8_ram,           // 3
  read8_io_registers,  // 4
  read8_palette_ram,   // 5
  read8_ram,           // 6
  read8_oam_ram,       // 7
  read8_gamepak,       // 8
  read8_gamepak,       // 9
  read8_gamepak,       // a
  read8_gamepak,       // b
  read8_gamepak,       // c
  read8_gamepak,       // d
  read8_backup,        // e
  read8_open           // f
};

static u32 (*mem_read16[16])(u32) =
{
  read16_bios,         // 0
  read16_open,         // 1
  read16_ram,          // 2
  read16_ram,          // 3
  read16_io_registers, // 4
  read16_palette_ram,  // 5
  read16_ram,          // 6
  read16_oam_ram,      // 7
  read16_gamepak,      // 8
  read16_gamepak,      // 9
  read16_gamepak,      // a
  read16_gamepak,      // b
  read16_gamepak,      // c
  read16_eeprom,       // d
  read16_backup,       // e
  read16_open          // f
};

static u32 (*mem_read32[16])(u32) =
{
  read32_bios,         // 0
  read32_open,         // 1
  read32_ram,          // 2
  read32_ram,          // 3
  read32_io_registers, // 4
  read32_palette_ram,  // 5
  read32_ram,          // 6
  read32_oam_ram,      // 7
  read32_gamepak,      // 8
  read32_gamepak,      // 9
  read32_gamepak,      // a
  read32_gamepak,      // b
  read32_gamepak,      // c
  read32_gamepak,      // d
  read32_backup,       // e
  read32_open          // f
};

static u32 (*dma_read16[16])(u32) =
{
  read_null,           // 0
  read_null,           // 1
  read16_ram,          // 2
  read16_ram,          // 3
  read16_io_registers, // 4
  read16_palette_ram,  // 5
  read16_ram,          // 6
  read16_oam_ram,      // 7
  read16_gamepak,      // 8
  read16_gamepak,      // 9
  read16_gamepak,      // a
  read16_gamepak,      // b
  read16_gamepak,      // c
  read16_eeprom,       // d
  read_null,           // e
  read_null            // f
};

static u32 (*dma_read32[16])(u32) =
{
  read_null,           // 0
  read_null,           // 1
  read32_ram,          // 2
  read32_ram,          // 3
  read32_io_registers, // 4
  read32_palette_ram,  // 5
  read32_ram,          // 6
  read32_oam_ram,      // 7
  read32_gamepak,      // 8
  read32_gamepak,      // 9
  read32_gamepak,      // a
  read32_gamepak,      // b
  read32_gamepak,      // c
  read32_gamepak,      // d
  read_null,           // e
  read_null            // f
};

static u32 (*open_read8[16])(u32) =
{
  read8_bios,          // 0
  read_null,           // 1
  read8_ram,           // 2
  read8_ram,           // 3
  read_null,           // 4
  read_null,           // 5
  read8_ram,           // 6
  read_null,           // 7
  read8_gamepak,       // 8
  read8_gamepak,       // 9
  read8_gamepak,       // a
  read8_gamepak,       // b
  read8_gamepak,       // c
  read8_gamepak,       // d
  read_null,           // e
  read_null            // f
};

static u32 (*open_read16[16])(u32) =
{
  read16_bios,         // 0
  read_null,           // 1
  read16_ram,          // 2
  read16_ram,          // 3
  read_null,           // 4
  read_null,           // 5
  read16_ram,          // 6
  read_null,           // 7
  read16_gamepak,      // 8
  read16_gamepak,      // 9
  read16_gamepak,      // a
  read16_gamepak,      // b
  read16_gamepak,      // c
  read16_gamepak,      // d
  read_null,           // e
  read_null            // f
};

static u32 (*open_read32[16])(u32) =
{
  read32_bios,         // 0
  read_null,           // 1
  read32_ram,          // 2
  read32_ram,          // 3
  read_null,           // 4
  read_null,           // 5
  read32_ram,          // 6
  read_null,           // 7
  read32_gamepak,      // 8
  read32_gamepak,      // 9
  read32_gamepak,      // a
  read32_gamepak,      // b
  read32_gamepak,      // c
  read32_gamepak,      // d
  read_null,           // e
  read_null            // f
};


static CPU_ALERT_TYPE write_null(u32 address, u32 value);

static CPU_ALERT_TYPE write8_ewram(u32 address, u32 value);
static CPU_ALERT_TYPE write8_iwram(u32 address, u32 value);
static CPU_ALERT_TYPE write8_io_registers(u32 address, u32 value);
static CPU_ALERT_TYPE write8_palette_ram(u32 address, u32 value);
static CPU_ALERT_TYPE write8_vram(u32 address, u32 value);
static CPU_ALERT_TYPE write8_backup(u32 address, u32 value);

static CPU_ALERT_TYPE write16_ewram(u32 address, u32 value);
static CPU_ALERT_TYPE write16_iwram(u32 address, u32 value);
static CPU_ALERT_TYPE write16_io_registers(u32 address, u32 value);
static CPU_ALERT_TYPE write16_palette_ram(u32 address, u32 value);
static CPU_ALERT_TYPE write16_vram(u32 address, u32 value);
static CPU_ALERT_TYPE write16_oam_ram(u32 address, u32 value);

static CPU_ALERT_TYPE write32_ewram(u32 address, u32 value);
static CPU_ALERT_TYPE write32_iwram(u32 address, u32 value);
static CPU_ALERT_TYPE write32_io_registers(u32 address, u32 value);
static CPU_ALERT_TYPE write32_palette_ram(u32 address, u32 value);
static CPU_ALERT_TYPE write32_vram(u32 address, u32 value);
static CPU_ALERT_TYPE write32_oam_ram(u32 address, u32 value);

static CPU_ALERT_TYPE (*mem_write8[16])(u32, u32) =
{
  write_null,           // 0
  write_null,           // 1
  write8_ewram,         // 2
  write8_iwram,         // 3
  write8_io_registers,  // 4
  write8_palette_ram,   // 5
  write8_vram,          // 6
  write_null,           // 7
  write_null,           // 8
  write_null,           // 9
  write_null,           // a
  write_null,           // b
  write_null,           // c
  write_null,           // d
  write8_backup,        // e
  write_null            // f
};

static CPU_ALERT_TYPE (*mem_write16[16])(u32, u32) =
{
  write_null,           // 0
  write_null,           // 1
  write16_ewram,        // 2
  write16_iwram,        // 3
  write16_io_registers, // 4
  write16_palette_ram,  // 5
  write16_vram,         // 6
  write16_oam_ram,      // 7
  write_rtc,            // 8
  write_null,           // 9
  write_null,           // a
  write_null,           // b
  write_null,           // c
  write_null,           // d
  write_null,           // e
  write_null            // f
};

static CPU_ALERT_TYPE (*mem_write32[16])(u32, u32) =
{
  write_null,           // 0
  write_null,           // 1
  write32_ewram,        // 2
  write32_iwram,        // 3
  write32_io_registers, // 4
  write32_palette_ram,  // 5
  write32_vram,         // 6
  write32_oam_ram,      // 7
  write_null,           // 8
  write_null,           // 9
  write_null,           // a
  write_null,           // b
  write_null,           // c
  write_null,           // d
  write_null,           // e
  write_null            // f
};

static CPU_ALERT_TYPE (*dma_write16[16])(u32, u32) =
{
  write_null,           // 0
  write_null,           // 1
  write16_ewram,        // 2
  write16_iwram,        // 3
  write16_io_registers, // 4
  write16_palette_ram,  // 5
  write16_vram,         // 6
  write16_oam_ram,      // 7
  write_null,           // 8
  write_null,           // 9
  write_null,           // a
  write_null,           // b
  write_null,           // c
  write_eeprom,         // d
  write_null,           // e
  write_null            // f
};

static CPU_ALERT_TYPE (*dma_write32[16])(u32, u32) =
{
  write_null,           // 0
  write_null,           // 1
  write32_ewram,        // 2
  write32_iwram,        // 3
  write32_io_registers, // 4
  write32_palette_ram,  // 5
  write32_vram,         // 6
  write32_oam_ram,      // 7
  write_null,           // 8
  write_null,           // 9
  write_null,           // a
  write_null,           // b
  write_null,           // c
  write_null,           // d
  write_null,           // e
  write_null            // f
};


#define READ_BIOS(type, mask)                                                 \
  if ((address >> 14) != 0)                                                   \
    return read##type##_open(address);                                        \
                                                                              \
  if ((reg[REG_PC] >> 14) != 0)                                               \
    return ADDRESS##type(&bios_read_protect, address & 0x03);                 \
                                                                              \
  return ADDRESS##type(bios.rom, address & mask);                             \

static u32 read8_bios(u32 address)
{
  READ_BIOS(8, 0x3FFF);
}

static u32 read16_bios(u32 address)
{
  READ_BIOS(16, 0x3FFE);
}

static u32 read32_bios(u32 address)
{
  READ_BIOS(32, 0x3FFC);
}

#define READ_IO_REGISTERS(type, mask)                                         \
  if (((address >> 10) & 0x3FFF) != 0)                                        \
    return read##type##_open(address);                                        \
                                                                              \
  return ADDRESS##type(io_registers, address & mask);                         \

static u32 read8_io_registers(u32 address)
{
  READ_IO_REGISTERS(8, 0x3FF);
}

static u32 read16_io_registers(u32 address)
{
  READ_IO_REGISTERS(16, 0x3FE);
}

static u32 read32_io_registers(u32 address)
{
  READ_IO_REGISTERS(32, 0x3FC);
}

#define READ_PALETTE_RAM(type, mask)                                          \
  return ADDRESS##type(palette_ram, address & mask);                          \

static u32 read8_palette_ram(u32 address)
{
  READ_PALETTE_RAM(8, 0x3FF);
}

static u32 read16_palette_ram(u32 address)
{
  READ_PALETTE_RAM(16, 0x3FE);
}

static u32 read32_palette_ram(u32 address)
{
  READ_PALETTE_RAM(32, 0x3FC);
}

#define READ_OAM_RAM(type, mask)                                              \
  return ADDRESS##type(oam_ram, address & mask);                              \

static u32 read8_oam_ram(u32 address)
{
  READ_OAM_RAM(8, 0x3FF);
}

static u32 read16_oam_ram(u32 address)
{
  READ_OAM_RAM(16, 0x3FE);
}

static u32 read32_oam_ram(u32 address)
{
  READ_OAM_RAM(32, 0x3FC);
}

#define READ_GAMEPAK(type, mask)                                              \
  if ((address & 0x1FFFFFF) < gamepak_size)                                   \
  {                                                                           \
    u32 new_region = address >> 15;                                           \
                                                                              \
    if (new_region != read_rom_region)                                        \
    {                                                                         \
      read_rom_region = new_region;                                           \
      read_rom_block = memory_map_read[read_rom_region];                      \
                                                                              \
      if (read_rom_block == NULL)                                             \
        read_rom_block = load_gamepak_page(read_rom_region & 0x3FF);          \
    }                                                                         \
                                                                              \
    return ADDRESS##type(read_rom_block, address & mask);                     \
  }                                                                           \

static u32 read8_gamepak(u32 address)
{
  READ_GAMEPAK(8, 0x7FFF);

  return read8_open(address);
}

static u32 read16_gamepak(u32 address)
{
  READ_GAMEPAK(16, 0x7FFE);

  return read16_open(address);
}

static u32 read32_gamepak(u32 address)
{
  READ_GAMEPAK(32, 0x7FFC);

  return read32_open(address);
}

static u32 read16_eeprom(u32 address)
{
  READ_GAMEPAK(16, 0x7FFE);

  return read_eeprom();
}

static u32 read8_backup(u32 address)
{
  return read_backup(address & 0xFFFF);
}

static u32 read16_backup(u32 address)
{
  u32 value = read_backup(address & 0xFFFe);
  return value | (value << 8);
}

static u32 read32_backup(u32 address)
{
  u32 value = read_backup(address & 0xFFFC);
  return value | (value << 8) | (value << 16) | (value << 24);
}

// ewram, iwram, vram
#define READ_RAM(type, mask)                                                  \
  u32 new_region = address >> 15;                                             \
                                                                              \
  if (new_region != read_ram_region)                                          \
  {                                                                           \
    read_ram_region = new_region;                                             \
    read_ram_block = memory_map_read[read_ram_region];                        \
  }                                                                           \
                                                                              \
  return ADDRESS##type(read_ram_block, address & mask);                       \

static u32 read8_ram(u32 address)
{
  READ_RAM(8, 0x7FFF);
}

static u32 read16_ram(u32 address)
{
  READ_RAM(16, 0x7FFE);
}

static u32 read32_ram(u32 address)
{
  READ_RAM(32, 0x7FFC);
}

static u32 read_null(u32 address)
{
  return 0;
}

#define THUMB_STATE (reg[REG_CPSR] & 0x20)

static u32 read8_open(u32 address)
{
  u32 offset = 0;

  if (cpu_dma_hack != 0)
    return cpu_dma_last & 0xFF;

  if (THUMB_STATE != 0)
    offset = reg[REG_PC] + 2 + (address & 0x01);
  else
    offset = reg[REG_PC] + 4 + (address & 0x03);

  return (*open_read8[offset >> 24])(offset);
}

static u32 read16_open(u32 address)
{
  u32 offset = 0;

  if (cpu_dma_hack != 0)
    return cpu_dma_last & 0xFFFF;

  if (THUMB_STATE != 0)
    offset = reg[REG_PC] + 2;
  else
    offset = reg[REG_PC] + 4 + (address & 0x02);

  return (*open_read16[offset >> 24])(offset);
}

static u32 read32_open(u32 address)
{
  if (cpu_dma_hack != 0)
    return cpu_dma_last;

  if (THUMB_STATE != 0)
  {
    u32 current_instruction = (*open_read16[reg[REG_PC] >> 24])(reg[REG_PC] + 2);
    return current_instruction | (current_instruction << 16);
  }

  return (*open_read32[reg[REG_PC] >> 24])(reg[REG_PC] + 4);
}


inline static CPU_ALERT_TYPE check_smc_write(u16 *metadata, u32 offset, u8 region)
{
  /* Get the Metadata Entry's [3], bits 0-1, to see if there's code at this
   * location. See "doc/partial flushing of RAM code.txt" for more info. */
  u16 smc = metadata[offset | 3] & 0x03;
  if (smc != 0) {
    partial_clear_metadata(offset, region);
    return CPU_ALERT_SMC;
  }
  return CPU_ALERT_NONE;
}

#define WRITE_EWRAM(type, mask)                                               \
  address &= mask;                                                            \
  ADDRESS##type(ewram_data, address) = value;                                 \
  return check_smc_write(ewram_metadata, address, 0x02);                      \

static CPU_ALERT_TYPE write8_ewram(u32 address, u32 value)
{
  WRITE_EWRAM(8, 0x3FFFF);
}

static CPU_ALERT_TYPE write16_ewram(u32 address, u32 value)
{
  WRITE_EWRAM(16, 0x3FFFE);
}

static CPU_ALERT_TYPE write32_ewram(u32 address, u32 value)
{
  WRITE_EWRAM(32, 0x3FFFC);
}

#define WRITE_IWRAM(type, mask)                                               \
  address &= mask;                                                            \
  ADDRESS##type(iwram_data, address) = value;                                 \
  return check_smc_write(iwram_metadata, address, 0x03);                      \

static CPU_ALERT_TYPE write8_iwram(u32 address, u32 value)
{
  WRITE_IWRAM(8, 0x7FFF);
}

static CPU_ALERT_TYPE write16_iwram(u32 address, u32 value)
{
  WRITE_IWRAM(16, 0x7FFE);
}

static CPU_ALERT_TYPE write32_iwram(u32 address, u32 value)
{
  WRITE_IWRAM(32, 0x7FFC);
}

#define WRITE_IO_REGISTERS(type, mask)                                        \
  if (((address >> 10) & 0x3FFF) != 0)                                        \
    return CPU_ALERT_NONE;                                                    \
                                                                              \
  return write_io_register##type(address & mask, value);                      \

static CPU_ALERT_TYPE write8_io_registers(u32 address, u32 value)
{
  WRITE_IO_REGISTERS(8, 0x3FF);
}

static CPU_ALERT_TYPE write16_io_registers(u32 address, u32 value)
{
  WRITE_IO_REGISTERS(16, 0x3FE);
}

static CPU_ALERT_TYPE write32_io_registers(u32 address, u32 value)
{
  WRITE_IO_REGISTERS(32, 0x3FC);
}

static CPU_ALERT_TYPE write8_palette_ram(u32 address, u32 value)
{
  ADDRESS16(palette_ram, address & 0x3Fe) = value | (value << 8);

  return CPU_ALERT_NONE;
}

static CPU_ALERT_TYPE write16_palette_ram(u32 address, u32 value)
{
  ADDRESS16(palette_ram, address & 0x3Fe) = value;

  return CPU_ALERT_NONE;
}

static CPU_ALERT_TYPE write32_palette_ram(u32 address, u32 value)
{
  ADDRESS32(palette_ram, address & 0x3FC) = value;

  return CPU_ALERT_NONE;
}

#define WRITE_VRAM(type, mask1, mask2)                                        \
  if (((address >> 16) & 0x01) != 0)                                          \
    address &= mask1;                                                         \
  else                                                                        \
    address &= mask2;                                                         \
                                                                              \
  ADDRESS##type(vram, address) = value;                                       \
  return check_smc_write(vram_metadata, address, 0x06);                       \

static CPU_ALERT_TYPE write8_vram(u32 address, u32 value)
{
  if (((address >> 16) & 0x01) != 0)
    address &= 0x17FFe;
  else
    address &= 0x0FFFe;

  ADDRESS16(vram, address) = value | (value << 8);
  return check_smc_write(vram_metadata, address, 0x06);
}

static CPU_ALERT_TYPE write16_vram(u32 address, u32 value)
{
  WRITE_VRAM(16, 0x17FFE, 0x0FFFE);
}

static CPU_ALERT_TYPE write32_vram(u32 address, u32 value)
{
  WRITE_VRAM(32, 0x17FFC, 0x0FFFC);
}

#define WRITE_OAM_RAM(type, mask)                                             \
  oam_update = 1;                                                             \
  ADDRESS##type(oam_ram, address & mask) = value;                             \
                                                                              \
  return  CPU_ALERT_NONE;                                                     \

static CPU_ALERT_TYPE write16_oam_ram(u32 address, u32 value)
{
  WRITE_OAM_RAM(16, 0x3FE);
}

static CPU_ALERT_TYPE write32_oam_ram(u32 address, u32 value)
{
  WRITE_OAM_RAM(32, 0x3FC);
}

static CPU_ALERT_TYPE write8_backup(u32 address, u32 value)
{
  return write_backup(address & 0xFFFF, value);
}

static CPU_ALERT_TYPE write_null(u32 address, u32 value)
{
  return CPU_ALERT_NONE;
}


// write io registers

static SIO_MODE_TYPE sio_mode(u16 reg_sio_cnt, u16 reg_rcnt)
{
  if ((reg_rcnt & 0x8000) == 0x0000)
  {
    switch (reg_sio_cnt & 0x3000)
    {
      case 0x0000:
        return NORMAL8;

      case 0x1000:
        return NORMAL32;

      case 0x2000:
        return MULTIPLAYER;

      case 0x3000:
        return UART;
    }
  }

  if ((reg_rcnt & 0x4000) != 0)
    return JOYBUS;

  return GP;
}

// Simulate 'No connection'
static CPU_ALERT_TYPE sio_control(u32 value)
{
  SIO_MODE_TYPE mode = sio_mode(value, io_registers[REG_RCNT]);
  CPU_ALERT_TYPE alert = CPU_ALERT_NONE;

  switch (mode)
  {
    case NORMAL8:
    case NORMAL32:
      if ((value & 0x80) != 0)
      {
        value &= 0xFF7F;

        if ((value & 0x4001) == 0x4001)
        {
          io_registers[REG_IF] |= IRQ_SERIAL;
          alert = CPU_ALERT_IRQ;
        }
      }
      break;

    case MULTIPLAYER:
      value &= 0xFF83;
      value |= 0x000C;
      break;

    case UART:
    case JOYBUS:
    case GP:
      break;
  }

  ADDRESS16(io_registers, 0x128) = value;

  return alert;
}

// multiモードのデータを送る
u32 send_adhoc_multi()
{
  return 0;
}

static void waitstate_control(u32 value)
{
  u32 i;
  const u8 waitstate_table[4] = { 4, 3, 2, 8 };
  const u8 gamepak_ws0_seq[2] = { 2, 1 };
  const u8 gamepak_ws1_seq[2] = { 4, 1 };
  const u8 gamepak_ws2_seq[2] = { 8, 1 };

  // Wait State First Access (8/16bit)
  pMEMORY_WS16N(0x08) = pMEMORY_WS16N(0x09) = waitstate_table[(value >> 2) & 0x03];
  pMEMORY_WS16N(0x0A) = pMEMORY_WS16N(0x0B) = waitstate_table[(value >> 5) & 0x03];
  pMEMORY_WS16N(0x0C) = pMEMORY_WS16N(0x0D) = waitstate_table[(value >> 8) & 0x03];

  // Wait State Second Access (8/16bit)
  pMEMORY_WS16S(0x08) = pMEMORY_WS16S(0x09) = gamepak_ws0_seq[(value >>  4) & 0x01];
  pMEMORY_WS16S(0x0A) = pMEMORY_WS16S(0x0B) = gamepak_ws1_seq[(value >>  7) & 0x01];
  pMEMORY_WS16S(0x0C) = pMEMORY_WS16S(0x0D) = gamepak_ws2_seq[(value >> 10) & 0x01];

  // SRAM Wait Control (8bit)
  pMEMORY_WS16N(0x0e) = pMEMORY_WS16S(0x0e) =
  pMEMORY_WS32N(0x0e) = pMEMORY_WS32S(0x0e) = waitstate_table[value & 0x03];

  for (i = 0x08; i <= 0x0D; i++)
  {
    // Wait State First Access (32bit)
    pMEMORY_WS32N(i) = pMEMORY_WS16N(i) + pMEMORY_WS16S(i) + 1;

    // Wait State Second Access (32bit)
    pMEMORY_WS32S(i) = (pMEMORY_WS16S(i) << 1) + 1;
  }

  // gamepak prefetch
  if (((value >> 14) & 0x01) != 0)
  {
    for (i = 0x08; i <= 0x0D; i++)
    {
      pFETCH_WS16N(i) = 0;
      pFETCH_WS16S(i) = 0;
      pFETCH_WS32N(i) = 1;
      pFETCH_WS32S(i) = 1;
    }
  }
  else
  {
    for (i = 0x08; i <= 0x0D; i++)
    {
      // Prefetch Disable Bug
      // the opcode fetch time from 1S to 1N.
      pFETCH_WS16N(i) = pMEMORY_WS16N(i);
      pFETCH_WS16S(i) = pMEMORY_WS16N(i);
      pFETCH_WS32N(i) = pMEMORY_WS32N(i);
      pFETCH_WS32S(i) = pMEMORY_WS32N(i);
    }
  }

  ADDRESS16(io_registers, 0x204) = (ADDRESS16(io_registers, 0x204) & 0x8000) | (value & 0x7FFF);
}


#define trigger_dma(address)                                                  \
{                                                                             \
  u32 channel = (address - 0xBA) / 12;                                        \
                                                                              \
  DmaTransferType *_dma = dma + channel;                                      \
                                                                              \
  if ((value & 0x8000) != 0)                                                  \
  {                                                                           \
    _dma->dma_channel = channel;                                              \
    _dma->source_direction = (value >>  7) & 0x03;                            \
    _dma->repeat_type = (value >> 9) & 0x01;                                  \
    _dma->irq = (value >> 14) & 0x01;                                         \
                                                                              \
    if (_dma->start_type == DMA_INACTIVE)                                     \
    {                                                                         \
      u32 start_type = (value >> 12) & 0x03;                                  \
      u32 dest_address = ADDRESS32(io_registers, (address - 0x06));           \
                                                                              \
      _dma->start_type = start_type;                                          \
      _dma->source_address = ADDRESS32(io_registers, (address - 0x0A));       \
      _dma->dest_address = dest_address;                                      \
                                                                              \
      /* If it is sound FIFO DMA make sure the settings are a certain way */  \
      if ((channel >= 1) && (channel <= 2) && (start_type == DMA_START_SPECIAL)) \
      {                                                                       \
        _dma->dest_direction = DMA_FIXED;                                     \
        _dma->length_type = DMA_32BIT;                                        \
        _dma->length = 4;                                                     \
                                                                              \
        if (dest_address == 0x40000A4)                                        \
          _dma->direct_sound_channel = DMA_DIRECT_SOUND_B;                    \
        else                                                                  \
          _dma->direct_sound_channel = DMA_DIRECT_SOUND_A;                    \
      }                                                                       \
      else                                                                    \
      {                                                                       \
        u32 length = ADDRESS16(io_registers, (address - 0x02));               \
                                                                              \
        if ((channel == 3) && ((dest_address >> 24) == 0x0D) && ((length & 0x1F) == 17)) \
          eeprom_size = EEPROM_8_KBYTE;                                       \
                                                                              \
        if (length == 0)                                                      \
        {                                                                     \
          if (channel == 3)                                                   \
            length = 0x10000;                                                 \
          else                                                                \
            length = 0x04000;                                                 \
        }                                                                     \
                                                                              \
        _dma->dest_direction = (value >> 5) & 0x03;                           \
        _dma->length_type = (value >> 10) & 0x01;                             \
        _dma->length = length;                                                \
                                                                              \
        if (start_type == DMA_START_IMMEDIATELY)                              \
          return dma_transfer(dma + channel);                                 \
      }                                                                       \
    }                                                                         \
    else                                                                      \
    {                                                                         \
      u32 start_type = (value >> 12) & 0x03;                                  \
                                                                              \
      _dma->start_type = start_type;                                          \
                                                                              \
      if ((channel >= 1) && (channel <= 2) && (start_type == DMA_START_SPECIAL)) \
      {                                                                       \
        _dma->dest_direction = DMA_FIXED;                                     \
        _dma->length_type = DMA_32BIT;                                        \
      }                                                                       \
      else                                                                    \
      {                                                                       \
        _dma->dest_direction = (value >> 5) & 0x03;                           \
        _dma->length_type = (value >> 10) & 0x01;                             \
      }                                                                       \
    }                                                                         \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    _dma->start_type = DMA_INACTIVE;                                          \
    _dma->direct_sound_channel = DMA_NO_DIRECT_SOUND;                         \
  }                                                                           \
}                                                                             \


#define COUNT_TIMER(address)                                                  \
{                                                                             \
  u32 timer_number = (address >> 2) & 0x03;                                   \
                                                                              \
  timer[timer_number].reload = 0x10000 - value;                               \
  timer[timer_number].reload_update = 1;                                      \
}                                                                             \

#define TRIGGER_TIMER(address)                                                \
{                                                                             \
  u32 timer_number = (address - 0x102) >> 2;                                  \
                                                                              \
  timer[timer_number].control_value = value;                                  \
  timer[timer_number].control_update = 1;                                     \
                                                                              \
  ADDRESS16(io_registers, address) = value;                                   \
                                                                              \
  return CPU_ALERT_TIMER;                                                     \
}                                                                             \


#define ACCESS_REGISTER8_HIGH(address)                                        \
  value = ADDRESS8(io_registers, address) | (value << 8);                     \

#define ACCESS_REGISTER8_LOW(address)                                         \
  value = value | (ADDRESS8(io_registers, address + 1) << 8);                 \

#define ACCESS_REGISTER16_HIGH(address)                                       \
  value = ADDRESS16(io_registers, address) | (value << 16);                   \

#define ACCESS_REGISTER16_LOW(address)                                        \
  value = value | (ADDRESS16(io_registers, address + 2) << 16);               \


CPU_ALERT_TYPE write_io_register8(u32 address, u32 value)
{
  switch(address)
  {
    // VCOUNT
    case 0x06:
    case 0x07:
      /* Read only */
      break;

    // P1
    case 0x130:
    case 0x131:
      /* Read only */
      break;

    // Post Boot
    case 0x300:
      ADDRESS8(io_registers, 0x300) = value;
      break;

    // Halt
    case 0x301:
      if ((value & 0x80) != 0)
        reg[CPU_HALT_STATE] = CPU_STOP;
      else
        reg[CPU_HALT_STATE] = CPU_HALT;

      ADDRESS8(io_registers, 0x301) = value;
      return CPU_ALERT_HALT;

    default:
      if ((address & 0x01) != 0)
      {
        address &= ~0x01;
        ACCESS_REGISTER8_HIGH(address);
      }
      else
      {
        ACCESS_REGISTER8_LOW(address);
      }
      return write_io_register16(address, value);
  }

  return CPU_ALERT_NONE;
}


CPU_ALERT_TYPE write_io_register16(u32 address, u32 value)
{
  switch(address)
  {
    // DISPCNT
    case 0x00:
    {
      u16 bg_mode = value & 0x07;
      u16 dispcnt = io_registers[REG_DISPCNT];

      if (bg_mode > 5)
        value &= 0x07;

      if (bg_mode != (dispcnt & 0x07))
        oam_update = 1;

      if ((((dispcnt ^ value) & 0x80) != 0) && ((value & 0x80) == 0))
      {
        if ((io_registers[REG_DISPSTAT] & 0x01) == 0)
          io_registers[REG_DISPSTAT] &= 0xFFFC;
      }

      ADDRESS16(io_registers, 0x00) = value;
      break;
    }

    // DISPSTAT
    case 0x04:
      ADDRESS16(io_registers, 0x04) =
       (ADDRESS16(io_registers, 0x04) & 0x07) | (value & ~0x07);
      break;

    // VCOUNT
    case 0x06:
      /* Read only */
      break;

    // BG2 reference X
    case 0x28:
      ACCESS_REGISTER16_LOW(0x28);
      affine_reference_x[0] = (s32)(value << 4) >> 4;
      ADDRESS32(io_registers, 0x28) = value;
      break;

    case 0x2A:
      ACCESS_REGISTER16_HIGH(0x28);
      affine_reference_x[0] = (s32)(value << 4) >> 4;
      ADDRESS32(io_registers, 0x28) = value;
      break;

    // BG2 reference Y
    case 0x2C:
      ACCESS_REGISTER16_LOW(0x2C);
      affine_reference_y[0] = (s32)(value << 4) >> 4;
      ADDRESS32(io_registers, 0x2C) = value;
      break;

    case 0x2E:
      ACCESS_REGISTER16_HIGH(0x2C);
      affine_reference_y[0] = (s32)(value << 4) >> 4;
      ADDRESS32(io_registers, 0x2C) = value;
      break;

    // BG3 reference X
    case 0x38:
      ACCESS_REGISTER16_LOW(0x38);
      affine_reference_x[1] = (s32)(value << 4) >> 4;
      ADDRESS32(io_registers, 0x38) = value;
      break;

    case 0x3A:
      ACCESS_REGISTER16_HIGH(0x38);
      affine_reference_x[1] = (s32)(value << 4) >> 4;
      ADDRESS32(io_registers, 0x38) = value;
      break;

    // BG3 reference Y
    case 0x3C:
      ACCESS_REGISTER16_LOW(0x3C);
      affine_reference_y[1] = (s32)(value << 4) >> 4;
      ADDRESS32(io_registers, 0x3C) = value;
      break;

    case 0x3E:
      ACCESS_REGISTER16_HIGH(0x3C);
      affine_reference_y[1] = (s32)(value << 4) >> 4;
      ADDRESS32(io_registers, 0x3C) = value;
      break;

    // Sound 1 control sweep
    case 0x60:
      GBC_SOUND_TONE_CONTROL_SWEEP();
      break;

    // Sound 1 control duty/length/envelope
    case 0x62:
      GBC_SOUND_TONE_CONTROL_LOW(0, 0x62);
      break;

    // Sound 1 control frequency
    case 0x64:
      GBC_SOUND_TONE_CONTROL_HIGH(0, 0x64);
      break;

    // Sound 2 control duty/length/envelope
    case 0x68:
      GBC_SOUND_TONE_CONTROL_LOW(1, 0x68);
      break;

    // Sound 2 control frequency
    case 0x6C:
      GBC_SOUND_TONE_CONTROL_HIGH(1, 0x6C);
      break;

    // Sound 3 control wave
    case 0x70:
      GBC_SOUND_WAVE_CONTROL();
      break;

    // Sound 3 control length/volume
    case 0x72:
      GBC_SOUND_TONE_CONTROL_LOW_WAVE();
      break;

    // Sound 3 control frequency
    case 0x74:
      GBC_SOUND_TONE_CONTROL_HIGH_WAVE();
      break;

    // Sound 4 control length/envelope
    case 0x78:
      GBC_SOUND_TONE_CONTROL_LOW(3, 0x78);
      break;

    // Sound 4 control frequency
    case 0x7C:
      GBC_SOUND_NOISE_CONTROL();
      break;

    // Sound control L
    case 0x80:
      GBC_TRIGGER_SOUND();
      break;

    // Sound control H
    case 0x82:
      TRIGGER_SOUND();
      break;

    // Sound control X
    case 0x84:
      SOUND_ON();
      break;

    // Sound wave RAM
    case 0x90 ... 0x9E:
      GBA_SOUND_WAVE_PATTERN_RAM16();
      ADDRESS16(io_registers, address) = value;
      break;

    // Sound FIFO A
    case 0xA0:
    case 0xA2:
      ADDRESS16(io_registers, address) = value;
      sound_timer_queue(0);
      break;

    // Sound FIFO B
    case 0xA4:
    case 0xA6:
      ADDRESS16(io_registers, address) = value;
      sound_timer_queue(1);
      break;

    // DMA Source Address High (internal memory)
    case 0xB2:
    // DMA Destination Address High (internal memory)
    case 0xB6: case 0xC2: case 0xCE:
      ADDRESS16(io_registers, address) = value & 0x07FF;
      break;

    // DMA Source Address High (any memory)
    case 0xBE: case 0xCA: case 0xD6:
    // DMA Destination Address High (any memory)
    case 0xDA:
      ADDRESS16(io_registers, address) = value & 0x0FFF;
      break;

    // DMA Word Count (14 bit)
    case 0xB8: case 0xC4: case 0xD0:
      ADDRESS16(io_registers, address) = value & 0x3FFF;
      break;

    // DMA control
    case 0xBA:  // DMA channel 0
    case 0xC6:  // DMA channel 1
    case 0xD2:  // DMA channel 2
    case 0xDE:  // DMA channel 3
      ADDRESS16(io_registers, address) = value;
      trigger_dma(address);
      break;

    // Timer counts
    case 0x100:
    case 0x104:
    case 0x108:
    case 0x10C:
      COUNT_TIMER(address);
      break;

    // Timer control
    case 0x102:  // Timer 0
    case 0x106:  // Timer 1
    case 0x10A:  // Timer 2
    case 0x10E:  // Timer 3
      TRIGGER_TIMER(address);
      break;

#ifdef USE_ADHOC
// SIOCNT
//      Bit   Expl.
//      0-1   Baud Rate          (0-3: 9600,38400,57600,115200 bps)
//      2     SI-Terminal        (0=Parent, 1=Child)                  (Read Only)
//      3     SD-Terminal        (0=Bad connection, 1=All GBAs Ready) (Read Only)

//      4-5   Multi-Player ID    (0=Parent, 1-3=1st-3rd child)        (Read Only)
//      6     Multi-Player Error (0=Normal, 1=Error)                  (Read Only)
//      7     Start/Busy Bit     (0=Inactive, 1=Start/Busy) (Read Only for Slaves)

//      8-11  Not used           (R/W, should be 0)

//      12    Must be "0" for Multi-Player mode
//      13    Must be "1" for Multi-Player mode
//      14    IRQ Enable         (0=Disable, 1=Want IRQ upon completion)
//      15    Not used           (Read only, always 0)
    case 0x128:
      if(g_adhoc_link_flag == NO)
        return sio_control(value);
      else
      {
        switch(sio_mode(value, ADDRESS16(io_registers, 0x134)))
        {
          case MULTIPLAYER:  // マルチプレイヤーモード
            if(value & 0x80) // bit7(start bit)が1の時 転送開始命令
            {
              if(!g_multi_id)  // 親モードの時 g_multi_id = 0
              {
                if(!g_adhoc_transfer_flag) // g_adhoc_transfer_flag == 0 転送中で無いとき
                {
                  g_multi_mode = MULTI_START; // データの送信
                } // 転送中の時
                value |= (g_adhoc_transfer_flag != 0)<<7;
              }
            }
            if(g_multi_id)
            {
              value &= 0xf00b;
              value |= 0x8;
              value |= g_multi_id << 4;
              ADDRESS16(io_registers, 0x128) = value;
              ADDRESS16(io_registers, 0x134) = 7; // 親と子で0x134の設定値を変える
            }
            else
            {
              value &= 0xf00f;
              value |= 0xc;
              value |= g_multi_id << 4;
              ADDRESS16(io_registers, 0x128) = value;
              ADDRESS16(io_registers, 0x134) = 3;
            }
            break;
        }
      }

    // SIOMLT_SEND
    case 0x12A:
      ADDRESS16(io_registers, 0x12A) = value;
      break;

    // RCNT
    case 0x134:
      if(!value) // 0の場合
      {
        ADDRESS16(io_registers, 0x134) = 0;
        return CPU_ALERT_NONE;
      }
      switch(sio_mode(ADDRESS16(io_registers, 0x128), value))
      {
        case MULTIPLAYER:
          value &= 0xc0f0;
          value |= 3;
          if(g_multi_id) value |= 4;
          ADDRESS16(io_registers, 0x134) = value;
          ADDRESS16(io_registers, 0x128) = ((ADDRESS16(io_registers, 0x128)&0xff8b)|(g_multi_id ? 0xc : 8)|(g_multi_id<<4));
          break;

        default:
          ADDRESS16(io_registers, 0x134) = value;
          break;
      }
      break;
#else
    case 0x128:
      return sio_control(value);
      break;
#endif

    // P1
    case 0x130:
      /* Read only */
      break;

    // IE - Interrupt Enable Register
    case 0x200:
      value &= 0x3FFF;
      ADDRESS16(io_registers, 0x200) = value;
      if (((value & io_registers[REG_IF]) != 0) && GBA_IME_STATE && ARM_IRQ_STATE)
      {
        return CPU_ALERT_IRQ;
      }
      break;

    // IF - Interrupt Request flags
    case 0x202:
      ADDRESS16(io_registers, 0x202) &= (~value & 0x3FFF);
      break;

    // WAITCNT
    case 0x204:
      waitstate_control(value);
      break;

    // IME - Interrupt Master Enable Register
    case 0x208:
      value &= 0x0001;
      ADDRESS16(io_registers, 0x208) = value;
      if (((io_registers[REG_IE] & io_registers[REG_IF]) != 0) && (value != 0) && ARM_IRQ_STATE)
      {
        return CPU_ALERT_IRQ;
      }
      break;

    // Halt
    case 0x300:
      if ((value & 0x8000) != 0)
        reg[CPU_HALT_STATE] = CPU_STOP;
      else
        reg[CPU_HALT_STATE] = CPU_HALT;

      ADDRESS16(io_registers, 0x300) = value;
      return CPU_ALERT_HALT;

    default:
      ADDRESS16(io_registers, address) = value;
      break;
  }

  return CPU_ALERT_NONE;
}


CPU_ALERT_TYPE write_io_register32(u32 address, u32 value)
{
  switch(address)
  {
    // BG2 reference X
    case 0x28:
      affine_reference_x[0] = (s32)(value << 4) >> 4;
      ADDRESS32(io_registers, 0x28) = value;
      break;

    // BG2 reference Y
    case 0x2C:
      affine_reference_y[0] = (s32)(value << 4) >> 4;
      ADDRESS32(io_registers, 0x2C) = value;
      break;

    // BG3 reference X
    case 0x38:
      affine_reference_x[1] = (s32)(value << 4) >> 4;
      ADDRESS32(io_registers, 0x38) = value;
      break;

    // BG3 reference Y
    case 0x3C:
      affine_reference_y[1] = (s32)(value << 4) >> 4;
      ADDRESS32(io_registers, 0x3C) = value;
      break;

    // Sound FIFO A
    case 0xA0:
      ADDRESS32(io_registers, 0xA0) = value;
      sound_timer_queue(0);
      break;

    // Sound FIFO B
    case 0xA4:
      ADDRESS32(io_registers, 0xA4) = value;
      sound_timer_queue(1);
      break;

    // DMA Source Address (internal memory)
    case 0xB0:
    // DMA Destination Address (internal memory)
    case 0xB4: case 0xC0: case 0xCC:
      ADDRESS32(io_registers, address) = value & 0x07FFFFFF;
      break;

    // DMA Source Address (any memory)
    case 0xBC: case 0xC8: case 0xD4:
    // DMA Destination Address (any memory)
    case 0xD8:
      ADDRESS32(io_registers, address) = value & 0x0FFFFFFF;
      break;

    // SIO Data (Normal-32bit Mode)
    case 0x120:
    // SIO JOY Bus
    case 0x150: case 0x154:
      ADDRESS32(io_registers, address) = value;
      break;

    default:
    {
      CPU_ALERT_TYPE alert_low  = write_io_register16(address, value & 0xFFFF);
      CPU_ALERT_TYPE alert_high = write_io_register16(address + 2, value >> 16);

      return alert_high | alert_low;
    }
  }

  return CPU_ALERT_NONE;
}


// EEPROM is 512 bytes by default; it is autodetecte as 8KB if
// 14bit address DMAs are made (this is done in the DMA handler).

CPU_ALERT_TYPE write_eeprom(u32 address, u32 value)
{
  // ROM is restricted to 8000000h-9FFFeFFh
  // (max.1FFFF00h bytes = 32MB minus 256 bytes)
  if (gamepak_size > 0x1FFFF00)
  {
    gamepak_size = 0x1FFFF00;
  }

  switch(eeprom_mode)
  {
    case EEPROM_BASE_MODE:
      backup_type = BACKUP_EEPROM;
      eeprom_buffer[0] |= (value & 0x01) << (1 - eeprom_counter);
      eeprom_counter++;
      if(eeprom_counter == 2)
      {
        if(eeprom_size == EEPROM_512_BYTE)
          eeprom_address_length = 6;
        else
          eeprom_address_length = 14;

        eeprom_counter = 0;

        switch(eeprom_buffer[0] & 0x03)
        {
          case 0x02:
            eeprom_mode = EEPROM_WRITE_ADDRESS_MODE;
            break;

          case 0x03:
            eeprom_mode = EEPROM_ADDRESS_MODE;
            break;
        }
        eeprom_buffer[0] = 0;
        eeprom_buffer[1] = 0;
      }
      break;

    case EEPROM_ADDRESS_MODE:
    case EEPROM_WRITE_ADDRESS_MODE:
      eeprom_buffer[eeprom_counter / 8] |= (value & 0x01) << (7 - (eeprom_counter % 8));
      eeprom_counter++;
      if(eeprom_counter == eeprom_address_length)
      {
        if(eeprom_size == EEPROM_512_BYTE)
        {
          // Little endian access
          eeprom_address = (((u32)eeprom_buffer[0] >> 2) | ((u32)eeprom_buffer[1] << 6)) * 8;
        }
        else
        {
          // Big endian access
          eeprom_address = (((u32)eeprom_buffer[1] >> 2) | ((u32)eeprom_buffer[0] << 6)) * 8;
        }
        eeprom_buffer[0] = 0;
        eeprom_buffer[1] = 0;

        eeprom_counter = 0;

        if(eeprom_mode == EEPROM_ADDRESS_MODE)
        {
          eeprom_mode = EEPROM_ADDRESS_FOOTER_MODE;
        }
        else
        {
          eeprom_mode = EEPROM_WRITE_MODE;
          memset(gamepak_backup + eeprom_address, 0, 8);
        }
      }
      break;

    case EEPROM_WRITE_MODE:
      gamepak_backup[eeprom_address + (eeprom_counter / 8)] |= (value & 0x01) << (7 - (eeprom_counter % 8));
      eeprom_counter++;
      if(eeprom_counter == 64)
      {
        backup_update = WRITE_BACKUP_DELAY;
        eeprom_mode = EEPROM_WRITE_FOOTER_MODE;
        eeprom_counter = 0;
      }
      break;

    case EEPROM_ADDRESS_FOOTER_MODE:
    case EEPROM_WRITE_FOOTER_MODE:
      eeprom_counter = 0;
      if(eeprom_mode == EEPROM_ADDRESS_FOOTER_MODE)
        eeprom_mode = EEPROM_READ_HEADER_MODE;
      else
        eeprom_mode = EEPROM_BASE_MODE;
      break;

    default:
    case EEPROM_READ_MODE:
    case EEPROM_READ_HEADER_MODE:
      break;
  }

  return CPU_ALERT_NONE;
}

u32 read_eeprom(void)
{
  u32 value;

  switch(eeprom_mode)
  {
    case EEPROM_BASE_MODE:
      value = 1;
      break;

    case EEPROM_READ_MODE:
      value = (gamepak_backup[eeprom_address + (eeprom_counter / 8)] >> (7 - (eeprom_counter % 8))) & 0x01;
      eeprom_counter++;
      if(eeprom_counter == 64)
      {
        eeprom_mode = EEPROM_BASE_MODE;
        eeprom_counter = 0;
      }
      break;

    case EEPROM_READ_HEADER_MODE:
      value = 0;
      eeprom_counter++;
      if(eeprom_counter == 4)
      {
        eeprom_mode = EEPROM_READ_MODE;
        eeprom_counter = 0;
      }
      break;

    default:
      value = 0;
      break;
  }

  return value;
}


u8 read_backup(u32 address)
{
  u8 value = 0;

  if(backup_type == BACKUP_NONE)
    backup_type = BACKUP_SRAM;

  switch (backup_type)
  {
    case BACKUP_SRAM:
      value = gamepak_backup[address];
      break;

    case BACKUP_FLASH:
      if(flash_mode == FLASH_ID_MODE)
      {
        /* ID manufacturer type */
        if(address == 0x0000)
          value = flash_manufacturer_id;
        else
        /* ID device type */
        if(address == 0x0001)
          value = flash_device_id;
      }
      else
      {
        value = gamepak_backup[flash_bank_offset + address];
      }
      break;

    case BACKUP_EEPROM:
      // Tilt Sensor
      switch(address)
      {
        case 0x8200:
          value = tilt_sensor_x & 255;
          break;
        case 0x8300:
          value = (tilt_sensor_x >> 8) | 0x80;
          break;
        case 0x8400:
          value = tilt_sensor_y & 255;
          break;
        case 0x8500:
          value = tilt_sensor_y >> 8;
          break;
      }
      break;

    case BACKUP_NONE:
      break;
  }

  return value;
}

CPU_ALERT_TYPE write_backup(u32 address, u32 value)
{
  value &= 0xFF;

  // Tilt Sensor
  if (backup_type == BACKUP_EEPROM)
  {
    // E008000h (W) Write 55h to start sampling
    // E008100h (W) Write AAh to start sampling
    return CPU_ALERT_NONE;
  }

  if (backup_type == BACKUP_NONE)
  {
    backup_type = BACKUP_SRAM;
  }

  // gamepak SRAM or Flash ROM
  if((address == 0x5555) && (flash_mode != FLASH_WRITE_MODE))
  {
    if((flash_command_position == 0) && (value == 0xAA))
    {
      backup_type = BACKUP_FLASH;
      flash_command_position = 1;
    }

    if(flash_command_position == 2)
    {
      switch(value)
      {
        case 0x90:
          // Enter ID mode, this also tells the emulator that we're using
          // flash, not SRAM
          if(flash_mode == FLASH_BASE_MODE)
            flash_mode = FLASH_ID_MODE;
          break;

        case 0x80:
          // Enter erase mode
          if(flash_mode == FLASH_BASE_MODE)
            flash_mode = FLASH_ERASE_MODE;
          break;

        case 0xF0:
          // Terminate ID mode
          if(flash_mode == FLASH_ID_MODE)
            flash_mode = FLASH_BASE_MODE;
          break;

        case 0xA0:
          // Write mode
          if(flash_mode == FLASH_BASE_MODE)
            flash_mode = FLASH_WRITE_MODE;
          break;

        case 0xB0:
          // Bank switch
          // Here the chip is now officially 128KB.
          flash_size = FLASH_SIZE_128KB;

          if(flash_mode == FLASH_BASE_MODE)
            flash_mode = FLASH_BANKSWITCH_MODE;
          break;

        case 0x10:
          // Erase chip
          if(flash_mode == FLASH_ERASE_MODE)
          {
            if(flash_size == FLASH_SIZE_64KB)
              memset(gamepak_backup, 0xFF, 1024 * 64);
            else
              memset(gamepak_backup, 0xFF, 1024 * 128);

            backup_update = WRITE_BACKUP_DELAY;
            flash_mode = FLASH_BASE_MODE;
          }
          break;

        default:
          break;
      }
      flash_command_position = 0;
    }

    if (backup_type == BACKUP_SRAM)
    {
      backup_update = WRITE_BACKUP_DELAY;
      gamepak_backup[0x5555] = value;
    }
  }
  else
  {
    if((flash_command_position == 1) && (address == 0x2AAA) && (value == 0x55))
    {
      flash_command_position = 2;
    }
    else
    {
      if((flash_command_position == 2) && (flash_mode == FLASH_ERASE_MODE) && (value == 0x30))
      {
        // Erase sector
        memset(gamepak_backup + flash_bank_offset + (address & 0xF000), 0xFF, 1024 * 4);
        backup_update = WRITE_BACKUP_DELAY;
        flash_mode = FLASH_BASE_MODE;
        flash_command_position = 0;
      }
      else
      {
        if((flash_command_position == 0) && (address == 0x0000) && (flash_mode == FLASH_BANKSWITCH_MODE) && (flash_size == FLASH_SIZE_128KB))
        {
          flash_bank_offset = ((value & 0x01) * (1024 * 64));
          flash_mode = FLASH_BASE_MODE;
        }
        else
        {
          if((flash_command_position == 0) && (flash_mode == FLASH_WRITE_MODE))
          {
            // Write value to flash ROM
            backup_update = WRITE_BACKUP_DELAY;

            gamepak_backup[flash_bank_offset + address] = value;
            flash_mode = FLASH_BASE_MODE;
          }
        }
      }
    }

    if(backup_type == BACKUP_SRAM)
    {
      // Write value to SRAM
      backup_update = WRITE_BACKUP_DELAY;

      // Hit 64KB territory?
      if(address >= 0x8000)
        sram_size = SRAM_SIZE_64KB;

      gamepak_backup[address] = value;
    }
  }

  return CPU_ALERT_NONE;
}


// RTC code derived from VBA's (due to lack of any real publically available
// documentation...)

static u32 encode_bcd(u8 value)
{
  return ((value / 10) << 4) | (value % 10);
}

#define WRITE_RTC_REGISTER(index, _value)                                     \
  update_address = 0x80000C4 + (index << 1);                                  \
  rtc_registers[index] = _value;                                              \
  rtc_page_index = update_address >> 15;                                      \
  map = memory_map_read[rtc_page_index];                                      \
                                                                              \
  if(map == NULL)                                                             \
    map = load_gamepak_page(rtc_page_index & 0x3FF);                          \
                                                                              \
  ADDRESS16(map, update_address & 0x7FFF) = _value;                           \

CPU_ALERT_TYPE write_rtc(u32 address, u32 value)
{
  u32 rtc_page_index;
  u32 update_address;
  u8 *map = NULL;

  address &= 0xFF;
  value &= 0xFFFF;

  switch(address)
  {
    // RTC command
    // Bit 0: SCHK, perform action
    // Bit 1: IO, input/output command data
    // Bit 2: CS, select input/output? If high make I/O write only
    case 0xC4:
      if(rtc_state == RTC_DISABLED)
        rtc_state = RTC_IDLE;

      if((rtc_registers[0] & 0x04) == 0)
        value = (rtc_registers[0] & 0x02) | (value & ~0x02);

      if((rtc_registers[2] & 0x01) != 0)
      {
        // To begin writing a command 1, 5 must be written to the command
        // registers.
        if((rtc_state == RTC_IDLE) && (rtc_registers[0] == 0x01) && (value == 0x05))
        {
          // We're now ready to begin receiving a command.
          WRITE_RTC_REGISTER(0, value);
          rtc_state = RTC_COMMAND;
          rtc_command = 0;
          rtc_bit_count = 7;
        }
        else
        {
          WRITE_RTC_REGISTER(0, value);
          switch(rtc_state)
          {
            // Accumulate RTC command by receiving the next bit, and if we
            // have accumulated enough bits to form a complete command
            // execute it.
            case RTC_COMMAND:
              if((rtc_registers[0] & 0x01) != 0)
              {
                rtc_command |= ((value & 0x02) >> 1) << rtc_bit_count;
                rtc_bit_count--;
              }

              // Have we received a full RTC command? If so execute it.
              if(rtc_bit_count < 0)
              {
                switch(rtc_command)
                {
                  // Resets RTC
                  case RTC_COMMAND_RESET:
                    rtc_state = RTC_IDLE;
                    memset(rtc_registers, 0, sizeof(rtc_registers));
                    break;

                  // Sets status of RTC
                  case RTC_COMMAND_WRITE_STATUS:
                    rtc_state = RTC_INPUT_DATA;
                    rtc_data_bytes = 1;
                    rtc_write_mode = RTC_WRITE_STATUS;
                    break;

                  // Outputs current status of RTC
                  case RTC_COMMAND_READ_STATUS:
                    rtc_state = RTC_OUTPUT_DATA;
                    rtc_data_bytes = 1;
                    rtc_data[0] = rtc_status;
                    break;

                  // Actually outputs the time, all of it
                  // 0x65
                  case RTC_COMMAND_OUTPUT_TIME_FULL:
                  {
                    rtc_state = RTC_OUTPUT_DATA;
                    rtc_data_bytes = 7;

                    struct ReGBA_RTC Time;
                    ReGBA_LoadRTCTime(&Time);
                    rtc_data[0] = encode_bcd(Time.year % 100);
                    rtc_data[1] = encode_bcd(Time.month);
                    rtc_data[2] = encode_bcd(Time.day);
                    rtc_data[3] = encode_bcd(Time.weekday);
                    rtc_data[4] = encode_bcd(Time.hours);
                    rtc_data[5] = encode_bcd(Time.minutes);
                    rtc_data[6] = encode_bcd(Time.seconds);

                    break;
                  }

                  // Only outputs the current time of day.
                  // 0x67
                  case RTC_COMMAND_OUTPUT_TIME:
                  {
                    rtc_state = RTC_OUTPUT_DATA;
                    rtc_data_bytes = 3;

                    struct ReGBA_RTC Time;
                    ReGBA_LoadRTCTime(&Time);
                    rtc_data[0] = encode_bcd(Time.hours);
                    rtc_data[1] = encode_bcd(Time.minutes);
                    rtc_data[2] = encode_bcd(Time.seconds);

                    break;
                  }
                }
                rtc_bit_count = 0;
              }
              break;

            // Receive parameters from the game as input to the RTC
            // for a given command. Read one bit at a time.
            case RTC_INPUT_DATA:
              // Bit 1 of parameter A must be high for input
              if((rtc_registers[1] & 0x02) != 0)
              {
                // Read next bit for input
                if((value & 0x01) == 0)
                {
                  rtc_data[rtc_bit_count >> 3] |= ((value & 0x01) << (7 - (rtc_bit_count & 0x07)));
                }
                else
                {
                  rtc_bit_count++;

                  if((u32)rtc_bit_count == (rtc_data_bytes * 8))
                  {
                    rtc_state = RTC_IDLE;
                    switch(rtc_write_mode)
                    {
                      case RTC_WRITE_STATUS:
                        rtc_status = rtc_data[0];
                        break;

                      default:
                      case RTC_WRITE_TIME:
                      case RTC_WRITE_TIME_FULL:
                        break;
                    }
                  }
                }
              }
              break;

            case RTC_OUTPUT_DATA:
              // Bit 1 of parameter A must be low for output
              if((rtc_registers[1] & 0x02) == 0)
              {
                // Write next bit to output, on bit 1 of parameter B
                if((value & 0x01) == 0)
                {
                  u8 current_output_byte = rtc_registers[2];

                  current_output_byte =
                   (current_output_byte & ~0x02) |
                   (((rtc_data[rtc_bit_count >> 3] >> (rtc_bit_count & 0x07)) & 0x01) << 1);

                  WRITE_RTC_REGISTER(0, current_output_byte);
                }
                else
                {
                  rtc_bit_count++;

                  if((u32)rtc_bit_count == (rtc_data_bytes * 8))
                  {
                    rtc_state = RTC_IDLE;
                    memset(rtc_registers, 0, sizeof(rtc_registers));
                  }
                }
              }
              break;

            default:
            case RTC_DISABLED:
            case RTC_IDLE:
              break;
          }
        }
      }
      else
      {
        WRITE_RTC_REGISTER(2, value);
      }
      break;

    // Write parameter A
    case 0xC6:
      WRITE_RTC_REGISTER(1, value);
      break;

    // Write parameter B
    case 0xC8:
      WRITE_RTC_REGISTER(2, value);
      break;
  }

  return CPU_ALERT_NONE;
}


u8 read_memory8(u32 address)
{
  u32 region = address >> 24;

  if ((region & 0xF0) != 0)
    return read8_open(address);

  return (u8)(*mem_read8[region])(address);
}

s16 read_memory16_signed(u32 address)
{
  if ((address & 0x01) != 0)
    return (s8)read_memory8(address);

  return (s16)read_memory16(address);
}

// unaligned reads are actually 32bit

u32 read_memory16(u32 address)
{
  u32 value;
  u32 region = address >> 24;
  u32 rotate = address & 0x01;

  if ((region & 0xF0) != 0)
    value = read16_open(address);
  else
    value = (*mem_read16[region])(address & ~0x01);

  if (rotate != 0)
  {
    ROR(value, value, 8);
  }

  return value;
}

u32 read_memory32(u32 address)
{
  u32 value;
  u32 region = address >> 24;
  u32 rotate = (address & 0x03) << 3;

  if ((region & 0xF0) != 0)
    value = read32_open(address);
  else
    value = (*mem_read32[region])(address & ~0x03);

  if (rotate != 0)
  {
    ROR(value, value, rotate);
  }

  return value;
}


u8 read_open_memory8(u32 address)
{
  if (cpu_dma_hack != 0)
    return cpu_dma_last & 0xFF;

  return (u8)(*open_read8[address >> 24])(address);
}

u16 read_open_memory16(u32 address)
{
  if (cpu_dma_hack != 0)
    return cpu_dma_last & 0xFFFF;

  return (u16)(*open_read16[address >> 24])(address);
}

u32 read_open_memory32(u32 address)
{
  if (cpu_dma_hack != 0)
    return cpu_dma_last;

  return (*open_read32[address >> 24])(address);
}


CPU_ALERT_TYPE write_memory8(u32 address, u8 value)
{
  u32 region = address >> 24;

  if ((region & 0xF0) != 0)
    return CPU_ALERT_NONE;

  return (*mem_write8[region])(address, value);
}

CPU_ALERT_TYPE write_memory16(u32 address, u16 value)
{
  u32 region = address >> 24;

  if ((region & 0xF0) != 0)
    return CPU_ALERT_NONE;

  return (*mem_write16[region])(address & ~0x01, value);
}

CPU_ALERT_TYPE write_memory32(u32 address, u32 value)
{
  u32 region = address >> 24;

  if ((region & 0xF0) != 0)
    return CPU_ALERT_NONE;

  return (*mem_write32[region])(address & ~0x03, value);
}


// 2N + 2(n-1)S + xI
// 2I (normally), or 4I (if both source and destination are in gamepak memory area)

extern u32 dma_cycle_count;

#define COUNT_DMA_CYCLES()                                                    \
{                                                                             \
  u8 *ws_n = memory_waitstate[0 + length_type];                               \
  u8 *ws_s = memory_waitstate[2 + length_type];                               \
  u8 src_region = src_ptr >> 24;                                              \
  u8 dest_region = dest_ptr >> 24;                                            \
                                                                             \
  dma_cycle_count += ws_n[src_region] + ws_n[dest_region] + 2                 \
    + ((ws_s[src_region] + ws_s[dest_region] + 2) * (length - 1)) + 2;        \
}                                                                             \

#define DMA_TRANSFER_LOOP(type)                                               \
  while (length--)                                                            \
  {                                                                           \
    read_value = (*dma_read##type[src_ptr >> 24])(src_ptr);                   \
    src_ptr += src_increment;                                                 \
                                                                              \
    return_value |= (*dma_write##type[dest_ptr >> 24])(dest_ptr, read_value); \
    dest_ptr += dest_increment;                                               \
  }                                                                           \

CPU_ALERT_TYPE dma_transfer(DmaTransferType *dma)
{
  u32 read_value = 0;
  CPU_ALERT_TYPE return_value = CPU_ALERT_DMA;

  DMA_LENGTH_TYPE length_type = dma->length_type;
  u32 length = dma->length;

  u32 src_ptr  = dma->source_address;
  u32 dest_ptr = dma->dest_address;

  s32 src_increment, dest_increment;

  COUNT_DMA_CYCLES();

  if (length_type == DMA_16BIT)
  {
    src_ptr  &= ~0x01;
    dest_ptr &= ~0x01;

    src_increment  = dma_addr_control[dma->source_direction];
    dest_increment = dma_addr_control[dma->dest_direction];

    DMA_TRANSFER_LOOP(16);

    cpu_dma_last = read_value | (read_value << 16);
  }
  else
  {
    src_ptr  &= ~0x03;
    dest_ptr &= ~0x03;

    src_increment  = dma_addr_control[dma->source_direction] << 1;
    dest_increment = dma_addr_control[dma->dest_direction] << 1;

    DMA_TRANSFER_LOOP(32);

    cpu_dma_last = read_value;
  }


  cpu_dma_hack = 1;

  u32 offset_address = dma->dma_channel * 12;

  dma->source_address = src_ptr;

  if (dma->dest_direction == DMA_RELOAD)
    dma->dest_address = ADDRESS32(io_registers, 0xB4 + offset_address);
  else
    dma->dest_address = dest_ptr;

  if ((dma->repeat_type == DMA_REPEAT) && (dma->start_type != DMA_START_SPECIAL))
  {
    u32 length_max = (dma->dma_channel == 3) ? 0x10000 : 0x4000;
    length = ADDRESS16(io_registers, 0xB8 + offset_address);

    dma->length = (length == 0) ? length_max : length;
  }

  if ((dma->repeat_type == DMA_NO_REPEAT) || (dma->start_type == DMA_START_IMMEDIATELY))
  {
    dma->start_type = DMA_INACTIVE;
    dma->direct_sound_channel = DMA_NO_DIRECT_SOUND;

    ADDRESS16(io_registers, 0xBA + offset_address) &= 0x7FFF;
  }

  if (dma->irq != 0)
  {
    io_registers[REG_IF] |= (IRQ_DMA0 << dma->dma_channel);
    return_value |= CPU_ALERT_IRQ;
  }

  return return_value;
}


// Be sure to do this after loading ROMs.

#define MAP_REGION(type, start, end, mirror_blocks, region)                   \
  for(map_offset = (start) / 0x8000; map_offset < ((end) / 0x8000);           \
   map_offset++)                                                              \
  {                                                                           \
    memory_map_##type[map_offset] =                                           \
     ((u8 *)region) + ((map_offset % mirror_blocks) * 0x8000);                \
  }                                                                           \

#define MAP_NULL(type, start, end)                                            \
  for(map_offset = (start) / 0x8000; map_offset < ((end) / 0x8000);           \
   map_offset++)                                                              \
  {                                                                           \
    memory_map_##type[map_offset] = NULL;                                     \
  }                                                                           \

#define MAP_VRAM(type)                                                        \
  for(map_offset = 0x6000000 / 0x8000; map_offset < (0x7000000 / 0x8000);     \
   map_offset += 4)                                                           \
  {                                                                           \
    memory_map_##type[map_offset] = vram;                                     \
    memory_map_##type[map_offset + 1] = vram + 0x8000;                        \
    memory_map_##type[map_offset + 2] = vram + (0x8000 * 2);                  \
    memory_map_##type[map_offset + 3] = vram + (0x8000 * 2);                  \
  }                                                                           \


static u32 evict_gamepak_page(void)
{
	// We will evict the page with index gamepak_next_swap, a bit like a ring
	// buffer.
	u32 page_index = gamepak_next_swap;
	gamepak_next_swap++;
	if (gamepak_next_swap >= gamepak_ram_pages)
		gamepak_next_swap = 0;
	u16 physical_index = gamepak_memory_map[page_index];

	memory_map_read[(0x8000000 / (32 * 1024)) + physical_index] = NULL;
	memory_map_read[(0xA000000 / (32 * 1024)) + physical_index] = NULL;
	memory_map_read[(0xC000000 / (32 * 1024)) + physical_index] = NULL;

#if TRACE_MEMORY
	ReGBA_Trace("T: Evicting virtual page %u", page_index);
#endif
	
	return page_index;
}

u8 *load_gamepak_page(u16 physical_index)
{
	if (memory_map_read[(0x08000000 / (32 * 1024)) + (uint32_t) physical_index] != NULL)
	{
#if TRACE_MEMORY
		ReGBA_Trace("T: Not reloading already loaded Game Pak page %u (%08X..%08X)", (uint32_t) physical_index, 0x08000000 + physical_index * (32 * 1024), 0x08000000 + (uint32_t) physical_index * (32 * 1024) + 0x7FFF);
#endif
		return memory_map_read[(0x08000000 / (32 * 1024)) + (uint32_t) physical_index];
	}
#if TRACE_MEMORY
	ReGBA_Trace("T: Loading Game Pak page %u (%08X..%08X)", (uint32_t) physical_index, 0x08000000 + (uint32_t) physical_index * (32 * 1024), 0x08000000 + (uint32_t) physical_index * (32 * 1024) + 0x7FFF);
#endif
	if((uint32_t) physical_index >= (gamepak_size >> 15))
		return gamepak_rom;

	u16 page_index = evict_gamepak_page();
	u32 page_offset = (uint32_t) page_index * (32 * 1024);
	u8 *swap_location = gamepak_rom + page_offset;

	gamepak_memory_map[page_index] = physical_index;

	FILE_SEEK(gamepak_file_large, (off_t) physical_index * (32 * 1024), SEEK_SET);
	FILE_READ(gamepak_file_large, swap_location, (32 * 1024));

	memory_map_read[(0x8000000 / (32 * 1024)) + physical_index] =
		memory_map_read[(0xA000000 / (32 * 1024)) + physical_index] =
		memory_map_read[(0xC000000 / (32 * 1024)) + physical_index] = swap_location;

	// If RTC is active page the RTC register bytes so they can be read
	if((rtc_state != RTC_DISABLED) && (physical_index == 0))
	{
		memcpy(swap_location + 0xC4, rtc_registers, sizeof(rtc_registers));
	}

	return swap_location;
}

static void init_memory_gamepak(void)
{
  u32 map_offset = 0;

  if (FILE_CHECK_VALID(gamepak_file_large))
  {
    // Large ROMs get special treatment because they
    // can't fit into the ROM buffer.
    // The size of this buffer varies per platform, and may actually
    // fit all of the ROM, in which case this is dead code.
    memset(gamepak_memory_map, 0, sizeof(gamepak_memory_map));
    gamepak_next_swap = 0;

    MAP_NULL(read, 0x8000000, 0xE000000);
  }
  else
  {
    MAP_REGION(read, 0x8000000, 0x8000000 + gamepak_size, 1024, gamepak_rom);
    MAP_NULL(read, 0x8000000 + gamepak_size, 0xA000000);
    MAP_REGION(read, 0xA000000, 0xA000000 + gamepak_size, 1024, gamepak_rom);
    MAP_NULL(read, 0xA000000 + gamepak_size, 0xC000000);
    MAP_REGION(read, 0xC000000, 0xC000000 + gamepak_size, 1024, gamepak_rom);
    MAP_NULL(read, 0xC000000 + gamepak_size, 0xE000000);
  }
}

void init_memory(void)
{
  u32 map_offset = 0;

  // Fill memory map regions, areas marked as NULL must be checked directly
  MAP_REGION(read, 0x0000000, 0x1000000, 1, bios.rom);
  MAP_NULL(read, 0x1000000, 0x2000000);
  MAP_REGION(read, 0x2000000, 0x3000000, 8, ewram_data);
  MAP_REGION(read, 0x3000000, 0x4000000, 1, iwram_data);
  MAP_NULL(read, 0x4000000, 0x5000000);
  MAP_NULL(read, 0x5000000, 0x6000000);
  MAP_VRAM(read);
  MAP_NULL(read, 0x7000000, 0x8000000);
  init_memory_gamepak();
  MAP_NULL(read, 0xE000000, 0x10000000);

  // Fill memory map regions, areas marked as NULL must be checked directly
  MAP_NULL(write, 0x0000000, 0x2000000);
  MAP_REGION(write, 0x2000000, 0x3000000, 8, ewram_data);
  MAP_REGION(write, 0x3000000, 0x4000000, 1, iwram_data);
  MAP_NULL(write, 0x4000000, 0x5000000);
  MAP_NULL(write, 0x5000000, 0x6000000);
  MAP_VRAM(write);
  MAP_NULL(write, 0x7000000, 0x8000000);
  MAP_NULL(write, 0x8000000, 0xE000000);
  MAP_NULL(write, 0xE000000, 0x10000000);

  memset(iwram_data, 0, sizeof(iwram_data));
  memset(ewram_data, 0, sizeof(ewram_data));
  memset(vram, 0, sizeof(vram));

  memset(io_registers, 0, sizeof(io_registers));
  memset(oam_ram, 0, sizeof(oam_ram));
  memset(palette_ram, 0, sizeof(palette_ram));

  io_registers[REG_DISPCNT]   = 0x0080;
  io_registers[REG_DISPSTAT]  = 0x0000;
  io_registers[REG_VCOUNT]    = ResolveSetting(BootFromBIOS, PerGameBootFromBIOS)/*gpsp_persistent_config.BootFromBIOS*/ ? 0x0000 : 0x007e;
  io_registers[REG_P1]        = 0x03FF;
  io_registers[REG_BG2PA]     = 0x0100;
  io_registers[REG_BG2PD]     = 0x0100;
  io_registers[REG_BG3PA]     = 0x0100;
  io_registers[REG_BG3PD]     = 0x0100;
  io_registers[REG_SOUNDBIAS] = 0x0200;

  io_registers[REG_RCNT]  = 0x800F;
  sio_control(0x0004);

  waitstate_control(0x0000);

  oam_update = 1;

  affine_reference_x[0] = 0;
  affine_reference_x[1] = 0;
  affine_reference_y[0] = 0;
  affine_reference_y[1] = 0;

  flash_mode = FLASH_BASE_MODE;
  flash_bank_offset = 0;
  flash_command_position = 0;

  eeprom_mode = EEPROM_BASE_MODE;
  eeprom_address = 0;
  eeprom_counter = 0;

  rtc_state = RTC_DISABLED;
  rtc_status = 0x40;
  memset(rtc_registers, 0, sizeof(rtc_registers));

  bios_read_protect = 0xe129f000;

  cpu_dma_hack = 0;
  cpu_dma_last = 0;

  read_rom_region = 0xFFFFFFFF;
  read_ram_region = 0xFFFFFFFF;
}

static void load_backup_id(void)
{
  u32 addr;

  u8 *block = NULL;
  u32 *data = NULL;

  u32 region = 0xFFFFFFFF;
  u32 new_region = 0;

  init_memory_gamepak();

  u32 find_id = 0;
  BACKUP_TYPE_TYPE backup_type_id[2] = { BACKUP_NONE, };

  backup_type = BACKUP_NONE;

  sram_size   = SRAM_SIZE_32KB;
  flash_size  = FLASH_SIZE_64KB;
  eeprom_size = EEPROM_512_BYTE;

  backup_id[0] = 0;

  for (addr = 0x08000000; addr < 0x08000000 + gamepak_size; addr += 4)
  {
    new_region = addr >> 15;

    if (new_region != region)
    {
      region = new_region;
      block = memory_map_read[region];

      if (block == NULL)
        block = load_gamepak_page(region & 0x3FF);
    }

    data = (u32 *)(block + (addr & 0x7FFC));

    switch (data[0])
    {
      case ('E' | ('E' << 8) | ('P' << 16) | ('R' << 24)):
      {
        // EEPROM_Vxxx : EEPROM 512 bytes or 8 Kbytes (4Kbit or 64Kbit)
        if (memcmp(data, "EEPROM_V", 8) == 0)
        {
          backup_type_id[find_id] = BACKUP_EEPROM;
          find_id++;

          memcpy(backup_id, data, 11);
          backup_id[11] = 0;
        }
      }
      break;

      case ('S' | ('R' << 8) | ('A' << 16) | ('M' << 24)):
      {
        // SRAM_Vxxx : SRAM 32 Kbytes (256Kbit)
        if (memcmp(data, "SRAM_V", 6) == 0)
        {
          backup_type_id[find_id] = BACKUP_SRAM;
          find_id++;

          memcpy(backup_id, data, 9);
          backup_id[9] = 0;
        }

        // SRAM_F_Vxxx : FRAM 32 Kbytes (256Kbit)
        if (memcmp(data, "SRAM_F", 6) == 0)
        {
          backup_type_id[find_id] = BACKUP_SRAM;
          find_id++;

          memcpy(backup_id, data, 11);
          backup_id[11] = 0;
        }
      }
      break;

      case ('F' | ('L' << 8) | ('A' << 16) | ('S' << 24)):
      {
        // FLASH_Vxxx : FLASH 64 Kbytes (512Kbit) (ID used in older files)
        if (memcmp(data, "FLASH_V", 7) == 0)
        {
          backup_type_id[find_id] = BACKUP_FLASH;
          find_id++;

          flash_size  = FLASH_SIZE_64KB;
          flash_device_id = FLASH_DEVICE_PANASONIC_64KB;
          flash_manufacturer_id = FLASH_MANUFACTURER_PANASONIC;

          memcpy(backup_id, data, 10);
          backup_id[10] = 0;
        }

        // FLASH512_Vxxx : FLASH 64 Kbytes (512Kbit) (ID used in newer files)
        if (memcmp(data, "FLASH512", 8) == 0)
        {
          backup_type_id[find_id] = BACKUP_FLASH;
          find_id++;

          flash_size  = FLASH_SIZE_64KB;
          flash_device_id = FLASH_DEVICE_PANASONIC_64KB;
          flash_manufacturer_id = FLASH_MANUFACTURER_PANASONIC;

          memcpy(backup_id, data, 13);
          backup_id[13] = 0;
        }

        // FLASH1M_Vxxx : FLASH 128 Kbytes (1Mbit)
        if (memcmp(data, "FLASH1M_", 8) == 0)
        {
          backup_type_id[find_id] = BACKUP_FLASH;
          find_id++;

          flash_size  = FLASH_SIZE_128KB;
          flash_device_id = FLASH_DEVICE_SANYO_128KB;
          flash_manufacturer_id = FLASH_MANUFACTURER_SANYO;

          memcpy(backup_id, data, 12);
          backup_id[12] = 0;
        }
      }
      break;
    }

    if (find_id > 1) break;
  }

  if (find_id > 1)
  {
    // backup_id same
    if (backup_type_id[0] == backup_type_id[1])
    {
      backup_type = backup_type_id[0];
      return;
    }

    // backup_id different
    backup_type = BACKUP_NONE;

    flash_size  = FLASH_SIZE_64KB;
    flash_device_id = FLASH_DEVICE_PANASONIC_64KB;
    flash_manufacturer_id = FLASH_MANUFACTURER_PANASONIC;

    backup_id[0] = 0;
    return;
  }

  backup_type = backup_type_id[0];
}

u32 load_backup(void)
{
	char BackupFilename[MAX_PATH + 1];
	if (!ReGBA_GetBackupFilename(BackupFilename, CurrentGamePath))
	{
		ReGBA_Trace("W: Failed to get the name of the saved data file for '%s'", CurrentGamePath);
		return 0;
	}
	ReGBA_ProgressInitialise(FILE_ACTION_LOAD_BATTERY);

  FILE_TAG_TYPE backup_file;

  FILE_OPEN(backup_file, BackupFilename, READ);

  if(FILE_CHECK_VALID(backup_file))
  {
    u32 backup_size = FILE_LENGTH(backup_file);

    FILE_READ(backup_file, gamepak_backup, backup_size);
    FILE_CLOSE(backup_file);
	ReGBA_ProgressUpdate(1, 1);
	ReGBA_ProgressFinalise();

    if (backup_type != BACKUP_NONE)
    {
      switch (backup_size)
      {
        case 0x200:
          eeprom_size = EEPROM_512_BYTE;
          break;

        case 0x2000:
          eeprom_size = EEPROM_8_KBYTE;
          break;
      }
    }
    else
    {
      // The size might give away what kind of backup it is.
      switch(backup_size)
      {
        case 0x200:
          backup_type = BACKUP_EEPROM;
          eeprom_size = EEPROM_512_BYTE;
          break;

        case 0x2000:
          backup_type = BACKUP_EEPROM;
          eeprom_size = EEPROM_8_KBYTE;
          break;

        case 0x8000:
          backup_type = BACKUP_SRAM;
          sram_size = SRAM_SIZE_32KB;
          break;

        // Could be either flash or SRAM, go with flash
        case 0x10000:
          sram_size  = SRAM_SIZE_64KB;
          flash_size = FLASH_SIZE_64KB;
          flash_device_id = FLASH_DEVICE_PANASONIC_64KB;
          flash_manufacturer_id = FLASH_MANUFACTURER_PANASONIC;
          break;

        case 0x20000:
          backup_type = BACKUP_FLASH;
          flash_size  = FLASH_SIZE_128KB;
          flash_device_id = FLASH_DEVICE_SANYO_128KB;
          flash_manufacturer_id = FLASH_MANUFACTURER_SANYO;
          break;
      }
    }

    return 1;
  }
  else
  {
	ReGBA_ProgressFinalise();
    memset(gamepak_backup, 0xFF, 1024 * 128);
  }

  return 0;
}

static u32 save_backup(void)
{
	char BackupFilename[MAX_PATH + 1];
	if (!ReGBA_GetBackupFilename(BackupFilename, CurrentGamePath))
	{
		ReGBA_Trace("W: Failed to get the name of the saved data file for '%s'", CurrentGamePath);
		return 0;
	}

  FILE_TAG_TYPE backup_file;

  if (backup_type != BACKUP_NONE)
  {
    ReGBA_ProgressInitialise(FILE_ACTION_SAVE_BATTERY);
    FILE_OPEN(backup_file, BackupFilename, WRITE);

    if (FILE_CHECK_VALID(backup_file))
    {
      u32 backup_size;

      switch(backup_type)
      {
        case BACKUP_SRAM:
          backup_size = sram_size;
          break;

        case BACKUP_FLASH:
          backup_size = flash_size;
          break;

        case BACKUP_EEPROM:
          backup_size = eeprom_size;
          break;

        default:
        case BACKUP_NONE:
          backup_size = 0x8000;
          break;
      }

      FILE_WRITE(backup_file, gamepak_backup, backup_size);
      FILE_CLOSE(backup_file);
	  ReGBA_ProgressUpdate(1, 1);
	  ReGBA_ProgressFinalise();
      return 1;
    }
  }

  return 0;
}

void update_backup(void)
{
  if(backup_update != (WRITE_BACKUP_DELAY + 1))
    backup_update--;

  if(backup_update == 0)
  {
    save_backup();
    backup_update = WRITE_BACKUP_DELAY + 1;
  }
}

void update_backup_force(void)
{
  if (backup_update != (WRITE_BACKUP_DELAY + 1))
  {
    save_backup();
    backup_update = WRITE_BACKUP_DELAY + 1;
  }
}


static char *skip_spaces(char *line_ptr)
{
  while(*line_ptr == ' ')
    line_ptr++;

  return line_ptr;
}

static s32 parse_config_line(char *current_line, char *current_variable, char *current_value)
{
  char *line_ptr = current_line;
  char *line_ptr_new;

  if((current_line[0] == 0) || (current_line[0] == '#'))
    return -1;

  line_ptr_new = strchr(line_ptr, ' ');
  if(line_ptr_new == NULL)
    return -1;

  *line_ptr_new = 0;
  strcpy(current_variable, line_ptr);
  line_ptr_new = skip_spaces(line_ptr_new + 1);

  if(*line_ptr_new != '=')
    return -1;

  line_ptr_new = skip_spaces(line_ptr_new + 1);
  strcpy(current_value, line_ptr_new);
  line_ptr_new = current_value + strlen(current_value) - 1;
  if(*line_ptr_new == '\n')
  {
    line_ptr_new--;
    *line_ptr_new = 0;
  }

  if(*line_ptr_new == '\r')
    *line_ptr_new = 0;

  return 0;
}

static s32 load_game_config(char *gamepak_title, char *gamepak_code, char *gamepak_maker)
{
	char config_path[MAX_PATH];
	FILE_TAG_TYPE config_file;
	u32 i;

	idle_loop_targets = 0;
	for (i = 0; i < MAX_IDLE_LOOPS; i++)
		idle_loop_target_pc[i] = 0xFFFFFFFF;

	iwram_stack_optimize = 1;

	if (IsNintendoBIOS)
	{
		bios.rom[0x39] = 0x00; // Only Nintendo's BIOS requires this.
		bios.rom[0x2C] = 0x00; // Normmatt's open-source replacement doesn't.
	}

	ReGBA_ProgressInitialise(FILE_ACTION_APPLY_GAME_COMPATIBILITY);

	sprintf(config_path, "%s/%s", main_path, CONFIG_FILENAME);

	FILE_OPEN(config_file, config_path, READ);

	if(FILE_CHECK_VALID(config_file))
	{
		if (lookup_game_config(gamepak_title, gamepak_code, gamepak_maker, config_file))
		{
			ReGBA_ProgressUpdate(2, 2);
			ReGBA_ProgressFinalise();
			return 0;
		}
		else
			ReGBA_ProgressUpdate(1, 2);
	}

	if (ReGBA_GetBundledGameConfig(config_path))
	{
		FILE_OPEN(config_file, config_path, READ);

		if(FILE_CHECK_VALID(config_file))
		{
			if (lookup_game_config(gamepak_title, gamepak_code, gamepak_maker, config_file))
			{
				ReGBA_ProgressUpdate(2, 2);
				ReGBA_ProgressFinalise();
				return 0;
			}
		}
	}

	ReGBA_ProgressUpdate(2, 2);
	ReGBA_ProgressFinalise();

	return -1;
}

static bool lookup_game_config(char *gamepak_title, char *gamepak_code, char *gamepak_maker, FILE_TAG_TYPE config_file)
{
	char current_line[256];
	char current_variable[256];
	char current_value[256];

	while(FILE_GETS(current_line, 256, config_file))
	{
		if(parse_config_line(current_line, current_variable, current_value) != -1)
		{
			if(strcasecmp(current_variable, "game_name") != 0 || strcasecmp(current_value, gamepak_title) != 0)
				continue;

			if(!FILE_GETS(current_line, 256, config_file) || (parse_config_line(current_line, current_variable, current_value) == -1) ||
			   strcasecmp(current_variable, "game_code") != 0 || strcasecmp(current_value, gamepak_code) != 0)
				continue;

			if(!FILE_GETS(current_line, 256, config_file) || (parse_config_line(current_line, current_variable, current_value) == -1) ||
			   strcasecmp(current_variable, "vender_code") != 0 || strcasecmp(current_value, gamepak_maker) != 0)
				continue;

			while(FILE_GETS(current_line, 256, config_file))
			{
				if(parse_config_line(current_line, current_variable, current_value) != -1)
				{
					if(!strcasecmp(current_variable, "game_name"))
					{
						FILE_CLOSE(config_file);
						return 0;
					}

					if(!strcasecmp(current_variable, "idle_loop_eliminate_target"))
					{
						if(idle_loop_targets < MAX_IDLE_LOOPS)
						{
							idle_loop_target_pc[idle_loop_targets] =
							strtol(current_value, NULL, 16);
							idle_loop_targets++;
						}
					}

					if(!strcasecmp(current_variable, "iwram_stack_optimize") && !strcasecmp(current_value, "no"))
					{
						iwram_stack_optimize = 0;
					}

					if(!strcasecmp(current_variable, "bios_rom_hack_39") &&
					   !strcasecmp(current_value, "yes") &&
					   IsNintendoBIOS)
					{
						bios.rom[0x39] = 0xC0;
					}

					if(!strcasecmp(current_variable, "bios_rom_hack_2C") &&
					   !strcasecmp(current_value, "yes") &&
					   IsNintendoBIOS)
					{
						bios.rom[0x2C] = 0x02;
					}
			}
		}

		FILE_CLOSE(config_file);
		return true;
		}
	}

	FILE_CLOSE(config_file);
	return false;
}

#define LOAD_ON_MEMORY FILE_TAG_INVALID

static ssize_t load_gamepak_raw(char *name_path)
{
	FILE_TAG_TYPE gamepak_file;

	FILE_OPEN(gamepak_file, name_path, READ);

	if(FILE_CHECK_VALID(gamepak_file))
	{
		size_t gamepak_size = FILE_LENGTH(gamepak_file);
		uint8_t* EntireROM = ReGBA_MapEntireROM(gamepak_file, gamepak_size);
		if (EntireROM == NULL)
		{
			// Read in just enough for the header
			gamepak_file_large = gamepak_file;
			gamepak_ram_buffer_size = ReGBA_AllocateOnDemandBuffer((void**) &gamepak_rom);
			FILE_READ(gamepak_file, gamepak_rom, 0x100);
		}
		else
		{
			gamepak_ram_buffer_size = gamepak_size;
			gamepak_rom = EntireROM;
			// Do not close the file, because it is required by the mapping.
			// However, do not preserve it as gamepak_file_large either.
		}

		return gamepak_size;
	}

	return -1;
}

/*
 * Loads a GBA ROM from a file whose full path is in the first parameter.
 * Returns 0 on success and -1 on failure.
 */
size_t load_gamepak(char *file_path)
{
	errno = 0;
	u8 magicbit[4];
	FILE_TAG_TYPE fd;
	if (IsGameLoaded) {
		update_backup_force();
		if(FILE_CHECK_VALID(gamepak_file_large))
		{
			FILE_CLOSE(gamepak_file_large);
			gamepak_file_large = FILE_TAG_INVALID;
			ReGBA_DeallocateROM(gamepak_rom);
		}
		else if (IsZippedROM)
		{
			ReGBA_DeallocateROM(gamepak_rom);
		}
		else
		{
			ReGBA_UnmapEntireROM(gamepak_rom);
		}
		IsGameLoaded = false;
		IsZippedROM = false;
		gamepak_size = 0;
		gamepak_rom = NULL;
		gamepak_ram_buffer_size = gamepak_ram_pages = 0;
	}

	ssize_t file_size;

	FILE_OPEN(fd, file_path, READ);
	if (fd)
	{
		FILE_READ(fd, &magicbit, 4);
		FILE_CLOSE(fd);

		if ((magicbit[0] == 0x50) && (magicbit[1] == 0x4B) && (magicbit[2] == 0x03) && (magicbit[3] == 0x04))
		{
			uint8_t* ROMBuffer = NULL;
			file_size = load_file_zip(file_path, &ROMBuffer);
			if(file_size == -2)
			{
				char extracted_file[MAX_FILE];
				sprintf(extracted_file, "%s/%s", main_path, ZIP_TMP);
				file_size = load_gamepak_raw(extracted_file);
			}
			else
			{
				gamepak_rom = ROMBuffer;
				gamepak_ram_buffer_size = file_size;
				IsZippedROM = true;
			}
		}
		else if (magicbit[3] == 0xEA)
		{
			file_size = load_gamepak_raw(file_path);
		}
		else
		{
			ReGBA_Trace("E: Unsupported file type; first 4 bytes are <%02X %02X %02X %02X>\n", magicbit[0], magicbit[1], magicbit[2], magicbit[3]);
			return -1;
		}
	}
	else
	{
		ReGBA_Trace("E: Failed to open %s\n", file_path);
		return -1;
	}

	if(file_size != -1)
	{
		gamepak_ram_pages = gamepak_ram_buffer_size / (32 * 1024);
		strcpy(CurrentGamePath, file_path);
		IsGameLoaded = true;

		frame_ticks = 0;
		init_rewind(); // Initialise rewinds for this game

		gamepak_size = (file_size + 0x7FFF) & ~0x7FFF;

		memcpy(gamepak_title, gamepak_rom + 0xA0, 12);
		memcpy(gamepak_code, gamepak_rom + 0xAC, 4);
		memcpy(gamepak_maker, gamepak_rom + 0xB0, 2);
		gamepak_title[12] = '\0';
		gamepak_code[4] = '\0';
		gamepak_maker[2] = '\0';

		load_game_config(gamepak_title, gamepak_code, gamepak_maker);

		load_backup_id();
		load_backup();

		reset_gba();
		reg[CHANGED_PC_STATUS] = 1;

		ReGBA_OnGameLoaded(file_path);

		return 0;
	}

	return -1;
}

s32 load_bios(char *name)
{
	FILE_TAG_TYPE bios_file;
	FILE_OPEN(bios_file, name, READ);

	if (FILE_CHECK_VALID(bios_file))
	{
		FILE_READ(bios_file, bios.rom, 0x4000);
		FILE_CLOSE(bios_file);

		IsNintendoBIOS = false;
		u32 bios_crc = crc32(0, bios.rom, 0x4000);

		if (bios_crc == 0x81977335) // GBA
			IsNintendoBIOS = true;

		if (bios_crc == 0xa6473709) // NDS
			IsNintendoBIOS = true;

		return 0;
	}

	return -1;
}

// type = read_mem / write_mem
#define savestate_block(type)                                                 \
  cpu_##type##_savestate();                                                   \
  input_##type##_savestate();                                                 \
  main_##type##_savestate();                                                  \
  memory_##type##_savestate();                                                \
  sound_##type##_savestate();                                                 \
  video_##type##_savestate();                                                 \

static unsigned int rewind_queue_wr_len;
unsigned int rewind_queue_len;
void init_rewind(void)
{
	rewind_queue_wr_len = 0;
	rewind_queue_len = 0;
}

void savestate_rewind(void)
{
	g_state_buffer_ptr = SAVESTATE_REWIND_MEM + rewind_queue_wr_len * SAVESTATE_REWIND_LEN;
	savestate_block(write_mem);

	rewind_queue_wr_len += 1;
	if(rewind_queue_wr_len >= SAVESTATE_REWIND_NUM)
		rewind_queue_wr_len = 0;

	if(rewind_queue_len < SAVESTATE_REWIND_NUM)
		rewind_queue_len += 1;
}

void loadstate_rewind(void)
{
	int i;

	if(rewind_queue_len == 0)  // There's no rewind data
		return;

	//Load latest recently
	rewind_queue_len--;
	if(rewind_queue_wr_len == 0)
		rewind_queue_wr_len = SAVESTATE_REWIND_NUM - 1;
	else
		rewind_queue_wr_len--;

	g_state_buffer_ptr = SAVESTATE_REWIND_MEM + rewind_queue_wr_len * SAVESTATE_REWIND_LEN;
	savestate_block(read_mem);

	clear_metadata_area(METADATA_AREA_IWRAM, CLEAR_REASON_LOADING_STATE);
	clear_metadata_area(METADATA_AREA_EWRAM, CLEAR_REASON_LOADING_STATE);
	clear_metadata_area(METADATA_AREA_VRAM, CLEAR_REASON_LOADING_STATE);

	oam_update = 1;
	reg[CHANGED_PC_STATUS] = 1;
}

/*
 * Loads a saved state, given its slot number.
 * Returns 0 on success, non-zero on failure.
 */
u32 load_state(uint32_t SlotNumber)
{
	FILE_TAG_TYPE savestate_file;
	size_t i;

	char SavedStateFilename[MAX_PATH + 1];
	if (!ReGBA_GetSavedStateFilename(SavedStateFilename, CurrentGamePath, SlotNumber))
	{
		ReGBA_Trace("W: Failed to get the name of saved state #%d for '%s'", SlotNumber, CurrentGamePath);
		return 0;
	}

	ReGBA_ProgressInitialise(FILE_ACTION_LOAD_STATE);

	FILE_OPEN(savestate_file, SavedStateFilename, READ);
	if(FILE_CHECK_VALID(savestate_file))
    {
		u8 header[SVS_HEADER_SIZE];
		errno = 0;
		i = FILE_READ(savestate_file, header, SVS_HEADER_SIZE);
		ReGBA_ProgressUpdate(SVS_HEADER_SIZE, SAVESTATE_SIZE);
		if (i < SVS_HEADER_SIZE) {
			FILE_CLOSE(savestate_file);
			ReGBA_ProgressFinalise();
			return 1; // Failed to fully read the file
		}
		if (!(
			memcmp(header, SVS_HEADER_E, SVS_HEADER_SIZE) == 0
		||	memcmp(header, SVS_HEADER_F, SVS_HEADER_SIZE) == 0
		)) {
			FILE_CLOSE(savestate_file);
			ReGBA_ProgressFinalise();
			return 2; // Bad saved state format
		}

		i = FILE_READ(savestate_file, savestate_write_buffer, SAVESTATE_SIZE);
		ReGBA_ProgressUpdate(SAVESTATE_SIZE, SAVESTATE_SIZE);
		ReGBA_ProgressFinalise();
		FILE_CLOSE(savestate_file);
		if (i < SAVESTATE_SIZE)
			return 1; // Failed to fully read the file

		g_state_buffer_ptr = savestate_write_buffer + sizeof(struct ReGBA_RTC) + (240 * 160 * sizeof(u16)) + 2;

		savestate_block(read_mem);

		// Perform fixups by saved-state version.

		// 1.0e: Uses precalculated variables with SOUND_FREQUENCY equal to
		// 65536. Port these values forward to 1.0f where it's 88200.
		if (memcmp(header, SVS_HEADER_E, SVS_HEADER_SIZE) == 0)
		{
			unsigned int n;
			for (n = 0; n < 4; n++) {
				gbc_sound_channel[n].frequency_step = FLOAT_TO_FP08_24(FP08_24_TO_FLOAT(gbc_sound_channel[n].frequency_step) * 65536.0f / SOUND_FREQUENCY);
			}
			for (n = 0; n < 2; n++) {
				timer[n].frequency_step = FLOAT_TO_FP08_24(FP08_24_TO_FLOAT(timer[n].frequency_step) * 65536.0f / SOUND_FREQUENCY);
			}
		}

		// End fixups.

		clear_metadata_area(METADATA_AREA_IWRAM, CLEAR_REASON_LOADING_STATE);
		clear_metadata_area(METADATA_AREA_EWRAM, CLEAR_REASON_LOADING_STATE);
		clear_metadata_area(METADATA_AREA_VRAM, CLEAR_REASON_LOADING_STATE);

		oam_update = 1;
		reg[CHANGED_PC_STATUS] = 1;

		return 0;
	}
	else
	{
		ReGBA_ProgressFinalise();
		return 1;
	}
}


/*--------------------------------------------------------
  保存即时存档
  input
    u32 SlotNumber           存档槽
    u16 *screen_capture      存档索引画面
  return
    0 失败
    1 成功
--------------------------------------------------------*/
u32 save_state(uint32_t SlotNumber, u16 *screen_capture)
{
  char savestate_path[MAX_PATH];
  FILE_TAG_TYPE savestate_file;
//  char buf[256];
  struct ReGBA_RTC Time;
  u32 ret = 1;

	char SavedStateFilename[MAX_PATH + 1];
	if (!ReGBA_GetSavedStateFilename(SavedStateFilename, CurrentGamePath, SlotNumber))
	{
		ReGBA_Trace("W: Failed to get the name of saved state #%d for '%s'", SlotNumber, CurrentGamePath);
		return 0;
	}
	
	ReGBA_ProgressInitialise(FILE_ACTION_SAVE_STATE);

  g_state_buffer_ptr = savestate_write_buffer;

  ReGBA_LoadRTCTime(&Time);
  FILE_WRITE_MEM_VARIABLE(g_state_buffer_ptr, Time);

  FILE_WRITE_MEM(g_state_buffer_ptr, screen_capture, 240 * 160 * 2);
  //Identify ID
  *(g_state_buffer_ptr++)= 0x5A;
  *(g_state_buffer_ptr++)= 0x3C;

  savestate_block(write_mem);

  FILE_OPEN(savestate_file, SavedStateFilename, WRITE);
  if(FILE_CHECK_VALID(savestate_file))
  {
    if (FILE_WRITE(savestate_file, SVS_HEADER_F, SVS_HEADER_SIZE) < SVS_HEADER_SIZE)
	{
		ret = 0;
		goto fail;
	}
	ReGBA_ProgressUpdate(SVS_HEADER_SIZE, SAVESTATE_SIZE);
    if (FILE_WRITE(savestate_file, savestate_write_buffer, sizeof(savestate_write_buffer)) < sizeof(savestate_write_buffer))
	{
		ret = 0;
		goto fail;
	}
	ReGBA_ProgressUpdate(SAVESTATE_SIZE, SAVESTATE_SIZE);
fail:
    FILE_CLOSE(savestate_file);
  }
  else
  {
    ret = 0;
  }
  ReGBA_ProgressFinalise();

  return ret;
}

#define SAVESTATE_READ_MEM_FILENAME(name)                                     \
    FILE_READ_MEM_ARRAY(g_state_buffer_ptr, name);                            \

#define SAVESTATE_WRITE_MEM_FILENAME(name)                                    \
    FILE_WRITE_MEM_ARRAY(g_state_buffer_ptr, name);                           \

#define memory_savestate_body(type)                                           \
{                                                                             \
  char fullname[512];                                                         \
  memset(fullname, 0, sizeof(fullname));                                      \
                                                                              \
  SAVESTATE_##type##_FILENAME(fullname);                                      \
                                                                              \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, backup_type);                    \
                                                                              \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, sram_size);                      \
                                                                              \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, flash_size);                     \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, flash_mode);                     \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, flash_bank_offset);              \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, flash_device_id);                \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, flash_manufacturer_id);          \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, flash_command_position);         \
                                                                              \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, eeprom_size);                    \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, eeprom_mode);                    \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, eeprom_address_length);          \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, eeprom_address);                 \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, eeprom_counter);                 \
  FILE_##type##_ARRAY(g_state_buffer_ptr, eeprom_buffer);                     \
                                                                              \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, rtc_state);                      \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, rtc_write_mode);                 \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, rtc_command);                    \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, rtc_status);                     \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, rtc_data_bytes);                 \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, rtc_bit_count);                  \
  FILE_##type##_ARRAY(g_state_buffer_ptr, rtc_registers);                     \
  FILE_##type##_ARRAY(g_state_buffer_ptr, rtc_data);                          \
                                                                              \
  FILE_##type##_ARRAY(g_state_buffer_ptr, dma);                               \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, cpu_dma_hack);                   \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, cpu_dma_last);                   \
                                                                              \
  FILE_##type##_ARRAY(g_state_buffer_ptr, iwram_data);                        \
  FILE_##type##_ARRAY(g_state_buffer_ptr, ewram_data);                        \
  FILE_##type(g_state_buffer_ptr, vram, 0x18000);                             \
  FILE_##type(g_state_buffer_ptr, oam_ram, 0x400);                            \
  FILE_##type(g_state_buffer_ptr, palette_ram, 0x400);                        \
  FILE_##type(g_state_buffer_ptr, io_registers, 0x400);                       \
}                                                                             \

void memory_read_mem_savestate()
memory_savestate_body(READ_MEM);

void memory_write_mem_savestate()
memory_savestate_body(WRITE_MEM);

