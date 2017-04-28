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

/******************************************************************************
 * main.h
 * メインヘッダ
 ******************************************************************************/
#ifndef MAIN_H
#define MAIN_H

#include "message.h"

#define LANGUAGE_PACK   "SYSTEM/language.msg"

/******************************************************************************
 * グローバル変数の宣言
 ******************************************************************************/
extern u32 execute_cycles;
extern u32 cpu_ticks;
extern u32 dma_cycle_count;

extern u32 to_skip;
extern u32 skip_next_frame_flag;
extern u32 frame_ticks;
extern unsigned int frame_interval; // For in-memory saved states used in rewinding

/******************************************************************************
 * グローバル関数の宣言
 ******************************************************************************/
u32 update_gba(void);
void reset_gba(void);
void quit(void);
void main_read_mem_savestate(void);
void main_write_mem_savestate(void);
void error_msg(char *text);
void change_ext(char *src, char *buffer, char *extension);
extern int gpsp_main(int argc, char **argv);
extern char* FS_FGets(char *buffer, int num, FILE_TAG_TYPE stream);

#endif /* MAIN_H */

