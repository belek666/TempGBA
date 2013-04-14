/*
 * Instruction decoder - data structures
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

/*
 * If the number of members of 'enum ARMAndThumbOpcodes' rises above 255,
 * the width of this type should rise appropriately.
 */
typedef u8 Opcode;

struct DecodedInstruction {
	/*
	 * This opcode field does not store an actual ARM or Thumb opcode,
	 * but rather one of the members of 'enum ARMAndThumbOpcodes', defined
	 * below.
	 */
	Opcode Op;
	/*
	 * The condition code for an ARM instruction, or 0xE (1110 binary) for
	 * Thumb instructions that don't have a condition code.
	 */
	u8 Condition;
	/*
	 * Registers specified by the instruction, in the order of highest bit
	 * to lowest bit, except register-list operands.
	 */
	u8 Reg1, Reg2, Reg3, Reg4;
	/*
	 * The flags modified by the instruction, in the order used by the
	 * ARM CPSR register:
	 * Bit 3 = N (Negative)
	 * Bit 2 = Z (Zero)
	 * Bit 1 = C (Carry)
	 * Bit 0 = V (oVerflow)
	 */
	u8 FlagsModified;
	/*
	 * The registers used by a register-list instruction.
	 */
	u16 RegisterList;
	/*
	 * An unsigned immediate value specified by the instruction.
	 */
	u32 Immediate;
	/*
	 * A signed immediate offset specified by the instruction. This is
	 * used in branches.
	 */
	s32 Offset;
	/*
	 * Other bits not decoded by the instruction decoder, in instruction-
	 * specific order.
	 */
	u32 OtherBits;
};

enum ARMAndThumbOpcodes {
	OPCODE_ADC                     /* ARM and Thumb */,
	OPCODE_ADD                     /* ARM and Thumb */,
	OPCODE_AND                     /* ARM and Thumb */,
	OPCODE_ASR                     /* Thumb */,
	OPCODE_B                       /* ARM and Thumb (w/ conditions) */,
	OPCODE_BIC                     /* ARM and Thumb */,
	OPCODE_BL                      /* ARM and Thumb */,
	OPCODE_BX                      /* ARM and Thumb */,
	OPCODE_CDP                     /* ARM */,
	OPCODE_CMN                     /* ARM and Thumb */,
	OPCODE_CMP                     /* ARM and Thumb */,
	OPCODE_EOR                     /* ARM and Thumb */,
	OPCODE_LDC                     /* ARM */,
	OPCODE_LDM                     /* ARM */,
	OPCODE_LDMIA                   /* Thumb */,
	OPCODE_LDR                     /* ARM and Thumb */,
	OPCODE_LDRB                    /* Thumb */,
	OPCODE_LDRH                    /* Thumb */,
	OPCODE_LSL                     /* Thumb */,
	OPCODE_LDSB                    /* Thumb */,
	OPCODE_LDSH                    /* Thumb */,
	OPCODE_LSR                     /* Thumb */,
	OPCODE_MCR                     /* ARM */,
	OPCODE_MLA                     /* ARM */,
	OPCODE_MOV                     /* ARM and Thumb */,
	OPCODE_MRC                     /* ARM */,
	OPCODE_MRS                     /* ARM */,
	OPCODE_MSR                     /* ARM */,
	OPCODE_MUL                     /* ARM and Thumb */,
	OPCODE_MVN                     /* ARM and Thumb */,
	OPCODE_NEG                     /* Thumb */,
	OPCODE_ORR                     /* ARM and Thumb */,
	OPCODE_POP                     /* Thumb */,
	OPCODE_PUSH                    /* Thumb */,
	OPCODE_ROR                     /* Thumb */,
	OPCODE_RSB                     /* ARM */,
	OPCODE_RSC                     /* ARM */,
	OPCODE_SBC                     /* ARM and Thumb */,
	OPCODE_STC                     /* ARM */,
	OPCODE_STM                     /* ARM */,
	OPCODE_STMIA                   /* Thumb */,
	OPCODE_STR                     /* ARM and Thumb */,
	OPCODE_STRB                    /* ARM and Thumb */,
	OPCODE_STRH                    /* ARM and Thumb */,
	OPCODE_SUB                     /* ARM */,
	OPCODE_SWI_ARM                 /* ARM: 24-bit SWI comment */,
	OPCODE_SWI_THUMB               /* Thumb: 8-bit SWI comment */,
	OPCODE_SUB                     /* Thumb */,
	OPCODE_SWP                     /* ARM */,
	OPCODE_TEQ                     /* ARM */,
	OPCODE_TST                     /* ARM and Thumb */,
};
