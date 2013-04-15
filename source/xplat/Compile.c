/*
 * ARM compiler - cross-platform driver code
 * Copyright (C) 2013 GBATemp user Nebuleon
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "common.h"
#include "Compile.h"
#include "Decode.h"

/* Read-only Thumb is mostly the Game Pak, but can be some BIOS stuff too. */
#define CACHE_SIZE_READONLY_THUMB (1024 * 1024)
/* Read-only ARM is mostly the BIOS, but can be some Game Pak stuff too. */
#define CACHE_SIZE_READONLY_ARM   (1024 * 96)
/* Writable Thumb is mostly the EWRAM and VRAM, but can be some IWRAM stuff
 * too. */
#define CACHE_SIZE_WRITABLE_THUMB (1024 * 384)
/* Writable ARM is mostly the IWRAM, but can be some EWRAM and VRAM stuff
 * too. */
#define CACHE_SIZE_WRITABLE_ARM   (1024 * 160)

/*
 * The cache size threshold dictates the minimum space required to start
 * compiling a block. If we don't have enough space, we need to flush the
 * affected compilation area first.
 * This value controls how high BLOCK_INSTRUCTION_COUNT can be
 * on a specific platform: that number of GBA instructions needs to fit inside
 * CACHE_SIZE_THRESHOLD bytes of native code.
 */
#define CACHE_SIZE_THRESHOLD      4096

static u8 CodeCacheReadonlyThumb[CACHE_SIZE_READONLY_THUMB];
static u8 CodeCacheReadonlyARM  [CACHE_SIZE_READONLY_ARM];
static u8 CodeCacheWritableThumb[CACHE_SIZE_WRITABLE_THUMB];
static u8 CodeCacheWritableARM  [CACHE_SIZE_WRITABLE_ARM];

static u8* NextReadonlyThumb = CodeCacheReadonlyThumb;
static u8* NextReadonlyARM   = CodeCacheReadonlyARM;
static u8* NextWritableThumb = CodeCacheWritableThumb;
static u8* NextWritableARM   = CodeCacheWritableARM;

// TODO Hash tables for native code block addresses here

u8 IWRAMModified[1024 * 32];
u8 EWRAMModified[1024 * 256];
u8 VRAMModified[1024 * 96];

GBAAddress ScanThumbBlock(GBAAddress BlockStartPC)
{
	return BlockStartPC + 2; // For now, one block per instruction
}

GBAAddress ScanARMBlock(GBAAddress BlockStartPC)
{
	return BlockStartPC + 4; // For now, one block per instruction
}

u8 CompileThumbCode(GBAAddress BlockStartPC, u8* NativeCodeStart)
{
	return 0; // Supports nothing
}

u8 CompileARMCode(GBAAddress BlockStartPC, u8* NativeCodeStart)
{
	return 0; // Supports nothing
}

u8* LookupOrCompileEither(GBAAddress PC)
{
	return NULL; // Supports no code storage zones
}

u8* LookupOrCompileThumb(GBAAddress PC)
{
	return NULL; // Supports no code storage zones
}

u8* LookupOrCompileARM(GBAAddress PC)
{
	return NULL; // Supports no code storage zones
}

void FlushCompilationArea(enum CompilationArea Area)
{
	switch (Area)
	{
		case AREA_READONLY_THUMB:
			// TODO Also clear the hash table for this area, when implemented
			NextReadonlyThumb = CodeCacheReadonlyThumb;
			break;
		case AREA_READONLY_ARM:
			// TODO Also clear the hash table for this area, when implemented
			NextReadonlyARM = CodeCacheReadonlyARM;
			break;
		case AREA_WRITABLE_THUMB:
			// TODO Also clear the hash table for this area, when implemented
			NextWritableThumb = CodeCacheWritableThumb;
			break;
		case AREA_WRITABLE_ARM:
			// TODO Also clear the hash table for this area, when implemented
			NextWritableARM = CodeCacheWritableARM;
			break;
	}
}
