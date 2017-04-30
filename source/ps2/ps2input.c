/* Per-platform code - ReGBA on OpenDingux
 *
 * Copyright (C) 2013 Paul Cercueil and Dingoonity user Nebuleon
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

#include "common.h"

uint_fast8_t FastForwardFrameskip = 0;

uint32_t PerGameFastForwardTarget = 0;
uint32_t FastForwardTarget = 4; // 6x by default

uint32_t PerGameAnalogSensitivity = 0;
uint32_t AnalogSensitivity = 0; // require 32256/32767 of the axis by default

uint32_t PerGameLeftAnalogAction = 0;
uint32_t LeftAnalogAction = 0;
uint32_t PerGameRightAnalogAction = 0;
uint32_t RightAnalogAction = 0;

uint_fast8_t FastForwardFrameskipControl = 0;


// These must be PS2 buttons at the bit suitable for the ReGBA_Buttons
// enumeration.

const enum PS2_Buttons DefaultKeypadRemapping[12] = {
	PS2_BUTTON_CROSS, // GBA A
	PS2_BUTTON_CIRCLE,  // GBA B
	PS2_BUTTON_SELECT,     // GBA Select
	PS2_BUTTON_START,      // GBA Start
	PS2_BUTTON_RIGHT,      // GBA D-pad Right
	PS2_BUTTON_LEFT,       // GBA D-pad Left
	PS2_BUTTON_UP,         // GBA D-pad Up
	PS2_BUTTON_DOWN,       // GBA D-pad Down
	PS2_BUTTON_R1,          // GBA R trigger
	PS2_BUTTON_L1,          // GBA L trigger
	0,                            // ReGBA rapid-fire A
	0,                            // ReGBA rapid-fire B
};

// These must be PS2 buttons at the bit suitable for the ReGBA_Buttons
// enumeration.

enum PS2_Buttons PerGameKeypadRemapping[12] = {
	0, // GBA A
	0, // GBA B
	0, // GBA Select
	0, // GBA Start
	0, // GBA D-pad Right
	0, // GBA D-pad Left
	0, // GBA D-pad Up
	0, // GBA D-pad Down
	0, // GBA R trigger
	0, // GBA L trigger
	0, // ReGBA rapid-fire A
	0, // ReGBA rapid-fire B
};

enum PS2_Buttons KeypadRemapping[12] = {
	PS2_BUTTON_CROSS, // GBA A
	PS2_BUTTON_CIRCLE,  // GBA B
	PS2_BUTTON_SELECT,     // GBA Select
	PS2_BUTTON_START,      // GBA Start
	PS2_BUTTON_RIGHT,      // GBA D-pad Right
	PS2_BUTTON_LEFT,       // GBA D-pad Left
	PS2_BUTTON_UP,         // GBA D-pad Up
	PS2_BUTTON_DOWN,       // GBA D-pad Down
	PS2_BUTTON_R1,          // GBA R trigger
	PS2_BUTTON_L1,          // GBA L trigger
	0,                            // ReGBA rapid-fire A
	0,                            // ReGBA rapid-fire B
};

enum PS2_Buttons PerGameHotkeys[5] = {
	0, // Fast-forward while held
	0, // Menu
	0, // Fast-forward toggle
	0, // Quick load state #1
	0, // Quick save state #1
};

enum PS2_Buttons Hotkeys[5] = {
	0,                            // Fast-forward while held
	PS2_BUTTON_TRIANGLE,    // Menu
	0,                            // Fast-forward toggle
	PS2_BUTTON_L2,                            // Quick load state #1
	PS2_BUTTON_R2,                            // Quick save state #1
};

// The menu keys, in decreasing order of priority when two or more are
// pressed. For example, when the user keeps a direction pressed but also
// presses A, start ignoring the direction.
enum PS2_Buttons MenuKeys[7] = {
	PS2_BUTTON_CROSS,                     // Select/Enter button
	PS2_BUTTON_CIRCLE,                      // Cancel/Leave button
	PS2_BUTTON_DOWN , // Menu navigation
	PS2_BUTTON_UP   ,
	PS2_BUTTON_RIGHT ,
	PS2_BUTTON_LEFT  ,
	PS2_BUTTON_SELECT,
};

// In the same order as MenuKeys above. Maps the OpenDingux buttons to their
// corresponding GUI action.
enum GUI_Action MenuKeysToGUI[7] = {
	GUI_ACTION_ENTER,
	GUI_ACTION_LEAVE,
	GUI_ACTION_DOWN,
	GUI_ACTION_UP,
	GUI_ACTION_RIGHT,
	GUI_ACTION_LEFT,
	GUI_ACTION_ALTERNATE,
};

void init_input()
{
	Setup_Pad();
	Wait_Pad_Ready();
}

static enum PS2_Buttons LastButtons = 0, CurButtons = 0;

static bool IsFastForwardToggled = false;
static bool WasFastForwardToggleHeld = false;
static bool WasLoadStateHeld = false;
static bool WasSaveStateHeld = false;

void ProcessSpecialKeys()
{
	enum PS2_Buttons ButtonCopy = LastButtons;

	enum PS2_Buttons FastForwardToggle = ResolveButtons(Hotkeys[2], PerGameHotkeys[2]);
	bool IsFastForwardToggleHeld = FastForwardToggle != 0 && (FastForwardToggle & LastButtons) == FastForwardToggle;
	if (!WasFastForwardToggleHeld && IsFastForwardToggleHeld)
		IsFastForwardToggled = !IsFastForwardToggled;
	WasFastForwardToggleHeld = IsFastForwardToggleHeld;

	// Resolve fast-forwarding. It is activated if it's either toggled by the
	// Toggle hotkey, or the While Held key is held down.
	enum PS2_Buttons FastForwardWhileHeld = ResolveButtons(Hotkeys[0], PerGameHotkeys[0]);
	bool IsFastForwardWhileHeld = FastForwardWhileHeld != 0 && (FastForwardWhileHeld & ButtonCopy) == FastForwardWhileHeld;
	FastForwardFrameskip = (IsFastForwardToggled || IsFastForwardWhileHeld)
		? ResolveSetting(FastForwardTarget, PerGameFastForwardTarget) + 1
		: 0;

	enum PS2_Buttons LoadState = ResolveButtons(Hotkeys[3], PerGameHotkeys[3]);
	bool IsLoadStateHeld = LoadState != 0 && (LoadState & ButtonCopy) == LoadState;
	if (!WasLoadStateHeld && IsLoadStateHeld)
	{
		SetMenuResolution();
		switch (load_state(0))
		{
			case 1:
				if (errno != 0)
					ShowErrorScreen("Reading saved state #1 failed:\n%s", strerror(errno));
				else
					ShowErrorScreen("Reading saved state #1 failed:\nIncomplete file");
				break;

			case 2:
				ShowErrorScreen("Reading saved state #1 failed:\nFile format invalid");
				break;
		}
		SetGameResolution();
	}
	WasLoadStateHeld = IsLoadStateHeld;
	
	enum PS2_Buttons SaveState = ResolveButtons(Hotkeys[4], PerGameHotkeys[4]);
	bool IsSaveStateHeld = SaveState != 0 && (SaveState & ButtonCopy) == SaveState;
	if (!WasSaveStateHeld && IsSaveStateHeld)
	{
		void* Screenshot = copy_screen();
		SetMenuResolution();
		if (Screenshot == NULL)
			ShowErrorScreen("Gathering the screenshot for saved state #1 failed: Memory allocation error");
		else
		{
			uint32_t ret = save_state(0, Screenshot /* preserved screenshot */);
			free(Screenshot);
			if (ret != 1)
			{
				if (errno != 0)
					ShowErrorScreen("Writing saved state #1 failed:\n%s", strerror(errno));
				else
					ShowErrorScreen("Writing saved state #1 failed:\nUnknown error");
			}
		}
		SetGameResolution();
	}
	WasSaveStateHeld = IsSaveStateHeld;
}

enum ReGBA_Buttons ReGBA_GetPressedButtons()
{
	struct padButtonStatus ctrl_data;
	enum ReGBA_Buttons Result = 0;
	uint_fast8_t i;
	enum PS2_Buttons analog_sensitivity = 92 - (ResolveSetting(AnalogSensitivity, PerGameAnalogSensitivity) * 4);
    enum PS2_Buttons inv_analog_sensitivity = 256 - analog_sensitivity;
	
	padRead(0, 0, &ctrl_data);
    CurButtons = (PS2_ALL_BUTTON_MASK ^ ctrl_data.btns);
	
	LastButtons = CurButtons;
	
	ProcessSpecialKeys();
	for(i = 0; i < 12; i++)
	{
		if(LastButtons & ResolveButtons(KeypadRemapping[i], PerGameKeypadRemapping[i]))
		{
			Result |= 1 << (uint_fast16_t) i;
		}
	}
	
	if(ResolveSetting(LeftAnalogAction, PerGameLeftAnalogAction) == 1)
	{
		if(ctrl_data.ljoy_h < analog_sensitivity)
			Result |= REGBA_BUTTON_LEFT;
		if(ctrl_data.ljoy_h > inv_analog_sensitivity)
			Result |= REGBA_BUTTON_RIGHT;
		if(ctrl_data.ljoy_v < analog_sensitivity)
			Result |= REGBA_BUTTON_UP;
		if(ctrl_data.ljoy_v > inv_analog_sensitivity)
			Result |= REGBA_BUTTON_DOWN;
	}	
		
	if(ResolveSetting(RightAnalogAction, PerGameRightAnalogAction) == 1)
	{
		if(ctrl_data.rjoy_h < analog_sensitivity)
			Result |= REGBA_BUTTON_L;
		if(ctrl_data.rjoy_h > inv_analog_sensitivity)
			Result |= REGBA_BUTTON_R;
	}
	else
	{
		tilt_sensor_x = ctrl_data.rjoy_h * 16;
		tilt_sensor_y = ctrl_data.rjoy_v * 16;
	}
	
	
	if((Result & REGBA_BUTTON_LEFT) && (Result & REGBA_BUTTON_RIGHT))
		Result &= ~REGBA_BUTTON_LEFT;
	if((Result & REGBA_BUTTON_UP) && (Result & REGBA_BUTTON_DOWN))
		Result &= ~REGBA_BUTTON_UP;
	
	if(LastButtons == Hotkeys[1])
		Result |= REGBA_BUTTON_MENU;
	
	return Result;
}

bool IsImpossibleHotkey(enum PS2_Buttons Hotkey)
{
	if ((Hotkey & (PS2_BUTTON_LEFT | PS2_BUTTON_RIGHT)) == (PS2_BUTTON_LEFT | PS2_BUTTON_RIGHT))
		return true;
	if ((Hotkey & (PS2_BUTTON_UP | PS2_BUTTON_DOWN)) == (PS2_BUTTON_UP | PS2_BUTTON_DOWN))
		return true;
#if defined DINGOO_A320
	if (Hotkey & (OPENDINGUX_ANALOG_LEFT | OPENDINGUX_ANALOG_RIGHT | OPENDINGUX_ANALOG_UP | OPENDINGUX_ANALOG_DOWN))
		return true;
#elif defined GCW_ZERO
	if ((Hotkey & (OPENDINGUX_ANALOG_LEFT | OPENDINGUX_ANALOG_RIGHT)) == (OPENDINGUX_ANALOG_LEFT | OPENDINGUX_ANALOG_RIGHT))
		return true;
	if ((Hotkey & (OPENDINGUX_ANALOG_UP | OPENDINGUX_ANALOG_DOWN)) == (OPENDINGUX_ANALOG_UP | OPENDINGUX_ANALOG_DOWN))
		return true;
#endif
	return false;
}


enum PS2_Buttons GetPressedPS2Buttons()
{
	//UpdateOpenDinguxButtons();
	struct padButtonStatus ctrl_data;
	
	padRead(0, 0, &ctrl_data);
    CurButtons = (PS2_ALL_BUTTON_MASK ^ ctrl_data.btns);
	
	LastButtons = CurButtons;
	//CurButtons = FutureButtons;

	return LastButtons;// & ~REGBA_BUTTON_MENU;
}

enum GUI_ActionRepeatState
{
  BUTTON_NOT_HELD,
  BUTTON_HELD_INITIAL,
  BUTTON_HELD_REPEAT
};

// Nanoseconds to wait before repeating GUI actions the first time.
static const uint64_t BUTTON_REPEAT_START    = 500000000;
// Nanoseconds to wait before repeating GUI actions subsequent times.
static const uint64_t BUTTON_REPEAT_CONTINUE = 100000000;

static enum GUI_ActionRepeatState ActionRepeatState = BUTTON_NOT_HELD;
static struct timespec LastActionRepeat;
static enum GUI_Action LastAction = GUI_ACTION_NONE;

enum GUI_Action GetGUIAction()
{
	uint_fast8_t i;
	enum GUI_Action Result = GUI_ACTION_NONE;
	struct padButtonStatus ctrl_data;
	
	padRead(0, 0, &ctrl_data);
    CurButtons = (PS2_ALL_BUTTON_MASK ^ ctrl_data.btns);
	
	LastButtons = CurButtons;
	
	enum PS2_Buttons EffectiveButtons = LastButtons;
	// Now get the currently-held button with the highest priority in MenuKeys.
	for(i = 0; i < sizeof(MenuKeys) / sizeof(MenuKeys[0]); i++)
	{
		if ((EffectiveButtons & MenuKeys[i]) != 0)
		{
			Result = MenuKeysToGUI[i];
			break;
		}
	}
	
	struct timespec Now;
    clock_gettime(&Now);
	if (Result == GUI_ACTION_NONE || LastAction != Result || ActionRepeatState == BUTTON_NOT_HELD)
	{
		LastAction = Result;
		LastActionRepeat = Now;
		if (Result != GUI_ACTION_NONE)
			ActionRepeatState = BUTTON_HELD_INITIAL;
		else
			ActionRepeatState = BUTTON_NOT_HELD;
		return Result;
	}

	// We are certain of the following here:
	// Result != GUI_ACTION_NONE && LastAction == Result
	// We need to check how much time has passed since the last repeat.
	struct timespec Difference = TimeDifference(LastActionRepeat, Now);
	uint64_t DiffNanos = (uint64_t) Difference.tv_sec * 1000000000 + Difference.tv_nsec;
	uint64_t RequiredNanos = (ActionRepeatState == BUTTON_HELD_INITIAL)
		? BUTTON_REPEAT_START
		: BUTTON_REPEAT_CONTINUE;

	if (DiffNanos < RequiredNanos)
		return GUI_ACTION_NONE;

	// Here we repeat the action.
	LastActionRepeat = Now;
	ActionRepeatState = BUTTON_HELD_REPEAT;
	return Result;
}
