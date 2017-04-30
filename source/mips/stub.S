# unofficial gameplaySP kai
#
# Copyright (C) 2006 Exophase <exophase@gmail.com>
# Copyright (C) 2007 takka <takka@tfact.net>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

#ifdef MIPS_32R2
.set mips32r2
#endif

#ifdef _EE
#define _SA 4
#else
#define _SA 1
#endif

.align 4

.global mips_update_gba
.global mips_indirect_branch_arm
.global mips_indirect_branch_thumb
.global mips_indirect_branch_dual
.global mips_indirect_branch_arm_no_update_gba
.global mips_indirect_branch_thumb_no_update_gba
.global mips_indirect_branch_dual_no_update_gba

.global execute_load_u8
.global execute_load_u16
.global execute_load_u32
.global execute_load_s8
.global execute_load_s16
.global execute_store_u8
.global execute_store_u16
.global execute_store_u32
.global execute_aligned_load32
.global execute_aligned_store32
.global execute_read_cpsr
.global execute_read_spsr

.global execute_swi
.global execute_spsr_restore
.global execute_store_cpsr
.global execute_store_spsr
.global execute_lsl_flags_reg
.global execute_lsr_flags_reg
.global execute_asr_flags_reg
.global execute_ror_flags_reg
.global invalidate_icache_region
.global invalidate_all_cache
.global call_bios_hle

.global memory_map_read
.global memory_map_write
.global reg

.extern ReGBA_MakeCodeVisible

.extern spsr

# MIPS register layout:

# $0 - constant zero
# $1 - temporary
# $2 - temporary / return value
# $3 - ARM r0 (not saved)
# $4 - temporary / function argument 0
# $5 - temporary / function argument 1
# $6 - temporary / function argument 2
# $7 - ARM r1 (not saved)
# $8 - ARM r2 (not saved)
# $9 - ARM r3 (not saved)
# $10 - ARM r4 (not saved)
# $11 - ARM r5 (not saved)
# $12 - ARM r6 (not saved)
# $13 - ARM r7 (not saved)
# $14 - ARM r8 (not saved)
# $15 - ARM r9 (not saved)
# $16 - ARM machine state pointer (saved)
# $17 - cycle counter (saved)
# $18 - ARM r10 (saved)
# $19 - block start address (roughly r15) (saved)
# $20 - ARM negative register (saved)
# $21 - ARM zero register (saved)
# $22 - ARM carry register (saved)
# $23 - ARM overflow register (saved)
# $24 - ARM r11 (not saved)
# $25 - ARM r12 (not saved)
# $26 - kernel temporary 0
# $27 - kernel temporary 1
# $28 - ARM r13 (saved)
# $29 - stack pointer
# $30 - ARM r14 (saved)
# $31 - return address

.equ REG_R0,              (0 * 4)
.equ REG_R1,              (1 * 4)
.equ REG_R2,              (2 * 4)
.equ REG_R3,              (3 * 4)
.equ REG_R4,              (4 * 4)
.equ REG_R5,              (5 * 4)
.equ REG_R6,              (6 * 4)
.equ REG_R7,              (7 * 4)
.equ REG_R8,              (8 * 4)
.equ REG_R9,              (9 * 4)
.equ REG_R10,             (10 * 4)
.equ REG_R11,             (11 * 4)
.equ REG_R12,             (12 * 4)
.equ REG_R13,             (13 * 4)
.equ REG_R14,             (14 * 4)
.equ REG_LR,              (14 * 4)
.equ REG_PC,              (15 * 4)
.equ REG_N_FLAG,          (16 * 4)
.equ REG_Z_FLAG,          (17 * 4)
.equ REG_C_FLAG,          (18 * 4)
.equ REG_V_FLAG,          (19 * 4)
.equ REG_CPSR,            (20 * 4)
.equ REG_SAVE,            (21 * 4)
.equ REG_SAVE2,           (22 * 4)
.equ REG_SAVE3,           (23 * 4)

.equ CPU_MODE,            (29 * 4)
.equ CPU_HALT_STATE,      (30 * 4)
.equ CHANGED_PC_STATUS,   (31 * 4)
.equ GP_SAVE,             (32 * 4)

.equ SUPERVISOR_LR,       (reg_mode + (3 * (7 * 4)) + (6 * 4))
.equ SUPERVISOR_SPSR,     (spsr + (3 * 4))

.set noat
.set noreorder

# make sure $16 has the register base for these macros

.macro collapse_flags_body        # insert flag into $2
#ifdef MIPS_32R2
  andi  $2, $2, 0xFF              # isolate lower 8 bits
  ins   $2, $20, 31, 1            # flag in $20 goes to $2, bit 31
  ins   $2, $21, 30, 1            # flag in $21 goes to $2, bit 30
  ins   $2, $22, 29, 1            # flag in $22 goes to $2, bit 29
  ins   $2, $23, 28, 1            # flag in $23 goes to $2, bit 28
#else
  sll   $20, $20, 31              # delay usefully even if loads don't delay
  andi  $2, $2, 0xFF              # isolate lower 8 bits
  or    $2, $2, $20
  sll   $20, $21, 30
  or    $2, $2, $20
  sll   $20, $22, 29
  or    $2, $2, $20
  sll   $20, $23, 28
  or    $2, $2, $20
  srl   $20, $2, 31
#endif
.endm

.macro collapse_flags
  lw $2, REG_CPSR($16)            # load CPSR
  collapse_flags_body             # insert flag into $2
  sw $2, REG_CPSR($16)            # store CPSR
.endm

.macro extract_flags_body         # extract flags from $1
  srl $20, $1, 31                 # flag in $1 bit 31 goes into $20
#ifdef MIPS_32R2
  ext $21, $1, 30, 1              # flag in $1 bit 30 goes into $21
  ext $22, $1, 29, 1              # flag in $1 bit 29 goes into $22
  ext $23, $1, 28, 1              # flag in $1 bit 28 goes into $23
#else
  srl $21, $1, 30
  andi $21, $21, 1
  srl $22, $1, 29
  andi $22, $22, 1
  srl $23, $1, 28
  andi $23, $23, 1
#endif
.endm

.macro extract_flags
  lw $1, REG_CPSR($16)            # load CPSR
#ifndef MIPS_XBURST
  nop
#endif
  extract_flags_body
.endm

.macro save_registers_0
  sw $3, REG_R0($16)
  sw $7, REG_R1($16)
  sw $8, REG_R2($16)
  sw $9, REG_R3($16)
  sw $10, REG_R4($16)
  sw $11, REG_R5($16)
  sw $12, REG_R6($16)
  sw $13, REG_R7($16)
  sw $14, REG_R8($16)
  sw $15, REG_R9($16)
  sw $18, REG_R10($16)
  sw $24, REG_R11($16)
  sw $25, REG_R12($16)
  sw $28, REG_R13($16)
.endm

.macro save_registers_1
  sw $30, REG_R14($16)
.endm

.macro save_registers
  save_registers_0
  save_registers_1
.endm

.macro restore_registers_0
  lw $3, REG_R0($16)
  lw $7, REG_R1($16)
  lw $8, REG_R2($16)
  lw $9, REG_R3($16)
  lw $10, REG_R4($16)
  lw $11, REG_R5($16)
  lw $12, REG_R6($16)
  lw $13, REG_R7($16)
  lw $14, REG_R8($16)
  lw $15, REG_R9($16)
  lw $18, REG_R10($16)
  lw $24, REG_R11($16)
  lw $25, REG_R12($16)
  lw $28, REG_R13($16)
.endm

.macro restore_registers_1
  lw $30, REG_R14($16)
.endm

.macro restore_registers
  restore_registers_0
  restore_registers_1
.endm


# Process a hardware event. Since an interrupt might be
# raised we have to check if the PC has changed.

# $4: next address
# $16: register base
# $17: cycle counter

.balign 32

mips_update_gba:
  lw $2, REG_CPSR($16)            # load CPSR
  sw $4, REG_PC($16)              # current PC = $4
  sw $0, CHANGED_PC_STATUS($16)

  addiu $sp, $sp, -4 * _SA        # make room on the stack
  sw $ra,($sp)                    # save return address

  save_registers                  # save the registers
  collapse_flags_body             # insert flag into $2

  jal update_gba                  # process the next event
  sw $2, REG_CPSR($16)            # store CPSR (delay)

  lw $1, CHANGED_PC_STATUS($16)   # load the new value 3 instructions early

  lw $ra, ($sp)                   # restore return address
  addiu $sp, $sp, 4 * _SA         # fix stack

  bne $1, $0, lookup_pc_changed
  addu $17, $2, $0                # $17 = new cycle count (delay slot)

  restore_registers
  jr $ra                          # if not, go back to caller
  nop                             # delay so the caller can use $30

branch_arm_update_gba:
  lw $2, REG_CPSR($16)            # load CPSR
  sw $4, REG_PC($16)              # next PC = $4

  save_registers                  # save the registers
  collapse_flags_body             # insert flag into $2

  jal update_gba                  # process the next event
  sw $2, REG_CPSR($16)            # store CPSR (delay)

  lw $1, CHANGED_PC_STATUS($16)   # load the new value 3 instructions early
#ifndef MIPS_XBURST
  nop
#endif

  bne $1, $0, lookup_pc_changed
  addu $17, $2, $0                # $17 = new cycle count (delay slot)

  jal block_lookup_address_arm    # $2 = MIPS address to jump to
  lw $4, REG_PC($16)              # restore branch address (delay)

  restore_registers
  jr $2                           # jump to it
  nop                             # delay so the caller can use $30

branch_thumb_update_gba:
  lw $2, REG_CPSR($16)            # load CPSR
  sw $4, REG_PC($16)              # next PC = $4

  save_registers                  # save the registers
  collapse_flags_body             # insert flag into $2

  jal update_gba                  # process the next event
  sw $2, REG_CPSR($16)            # store CPSR (delay)

  lw $1, CHANGED_PC_STATUS($16)   # load the new value 3 instructions early
#ifndef MIPS_XBURST
  nop
#endif

  bne $1, $0, lookup_pc_changed
  addu $17, $2, $0                # $17 = new cycle count (delay slot)

  jal block_lookup_address_thumb  # $2 = MIPS address to jump to
  lw $4, REG_PC($16)              # restore branch address (delay)

  restore_registers
  jr $2                           # jump to it
  nop                             # delay so the caller can use $30

branch_dual_update_gba:
  lw $2, REG_CPSR($16)            # load CPSR
  sw $4, REG_PC($16)              # next PC = $4
  save_registers                  # save the registers

  andi $1, $4, 0x01
#ifdef MIPS_32R2
  ins $2, $1, 5, 1                # state bit = address & 0x01
#else
  andi $2, $2, 0xDF               # mask state bit
  srl $1, $1, 5
  or $2, $2, $1
#endif
  collapse_flags_body             # insert flag into $2

  jal update_gba                  # process the next event
  sw $2, REG_CPSR($16)            # store CPSR (delay)

  lw $1, CHANGED_PC_STATUS($16)   # load the new value 3 instructions early
#ifndef MIPS_XBURST
  nop
#endif

  bne $1, $0, lookup_pc_changed
  addu $17, $2, $0                # $17 = new cycle count (delay slot)

  jal block_lookup_address_dual   # $2 = MIPS address to jump to
  lw $4, REG_PC($16)              # restore branch address (delay)

  restore_registers
  jr $2                           # jump to it
  nop                             # delay so the caller can use $30


.macro count_branch_ticks_arm     # Add 2S+1N cycles
  la $2, memory_waitstate         # multiple instruction
  addu $2, $2, $1
  lb $1, 0x10($2)                 # ARM state offset +16
  addiu $17, $17, -2              # CPU cycles
  sll $1, $1, 1
  subu $17, $17, $1               # waitstate 2S

  lb $1, 0x30($2)                 # ARM state offset +16
#ifndef MIPS_XBURST
  nop
#endif
  subu $17, $17, $1               # waitstate 1N
.endm

.macro count_branch_ticks_thumb   # Add 2S+1N cycles
  la $2, memory_waitstate         # multiple instruction
  addu $2, $2, $1
  lb $1, 0($2)
  addiu $17, $17, -2              # CPU cycles
  sll $1, $1, 1
  subu $17, $17, $1               # waitstate 2S

  lb $1, 0x20($2)
#ifndef MIPS_XBURST
  nop
#endif
  subu $17, $17, $1               # waitstate 1N
.endm

.macro count_branch_ticks_dual    # Add 2S+1N cycles
  la $2, memory_waitstate         # multiple instruction
  addu $2, $2, $1
  lb $1, 0($2)
  addiu $17, $17, -2              # CPU cycles
  sll $1, $1, 1
  subu $17, $17, $1               # waitstate 2S

  lb $1, 0x20($2)
#ifndef MIPS_XBURST
  nop
#endif
  subu $17, $17, $1               # waitstate 1N
.endm


# Perform an indirect branch.

# $4: GBA address to branch to

mips_indirect_branch_arm:
  srl $1, $4, 24
  count_branch_ticks_arm

  blez $17, branch_arm_update_gba
  sw $0, CHANGED_PC_STATUS($16)

mips_indirect_branch_arm_no_update_gba:
  save_registers_0                # save the registers 1/2
  jal block_lookup_address_arm    # $2 = MIPS address to jump to
  save_registers_1                # save the registers 2/2 (delay)
  
  restore_registers
  jr $2                           # jump to it
  nop                             # delay so the target can use $30

mips_indirect_branch_thumb:
  srl $1, $4, 24
  count_branch_ticks_thumb

  blez $17, branch_thumb_update_gba
  sw $0, CHANGED_PC_STATUS($16)

mips_indirect_branch_thumb_no_update_gba:
  save_registers_0                # save the registers 1/2
  jal block_lookup_address_thumb  # $2 = MIPS address to jump to
  save_registers_1                # save the registers 2/2 (delay)

  restore_registers
  jr $2                           # jump to it
  nop                             # delay so the target can use $30

mips_indirect_branch_dual:
  srl $1, $4, 24
  andi $2, $4, 0x01               # address & 0x01
  beql $2, $0, 1f
  addiu $1, $1, 0x10              # if ARM state offset +16 (delay Likely)
1:
  count_branch_ticks_dual

  blez $17, branch_dual_update_gba
  sw $0, CHANGED_PC_STATUS($16)

mips_indirect_branch_dual_no_update_gba: 
  save_registers_0                # save the registers 1/2
  jal block_lookup_address_dual   # $2 = MIPS address to jump to
  save_registers_1                # save the registers 2/2 (delay)
  
  restore_registers
  jr $2                           # jump to it
  nop                             # delay so the target can use $30


# $4: address to write to
# $5: current PC

# Will patch the return address with a call to the correct handler as
# listed in the given table.

# Value will be set to force_open if it's open

.macro patch_handler ftable, force_open
  srl $1, $4, 24                  # $1 = address region
  sltiu $2, $1, 0x0F              # check if the value is open
  sll $1, $1, 2                   # make address word indexed

  beql $2, $0, 1f
  addiu $1, $0, (\force_open * 4) # (delay Likely)

1:
  lui $2, %hi(\ftable)
  addu $2, $2, $1
  lw $2, %lo(\ftable)($2)         # new function handler is in $2

  lui   $1, %hi(3 << 26)          # $1 = 3 (JAL opcode)
#ifdef MIPS_32R2
  srl   $2, $2, 2
  ins   $1, $2, 0, 26             # insert offset into the generated JAL
#else
  sll   $2, $2, 4
  srl   $2, $2, 6
  or    $1, $1, $2                # insert offset into the generated JAL
#endif

  addiu $ra, $ra, -8              # rewind return address to function call
  sw $1, ($ra)                    # modify to call new handler
  
  addiu $sp, $sp, -12 * _SA       # adjust the stack to allow for a call
  sw    $ra, 0($sp)               # save the return address (branch delay)
  sw    $4,  4($sp)
  sw    $5,  8($sp)

  move  $4, $ra                   # parameter #1: start of code to be flushed
  jal   ReGBA_MakeCodeVisible
  ori   $5, $0, 4                 # parameter #2: length (branch delay)

  lw    $ra, 0($sp)
  lw    $4,  4($sp)
  lw    $5,  8($sp)
  addiu $sp, $sp, 12 * _SA        # adjust the stack back

#ifdef PERFORMANCE_IMPACTING_STATISTICS
  la $1, Stats
  lw $2, 0($1)
#  ifndef MIPS_XBURST
  nop
#  endif
  addiu $2, $2, 1
  sw $2, 0($1)
#endif

  jr $ra                          # return
  nop
.endm


# Like the above, but will use the table of the proper alignment,
# The tables should be ordered by alignment

.macro patch_handler_align ftable, alignment
  srl $1, $4, 24                  # $1 = address region
  sltiu $2, $1, 0x0F              # check if the value is open
  sll $1, $1, 2                   # make address word indexed

  beql $2, $0, 1f
  addiu $1, $0, 4                 # force address to 0x1 (open) delay slot, Likely

1:
#ifdef MIPS_32R2
  ins $1, $4, 6, \alignment       # place alignment bits into offset
#else
  ori  $2, $0, (((1<<\alignment) -1)<<6)
  nor  $2, $0, $2
  and  $1, $1, $2
  andi $2, $4, ((1<<\alignment) -1)
  sll  $2, $2, 6
  or   $1, $1, $2
#endif

  lui $2, %hi(\ftable)
  addu $2, $2, $1
  lw $2, %lo(\ftable)($2)         # new function handler is in $2

  lui $1, %hi(3 << 26)            # $1 = 3 << 26 (JAL opcode), delays usefully
#ifdef MIPS_32R2
  srl $2, $2, 2                   # remove lower two bits
  ins $1, $2, 0, 26               # insert offset into the generated JAL
#else
  sll $2, $2, 4
  srl $2, $2, 6
  or  $1, $1, $2                  # insert offset into the generated JAL
#endif

  addiu $ra, $ra, -8              # rewind return address to function call
  sw $1, ($ra)                    # modify to call new handler

  addiu $sp, $sp, -12 * _SA       # adjust the stack to allow for a call
  sw    $ra, 0($sp)               # save the return address (branch delay)
  sw    $4,  4($sp)
  sw    $5,  8($sp)

  move  $4, $ra                    # parameter #1: start of code to be flushed
  jal   ReGBA_MakeCodeVisible
  ori   $5, $0, 4                  # parameter #2: length (branch delay)

  lw    $ra, 0($sp)
  lw    $4,  4($sp)
  lw    $5,  8($sp)
  addiu $sp, $sp, 12 * _SA        # adjust the stack back

#ifdef PERFORMANCE_IMPACTING_STATISTICS
  la $1, Stats
  lw $2, 0($1)
#  ifndef MIPS_XBURST
  nop
#  endif
  addiu $2, $2, 1
  sw $2, 0($1)
#endif

  jr $ra                          # return
  nop
.endm


.macro region_check region, patch_handler
  srl $1, $4, 24                  # check upper 8bits of address
  xor $1, $1, \region             # see if it is the given region
  bne $1, $0, \patch_handler      # if not repatch/try again
  # There is a branch delay slot here
.endm

.macro region_check_open patch_handler
  srl $1, $4, 24                  # check upper 8bits of address
  sltiu $2, $1, 0x0F              # true if it is a low address
  addiu $1, $1, -1                # non-zero if it is not a low open
  sltu $1, $0, $1                 # true if lower bits != 1
  and $1, $1, $2                  # true if low address and not open
  bne $1, $0, \patch_handler      # if above is true, patch
  # There is a branch delay slot here
.endm


.macro region_check_align region, align_bits, alignment, patch_handler
  srl $1, $4, 24                  # check upper 8bits of address
#ifdef MIPS_32R2
  ins $1, $4, 8, \align_bits      # look at lower bits of address too
#else
  andi $2, $4, ((1<<\align_bits) -1)
  sll  $2, $2, 8
  or   $1, $1, $2
#endif
  # See if it is the given region and alignment
  xori $1, $1, (\region | (\alignment << 8))
  bne $1, $0, \patch_handler      # if not repatch/try again
  # There is a branch delay slot here
.endm

.macro region_check_open_align align_bits, alignment, patch_handler
  srl $1, $4, 24                  # check upper 8bits of address
  sltiu $2, $1, 0x0F              # true if it is a low address
  addiu $1, $1, -1                # non-zero if it is not a low open
  sltu $1, $0, $1                 # true if $1 != 0
  and $1, $1, $2                  # true if low address and not open
  andi $2, $4, ((1<<\align_bits)-1)
  xori $2, $2, \alignment         # true if alignment doesn't match
  or $1, $1, $2                   # align failure will trigger too
  bne $1, $0, \patch_handler      # if above is true, patch
  # There is a branch delay slot here
.endm


.macro ignore_region region, patch_handler
  region_check \region, \patch_handler
  nop                             # delay uselessly (BNE delay slot)
  jr $ra
  addiu $17, $17, -1              # waitstate cycles (delay)
.endm

.macro ignore_high patch_handler
  srl $1, $4, 24                  # check upper 8bits of address
  sltiu $1, $1, 0x0F              # see if it is not high
  bne $1, $0, \patch_handler      # if not repatch/try again
  nop                             # delay uselessly (branch delay slot)
  jr $ra
  addiu $17, $17, -1              # waitstate cycles (delay)
.endm


.macro translate_region_core base, size
  lui $2, %hi(\base)              # generate upper address
  andi $4, $4, \size              # generate offset
  addu $2, $2, $4                 # add ptr upper and offset
.endm

.macro translate_region region, patch_handler, base, size
  region_check \region, \patch_handler
  translate_region_core \base, \size # partially in BNE delay slot
.endm

# I refuse to have > 80 char lines, and GAS has a problem with the param
# list spilling over (grumble)

.macro translate_region_align region, a_b, alignment, p_h, base, size
  region_check_align \region, \a_b, \alignment, \p_h
  translate_region_core \base, \size # partially in BNE delay slot
.endm


.macro translate_region_ewram_core mask
  lui  $2, 3                      # size of EWRAM = 0x40000; load 3 in 31..16 (delay)
  ori  $1, $2, \mask              # now append the rest of the mask, 0x3XXXX
  and  $4, $4, $1
  lui  $2, %hi(ewram_data)        # generate upper address
  addu $2, $2, $4
.endm

.macro translate_region_ewram patch_handler
  region_check 2, \patch_handler
  translate_region_ewram_core 0xFFFF # partially in BNE delay slot
.endm

.macro translate_region_ewram_load_align align_bits, alignment, patch_handler
  region_check_align 2, \align_bits, \alignment, \patch_handler
  translate_region_ewram_core 0xFFFF # partially in BNE delay slot
.endm

.macro translate_region_ewram_load_align16 align_bits, alignment, patch_handler
  region_check_align 2, \align_bits, \alignment, \patch_handler
  translate_region_ewram_core 0xFFFE # partially in BNE delay slot
.endm

.macro translate_region_ewram_load_align32 align_bits, alignment, patch_handler
  region_check_align 2, \align_bits, \alignment, \patch_handler
  translate_region_ewram_core 0xFFFC # partially in BNE delay slot
.endm

.macro translate_region_ewram_store_align16 patch_handler
  region_check 2, \patch_handler
  translate_region_ewram_core 0xFFFE # partially in BNE delay slot
.endm

.macro translate_region_ewram_store_align32 patch_handler
  region_check 2, \patch_handler
  translate_region_ewram_core 0xFFFC # partially in BNE delay slot
.endm


.macro translate_region_vram_core
  beql $1, $0, 1f
  ori $2, $2, 0x8000              # if bit16 = 0, $2 = 0x1FFFF (delay Likely)
1:
  and $4, $4, $2                  # generate 17bit offset
  lui $1, %hi(vram)               # start loading vram address
  addu $2, $1, $4                 # $2 = (hi)vram + address
.endm

.macro translate_region_vram patch_handler
  region_check 6, \patch_handler
  lui $2, 1                       # $2 = 0x10000 (delay)
  and $1, $4, $2                  # $1 = bit16 of address
  ori $2, $2, 0x7FFF              # $2 = 0x17FFF
  translate_region_vram_core
.endm

.macro translate_region_vram_load_align align_bits, alignment, patch_handler
  region_check_align 6, \align_bits, \alignment, \patch_handler
  lui $2, 1
  and $1, $4, $2
  ori $2, $2, 0x7FFF
  translate_region_vram_core
.endm

.macro translate_region_vram_load_align16 align_bits, alignment, patch_handler
  region_check_align 6, \align_bits, \alignment, \patch_handler
  lui $2, 1
  and $1, $4, $2
  ori $2, $2, 0x7FFE
  translate_region_vram_core
.endm

.macro translate_region_vram_load_align32 align_bits, alignment, patch_handler
  region_check_align 6, \align_bits, \alignment, \patch_handler
  lui $2, 1
  and $1, $4, $2
  ori $2, $2, 0x7FFC
  translate_region_vram_core
.endm

.macro translate_region_vram_store_align16 patch_handler
  region_check 6, \patch_handler
  lui $2, 1
  and $1, $4, $2
  ori $2, $2, 0x7FFE
  translate_region_vram_core
.endm

.macro translate_region_vram_store_align32 patch_handler
  region_check 6, \patch_handler
  lui $2, 1
  and $1, $4, $2
  ori $2, $2, 0x7FFC
  translate_region_vram_core
.endm


.macro translate_region_gamepak_core mask
  srl $2, $4, 15                  # $2 = page number of address (delay)
  sll $2, $2, 2                   # adjust to word index
  addu $2, $2, $16                # $2 = memory_map_read[address >> 15]
  lw $2, -32768($2)
#ifndef MIPS_XBURST
  nop
#endif
  bne $2, $0, 1f                  # if it's non-NULL continue
  andi $1, $4, \mask              # $1 = low 15bits of address (delay slot)

  sw $ra, REG_SAVE2($16)          # save return address
  save_registers                  # save the registers

#ifdef MIPS_32R2
  ext $4, $4, 15, 10              # $4 = (address >> 15) & 0x3FF
#else
  srl $4, $4, 15
  andi $4, $4, 0x03FF
#endif
  jal load_gamepak_page           # get page in $2
  sw $1, REG_SAVE($16)            # save offset (delay)

  lw $1, REG_SAVE($16)            # restore offset
  lw $ra, REG_SAVE2($16)          # restore return address
  restore_registers               # restore the registers

1:
  addu $2, $2, $1                 # add the memory map offset
.endm

.macro translate_region_gamepak region, patch_handler
  region_check \region, \patch_handler
  translate_region_gamepak_core 0x7FFF # partially in BNE delay slot
.endm

.macro translate_region_gamepak_align region, a_b, alignment, patch_handler
  region_check_align \region, \a_b, \alignment, \patch_handler
  translate_region_gamepak_core 0x7FFF # partially in BNE delay slot
.endm

.macro translate_region_gamepak_align16 region, a_b, alignment, patch_handler
  region_check_align \region, \a_b, \alignment, \patch_handler
  translate_region_gamepak_core 0x7FFE # partially in BNE delay slot
.endm

.macro translate_region_gamepak_align32 region, a_b, alignment, patch_handler
  region_check_align \region, \a_b, \alignment, \patch_handler
  translate_region_gamepak_core 0x7FFC # partially in BNE delay slot
.endm


.macro translate_region_gamepak_a region, patch_handler
  region_check \region, \patch_handler
  srl $2, $4, 15                  # $2 = page number of address (delay)
  sll $2, $2, 2                   # adjust to word index
  addu $2, $2, $16                # $2 = memory_map_read[address >> 15]
  lw $2, -32768($2)
#ifndef MIPS_XBURST
  nop
#endif
  bne $2, $0, 1f                  # if it's non-NULL continue
  andi $1, $4, 0x7FFC             # $1 = low 15bits of address (delay slot)

  sw $ra, REG_SAVE2($16)          # save return address
  sw $6, REG_SAVE3($16)           # save a2
  save_registers                  # save the registers

#ifdef MIPS_32R2
  ext $4, $4, 15, 10              # $4 = (address >> 15) & 0x3FF
#else
  srl $4, $4, 15
  andi $4, $4, 0x03FF
#endif
  jal load_gamepak_page           # get page in $2
  sw $1, REG_SAVE($16)            # save offset (delay)

  lw $1, REG_SAVE($16)            # restore offset
  lw $ra, REG_SAVE2($16)          # restore return address
  lw $6, REG_SAVE3($16)           # restore a2
  restore_registers               # restore the registers

1:
  addu $2, $2, $1                 # add the memory map offset
.endm


.macro eeprom_load_core
  sw $ra, REG_SAVE($16)           # save the return address (delay)

  save_registers_0                # save the registers 1/2
  jal read_eeprom                 # get eeprom value in $2
  save_registers_1                # save the registers 2/2 (delay)

  lw $ra, REG_SAVE($16)           # restore return address
  restore_registers               # restore the registers

  jr $ra                          # return
  addiu $17, $17, -9              # waitstate cycles (delay)
.endm

.macro eeprom_load_align align_bits, alignment, patch_handler
  region_check_align 0xD, \align_bits, \alignment, \patch_handler
  eeprom_load_core # partially in BNE delay slot
.endm

.macro eeprom_load_align16 align_bits, alignment, patch_handler
  eeprom_load_align \align_bits, \alignment, \patch_handler
.endm


.macro backup_load_core
  save_registers                  # save the registers

  jal read_backup                 # get backup value in $2
  andi $4, $4, 0xFFFF             # address &= 0xFFFF (delay)

  lw $ra, REG_SAVE($16)           # restore return address
  restore_registers               # restore the registers

  jr $ra                          # return
  addiu $17, $17, -9              # waitstate cycles (delay)
.endm

.macro backup_load_a patch_handler
  region_check 0xE, \patch_handler

  sw $ra, REG_SAVE($16)           # save return address (delay)
  sw $6, REG_SAVE2($16)           # save a2
  save_registers                  # save the registers

  jal read_backup                 # get backup value in $2
  andi $4, $4, 0xFFFF             # address &= 0xFFFF (delay)

  lw $ra, REG_SAVE($16)           # restore return address
  lw $6, REG_SAVE2($16)           # restore a2
  restore_registers               # restore the registers

  jr $ra                          # return
  addiu $17, $17, -9              # waitstate cycles (delay)
.endm


.macro backup_load patch_handler
  region_check 0xE, \patch_handler
  sw $ra, REG_SAVE($16)           # save the return address (delay)
  backup_load_core
.endm

.macro backup_load_align align_bits, alignment, patch_handler
  region_check_align 0xE, \align_bits, \alignment, \patch_handler
  sw $ra, REG_SAVE($16)           # save the return address (delay)
  backup_load_core
.endm

.macro backup_load_align16 align_bits, alignment, patch_handler
  region_check_align 0xE, \align_bits, \alignment, \patch_handler
  sw $ra, REG_SAVE($16)           # save the return address (delay)
  andi $4, $4, 0xFFFE             # mask out lower bit
  backup_load_core
.endm

.macro backup_load_align32 align_bits, alignment, patch_handler
  region_check_align 0xE, \align_bits, \alignment, \patch_handler
  sw $ra, REG_SAVE($16)           # save the return address (delay)
  andi $4, $4, 0xFFFC             # mask out lower two bits
  backup_load_core
.endm


.macro open_load8_core
  lw $2, REG_CPSR($16)            # $2 = CPSR (delay)

  sw $ra, REG_SAVE($16)           # save the return address
  save_registers                  # save the registers

  andi $4, $4, 0x03               # in ARM mode, isolate lower 2bits from address
  andi $2, $2, 0x20               # test T bit

  bnel $2, $0, 1f                 # branch if Thumb mode
  andi $4, $4, 0x01               # in Thumb mode, isolate one more bit (delay Likely)

1:
  jal read_open_memory8           # get instruction at PC
  addu $4, $5, $4                 # a0 = PC + low bits of address (delay)

  lw $ra, REG_SAVE($16)           # restore return address
  restore_registers               # restore the registers

  jr $ra                          # return
  addiu $17, $17, -1              # waitstate cycles (delay)
.endm

.macro open_load8_seb_core
  lw $2, REG_CPSR($16)            # $2 = CPSR (delay)

  sw $ra, REG_SAVE($16)           # save the return address
  save_registers                  # save the registers

  andi $4, $4, 0x03               # in ARM mode, isolate lower 2bits from address
  andi $2, $2, 0x20               # test T bit

  bnel $2, $0, 1f                 # branch if Thumb mode
  andi $4, $4, 0x01               # in Thumb mode, isolate one more bit (delay Likely)

1:
  jal read_open_memory8           # get instruction at PC
  addu $4, $5, $4                 # a0 = PC + low bits of address (delay)

  lw $ra, REG_SAVE($16)           # restore return address
  restore_registers               # restore the registers

#ifdef MIPS_32R2
  seb $2, $2
#else
  sll $2, $2, 24
  sra $2, $2, 24
#endif

  jr $ra                          # return
  addiu $17, $17, -1              # waitstate cycles (delay)
.endm

.macro open_load8 patch_handler
  region_check_open \patch_handler
  open_load8_core # partially in BNE delay slot
.endm


.macro open_load16_core
  lw $2, REG_CPSR($16)            # $2 = CPSR (delay)

  sw $ra, REG_SAVE($16)           # save the return address
  save_registers                  # save the registers

  andi $4, $4, 0x02               # in ARM mode, isolate bit 1 from address
  andi $2, $2, 0x20               # test T bit

  bnel $2, $0, 1f                 # branch if Thumb mode
  addu $4, $0, $0                 # in Thumb mode, zero out address bit (delay Likely)

1:
  jal read_open_memory16          # get instruction at PC
  addu $4, $5, $4                 # a0 = PC + low bits of address (delay)

  lw $ra, REG_SAVE($16)           # restore return address
  restore_registers               # restore the registers

  jr $ra                          # return
  addiu $17, $17, -1              # waitstate cycles (delay)
.endm

.macro open_load16_seh_core
  lw $2, REG_CPSR($16)            # $2 = CPSR (delay)

  sw $ra, REG_SAVE($16)           # save the return address
  save_registers                  # save the registers

  andi $4, $4, 0x02               # in ARM mode, isolate bit 1 from address
  andi $2, $2, 0x20               # test T bit

  bnel $2, $0, 1f                 # branch if Thumb mode
  addu $4, $0, $0                 # in Thumb mode, zero out address bit (delay Likely)

1:
  jal read_open_memory16          # get instruction at PC
  addu $4, $5, $4                 # a0 = PC + low bits of address (delay)

  lw $ra, REG_SAVE($16)           # restore return address
  restore_registers               # restore the registers

#ifdef MIPS_32R2
  seh $2, $2
#else
  sll $2, $2, 16
  sra $2, $2, 16
#endif

  jr $ra                          # return
  addiu $17, $17, -1              # waitstate cycles (delay)
.endm

.macro open_load16_ror_core
  lw $2, REG_CPSR($16)            # $2 = CPSR (delay)

  sw $ra, REG_SAVE($16)           # save the return address
  save_registers                  # save the registers

  andi $4, $4, 0x02               # in ARM mode, isolate bit 1 from address
  andi $2, $2, 0x20               # test T bit

  bnel $2, $0, 1f                 # branch if Thumb mode
  addu $4, $0, $0                 # in Thumb mode, zero out address bit (delay Likely)

1:
  jal read_open_memory16          # get instruction at PC
  addu $4, $5, $4                 # a0 = PC + low bits of address (delay)

  lw $ra, REG_SAVE($16)           # restore return address
  restore_registers               # restore the registers

#ifdef MIPS_32R2
  ror   $2, $2, 8                 # rotate value by 8 bits
#else
  sll   $1, $2, 24
  srl   $2, $2, 8
  or    $2, $2, $1                # rotate value by 8 bits
#endif

  jr $ra                          # return
  addiu $17, $17, -1              # waitstate cycles (delay)
.endm

.macro open_load16_align align_bits, alignment, patch_handler
  region_check_open_align \align_bits, \alignment, \patch_handler
  open_load16_core # partially in BNE delay slot
.endm

.macro open_load16_align16 align_bits, alignment, patch_handler
  region_check_open_align \align_bits, \alignment, \patch_handler
  open_load16_ror_core # partially in BNE delay slot
.endm


.macro open_load32_core
  lw $2, REG_CPSR($16)            # $2 = CPSR (delay)

  save_registers                  # save the registers

  andi $2, $2, 0x20               # test T bit

  beq $2, $0, 1f                  # branch if ARM mode
  sw $ra, REG_SAVE($16)           # save the return address (delay)

  jal read_open_memory16          # get instruction at PC
  addu $4, $5, $0                 # a0 = PC (delay)

#ifdef MIPS_32R2
  beq   $0, $0, 2f                # (short unconditional jump)
  ins   $2, $2, 16, 16            # result = (result << 16) | result (delay)
#else
  sll   $2, $2, 16
  srl   $1, $2, 16
  beq   $0, $0, 2f                # (short unconditional jump)
  or    $2, $1, $2                # result = (result << 16) | result (delay)
#endif

1:
  jal read_open_memory32          # get instruction at PC
  addu $4, $5, $0                 # a0 = PC (delay)

2:                                # join point
  lw $ra, REG_SAVE($16)           # restore return address
  restore_registers               # restore the registers

  jr $ra                          # return
  addiu $17, $17, -1              # waitstate cycles (delay)
.endm

.macro open_load32_ror_core alignment
  lw $2, REG_CPSR($16)            # $2 = CPSR (delay)

  save_registers                  # save the registers

  andi $2, $2, 0x20               # test T bit

  beq $2, $0, 1f                  # branch if ARM mode
  sw $ra, REG_SAVE($16)           # save the return address (delay)

  jal read_open_memory16          # get instruction at PC
  addu $4, $5, $0                 # a0 = PC (delay)

#ifdef MIPS_32R2
  beq   $0, $0, 2f                # (short unconditional jump)
  ins   $2, $2, 16, 16            # result = (result << 16) | result (delay)
#else
  sll   $2, $2, 16
  srl   $1, $2, 16
  beq   $0, $0, 2f                # (short unconditional jump)
  or    $2, $1, $2                # result = (result << 16) | result (delay)
#endif

1:
  jal read_open_memory32          # get instruction at PC
  addu $4, $5, $0                 # a0 = PC (delay)

2:                                # join point
  lw $ra, REG_SAVE($16)           # restore return address
  restore_registers               # restore the registers

#ifdef MIPS_32R2
  ror   $2, $2, (\alignment * 8)  # rotate value by 8 bits
#else
  sll   $1, $2, (32-(\alignment * 8))
  srl   $2, $2, (\alignment * 8) 
  or    $2, $2, $1                # rotate value by 8 bits
#endif

  jr $ra                          # return
  addiu $17, $17, -1              # waitstate cycles (delay)
.endm

.macro open_load32_a patch_handler
  region_check_open \patch_handler

  lw $2, REG_CPSR($16)            # $2 = CPSR (delay)

  save_registers                  # save the registers
  sw $6, REG_SAVE2($16)           # save a2

  andi $2, $2, 0x20               # test T bit

  beq $2, $0, 1f                  # branch if ARM mode
  sw $ra, REG_SAVE($16)           # save the return address (delay)

  jal read_open_memory16          # get instruction at PC
  addu $4, $5, $0                 # a0 = PC (delay)

#ifdef MIPS_32R2
  beq   $0, $0, 2f                # (short unconditional jump)
  ins   $2, $2, 16, 16            # result = (result << 16) | result (delay)
#else
  sll   $2, $2, 16
  srl   $1, $2, 16
  beq   $0, $0, 2f                # (short unconditional jump)
  or    $2, $1, $2                # result = (result << 16) | result (delay)
#endif

1:
  jal read_open_memory32          # get instruction at PC
  addu $4, $5, $0                 # a0 = PC (delay)

2:
  lw $ra, REG_SAVE($16)           # restore return address
  lw $6, REG_SAVE2($16)           # restore a2
  restore_registers               # restore the registers

  jr $ra                          # return
  addiu $17, $17, -1              # waitstate cycles (delay)
.endm

.macro open_load32_align align_bits, alignment, patch_handler
  region_check_open_align \align_bits, \alignment, \patch_handler
  open_load32_core # partially in BNE delay slot
.endm

.macro open_load32_align32 align_bits, alignment, patch_handler
  region_check_open_align \align_bits, \alignment, \patch_handler
  open_load32_ror_core \alignment # partially in BNE delay slot
.endm


.macro store_function function, region, patch_handler, mask, waitstate
  region_check \region, \patch_handler
  sw $ra, REG_SAVE($16)           # save the return address (delay)
  sw $6, REG_PC($16)              # next PC = $6
  save_registers                  # save the registers

  jal \function                   # store value out
  andi $4, $4, \mask              # mask address (delay)

  addiu $17, $17, \waitstate      # waitstate cycles

  bgtz $17, 1f                    # goto label if $17 > 0
  sw $0, CHANGED_PC_STATUS($16)

  lw $2, REG_CPSR($16)            # load CPSR
  collapse_flags_body             # insert flag into $2

  jal update_gba                  # process the next event
  sw $2, REG_CPSR($16)            # store CPSR (delay)

  lw $1, CHANGED_PC_STATUS($16)
#ifndef MIPS_XBURST
  nop
#endif

  bne $1, $0, lookup_pc_changed
  addu $17, $2, $0                # $17 = new cycle count (delay slot)

1:
  lw $ra, REG_SAVE($16)           # restore return address
  restore_registers_0             # restore the registers 1/2
  jr $ra                          # return
  restore_registers_1             # restore the registers 2/2 (delay)
.endm

.macro store_function_a function, region, patch_handler, mask, waitstate
  region_check \region, \patch_handler
  sw $ra, REG_SAVE($16)           # save the return address (delay)
  sw $6, REG_SAVE2($16)           # save a2
  save_registers                  # save the registers

  jal \function                   # store value out
  andi $4, $4, \mask              # mask address (delay)

  lw $ra, REG_SAVE($16)           # restore return address
  lw $6, REG_SAVE2($16)           # restore a2
  restore_registers               # restore the registers

  jr $ra                          # return
  addiu $17, $17, \waitstate      # waitstate cycles (delay)
.endm


.macro load_u8 base, waitstate
  lbu $2, %lo(\base)($2)          # return base[offset]
  jr $ra                          # return
  addiu $17, $17, \waitstate      # waitstate cycles (delay)
.endm

.macro load_s8 base, waitstate
  lb $2, %lo(\base)($2)           # return base[offset]
  jr $ra                          # return
  addiu $17, $17, \waitstate      # waitstate cycles (delay)
.endm

.macro load_u16 base, waitstate
  lhu $2, %lo(\base)($2)          # return base[offset]
  jr $ra                          # return
  addiu $17, $17, \waitstate      # waitstate cycles (delay)
.endm

.macro load_s16 base, waitstate
  lh $2, %lo(\base)($2)           # return base[offset]
  jr $ra                          # return
  addiu $17, $17, \waitstate      # waitstate cycles (delay)
.endm

.macro load_u32 base, waitstate
  lw $2, %lo(\base)($2)           # return base[offset]
  jr $ra                          # return
  addiu $17, $17, \waitstate      # waitstate cycles (delay)
.endm


# 16bit unaligned load will always have a 1 in the LSB;
# should have already been taken care of in indexing.

.macro load_u16_unaligned base, waitstate
  lhu $2, %lo(\base)($2)          # load base[offset]
  addiu $17, $17, \waitstate      # waitstate cycles
#ifdef MIPS_32R2
  jr    $ra                       # return
  ror   $2, $2, 8                 # rotate value by 8 bits (delay)
#else
  sll   $1, $2, 24
  srl   $2, $2, 8
  jr    $ra                       # return
  or    $2, $2, $1                # rotate value by 8 bits (delay)
#endif
.endm

# This is technically the same as load_s8, but kept to
# avoid confusion.

.macro load_s16_unaligned base, waitstate
  lb $2, %lo(\base)($2)           # return base[offset]
  jr $ra                          # return
  addiu $17, $17, \waitstate      # waitstate cycles (delay)
.endm

# Unalignment must be known statically (use the tables to
# patch correctly)

.macro load_u32_unaligned base, alignment, waitstate
  lw $2, %lo(\base)($2)           # load base[offset]
  addiu $17, $17, \waitstate      # waitstate cycles
#ifdef MIPS_32R2
  jr    $ra                       # return
  ror   $2, $2, (\alignment * 8)  # rotate value by 8 bits (delay)
#else
  sll   $1, $2, (32-(\alignment * 8))
  srl   $2, $2, (\alignment * 8) 
  jr    $ra                       # return
  or    $2, $2, $1                # rotate value by 8 bits (delay)
#endif
.endm


.macro store_u8 base, waitstate
  sb $5, %lo(\base)($2)           # store value at base[offset]
  addiu $17, $17, \waitstate      # waitstate cycles

  blez $17, store_update_gba
  sw $0, CHANGED_PC_STATUS($16)

  jr $ra                          # return
  nop
.endm

.macro store_u16 base, waitstate
  sh $5, %lo(\base)($2)           # store value at base[offset]
  addiu $17, $17, \waitstate      # waitstate cycles

  blez $17, store_update_gba
  sw $0, CHANGED_PC_STATUS($16)

  jr $ra                          # return
  nop
.endm

.macro store_u32 base, waitstate
  sw $5, %lo(\base)($2)           # store value at base[offset]
  addiu $17, $17, \waitstate      # waitstate cycles

  blez $17, store_update_gba
  sw $0, CHANGED_PC_STATUS($16)

  jr $ra                          # return
  nop
.endm

.macro store_u32a base, waitstate
  sw $5, %lo(\base)($2)           # store value at base[offset]
  jr $ra                          # return
  addiu $17, $17, \waitstate      # waitstate cycles (delay)
.endm


# Store the value double mirrored (u16)

.macro store_u8_double base
#ifdef MIPS_32R2
  ins   $5, $5, 8, 8              # value = (value << 8) | value
#else
  andi  $5, $5, 0x00FF
  sll   $1, $5, 8
  or    $5, $1, $5
#endif
  sh    $5, %lo(\base)($2)        # store value at base[offset]
.endm


store_update_gba:
  lw $2, REG_CPSR($16)            # load CPSR
  sw $6, REG_PC($16)              # next PC = $6

  addiu $sp, $sp, -4 * _SA        # make room on the stack
  sw $ra, ($sp)                   # save return address

  save_registers                  # save the registers
  collapse_flags_body             # insert flag into $2

  jal update_gba                  # process the next event
  sw $2, REG_CPSR($16)            # store CPSR (delay)

  lw $1, CHANGED_PC_STATUS($16)

  lw $ra, ($sp)                   # restore return address
  addiu $sp, $sp, 4 * _SA         # fix stack

  bne $1, $0, lookup_pc_changed
  addu $17, $2, $0                # $17 = new cycle count (delay slot)

  restore_registers_0             # restore the registers 1/2
  jr $ra
  restore_registers_1             # restore the registers 2/2 (delay)


# Store the values and check if it overwrote code there, by way of a Metadata
# Area. A Metadata Area has 4 half-words for each word of the Data Area it
# describes, so data[0] is at metadata[0] and data[4] is at metadata[8] and so
# on, when the offsets are taken as bytes. The entries all have [3]'s bit 0
# stating whether the Data Word is a code word: 1 if it is, 0 if it isn't.
# [3] is at byte offset 6 because this is an assembly file.

# Register assignment:
#   $1 available
#   $2 available
#   $4 (invariant) = Data Area offset
#   $5 (outgoing)  = memory region
#   $6 (invariant) = PC
# See smc_write for its register assignment.
.macro post_write_metadata_core base, metabase, memory_region
  srl $1, $4, 2                   # $1 = (address & ~3) [>> 2]
  sll $1, $1, 3                   # byte offset into a 16-bit array: * 2
  la $2, \metabase                # load the Metadata Area's address
  addu $2, $2, $1                 # $2 = &metabase[offset] (u16)

  lhu $1, 6($2)                   # load the code modification status
#ifndef MIPS_XBURST
  nop
#endif
  andi $1, $1, 0x3                # and extract the Currently Code bits

  bne $1, $0, smc_write           # if there has been code there, go flush RAM
  addiu $5, $0, \memory_region    # $5 = memory region (delay)

  blez $17, store_update_gba
  sw $0, CHANGED_PC_STATUS($16)

  jr $ra                          # if there has not been code there, return
  nop                             # cannot usefully delay here
.endm

post_write_metadata_ewram:
  post_write_metadata_core ewram_data, ewram_metadata, 0x02

post_write_metadata_iwram:
  post_write_metadata_core iwram_data, iwram_metadata, 0x03

post_write_metadata_vram:
  post_write_metadata_core vram, vram_metadata, 0x06

.macro store_u8_metadata base, waitstate, post_function
  sb $5, %lo(\base)($2)           # store value at base[offset]
  j \post_function
  addiu $17, $17, \waitstate      # waitstate cycles (delay)
.endm

.macro store_u16_metadata base, waitstate, post_function
  sh $5, %lo(\base)($2)           # store value at base[offset]
  j \post_function
  addiu $17, $17, \waitstate      # waitstate cycles (delay)
.endm

.macro store_u32_metadata base, waitstate, post_function
  sw $5, %lo(\base)($2)           # store value at base[offset]
  j \post_function
  addiu $17, $17, \waitstate      # waitstate cycles (delay)
.endm


# Unsigned 8bit load handlers

execute_load_bios_u8:
  region_check 0, patch_load_u8
  srl $2, $4, 14                  # check if address is in BIOS region (delay)
  bne $2, $0, 2f                  # if not, perform open read
  srl $1, $5, 14                  # check if PC is in BIOS region (delay)
  bne $1, $0, 1f                  # if not, perform BIOS protected read

  lui $2, %hi(bios)               # generate upper address (delay)
  andi $4, $4, 0x3FFF             # generate offset
  addu $2, $2, $4
  load_u8 bios, -1

1:
  lui $2, %hi(bios_read_protect)  # generate upper address
#ifdef MIPS_32R2
  ins   $2, $4, 0, 2              # lower 2 bits address contributes
#else
  andi  $1, $4, 3
  srl   $2, $2, 2
  sll   $2, $2, 2                 # clear out the lower 2 bits of $2 first
  or    $2, $2, $1                # and add the lower 2 bits of $1
#endif
  load_u8 bios_read_protect, -1

2:
  open_load8_core


execute_load_ewram_u8:
  translate_region_ewram patch_load_u8
  load_u8 ewram_data, -3

# Put the generic address over the handler you want to be default
# IWRAM is typically the most frequently read and written to.

execute_load_u8:
execute_load_iwram_u8:
  translate_region 3, patch_load_u8, iwram_data, 0x7FFF
  load_u8 iwram_data, -1

execute_load_io_u8:
  region_check 4, patch_load_u8

  srl $2, $4, 10
  andi $1, $2, 0x3FFF             # $1 = (address >> 10) & 0x3FFF
  bne $1, $0, 1f

  lui $2, %hi(io_registers)       # generate upper address (delay)
  andi $4, $4, 0x3FF              # generate offset
  addu $2, $2, $4
  load_u8 io_registers, -1

1:
  open_load8_core

execute_load_palette_u8:
  translate_region 5, patch_load_u8, palette_ram, 0x3FF
  load_u8 palette_ram, -1

execute_load_vram_u8:
  translate_region_vram patch_load_u8
  load_u8 vram, -1

execute_load_oam_u8:
  translate_region 7, patch_load_u8, oam_ram, 0x3FF
  load_u8 oam_ram, -1

execute_load_gamepak8_u8:
  translate_region_gamepak 8, patch_load_u8
  load_u8 0, -4

execute_load_gamepak9_u8:
  translate_region_gamepak 9, patch_load_u8
  load_u8 0, -4

execute_load_gamepakA_u8:
  translate_region_gamepak 10, patch_load_u8
  load_u8 0, -4

execute_load_gamepakB_u8:
  translate_region_gamepak 11, patch_load_u8
  load_u8 0, -4

execute_load_gamepakC_u8:
  translate_region_gamepak 12, patch_load_u8
  load_u8 0, -4

execute_load_gamepakD_u8:
  translate_region_gamepak 13, patch_load_u8
  load_u8 0, -4

execute_load_backup_u8:
  backup_load patch_load_u8

execute_load_open_u8:
  open_load8 patch_load_u8

load_u8_ftable:
  .long execute_load_bios_u8      # 0x00 BIOS
  .long execute_load_open_u8      # 0x01 open address
  .long execute_load_ewram_u8     # 0x02 EWRAM
  .long execute_load_iwram_u8     # 0x03 IWRAM
  .long execute_load_io_u8        # 0x04 I/O registers
  .long execute_load_palette_u8   # 0x05 Palette RAM
  .long execute_load_vram_u8      # 0x06 VRAM
  .long execute_load_oam_u8       # 0x07 OAM RAM
  .long execute_load_gamepak8_u8  # 0x08 gamepak
  .long execute_load_gamepak9_u8  # 0x09 gamepak
  .long execute_load_gamepakA_u8  # 0x0A gamepak
  .long execute_load_gamepakB_u8  # 0x0B gamepak
  .long execute_load_gamepakC_u8  # 0x0C gamepak
  .long execute_load_gamepakD_u8  # 0x0D gamepak
  .long execute_load_backup_u8    # 0x0E Flash ROM/SRAM
  .long execute_load_open_u8      # 0x0F open address

patch_load_u8:
  patch_handler load_u8_ftable, 0x01



# Signed 8bit load handlers

execute_load_bios_s8:
  region_check 0, patch_load_s8
  srl $2, $4, 14                  # check if address is in BIOS region (delay)
  bne $2, $0, 2f
  srl $1, $5, 14                  # check if PC is in BIOS region (delay)
  bne $1, $0, 1f                  # if not, perform BIOS protected read

  lui $2, %hi(bios)               # generate upper address (delay)
  andi $4, $4, 0x3FFF             # generate offset
  addu $2, $2, $4
  load_s8 bios, -1

1:
  lui $2, %hi(bios_read_protect)  # generate upper address
#ifdef MIPS_32R2
  ins   $2, $4, 0, 2              # lower 2 bits address contributes
#else
  andi  $1, $4, 3
  srl   $2, $2, 2
  sll   $2, $2, 2                 # clear out the lower 2 bits of $2 first
  or    $2, $2, $1                # and add the lower 2 bits of $1
#endif
  load_s8 bios_read_protect, -1

2:
  open_load8_seb_core


execute_load_ewram_s8:
  translate_region_ewram patch_load_s8
  load_s8 ewram_data, -3

execute_load_s8:
execute_load_iwram_s8:
  translate_region 3, patch_load_s8, iwram_data, 0x7FFF
  load_s8 iwram_data, -1

execute_load_io_s8:
  region_check 4, patch_load_s8

  srl $2, $4, 10
  andi $1, $2, 0x3FFF             # $1 = (address >> 10) & 0x3FFF
  bne $1, $0, 1f

  lui $2, %hi(io_registers)       # generate upper address (delay)
  andi $4, $4, 0x3FF              # generate offset
  addu $2, $2, $4
  load_s8 io_registers, -1

1:
  open_load8_seb_core

execute_load_palette_s8:
  translate_region 5, patch_load_s8, palette_ram, 0x3FF
  load_s8 palette_ram, -1

execute_load_vram_s8:
  translate_region_vram patch_load_s8
  load_s8 vram, -1

execute_load_oam_s8:
  translate_region 7, patch_load_s8, oam_ram, 0x3FF
  load_s8 oam_ram, -1

execute_load_gamepak8_s8:
  translate_region_gamepak 8, patch_load_s8
  load_s8 0, -4

execute_load_gamepak9_s8:
  translate_region_gamepak 9, patch_load_s8
  load_s8 0, -4

execute_load_gamepakA_s8:
  translate_region_gamepak 10, patch_load_s8
  load_s8 0, -4

execute_load_gamepakB_s8:
  translate_region_gamepak 11, patch_load_s8
  load_s8 0, -4

execute_load_gamepakC_s8:
  translate_region_gamepak 12, patch_load_s8
  load_s8 0, -4

execute_load_gamepakD_s8:
  translate_region_gamepak 13, patch_load_s8
  load_s8 0, -4

execute_load_backup_s8:
  backup_load patch_load_s8

execute_load_open_s8:
  region_check_open patch_load_s8
  open_load8_seb_core

load_s8_ftable:
  .long execute_load_bios_s8      # 0x00 BIOS
  .long execute_load_open_s8      # 0x01 open address
  .long execute_load_ewram_s8     # 0x02 EWRAM
  .long execute_load_iwram_s8     # 0x03 IWRAM
  .long execute_load_io_s8        # 0x04 I/O registers
  .long execute_load_palette_s8   # 0x05 Palette RAM
  .long execute_load_vram_s8      # 0x06 VRAM
  .long execute_load_oam_s8       # 0x07 OAM RAM
  .long execute_load_gamepak8_s8  # 0x08 gamepak
  .long execute_load_gamepak9_s8  # 0x09 gamepak
  .long execute_load_gamepakA_s8  # 0x0A gamepak
  .long execute_load_gamepakB_s8  # 0x0B gamepak
  .long execute_load_gamepakC_s8  # 0x0C gamepak
  .long execute_load_gamepakD_s8  # 0x0D gamepak
  .long execute_load_backup_s8    # 0x0E Flash ROM/SRAM
  .long execute_load_open_s8      # 0x0F open address

patch_load_s8:
  patch_handler load_s8_ftable, 1



# Unsigned aligned 16bit load handlers

execute_load_bios_u16:
  region_check_align 0, 1, 0, patch_load_u16
  srl $2, $4, 14                  # check if address is in BIOS region (delay)
  bne $2, $0, 2f                  # if not, perform open read
  srl $1, $5, 14                  # check if PC is in BIOS region (delay)
  bne $1, $0, 1f                  # if not, perform BIOS protected read

  lui $2, %hi(bios)               # generate upper address (delay)
  andi $4, $4, 0x3FFF             # generate offset
  addu $2, $2, $4
  load_u16 bios, -1

1:
  lui $2, %hi(bios_read_protect)  # generate upper address
#ifdef MIPS_32R2
  ins   $2, $4, 0, 2              # bit 1 contributes, and bit 0 is unset
#else
  andi  $1, $4, 3
  srl   $2, $2, 2
  sll   $2, $2, 2                 # clear out the lower 2 bits of $2 first
  or    $2, $2, $1                # and add the lower 2 bits of $1
#endif
  load_u16 bios_read_protect, -1

2:
  open_load16_core

execute_load_ewram_u16:
  translate_region_ewram_load_align 1, 0, patch_load_u16
  load_u16 ewram_data, -3

execute_load_u16:
execute_load_iwram_u16:
  translate_region_align 3, 1, 0, patch_load_u16, iwram_data, 0x7FFF
  load_u16 iwram_data, -1

execute_load_io_u16:
  region_check_align 4, 1, 0, patch_load_u16

  srl $2, $4, 10
  andi $1, $2, 0x3FFF             # $1 = (address >> 10) & 0x3FFF
  bne $1, $0, 1f

  lui $2, %hi(io_registers)       # generate upper address (delay)
  andi $4, $4, 0x3FF              # generate offset
  addu $2, $2, $4
  load_u16 io_registers, -1

1:
  open_load16_core

execute_load_palette_u16:
  translate_region_align 5, 1, 0, patch_load_u16, palette_ram, 0x3FF
  load_u16 palette_ram, -1

execute_load_vram_u16:
  translate_region_vram_load_align 1, 0, patch_load_u16
  load_u16 vram, -1

execute_load_oam_u16:
  translate_region_align 7, 1, 0, patch_load_u16, oam_ram, 0x3FF
  load_u16 oam_ram, -1

execute_load_gamepak8_u16:
  translate_region_gamepak_align 8, 1, 0, patch_load_u16
  load_u16 0, -4

execute_load_gamepak9_u16:
  translate_region_gamepak_align 9, 1, 0, patch_load_u16
  load_u16 0, -4

execute_load_gamepakA_u16:
  translate_region_gamepak_align 10, 1, 0, patch_load_u16
  load_u16 0, -4

execute_load_gamepakB_u16:
  translate_region_gamepak_align 11, 1, 0, patch_load_u16
  load_u16 0, -4

execute_load_gamepakC_u16:
  translate_region_gamepak_align 12, 1, 0, patch_load_u16
  load_u16 0, -4

execute_load_eeprom_u16:
  eeprom_load_align 1, 0, patch_load_u16

execute_load_backup_u16:
  backup_load_align 1, 0, patch_load_u16

execute_load_open_u16:
  open_load16_align 1, 0, patch_load_u16


# Unsigned unaligned 16bit load handlers

execute_load_bios_u16u:
  region_check_align 0, 1, 1, patch_load_u16
  srl $2, $4, 14                  # check if address is in BIOS region (delay)
  bne $2, $0, 2f                  # if not, perform open read
  srl $1, $5, 14                  # check if PC is in BIOS region (delay)
  bne $1, $0, 1f                  # if not, perform BIOS protected read

  lui $2, %hi(bios)               # generate upper address (delay)
  andi $4, $4, 0x3FFE             # generate offset
  addu $2, $2, $4
  load_u16_unaligned bios, -1

1:
  lui $2, %hi(bios_read_protect)  # generate upper address
#ifdef MIPS_32R2
  ext $1, $4, 1, 1
  ins $2, $1, 1, 1                # bit 1 contributes
#else
  andi $1, $4, 2
  srl  $2, $2, 2
  sll  $2, $2, 2
  or   $2, $2, $1                 # bit 0 is unset (ANDI 0x3FFE earlier)
#endif
  load_u16_unaligned bios_read_protect, -1

2:
  open_load16_ror_core


execute_load_ewram_u16u:
  translate_region_ewram_load_align16 1, 1, patch_load_u16
  load_u16_unaligned ewram_data, -3

execute_load_iwram_u16u:
  translate_region_align 3, 1, 1, patch_load_u16, iwram_data, 0x7FFE
  load_u16_unaligned iwram_data, -1

execute_load_io_u16u:
  region_check_align 4, 1, 1, patch_load_u16

  srl $2, $4, 10
  andi $1, $2, 0x3FFF             # $1 = (address >> 10) & 0x3FFF
  bne $1, $0, 1f

  lui $2, %hi(io_registers)       # generate upper address (delay)
  andi $4, $4, 0x3FE              # generate offset
  addu $2, $2, $4
  load_u16_unaligned io_registers, -1

1:
  open_load16_ror_core

execute_load_palette_u16u:
  translate_region_align 5, 1, 1, patch_load_u16, palette_ram, 0x3FE
  load_u16_unaligned palette_ram, -1

execute_load_vram_u16u:
  translate_region_vram_load_align16 1, 1, patch_load_u16
  load_u16_unaligned vram, -1

execute_load_oam_u16u:
  translate_region_align 7, 1, 1, patch_load_u16, oam_ram, 0x3FE
  load_u16_unaligned oam_ram, -1

execute_load_gamepak8_u16u:
  translate_region_gamepak_align16 8, 1, 1, patch_load_u16
  load_u16_unaligned 0, -4

execute_load_gamepak9_u16u:
  translate_region_gamepak_align16 9, 1, 1, patch_load_u16
  load_u16_unaligned 0, -4

execute_load_gamepakA_u16u:
  translate_region_gamepak_align16 10, 1, 1, patch_load_u16
  load_u16_unaligned 0, -4

execute_load_gamepakB_u16u:
  translate_region_gamepak_align16 11, 1, 1, patch_load_u16
  load_u16_unaligned 0, -4

execute_load_gamepakC_u16u:
  translate_region_gamepak_align16 12, 1, 1, patch_load_u16
  load_u16_unaligned 0, -4

execute_load_eeprom_u16u:
  eeprom_load_align16 1, 1, patch_load_u16

execute_load_backup_u16u:
  backup_load_align16 1, 1, patch_load_u16

execute_load_open_u16u:
  open_load16_align16 1, 1, patch_load_u16

load_u16_ftable:
  .long execute_load_bios_u16     # 0x00 BIOS
  .long execute_load_open_u16     # 0x01 open address
  .long execute_load_ewram_u16    # 0x02 EWRAM
  .long execute_load_iwram_u16    # 0x03 IWRAM
  .long execute_load_io_u16       # 0x04 I/O registers
  .long execute_load_palette_u16  # 0x05 Palette RAM
  .long execute_load_vram_u16     # 0x06 VRAM
  .long execute_load_oam_u16      # 0x07 OAM RAM
  .long execute_load_gamepak8_u16 # 0x08 gamepak
  .long execute_load_gamepak9_u16 # 0x09 gamepak
  .long execute_load_gamepakA_u16 # 0x0A gamepak
  .long execute_load_gamepakB_u16 # 0x0B gamepak
  .long execute_load_gamepakC_u16 # 0x0C gamepak
  .long execute_load_eeprom_u16   # 0x0D gamepak/eeprom
  .long execute_load_backup_u16   # 0x0E Flash ROM/SRAM
  .long execute_load_open_u16     # 0x0F open

  .long execute_load_bios_u16u    # 0x00 BIOS unaligned
  .long execute_load_open_u16u    # 0x01 open address unaligned
  .long execute_load_ewram_u16u   # 0x02 EWRAM unaligned
  .long execute_load_iwram_u16u   # 0x03 IWRAM unaligned
  .long execute_load_io_u16u      # 0x04 I/O registers unaligned
  .long execute_load_palette_u16u # 0x05 Palette RAM unaligned
  .long execute_load_vram_u16u    # 0x06 VRAM unaligned
  .long execute_load_oam_u16u     # 0x07 OAM RAM unaligned
  .long execute_load_gamepak8_u16u# 0x08 gamepak unaligned
  .long execute_load_gamepak9_u16u# 0x09 gamepak unaligned
  .long execute_load_gamepakA_u16u# 0x0A gamepak unaligned
  .long execute_load_gamepakB_u16u# 0x0B gamepak unaligned
  .long execute_load_gamepakC_u16u# 0x0C gamepak unaligned
  .long execute_load_eeprom_u16u  # 0x0D gamepak/eeprom unaligned
  .long execute_load_backup_u16u  # 0x0E Flash ROM/SRAM unaligned
  .long execute_load_open_u16u    # 0x0F open unaligned


patch_load_u16:
  patch_handler_align load_u16_ftable, 1

# Signed aligned 16bit load handlers

execute_load_bios_s16:
  region_check_align 0, 1, 0, patch_load_s16
  srl $2, $4, 14                  # check if address is in BIOS region (delay)
  bne $2, $0, 2f                  # if not, perform open read
  srl $1, $5, 14                  # check if PC is in BIOS region (delay)
  bne $1, $0, 1f                  # if not, perform BIOS protected read

  lui $2, %hi(bios)               # generate upper address (delay)
  andi $4, $4, 0x3FFF             # generate offset
  addu $2, $2, $4
  load_s16 bios, -1

1:
  lui $2, %hi(bios_read_protect)  # generate upper address
#ifdef MIPS_32R2
  ins   $2, $4, 0, 2              # bit 1 contributes, and bit 0 is unset
#else
  andi  $1, $4, 3
  srl   $2, $2, 2
  sll   $2, $2, 2                 # clear out the lower 2 bits of $2 first
  or    $2, $2, $1                # and add the lower 2 bits of $1
#endif
  load_s16 bios_read_protect, -1

2:
  open_load16_seh_core

execute_load_ewram_s16:
  translate_region_ewram_load_align16 1, 0, patch_load_s16
  load_s16 ewram_data, -3

execute_load_s16:
execute_load_iwram_s16:
  translate_region_align 3, 1, 0, patch_load_s16, iwram_data, 0x7FFe
  load_s16 iwram_data, -1

execute_load_io_s16:
  region_check_align 4, 1, 0, patch_load_s16

  srl $2, $4, 10
  andi $1, $2, 0x3FFF             # $1 = (address >> 10) & 0x3FFF
  bne $1, $0, 1f

  lui $2, %hi(io_registers)       # generate upper address (delay)
  andi $4, $4, 0x3FF              # generate offset
  addu $2, $2, $4
  load_s16 io_registers, -1

1:
  open_load16_seh_core

execute_load_palette_s16:
  translate_region_align 5, 1, 0, patch_load_s16, palette_ram, 0x3Fe
  load_s16 palette_ram, -1

execute_load_vram_s16:
  translate_region_vram_load_align16 1, 0, patch_load_s16
  load_s16 vram, -1

execute_load_oam_s16:
  translate_region_align 7, 1, 0, patch_load_s16, oam_ram, 0x3Fe
  load_s16 oam_ram, -1

execute_load_gamepak8_s16:
  translate_region_gamepak_align16 8, 1, 0, patch_load_s16
  load_s16 0, -4

execute_load_gamepak9_s16:
  translate_region_gamepak_align16 9, 1, 0, patch_load_s16
  load_s16 0, -4

execute_load_gamepakA_s16:
  translate_region_gamepak_align16 10, 1, 0, patch_load_s16
  load_s16 0, -4

execute_load_gamepakB_s16:
  translate_region_gamepak_align16 11, 1, 0, patch_load_s16
  load_s16 0, -4

execute_load_gamepakC_s16:
  translate_region_gamepak_align16 12, 1, 0, patch_load_s16
  load_s16 0, -4

execute_load_eeprom_s16:
  eeprom_load_align 1, 0, patch_load_s16

execute_load_backup_s16:
  backup_load_align 1, 0, patch_load_s16

execute_load_open_s16:
  region_check_open_align 1, 0, patch_load_s16
  open_load16_seh_core


# Signed unaligned 16bit load handlers

execute_load_bios_s16u:
  region_check_align 0, 1, 1, patch_load_s16
  srl $2, $4, 14                  # check if address is in BIOS region (delay)
  bne $2, $0, 2f                  # if not, perform open read
  srl $1, $5, 14                  # check if PC is in BIOS region (delay)
  bne $1, $0, 1f                  # if not, perform BIOS protected read

  lui $2, %hi(bios)               # generate upper address (delay)
  andi $4, $4, 0x3FFF             # generate offset
  addu $2, $2, $4
  load_s16_unaligned bios, -1

1:
  lui $2, %hi(bios_read_protect)  # generate upper address
#ifdef MIPS_32R2
  ins   $2, $4, 0, 2              # lower 2 bits address contributes
#else
  andi  $1, $4, 3
  srl   $2, $2, 2
  sll   $2, $2, 2                 # clear out the lower 2 bits of $2 first
  or    $2, $2, $1                # and add the lower 2 bits of $1
#endif
  load_s16_unaligned bios_read_protect, -1

2:
  open_load8_seb_core

execute_load_ewram_s16u:
  translate_region_ewram_load_align 1, 1, patch_load_s16
  load_s16_unaligned ewram_data, -3

execute_load_iwram_s16u:
  translate_region_align 3, 1, 1, patch_load_s16, iwram_data, 0x7FFF
  load_s16_unaligned iwram_data, -1

execute_load_io_s16u:
  region_check_align 4, 1, 1, patch_load_s16

  srl $2, $4, 10
  andi $1, $2, 0x3FFF             # $1 = (address >> 10) & 0x3FFF
  bne $1, $0, 1f

  lui $2, %hi(io_registers)       # generate upper address (delay)
  andi $4, $4, 0x3FF              # generate offset
  addu $2, $2, $4
  load_s16_unaligned io_registers, -1

1:
  open_load8_seb_core

execute_load_palette_s16u:
  translate_region_align 5, 1, 1, patch_load_s16, palette_ram, 0x3FF
  load_s16_unaligned palette_ram, -1

execute_load_vram_s16u:
  translate_region_vram_load_align 1, 1, patch_load_s16
  load_s16_unaligned vram, -1

execute_load_oam_s16u:
  translate_region_align 7, 1, 1, patch_load_s16, oam_ram, 0x3FF
  load_s16_unaligned oam_ram, -1

execute_load_gamepak8_s16u:
  translate_region_gamepak_align 8, 1, 1, patch_load_s16
  load_s16_unaligned 0, -4

execute_load_gamepak9_s16u:
  translate_region_gamepak_align 9, 1, 1, patch_load_s16
  load_s16_unaligned 0, -4

execute_load_gamepakA_s16u:
  translate_region_gamepak_align 10, 1, 1, patch_load_s16
  load_s16_unaligned 0, -4

execute_load_gamepakB_s16u:
  translate_region_gamepak_align 11, 1, 1, patch_load_s16
  load_s16_unaligned 0, -4

execute_load_gamepakC_s16u:
  translate_region_gamepak_align 12, 1, 1, patch_load_s16
  load_s16_unaligned 0, -4

execute_load_eeprom_s16u:
  eeprom_load_align 1, 1, patch_load_s16

execute_load_backup_s16u:
  backup_load_align 1, 1, patch_load_s16

execute_load_open_s16u:
  region_check_open_align 1, 1, patch_load_s16
  open_load8_seb_core

load_s16_ftable:
  .long execute_load_bios_s16     # 0x00 BIOS
  .long execute_load_open_s16     # 0x01 open address
  .long execute_load_ewram_s16    # 0x02 EWRAM
  .long execute_load_iwram_s16    # 0x03 IWRAM
  .long execute_load_io_s16       # 0x04 I/O registers
  .long execute_load_palette_s16  # 0x05 Palette RAM
  .long execute_load_vram_s16     # 0x06 VRAM
  .long execute_load_oam_s16      # 0x07 OAM RAM
  .long execute_load_gamepak8_s16 # 0x08 gamepak
  .long execute_load_gamepak9_s16 # 0x09 gamepak
  .long execute_load_gamepakA_s16 # 0x0A gamepak
  .long execute_load_gamepakB_s16 # 0x0B gamepak
  .long execute_load_gamepakC_s16 # 0x0C gamepak
  .long execute_load_eeprom_s16   # 0x0D gamepak/eeprom
  .long execute_load_backup_s16   # 0x0E Flash ROM/SRAM
  .long execute_load_open_s16     # 0x0F open unaligned

  .long execute_load_bios_s16u    # 0x00 BIOS unaligned
  .long execute_load_open_s16u    # 0x01 open address unaligned
  .long execute_load_ewram_s16u   # 0x02 EWRAM unaligned
  .long execute_load_iwram_s16u   # 0x03 IWRAM unaligned
  .long execute_load_io_s16u      # 0x04 I/O registers unaligned
  .long execute_load_palette_s16u # 0x05 Palette RAM unaligned
  .long execute_load_vram_s16u    # 0x06 VRAM unaligned
  .long execute_load_oam_s16u     # 0x07 OAM RAM unaligned
  .long execute_load_gamepak8_s16u# 0x08 gamepak unaligned
  .long execute_load_gamepak9_s16u# 0x09 gamepak unaligned
  .long execute_load_gamepakA_s16u# 0x0A gamepak unaligned
  .long execute_load_gamepakB_s16u# 0x0B gamepak unaligned
  .long execute_load_gamepakC_s16u# 0x0C gamepak unaligned
  .long execute_load_eeprom_s16u  # 0x0D gamepak/eeprom unaligned
  .long execute_load_backup_s16u  # 0x0E Flash ROM/SRAM unaligned
  .long execute_load_open_s16u    # 0x0F open unaligned

patch_load_s16:
  patch_handler_align load_s16_ftable, 1



# Unsigned aligned 32bit load handlers

execute_load_bios_u32:
  region_check_align 0, 2, 0, patch_load_u32
  srl $2, $4, 14                  # check if address is in BIOS region (delay)
  bne $2, $0, 2f                  # if not, perform open read
  srl $1, $5, 14                  # check if PC is in BIOS region (delay)
  bne $1, $0, 1f                  # if not, perform BIOS protected read

  lui $2, %hi(bios)               # generate upper address (delay)
  andi $4, $4, 0x3FFF             # generate offset
  addu $2, $2, $4
  load_u32 bios, -1

1:
  lui $2, %hi(bios_read_protect)  # generate upper address
  load_u32 bios_read_protect, -1

2:
  open_load32_core


execute_load_ewram_u32:
  translate_region_ewram_load_align 2, 0, patch_load_u32
  load_u32 ewram_data, -6

  .ent execute_load_u32
execute_load_u32:
execute_load_iwram_u32:
  translate_region_align 3, 2, 0, patch_load_u32, iwram_data, 0x7FFF
  load_u32 iwram_data, -1
  .end execute_load_u32
 
execute_load_io_u32:
  region_check_align 4, 2, 0, patch_load_u32

  srl $2, $4, 10
  andi $1, $2, 0x3FFF             # $1 = (address >> 10) & 0x3FFF
  bne $1, $0, 1f

  lui $2, %hi(io_registers)       # generate upper address (delay)
  andi $4, $4, 0x3FF              # generate offset
  addu $2, $2, $4
  load_u32 io_registers, -1

1:
  open_load32_core

execute_load_palette_u32:
  translate_region_align 5, 2, 0, patch_load_u32, palette_ram, 0x3FF
  load_u32 palette_ram, -2

execute_load_vram_u32:
  translate_region_vram_load_align 2, 0, patch_load_u32
  load_u32 vram, -2

execute_load_oam_u32:
  translate_region_align 7, 2, 0, patch_load_u32, oam_ram, 0x3FF
  load_u32 oam_ram, -2

execute_load_gamepak8_u32:
  translate_region_gamepak_align 8, 2, 0, patch_load_u32
  load_u32 0, -6

execute_load_gamepak9_u32:
  translate_region_gamepak_align 9, 2, 0, patch_load_u32
  load_u32 0, -6

execute_load_gamepakA_u32:
  translate_region_gamepak_align 10, 2, 0, patch_load_u32
  load_u32 0, -6

execute_load_gamepakB_u32:
  translate_region_gamepak_align 11, 2, 0, patch_load_u32
  load_u32 0, -6

execute_load_gamepakC_u32:
  translate_region_gamepak_align 12, 2, 0, patch_load_u32
  load_u32 0, -6

execute_load_gamepakD_u32:
  translate_region_gamepak_align 13, 2, 0, patch_load_u32
  load_u32 0, -6

execute_load_backup_u32:
  backup_load_align 2, 0, patch_load_u32

execute_load_open_u32:
  open_load32_align 2, 0, patch_load_u32


# Unsigned unaligned (by 1) 32bit load handlers

execute_load_bios_u32u1:
  region_check_align 0, 2, 1, patch_load_u32
  srl $2, $4, 14                  # check if address is in BIOS region (delay)
  bne $2, $0, 2f                  # if not, perform open read
  srl $1, $5, 14                  # check if PC is in BIOS region (delay)
  bne $1, $0, 1f                  # if not, perform BIOS protected read

  lui $2, %hi(bios)               # generate upper address (delay)
  andi $4, $4, 0x3FFC             # generate offset
  addu $2, $2, $4
  load_u32_unaligned bios, 1, -1

1:
  lui $2, %hi(bios_read_protect)  # generate upper address
  load_u32_unaligned bios_read_protect, 1, -1

2:
  open_load32_ror_core 1

execute_load_ewram_u32u1:
  translate_region_ewram_load_align32 2, 1, patch_load_u32
  load_u32_unaligned ewram_data, 1, -6

execute_load_iwram_u32u1:
  translate_region_align 3, 2, 1, patch_load_u32, iwram_data, 0x7FFC
  load_u32_unaligned iwram_data, 1, -1

execute_load_io_u32u1:
  region_check_align 4, 2, 1, patch_load_u32

  srl $2, $4, 10
  andi $1, $2, 0x3FFF             # $1 = (address >> 10) & 0x3FFF
  bne $1, $0, 1f

  lui $2, %hi(io_registers)       # generate upper address (delay)
  andi $4, $4, 0x3FC              # generate offset
  addu $2, $2, $4
  load_u32_unaligned io_registers, 1, -1

1:
  open_load32_ror_core 1

execute_load_palette_u32u1:
  translate_region_align 5, 2, 1, patch_load_u32, palette_ram, 0x3FC
  load_u32_unaligned palette_ram, 1, -2

execute_load_vram_u32u1:
  translate_region_vram_load_align32 2, 1, patch_load_u32
  load_u32_unaligned vram, 1, -2

execute_load_oam_u32u1:
  translate_region_align 7, 2, 1, patch_load_u32, oam_ram, 0x3FC
  load_u32_unaligned oam_ram, 1, -2

execute_load_gamepak8_u32u1:
  translate_region_gamepak_align32 8, 2, 1, patch_load_u32
  load_u32_unaligned 0, 1, -6

execute_load_gamepak9_u32u1:
  translate_region_gamepak_align32 9, 2, 1, patch_load_u32
  load_u32_unaligned 0, 1, -6

execute_load_gamepakA_u32u1:
  translate_region_gamepak_align32 10, 2, 1, patch_load_u32
  load_u32_unaligned 0, 1, -6

execute_load_gamepakB_u32u1:
  translate_region_gamepak_align32 11, 2, 1, patch_load_u32
  load_u32_unaligned 0, 1, -6

execute_load_gamepakC_u32u1:
  translate_region_gamepak_align32 12, 2, 1, patch_load_u32
  load_u32_unaligned 0, 1, -6

execute_load_gamepakD_u32u1:
  translate_region_gamepak_align32 13, 2, 1, patch_load_u32
  load_u32_unaligned 0, 1, -6

execute_load_backup_u32u1:
  backup_load_align32 2, 1, patch_load_u32

execute_load_open_u32u1:
  open_load32_align32 2, 1, patch_load_u32

# Unsigned unaligned (by 2) 32bit load handlers

execute_load_bios_u32u2:
  region_check_align 0, 2, 2, patch_load_u32
  srl $2, $4, 14                  # check if address is in BIOS region (delay)
  bne $2, $0, 2f                  # if not, perform open read
  srl $1, $5, 14                  # check if PC is in BIOS region (delay)
  bne $1, $0, 1f                  # if not, perform BIOS protected read

  lui $2, %hi(bios)               # generate upper address (delay)
  andi $4, $4, 0x3FFC             # generate offset
  addu $2, $2, $4
  load_u32_unaligned bios, 2, -1

1:
  lui $2, %hi(bios_read_protect)  # generate upper address
  load_u32_unaligned bios_read_protect, 2, -1

2:
  open_load32_ror_core 2

execute_load_ewram_u32u2:
  translate_region_ewram_load_align32 2, 2, patch_load_u32
  load_u32_unaligned ewram_data, 2, -6

execute_load_iwram_u32u2:
  translate_region_align 3, 2, 2, patch_load_u32, iwram_data, 0x7FFC
  load_u32_unaligned iwram_data, 2, -1

execute_load_io_u32u2:
  region_check_align 4, 2, 2, patch_load_u32

  srl $2, $4, 10
  andi $1, $2, 0x3FFF             # $1 = (address >> 10) & 0x3FFF
  bne $1, $0, 1f

  lui $2, %hi(io_registers)       # generate upper address (delay)
  andi $4, $4, 0x3FC              # generate offset
  addu $2, $2, $4
  load_u32_unaligned io_registers, 2, -1

1:
  open_load32_ror_core 2

execute_load_palette_u32u2:
  translate_region_align 5, 2, 2, patch_load_u32, palette_ram, 0x3FC
  load_u32_unaligned palette_ram, 2, -2

execute_load_vram_u32u2:
  translate_region_vram_load_align32 2, 2, patch_load_u32
  load_u32_unaligned vram, 2, -2

execute_load_oam_u32u2:
  translate_region_align 7, 2, 2, patch_load_u32, oam_ram, 0x3FC
  load_u32_unaligned oam_ram, 2, -2

execute_load_gamepak8_u32u2:
  translate_region_gamepak_align32 8, 2, 2, patch_load_u32
  load_u32_unaligned 0, 2, -6

execute_load_gamepak9_u32u2:
  translate_region_gamepak_align32 9, 2, 2, patch_load_u32
  load_u32_unaligned 0, 2, -6

execute_load_gamepakA_u32u2:
  translate_region_gamepak_align32 10, 2, 2, patch_load_u32
  load_u32_unaligned 0, 2, -6

execute_load_gamepakB_u32u2:
  translate_region_gamepak_align32 11, 2, 2, patch_load_u32
  load_u32_unaligned 0, 2, -6

execute_load_gamepakC_u32u2:
  translate_region_gamepak_align32 12, 2, 2, patch_load_u32
  load_u32_unaligned 0, 2, -6

execute_load_gamepakD_u32u2:
  translate_region_gamepak_align32 13, 2, 2, patch_load_u32
  load_u32_unaligned 0, 2, -6

execute_load_backup_u32u2:
  backup_load_align32 2, 2, patch_load_u32

execute_load_open_u32u2:
  open_load32_align32 2, 2, patch_load_u32

# Unsigned unaligned (by 1) 32bit load handlers

execute_load_bios_u32u3:
  region_check_align 0, 2, 3, patch_load_u32
  srl $2, $4, 14                  # check if address is in BIOS region (delay)
  bne $2, $0, 2f                  # if not, perform open read
  srl $1, $5, 14                  # check if PC is in BIOS region (delay)
  bne $1, $0, 1f                  # if not, perform BIOS protected read

  lui $2, %hi(bios)               # generate upper address (delay)
  andi $4, $4, 0x3FFC             # generate offset
  addu $2, $2, $4
  load_u32_unaligned bios, 3, -1

1:
  lui $2, %hi(bios_read_protect)  # generate upper address
  load_u32_unaligned bios_read_protect, 3, -1

2:
  open_load32_ror_core 3

execute_load_ewram_u32u3:
  translate_region_ewram_load_align32 2, 3, patch_load_u32
  load_u32_unaligned ewram_data, 3, -6

execute_load_iwram_u32u3:
  translate_region_align 3, 2, 3, patch_load_u32, iwram_data, 0x7FFC
  load_u32_unaligned iwram_data, 3, -1

execute_load_io_u32u3:
  region_check_align 4, 2, 3, patch_load_u32

  srl $2, $4, 10
  andi $1, $2, 0x3FFF             # $1 = (address >> 10) & 0x3FFF
  bne $1, $0, 1f

  lui $2, %hi(io_registers)       # generate upper address (delay)
  andi $4, $4, 0x3FC              # generate offset
  addu $2, $2, $4
  load_u32_unaligned io_registers, 3, -1

1:
  open_load32_ror_core 3

execute_load_palette_u32u3:
  translate_region_align 5, 2, 3, patch_load_u32, palette_ram, 0x3FC
  load_u32_unaligned palette_ram, 3, -2

execute_load_vram_u32u3:
  translate_region_vram_load_align32 2, 3, patch_load_u32
  load_u32_unaligned vram, 3, -2

execute_load_oam_u32u3:
  translate_region_align 7, 2, 3, patch_load_u32, oam_ram, 0x3FC
  load_u32_unaligned oam_ram, 3, -2

execute_load_gamepak8_u32u3:
  translate_region_gamepak_align32 8, 2, 3, patch_load_u32
  load_u32_unaligned 0, 3, -6

execute_load_gamepak9_u32u3:
  translate_region_gamepak_align32 9, 2, 3, patch_load_u32
  load_u32_unaligned 0, 3, -6

execute_load_gamepakA_u32u3:
  translate_region_gamepak_align32 10, 2, 3, patch_load_u32
  load_u32_unaligned 0, 3, -6

execute_load_gamepakB_u32u3:
  translate_region_gamepak_align32 11, 2, 3, patch_load_u32
  load_u32_unaligned 0, 3, -6

execute_load_gamepakC_u32u3:
  translate_region_gamepak_align32 12, 2, 3, patch_load_u32
  load_u32_unaligned 0, 3, -6

execute_load_gamepakD_u32u3:
  translate_region_gamepak_align32 13, 2, 3, patch_load_u32
  load_u32_unaligned 0, 3, -6

execute_load_backup_u32u3:
  backup_load_align32 2, 3, patch_load_u32

execute_load_open_u32u3:
  open_load32_align32 2, 3, patch_load_u32


load_u32_ftable:
  .long execute_load_bios_u32     # 0x00 BIOS
  .long execute_load_open_u32     # 0x01 open address
  .long execute_load_ewram_u32    # 0x02 EWRAM
  .long execute_load_iwram_u32    # 0x03 IWRAM
  .long execute_load_io_u32       # 0x04 I/O registers
  .long execute_load_palette_u32  # 0x05 Palette RAM
  .long execute_load_vram_u32     # 0x06 VRAM
  .long execute_load_oam_u32      # 0x07 OAM RAM
  .long execute_load_gamepak8_u32 # 0x08 gamepak
  .long execute_load_gamepak9_u32 # 0x09 gamepak
  .long execute_load_gamepakA_u32 # 0x0A gamepak
  .long execute_load_gamepakB_u32 # 0x0B gamepak
  .long execute_load_gamepakC_u32 # 0x0C gamepak
  .long execute_load_gamepakD_u32 # 0x0D gamepak
  .long execute_load_backup_u32   # 0x0E Flash ROM/SRAM
  .long execute_load_open_u32     # 0x0F open

  .long execute_load_bios_u32u1     # 0x00 BIOS unaligned (1b)
  .long execute_load_open_u32u1     # 0x01 open address unaligned (1b)
  .long execute_load_ewram_u32u1    # 0x02 EWRAM unaligned (1b)
  .long execute_load_iwram_u32u1    # 0x03 IWRAM unaligned (1b)
  .long execute_load_io_u32u1       # 0x04 I/O registers unaligned (1b)
  .long execute_load_palette_u32u1  # 0x05 Palette RAM unaligned (1b)
  .long execute_load_vram_u32u1     # 0x06 VRAM unaligned (1b)
  .long execute_load_oam_u32u1      # 0x07 OAM RAM unaligned (1b)
  .long execute_load_gamepak8_u32u1 # 0x08 gamepak unaligned (1b)
  .long execute_load_gamepak9_u32u1 # 0x09 gamepak unaligned (1b)
  .long execute_load_gamepakA_u32u1 # 0x0A gamepak unaligned (1b)
  .long execute_load_gamepakB_u32u1 # 0x0B gamepak unaligned (1b)
  .long execute_load_gamepakC_u32u1 # 0x0C gamepak unaligned (1b)
  .long execute_load_gamepakD_u32u1 # 0x0D gamepak unaligned (1b)
  .long execute_load_backup_u32u1   # 0x0E Flash ROM/SRAM unaligned (1b)
  .long execute_load_open_u32u1     # 0x0F open unaligned (1b)

  .long execute_load_bios_u32u2     # 0x00 BIOS unaligned (2b)
  .long execute_load_open_u32u2     # 0x01 open address unaligned (2b)
  .long execute_load_ewram_u32u2    # 0x02 EWRAM unaligned (2b)
  .long execute_load_iwram_u32u2    # 0x03 IWRAM unaligned (2b)
  .long execute_load_io_u32u2       # 0x04 I/O registers unaligned (2b)
  .long execute_load_palette_u32u2  # 0x05 Palette RAM unaligned (2b)
  .long execute_load_vram_u32u2     # 0x06 VRAM unaligned (2b)
  .long execute_load_oam_u32u2      # 0x07 OAM RAM unaligned (2b)
  .long execute_load_gamepak8_u32u2 # 0x08 gamepak unaligned (2b)
  .long execute_load_gamepak9_u32u2 # 0x09 gamepak unaligned (2b)
  .long execute_load_gamepakA_u32u2 # 0x0A gamepak unaligned (2b)
  .long execute_load_gamepakB_u32u2 # 0x0B gamepak unaligned (2b)
  .long execute_load_gamepakC_u32u2 # 0x0C gamepak unaligned (2b)
  .long execute_load_gamepakD_u32u2 # 0x0D gamepak unaligned (2b)
  .long execute_load_backup_u32u2   # 0x0E Flash ROM/SRAM unaligned (2b)
  .long execute_load_open_u32u2     # 0x0F open unaligned (2b)

  .long execute_load_bios_u32u3     # 0x00 BIOS unaligned (3b)
  .long execute_load_open_u32u3     # 0x01 open address unaligned (3b)
  .long execute_load_ewram_u32u3    # 0x02 EWRAM unaligned (3b)
  .long execute_load_iwram_u32u3    # 0x03 IWRAM unaligned (3b)
  .long execute_load_io_u32u3       # 0x04 I/O registers unaligned (3b)
  .long execute_load_palette_u32u3  # 0x05 Palette RAM unaligned (3b)
  .long execute_load_vram_u32u3     # 0x06 VRAM unaligned (3b)
  .long execute_load_oam_u32u3      # 0x07 OAM RAM unaligned (3b)
  .long execute_load_gamepak8_u32u3 # 0x08 gamepak unaligned (3b)
  .long execute_load_gamepak9_u32u3 # 0x09 gamepak unaligned (3b)
  .long execute_load_gamepakA_u32u3 # 0x0A gamepak unaligned (3b)
  .long execute_load_gamepakB_u32u3 # 0x0B gamepak unaligned (3b)
  .long execute_load_gamepakC_u32u3 # 0x0C gamepak unaligned (3b)
  .long execute_load_gamepakD_u32u3 # 0x0D gamepak unaligned (3b)
  .long execute_load_backup_u32u3   # 0x0E Flash ROM/SRAM unaligned (3b)
  .long execute_load_open_u32u3     # 0x0F open unaligned (3b)

patch_load_u32:
  patch_handler_align load_u32_ftable, 2



# Unsigned always aligned 32bit load handlers

execute_load_bios_u32a:
  region_check 0, patch_load_u32a
  srl $2, $4, 14                  # check if address is in BIOS region (delay)
  bne $2, $0, 2f                  # if not, perform open read
  srl $1, $5, 14                  # check if PC is in BIOS region (delay)
  bne $1, $0, 1f                  # if not, perform BIOS protected read

  lui $2, %hi(bios)               # generate upper address (delay)
  andi $4, $4, 0x3FFF             # generate offset
  addu $2, $2, $4
  load_u32 bios, -1

1:
  lui $2, %hi(bios_read_protect)  # generate upper address
  load_u32 bios_read_protect, -1

2:
  open_load32_a patch_load_u32a

execute_load_ewram_u32a:
  translate_region_ewram patch_load_u32a
  load_u32 ewram_data, -6

execute_aligned_load32:
execute_load_iwram_u32a:
  translate_region 3, patch_load_u32a, iwram_data, 0x7FFF
  load_u32 iwram_data, -1

execute_load_io_u32a:
  region_check 4, patch_load_u32a

  srl $2, $4, 10
  andi $1, $2, 0x3FFF             # $1 = (address >> 10) & 0x3FFF
  bne $1, $0, 1f

  lui $2, %hi(io_registers)       # generate upper address (delay)
  andi $4, $4, 0x3FF              # generate offset
  addu $2, $2, $4
  load_u32 io_registers, -1

1:
  open_load32_a patch_load_u32a

execute_load_palette_u32a:
  translate_region 5, patch_load_u32a, palette_ram, 0x3FF
  load_u32 palette_ram, -2

execute_load_vram_u32a:
  translate_region_vram patch_load_u32a
  load_u32 vram, -2

execute_load_oam_u32a:
  translate_region 7, patch_load_u32a, oam_ram, 0x3FF
  load_u32 oam_ram, -2

execute_load_gamepak8_u32a:
  translate_region_gamepak_a 8, patch_load_u32a
  load_u32 0, -4

execute_load_gamepak9_u32a:
  translate_region_gamepak_a 9, patch_load_u32a
  load_u32 0, -4

execute_load_gamepakA_u32a:
  translate_region_gamepak_a 10, patch_load_u32a
  load_u32 0, -4

execute_load_gamepakB_u32a:
  translate_region_gamepak_a 11, patch_load_u32a
  load_u32 0, -4

execute_load_gamepakC_u32a:
  translate_region_gamepak_a 12, patch_load_u32a
  load_u32 0, -4

execute_load_gamepakD_u32a:
  translate_region_gamepak_a 13, patch_load_u32a
  load_u32 0, -4

execute_load_backup_u32a:
  backup_load_a patch_load_u32a

execute_load_open_u32a:
  open_load32_a patch_load_u32a

load_u32a_ftable:
  .long execute_load_bios_u32a      # 0x00 BIOS unaligned (3b)
  .long execute_load_open_u32a      # 0x01 open address unaligned (3b)
  .long execute_load_ewram_u32a     # 0x02 EWRAM unaligned (3b)
  .long execute_load_iwram_u32a     # 0x03 IWRAM unaligned (3b)
  .long execute_load_io_u32a        # 0x04 I/O registers unaligned (3b)
  .long execute_load_palette_u32a   # 0x05 Palette RAM unaligned (3b)
  .long execute_load_vram_u32a      # 0x06 VRAM unaligned (3b)
  .long execute_load_oam_u32a       # 0x07 OAM RAM unaligned (3b)
  .long execute_load_gamepak8_u32a  # 0x08 gamepak unaligned (3b)
  .long execute_load_gamepak9_u32a  # 0x09 gamepak unaligned (3b)
  .long execute_load_gamepakA_u32a  # 0x0A gamepak unaligned (3b)
  .long execute_load_gamepakB_u32a  # 0x0B gamepak unaligned (3b)
  .long execute_load_gamepakC_u32a  # 0x0C gamepak unaligned (3b)
  .long execute_load_gamepakD_u32a  # 0x0D gamepak unaligned (3b)
  .long execute_load_backup_u32a    # 0x0E Flash ROM/SRAM unaligned (3b)
  .long execute_load_open_u32a      # 0x0F open unaligned (3b)

patch_load_u32a:
  patch_handler load_u32a_ftable, 1


# Unsigned 8bit store handlers

execute_store_ignore0_u8:
  ignore_region 0, patch_store_u8

execute_store_ignore1_u8:
  ignore_region 1, patch_store_u8

execute_store_ewram_u8:
  translate_region_ewram patch_store_u8
  store_u8_metadata ewram_data, -3, post_write_metadata_ewram

execute_store_u8:
execute_store_iwram_u8:
  translate_region 3, patch_store_u8, iwram_data, 0x7FFF
  store_u8_metadata iwram_data, -1, post_write_metadata_iwram

execute_store_io_u8:
  region_check 4, patch_store_u8

  srl $2, $4, 10
  andi $2, $2, 0x3FFF             # $2 = (address >> 10) & 0x3FFF
  bne $2, $0, store_ignore        # if not, unwritable

  andi $5, $5, 0xFF               # make value 8bit (BNE delay slot)
  andi $4, $4, 0x3FF              # wrap around address

  sw $6, REG_PC($16)              # save the PC

  addiu $sp, $sp, -4 * _SA        # make room on the stack for $ra
  sw $ra, ($sp)

  save_registers
  jal write_io_register8          # write the value out
  addiu $17, $17, -1              # waitstate cycles (delay)

  j write_io_epilogue             # handle any state changes
  nop                             # cannot delay usefully here

execute_store_palette_u8:
  region_check 5, patch_store_u8
  lui $2, %hi(palette_ram)        # start loading palette_ram address (delay)
#ifdef MIPS_32R2
  ins   $5, $5, 8, 8              # double value
#else
  andi  $5, $5, 0x00FF
  sll   $1, $5, 8
  or    $5, $1, $5
#endif
  andi $4, $4, 0x3FE              # align palette address
  addu $2, $2, $4

  sh $5, %lo(palette_ram)($2)     # palette_ram[address] = value
  addiu $17, $17, -1              # waitstate cycles

  blez $17, store_update_gba
  sw $0, CHANGED_PC_STATUS($16)

  jr $ra                          # return
  nop

execute_store_vram_u8:
  translate_region_vram_store_align16 patch_store_u8
  store_u8_double vram
  j post_write_metadata_vram
  addiu $17, $17, -1              # waitstate cycles

execute_store_oam_u8:
  ignore_region 7, patch_store_u8 # Write byte datas are ignore

execute_store_ignore8_u8:
  ignore_region 8, patch_store_u8

execute_store_ignore9_u8:
  ignore_region 9, patch_store_u8

execute_store_ignoreA_u8:
  ignore_region 10, patch_store_u8

execute_store_ignoreB_u8:
  ignore_region 11, patch_store_u8

execute_store_ignoreC_u8:
  ignore_region 12, patch_store_u8

execute_store_ignoreD_u8:
  ignore_region 13, patch_store_u8

execute_store_backup_u8:
  store_function write_backup, 14, patch_store_u8, 0xFFFF, -9

execute_store_ignoreF_u8:
  ignore_high patch_store_u8

store_u8_ftable:
  .long execute_store_ignore0_u8  # 0x00 BIOS
  .long execute_store_ignore1_u8  # 0x01 open address
  .long execute_store_ewram_u8    # 0x02 EWRAM
  .long execute_store_iwram_u8    # 0x03 IWRAM
  .long execute_store_io_u8       # 0x04 I/O registers
  .long execute_store_palette_u8  # 0x05 Palette RAM
  .long execute_store_vram_u8     # 0x06 VRAM
  .long execute_store_oam_u8      # 0x07 OAM RAM
  .long execute_store_ignore8_u8  # 0x08 gamepak
  .long execute_store_ignore9_u8  # 0x09 gamepak
  .long execute_store_ignoreA_u8  # 0x0A gamepak
  .long execute_store_ignoreB_u8  # 0x0B gamepak
  .long execute_store_ignoreC_u8  # 0x0C gamepak
  .long execute_store_ignoreD_u8  # 0x0D gamepak
  .long execute_store_backup_u8   # 0x0E Flash ROM/SRAM
  .long execute_store_ignoreF_u8  # 0x0F open address

patch_store_u8:
  patch_handler store_u8_ftable, 0x0F


# Unsigned 16bit store handlers

execute_store_ignore0_u16:
  ignore_region 0, patch_store_u16

execute_store_ignore1_u16:
  ignore_region 1, patch_store_u16

execute_store_ewram_u16:
  translate_region_ewram_store_align16 patch_store_u16
  store_u16_metadata ewram_data, -3, post_write_metadata_ewram

execute_store_u16:
execute_store_iwram_u16:
  translate_region 3, patch_store_u16, iwram_data, 0x7FFE
  store_u16_metadata iwram_data, -1, post_write_metadata_iwram

execute_store_io_u16:
  region_check 4, patch_store_u16

  srl $2, $4, 10
  andi $2, $2, 0x3FFF             # $2 = (address >> 10) & 0x3FFF
  bne $2, $0, store_ignore        # if not, unwritable

  andi $4, $4, 0x3FE              # wrap around/align address (delay)
  andi $5, $5, 0xFFFF             # make value 16bit

  sw $6, REG_PC($16)              # save the PC

  addiu $sp, $sp, -4 * _SA        # make room on the stack for $ra
  sw $ra, ($sp)

  save_registers
  jal write_io_register16         # write the value out
  addiu $17, $17, -1              # waitstate cycles (delay)

  j write_io_epilogue             # handle any state changes
  nop                             # cannot delay usefully here

execute_store_palette_u16:
  translate_region 5, patch_store_u16, palette_ram, 0x3FE
  store_u16 palette_ram, -1

execute_store_vram_u16:
  translate_region_vram_store_align16 patch_store_u16
  store_u16_metadata vram, -1, post_write_metadata_vram

execute_store_oam_u16:
  translate_region 7, patch_store_u16, oam_ram, 0x3FE
  lui $1, %hi(oam_update)         # write non-zero to oam_update
  sw $1, %lo(oam_update)($1)      # cheap, but the address is non-zero
  store_u16 oam_ram, -1

execute_store_rtc_u16:
  store_function write_rtc, 8, patch_store_u16, 0xFE, -4

execute_store_ignore9_u16:
  ignore_region 9, patch_store_u16

execute_store_ignoreA_u16:
  ignore_region 10, patch_store_u16

execute_store_ignoreB_u16:
  ignore_region 11, patch_store_u16

execute_store_ignoreC_u16:
  ignore_region 12, patch_store_u16

execute_store_ignoreD_u16:
  ignore_region 13, patch_store_u16

execute_store_ignoreE_u16:
  ignore_region 14, patch_store_u16

execute_store_ignoreF_u16:
  ignore_high patch_store_u16


store_u16_ftable:
  .long execute_store_ignore0_u16 # 0x00 BIOS
  .long execute_store_ignore1_u16 # 0x01 open address
  .long execute_store_ewram_u16   # 0x02 EWRAM
  .long execute_store_iwram_u16   # 0x03 IWRAM
  .long execute_store_io_u16      # 0x04 I/O registers
  .long execute_store_palette_u16 # 0x05 Palette RAM
  .long execute_store_vram_u16    # 0x06 VRAM
  .long execute_store_oam_u16     # 0x07 OAM RAM
  .long execute_store_rtc_u16     # 0x08 gamepak
  .long execute_store_ignore9_u16 # 0x09 gamepak
  .long execute_store_ignoreA_u16 # 0x0A gamepak
  .long execute_store_ignoreB_u16 # 0x0B gamepak
  .long execute_store_ignoreC_u16 # 0x0C gamepak
  .long execute_store_ignoreD_u16 # 0x0D gamepak
  .long execute_store_ignoreE_u16 # 0x0E Flash ROM/SRAM
  .long execute_store_ignoreF_u16 # 0x0F open address

patch_store_u16:
  patch_handler store_u16_ftable, 0x0F


# Unsigned 32bit store handlers

execute_store_ignore0_u32:
  ignore_region 0, patch_store_u32

execute_store_ignore1_u32:
  ignore_region 1, patch_store_u32

execute_store_ewram_u32:
  translate_region_ewram_store_align32 patch_store_u32
  store_u32_metadata ewram_data, -6, post_write_metadata_ewram

execute_store_u32:
execute_store_iwram_u32:
  translate_region 3, patch_store_u32, iwram_data, 0x7FFC
  store_u32_metadata iwram_data, -1, post_write_metadata_iwram

execute_store_io_u32:
  region_check 4, patch_store_u32

  srl $2, $4, 10
  andi $2, $2, 0x3FFF             # $2 = (address >> 10) & 0x3FFF
  bne $2, $0, store_ignore        # if not, unwritable

  andi $4, $4, 0x3FC              # wrap around/align address (delay)
  sw $6, REG_PC($16)              # save the PC

  addiu $sp, $sp, -4 * _SA        # make room on the stack for $ra
  sw $ra, ($sp)

  save_registers
  jal write_io_register32         # write the value out
  addiu $17, $17, -1              # waitstate cycles (delay)

  j write_io_epilogue             # handle any state changes
  nop                             # cannot delay usefully here

execute_store_palette_u32:
  translate_region 5, patch_store_u32, palette_ram, 0x3FC
  store_u32 palette_ram, -2

execute_store_vram_u32:
  translate_region_vram_store_align32 patch_store_u32
  store_u32_metadata vram, -2, post_write_metadata_vram

execute_store_oam_u32:
  translate_region 7, patch_store_u32, oam_ram, 0x3FC
  lui $1, %hi(oam_update)         # write non-zero to oam_update
  sw $1, %lo(oam_update)($1)      # cheap, but the address is non-zero
  store_u32 oam_ram, -2

execute_store_ignore8_u32:
  ignore_region 8, patch_store_u32

execute_store_ignore9_u32:
  ignore_region 9, patch_store_u32

execute_store_ignoreA_u32:
  ignore_region 10, patch_store_u32

execute_store_ignoreB_u32:
  ignore_region 11, patch_store_u32

execute_store_ignoreC_u32:
  ignore_region 12, patch_store_u32

execute_store_ignoreD_u32:
  ignore_region 13, patch_store_u32

execute_store_ignoreE_u32:
  ignore_region 14, patch_store_u32

execute_store_ignoreF_u32:
  ignore_high patch_store_u32

store_u32_ftable:
  .long execute_store_ignore0_u32 # 0x00 BIOS
  .long execute_store_ignore1_u32 # 0x01 open address
  .long execute_store_ewram_u32   # 0x02 EWRAM
  .long execute_store_iwram_u32   # 0x03 IWRAM
  .long execute_store_io_u32      # 0x04 I/O registers
  .long execute_store_palette_u32 # 0x05 Palette RAM
  .long execute_store_vram_u32    # 0x06 VRAM
  .long execute_store_oam_u32     # 0x07 OAM RAM
  .long execute_store_ignore8_u32 # 0x08 gamepak
  .long execute_store_ignore9_u32 # 0x09 gamepak
  .long execute_store_ignoreA_u32 # 0x0A gamepak
  .long execute_store_ignoreB_u32 # 0x0B gamepak
  .long execute_store_ignoreC_u32 # 0x0C gamepak
  .long execute_store_ignoreD_u32 # 0x0D gamepak
  .long execute_store_ignoreE_u32 # 0x0E Flash ROM/SRAM
  .long execute_store_ignoreF_u32 # 0x0F open address


patch_store_u32:
  patch_handler store_u32_ftable, 0x0F



# Unsigned always aligned, a2 safe 32bit store handlers

execute_store_ignore0_u32a:
  ignore_region 0, patch_store_u32a

execute_store_ignore1_u32a:
  ignore_region 1, patch_store_u32a

execute_store_ewram_u32a:
  translate_region_ewram_store_align32 patch_store_u32a
  store_u32a ewram_data, -6

execute_aligned_store32:
execute_store_iwram_u32a:
  translate_region 3, patch_store_u32a, iwram_data, 0x7FFC
  store_u32a iwram_data, -1

execute_store_io_u32a:
  region_check 4, patch_store_u32a

  srl $2, $4, 10
  andi $2, $2, 0x3FFF             # $2 = (address >> 10) & 0x3FFF
  bne $2, $0, store_ignore        # if not, unwritable

  sw $6, REG_SAVE($16)            # save a2 (delay)
  sw $ra, REG_SAVE2($16)          # save ra

  save_registers
  jal write_io_register32         # write the value out
  andi $4, $4, 0x3FC              # wrap around/align address (branch delay)

  lw $6, REG_SAVE($16)            # restore a2
  lw $ra, REG_SAVE2($16)          # restore ra
  restore_registers

  jr $ra
  addiu $17, $17, -1              # waitstate cycles (delay)

execute_store_palette_u32a:
  translate_region 5, patch_store_u32a, palette_ram, 0x3FC
  store_u32a palette_ram, -2

execute_store_vram_u32a:
  translate_region_vram_store_align32 patch_store_u32a
  store_u32a vram, -2

execute_store_oam_u32a:
  translate_region 7, patch_store_u32a, oam_ram, 0x3FC
  lui $1, %hi(oam_update)         # write non-zero to oam_update
  sw $1, %lo(oam_update)($1)      # cheap, but the address is non-zero
  store_u32a oam_ram, -2

execute_store_ignore8_u32a:
  ignore_region 8, patch_store_u32a

execute_store_ignore9_u32a:
  ignore_region 9, patch_store_u32a

execute_store_ignoreA_u32a:
  ignore_region 10, patch_store_u32a

execute_store_ignoreB_u32a:
  ignore_region 11, patch_store_u32a

execute_store_ignoreC_u32a:
  ignore_region 12, patch_store_u32a

execute_store_ignoreD_u32a:
  ignore_region 13, patch_store_u32a

execute_store_ignoreE_u32a:
  ignore_region 14, patch_store_u32a

execute_store_ignoreF_u32a:
  ignore_high patch_store_u32a

store_u32a_ftable:
  .long execute_store_ignore0_u32a # 0x00 BIOS
  .long execute_store_ignore1_u32a # 0x01 open address
  .long execute_store_ewram_u32a   # 0x02 EWRAM
  .long execute_store_iwram_u32a   # 0x03 IWRAM
  .long execute_store_io_u32a      # 0x04 I/O registers
  .long execute_store_palette_u32a # 0x05 Palette RAM
  .long execute_store_vram_u32a    # 0x06 VRAM
  .long execute_store_oam_u32a     # 0x07 OAM RAM
  .long execute_store_ignore8_u32a # 0x08 gamepak
  .long execute_store_ignore9_u32a # 0x09 gamepak
  .long execute_store_ignoreA_u32a # 0x0A gamepak
  .long execute_store_ignoreB_u32a # 0x0B gamepak
  .long execute_store_ignoreC_u32a # 0x0C gamepak
  .long execute_store_ignoreD_u32a # 0x0D gamepak
  .long execute_store_ignoreE_u32a # 0x0E Flash ROM/SRAM
  .long execute_store_ignoreF_u32a # 0x0F open address

patch_store_u32a:
  patch_handler store_u32a_ftable, 0x0F



# General ext memory routines

store_ignore:
  jr $ra                          # ignore these writes
  addiu $17, $17, -1              # waitstate cycles (delay)


# $2: alert type
# bit0 - SMC
# bit1 - IRQ
# bit2 - HALT
# bit3 - timer counter change.
# bit4 - in DMA transfer, cpu stop.

write_io_epilogue:
  andi $4, $2, 0x01
  bne $4, $0, io_smc_write        # is it an SMC alert?
  andi $2, $2, 0x1e

  blez $17, io_update_gba         # if cycle counter <= 0
  nop

  bne $2, $0, io_alert
  nop

no_alert:
  lw $ra, ($sp)                   # restore return address
  restore_registers               # restore the registers
  jr $ra                          # return
  addiu $sp, $sp, 4 * _SA         # fix the stack (delay)

io_alert:
  la $4, execute_cycles
  lw $1, 0($4)
  lw $2, REG_CPSR($16)            # load CPSR
  sw $0, CHANGED_PC_STATUS($16)

  subu $1, $1, $17                # execute_cycles -= cycle counter
  sw $1, 0($4)

  collapse_flags_body             # insert flag into $2
  jal update_gba                  # process the next event
  sw $2, REG_CPSR($16)            # store CPSR

  lw $1, CHANGED_PC_STATUS($16)

  lw $ra, ($sp)                   # restore return address
  addiu $sp, $sp, 4 * _SA         # fix the stack

  bne $1, $0, lookup_pc_changed
  addu $17, $2, $0                # $17 = new cycle count (delay)

  restore_registers_0             # restore the registers 1/2
  jr $ra                          # return
  restore_registers_1             # restore the registers 2/2 (delay)

io_update_gba:
  lw $2, REG_CPSR($16)            # load CPSR
  sw $0, CHANGED_PC_STATUS($16)

  collapse_flags_body             # insert flag into $2
  jal update_gba                  # process the next event
  sw $2, REG_CPSR($16)            # store CPSR

  lw $1, CHANGED_PC_STATUS($16)

  lw $ra, ($sp)                   # restore return address
  addiu $sp, $sp, 4 * _SA         # fix the stack

  bne $1, $0, lookup_pc_changed
  addu $17, $2, $0                # $17 = new cycle count (delay)

  restore_registers_0             # restore the registers 1/2
  jr $ra                          # return
  restore_registers_1             # restore the registers 2/2 (delay)

io_smc_write:
  blez $17, smc_write_update_gba  # if cycle counter <= 0
  addiu $sp, $sp, 4 * _SA         # fix the stack (delay)

  beq $2, $0, lookup_pc           # alert none
  nop

io_smc_alert:
  la $4, execute_cycles
  lw $1, 0($4)
  lw $2, REG_CPSR($16)            # load CPSR
  sw $0, CHANGED_PC_STATUS($16)

  subu $1, $1, $17                # execute_cycles -= cycle counter
  sw $1, 0($4)

  collapse_flags_body             # insert flag into $2
  jal update_gba                  # process the next event
  sw $2, REG_CPSR($16)            # store CPSR

  lw $1, CHANGED_PC_STATUS($16)
#ifndef MIPS_XBURST
  nop
#endif

  bne $1, $0, lookup_pc_changed
  addu $17, $2, $0                # $17 = new cycle count (delay)

  j lookup_pc
  nop


smc_write_update_gba:
  lw $2, REG_CPSR($16)            # load CPSR
  sw $0, CHANGED_PC_STATUS($16)

  collapse_flags_body             # insert flag into $2
  jal update_gba                  # process the next event
  sw $2, REG_CPSR($16)            # store CPSR (delay)

  lw $1, CHANGED_PC_STATUS($16)
#ifndef MIPS_XBURST
  nop
#endif

  bne $1, $0, lookup_pc_changed
  addu $17, $2, $0                # $17 = new cycle count (delay)

  j lookup_pc
  nop

# smc_write expects three registers to be set:
# $4 = offset into the Data Area, canonical (address AND (size - 1))
# $5 = GBA memory region (0x02, 0x03, 0x06, etc.)
# $6 = Program Counter, which is immediately looked up
smc_write:
  save_registers
  jal partial_clear_metadata      # flush some of the Metadata Entries
  sw $6, REG_PC($16)              # save PC (delay slot)
  blez $17, smc_write_update_gba  # if counter <= 0
  nop

lookup_pc:
  lw $2, REG_CPSR($16)            # $2 = cpsr
#ifndef MIPS_XBURST
  nop
#endif
  andi $2, $2, 0x20               # isolate mode bit
  beq $2, $0, lookup_pc_arm       # if T bit is zero use arm handler
  nop                             # cannot delay usefully here

lookup_pc_thumb:
#ifndef MIPS_XBURST
  lw $4, REG_PC($16)              # load PC as arg 0 (delay slot)
  jal block_lookup_address_thumb  # get Thumb address
  nop
#else
  jal block_lookup_address_thumb  # get Thumb address
  lw $4, REG_PC($16)              # load PC as arg 0 (branch delay slot)
#endif

  restore_registers
  jr $2                           # jump to result
  nop                             # cannot delay usefully here

lookup_pc_changed:
  extract_flags                   # $1 = cpsr
  andi $2, $1, 0x20               # isolate state bit
  bne $2, $0, lookup_pc_thumb     # if T bit is not zero use thumb handler
  nop

lookup_pc_arm:
#ifndef MIPS_XBURST
  lw $4, REG_PC($16)              # load PC as arg 0 (delay slot)
  jal block_lookup_address_arm    # get ARM address
  nop
#else
  jal block_lookup_address_arm    # get ARM address
  lw $4, REG_PC($16)              # load PC as arg 0 (branch delay slot)
#endif

  restore_registers
  jr $2                           # jump to result
  nop                             # cannot delay usefully here

# Return the current cpsr

execute_read_cpsr:
  lw $2, REG_CPSR($16)            # load CPSR
#ifndef MIPS_XBURST
nop
#endif
  collapse_flags_body             # insert flag into $2
  sw $2, REG_CPSR($16)            # store CPSR
  jr $ra                          # return
  nop                             # cannot delay usefully here

# Return the current spsr

execute_read_spsr:
  lw $1, CPU_MODE($16)            # $1 = cpu_mode

  lui $2, %hi(spsr)
  sll $1, $1, 2                   # adjust to word offset size
  addu $2, $2, $1
  lw $2, %lo(spsr)($2)            # $2 = spsr[cpu_mode] (delay slot)

  jr $ra                          # return
  nop


# Switch into SWI, has to collapse flags
# $4: Current pc

execute_swi:
  lw $2, REG_CPSR($16)            # load CPSR

  lui $1, %hi(SUPERVISOR_LR)
  sw $4, %lo(SUPERVISOR_LR)($1)   # store next PC in the supervisor's LR

  addiu $sp, $sp, -4 * _SA        # push $ra
  sw $ra, ($sp)

  collapse_flags_body             # insert flag into $2
  lui $5, %hi(SUPERVISOR_SPSR)
  sw $2, %lo(SUPERVISOR_SPSR)($5) # save cpsr in SUPERVISOR_SPSR

#ifdef MIPS_32R2
  ins $2, $0, 0, 8                # zero out bottom 8 bits of CPSR
#else
  srl $2, $2, 8
  sll $2, $2, 8
#endif
  ori $2, 0x93                    # set mode to supervisor & disable IRQ
  sw $2, REG_CPSR($16)            # write back CPSR

  save_registers
  jal set_cpu_mode                # set the CPU mode to supervisor
  ori $4, $0, 3                   # 3 is supervisor mode (delay slot)

  lw $ra, ($sp)                   # pop $ra
  restore_registers

  jr $ra                          # return
  addiu $sp, $sp, 4 * _SA         # fix stack (delay slot)


# $4: pc to restore to
# returns in $4

execute_spsr_restore:
  lw $1, CPU_MODE($16)            # $1 = cpu_mode
#ifndef MIPS_XBURST
  nop
#endif
  beq $1, $0, no_spsr_restore     # only restore if the cpu isn't usermode

  lui $2, %hi(spsr)               # start loading SPSR (delay)
  sll $1, $1, 2                   # adjust to word offset size
  addu $2, $2, $1
  lw $1, %lo(spsr)($2)            # $1 = spsr[cpu_mode]
#ifndef MIPS_XBURST
  nop
#endif
  sw $1, REG_CPSR($16)            # cpsr = spsr[cpu_mode]
  extract_flags_body              # extract flags from $1

  srl $2, $1, 5
  andi $2, $2, 0x01               # $2 = cpu state
  or $4, $4, $2                   # if Thumb state, address |= 0x01

  srl $1, $4, 24
  beql $2, $0, 1f
  addiu $1, $1, 0x10              # if ARM state offset +16 (delay Likely)

1:
  count_branch_ticks_dual

  save_registers_0                # save the registers 1/2
  jal execute_spsr_restore_body   # do the dirty work in this C function
  save_registers_1                # save the registers 2/2 (delay)

  srl $5, $2, 31                  # bit 31: interrupt flag
  sll $4, $2, 1
  blez $17, 2f
  srl $4, $4, 1                   # move return value to $4 (delay)

  bne $5, $0, 3f
  nop

  jal block_lookup_address_dual   # $2 = MIPS address to jump to
  nop

  restore_registers_0             # restore the registers 1/2
  jr $2
  restore_registers_1             # restore the registers 2/2 (delay)

2:
  sw $0, CHANGED_PC_STATUS($16)

  jal update_gba                  # process the next event
  sw $4, REG_PC($16)              # next PC = $4 (delay)

  lw $1, CHANGED_PC_STATUS($16)
#ifndef MIPS_XBURST
  nop
#endif

  bne $1, $0, lookup_pc_changed
  addu $17, $2, $0                # $17 = new cycle count (delay slot)

  jal block_lookup_address_dual   # $2 = MIPS address to jump to
  lw $4, REG_PC($16)              # restore branch address (delay)

  restore_registers_0             # restore the registers 1/2
  jr $2
  restore_registers_1             # restore the registers 2/2 (delay)

3:
  la $2, execute_cycles
  lw $1, 0($2)
  sw $4, REG_PC($16)              # next PC = $4
  sw $0, CHANGED_PC_STATUS($16)

  subu $1, $1, $17
  jal update_gba                  # process the next event
  sw $1, 0($2)

  lw $1, CHANGED_PC_STATUS($16)
#ifndef MIPS_XBURST
  nop
#endif

  bne $1, $0, lookup_pc_changed
  addu $17, $2, $0                # $17 = new cycle count (delay slot)

  jal block_lookup_address_dual   # $2 = MIPS address to jump to
  lw $4, REG_PC($16)              # restore branch address (delay)

  restore_registers_0             # restore the registers 1/2
  jr $2                           # jump to it
  restore_registers_1             # restore the registers 2/2 (delay)

no_spsr_restore:
  jr $ra
  nop

# $4: new cpsr
# $5: store mask
# $6: current PC

execute_store_cpsr:
  lw $2, REG_CPSR($16)            # $2 = current cpsr
  sw $6, REG_PC($16)              # current PC = $6

  addiu $sp, $sp, -4 * _SA
  sw $ra, ($sp)

  andi $1, $2, 0x0F
  bne $1, $0, 1f
  srl $1, $5, 24
  sll $5, $1, 24                  # user mode, only condition code bits

1:
  and $1, $4, $5                  # $1 = new_cpsr & store_mask
  nor $4, $5, $0                  # $4 = ~store_mask (delay usefully for $2)
  and $2, $2, $4                  # $2 = (cpsr & (~store_mask))
  or $1, $1, $2                   # $1 = new cpsr combined with old

  extract_flags_body              # extract flags from $1
  ori $4, $1, 0x10                # fix mode bits

  save_registers
  jal execute_store_cpsr_body     # do the dirty work in this C function
  sw $4, REG_CPSR($16)            # store the new CPSR (delay slot)

  beq $2, $0, 2f
  lui $4, %hi(execute_cycles)     # $4 = execute_cycles (delay)
  addiu $4, $4, %lo(execute_cycles)
  lw $1, 0($4)

  sw $0, CHANGED_PC_STATUS($16)

  subu $1, $1, $17
  jal update_gba                  # process the next event
  sw $1, 0($4)

  lw $1, CHANGED_PC_STATUS($16)
  addu $17, $2, $0                # $17 = new cycle count

  bnel $1, $0, lookup_pc_changed
  addiu $sp, $sp, 4 * _SA         # fix stack (delay Likely)

2:
  lw $ra, ($sp)
  restore_registers               # restore the registers

  jr $ra
  addiu $sp, $sp, 4 * _SA


# $4: new spsr
# $5: store mask

execute_store_spsr:
  lw $1, CPU_MODE($16)            # $1 = cpu_mode

  lui $2, %hi(spsr)
  sll $1, $1, 2                   # adjust to word offset size
  addu $1, $2, $1
  lw $2, %lo(spsr)($1)            # $2 = spsr[cpu_mode]

  and $4, $4, $5                  # $4 = new_spsr & store_mask
  nor $5, $5, $0                  # $5 = ~store_mask
  and $2, $2, $5                  # $2 = (spsr & (~store_mask))
  or $4, $4, $2                   # $4 = new spsr combined with old

  jr $ra                          # return
  sw $4, %lo(spsr)($1)            # spsr[cpu_mode] = $4 (delay slot)


# $4: value
# $5: shift

execute_lsl_flags_reg:
  beq $5, $0, lsl_shift_zero      # is the shift zero?
  nop
  sltiu $1, $5, 32                # $1 = (shift < 32)
  beq $1, $0, lsl_shift_high      # is the shift >= 32?
  addiu $2, $0, 32                # $2 = 32 (delay)

  subu $2, $2, $5                 # $2 = (32 - shift)
  srlv $2, $4, $2                 # $2 = (value >> (32 - shift))
  andi $22, $2, 1                 # c flag = (value >> (32 - shift)) & 0x01

lsl_shift_zero:
  jr $ra                          # return
  sllv $4, $4, $5                 # return (value << shift) (delay)

lsl_shift_high:
  sltiu $1, $5, 33                # $1 = (shift < 33)
  andi $22, $4, 1                 # c flag = value & 0x01

  beql $1, $0, lsl_shift_done     # jump if shift != 32
  add $22, $0, $0                 # c flag = 0 otherwise (delay Likely)

lsl_shift_done:
  jr $ra                          # return
  add $4, $0, $0                  # value = 0 no matter what (delay)


execute_lsr_flags_reg:
  beq $5, $0, lsr_shift_zero      # is the shift zero?
  nop
  sltiu $1, $5, 32                # $1 = (shift < 32)
  beq $1, $0, lsr_shift_high      # is the shift >= 32?
  addiu $2, $5, -1                # $2 = shift - 1 (delay)

  srlv $2, $4, $2                 # $2 = (value >> (shift - 1))
  andi $22, $2, 1                 # c flag = (value >> (shift - 1)) & 0x01

lsr_shift_zero:
  jr $ra                          # return
  srlv $4, $4, $5                 # return (value >> shift) (delay)

lsr_shift_high:
  sltiu $1, $5, 33                # $1 = (shift < 33)
  srl $22, $4, 31                 # c flag = value >> 31

  beql $1, $0, lsr_shift_done     # jump if shift != 32
  add $22, $0, $0                 # c flag = 0 otherwise (delay Likely)

lsr_shift_done:
  jr $ra                          # return
  add $4, $0, $0                  # value = 0 no matter what


execute_asr_flags_reg:
  beq $5, $0, asr_shift_zero      # is the shift zero?
  nop
  sltiu $1, $5, 32                # $1 = (shift < 32)
  beq $1, $0, asr_shift_high      # is the shift >= 32?
  addiu $2, $5, -1                # $2 = shift - 1 (delay)

  srlv $2, $4, $2                 # $2 = (value >> (shift - 1))
  andi $22, $2, 1                 # c flag = (value >> (shift - 1)) & 0x01

asr_shift_zero:
  jr $ra                          # return
  srav $4, $4, $5                 # return (value >> shift) (delay)

asr_shift_high:
  sra $4, $4, 31                  # value >>= 31
  jr $ra                          # return
  andi $22, $4, 1                 # c flag = value & 0x01


execute_ror_flags_reg:
  beq $5, $0, ror_zero_shift      # is the shift zero?
  nop
  sltiu $1, $5, 32                # $1 = (shift < 32)
  beq $1, $0, ror_shift_high      # is the shift >= 32?
  addiu $1, $5, -1                # $1 = (shift - 1) (delay)

  srav $1, $4, $1                 # $1 = (value >> (shift - 1))
  andi $22, $1, 1                 # c flag = $1 & 1

ror_zero_shift:
# TODO [Neb] Optimise with ROTRV instruction
  subu $1, $0, $5
  addiu $1, $1, 32
  sllv $1, $4, $1
  srlv $4, $4, $5
  jr $ra                          # return
#  rotrv $4, $4, $5                # return (value ror shift) delay
  or   $4, $4, $1                 # emulate ROTRV instruction

ror_shift_high:
  jr $ra                          # return
  srl $22, $4, 31                 # c flag = value >> 31 (delay)


# $4: cycle counter argument

  .text
  .global execute_arm_translate
  .ent execute_arm_translate
  .set noat
  .set noreorder
execute_arm_translate:
  lui $16, %hi(reg)               # load base register
  addiu $16, $16, %lo(reg)
  addu $17, $4, $0                # load cycle counter register
  extract_flags

  jal block_lookup_address_arm    # lookup initial jump address
  lw $4, REG_PC($16)              # load PC into $4 (delay)

  restore_registers_0             # restore the registers 1/2
  jr $2                           # jump to return
  restore_registers_1             # restore the registers 2/2 (delay)

  .end execute_arm_translate

call_bios_hle:
  addiu $sp, $sp, -4 * _SA
  sw $ra, ($sp)
  collapse_flags
  save_registers_0
  jalr $1
  save_registers_1

  lw $ra, ($sp)
  restore_registers

  jr $ra
  addiu $sp, $sp, 4 * _SA         # branch delay

.bss


memory_map_read:
  .space 0x8000

reg:
  .space 0x100

memory_map_write:
  .space 0x8000

#ifdef MIPS_32R2
.set mips0
#endif
