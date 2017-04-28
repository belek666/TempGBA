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


#define RING_BUFFER_SIZE  (0x10000)
#define RING_BUFFER_MASK  (0x0FFFF)

DirectSoundStruct ALIGN_DATA direct_sound_channel[2];
GBCSoundStruct    ALIGN_DATA gbc_sound_channel[4];

// Initial pattern data = 4bits (signed)
// Channel volume = 12bits
// Envelope volume = 14bits
// Master volume = 2bits

// Recalculate left and right volume as volume changes.
// To calculate the current sample, use (sample * volume) >> 16

// Square waves range from -8 (low) to 7 (high)

s8 ALIGN_DATA square_pattern_duty[4][8] =
{
  { 0x07, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8 },
  { 0x07, 0x07, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8 },
  { 0x07, 0x07, 0x07, 0x07, 0xF8, 0xF8, 0xF8, 0xF8 },
  { 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0xF8, 0xF8 }
};

u32 gbc_sound_master_volume_left;
u32 gbc_sound_master_volume_right;
u32 gbc_sound_master_volume;

u32 sound_on = 0;

u32 gbc_sound_wave_enable = 0;
u32 gbc_sound_wave_update = 0;
u32 gbc_sound_wave_type = 0;
u32 gbc_sound_wave_bank = 0;
u32 gbc_sound_wave_bank_user = 0x00;
u32 gbc_sound_wave_volume = 0;
u8 ALIGN_DATA gbc_sound_wave_ram_data[32];
const u32 ALIGN_DATA gbc_sound_wave_volume_table[4] = { 0, 16384, 8192, 4096 };

u32 gbc_sound_noise_type = 0;
u32 gbc_sound_noise_index = 0;

u32 gbc_sound_buffer_index = 0;
u32 gbc_sound_partial_ticks = 0;


static s16 ALIGN_DATA sound_buffer[RING_BUFFER_SIZE];
static u32 sound_buffer_base = 0;
static FIXED08_24 gbc_sound_tick_step;

const u32 ALIGN_DATA gbc_sound_master_volume_table[4] = { 1, 2, 4, 0 };

const u32 ALIGN_DATA gbc_sound_channel_volume_table[8] =
{
  FIXED_DIV(0, 7, 12),
  FIXED_DIV(1, 7, 12),
  FIXED_DIV(2, 7, 12),
  FIXED_DIV(3, 7, 12),
  FIXED_DIV(4, 7, 12),
  FIXED_DIV(5, 7, 12),
  FIXED_DIV(6, 7, 12),
  FIXED_DIV(7, 7, 12)
};

const u32 ALIGN_DATA gbc_sound_envelope_volume_table[16] =
{
  FIXED_DIV(0, 15, 14),
  FIXED_DIV(1, 15, 14),
  FIXED_DIV(2, 15, 14),
  FIXED_DIV(3, 15, 14),
  FIXED_DIV(4, 15, 14),
  FIXED_DIV(5, 15, 14),
  FIXED_DIV(6, 15, 14),
  FIXED_DIV(7, 15, 14),
  FIXED_DIV(8, 15, 14),
  FIXED_DIV(9, 15, 14),
  FIXED_DIV(10, 15, 14),
  FIXED_DIV(11, 15, 14),
  FIXED_DIV(12, 15, 14),
  FIXED_DIV(13, 15, 14),
  FIXED_DIV(14, 15, 14),
  FIXED_DIV(15, 15, 14)
};

s8 ALIGN_DATA gbc_sound_wave_samples[64];

s32 ALIGN_DATA gbc_sound_noise_table15[1024];
s32 ALIGN_DATA gbc_sound_noise_table7[4];

u32 gbc_sound_last_cpu_ticks = 0;


static void init_noise_table(s32 *table, u32 period, u32 bit_length);
static u32 buffer_length(u32 top, u32 base, u32 length);
static u64 delta_ticks(u32 now_ticks, u32 last_ticks);


void adjust_direct_sound_buffer(u8 channel, u32 cpu_ticks)
{
  u64 count_ticks;
  u32 buffer_ticks, partial_ticks;

  // count_ticks = (ticks * 16777216.0) * (SOUND_FREQUENCY / 16777216.0)
  count_ticks = delta_ticks(cpu_ticks, gbc_sound_last_cpu_ticks) * SOUND_FREQUENCY;

  buffer_ticks = FP08_24_TO_U32(count_ticks);
  partial_ticks = gbc_sound_partial_ticks + FP08_24_FRACTIONAL_PART(count_ticks);

  buffer_ticks += FP08_24_TO_U32(partial_ticks);

  direct_sound_channel[channel].buffer_index = (gbc_sound_buffer_index + (buffer_ticks << 1)) & RING_BUFFER_MASK;
}

void sound_timer_queue(u8 channel)
{
  DirectSoundStruct *ds = direct_sound_channel + channel;

  u32 i;
  u32 fifo_top = ds->fifo_top;
  s8 *fifo_data = (s8 *)io_registers + (0xA0 + (channel << 2));

  for (i = 0; i < 4; i++)
  {
    ds->fifo[fifo_top] = fifo_data[i];
    fifo_top = (fifo_top + 1) & 0x1F;
  }

  ds->fifo_top = fifo_top;
}

void sound_reset_fifo(u32 channel)
{
  DirectSoundStruct *ds = direct_sound_channel + channel;

  ds->fifo_top  = 0;
  ds->fifo_base = 0;

  memset(ds->fifo, 0, 32);
}


#define RENDER_SAMPLE_NULL()                                                  \
  fifo_fractional += frequency_step;                                          \

#define RENDER_SAMPLE_RIGHT()                                                 \
  fractional_sample = (s64)diff_sample * fifo_fractional;                     \
  fifo_fractional += frequency_step;                                          \
  sound_buffer[buffer_index + 1] += current_sample + FP08_24_TO_U32(fractional_sample); \

#define RENDER_SAMPLE_LEFT()                                                  \
  fractional_sample = (s64)diff_sample * fifo_fractional;                     \
  fifo_fractional += frequency_step;                                          \
  sound_buffer[buffer_index + 0] += current_sample + FP08_24_TO_U32(fractional_sample); \

#define RENDER_SAMPLE_BOTH()                                                  \
  fractional_sample = (s64)diff_sample * fifo_fractional;                     \
  fifo_fractional += frequency_step;                                          \
  dest_sample = current_sample + FP08_24_TO_U32(fractional_sample);           \
  sound_buffer[buffer_index + 0] += dest_sample;                              \
  sound_buffer[buffer_index + 1] += dest_sample;                              \

#define RENDER_SAMPLES(type)                                                  \
  while (FP08_24_TO_U32(fifo_fractional) == 0)                                \
  {                                                                           \
    RENDER_SAMPLE_##type();                                                   \
    buffer_index = (buffer_index + 2) & RING_BUFFER_MASK;                     \
  }                                                                           \

void sound_timer(FIXED08_24 frequency_step, u32 channel)
{
  DirectSoundStruct *ds = direct_sound_channel + channel;

  FIXED08_24 fifo_fractional = ds->fifo_fractional;
  u32 fifo_base = ds->fifo_base;
  u32 buffer_index = ds->buffer_index;

  s16 current_sample, diff_sample, dest_sample;
  s64 fractional_sample;

  current_sample = ds->fifo[fifo_base] << 4;

  ds->fifo[fifo_base] = 0;
  fifo_base = (fifo_base + 1) & 0x1F;

  diff_sample = (ds->fifo[fifo_base] << 4) - current_sample;

  if (sound_on != 0)
  {
    if (ds->volume == DIRECT_SOUND_VOLUME_50)
    {
      current_sample >>= 1;
      diff_sample >>= 1;
    }

    switch (ds->status)
    {
      case DIRECT_SOUND_INACTIVE:
        RENDER_SAMPLES(NULL);
        break;

      case DIRECT_SOUND_RIGHT:
        RENDER_SAMPLES(RIGHT);
        break;

      case DIRECT_SOUND_LEFT:
        RENDER_SAMPLES(LEFT);
        break;

      case DIRECT_SOUND_LEFTRIGHT:
        RENDER_SAMPLES(BOTH);
        break;
    }
  }
  else
  {
    RENDER_SAMPLES(NULL);
  }

  ds->buffer_index = buffer_index;
  ds->fifo_base = fifo_base;
  ds->fifo_fractional = FP08_24_FRACTIONAL_PART(fifo_fractional);

  if (buffer_length(ds->fifo_top, fifo_base, 32) <= 16)
  {
    if (dma[1].direct_sound_channel == channel)
      dma_transfer(dma + 1);

    if (dma[2].direct_sound_channel == channel)
      dma_transfer(dma + 2);
  }
}


static u64 delta_ticks(u32 now_ticks, u32 last_ticks)
{
  if (now_ticks == last_ticks)
    return 0ULL;

  if (now_ticks > last_ticks)
    return (u64)now_ticks - last_ticks;

  return 4294967296ULL - last_ticks + now_ticks;
}

static u32 buffer_length(u32 top, u32 base, u32 length)
{
  if (top == base)
    return 0;

  if (top > base)
    return top - base;

  return length - base + top;
}


#define UPDATE_VOLUME_CHANNEL_ENVELOPE(channel)                               \
  volume_##channel = gbc_sound_envelope_volume_table[envelope_volume]         \
                   * gbc_sound_channel_volume_table[gbc_sound_master_volume_##channel] \
                   * gbc_sound_master_volume_table[gbc_sound_master_volume]   \

#define UPDATE_VOLUME_CHANNEL_NOENVELOPE(channel)                             \
  volume_##channel = gbc_sound_wave_volume                                    \
                   * gbc_sound_channel_volume_table[gbc_sound_master_volume_##channel] \
                   * gbc_sound_master_volume_table[gbc_sound_master_volume]   \

#define UPDATE_VOLUME(type)                                                   \
  UPDATE_VOLUME_CHANNEL_##type(left);                                         \
  UPDATE_VOLUME_CHANNEL_##type(right);                                        \

#define UPDATE_TONE_SWEEP()                                                   \
  if (gs->sweep_status != 0)                                                  \
  {                                                                           \
    u32 sweep_ticks = gs->sweep_ticks - 1;                                    \
                                                                              \
    if (sweep_ticks == 0)                                                     \
    {                                                                         \
      u32 rate = gs->rate;                                                    \
                                                                              \
      if (gs->sweep_direction != 0)                                           \
        rate = rate - (rate >> gs->sweep_shift);                              \
      else                                                                    \
        rate = rate + (rate >> gs->sweep_shift);                              \
                                                                              \
      if (rate > 2047)                                                        \
      {                                                                       \
        gs->active_flag = 0;                                                  \
        break;                                                                \
      }                                                                       \
                                                                              \
      frequency_step = FLOAT_TO_FP08_24((1048576.0f / SOUND_FREQUENCY) / (2048 - rate)); \
                                                                              \
      gs->frequency_step = frequency_step;                                    \
      gs->rate = rate;                                                        \
                                                                              \
      sweep_ticks = gs->sweep_initial_ticks;                                  \
    }                                                                         \
                                                                              \
    gs->sweep_ticks = sweep_ticks;                                            \
  }                                                                           \

#define UPDATE_TONE_NOSWEEP()                                                 \

#define UPDATE_TONE_ENVELOPE()                                                \
  if (gs->envelope_status != 0)                                               \
  {                                                                           \
    u32 envelope_ticks = gs->envelope_ticks - 1;                              \
    envelope_volume = gs->envelope_volume;                                    \
                                                                              \
    if (envelope_ticks == 0)                                                  \
    {                                                                         \
      if (gs->envelope_direction != 0)                                        \
      {                                                                       \
        if (envelope_volume != 15)                                            \
          envelope_volume++;                                                  \
      }                                                                       \
      else                                                                    \
      {                                                                       \
        if (envelope_volume != 0)                                             \
          envelope_volume--;                                                  \
      }                                                                       \
                                                                              \
      UPDATE_VOLUME(ENVELOPE);                                                \
                                                                              \
      gs->envelope_volume = envelope_volume;                                  \
      envelope_ticks = gs->envelope_initial_ticks;                            \
    }                                                                         \
                                                                              \
    gs->envelope_ticks = envelope_ticks;                                      \
  }                                                                           \

#define UPDATE_TONE_NOENVELOPE()                                              \

#define UPDATE_TONE_COUNTERS(envelope_op, sweep_op)                           \
  tick_counter += gbc_sound_tick_step;                                        \
  if (FP08_24_TO_U32(tick_counter) != 0)                                      \
  {                                                                           \
    tick_counter &= 0x00FFFFFF;                                               \
                                                                              \
    if (gs->length_status != 0)                                               \
    {                                                                         \
      u32 length_ticks = gs->length_ticks - 1;                                \
      gs->length_ticks = length_ticks;                                        \
                                                                              \
      if (length_ticks == 0)                                                  \
      {                                                                       \
        gs->active_flag = 0;                                                  \
        break;                                                                \
      }                                                                       \
    }                                                                         \
                                                                              \
    UPDATE_TONE_##envelope_op();                                              \
    UPDATE_TONE_##sweep_op();                                                 \
  }                                                                           \

#define GBC_SOUND_RENDER_SAMPLE_RIGHT()                                       \
  sound_buffer[buffer_index + 1] += (current_sample * volume_right) >> 22;    \

#define GBC_SOUND_RENDER_SAMPLE_LEFT()                                        \
  sound_buffer[buffer_index + 0] += (current_sample * volume_left ) >> 22;    \

#define GBC_SOUND_RENDER_SAMPLE_BOTH()                                        \
  GBC_SOUND_RENDER_SAMPLE_RIGHT();                                            \
  GBC_SOUND_RENDER_SAMPLE_LEFT();                                             \

#define GBC_SOUND_RENDER_SAMPLES(type, sample_length, envelope_op, sweep_op)  \
  for(i = 0; i < buffer_ticks; i++)                                           \
  {                                                                           \
    current_sample = sample_data[FP08_24_TO_U32(sample_index) % sample_length]; \
                                                                              \
    GBC_SOUND_RENDER_SAMPLE_##type();                                         \
                                                                              \
    sample_index += frequency_step;                                           \
    buffer_index = (buffer_index + 2) & RING_BUFFER_MASK;                     \
                                                                              \
    UPDATE_TONE_COUNTERS(envelope_op, sweep_op);                              \
  }                                                                           \

#define GBC_NOISE_WRAP_FULL 32767

#define GBC_NOISE_WRAP_HALF 127

#define GET_NOISE_SAMPLE_FULL()                                               \
  current_sample = ((gbc_sound_noise_table15[gbc_sound_noise_index >> 5] << (gbc_sound_noise_index & 0x1F)) >> 31) ^ 0x07; \

#define GET_NOISE_SAMPLE_HALF()                                               \
  current_sample = ((gbc_sound_noise_table7[gbc_sound_noise_index >> 5]  << (gbc_sound_noise_index & 0x1F)) >> 31) ^ 0x07; \

#define GBC_SOUND_RENDER_NOISE(type, noise_type, envelope_op, sweep_op)       \
  for(i = 0; i < buffer_ticks; i++)                                           \
  {                                                                           \
    GET_NOISE_SAMPLE_##noise_type();                                          \
    GBC_SOUND_RENDER_SAMPLE_##type();                                         \
                                                                              \
    sample_index += frequency_step;                                           \
                                                                              \
    if (FP08_24_TO_U32(sample_index) != 0)                                    \
    {                                                                         \
      gbc_sound_noise_index = (gbc_sound_noise_index + 1) % GBC_NOISE_WRAP_##noise_type; \
      sample_index = FP08_24_FRACTIONAL_PART(sample_index);                   \
    }                                                                         \
                                                                              \
    buffer_index = (buffer_index + 2) & RING_BUFFER_MASK;                     \
                                                                              \
    UPDATE_TONE_COUNTERS(envelope_op, sweep_op);                              \
  }                                                                           \

#define GBC_SOUND_RENDER_CHANNEL(type, sample_length, envelope_op, sweep_op)  \
  buffer_index = gbc_sound_buffer_index;                                      \
  sample_index = gs->sample_index;                                            \
  frequency_step = gs->frequency_step;                                        \
  tick_counter = gs->tick_counter;                                            \
                                                                              \
  UPDATE_VOLUME(envelope_op);                                                 \
                                                                              \
  switch(gs->status)                                                          \
  {                                                                           \
    case GBC_SOUND_INACTIVE:                                                  \
      break;                                                                  \
                                                                              \
    case GBC_SOUND_RIGHT:                                                     \
      GBC_SOUND_RENDER_##type(RIGHT, sample_length, envelope_op, sweep_op);   \
      break;                                                                  \
                                                                              \
    case GBC_SOUND_LEFT:                                                      \
      GBC_SOUND_RENDER_##type(LEFT, sample_length, envelope_op, sweep_op);    \
      break;                                                                  \
                                                                              \
    case GBC_SOUND_LEFTRIGHT:                                                 \
      GBC_SOUND_RENDER_##type(BOTH, sample_length, envelope_op, sweep_op);    \
      break;                                                                  \
  }                                                                           \
                                                                              \
  gs->sample_index = sample_index;                                            \
  gs->tick_counter = tick_counter;                                            \


#define GBC_SOUND_LOAD_WAVE_RAM()                                             \
  for (i = 0, i2 = 0; i < 16; i++, i2 += 2)                                   \
  {                                                                           \
    current_sample = wave_ram[i];                                             \
    wave_ram_bank[i2 + 0] = ((current_sample >> 4) & 0x0F) - 8;               \
    wave_ram_bank[i2 + 1] = ((current_sample >> 0) & 0x0F) - 8;               \
  }                                                                           \

#define GBC_SOUND_UPDATE_WAVE_RAM()                                           \
{                                                                             \
  u8 *wave_ram = gbc_sound_wave_ram_data;                                     \
  s8 *wave_ram_bank = gbc_sound_wave_samples;                                 \
                                                                              \
  /* Wave RAM Bank 0 */                                                       \
  if ((gbc_sound_wave_update & 1) != 0)                                       \
  {                                                                           \
    GBC_SOUND_LOAD_WAVE_RAM();                                                \
  }                                                                           \
                                                                              \
  /* Wave RAM Bank 1 */                                                       \
  if ((gbc_sound_wave_update & 2) != 0)                                       \
  {                                                                           \
    wave_ram += 16;                                                           \
    wave_ram_bank += 32;                                                      \
    GBC_SOUND_LOAD_WAVE_RAM();                                                \
  }                                                                           \
                                                                              \
  gbc_sound_wave_update = 0;                                                  \
}                                                                             \

void update_gbc_sound(u32 cpu_ticks)
{
  u32 i, i2;
  GBCSoundStruct *gs = gbc_sound_channel;

  FIXED08_24 sample_index, frequency_step;
  FIXED08_24 tick_counter;
  u32 buffer_index, buffer_ticks;

  s32 volume_left, volume_right;
  u32 envelope_volume;

  s32 current_sample;
  s8 *sample_data;

  u64 count_ticks = delta_ticks(cpu_ticks, gbc_sound_last_cpu_ticks) * SOUND_FREQUENCY;

  buffer_ticks = FP08_24_TO_U32(count_ticks);
  gbc_sound_partial_ticks += FP08_24_FRACTIONAL_PART(count_ticks);

  if (FP08_24_TO_U32(gbc_sound_partial_ticks) != 0)
  {
    buffer_ticks++;
    gbc_sound_partial_ticks &= 0x00FFFFFF;
  }

  u16 sound_status= ADDRESS16(io_registers, 0x84) & 0xFFF0;

  if (sound_on != 0)
  {
    // Sound Channel 1 - Tone & Sweep
    gs = gbc_sound_channel + 0;

    if (gs->active_flag != 0)
    {
      sample_data = gs->sample_data;
      envelope_volume = gs->envelope_volume;

      GBC_SOUND_RENDER_CHANNEL(SAMPLES, 8, ENVELOPE, SWEEP);

      if (gs->active_flag != 0)
        sound_status |= 0x01;
    }

    // Sound Channel 2 - Tone
    gs = gbc_sound_channel + 1;

    if (gs->active_flag != 0)
    {
      sample_data = gs->sample_data;
      envelope_volume = gs->envelope_volume;

      GBC_SOUND_RENDER_CHANNEL(SAMPLES, 8, ENVELOPE, NOSWEEP);

      if (gs->active_flag != 0)
        sound_status |= 0x02;
    }

    // Sound Channel 3 - Wave Output
    gs = gbc_sound_channel + 2;

    GBC_SOUND_UPDATE_WAVE_RAM();

    if ((gs->active_flag & gbc_sound_wave_enable) != 0)
    {
      sample_data = gbc_sound_wave_samples;

      if (gbc_sound_wave_type != 0)
      {
        GBC_SOUND_RENDER_CHANNEL(SAMPLES, 64, NOENVELOPE, NOSWEEP);
      }
      else
      {
        if (gbc_sound_wave_bank != 0)
          sample_data += 32;

        GBC_SOUND_RENDER_CHANNEL(SAMPLES, 32, NOENVELOPE, NOSWEEP);
      }

      if (gs->active_flag != 0)
        sound_status |= 0x04;
    }

    // Sound Channel 4 - Noise
    gs = gbc_sound_channel + 3;

    if (gs->active_flag != 0)
    {
      envelope_volume = gs->envelope_volume;

      if (gbc_sound_noise_type != 0)
      {
        GBC_SOUND_RENDER_CHANNEL(NOISE, HALF, ENVELOPE, NOSWEEP);
      }
      else
      {
        GBC_SOUND_RENDER_CHANNEL(NOISE, FULL, ENVELOPE, NOSWEEP);
      }

      if (gs->active_flag != 0)
        sound_status |= 0x08;
    }
  }

  ADDRESS16(io_registers, 0x84) = sound_status;

  gbc_sound_last_cpu_ticks = cpu_ticks;
  gbc_sound_buffer_index = (gbc_sound_buffer_index + (buffer_ticks << 1)) & RING_BUFFER_MASK;

//  ReGBA_AudioUpdate();
}


// Special thanks to blarrg for the LSFR frequency used in Meridian, as posted
// on the forum at http://meridian.overclocked.org:
// http://meridian.overclocked.org/cgi-bin/wwwthreads/showpost.pl?Board=merid
// angeneraldiscussion&Number=2069&page=0&view=expanded&mode=threaded&sb=4
// Hope you don't mind me borrowing it ^_-

static void init_noise_table(s32 *table, u32 period, u32 bit_length)
{
  u32 shift_register = 0xFF;
  u32 mask = ~(1 << bit_length);
  s32 table_pos, bit_pos;
  s32 current_entry;
  s32 table_period = (period + 31) / 32;

  // Bits are stored in reverse  order so they can be more easily moved to
  // bit 31, for sign extended shift down.

  for (table_pos = 0; table_pos < table_period; table_pos++)
  {
    current_entry = 0;
    for (bit_pos = 31; bit_pos >= 0; bit_pos--)
    {
      current_entry |= (shift_register & 0x01) << bit_pos;

      shift_register = ((1 & (shift_register ^ (shift_register >> 1))) << bit_length) | ((shift_register >> 1) & mask);
    }

    table[table_pos] = current_entry;
  }
}


void reset_sound(void)
{
  DirectSoundStruct *ds = direct_sound_channel;
  GBCSoundStruct    *gs = gbc_sound_channel;
  u32 i;

  sound_on = 0;
  sound_buffer_base = 0;

  memset(sound_buffer, 0, sizeof(sound_buffer));

  for (i = 0; i < 2; i++, ds++)
  {
    ds->buffer_index = 0;
    ds->status = DIRECT_SOUND_INACTIVE;
    ds->fifo_top = 0;
    ds->fifo_base = 0;
    ds->fifo_fractional = 0;
    memset(ds->fifo, 0, sizeof(ds->fifo));
  }

  gbc_sound_buffer_index = 0;
  gbc_sound_last_cpu_ticks = 0;
  gbc_sound_partial_ticks = 0;

  gbc_sound_master_volume_left = 0;
  gbc_sound_master_volume_right = 0;
  gbc_sound_master_volume = 0;

  for (i = 0; i < 4; i++, gs++)
  {
    gs->status = GBC_SOUND_INACTIVE;
    gs->sample_data = square_pattern_duty[2];
    gs->active_flag = 0;
  }

  gbc_sound_wave_enable = 0;
  gbc_sound_wave_update = 0;
  gbc_sound_wave_type = 0;
  gbc_sound_wave_bank = 0;
  gbc_sound_wave_bank_user = 0x00;
  gbc_sound_wave_volume = 0;
  memset(gbc_sound_wave_ram_data, 0x88, sizeof(gbc_sound_wave_ram_data));
  memset(gbc_sound_wave_samples,  0x00, sizeof(gbc_sound_wave_samples));

  gbc_sound_noise_type = 0;
  gbc_sound_noise_index = 0;
}

void init_sound(void)
{
  gbc_sound_tick_step = FLOAT_TO_FP08_24(256.0f / SOUND_FREQUENCY);

  init_noise_table(gbc_sound_noise_table15, 32767, 14);
  init_noise_table(gbc_sound_noise_table7, 127, 6);

  reset_sound();
}


#define sound_savestate_body(type)                                            \
{                                                                             \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, sound_on);                       \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, sound_buffer_base);              \
                                                                              \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, gbc_sound_buffer_index);         \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, gbc_sound_last_cpu_ticks);       \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, gbc_sound_partial_ticks);        \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, gbc_sound_master_volume_left);   \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, gbc_sound_master_volume_right);  \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, gbc_sound_master_volume);        \
                                                                              \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, gbc_sound_wave_enable);          \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, gbc_sound_wave_type);            \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, gbc_sound_wave_bank);            \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, gbc_sound_wave_bank_user);       \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, gbc_sound_wave_volume);          \
  FILE_##type##_ARRAY(g_state_buffer_ptr, gbc_sound_wave_samples);            \
  FILE_##type##_ARRAY(g_state_buffer_ptr, gbc_sound_wave_ram_data);           \
                                                                              \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, gbc_sound_noise_type);           \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, gbc_sound_noise_index);          \
                                                                              \
  FILE_##type##_ARRAY(g_state_buffer_ptr, direct_sound_channel);              \
  FILE_##type##_ARRAY(g_state_buffer_ptr, gbc_sound_channel);                 \
}                                                                             \

void sound_read_mem_savestate(void)
{
  memset(sound_buffer, 0, sizeof(sound_buffer));

  sound_savestate_body(READ_MEM);

  gbc_sound_channel[0].sample_data = square_pattern_duty[2];
  gbc_sound_channel[1].sample_data = square_pattern_duty[2];
  gbc_sound_channel[2].sample_data = square_pattern_duty[2];
  gbc_sound_channel[3].sample_data = square_pattern_duty[2];
}

void sound_write_mem_savestate(void)
{
  sound_savestate_body(WRITE_MEM);
}


u32 ReGBA_GetAudioSamplesAvailable(void)
{
	return buffer_length(gbc_sound_buffer_index, sound_buffer_base, RING_BUFFER_SIZE);
}

u32 ReGBA_LoadNextAudioSample(s16* Left, s16* Right)
{
	if (sound_buffer_base == gbc_sound_buffer_index)
		return 0;

	*Left  = sound_buffer[sound_buffer_base];
	sound_buffer[sound_buffer_base] = 0;
	*Right = sound_buffer[sound_buffer_base + 1];
	sound_buffer[sound_buffer_base + 1] = 0;
	sound_buffer_base = (sound_buffer_base + 2) & RING_BUFFER_MASK;
	return 1;
}

u32 ReGBA_DiscardAudioSamples(u32 Count)
{
	u32 Available = ReGBA_GetAudioSamplesAvailable();
	if (Count > Available)
		Count = Available;
	if (sound_buffer_base + Count * 2 > RING_BUFFER_SIZE)
	{
		// Requested samples wrap around. Split the clearing.
		memset(&sound_buffer[sound_buffer_base], 0, (RING_BUFFER_SIZE - sound_buffer_base) * sizeof(s16));
		memset(sound_buffer, 0, ((sound_buffer_base + Count * 2) & RING_BUFFER_MASK) * sizeof(s16));
	}
	else
	{
		memset(&sound_buffer[sound_buffer_base], 0, Count * 2 * sizeof(s16));
	}
	sound_buffer_base = (sound_buffer_base + Count * 2) & RING_BUFFER_MASK;
	return Count;
}

