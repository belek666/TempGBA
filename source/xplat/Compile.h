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

/* Defines BLOCK_INSTRUCTION_COUNT */
#include "CompileArch.h"

typedef u32 GBAAddress;

enum CompilationArea {
	AREA_READONLY_THUMB,
	AREA_READONLY_ARM,
	AREA_WRITABLE_THUMB,
	AREA_WRITABLE_ARM
};

/*
 * To check for self-modification of code in a writable area, we keep here
 * an array of bytes for each GBA address line which can contain modifiable
 * code. If one of the bytes is 0xFF, it has been modified, and if it was
 * code, then it needs to be recompiled. The memory storage functions need
 * to see these arrays, so they're extern, and not static.
 */
extern u8 IWRAMModified[1024 * 32];
extern u8 EWRAMModified[1024 * 256];
extern u8 VRAMModified[1024 * 96];

/*
 * Before a writable block, this metadata structure is written, allowing the
 * LookupOrCompile functions to detect self-modification for the affected
 * (ARM or Thumb) code block and trigger recompilation to native code.
 */
struct WritableBlockMedatata {
	u32 BlockStartPC;
	u32 BlockEndPC;
};

/*
 * Determines the location of the end of a block of Thumb code starting at
 * the given address in the GBA address space.
 * The block ends when either of these occur:
 * a) an unconditional branch instruction is met;
 * b) the architecture-specific BLOCK_INSTRUCTION_COUNT has been exceeded
 *    for the block;
 * c) the boundary of the GBA memory section containing BlockStartPC has
 *    been reached, or nearly reached.
 * The return value is the address, in the GBA address space,
 * of the instruction after the last one to be included in the block. That
 * address is guaranteed to be 2-byte aligned.
 */
GBAAddress ScanThumbBlock(GBAAddress BlockStartPC);

/*
 * Determines the location of the end of a block of ARM code starting at
 * the given address in the GBA address space.
 * The block ends when either of these occur:
 * a) an unconditional branch instruction is met;
 * b) the architecture-specific BLOCK_INSTRUCTION_COUNT has been exceeded
 *    for the block;
 * c) the boundary of the GBA memory section containing BlockStartPC has
 *    been reached, or nearly reached.
 * The return value is the address, in the GBA address space,
 * of the instruction after the last one to be included in the block. That
 * address is guaranteed to be 4-byte aligned.
 */
GBAAddress ScanARMBlock(GBAAddress BlockStartPC);

/*
 * Compiles code in the Thumb state, starting from the given address in the
 * GBA address space. Native code is written to the host's address space,
 * starting at NativeCodeStart, in such a way that:
 * a) It can be called like a regular procedure for the host, with its
 *    usual calling convention. It is not jumped into. Any metadata for
 *    the block, if any, precedes NativeCodeStart.
 *    The prototype for the called procedure looks like this:
 *    GBAAddress Nameless(u32* Cycles, u32* Registers);
 * b) Code is written to load the current Program Counter (PC) into one of
 *    the host's registers if the Thumb code is about to read it.
 * c) Code is written to call ReadMemory8, ReadMemory16 and ReadMemory32
 *    with the address as a parameter, if the Thumb code is about to read
 *    from GBA memory. Similarly, code is written to call WriteMemory8,
 *    WriteMemory16 and WriteMemory32 if the code is about to write to
 *    GBA memory, with the address and value as parameters.
 * d) Code is written to update cycles after some instructions.
 * e) Code is written to update the CPSR after instructions.
 * f) The compiled procedure's return value shall be the Program Counter
 *    in the GBA address space at which execution should continue, interpreted
 *    as STATE_EITHER (if a Thumb procedure jumps to another Thumb
 *    procedure, or to somewhere within itself, bit 0 of the return address
 *    shall be 1). If a conditional branch is taken, the procedure returns
 *    at that point with the new address. If a conditional branch is not
 *    taken, execution continues inside the compiled procedure. If an
 *    unconditional branch appears, the procedure returns at that point
 *    and so does the compiler.
 * The call to the compiled procedure can be as follows:
 *   reg[REG_PC] = (*NativeCodeStart)(&cycles, &regs);
 * Returns 1 if it has successfully created a block, and 0 if it has not.
 * If successful:
 * a) BlockEndPC is dereferenced and updated with the address, in the GBA
 *    address space, of the instruction after the last one compiled.
 * b) NativeCodeEnd is dereferenced and updated with a pointer in the host's
 *    address space of the byte immediately after the last one emitted.
 * c) The native code that was just written is made visible to the host's
 *    instruction fetcher, if needed. On the MIPS, this means the native code
 *    is written back to memory and the written region is invalidated in the
 *    instruction cache.
 */
u8 CompileThumbCode(GBAAddress BlockStartPC, u8* NativeCodeStart, GBAAddress* BlockEndPC, u8** NativeCodeEnd);

/*
 * Compiles code in the ARM state, starting from the given address in the
 * GBA address space. Native code is written to the host's address space,
 * starting at NativeCodeStart, in such a way that:
 * a) It can be called like a regular procedure for the host, with its
 *    usual calling convention. It is not jumped into. Any metadata for
 *    the block, if any, precedes NativeCodeStart.
 *    The prototype for the called procedure looks like this:
 *    GBAAddress Nameless(u32* Cycles, u32* Registers);
 * b) Code is written to load the current Program Counter (PC) into one of
 *    the host's registers if the Thumb code is about to read it.
 * c) Code is written to call ReadMemory8, ReadMemory16 and ReadMemory32
 *    with the address as a parameter, if the Thumb code is about to read
 *    from GBA memory. Similarly, code is written to call WriteMemory8,
 *    WriteMemory16 and WriteMemory32 if the code is about to write to
 *    GBA memory, with the address and value as parameters.
 * d) Code is written to update cycles after some instructions.
 * e) Code is written to update the CPSR after instructions.
 * f) The compiled procedure's return value shall be the Program Counter
 *    in the GBA address space at which execution should continue, interpreted
 *    as STATE_EITHER (if an ARM procedure jumps to another ARM
 *    procedure, or to somewhere within itself, bit 0 of the return address
 *    shall be 0). If a conditional branch is taken, the procedure returns
 *    at that point with the new address. If a conditional branch is not
 *    taken, execution continues inside the compiled procedure. If an
 *    unconditional branch appears, the procedure returns at that point
 *    and so does the compiler.
 * The call to the compiled procedure can be as follows:
 *   reg[REG_PC] = (*NativeCodeStart)(&cycles, &regs);
 * Returns 1 if it has successfully created a block, and 0 if it has not.
 * If successful:
 * a) BlockEndPC is dereferenced and updated with the address, in the GBA
 *    address space, of the instruction after the last one compiled.
 * b) NativeCodeEnd is dereferenced and updated with a pointer in the host's
 *    address space of the byte immediately after the last one emitted.
 * c) The native code that was just written is made visible to the host's
 *    instruction fetcher, if needed. On the MIPS, this means the native code
 *    is written back to memory and the written region is invalidated in the
 *    instruction cache.
 */
u8 CompileARMCode(GBAAddress BlockStartPC, u8* NativeCodeStart, GBAAddress* BlockEndPC, u8** NativeCodeEnd);

/*
 * Looks up the address in the host's address space containing native code
 * corresponding to the ARM or Thumb code at the given address in the GBA
 * address space, or compile it if it hasn't been compiled yet.
 * The Program Counter is interpreted as STATE_EITHER, so as Thumb if
 * the least significant bit is 1 and as ARM otherwise.
 * The return value is NULL if no native code is available to replace the
 * code at the given GBA address and the compiler has failed to create a
 * block.
 */
u8* LookupOrCompileEither(GBAAddress PC);

/*
 * Looks up the address in the host's address space containing native code
 * corresponding to the Thumb code at the given address in the GBA
 * address space, or compile it if it hasn't been compiled yet.
 * The return value is NULL if no native code is available to replace the
 * code at the given GBA address and the compiler has failed to create a
 * block.
 */
u8* LookupOrCompileThumb(GBAAddress PC);

/*
 * Looks up the address in the host's address space containing native code
 * corresponding to the ARM code at the given address in the GBA
 * address space, or compile it if it hasn't been compiled yet.
 * The return value is NULL if no native code is available to replace the
 * code at the given GBA address and the compiler has failed to create a
 * block.
 */
u8* LookupOrCompileARM(GBAAddress PC);

/*
 * Empties a compilation area in response to any of these events:
 * a) The area has been deemed too full to accommodate a new block of
 *    native instructions (affects the specified area);
 * b) The area is writable and has reached the maximum allowable tag,
 *    which is 0xFEFE (affects the specified area).
 * b) A new ROM is loaded (affects all areas);
 * c) A saved state is loaded, invalidating all contents of IWRAM, EWRAM and
 *    VRAM (affects AREA_WRITABLE_ARM and AREA_WRITABLE_THUMB).
 */
void FlushCompilationArea(enum CompilationArea Area);
