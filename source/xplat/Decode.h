/*
 * Instruction decoder - common data
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

enum ARMCondition {
	/* EQ (EQual): Z == 1 */
	CONDITION_EQ,
	/* NE (Not Equal): Z == 0 */
	CONDITION_NE,
	/* CS (Carry Set): C == 1 */
	CONDITION_CS,
	/* CC (Carry Clear): C == 0 */
	CONDITION_CC,
	/* MI (MInus): N == 1 */
	CONDITION_MI,
	/* PL (PLus): N == 0 */
	CONDITION_PL,
	/* VS (oVerflow Set): V == 1 */
	CONDITION_VS,
	/* VC (oVerflow Clear): V == 0 */
	CONDITION_VC,
	/* HI (unsigned HIgher): C == 1 && Z == 0 */
	CONDITION_HI,
	/* LS (unsigned Lower or Same): C == 0 || Z == 1 */
	CONDITION_LS,
	/* GE (Greater than or Equal): N == V */
	CONDITION_GE,
	/* LT (Less Than): N != V */
	CONDITION_LT,
	/* GT (Greater Than): Z == 0 && N == V */
	CONDITION_GT,
	/* LE (Less than or Equal): Z == 1 || N != V */
	CONDITION_LE,
	/* AL (ALl) */
	CONDITION_AL,
	/* 0xF is reserved and must not be used.
	 * (ARM Instruction Set, section 4.2, "The Condition Field") */
	CONDITION_RESERVED
};

enum ARMInstructionFlags {
	FLAG_N = 0x80000000,
	FLAG_Z = 0x40000000,
	FLAG_C = 0x20000000,
	FLAG_V = 0x10000000
};
