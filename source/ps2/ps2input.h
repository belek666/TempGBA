/* Per-platform code - gpSP on GCW Zero
 *
 * Copyright (C) 2013 Dingoonity/GBATemp user Nebuleon
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licens e as
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

#ifndef __OD_INPUT_H__
#define __OD_INPUT_H__

#define PS2_BUTTON_COUNT 20

#define PS2_ALL_BUTTON_MASK 0xFFFF


// These must be in the order defined in OpenDinguxKeys in od-input.c.
enum PS2_Buttons {
	PS2_BUTTON_SELECT 	= 0x00001,
	PS2_BUTTON_L3		= 0x00002,
	PS2_BUTTON_R3		= 0x00004,
	PS2_BUTTON_START    = 0x00008,
	PS2_BUTTON_UP       = 0x00010,
	PS2_BUTTON_RIGHT    = 0x00020,
	PS2_BUTTON_DOWN     = 0x00040,
	PS2_BUTTON_LEFT     = 0x00080,
	PS2_BUTTON_L2  		= 0x00100,
	PS2_BUTTON_R2 		= 0x00200,
	PS2_BUTTON_L1  		= 0x00400,
	PS2_BUTTON_R1    	= 0x00800,
	PS2_BUTTON_TRIANGLE = 0x01000,
	PS2_BUTTON_CIRCLE   = 0x02000,
	PS2_BUTTON_CROSS    = 0x04000,
	PS2_BUTTON_SQUARE   = 0x08000,
};

enum GUI_Action {
	GUI_ACTION_NONE,
	GUI_ACTION_UP,
	GUI_ACTION_DOWN,
	GUI_ACTION_LEFT,
	GUI_ACTION_RIGHT,
	GUI_ACTION_ENTER,
	GUI_ACTION_LEAVE,
	GUI_ACTION_ALTERNATE,
};

void init_input();

// 0 if not fast-forwarding.
// Otherwise, the amount of frames to skip per rendered frame.
// 1 amounts to targetting 200% real-time;
// 2 amounts to targetting 300% real-time;
// 3 amounts to targetting 400% real-time;
// 4 amounts to targetting 500% real-time;
// 5 amounts to targetting 600% real-time.
extern uint_fast8_t FastForwardFrameskip;

// The number of times to target while fast-forwarding is enabled. (UI option)
// 0 means 2x, 1 means 3x, ... 4 means 6x.
extern uint32_t PerGameFastForwardTarget;
extern uint32_t FastForwardTarget;

// A value indicating how sensitive the analog stick is to being moved in a
// direction.
// 0 is the least sensitive, requiring the axis to be fully engaged.
// 4 is the most sensitive, requiring the axis to be lightly tapped.
extern uint32_t PerGameAnalogSensitivity;
extern uint32_t AnalogSensitivity;

// A value indicating what the analog stick does in a game.
// 0 means nothing special is done (but hotkeys may use the axes).
// 1 means the analog stick is mapped to the GBA d-pad.
extern uint32_t PerGameRightAnalogAction;
extern uint32_t RightAnalogAction;
extern uint32_t PerGameLeftAnalogAction;
extern uint32_t LeftAnalogAction;

// If this is greater than 0, a frame and its audio are skipped.
// The value then goes to FastForwardFrameskip and is decremented until 0.
extern uint_fast8_t FastForwardFrameskipControl;

/*
 * Reads the buttons pressed at the time of the function call on the input
 * mechanism most appropriate for the port being compiled, and returns a GUI
 * action according to a priority order and the buttons being pressed.
 * Returns:
 *   A single GUI action according to the priority order, or GUI_ACTION_NONE
 *   if either the user is pressing no buttons or a repeat interval has not
 *   yet passed since the last automatic repeat of the same button.
 * Output assertions:
 *   a) The return value is a valid member of 'enum GUI_Action'.
 *   b) The priority order is Enter (sub-menu), Leave, Down, Up, Right, Left,
 *      Alternate.
 *      For example, if the user had pressed Down, and now presses Enter+Down,
 *      Down's repeating is stopped while Enter is held.
 *      If the user had pressed Enter, and now presses Enter+Down, Down is
 *      ignored until Enter is released.
 */
extern enum GUI_Action GetGUIAction();

extern const enum PS2_Buttons DefaultKeypadRemapping[12];

// The OpenDingux_Buttons corresponding to each GBA or ReGBA button.
// [0] = GBA A
// GBA B
// GBA Select
// GBA Start
// GBA D-pad Right
// GBA D-pad Left
// GBA D-pad Up
// GBA D-pad Down
// GBA R trigger
// GBA L trigger
// ReGBA rapid-fire A
// [11] = ReGBA rapid-fire B
extern enum PS2_Buttons PerGameKeypadRemapping[12];
extern enum PS2_Buttons KeypadRemapping[12];

// The OpenDingux_Buttons (as bitfields) for hotkeys.
// [0] = Fast-forward while held
// Menu
// Fast-forward toggle
// Quick load state #1
// [4] = Quick save state #1
extern enum PS2_Buttons PerGameHotkeys[5];
extern enum PS2_Buttons Hotkeys[5];

/*
 * Returns true if the given hotkey is completely impossible to input on the
 * port being compiled.
 * Input:
 *   Hotkey: Bitfield of OPENDINGUX_* buttons to test.
 * Returns:
 *   true if, and only if, any of the following conditions is true:
 *   a) D-pad Up and Down are both in the bitfield;
 *   b) D-pad Left and Right are both in the bitfield;
 *   c) Analog Up and Down are both in the bitfield;
 *   d) Analog Left and Right are both in the bitfield;
 *   e) on the Dingoo A320, any analog direction is in the bitfield.
 */
extern bool IsImpossibleHotkey(enum PS2_Buttons Hotkey);

/*
 * Returns the OpenDingux buttons that are currently pressed.
 */
extern enum PS2_Buttons GetPressedPS2Buttons();

#endif // !defined __OD_INPUT_H__
