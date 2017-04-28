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

#ifndef SOUND_H
#define SOUND_H


// This is the frequency of sound output by the GBA. It is stored in a
// ring buffer containing BUFFER_SIZE bytes.
// The value should technically be 32768, but at least the GBA Video ROMs and
// Golden Sun - The Lost Age require this to be 2 times that, and
// the Pokémon GBA games require it to be a multiple or divisor of 22050 Hz.
#define SOUND_FREQUENCY (88200.0f)


#define GBC_SOUND_TONE_CONTROL_LOW(channel, address)                          \
{                                                                             \
  GBCSoundStruct *gs = gbc_sound_channel + channel;                           \
                                                                              \
  u32 envelope_volume = (value >> 12) & 0x0F;                                 \
  u32 envelope_ticks = ((value >>  8) & 0x07) << 2;                           \
                                                                              \
  gs->length_ticks = 64 - (value & 0x3F);                                     \
  gs->sample_data = square_pattern_duty[(value >> 6) & 0x03];                 \
                                                                              \
  gs->envelope_status = (envelope_ticks != 0);                                \
  gs->envelope_direction = (value >> 11) & 0x01;                              \
                                                                              \
  gs->envelope_initial_volume = envelope_volume;                              \
  gs->envelope_initial_ticks = envelope_ticks;                                \
                                                                              \
  /* No Sound */                                                              \
  if (envelope_volume == 0)                                                   \
    gs->envelope_volume = 0;                                                  \
                                                                              \
  /* No Envelope */                                                           \
  if (envelope_ticks == 0)                                                    \
    gs->envelope_ticks = 0;                                                   \
                                                                              \
  ADDRESS16(io_registers, address) = value;                                   \
}                                                                             \

#define GBC_SOUND_TONE_CONTROL_HIGH(channel, address)                         \
{                                                                             \
  GBCSoundStruct *gs = gbc_sound_channel + channel;                           \
                                                                              \
  u32 rate = value & 0x7FF;                                                   \
                                                                              \
  gs->rate = rate;                                                            \
  gs->frequency_step = FLOAT_TO_FP08_24((1048576.0f / SOUND_FREQUENCY) / (2048 - rate)); \
  gs->length_status = (value >> 14) & 0x01;                                   \
                                                                              \
  if ((value & 0x8000) != 0)                                                  \
  {                                                                           \
    gs->active_flag = 1;                                                      \
    gs->sample_index -= FLOAT_TO_FP08_24(1.0f / 12.0f);                       \
    gs->envelope_ticks = gs->envelope_initial_ticks;                          \
    gs->envelope_volume = gs->envelope_initial_volume;                        \
    gs->sweep_ticks = gs->sweep_initial_ticks;                                \
  }                                                                           \
                                                                              \
  ADDRESS16(io_registers, address) = value;                                   \
}                                                                             \

#define GBC_SOUND_TONE_CONTROL_SWEEP()                                        \
{                                                                             \
  GBCSoundStruct *gs = gbc_sound_channel + 0;                                 \
                                                                              \
  u32 sweep_shift = value & 0x07;                                             \
  u32 sweep_ticks = ((value >> 4) & 0x07) << 1;                               \
                                                                              \
  gs->sweep_status = (sweep_shift != 0) && (sweep_ticks != 0);                \
                                                                              \
  gs->sweep_shift = sweep_shift;                                              \
  gs->sweep_direction = (value >> 3) & 0x01;                                  \
                                                                              \
  gs->sweep_initial_ticks = sweep_ticks;                                      \
                                                                              \
  ADDRESS16(io_registers, 0x60) = value;                                      \
}                                                                             \

#define GBC_SOUND_WAVE_CONTROL()                                              \
{                                                                             \
  gbc_sound_wave_bank = (value >> 6) & 0x01;                                  \
  gbc_sound_wave_bank_user = (gbc_sound_wave_bank ^ 0x01) << 4;               \
                                                                              \
  gbc_sound_wave_type = (value >> 5) & 0x01;                                  \
                                                                              \
  gbc_sound_wave_enable = (value >> 7) & 0x01;                                \
                                                                              \
  ADDRESS16(io_registers, 0x70) = value;                                      \
}                                                                             \

#define GBC_SOUND_TONE_CONTROL_LOW_WAVE()                                     \
{                                                                             \
  GBCSoundStruct *gs = gbc_sound_channel + 2;                                 \
                                                                              \
  gs->length_ticks = 256 - (value & 0xFF);                                    \
                                                                              \
  if ((value & 0x8000) != 0)                                                  \
    gbc_sound_wave_volume = 12288;                                            \
  else                                                                        \
    gbc_sound_wave_volume = gbc_sound_wave_volume_table[(value >> 13) & 0x03];\
                                                                              \
  ADDRESS16(io_registers, 0x72) = value;                                      \
}                                                                             \

#define GBC_SOUND_TONE_CONTROL_HIGH_WAVE()                                    \
{                                                                             \
  GBCSoundStruct *gs = gbc_sound_channel + 2;                                 \
                                                                              \
  u32 rate = value & 0x7FF;                                                   \
                                                                              \
  gs->rate = rate;                                                            \
  gs->frequency_step = FLOAT_TO_FP08_24((2097152.0f / SOUND_FREQUENCY) / (2048 - rate)); \
  gs->length_status = (value >> 14) & 0x01;                                   \
                                                                              \
  if ((value & 0x8000) != 0)                                                  \
  {                                                                           \
    gs->sample_index = 0;                                                     \
    gs->active_flag = 1;                                                      \
  }                                                                           \
                                                                              \
  ADDRESS16(io_registers, 0x74) = value;                                      \
}                                                                             \

#define GBA_SOUND_WAVE_PATTERN_RAM16()                                        \
{                                                                             \
  if (gbc_sound_wave_bank_user != 0)                                          \
    gbc_sound_wave_update |= 2;                                               \
  else                                                                        \
    gbc_sound_wave_update |= 1;                                               \
                                                                              \
  ADDRESS16(gbc_sound_wave_ram_data, (address & 0x0e) | gbc_sound_wave_bank_user) = value; \
}                                                                             \

#define GBC_SOUND_NOISE_CONTROL()                                             \
{                                                                             \
  GBCSoundStruct *gs = gbc_sound_channel + 3;                                 \
                                                                              \
  u32 dividing_ratio = value & 0x07;                                          \
  u32 frequency_shift = (value >> 4) & 0x0F;                                  \
                                                                              \
  if (dividing_ratio == 0)                                                    \
    gs->frequency_step = FLOAT_TO_FP08_24((1048576.0f / SOUND_FREQUENCY) / (2 << frequency_shift)); \
  else                                                                        \
    gs->frequency_step = FLOAT_TO_FP08_24(( 524288.0f / SOUND_FREQUENCY) / (dividing_ratio << (1 + frequency_shift))); \
                                                                              \
  gbc_sound_noise_type = (value >> 3) & 0x01;                                 \
  gs->length_status = (value >> 14) & 0x01;                                   \
                                                                              \
  if ((value & 0x8000) != 0)                                                  \
  {                                                                           \
    gbc_sound_noise_index = 0;                                                \
                                                                              \
    gs->sample_index = 0;                                                     \
    gs->active_flag = 1;                                                      \
    gs->envelope_ticks = gs->envelope_initial_ticks;                          \
    gs->envelope_volume = gs->envelope_initial_volume;                        \
  }                                                                           \
                                                                              \
  ADDRESS16(io_registers, 0x7C) = value;                                      \
}                                                                             \

#define GBC_SOUND_CHANNEL_STATUS(channel)                                     \
  gbc_sound_channel[channel].status = ((value >> (channel + 11)) & 0x02)      \
                                    | ((value >> (channel +  8)) & 0x01)      \

#define GBC_TRIGGER_SOUND()                                                   \
{                                                                             \
  gbc_sound_master_volume_right = value & 0x07;                               \
  gbc_sound_master_volume_left = (value >> 4) & 0x07;                         \
                                                                              \
  GBC_SOUND_CHANNEL_STATUS(0);                                                \
  GBC_SOUND_CHANNEL_STATUS(1);                                                \
  GBC_SOUND_CHANNEL_STATUS(2);                                                \
  GBC_SOUND_CHANNEL_STATUS(3);                                                \
                                                                              \
  ADDRESS16(io_registers, 0x80) = value;                                      \
}                                                                             \

#define TRIGGER_SOUND()                                                       \
{                                                                             \
  gbc_sound_master_volume = value & 0x03;                                     \
                                                                              \
  timer[0].direct_sound_channels = ((~value >> 13) & 0x02) | ((~value >> 10) & 0x01); \
  timer[1].direct_sound_channels = (( value >> 13) & 0x02) | (( value >> 10) & 0x01); \
                                                                              \
  direct_sound_channel[0].volume = (value >>  2) & 0x01;                      \
  direct_sound_channel[0].status = (value >>  8) & 0x03;                      \
  direct_sound_channel[1].volume = (value >>  3) & 0x01;                      \
  direct_sound_channel[1].status = (value >> 12) & 0x03;                      \
                                                                              \
  if ((value & 0x0800) != 0)                                                  \
    sound_reset_fifo(0);                                                      \
                                                                              \
  if ((value & 0x8000) != 0)                                                  \
    sound_reset_fifo(1);                                                      \
                                                                              \
  ADDRESS16(io_registers, 0x82) = value;                                      \
}                                                                             \

#define SOUND_ON()                                                            \
  if ((value & 0x80) != 0)                                                    \
  {                                                                           \
    sound_on = 1;                                                             \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    gbc_sound_channel[0].active_flag = 0;                                     \
    gbc_sound_channel[1].active_flag = 0;                                     \
    gbc_sound_channel[2].active_flag = 0;                                     \
    gbc_sound_channel[3].active_flag = 0;                                     \
    sound_on = 0;                                                             \
  }                                                                           \
                                                                              \
  ADDRESS16(io_registers, 0x84) = (ADDRESS16(io_registers, 0x84) & 0x000F) | (value & 0xFFF0); \

#define SOUND_UPDATE_FREQUENCY_STEP(timer_number)                             \
  timer[timer_number].frequency_step =                                        \
   FLOAT_TO_FP08_24((SYS_CLOCK / SOUND_FREQUENCY) / timer_reload)             \


typedef enum
{
  DIRECT_SOUND_INACTIVE,
  DIRECT_SOUND_RIGHT,
  DIRECT_SOUND_LEFT,
  DIRECT_SOUND_LEFTRIGHT
} DIRECT_SOUND_STATUS_TYPE;

typedef enum
{
  DIRECT_SOUND_VOLUME_50,
  DIRECT_SOUND_VOLUME_100
} DIRECT_SOUND_VOLUME_TYPE;

typedef enum
{
  GBC_SOUND_INACTIVE,
  GBC_SOUND_RIGHT,
  GBC_SOUND_LEFT,
  GBC_SOUND_LEFTRIGHT
} GBC_SOUND_STATUS_TYPE;

typedef struct
{
  DIRECT_SOUND_STATUS_TYPE status;
  DIRECT_SOUND_VOLUME_TYPE volume;

  FIXED08_24 fifo_fractional;
  // The + 1 is to give some extra room for linear interpolation
  // when wrapping around.

  u32 buffer_index;

  s8 fifo[32];
  u32 fifo_base;
  u32 fifo_top;
} DirectSoundStruct;

typedef struct
{
  GBC_SOUND_STATUS_TYPE status;

  FIXED08_24 frequency_step;
  FIXED08_24 tick_counter;

  FIXED08_24 sample_index;
  s8 *sample_data;

  u32 active_flag;

  u32 envelope_status;
  u32 envelope_direction;
  u32 envelope_initial_volume;
  u32 envelope_volume;
  u32 envelope_initial_ticks;
  u32 envelope_ticks;

  u32 sweep_status;
  u32 sweep_direction;
  u32 sweep_shift;
  u32 sweep_initial_ticks;
  u32 sweep_ticks;

  u32 length_status;
  u32 length_ticks;

  u32 rate;
} GBCSoundStruct;

extern DirectSoundStruct direct_sound_channel[2];
extern GBCSoundStruct gbc_sound_channel[4];
extern s8 square_pattern_duty[4][8];
extern u32 gbc_sound_master_volume_left;
extern u32 gbc_sound_master_volume_right;
extern u32 gbc_sound_master_volume;

extern u32 sound_on;

extern u32 gbc_sound_wave_enable;
extern u32 gbc_sound_wave_update;
extern u32 gbc_sound_wave_type;
extern u32 gbc_sound_wave_bank;
extern u32 gbc_sound_wave_bank_user;
extern u32 gbc_sound_wave_volume;
extern u8 gbc_sound_wave_ram_data[32];
extern const u32 gbc_sound_wave_volume_table[4];

extern u32 gbc_sound_noise_type;
extern u32 gbc_sound_noise_index;

extern u32 gbc_sound_buffer_index;
extern u32 gbc_sound_partial_ticks;


void update_gbc_sound(u32 cpu_ticks);

void sound_timer(FIXED08_24 frequency_step, u32 channel);
void adjust_direct_sound_buffer(u8 channel, u32 cpu_ticks);

void sound_timer_queue(u8 channel);
void sound_reset_fifo(u32 channel);


void init_sound(void);
void reset_sound(void);

void sound_write_mem_savestate(void);
void sound_read_mem_savestate(void);


// Services provided to ports

/*
 * Returns the number of samples available in the core's audio buffer.
 * This number of samples is always interpreted with a sample rate of
 * SOUND_FREQUENCY Hz, which is used by the core to render to its buffer.
 * 
 * Returns:
 *   The number of samples available for reading by ReGBA_LoadNextAudioSample
 *   at the time of the function call, counting 1 for each two stereo samples
 *   available; that is, the number of times ReGBA_LoadNextAudioSample can be
 *   called with Left and Right without it returning zero.
 */
u32 ReGBA_GetAudioSamplesAvailable(void);

/*
 * Loads and consumes the next audio sample from the core's audio buffer.
 * The sample rate is SOUND_FREQUENCY Hz.
 * Output:
 *   Left: A pointer to a signed 16-bit variable updated with the value for
 *   the next sample's left stereo channel.
 *   Right: A pointer to a signed 16-bit variable updated with the value for
 *   the next sample's right stereo channel.
 * Returns:
 *   Non-zero if a sample was available; zero if no samples were available.
 * Output assertions:
 *   If non-zero is returned, the loaded sample is consumed in the core's
 *   audio buffer.
 *   If zero is returned, neither variable is written to.
 */
u32 ReGBA_LoadNextAudioSample(s16* Left, s16* Right);

/*
 * Discards the requested number of samples from the core's audio buffer.
 * The sample rate is SOUND_FREQUENCY Hz.
 * Input:
 *   Count: The number of samples to be discarded by the core.
 * Returns:
 *   The number of samples that were actually discarded, which may be Count
 *   or fewer.
 */
u32 ReGBA_DiscardAudioSamples(u32 Count);

#endif /* SOUND_H */
