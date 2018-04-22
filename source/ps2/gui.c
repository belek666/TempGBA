/* ReGBA - In-application menu
 *
 * Copyright (C) 2013 Dingoonity user Nebuleon
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

#define COLOR_BACKGROUND       RGB888_TO_RGB565(  10,   20,   60)
#define COLOR_ERROR_BACKGROUND RGB888_TO_RGB565( 64,   0,   0)
#define COLOR_INACTIVE_TEXT    RGB888_TO_RGB565(255,  255, 0)
#define COLOR_INACTIVE_OUTLINE RGB888_TO_RGB565(0, 0, 20)
#define COLOR_ACTIVE_TEXT      RGB888_TO_RGB565(255,   0,   0)
#define COLOR_ACTIVE_OUTLINE   RGB888_TO_RGB565(0, 0, 50)
#define COLOR_TITLE_TEXT       RGB888_TO_RGB565(255, 255, 128)
#define COLOR_TITLE_OUTLINE    RGB888_TO_RGB565(  0,  0,   50)
#define COLOR_ERROR_TEXT       RGB888_TO_RGB565(255,  64,  64)
#define COLOR_ERROR_OUTLINE    RGB888_TO_RGB565( 80,   0,   0)
#define FRAME_COLOR            RGB888_TO_RGB565(0,200,100)
#define COLOR_FILE			   RGB888_TO_RGB565(0x10,0xFF,0x10);
#define COLOR_FILE_OUTLINE     RGB888_TO_RGB565(0, 0, 0)
#define COLOR_FILE_A		   RGB888_TO_RGB565(0x10,0xA0,0x10);

#define OFFSET 35

// -- Shorthand for creating menu entries --

#define MENU_PER_GAME \
	.DisplayTitleFunction = DisplayPerGameTitleFunction

#define ENTRY_OPTION(_PersistentName, _Name, _Target) \
	.Kind = KIND_OPTION, .PersistentName = _PersistentName, .Name = _Name, .Target = _Target

#define ENTRY_DISPLAY(_Name, _Target, _DisplayType) \
	.Kind = KIND_DISPLAY, .Name = _Name, .Target = _Target, .DisplayType = _DisplayType

#define ENTRY_SUBMENU(_Name, _Target) \
	.Kind = KIND_SUBMENU, .Name = _Name, .Target = _Target

#define ENTRY_MANDATORY_MAPPING \
	.ChoiceCount = 0, \
	.ButtonLeftFunction = NullLeftFunction, .ButtonRightFunction = NullRightFunction, \
	.ButtonEnterFunction = ActionSetMapping, .DisplayValueFunction = DisplayButtonMappingValue, \
	.LoadFunction = LoadMappingFunction, .SaveFunction = SaveMappingFunction

#define ENTRY_OPTIONAL_MAPPING \
	.ChoiceCount = 0, \
	.ButtonLeftFunction = NullLeftFunction, .ButtonRightFunction = NullRightFunction, \
	.ButtonEnterFunction = ActionSetOrClearMapping, .DisplayValueFunction = DisplayButtonMappingValue, \
	.LoadFunction = LoadMappingFunction, .SaveFunction = SaveMappingFunction

#define ENTRY_MANDATORY_HOTKEY \
	.ChoiceCount = 0, \
	.ButtonLeftFunction = NullLeftFunction, .ButtonRightFunction = NullRightFunction, \
	.ButtonEnterFunction = ActionSetHotkey, .DisplayValueFunction = DisplayHotkeyValue, \
	.LoadFunction = LoadHotkeyFunction, .SaveFunction = SaveHotkeyFunction

#define ENTRY_OPTIONAL_HOTKEY \
	.ChoiceCount = 0, \
	.ButtonLeftFunction = NullLeftFunction, .ButtonRightFunction = NullRightFunction, \
	.ButtonEnterFunction = ActionSetOrClearHotkey, .DisplayValueFunction = DisplayHotkeyValue, \
	.LoadFunction = LoadHotkeyFunction, .SaveFunction = SaveHotkeyFunction

// -- Data --

static uint32_t SelectedState = 0;

// -- Forward declarations --

static struct Menu PerGameMainMenu;
static struct Menu DisplayMenu;
static struct Menu InputMenu;
static struct Menu HotkeyMenu;
static struct Menu ErrorScreen;
static struct Menu DebugMenu;

/*
 * A strut is an invisible line that cannot receive the focus, does not
 * display anything and does not act in any way.
 */
static struct MenuEntry Strut;

static void SavedStateUpdatePreview(struct Menu* ActiveMenu);

int main_ret = 0;

static void DrawFrame(int offset)
{
	draw_rect(offset, offset, gsTexture.Width - offset * 2, 1, FRAME_COLOR);
	draw_rect(offset, gsTexture.Height - offset, gsTexture.Width - offset * 2, 1, FRAME_COLOR);
	draw_rect(offset, offset, 1, gsTexture.Height - offset * 2, FRAME_COLOR);
	draw_rect(gsTexture.Width - offset, offset, 1, gsTexture.Height - offset * 2, FRAME_COLOR);
}

static void PrintEntries(char **dirs, char **files, u32 num_files, u32 num_dirs, u32 selected)
{
	u32 num_to_print;
	u32 page, maxpage;
	int i;
	
	u32 num_of_entries = num_files + num_dirs;
	u16 color, ocolor;
		
	if(files == NULL || dirs == NULL || num_of_entries == 0)
	return;
	
	u32 MaxEntryDisplay = ((gsTexture.Height - (OFFSET - 15) * 2 - (5 + 10 + 40 + 10 + 15 + 50)) / 14);
	
	maxpage = num_of_entries / MaxEntryDisplay + ((num_of_entries % MaxEntryDisplay) > 0 ? 1 : 0);
		
	//printf(" %d ", selected);
	
	page = 1;
		
	if(maxpage == 0)
	{
		num_to_print = num_of_entries;
	}
	else
	{
		if(selected > MaxEntryDisplay)
		{
			try:
			page++;
			if(selected > MaxEntryDisplay * page)
			goto try;
		}
			
		if(page == maxpage && (num_of_entries % MaxEntryDisplay) > 0)
			num_to_print = num_of_entries % MaxEntryDisplay;
		else
			num_to_print = MaxEntryDisplay;
	}

	//printf("page %d num_to_print %d\n", page, num_to_print);
		
	for(i = 1; i < num_to_print + 1; i++)
	{
		if(i + (page - 1) * MaxEntryDisplay == selected)
		{
			color = COLOR_ACTIVE_TEXT;
			ocolor = COLOR_ACTIVE_OUTLINE;
			
			//printf("file %s rh %d\n", files[i + (page - 1) * MaxEntryDisplay - num_dirs], GetRenderedHeight(" "));
		}
		else if(i + (page - 1) * MaxEntryDisplay > num_dirs)
		{
			color = COLOR_FILE;
			ocolor = COLOR_FILE_OUTLINE;
		}
		else
		{
			color = COLOR_INACTIVE_TEXT;
			ocolor = COLOR_INACTIVE_OUTLINE;
		}
		
		if(i + (page - 1) * MaxEntryDisplay > num_dirs)
		{
			//strcpy(entry_name, files[i + (page - 1) * MaxEntryDisplay - num_dirs]);
			PrintStringOutline(files[i + (page - 1) * MaxEntryDisplay - num_dirs - 1], color, ocolor, gsTexture.Mem, gsTexture.Width * 2, OFFSET, GetRenderedHeight(" ") * (i + 2) + OFFSET, gsTexture.Width, GetRenderedHeight(" ") + 2, LEFT, TOP);
			//PrintText(files[i + (page - 1) * MaxEntryDisplay - num_dirs], 15, 5 + 10 + 40 + 10 + 15 + i * 14, color);
		}
		else
		{
			//strcpy(entry_name, dirs[i + (page - 1) * MaxEntryDisplay]);
			PrintStringOutline(dirs[i + (page - 1) * MaxEntryDisplay - 1], color, ocolor, gsTexture.Mem, gsTexture.Width * 2, OFFSET, GetRenderedHeight(" ") * (i + 2) + OFFSET, gsTexture.Width, GetRenderedHeight(" ") + 2, LEFT, TOP);
			//PrintText(dirs[i + (page - 1) * MaxEntryDisplay], 15, 5 + 10 + 40 + 10 + 15 + i * 14, color);
		}
		
		//PrintText(entry_name, 15, 5 + 10 + 40 + 10 + 15 + i * 14, color);
		
	}
}

static int sort_function(const void *dest_str_ptr, const void *src_str_ptr)
{
  char *dest_str = *((char **)dest_str_ptr);
  char *src_str = *((char **)src_str_ptr);

  if(src_str[0] == '.')
    return 1;

  if(dest_str[0] == '.')
    return -1;

  return strcasecmp(dest_str, src_str);
}

int load_file(char **wildcards, char *result)
{
	int i;
	PS2DIR *current_dir;
	struct ps2dirent *current_file;
	int return_value = 1;
	char current_dir_name[MAX_PATH];
	char current_dir_short[81];
	u32 current_dir_length;
	u32 total_filenames_allocated;
	u32 total_dirnames_allocated;
	char **file_list;
	char **dir_list;
	u32 num_files;
	u32 num_dirs;
	u32 current_entry;
	char *file_name;
	u32 file_name_length;
	s32 ext_pos = -1;
	u32 repeat;
	enum GUI_Action Action = GUI_ACTION_NONE;
	char *p;
	
	while(return_value == 1)
	{
		total_filenames_allocated = 32;
		total_dirnames_allocated = 32;
		
		file_list = (char **)malloc(sizeof(char *) * 32);
		dir_list = (char **)malloc(sizeof(char *) * 32);
		memset(file_list, 0, sizeof(char *) * 32);
		memset(dir_list, 0, sizeof(char *) * 32);

		num_files = 0;
		num_dirs = 0;
	
		ps2Getcwd(current_dir_name, MAX_PATH);
		current_dir = ps2Opendir(current_dir_name);
		
		do
		{
			if(current_dir)
			current_file = ps2Readdir(current_dir);
			else
			current_file = NULL;
			
			if(current_file)
			{
				file_name = current_file->d_name;
				file_name_length = strlen(file_name);
				
				if((file_name[0] != '.') || (file_name[1] == '.'))
				{
					if(current_dir->d_entry->d_type == DT_DIR)
					{
						dir_list[num_dirs] = (char *)malloc(file_name_length + 1);
						sprintf(dir_list[num_dirs], "%s", file_name);
						num_dirs++;
					}
					else
					{
						// Must match one of the wildcards, also ignore the .
						if(file_name_length >= 4)
						{
							if(file_name[file_name_length - 4] == '.')
							ext_pos = file_name_length - 4;
							else if(file_name[file_name_length - 3] == '.')
							ext_pos = file_name_length - 3;
							else
							ext_pos = 0;

							for(i = 0; wildcards[i] != NULL; i++)
							{
								if(!strcasecmp((file_name + ext_pos), wildcards[i]))
								{
									file_list[num_files] = (char *)malloc(file_name_length + 1);
									sprintf(file_list[num_files], "%s", file_name);
									num_files++;
									break;
								}
							}
						}
					}
				}
				
				if(num_files == total_filenames_allocated)
				{
					file_list = (char **)realloc(file_list, sizeof(char *) *
					total_filenames_allocated * 2);
					memset(file_list + total_filenames_allocated, 0, sizeof(u8 *) * total_filenames_allocated);
					total_filenames_allocated *= 2;
				}

				if(num_dirs == total_dirnames_allocated)
				{
					dir_list = (char **)realloc(dir_list, sizeof(char *) *total_dirnames_allocated * 2);
					memset(dir_list + total_dirnames_allocated, 0, sizeof(char *) * total_dirnames_allocated);
					total_dirnames_allocated *= 2;
				}
			}
			
		} 
		while(current_file);
		
		qsort((void *)file_list, num_files, sizeof(u8 *), sort_function);
		qsort((void *)dir_list, num_dirs, sizeof(u8 *), sort_function);
		
		ps2Closedir(current_dir);

		current_dir_length = strlen(current_dir_name);

		if(current_dir_length > 80)
		{
			memcpy(current_dir_short, "...", 3);
			memcpy(current_dir_short + 3, current_dir_name + current_dir_length - 77, 77);
			current_dir_short[80] = 0;
		}
		else
		{
			memcpy(current_dir_short, current_dir_name, current_dir_length + 1);
		}
		
		repeat = 1;
		current_entry = 0;
		
		clear_screen(COLOR_BACKGROUND);
		Action = GetGUIAction();
		
		while(repeat)
		{
			Action = GetGUIAction();
			
			switch(Action)
			{
				case GUI_ACTION_ENTER:
					if(current_entry <= num_dirs - 1)
					{
						repeat = 0;
						ps2Chdir(dir_list[current_entry]);
					}
					else
					{
						if(num_files != 0)
						{
							repeat = 0;
							return_value = 0;
							
							strcpy(result, current_dir_name);
							
							if((p=strrchr(result, '/'))!=NULL && *(p+1) == 0)
							{
								*p = 0;
							}
							
							sprintf(result, "%s/%s", result, file_list[current_entry - num_dirs]);
						}
					}
					break;
				
				case GUI_ACTION_LEAVE:
				    repeat = 0;
					ps2Chdir("..");
					break;

				case GUI_ACTION_DOWN:
					if(current_entry < (num_dirs + num_files - 1))
					current_entry++;
					break;
					
				case GUI_ACTION_UP:
					if(current_entry > 0)
					current_entry--;
					break;
					
				case GUI_ACTION_RIGHT:
					if(num_dirs + num_files > 5 && current_entry < (num_dirs + num_files - 1 - 5))
					current_entry+=5;
					else
					current_entry = num_dirs + num_files - 1;
					break;
					
				case GUI_ACTION_LEFT:
					if(current_entry > 5)
					current_entry-=5;
					else
					current_entry = 0;
					break;
					break;
					
				case GUI_ACTION_ALTERNATE:
					return_value = -1;
					repeat = 0;
					break;
				
				default: break;
			}
			
			//printf("dirs: %d files: %d entry %d\n", num_dirs, num_files, current_entry);
			
			clear_screen(COLOR_BACKGROUND);
			
			DrawFrame(OFFSET - 15);

			PrintStringOutline("TempGBA Load New Game", COLOR_TITLE_TEXT, COLOR_TITLE_OUTLINE, gsTexture.Mem, gsTexture.Width * 2, OFFSET, GetRenderedHeight(" ") * (0 + 2), gsTexture.Width, GetRenderedHeight(" ") + 2, CENTER, TOP);
			
			PrintStringOutline(current_dir_short, COLOR_ACTIVE_TEXT, COLOR_ACTIVE_OUTLINE, gsTexture.Mem, gsTexture.Width * 2, OFFSET + 5, GetRenderedHeight(" ") * (1 + 2), gsTexture.Width, GetRenderedHeight(" ") + 2, LEFT, TOP);
			
			PrintEntries(dir_list, file_list, num_files , num_dirs, current_entry + 1);
			
			PrintStringOutline("Press SELECT to menu", COLOR_INACTIVE_TEXT, COLOR_INACTIVE_OUTLINE, gsTexture.Mem, gsTexture.Width * 2, OFFSET, gsTexture.Height - OFFSET * 2, gsTexture.Width, GetRenderedHeight(" ") + 2, RIGHT, BOTTOM);
			
			ReGBA_VideoFlip();
			
		}
		
	for(i = 0; i < num_files; i++)
    {
		free(file_list[i]);
    }
    free(file_list);

    for(i = 0; i < num_dirs; i++)
    {
		free(dir_list[i]);
    }
    free(dir_list);
		
	}
	
	clear_screen(COLOR_BACKGROUND);
	return return_value;
}

static int cheats_menu()
{
	int i;
	enum GUI_Action Action = GUI_ACTION_NONE;
	u32 current_entry = 1, repeat = 1;
	u32 MaxEntryDisplay = ((gsTexture.Height - (OFFSET - 15) * 2 - (5 + 10 + 40 + 10 + 15 + 50)) / 14);
	u32 num_to_print;
	u32 page, maxpage;
	u16 color, ocolor;
	int return_value = 1;

	clear_screen(COLOR_BACKGROUND);
	Action = GetGUIAction();
	
	while(repeat)
	{
		Action = GetGUIAction();
			
		switch(Action)
		{
			case GUI_ACTION_ENTER:
				if(g_num_cheats > 0)
				{
					if (current_cheats_flag[current_entry - 1].cheat_active == 1)
						current_cheats_flag[current_entry - 1].cheat_active = 0;
					else
						current_cheats_flag[current_entry - 1].cheat_active = 1;
				}
				break;
				
			case GUI_ACTION_LEAVE:
				repeat = 0;
				break;

			case GUI_ACTION_DOWN:
				if(g_num_cheats > 0)
				{
					if(current_entry < g_num_cheats)
						current_entry++;
				}
				break;
					
			case GUI_ACTION_UP:
				if(current_entry > 1)
					current_entry--;
				break;
					
			case GUI_ACTION_RIGHT:
				if(g_num_cheats > 5 && current_entry < (g_num_cheats - 5))
					current_entry += 5;
				else
					current_entry = g_num_cheats;
				break;
					
			case GUI_ACTION_LEFT:
				if(current_entry > 6)
					current_entry -= 5;
				else
					current_entry = 1;
				break;
					
			case GUI_ACTION_ALTERNATE:
				repeat = 0;
				break;
				
				default: 
					break;
		}
		
		clear_screen(COLOR_BACKGROUND);
		DrawFrame(OFFSET - 15);
		PrintStringOutline("Cheats", COLOR_TITLE_TEXT, COLOR_TITLE_OUTLINE, gsTexture.Mem, gsTexture.Width * 2, OFFSET, GetRenderedHeight(" ") * (0 + 2), gsTexture.Width, GetRenderedHeight(" ") + 2, CENTER, TOP);

		if (g_num_cheats == 0) 
		{
			PrintStringOutline("Cheats file not found.", COLOR_ACTIVE_TEXT, COLOR_ACTIVE_OUTLINE, gsTexture.Mem, gsTexture.Width * 2, OFFSET + 5, GetRenderedHeight(" ") * (1 + 2), gsTexture.Width, GetRenderedHeight(" ") + 2, LEFT, TOP);
			return_value = 0;
		}
		else
		{
			maxpage = g_num_cheats / MaxEntryDisplay + ((g_num_cheats % MaxEntryDisplay) > 0 ? 1 : 0);
			page = 1;
		
			if(maxpage == 0)
			{
				num_to_print = g_num_cheats;
			}
			else
			{
				if(current_entry > MaxEntryDisplay)
				{
					try:
					page++;
					if(current_entry > MaxEntryDisplay * page)
						goto try;
				}
			
				if(page == maxpage && (g_num_cheats % MaxEntryDisplay) > 0)
					num_to_print = g_num_cheats % MaxEntryDisplay;
				else
					num_to_print = MaxEntryDisplay;
			}

			for(i = 1; i < num_to_print + 1; i++)
			{
				if(i + (page - 1) * MaxEntryDisplay == current_entry)
				{
					if(current_cheats_flag[current_entry - 1].cheat_active == 1)
					{
						color = COLOR_FILE_A;
						ocolor = COLOR_FILE_OUTLINE;
					}
					else
					{
						color = COLOR_ACTIVE_TEXT;
						ocolor = COLOR_ACTIVE_OUTLINE;
					}
				}
				else if(current_cheats_flag[i + (page - 1) * MaxEntryDisplay - 1].cheat_active == 1)
				{
					color = COLOR_FILE;
					ocolor = COLOR_FILE_OUTLINE;
				}
				else
				{
					color = COLOR_INACTIVE_TEXT;
					ocolor = COLOR_INACTIVE_OUTLINE;
				}
				
				PrintStringOutline(current_cheats_flag[i + (page - 1) * MaxEntryDisplay - 1].cheat_name, color, ocolor, gsTexture.Mem, gsTexture.Width * 2, OFFSET, GetRenderedHeight(" ") * (i + 2) + OFFSET, gsTexture.Width, GetRenderedHeight(" ") + 2, LEFT, TOP);
			}
		}
		
		ReGBA_VideoFlip();
	}
	
	return return_value;
}

static bool DefaultCanFocusFunction(struct Menu* ActiveMenu, struct MenuEntry* ActiveMenuEntry)
{
	if (ActiveMenuEntry->Kind == KIND_DISPLAY)
		return false;
	return true;
}

static uint32_t FindNullEntry(struct Menu* ActiveMenu)
{
	uint32_t Result = 0;
	while (ActiveMenu->Entries[Result] != NULL)
		Result++;
	return Result;
}

static bool MoveUp(struct Menu* ActiveMenu, uint32_t* ActiveMenuEntryIndex)
{
	if (*ActiveMenuEntryIndex == 0)  // Going over the top?
	{  // Go back to the bottom.
		uint32_t NullEntry = FindNullEntry(ActiveMenu);
		if (NullEntry == 0)
			return false;
		*ActiveMenuEntryIndex = NullEntry - 1;
		return true;
	}
	(*ActiveMenuEntryIndex)--;
	return true;
}

static bool MoveDown(struct Menu* ActiveMenu, uint32_t* ActiveMenuEntryIndex)
{
	if (*ActiveMenuEntryIndex == 0 && ActiveMenu->Entries[*ActiveMenuEntryIndex] == NULL)
		return false;
	if (ActiveMenu->Entries[*ActiveMenuEntryIndex] == NULL)  // Is the sentinel "active"?
		*ActiveMenuEntryIndex = 0;  // Go back to the top.
	(*ActiveMenuEntryIndex)++;
	if (ActiveMenu->Entries[*ActiveMenuEntryIndex] == NULL)  // Going below the bottom?
		*ActiveMenuEntryIndex = 0;  // Go back to the top.
	return true;
}

static void DefaultUpFunction(struct Menu** ActiveMenu, uint32_t* ActiveMenuEntryIndex)
{
	if (MoveUp(*ActiveMenu, ActiveMenuEntryIndex))
	{
		// Keep moving up until a menu entry can be focused.
		uint32_t Sentinel = *ActiveMenuEntryIndex;
		MenuEntryCanFocusFunction CanFocusFunction = (*ActiveMenu)->Entries[*ActiveMenuEntryIndex]->CanFocusFunction;
		if (CanFocusFunction == NULL) CanFocusFunction = DefaultCanFocusFunction;
		while (!(*CanFocusFunction)(*ActiveMenu, (*ActiveMenu)->Entries[*ActiveMenuEntryIndex]))
		{
			MoveUp(*ActiveMenu, ActiveMenuEntryIndex);
			if (*ActiveMenuEntryIndex == Sentinel)
			{
				// If we went through all of them, we cannot focus anything.
				// Place the focus on the NULL entry.
				*ActiveMenuEntryIndex = FindNullEntry(*ActiveMenu);
				return;
			}
			CanFocusFunction = (*ActiveMenu)->Entries[*ActiveMenuEntryIndex]->CanFocusFunction;
			if (CanFocusFunction == NULL) CanFocusFunction = DefaultCanFocusFunction;
		}
	}
}

static void DefaultDownFunction(struct Menu** ActiveMenu, uint32_t* ActiveMenuEntryIndex)
{
	if (MoveDown(*ActiveMenu, ActiveMenuEntryIndex))
	{
		// Keep moving down until a menu entry can be focused.
		uint32_t Sentinel = *ActiveMenuEntryIndex;
		MenuEntryCanFocusFunction CanFocusFunction = (*ActiveMenu)->Entries[*ActiveMenuEntryIndex]->CanFocusFunction;
		if (CanFocusFunction == NULL) CanFocusFunction = DefaultCanFocusFunction;
		while (!(*CanFocusFunction)(*ActiveMenu, (*ActiveMenu)->Entries[*ActiveMenuEntryIndex]))
		{
			MoveDown(*ActiveMenu, ActiveMenuEntryIndex);
			if (*ActiveMenuEntryIndex == Sentinel)
			{
				// If we went through all of them, we cannot focus anything.
				// Place the focus on the NULL entry.
				*ActiveMenuEntryIndex = FindNullEntry(*ActiveMenu);
				return;
			}
			CanFocusFunction = (*ActiveMenu)->Entries[*ActiveMenuEntryIndex]->CanFocusFunction;
			if (CanFocusFunction == NULL) CanFocusFunction = DefaultCanFocusFunction;
		}
	}
}

static void DefaultRightFunction(struct Menu* ActiveMenu, struct MenuEntry* ActiveMenuEntry)
{
	if (ActiveMenuEntry->Kind == KIND_OPTION
	|| (ActiveMenuEntry->Kind == KIND_CUSTOM /* chose to use this function */
	 && ActiveMenuEntry->Target != NULL)
	)
	{
		uint32_t* Target = (uint32_t*) ActiveMenuEntry->Target;
		(*Target)++;
		if (*Target >= ActiveMenuEntry->ChoiceCount)
			*Target = 0;
	}
}

static void DefaultLeftFunction(struct Menu* ActiveMenu, struct MenuEntry* ActiveMenuEntry)
{
	if (ActiveMenuEntry->Kind == KIND_OPTION
	|| (ActiveMenuEntry->Kind == KIND_CUSTOM /* chose to use this function */
	 && ActiveMenuEntry->Target != NULL)
	)
	{
		uint32_t* Target = (uint32_t*) ActiveMenuEntry->Target;
		if (*Target == 0)
			*Target = ActiveMenuEntry->ChoiceCount;
		(*Target)--;
	}
}

static void DefaultEnterFunction(struct Menu** ActiveMenu, uint32_t* ActiveMenuEntryIndex)
{
	if ((*ActiveMenu)->Entries[*ActiveMenuEntryIndex]->Kind == KIND_SUBMENU)
	{
		*ActiveMenu = (struct Menu*) (*ActiveMenu)->Entries[*ActiveMenuEntryIndex]->Target;
	}
}

static void DefaultLeaveFunction(struct Menu** ActiveMenu, uint32_t* ActiveMenuEntryIndex)
{
	*ActiveMenu = (*ActiveMenu)->Parent;
}

static void DefaultDisplayNameFunction(struct MenuEntry* DrawnMenuEntry, struct MenuEntry* ActiveMenuEntry, uint32_t Position)
{
	bool IsActive = (DrawnMenuEntry == ActiveMenuEntry);
	uint16_t TextColor = IsActive ? COLOR_ACTIVE_TEXT : COLOR_INACTIVE_TEXT;
	uint16_t OutlineColor = IsActive ? COLOR_ACTIVE_OUTLINE : COLOR_INACTIVE_OUTLINE;
	PrintStringOutline(DrawnMenuEntry->Name, TextColor, OutlineColor, gsTexture.Mem, gsTexture.Width * 2, OFFSET, GetRenderedHeight(" ") * (Position + 2) + OFFSET, gsTexture.Width, GetRenderedHeight(" ") + 2, LEFT, TOP);
}

static void print_u64(char* Result, uint64_t Value)
{
	if (Value == 0)
		strcpy(Result, "0");
	else
	{
		uint_fast8_t Length = 0;
		uint64_t Temp = Value;
		while (Temp > 0)
		{
			Temp /= 10;
			Length++;
		}
		Result[Length] = '\0';
		while (Value > 0)
		{
			Length--;
			Result[Length] = '0' + (Value % 10);
			Value /= 10;
		}
	}
}

static void print_i64(char* Result, int64_t Value)
{
	if (Value < -9223372036854775807)
		strcpy(Result, "-9223372036854775808");
	else if (Value < 0)
	{
		Result[0] = '-';
		print_u64(Result + 1, (uint64_t) -Value);
	}
	else
		print_u64(Result, (uint64_t) Value);
}

static void DefaultDisplayValueFunction(struct MenuEntry* DrawnMenuEntry, struct MenuEntry* ActiveMenuEntry, uint32_t Position)
{
	if (DrawnMenuEntry->Kind == KIND_OPTION || DrawnMenuEntry->Kind == KIND_DISPLAY)
	{
		char Temp[21];
		Temp[0] = '\0';
		char* Value = Temp;
		bool Error = false;
		if (DrawnMenuEntry->Kind == KIND_OPTION)
		{
			if (*(uint32_t*) DrawnMenuEntry->Target < DrawnMenuEntry->ChoiceCount)
				Value = DrawnMenuEntry->Choices[*(uint32_t*) DrawnMenuEntry->Target].Pretty;
			else
			{
				Value = "Out of bounds";
				Error = true;
			}
		}
		else if (DrawnMenuEntry->Kind == KIND_DISPLAY)
		{
			switch (DrawnMenuEntry->DisplayType)
			{
				case TYPE_STRING:
					Value = (char*) DrawnMenuEntry->Target;
					break;
				case TYPE_INT32:
					sprintf(Temp, "%" PRIi32, *(int32_t*) DrawnMenuEntry->Target);
					break;
				case TYPE_UINT32:
					sprintf(Temp, "%" PRIu32, *(uint32_t*) DrawnMenuEntry->Target);
					break;
				case TYPE_INT64:
					print_i64(Temp, *(int64_t*) DrawnMenuEntry->Target);
					break;
				case TYPE_UINT64:
					print_u64(Temp, *(uint64_t*) DrawnMenuEntry->Target);
					break;
				default:
					Value = "Unknown type";
					Error = true;
					break;
			}
		}
		bool IsActive = (DrawnMenuEntry == ActiveMenuEntry);
		uint16_t TextColor = Error ? COLOR_ERROR_TEXT : (IsActive ? COLOR_ACTIVE_TEXT : COLOR_INACTIVE_TEXT);
		uint16_t OutlineColor = Error ? COLOR_ERROR_OUTLINE : (IsActive ? COLOR_ACTIVE_OUTLINE : COLOR_INACTIVE_OUTLINE);
		PrintStringOutline(Value, TextColor, OutlineColor, gsTexture.Mem, gsTexture.Width * 2, OFFSET, GetRenderedHeight(" ") * (Position + 2) + OFFSET, gsTexture.Width, GetRenderedHeight(" ") + 2, RIGHT, TOP);
	}
}

static void DefaultDisplayBackgroundFunction(struct Menu* ActiveMenu)
{
	/*if (SDL_MUSTLOCK(OutputSurface))
		SDL_UnlockSurface(OutputSurface);*/
	clear_screen(COLOR_BACKGROUND);
	/*if (SDL_MUSTLOCK(OutputSurface))
		SDL_LockSurface(OutputSurface);*/
}

static void DefaultDisplayDataFunction(struct Menu* ActiveMenu, struct MenuEntry* ActiveMenuEntry)
{
	uint32_t DrawnMenuEntryIndex = 0;
	struct MenuEntry* DrawnMenuEntry = ActiveMenu->Entries[0];
	for (; DrawnMenuEntry != NULL; DrawnMenuEntryIndex++, DrawnMenuEntry = ActiveMenu->Entries[DrawnMenuEntryIndex])
	{
		MenuEntryDisplayFunction Function = DrawnMenuEntry->DisplayNameFunction;
		if (Function == NULL) Function = &DefaultDisplayNameFunction;
		(*Function)(DrawnMenuEntry, ActiveMenuEntry, DrawnMenuEntryIndex);

		Function = DrawnMenuEntry->DisplayValueFunction;
		if (Function == NULL) Function = &DefaultDisplayValueFunction;
		(*Function)(DrawnMenuEntry, ActiveMenuEntry, DrawnMenuEntryIndex);

		DrawnMenuEntry++;
	}
}

static void DefaultDisplayTitleFunction(struct Menu* ActiveMenu)
{
	PrintStringOutline(ActiveMenu->Title, COLOR_TITLE_TEXT, COLOR_TITLE_OUTLINE, gsTexture.Mem, gsTexture.Width * 2, OFFSET, OFFSET, gsTexture.Width, gsTexture.Height, CENTER, TOP);
}

static void DisplayPerGameTitleFunction(struct Menu* ActiveMenu)
{
	PrintStringOutline(ActiveMenu->Title, COLOR_TITLE_TEXT, COLOR_TITLE_OUTLINE, gsTexture.Mem, gsTexture.Width * 2, OFFSET, OFFSET, gsTexture.Width, gsTexture.Height, CENTER, TOP);
	char ForGame[MAX_PATH * 2];
	char FileNameNoExt[MAX_PATH + 1];
	GetFileNameNoExtension(FileNameNoExt, CurrentGamePath);
	sprintf(ForGame, "for %s", FileNameNoExt);
	PrintStringOutline(ForGame, COLOR_TITLE_TEXT, COLOR_TITLE_OUTLINE, gsTexture.Mem, gsTexture.Width * 2, OFFSET, GetRenderedHeight(" ") + OFFSET, gsTexture.Width, GetRenderedHeight(" ") + 2, CENTER, TOP);
}

void DefaultLoadFunction(struct MenuEntry* ActiveMenuEntry, char* Value)
{
	uint32_t i;
	for (i = 0; i < ActiveMenuEntry->ChoiceCount; i++)
	{
		if (strcasecmp(ActiveMenuEntry->Choices[i].Persistent, Value) == 0)
		{
			*(uint32_t*) ActiveMenuEntry->Target = i;
			return;
		}
	}
	ReGBA_Trace("W: Value '%s' for option '%s' not valid; ignored", Value, ActiveMenuEntry->PersistentName);
}

void DefaultSaveFunction(struct MenuEntry* ActiveMenuEntry, char* Value)
{
	snprintf(Value, 256, "%s = %s #%s\n", ActiveMenuEntry->PersistentName,
		ActiveMenuEntry->Choices[*((uint32_t*) ActiveMenuEntry->Target)].Persistent,
		ActiveMenuEntry->Choices[*((uint32_t*) ActiveMenuEntry->Target)].Pretty);
}

// -- Custom initialisation/finalisation --

static void SavedStateMenuInit(struct Menu** ActiveMenu)
{
	(*ActiveMenu)->UserData = calloc(GBA_SCREEN_WIDTH * GBA_SCREEN_HEIGHT, sizeof(uint16_t));
	if ((*ActiveMenu)->UserData == NULL)
	{
		ReGBA_Trace("E: Memory allocation error while entering the Saved States menu");
		*ActiveMenu = (*ActiveMenu)->Parent;
	}
	else
		SavedStateUpdatePreview(*ActiveMenu);
}

static void SavedStateMenuEnd(struct Menu* ActiveMenu)
{
	if (ActiveMenu->UserData != NULL)
	{
		free(ActiveMenu->UserData);
		ActiveMenu->UserData = NULL;
	}
}

void ScreenPosLoadFunction(struct MenuEntry* ActiveMenuEntry, char* Value)
{
	uint32_t i;
	for (i = 0; i < ActiveMenuEntry->ChoiceCount; i++)
	{
		//if (strcasecmp(ActiveMenuEntry->Choices[i].Persistent, Value) == 0)
		if(atoi(Value) == i)
		{
			*(uint32_t*) ActiveMenuEntry->Target = i;
			return;
		}
	}
	ReGBA_Trace("W: Value '%s' for option '%s' not valid; ignored", Value, ActiveMenuEntry->PersistentName);
}

void ScreenPosSaveFunction(struct MenuEntry* ActiveMenuEntry, char* Value)
{
	snprintf(Value, 256, "%s = %d #Screen position\n", ActiveMenuEntry->PersistentName,
		*((uint32_t*) ActiveMenuEntry->Target));
}

void ScreenOverSaveFunction(struct MenuEntry* ActiveMenuEntry, char* Value)
{
	snprintf(Value, 256, "%s = %d #Screen overscan\n", ActiveMenuEntry->PersistentName,
		*((uint32_t*) ActiveMenuEntry->Target));
}

// -- Custom display --
/*
#define PAD_SELECT    0x0001
#define PAD_L3        0x0002
#define PAD_R3        0x0004
#define PAD_START     0x0008
#define PAD_UP        0x0010
#define PAD_RIGHT     0x0020
#define PAD_DOWN      0x0040
#define PAD_LEFT      0x0080
#define PAD_L2        0x0100
#define PAD_R2        0x0200
#define PAD_L1        0x0400
#define PAD_R1        0x0800
#define PAD_TRIANGLE  0x1000
#define PAD_CIRCLE    0x2000
#define PAD_CROSS     0x4000
#define PAD_SQUARE    0x8000
*/

static char* PS2ButtonText[PS2_BUTTON_COUNT] = {
	"Select",
	"L3",
	"R3",
	"Start",
	"D-pad Up",
	"D-pad Right",
	"D-pad Down",
	"D-pad Left",
	"L2",
	"R2",
	"L1",
	"R1",
	"Triangle",
	"Circle",
	"Cross",
	"Square",
};

/*
 * Retrieves the button text for a single OpenDingux button.
 * Input:
 *   Button: The single button to describe. If this value is 0, the value is
 *   considered valid and "None" is the description text.
 * Output:
 *   Valid: A pointer to a Boolean variable which is updated with true if
 *     Button was a single button or none, or false otherwise.
 * Returns:
 *   A pointer to a null-terminated string describing Button. This string must
 *   never be freed, as it is statically allocated.
 */
static char* GetButtonText(enum PS2_Buttons Button, bool* Valid)
{
	uint_fast8_t i;
	if (Button == 0)
	{
		*Valid = true;
		return "None";
	}
	else
	{
		for (i = 0; i < PS2_BUTTON_COUNT; i++)
		{
			if (Button == 1 << i)
			{
				*Valid = true;
				return PS2ButtonText[i];
			}
		}
		*Valid = false;
		return "Invalid";
	}
}

/*
 * Retrieves the button text for an OpenDingux button combination.
 * Input:
 *   Button: The buttons to describe. If this value is 0, the description text
 *   is "None". If there are multiple buttons in the bitfield, they are all
 *   added, separated by a '+' character.
 * Output:
 *   Result: A pointer to a buffer which is updated with the description of
 *   the button combination.
 */
static void GetButtonsText(enum PS2_Buttons Buttons, char* Result)
{
	uint_fast8_t i;
	if (Buttons == 0)
	{
		strcpy(Result, "None");
	}
	else
	{
		Result[0] = '\0';
		bool AfterFirst = false;
		for (i = 0; i < PS2_BUTTON_COUNT; i++)
		{
			if ((Buttons & 1 << i) != 0)
			{
				if (AfterFirst)
					strcat(Result, "+");
				AfterFirst = true;
				strcat(Result, PS2ButtonText[i]);
			}
		}
	}
}

static void DisplayButtonMappingValue(struct MenuEntry* DrawnMenuEntry, struct MenuEntry* ActiveMenuEntry, uint32_t Position)
{
	bool Valid;
	char* Value = GetButtonText(*(uint32_t*) DrawnMenuEntry->Target, &Valid);

	bool IsActive = (DrawnMenuEntry == ActiveMenuEntry);
	uint16_t TextColor = Valid ? (IsActive ? COLOR_ACTIVE_TEXT : COLOR_INACTIVE_TEXT) : COLOR_ERROR_TEXT;
	uint16_t OutlineColor = Valid ? (IsActive ? COLOR_ACTIVE_OUTLINE : COLOR_INACTIVE_OUTLINE) : COLOR_ERROR_OUTLINE;
	PrintStringOutline(Value, TextColor, OutlineColor, gsTexture.Mem, gsTexture.Width * 2, OFFSET, GetRenderedHeight(" ") * (Position + 2) + OFFSET, gsTexture.Width, GetRenderedHeight(" ") + 2, RIGHT, TOP);
}

static void DisplayHotkeyValue(struct MenuEntry* DrawnMenuEntry, struct MenuEntry* ActiveMenuEntry, uint32_t Position)
{
	char Value[256];
	GetButtonsText(*(uint32_t*) DrawnMenuEntry->Target, Value);

	bool IsActive = (DrawnMenuEntry == ActiveMenuEntry);
	uint16_t TextColor = IsActive ? COLOR_ACTIVE_TEXT : COLOR_INACTIVE_TEXT;
	uint16_t OutlineColor = IsActive ? COLOR_ACTIVE_OUTLINE : COLOR_INACTIVE_OUTLINE;
	PrintStringOutline(Value, TextColor, OutlineColor, gsTexture.Mem, gsTexture.Width * 2, OFFSET, GetRenderedHeight(" ") * (Position + 2) + OFFSET, gsTexture.Width, GetRenderedHeight(" ") + 2, RIGHT, TOP);
}

static void DisplayErrorBackgroundFunction(struct Menu* ActiveMenu)
{
	clear_screen(COLOR_ERROR_BACKGROUND);
}

static void SavedStateMenuDisplayData(struct Menu* ActiveMenu, struct MenuEntry* ActiveMenuEntry)
{
	PrintStringOutline("Preview", COLOR_INACTIVE_TEXT, COLOR_INACTIVE_OUTLINE, gsTexture.Mem, gsTexture.Width * 2, gsTexture.Width - GBA_SCREEN_WIDTH / 2 + OFFSET, GetRenderedHeight(" ") * 2 + OFFSET, GBA_SCREEN_WIDTH / 2, GetRenderedHeight(" ") + 2, LEFT, TOP);

	/*gba_render_half((uint16_t*) gsTexture.Mem, (uint16_t*) ActiveMenu->UserData,
		gsTexture.Width - GBA_SCREEN_WIDTH / 2 - OFFSET,
		GetRenderedHeight(" ") * 3 + 1 + OFFSET,
		GBA_SCREEN_WIDTH * sizeof(uint16_t),
		gsTexture.Width * 2);*/
	
	blit_to_screen((uint16_t*)ActiveMenu->UserData, GBA_SCREEN_WIDTH, GBA_SCREEN_HEIGHT, gsTexture.Width - GBA_SCREEN_WIDTH - OFFSET, GetRenderedHeight(" ") * 3 + 1 + OFFSET);
	
	DefaultDisplayDataFunction(ActiveMenu, ActiveMenuEntry);
}

static void SavedStateSelectionDisplayValue(struct MenuEntry* DrawnMenuEntry, struct MenuEntry* ActiveMenuEntry, uint32_t Position)
{
	char Value[11];
	sprintf(Value, "%" PRIu32, *(uint32_t*) DrawnMenuEntry->Target + 1);

	bool IsActive = (DrawnMenuEntry == ActiveMenuEntry);
	uint16_t TextColor = IsActive ? COLOR_ACTIVE_TEXT : COLOR_INACTIVE_TEXT;
	uint16_t OutlineColor = IsActive ? COLOR_ACTIVE_OUTLINE : COLOR_INACTIVE_OUTLINE;
	PrintStringOutline(Value, TextColor, OutlineColor, gsTexture.Mem, gsTexture.Width * 2, OFFSET, GetRenderedHeight(" ") * (Position + 2) + OFFSET, gsTexture.Width - GBA_SCREEN_WIDTH / 2 - 16, GetRenderedHeight(" ") + 2, RIGHT, TOP);
}

static void SavedStateUpdatePreview(struct Menu* ActiveMenu)
{
	memset(ActiveMenu->UserData, 0, GBA_SCREEN_WIDTH * GBA_SCREEN_HEIGHT * sizeof(u16));
	char SavedStateFilename[MAX_PATH + 1];
	if (!ReGBA_GetSavedStateFilename(SavedStateFilename, CurrentGamePath, SelectedState))
	{
		ReGBA_Trace("W: Failed to get the name of saved state #%d for '%s'", SelectedState, CurrentGamePath);
		return;
	}

	FILE_TAG_TYPE fp;
	FILE_OPEN(fp, SavedStateFilename, READ);
	if (!FILE_CHECK_VALID(fp))
		return;

	FILE_SEEK(fp, SVS_HEADER_SIZE + sizeof(struct ReGBA_RTC), SEEK_SET);

	FILE_READ(fp, ActiveMenu->UserData, GBA_SCREEN_WIDTH * GBA_SCREEN_HEIGHT * sizeof(u16));

	FILE_CLOSE(fp);
}

static void ScreenPosDisplayValue(struct MenuEntry* DrawnMenuEntry, struct MenuEntry* ActiveMenuEntry, uint32_t Position)
{
	char Value[11];
	sprintf(Value, "%" PRIu32, *(uint32_t*) DrawnMenuEntry->Target + 1);

	bool IsActive = (DrawnMenuEntry == ActiveMenuEntry);
	uint16_t TextColor = IsActive ? COLOR_ACTIVE_TEXT : COLOR_INACTIVE_TEXT;
	uint16_t OutlineColor = IsActive ? COLOR_ACTIVE_OUTLINE : COLOR_INACTIVE_OUTLINE;
	PrintStringOutline(Value, TextColor, OutlineColor, gsTexture.Mem, gsTexture.Width * 2, OFFSET, GetRenderedHeight(" ") * (Position + 2) + OFFSET, gsTexture.Width - GBA_SCREEN_WIDTH / 2 - 16, GetRenderedHeight(" ") + 2, RIGHT, TOP);

	if(ScreenPosX != CurrentScreenPosX || ScreenPosY != CurrentScreenPosY)
	{
		gsKit_set_display_offset(gsGlobal, ScreenPosX - gsGlobal->StartX, ScreenPosY - gsGlobal->StartY);
		
		CurrentScreenPosX = ScreenPosX;
    	CurrentScreenPosY = ScreenPosY;
	}
	
	/*if (ScreenOverscan != CurrentScreenOverscan) {
		CurrentScreenOverscan = ScreenOverscan;
	}*/
}

// -- Custom saving --
/*
#define PAD_SELECT    0x0001
#define PAD_L3        0x0002
#define PAD_R3        0x0004
#define PAD_START     0x0008
#define PAD_UP        0x0010
#define PAD_RIGHT     0x0020
#define PAD_DOWN      0x0040
#define PAD_LEFT      0x0080
#define PAD_L2        0x0100
#define PAD_R2        0x0200
#define PAD_L1        0x0400
#define PAD_R1        0x0800
#define PAD_TRIANGLE  0x1000
#define PAD_CIRCLE    0x2000
#define PAD_CROSS     0x4000
#define PAD_SQUARE    0x8000
*/

static char PS2ButtonSave[PS2_BUTTON_COUNT] = {
	'L',
	'R',
	'v', // D-pad directions.
	'^',
	'<',
	'>', // (end)
	'S',
	's',
	'B',
	'A',
	'Y', // Using the SNES/DS/A320 mapping, this is the left face button.
	'X', // Using the SNES/DS/A320 mapping, this is the upper face button.
	'd', // Analog nub directions (GCW Zero).
	'u',
	'l',
	'r', // (end)
};

static void LoadMappingFunction(struct MenuEntry* ActiveMenuEntry, char* Value)
{
	uint32_t Mapping = 0;
	if (Value[0] != 'x')
	{
		uint_fast8_t i;
		for (i = 0; i < PS2_BUTTON_COUNT; i++)
			if (Value[0] == PS2ButtonSave[i])
			{
				Mapping = 1 << i;
				break;
			}
	}
	*(uint32_t*) ActiveMenuEntry->Target = Mapping;
}

static void SaveMappingFunction(struct MenuEntry* ActiveMenuEntry, char* Value)
{
	char Temp[32];
	Temp[0] = '\0';
	uint_fast8_t i;
	for (i = 0; i < PS2_BUTTON_COUNT; i++)
		if (*(uint32_t*) ActiveMenuEntry->Target == 1 << i)
		{
			Temp[0] = PS2ButtonSave[i];
			sprintf(&Temp[1], " #%s", PS2ButtonText[i]);
			break;
		}
	if (Temp[0] == '\0')
		strcpy(Temp, "x #None");
	snprintf(Value, 256, "%s = %s\n", ActiveMenuEntry->PersistentName, Temp);
}

static void LoadHotkeyFunction(struct MenuEntry* ActiveMenuEntry, char* Value)
{
	uint32_t Hotkey = 0;
	if (Value[0] != 'x')
	{
		char* Ptr = Value;
		while (*Ptr)
		{
			uint_fast8_t i;
			for (i = 0; i < PS2_BUTTON_COUNT; i++)
				if (*Ptr == PS2ButtonSave[i])
				{
					Hotkey |= 1 << i;
					break;
				}
			Ptr++;
		}
	}
	*(uint32_t*) ActiveMenuEntry->Target = Hotkey;
}

static void SaveHotkeyFunction(struct MenuEntry* ActiveMenuEntry, char* Value)
{
	char Temp[192];
	char* Ptr = Temp;
	uint_fast8_t i;
	for (i = 0; i < PS2_BUTTON_COUNT; i++)
		if ((*(uint32_t*) ActiveMenuEntry->Target & (1 << i)) != 0)
		{
			*Ptr++ = PS2ButtonSave[i];
		}
	if (Ptr == Temp)
		strcpy(Temp, "x #None");
	else
	{
		*Ptr++ = ' ';
		*Ptr++ = '#';
		GetButtonsText(*(uint32_t*) ActiveMenuEntry->Target, Ptr);
	}
	snprintf(Value, 256, "%s = %s\n", ActiveMenuEntry->PersistentName, Temp);
}

// -- Custom actions --

static void ActionExit(struct Menu** ActiveMenu, uint32_t* ActiveMenuEntryIndex)
{
	// Please ensure that the Main Menu itself does not have entries of type
	// KIND_OPTION. The on-demand writing of settings to storage will not
	// occur after quit(), since it acts after the action function returns.
	quit();
	*ActiveMenu = NULL;
}

static void ActionReturn(struct Menu** ActiveMenu, uint32_t* ActiveMenuEntryIndex)
{
	*ActiveMenu = NULL;
}

static void ActionReset(struct Menu** ActiveMenu, uint32_t* ActiveMenuEntryIndex)
{
	reset_gba();
	reg[CHANGED_PC_STATUS] = 1;
	*ActiveMenu = NULL;
}

static void ActionLoadGame(struct Menu** ActiveMenu, uint32_t* ActiveMenuEntryIndex)
{
    char *file_ext[] = { ".gba", ".bin", ".zip", NULL };
    char load_filename[MAX_FILE];
	
	if(load_file(file_ext, load_filename) != -1)
    {
       if(load_gamepak(load_filename) == -1)
       {
       	    ShowErrorScreen("Failed to load gamepak %s, back to menu.\n", load_filename);
        	ps2delay(5);
        	//quit();
       }
       else
       {
			if (IsGameLoaded)
			{
				char FileNameNoExt[MAX_PATH + 1];
				GetFileNameNoExtension(FileNameNoExt, CurrentGamePath);
#ifdef CHEATS
				add_cheats(FileNameNoExt);
#endif
				ReGBA_LoadSettings(FileNameNoExt, true);
			}
       	
       	    init_cpu(ResolveSetting(BootFromBIOS, PerGameBootFromBIOS));
	   
	   		*ActiveMenu = NULL;
	   		main_ret = 0;
	   }
    }
}

uint32_t PerGameCheatsSettings = 0;

static void ActionCheatsMenu(struct Menu** ActiveMenu, uint32_t* ActiveMenuEntryIndex)
{
	int ret;
	
	ret = cheats_menu();
	
	if(ret)
	{
		if (PerGameCheatsSettings)
			PerGameCheatsSettings = 0;
		else
			PerGameCheatsSettings = 1;
	}
}

void CheatsMenuLoadFunction(struct MenuEntry* ActiveMenuEntry, char* Value)
{
	int i;
	u32 CheatsSettings = atoi(Value);
	
	if (g_num_cheats > 0)
	{
		for(i = 0; i < 32; i++)
			current_cheats_flag[i].cheat_active = CheatsSettings >> i & 1;
	}
}

void CheatsMenuSaveFunction(struct MenuEntry* ActiveMenuEntry, char* Value)
{
	int i;
	u32 CheatsSettings = 0;
	
	if (g_num_cheats > 0)
	{
		for(i = 0; i < 32; i++)
			CheatsSettings |= current_cheats_flag[i].cheat_active << i;
	}
	
	snprintf(Value, 256, "%s = %d #cheats\n", ActiveMenuEntry->PersistentName, CheatsSettings);
}

static void NullLeftFunction(struct Menu* ActiveMenu, struct MenuEntry* ActiveMenuEntry)
{
}

static void NullRightFunction(struct Menu* ActiveMenu, struct MenuEntry* ActiveMenuEntry)
{
}

static enum PS2_Buttons GrabButton(struct Menu* ActiveMenu, char* Text)
{
	MenuFunction DisplayBackgroundFunction = ActiveMenu->DisplayBackgroundFunction;
	if (DisplayBackgroundFunction == NULL) DisplayBackgroundFunction = DefaultDisplayBackgroundFunction;
	enum PS2_Buttons Buttons;
	// Wait for the buttons that triggered the action to be released.
	while (GetPressedPS2Buttons() != 0)
	{
		(*DisplayBackgroundFunction)(ActiveMenu);
		ReGBA_VideoFlip();
		//usleep(5000); // for platforms that don't sync their flips
	}
	// Wait until a button is pressed.
	while ((Buttons = GetPressedPS2Buttons()) == 0)
	{
		(*DisplayBackgroundFunction)(ActiveMenu);
		PrintStringOutline(Text, COLOR_ACTIVE_TEXT, COLOR_ACTIVE_OUTLINE, gsTexture.Mem, gsTexture.Width * 2, OFFSET, OFFSET, gsTexture.Width, gsTexture.Height, CENTER, MIDDLE);
		ReGBA_VideoFlip();
		//usleep(5000); // for platforms that don't sync their flips
	}
	// Accumulate buttons until they're all released.
	enum PS2_Buttons ButtonTotal = Buttons;
	while ((Buttons = GetPressedPS2Buttons()) != 0)
	{
		ButtonTotal |= Buttons;
		(*DisplayBackgroundFunction)(ActiveMenu);
		PrintStringOutline(Text, COLOR_ACTIVE_TEXT, COLOR_ACTIVE_OUTLINE, gsTexture.Mem, gsTexture.Width * 2, OFFSET, OFFSET, gsTexture.Width, gsTexture.Height, CENTER, MIDDLE);
		ReGBA_VideoFlip();
		//usleep(5000); // for platforms that don't sync their flips
	}
	return ButtonTotal;
}

static enum PS2_Buttons GrabButtons(struct Menu* ActiveMenu, char* Text)
{
	MenuFunction DisplayBackgroundFunction = ActiveMenu->DisplayBackgroundFunction;
	if (DisplayBackgroundFunction == NULL) DisplayBackgroundFunction = DefaultDisplayBackgroundFunction;
	enum PS2_Buttons Buttons;
	// Wait for the buttons that triggered the action to be released.
	while (GetPressedPS2Buttons() != 0)
	{
		(*DisplayBackgroundFunction)(ActiveMenu);
		ReGBA_VideoFlip();
		//usleep(5000); // for platforms that don't sync their flips
	}
	// Wait until a button is pressed.
	while ((Buttons = GetPressedPS2Buttons()) == 0)
	{
		(*DisplayBackgroundFunction)(ActiveMenu);
		PrintStringOutline(Text, COLOR_ACTIVE_TEXT, COLOR_ACTIVE_OUTLINE, gsTexture.Mem, gsTexture.Width * 2, OFFSET, OFFSET, gsTexture.Width, gsTexture.Height, CENTER, MIDDLE);
		ReGBA_VideoFlip();
		//usleep(5000); // for platforms that don't sync their flips
	}
	// Accumulate buttons until they're all released.
	enum PS2_Buttons ButtonTotal = Buttons;
	while ((Buttons = GetPressedPS2Buttons()) != 0)
	{
		// a) If the old buttons are a strict subset of the new buttons,
		//    add the new buttons.
		if ((Buttons | ButtonTotal) == Buttons)
			ButtonTotal |= Buttons;
		// b) If the new buttons are a strict subset of the old buttons,
		//    do nothing. (The user is releasing the buttons to return.)
		else if ((Buttons | ButtonTotal) == ButtonTotal)
			;
		// c) If the new buttons are on another path, replace the buttons
		//    completely, for example, R+X turning into R+Y.
		else
			ButtonTotal = Buttons;
		(*DisplayBackgroundFunction)(ActiveMenu);
		PrintStringOutline(Text, COLOR_ACTIVE_TEXT, COLOR_ACTIVE_OUTLINE, gsTexture.Mem, gsTexture.Width * 2, OFFSET, OFFSET, gsTexture.Width, gsTexture.Height, CENTER, MIDDLE);
		ReGBA_VideoFlip();
		//usleep(5000); // for platforms that don't sync their flips
	}
	return ButtonTotal;
}

void ShowErrorScreen(const char* Format, ...)
{
	char* line = malloc(82);
	va_list args;
	int linelen;

	va_start(args, Format);
	if ((linelen = vsnprintf(line, 82, Format, args)) >= 82)
	{
		va_end(args);
		va_start(args, Format);
		free(line);
		line = malloc(linelen + 1);
		vsnprintf(line, linelen + 1, Format, args);
	}
	va_end(args);
	GrabButton(&ErrorScreen, line);
	free(line);
}

static enum PS2_Buttons GrabYesOrNo(struct Menu* ActiveMenu, char* Text)
{
	return GrabButtons(ActiveMenu, Text) == PS2_BUTTON_CROSS;
}

static void ActionSetMapping(struct Menu** ActiveMenu, uint32_t* ActiveMenuEntryIndex)
{
	char Text[256];
	bool Valid;
	sprintf(Text, "Setting binding for %s\nCurrently %s\n"
		"Press the new button or two at once to leave the binding alone.",
		(*ActiveMenu)->Entries[*ActiveMenuEntryIndex]->Name,
		GetButtonText(*(uint32_t*) (*ActiveMenu)->Entries[*ActiveMenuEntryIndex]->Target, &Valid));

	enum PS2_Buttons ButtonTotal = GrabButton(*ActiveMenu, Text);
	// If there's more than one button, change nothing.
	uint_fast8_t BitCount = 0, i;
	for (i = 0; i < PS2_BUTTON_COUNT; i++)
		if ((ButtonTotal & (1 << i)) != 0)
			BitCount++;
	if (BitCount == 1)
		*(uint32_t*) (*ActiveMenu)->Entries[*ActiveMenuEntryIndex]->Target = ButtonTotal;
}

static void ActionSetOrClearMapping(struct Menu** ActiveMenu, uint32_t* ActiveMenuEntryIndex)
{
	char Text[256];
	bool Valid;
	sprintf(Text, "Setting binding for %s\nCurrently %s\n"
		"Press the new button or two at once to clear the binding.",
		(*ActiveMenu)->Entries[*ActiveMenuEntryIndex]->Name,
		GetButtonText(*(uint32_t*) (*ActiveMenu)->Entries[*ActiveMenuEntryIndex]->Target, &Valid));

	enum PS2_Buttons ButtonTotal = GrabButton(*ActiveMenu, Text);
	// If there's more than one button, clear the mapping.
	uint_fast8_t BitCount = 0, i;
	for (i = 0; i < PS2_BUTTON_COUNT; i++)
		if ((ButtonTotal & (1 << i)) != 0)
			BitCount++;
	*(uint32_t*) (*ActiveMenu)->Entries[*ActiveMenuEntryIndex]->Target = (BitCount == 1)
		? ButtonTotal
		: 0;
}

static void ActionSetHotkey(struct Menu** ActiveMenu, uint32_t* ActiveMenuEntryIndex)
{
	char Text[256];
	char Current[256];
	GetButtonsText(*(uint32_t*) (*ActiveMenu)->Entries[*ActiveMenuEntryIndex]->Target, Current);
	sprintf(Text, "Setting hotkey binding for %s\nCurrently %s\n"
		"Press the new buttons or B to leave the hotkey binding alone.",
		(*ActiveMenu)->Entries[*ActiveMenuEntryIndex]->Name,
		Current);

	enum PS2_Buttons ButtonTotal = GrabButtons(*ActiveMenu, Text);
	if (ButtonTotal != PS2_BUTTON_CIRCLE)
		*(uint32_t*) (*ActiveMenu)->Entries[*ActiveMenuEntryIndex]->Target = ButtonTotal;
}

static void ActionSetOrClearHotkey(struct Menu** ActiveMenu, uint32_t* ActiveMenuEntryIndex)
{
	char Text[256];
	char Current[256];
	GetButtonsText(*(uint32_t*) (*ActiveMenu)->Entries[*ActiveMenuEntryIndex]->Target, Current);
	sprintf(Text, "Setting hotkey binding for %s\nCurrently %s\n"
		"Press the new buttons or B to clear the hotkey binding.",
		(*ActiveMenu)->Entries[*ActiveMenuEntryIndex]->Name,
		Current);

	enum PS2_Buttons ButtonTotal = GrabButtons(*ActiveMenu, Text);
	*(uint32_t*) (*ActiveMenu)->Entries[*ActiveMenuEntryIndex]->Target = (ButtonTotal == PS2_BUTTON_CIRCLE)
		? 0
		: ButtonTotal;
}

static void SavedStateSelectionLeft(struct Menu* ActiveMenu, struct MenuEntry* ActiveMenuEntry)
{
	DefaultLeftFunction(ActiveMenu, ActiveMenuEntry);
	SavedStateUpdatePreview(ActiveMenu);
}

static void SavedStateSelectionRight(struct Menu* ActiveMenu, struct MenuEntry* ActiveMenuEntry)
{
	DefaultRightFunction(ActiveMenu, ActiveMenuEntry);
	SavedStateUpdatePreview(ActiveMenu);
}

static void ActionSavedStateRead(struct Menu** ActiveMenu, uint32_t* ActiveMenuEntryIndex)
{
	switch (load_state(SelectedState))
	{
		case 0:
			*ActiveMenu = NULL;
			break;

		case 1:
			if (errno != 0)
				ShowErrorScreen("Reading saved state #%" PRIu32 " failed:\n%s", SelectedState + 1, strerror(errno));
			else
				ShowErrorScreen("Reading saved state #%" PRIu32 " failed:\nIncomplete file", SelectedState + 1);
			break;

		case 2:
			ShowErrorScreen("Reading saved state #%" PRIu32 " failed:\nFile format invalid", SelectedState + 1);
			break;
	}
}

static void ActionSavedStateWrite(struct Menu** ActiveMenu, uint32_t* ActiveMenuEntryIndex)
{
	// 1. If the file already exists, ask the user if s/he wants to overwrite it.
	char SavedStateFilename[MAX_PATH + 1];
	if (!ReGBA_GetSavedStateFilename(SavedStateFilename, CurrentGamePath, SelectedState))
	{
		ReGBA_Trace("W: Failed to get the name of saved state #%d for '%s'", SelectedState, CurrentGamePath);
		ShowErrorScreen("Preparing to write saved state #%" PRIu32 " failed:\nPath too long", SelectedState + 1);
		return;
	}
	FILE_TAG_TYPE dummy;
	FILE_OPEN(dummy, SavedStateFilename, READ);
	if (FILE_CHECK_VALID(dummy))
	{
		FILE_CLOSE(dummy);
		char Temp[1024];
		sprintf(Temp, "Do you want to overwrite saved state #%" PRIu32 "?\n[A] = Yes  Others = No", SelectedState + 1);
		if (!GrabYesOrNo(*ActiveMenu, Temp))
			return;
	}
	
	// 2. If the file didn't exist or the user wanted to overwrite it, save.
	uint32_t ret = save_state(SelectedState, MainMenu.UserData /* preserved screenshot */);
	if (ret != 1)
	{
		if (errno != 0)
			ShowErrorScreen("Writing saved state #%" PRIu32 " failed:\n%s", SelectedState + 1, strerror(errno));
		else
			ShowErrorScreen("Writing saved state #%" PRIu32 " failed:\nUnknown error", SelectedState + 1);
	}
	SavedStateUpdatePreview(*ActiveMenu);
}

static void ActionSavedStateDelete(struct Menu** ActiveMenu, uint32_t* ActiveMenuEntryIndex)
{
	// 1. Ask the user if s/he wants to delete this saved state.
	char SavedStateFilename[MAX_PATH + 1];
	if (!ReGBA_GetSavedStateFilename(SavedStateFilename, CurrentGamePath, SelectedState))
	{
		ReGBA_Trace("W: Failed to get the name of saved state #%d for '%s'", SelectedState, CurrentGamePath);
		ShowErrorScreen("Preparing to delete saved state #%" PRIu32 " failed:\nPath too long", SelectedState + 1);
		return;
	}
	char Temp[1024];
	sprintf(Temp, "Do you want to delete saved state #%" PRIu32 "?\n[A] = Yes  Others = No", SelectedState + 1);
	if (!GrabYesOrNo(*ActiveMenu, Temp))
		return;
	
	// 2. If the user wants to, delete the saved state.
	if (FILE_DELETE(SavedStateFilename) != 0)
	{
		ShowErrorScreen("Deleting saved state #%" PRIu32 " failed:\n%s", SelectedState + 1, strerror(errno));
	}
	SavedStateUpdatePreview(*ActiveMenu);
}

static void ActionShowVersion(struct Menu** ActiveMenu, uint32_t* ActiveMenuEntryIndex)
{
	char Text[1024];
#ifdef GIT_VERSION_STRING
	sprintf(Text, "TempGBA version %s\nNebuleon/TempGBA commit %s", REGBA_VERSION_STRING, GIT_VERSION_STRING);
#else
	sprintf(Text, "TempGBA version %s", REGBA_VERSION_STRING);
#endif
	GrabButtons(*ActiveMenu, Text);
}

static void ActionVideoMode(struct Menu** ActiveMenu, uint32_t* ActiveMenuEntryIndex)
{
	//char Text[1024];

	if(OldVideoMode == ResolveSetting(VideoMode, PerGameVideoMode))
	return;
	
	reload_video();
	
	ScreenPosX = gsGlobal->StartX;
	ScreenPosY = gsGlobal->StartY;
	
	gsKit_set_display_offset(gsGlobal, ScreenPosX - gsGlobal->StartX, ScreenPosY - gsGlobal->StartY);
		
	CurrentScreenPosX = ScreenPosX;
    CurrentScreenPosY = ScreenPosY;
	
	//sprintf(Temp, "Do you want to delete saved state #%" PRIu32 "?\n[A] = Yes  Others = No", SelectedState + 1);
	//if (!GrabYesOrNo(*ActiveMenu, Temp))
		return;

}

static void ActionMenuRes(struct Menu** ActiveMenu, uint32_t* ActiveMenuEntryIndex)
{
	SetMenuResolution();
}

static void ActionScreenPosSetDefault(struct Menu** ActiveMenu, uint32_t* ActiveMenuEntryIndex)
{
	ScreenPosX = gsGlobal->StartX;
	ScreenPosY = gsGlobal->StartY;
	ScreenOverscanX = 99;
	ScreenOverscanY = 99;
}

// -- Strut --

static bool CanNeverFocusFunction(struct Menu* ActiveMenu, struct MenuEntry* ActiveMenuEntry)
{
	return false;
}

static struct MenuEntry Strut = {
	.Kind = KIND_CUSTOM, .Name = "",
	.CanFocusFunction = CanNeverFocusFunction
};

// -- Debug > Native code stats --

static struct MenuEntry NativeCodeMenu_ROPeak = {
	ENTRY_DISPLAY("Read-only bytes at peak", &Stats.TranslationBytesPeak[TRANSLATION_REGION_READONLY], TYPE_UINT64)
};

static struct MenuEntry NativeCodeMenu_RWPeak = {
	ENTRY_DISPLAY("Writable bytes at peak", &Stats.TranslationBytesPeak[TRANSLATION_REGION_WRITABLE], TYPE_UINT64)
};

static struct MenuEntry NativeCodeMenu_ROFlushed = {
	ENTRY_DISPLAY("Read-only bytes flushed", &Stats.TranslationBytesFlushed[TRANSLATION_REGION_READONLY], TYPE_UINT64)
};

static struct MenuEntry NativeCodeMenu_RWFlushed = {
	ENTRY_DISPLAY("Writable bytes flushed", &Stats.TranslationBytesFlushed[TRANSLATION_REGION_WRITABLE], TYPE_UINT64)
};

static struct Menu NativeCodeMenu = {
	.Parent = &DebugMenu, .Title = "Native code statistics",
	.Entries = { &NativeCodeMenu_ROPeak, &NativeCodeMenu_RWPeak, &NativeCodeMenu_ROFlushed, &NativeCodeMenu_RWFlushed, NULL }
};

static struct MenuEntry DebugMenu_NativeCode = {
	ENTRY_SUBMENU("Native code statistics...", &NativeCodeMenu)
};

// -- Debug > Metadata stats --

static struct MenuEntry MetadataMenu_ROFull = {
	ENTRY_DISPLAY("Read-only area full", &Stats.TranslationFlushCount[TRANSLATION_REGION_READONLY][FLUSH_REASON_FULL_CACHE], TYPE_UINT64)
};

static struct MenuEntry MetadataMenu_RWFull = {
	ENTRY_DISPLAY("Writable area full", &Stats.TranslationFlushCount[TRANSLATION_REGION_WRITABLE][FLUSH_REASON_FULL_CACHE], TYPE_UINT64)
};

static struct MenuEntry MetadataMenu_BIOSLastTag = {
	ENTRY_DISPLAY("BIOS tags full", &Stats.MetadataClearCount[METADATA_AREA_BIOS][CLEAR_REASON_LAST_TAG], TYPE_UINT64)
};

static struct MenuEntry MetadataMenu_EWRAMLastTag = {
	ENTRY_DISPLAY("EWRAM tags full", &Stats.MetadataClearCount[METADATA_AREA_EWRAM][CLEAR_REASON_LAST_TAG], TYPE_UINT64)
};

static struct MenuEntry MetadataMenu_IWRAMLastTag = {
	ENTRY_DISPLAY("IWRAM tags full", &Stats.MetadataClearCount[METADATA_AREA_IWRAM][CLEAR_REASON_LAST_TAG], TYPE_UINT64)
};

static struct MenuEntry MetadataMenu_VRAMLastTag = {
	ENTRY_DISPLAY("VRAM tags full", &Stats.MetadataClearCount[METADATA_AREA_VRAM][CLEAR_REASON_LAST_TAG], TYPE_UINT64)
};

static struct MenuEntry MetadataMenu_PartialClears = {
	ENTRY_DISPLAY("Partial clears", &Stats.PartialFlushCount, TYPE_UINT64)
};

static struct Menu MetadataMenu = {
	.Parent = &DebugMenu, .Title = "Metadata clear statistics",
	.Entries = { &MetadataMenu_ROFull, &MetadataMenu_RWFull, &MetadataMenu_BIOSLastTag, &MetadataMenu_EWRAMLastTag, &MetadataMenu_IWRAMLastTag, &MetadataMenu_VRAMLastTag, &MetadataMenu_PartialClears, NULL }
};

static struct MenuEntry DebugMenu_Metadata = {
	ENTRY_SUBMENU("Metadata clear statistics...", &MetadataMenu)
};

// -- Debug > Execution stats --

static struct MenuEntry ExecutionMenu_SoundUnderruns = {
	ENTRY_DISPLAY("Sound buffer underruns", &Stats.SoundBufferUnderrunCount, TYPE_UINT64)
};

static struct MenuEntry ExecutionMenu_FramesEmulated = {
	ENTRY_DISPLAY("Frames emulated", &Stats.TotalEmulatedFrames, TYPE_UINT64)
};

static struct MenuEntry ExecutionMenu_FramesRendered = {
	ENTRY_DISPLAY("Frames rendered", &Stats.TotalRenderedFrames, TYPE_UINT64)
};

#ifdef PERFORMANCE_IMPACTING_STATISTICS
static struct MenuEntry ExecutionMenu_ARMOps = {
	ENTRY_DISPLAY("ARM opcodes decoded", &Stats.ARMOpcodesDecoded, TYPE_UINT64)
};

static struct MenuEntry ExecutionMenu_ThumbOps = {
	ENTRY_DISPLAY("Thumb opcodes decoded", &Stats.ThumbOpcodesDecoded, TYPE_UINT64)
};

static struct MenuEntry ExecutionMenu_MemAccessors = {
	ENTRY_DISPLAY("Memory accessors patched", &Stats.WrongAddressLineCount, TYPE_UINT32)
};
#endif

static struct Menu ExecutionMenu = {
	.Parent = &DebugMenu, .Title = "Execution statistics",
	.Entries = { &ExecutionMenu_SoundUnderruns, &ExecutionMenu_FramesEmulated, &ExecutionMenu_FramesRendered
#ifdef PERFORMANCE_IMPACTING_STATISTICS
	, &ExecutionMenu_ARMOps, &ExecutionMenu_ThumbOps, &ExecutionMenu_MemAccessors
#endif
	, NULL }
};

static struct MenuEntry DebugMenu_Execution = {
	ENTRY_SUBMENU("Execution statistics...", &ExecutionMenu)
};

// -- Debug > Code reuse stats --

#ifdef PERFORMANCE_IMPACTING_STATISTICS
static struct MenuEntry ReuseMenu_OpsRecompiled = {
	ENTRY_DISPLAY("Opcodes recompiled", &Stats.OpcodeRecompilationCount, TYPE_UINT64)
};

static struct MenuEntry ReuseMenu_BlocksRecompiled = {
	ENTRY_DISPLAY("Blocks recompiled", &Stats.BlockRecompilationCount, TYPE_UINT64)
};

static struct MenuEntry ReuseMenu_OpsReused = {
	ENTRY_DISPLAY("Opcodes reused", &Stats.OpcodeReuseCount, TYPE_UINT64)
};

static struct MenuEntry ReuseMenu_BlocksReused = {
	ENTRY_DISPLAY("Blocks reused", &Stats.BlockReuseCount, TYPE_UINT64)
};

static struct Menu ReuseMenu = {
	.Parent = &DebugMenu, .Title = "Code reuse statistics",
	.Entries = { &ReuseMenu_OpsRecompiled, &ReuseMenu_BlocksRecompiled, &ReuseMenu_OpsReused, &ReuseMenu_BlocksReused, NULL }
};

static struct MenuEntry DebugMenu_Reuse = {
	ENTRY_SUBMENU("Code reuse statistics...", &ReuseMenu)
};
#endif

static struct MenuEntry ROMInfoMenu_GameName = {
	ENTRY_DISPLAY("game_name =", gamepak_title, TYPE_STRING)
};

static struct MenuEntry ROMInfoMenu_GameCode = {
	ENTRY_DISPLAY("game_code =", gamepak_code, TYPE_STRING)
};

static struct MenuEntry ROMInfoMenu_VendorCode = {
	ENTRY_DISPLAY("vender_code =", gamepak_maker, TYPE_STRING)
};

static struct Menu ROMInfoMenu = {
	.Parent = &DebugMenu, .Title = "ROM information",
	.Entries = { &ROMInfoMenu_GameName, &ROMInfoMenu_GameCode, &ROMInfoMenu_VendorCode, NULL }
};

static struct MenuEntry DebugMenu_ROMInfo = {
	ENTRY_SUBMENU("ROM information...", &ROMInfoMenu)
};

static struct MenuEntry DebugMenu_VersionInfo = {
	.Kind = KIND_CUSTOM, .Name = "TempGBA version information...",
	.ButtonEnterFunction = &ActionShowVersion
};

// -- Debug --

static struct Menu DebugMenu = {
	.Parent = &MainMenu, .Title = "Performance and debugging",
	.Entries = { &DebugMenu_NativeCode, &DebugMenu_Metadata, &DebugMenu_Execution
#ifdef PERFORMANCE_IMPACTING_STATISTICS
	, &DebugMenu_Reuse
#endif
	, &Strut, &DebugMenu_ROMInfo, &Strut, &DebugMenu_VersionInfo, NULL }
};

// -- Display Settings --

static struct MenuEntry PerGameDisplayMenu_BootSource = {
	ENTRY_OPTION("boot_from", "Boot from", &PerGameBootFromBIOS),
	.ChoiceCount = 3, .Choices = { { "No override", "" }, { "Cartridge ROM", "cartridge" }, { "GBA BIOS", "gba_bios" } }
};
static struct MenuEntry DisplayMenu_BootSource = {
	ENTRY_OPTION("boot_from", "Boot from", &BootFromBIOS),
	.ChoiceCount = 2, .Choices = { { "Cartridge ROM", "cartridge" }, { "GBA BIOS", "gba_bios" } }
};

static struct MenuEntry PerGameDisplayMenu_FPSCounter = {
	ENTRY_OPTION("fps_counter", "FPS counter", &PerGameShowFPS),
	.ChoiceCount = 3, .Choices = { { "No override", "" }, { "Hide", "hide" }, { "Show", "show" } }
};
static struct MenuEntry DisplayMenu_FPSCounter = {
	ENTRY_OPTION("fps_counter", "FPS counter", &ShowFPS),
	.ChoiceCount = 2, .Choices = { { "Hide", "hide" }, { "Show", "show" } }
};

static struct MenuEntry PerGameDisplayMenu_ScaleMode = {
	ENTRY_OPTION("image_size", "Image scaling", &PerGameScaleMode),
	.ChoiceCount = 10, .Choices = { { "No override", "" }, { "None", "none" }, { "Nearest Neighbor 2x", "scale2xnn" }, { "Scale2x", "scale2x" }, { "Super2xSaI", "sup2xsai" }, { "SuperEagle", "supeagle" }, { "2XSAI", "_2xsai" }, { "SCALE2XSAI", "scale2xsai" }, { "Nearest Neighbor 3x", "scale3xnn" }, { "Scale3x", "scale3x" }  }
};
static struct MenuEntry DisplayMenu_ScaleMode = {
	ENTRY_OPTION("image_size", "Image scaling", &ScaleMode),
	.ChoiceCount = 9, .Choices = { { "None", "none" }, { "Nearest Neighbor 2x", "scale2xnn" }, { "Scale2x", "scale2x" }, { "Super2xSaI", "sup2xsai" }, { "SuperEagle", "supeagle" }, { "2XSAI", "_2xsai" }, { "SCALE2XSAI", "scale2xsai" }, { "Nearest Neighbor 3x", "scale3xnn" }, { "Scale3x", "scale3x" } }
};

static struct MenuEntry PerGameDisplayMenu_VideoMode = {
	ENTRY_OPTION("video_mode", "Video Mode", &PerGameVideoMode),
	.ChoiceCount = 7, .Choices = { { "No override", "" }, { "Auto", "mode_auto" }, { "PAL", "pal" }, { "NTSC", "ntsc" }, { "480P", "dtv_480p" }, { "720P", "dtv_720p" }, { "1080I", "dtv_1080i" } }, .ButtonEnterFunction = &ActionVideoMode
};

static struct MenuEntry DisplayMenu_VideoMode = {
	ENTRY_OPTION("video_mode", "Video Mode", &VideoMode),
	.ChoiceCount = 6, .Choices = { { "Auto", "auto" }, { "PAL", "pal" }, { "NTSC", "ntsc" }, { "480P", "480p" }, { "720P", "720p" }, { "1080I", "1080i" } }, .ButtonEnterFunction = &ActionVideoMode
};

static struct MenuEntry PerGameDisplayMenu_ScreenRatio = {
	ENTRY_OPTION("video_ratio", "Screen Ratio", &PerGameScreenRatio),
	.ChoiceCount = 3, .Choices = { { "No override", "" }, { "Fullscreen", "fullscreen" }, { "Original", "original" } }
};

static struct MenuEntry DisplayMenu_ScreenRatio = {
	ENTRY_OPTION("video_ratio", "Screen Ratio", &ScreenRatio),
	.ChoiceCount = 2, .Choices = { { "Fullscreen", "fullscreen" }, { "Original", "original" } }
};

static struct MenuEntry PerGameDisplayMenu_VideoFilter = {
	ENTRY_OPTION("video_filter", "Texture Filter", &PerGameVideoFilter),
	.ChoiceCount = 3, .Choices = { { "No override", "" }, { "Linear", "linear" }, { "Nearest", "nearest" } }
};

static struct MenuEntry DisplayMenu_VideoFilter = {
	ENTRY_OPTION("video_filter", "Texture Filter", &VideoFilter),
	.ChoiceCount = 2, .Choices = { { "Linear", "linear" }, { "Nearest", "nearest" } }
};

static struct MenuEntry PerGameDisplayMenu_Frameskip = {
	ENTRY_OPTION("frameskip", "Frame skipping", &PerGameUserFrameskip),
	.ChoiceCount = 6, .Choices = { { "No override", "" }, { "Automatic", "auto" }, { "0 (~60 FPS)", "0" }, { "1 (~30 FPS)", "1" }, { "2 (~20 FPS)", "2" }, { "3 (~15 FPS)", "3" } }
};

static struct MenuEntry DisplayMenu_Frameskip = {
	ENTRY_OPTION("frameskip", "Frame skipping", &UserFrameskip),
	.ChoiceCount = 5, .Choices = { { "Automatic", "auto" }, { "0 (~60 FPS)", "0" }, { "1 (~30 FPS)", "1" }, { "2 (~20 FPS)", "2" }, { "3 (~15 FPS)", "3" } }
};

static struct MenuEntry PerGameDisplayMenu_FastForwardTarget = {
	ENTRY_OPTION("fast_forward_target", "Fast-forward target", &PerGameFastForwardTarget),
	.ChoiceCount = 6, .Choices = { { "No override", "" }, { "2x (~120 FPS)", "2" }, { "3x (~180 FPS)", "3" }, { "4x (~240 FPS)", "4" }, { "5x (~300 FPS)", "5" }, { "6x (~360 FPS)", "6" } }
};
static struct MenuEntry DisplayMenu_FastForwardTarget = {
	ENTRY_OPTION("fast_forward_target", "Fast-forward target", &FastForwardTarget),
	.ChoiceCount = 5, .Choices = { { "2x (~120 FPS)", "2" }, { "3x (~180 FPS)", "3" }, { "4x (~240 FPS)", "4" }, { "5x (~300 FPS)", "5" }, { "6x (~360 FPS)", "6" } }
};

static struct MenuEntry DisplayMenu_MenuRes = {
	ENTRY_OPTION("menu_res", "Menu Resolution", &MenuRes),
	.ChoiceCount = 2, .Choices = { { "2x", "menu2x" }, { "3x", "menu3x" } },
	.ButtonEnterFunction = &ActionMenuRes
};

static struct MenuEntry PerGameDisplayMenu_MenuRes = {
	ENTRY_OPTION("menu_res", "Menu Resolution", &PerGameMenuRes),
	.ChoiceCount = 3, .Choices = { { "No override", "" }, { "2x", "menu2x" }, { "3x", "menu3x" } },
	.ButtonEnterFunction = &ActionMenuRes
};

static struct MenuEntry ScreenPos_PosX = {
	.Kind = KIND_OPTION, .Name = "Position X:", .PersistentName = "xpos",
	.Target = &ScreenPosX,
	.ChoiceCount = 1000,
	.LoadFunction = ScreenPosLoadFunction,
	.SaveFunction = ScreenPosSaveFunction,
	/*.ButtonLeftFunction = SavedStateSelectionLeft, .ButtonRightFunction = SavedStateSelectionRight,*/
	.DisplayValueFunction = ScreenPosDisplayValue
};

static struct MenuEntry ScreenPos_PosY = {
	.Kind = KIND_OPTION, .Name = "Position Y:", .PersistentName = "ypos",
	.Target = &ScreenPosY,
	.ChoiceCount = 1000,
	.LoadFunction = ScreenPosLoadFunction,
	.SaveFunction = ScreenPosSaveFunction,
	/*.ButtonLeftFunction = SavedStateSelectionLeft, .ButtonRightFunction = SavedStateSelectionRight,*/
	.DisplayValueFunction = ScreenPosDisplayValue
};

static struct MenuEntry ScreenOverscan_X = {
	.Kind = KIND_OPTION, .Name = "Overscan X:", .PersistentName = "overx",
	.Target = &ScreenOverscanX,
	.ChoiceCount = 200,
	.LoadFunction = ScreenPosLoadFunction,
	.SaveFunction = ScreenOverSaveFunction,
	/*.ButtonLeftFunction = SavedStateSelectionLeft, .ButtonRightFunction = SavedStateSelectionRight,*/
	.DisplayValueFunction = ScreenPosDisplayValue
};

static struct MenuEntry ScreenOverscan_Y = {
	.Kind = KIND_OPTION, .Name = "Overscan Y:", .PersistentName = "overy",
	.Target = &ScreenOverscanY,
	.ChoiceCount = 200,
	.LoadFunction = ScreenPosLoadFunction,
	.SaveFunction = ScreenOverSaveFunction,
	/*.ButtonLeftFunction = SavedStateSelectionLeft, .ButtonRightFunction = SavedStateSelectionRight,*/
	.DisplayValueFunction = ScreenPosDisplayValue
};

static struct MenuEntry ScreenPos_SetDefault = {
	.Kind = KIND_CUSTOM, .Name = "Set default",
	.ButtonEnterFunction = ActionScreenPosSetDefault
};

static struct Menu ScreenPos = {
	.Parent = &DisplayMenu, .Title = "Screen settings",
	/*.InitFunction = ScreenPosInit, .EndFunction = ScreenPosEnd,*/
	/*.DisplayDataFunction = SavedStateMenuDisplayData,*/
	.Entries = { &ScreenPos_PosX, &ScreenPos_PosY, &ScreenOverscan_X, &ScreenOverscan_Y, &Strut, &ScreenPos_SetDefault, NULL }
};

static struct MenuEntry PerGameDisplayMenu_ScreenPos = {
	ENTRY_SUBMENU("Screen settings", &ScreenPos)
};
static struct MenuEntry DisplayMenu_ScreenPos = {
	ENTRY_SUBMENU("Screen settings", &ScreenPos)
};


//////////////////////////////////////////////

static struct Menu PerGameDisplayMenu = {
	.Parent = &PerGameMainMenu, .Title = "Display settings",
	MENU_PER_GAME,
	.AlternateVersion = &DisplayMenu,
	.Entries = { &PerGameDisplayMenu_BootSource, &PerGameDisplayMenu_FPSCounter, &PerGameDisplayMenu_VideoMode, &PerGameDisplayMenu_ScreenRatio, &PerGameDisplayMenu_VideoFilter, &PerGameDisplayMenu_ScaleMode, &PerGameDisplayMenu_Frameskip, &PerGameDisplayMenu_FastForwardTarget, &PerGameDisplayMenu_MenuRes, &PerGameDisplayMenu_ScreenPos, NULL }
};
static struct Menu DisplayMenu = {
	.Parent = &MainMenu, .Title = "Display settings",
	.AlternateVersion = &PerGameDisplayMenu,
	.Entries = { &DisplayMenu_BootSource, &DisplayMenu_FPSCounter, &DisplayMenu_VideoMode, &DisplayMenu_ScreenRatio, &DisplayMenu_VideoFilter, &DisplayMenu_ScaleMode, &DisplayMenu_Frameskip, &DisplayMenu_FastForwardTarget, &DisplayMenu_MenuRes, &DisplayMenu_ScreenPos, NULL }
};

// -- Input Settings --
static struct MenuEntry PerGameInputMenu_A = {
	ENTRY_OPTION("gba_a", "GBA A", &PerGameKeypadRemapping[0]),
	ENTRY_OPTIONAL_MAPPING
};
static struct MenuEntry InputMenu_A = {
	ENTRY_OPTION("gba_a", "GBA A", &KeypadRemapping[0]),
	ENTRY_MANDATORY_MAPPING
};

static struct MenuEntry PerGameInputMenu_B = {
	ENTRY_OPTION("gba_b", "GBA B", &PerGameKeypadRemapping[1]),
	ENTRY_OPTIONAL_MAPPING
};
static struct MenuEntry InputMenu_B = {
	ENTRY_OPTION("gba_b", "GBA B", &KeypadRemapping[1]),
	ENTRY_MANDATORY_MAPPING
};

static struct MenuEntry PerGameInputMenu_Start = {
	ENTRY_OPTION("gba_start", "GBA Start", &PerGameKeypadRemapping[3]),
	ENTRY_OPTIONAL_MAPPING
};
static struct MenuEntry InputMenu_Start = {
	ENTRY_OPTION("gba_start", "GBA Start", &KeypadRemapping[3]),
	ENTRY_MANDATORY_MAPPING
};

static struct MenuEntry PerGameInputMenu_Select = {
	ENTRY_OPTION("gba_select", "GBA Select", &PerGameKeypadRemapping[2]),
	ENTRY_OPTIONAL_MAPPING
};
static struct MenuEntry InputMenu_Select = {
	ENTRY_OPTION("gba_select", "GBA Select", &KeypadRemapping[2]),
	ENTRY_MANDATORY_MAPPING
};

static struct MenuEntry PerGameInputMenu_L = {
	ENTRY_OPTION("gba_l", "GBA L", &PerGameKeypadRemapping[9]),
	ENTRY_OPTIONAL_MAPPING
};
static struct MenuEntry InputMenu_L = {
	ENTRY_OPTION("gba_l", "GBA L", &KeypadRemapping[9]),
	ENTRY_MANDATORY_MAPPING
};

static struct MenuEntry PerGameInputMenu_R = {
	ENTRY_OPTION("gba_r", "GBA R", &PerGameKeypadRemapping[8]),
	ENTRY_OPTIONAL_MAPPING
};
static struct MenuEntry InputMenu_R = {
	ENTRY_OPTION("gba_r", "GBA R", &KeypadRemapping[8]),
	ENTRY_MANDATORY_MAPPING
};

static struct MenuEntry PerGameInputMenu_RapidA = {
	ENTRY_OPTION("rapid_a", "Rapid-fire A", &PerGameKeypadRemapping[10]),
	ENTRY_OPTIONAL_MAPPING
};
static struct MenuEntry InputMenu_RapidA = {
	ENTRY_OPTION("rapid_a", "Rapid-fire A", &KeypadRemapping[10]),
	ENTRY_OPTIONAL_MAPPING
};

static struct MenuEntry PerGameInputMenu_RapidB = {
	ENTRY_OPTION("rapid_b", "Rapid-fire B", &PerGameKeypadRemapping[11]),
	ENTRY_OPTIONAL_MAPPING
};
static struct MenuEntry InputMenu_RapidB = {
	ENTRY_OPTION("rapid_b", "Rapid-fire B", &KeypadRemapping[11]),
	ENTRY_OPTIONAL_MAPPING
};
///////////////////////////////
static struct MenuEntry PerGameInputMenu_AnalogSensitivity = {
	ENTRY_OPTION("analog_sensitivity", "Analog sensitivity", &PerGameAnalogSensitivity),
	.ChoiceCount = 6, .Choices = { { "No override", "" }, { "Very low", "lowest" }, { "Low", "low" }, { "Medium", "medium" }, { "High", "high" }, { "Very high", "highest" } }
};
static struct MenuEntry InputMenu_AnalogSensitivity = {
	ENTRY_OPTION("analog_sensitivity", "Analogs sensitivity", &AnalogSensitivity),
	.ChoiceCount = 5, .Choices = { { "Very low", "lowest" }, { "Low", "low" }, { "Medium", "medium" }, { "High", "high" }, { "Very high", "highest" } }
};

static struct MenuEntry PerGameInputMenu_LeftAnalogAction = {
	ENTRY_OPTION("analog_action", "Left analog in-game binding", &PerGameLeftAnalogAction),
	.ChoiceCount = 3, .Choices = { { "No override", "" }, { "None", "none" }, { "GBA D-pad", "dpad" } }
};
static struct MenuEntry InputMenu_LeftAnalogAction = {
	ENTRY_OPTION("analog_action", "Left analog in-game binding", &LeftAnalogAction),
	.ChoiceCount = 2, .Choices = { { "None", "none" }, { "GBA D-pad", "dpad" } }
};

static struct MenuEntry PerGameInputMenu_RightAnalogAction = {
	ENTRY_OPTION("analog_action", "Right analog in-game binding", &PerGameRightAnalogAction),
	.ChoiceCount = 3, .Choices = { { "No override", "" }, { "Tilt sensor", "tilt_sensor" }, { "GBA L/R", "lr" } }
};
static struct MenuEntry InputMenu_RightAnalogAction = {
	ENTRY_OPTION("analog_action", "Right analog in-game binding", &RightAnalogAction),
	.ChoiceCount = 2, .Choices = { { "Tilt sensor", "tilt_sensor" }, { "GBA L/R", "lr" } }
};

///////////////////////////
static struct Menu PerGameInputMenu = {
	.Parent = &PerGameMainMenu, .Title = "Input settings",
	MENU_PER_GAME,
	.AlternateVersion = &InputMenu,
	.Entries = { &PerGameInputMenu_A, &PerGameInputMenu_B, &PerGameInputMenu_Start, &PerGameInputMenu_Select, &PerGameInputMenu_L, &PerGameInputMenu_R, &PerGameInputMenu_RapidA, &PerGameInputMenu_RapidB
	, &Strut, &PerGameInputMenu_AnalogSensitivity, &PerGameInputMenu_LeftAnalogAction, &PerGameInputMenu_RightAnalogAction
	, NULL }
};
static struct Menu InputMenu = {
	.Parent = &MainMenu, .Title = "Input settings",
	.AlternateVersion = &PerGameInputMenu,
	.Entries = { &InputMenu_A, &InputMenu_B, &InputMenu_Start, &InputMenu_Select, &InputMenu_L, &InputMenu_R, &InputMenu_RapidA, &InputMenu_RapidB
	, &Strut, &InputMenu_AnalogSensitivity, &InputMenu_LeftAnalogAction, &InputMenu_RightAnalogAction
	, NULL }
};

// -- Hotkeys --

static struct MenuEntry PerGameHotkeyMenu_FastForward = {
	ENTRY_OPTION("hotkey_fast_forward", "Fast-forward while held", &PerGameHotkeys[0]),
	ENTRY_OPTIONAL_HOTKEY
};
static struct MenuEntry HotkeyMenu_FastForward = {
	ENTRY_OPTION("hotkey_fast_forward", "Fast-forward while held", &Hotkeys[0]),
	ENTRY_OPTIONAL_HOTKEY
};

#if !defined GCW_ZERO
static struct MenuEntry HotkeyMenu_Menu = {
	ENTRY_OPTION("hotkey_menu", "Menu", &Hotkeys[1]),
	ENTRY_MANDATORY_HOTKEY
};
#endif

static struct MenuEntry PerGameHotkeyMenu_FastForwardToggle = {
	ENTRY_OPTION("hotkey_fast_forward_toggle", "Fast-forward toggle", &PerGameHotkeys[2]),
	ENTRY_OPTIONAL_HOTKEY
};
static struct MenuEntry HotkeyMenu_FastForwardToggle = {
	ENTRY_OPTION("hotkey_fast_forward_toggle", "Fast-forward toggle", &Hotkeys[2]),
	ENTRY_OPTIONAL_HOTKEY
};

static struct MenuEntry PerGameHotkeyMenu_QuickLoadState = {
	ENTRY_OPTION("hotkey_quick_load_state", "Quick load state #1", &PerGameHotkeys[3]),
	ENTRY_OPTIONAL_HOTKEY
};
static struct MenuEntry HotkeyMenu_QuickLoadState = {
	ENTRY_OPTION("hotkey_quick_load_state", "Quick load state #1", &Hotkeys[3]),
	ENTRY_OPTIONAL_HOTKEY
};

static struct MenuEntry PerGameHotkeyMenu_QuickSaveState = {
	ENTRY_OPTION("hotkey_quick_save_state", "Quick save state #1", &PerGameHotkeys[4]),
	ENTRY_OPTIONAL_HOTKEY
};
static struct MenuEntry HotkeyMenu_QuickSaveState = {
	ENTRY_OPTION("hotkey_quick_save_state", "Quick save state #1", &Hotkeys[4]),
	ENTRY_OPTIONAL_HOTKEY
};

static struct Menu PerGameHotkeyMenu = {
	.Parent = &PerGameMainMenu, .Title = "Hotkeys",
	MENU_PER_GAME,
	.AlternateVersion = &HotkeyMenu,
	.Entries = { &Strut, &PerGameHotkeyMenu_FastForward, &PerGameHotkeyMenu_FastForwardToggle, &PerGameHotkeyMenu_QuickLoadState, &PerGameHotkeyMenu_QuickSaveState, NULL }
};
static struct Menu HotkeyMenu = {
	.Parent = &MainMenu, .Title = "Hotkeys",
	.AlternateVersion = &PerGameHotkeyMenu,
	.Entries = {
#if !defined GCW_ZERO
		&HotkeyMenu_Menu,
#else
		&Strut,
#endif
		&HotkeyMenu_FastForward, &HotkeyMenu_FastForwardToggle, &HotkeyMenu_QuickLoadState, &HotkeyMenu_QuickSaveState, NULL
	}
};

// -- Saved States --

static struct MenuEntry SavedStateMenu_SelectedState = {
	.Kind = KIND_CUSTOM, .Name = "Save slot #", .PersistentName = "",
	.Target = &SelectedState,
	.ChoiceCount = 100,
	.ButtonLeftFunction = SavedStateSelectionLeft, .ButtonRightFunction = SavedStateSelectionRight,
	.DisplayValueFunction = SavedStateSelectionDisplayValue
};

static struct MenuEntry SavedStateMenu_Read = {
	.Kind = KIND_CUSTOM, .Name = "Load from selected slot",
	.ButtonEnterFunction = ActionSavedStateRead
};

static struct MenuEntry SavedStateMenu_Write = {
	.Kind = KIND_CUSTOM, .Name = "Save to selected slot",
	.ButtonEnterFunction = ActionSavedStateWrite
};

static struct MenuEntry SavedStateMenu_Delete = {
	.Kind = KIND_CUSTOM, .Name = "Delete selected state",
	.ButtonEnterFunction = ActionSavedStateDelete
};

static struct Menu SavedStateMenu = {
	.Parent = &MainMenu, .Title = "Saved states",
	.InitFunction = SavedStateMenuInit, .EndFunction = SavedStateMenuEnd,
	.DisplayDataFunction = SavedStateMenuDisplayData,
	.Entries = { &SavedStateMenu_SelectedState, &Strut, &SavedStateMenu_Read, &SavedStateMenu_Write, &SavedStateMenu_Delete, NULL }
};

// -- Main Menu --

static struct MenuEntry PerGameMainMenu_Display = {
	ENTRY_SUBMENU("Display settings...", &PerGameDisplayMenu)
};
static struct MenuEntry MainMenu_Display = {
	ENTRY_SUBMENU("Display settings...", &DisplayMenu)
};

static struct MenuEntry PerGameMainMenu_Input = {
	ENTRY_SUBMENU("Input settings...", &PerGameInputMenu)
};
static struct MenuEntry MainMenu_Input = {
	ENTRY_SUBMENU("Input settings...", &InputMenu)
};

static struct MenuEntry PerGameMainMenu_Hotkey = {
	ENTRY_SUBMENU("Hotkeys...", &PerGameHotkeyMenu)
};
static struct MenuEntry MainMenu_Hotkey = {
	ENTRY_SUBMENU("Hotkeys...", &HotkeyMenu)
};

static struct MenuEntry MainMenu_SavedStates = {
	ENTRY_SUBMENU("Saved states...", &SavedStateMenu)
};

static struct MenuEntry PerGameMainMenu_Cheats = {
	.Kind = KIND_OPTION, .Name = "Cheats...", .PersistentName = "cheat",
	.Target = &PerGameCheatsSettings, .ChoiceCount = 2, .Choices = {{ "", "" },{ "", "" }},
	.LoadFunction = &CheatsMenuLoadFunction, .SaveFunction = &CheatsMenuSaveFunction,
	.ButtonEnterFunction = &ActionCheatsMenu
};

static struct MenuEntry MainMenu_Debug = {
	ENTRY_SUBMENU("Performance and debugging...", &DebugMenu)
};

static struct MenuEntry MainMenu_Reset = {
	.Kind = KIND_CUSTOM, .Name = "Reset the game",
	.ButtonEnterFunction = &ActionReset
};

static struct MenuEntry MainMenu_LoadGame = {
	.Kind = KIND_CUSTOM, .Name = "Load new game",
	.ButtonEnterFunction = &ActionLoadGame
};

static struct MenuEntry MainMenu_Return = {
	.Kind = KIND_CUSTOM, .Name = "Return to the game",
	.ButtonEnterFunction = &ActionReturn
};

static struct MenuEntry MainMenu_Exit = {
	.Kind = KIND_CUSTOM, .Name = "Exit",
	.ButtonEnterFunction = &ActionExit
};

static struct Menu PerGameMainMenu = {
	.Parent = NULL, .Title = "TempGBA Main Menu",
	MENU_PER_GAME,
	.AlternateVersion = &MainMenu,
	.Entries = { &PerGameMainMenu_Display, &PerGameMainMenu_Input, &PerGameMainMenu_Hotkey, &Strut, &PerGameMainMenu_Cheats, &Strut, &Strut, &Strut, &MainMenu_LoadGame, &MainMenu_Reset, &MainMenu_Return, &MainMenu_Exit, NULL }
};

struct Menu MainMenu = {
	.Parent = NULL, .Title = "TempGBA Main Menu",
	.AlternateVersion = &PerGameMainMenu,
	.Entries = { &MainMenu_Display, &MainMenu_Input, &MainMenu_Hotkey, &Strut, &MainMenu_SavedStates, &Strut, &Strut, &MainMenu_Debug, &MainMenu_LoadGame, &MainMenu_Reset, &MainMenu_Return, &MainMenu_Exit, NULL }
};

/* Do not make this the active menu */
static struct Menu ErrorScreen = {
	.Parent = &MainMenu, .Title = "Error",
	.DisplayBackgroundFunction = DisplayErrorBackgroundFunction,
	.Entries = { NULL }
};

u32 ReGBA_Menu(enum ReGBA_MenuEntryReason EntryReason)
{
	pause_audio();
	MainMenu.UserData = copy_screen();

	// Avoid entering the menu with menu keys pressed (namely the one bound to
	// the per-game option switching key, Select).
	while (ReGBA_GetPressedButtons() != 0)
	{
		;//usleep(5000);
	}

	SetMenuResolution();


	if(EntryReason == REGBA_MENU_ENTRY_REASON_NO_ROM)
		main_ret = 1;

	struct Menu *ActiveMenu = &MainMenu, *PreviousMenu = ActiveMenu;
	
	if (MainMenu.InitFunction != NULL)
	{
		(*(MainMenu.InitFunction))(&ActiveMenu);
		while (PreviousMenu != ActiveMenu)
		{
			if (PreviousMenu->EndFunction != NULL)
				(*(PreviousMenu->EndFunction))(PreviousMenu);
			PreviousMenu = ActiveMenu;
			if (ActiveMenu != NULL && ActiveMenu->InitFunction != NULL)
				(*(ActiveMenu->InitFunction))(&ActiveMenu);
		}
	}

	void* PreviousGlobal = ReGBA_PreserveMenuSettings(&MainMenu);
	void* PreviousPerGame = ReGBA_PreserveMenuSettings(MainMenu.AlternateVersion);

	while (ActiveMenu != NULL)
	{
		// Draw.
		MenuFunction DisplayBackgroundFunction = ActiveMenu->DisplayBackgroundFunction;
		if (DisplayBackgroundFunction == NULL) DisplayBackgroundFunction = DefaultDisplayBackgroundFunction;
		(*DisplayBackgroundFunction)(ActiveMenu);
		
		DrawFrame(OFFSET - 15);
		
		//blit_to_screen((uint16_t*)MainMenu.UserData, GBA_SCREEN_WIDTH, GBA_SCREEN_HEIGHT, gsTexture.Width - GBA_SCREEN_WIDTH - OFFSET, GetRenderedHeight(" ") * 3 + 1 + OFFSET);
		
		MenuFunction DisplayTitleFunction = ActiveMenu->DisplayTitleFunction;
		if (DisplayTitleFunction == NULL) DisplayTitleFunction = DefaultDisplayTitleFunction;
		(*DisplayTitleFunction)(ActiveMenu);

		MenuEntryFunction DisplayDataFunction = ActiveMenu->DisplayDataFunction;
		if (DisplayDataFunction == NULL) DisplayDataFunction = DefaultDisplayDataFunction;
		(*DisplayDataFunction)(ActiveMenu, ActiveMenu->Entries[ActiveMenu->ActiveEntryIndex]);

		ReGBA_VideoFlip();
		
		// Wait. (This is for platforms on which flips don't wait for vertical
		// sync.)
		//usleep(5000);

		// Get input.
		enum GUI_Action Action = GetGUIAction();
		
		switch (Action)
		{
			case GUI_ACTION_ENTER:
				if (ActiveMenu->Entries[ActiveMenu->ActiveEntryIndex] != NULL)
				{
					MenuModifyFunction ButtonEnterFunction = ActiveMenu->Entries[ActiveMenu->ActiveEntryIndex]->ButtonEnterFunction;
					if (ButtonEnterFunction == NULL) ButtonEnterFunction = DefaultEnterFunction;
					(*ButtonEnterFunction)(&ActiveMenu, &ActiveMenu->ActiveEntryIndex);
					break;
				}
				// otherwise, no entry has the focus, so ENTER acts like LEAVE
				// (fall through)

			case GUI_ACTION_LEAVE:
			{
				MenuModifyFunction ButtonLeaveFunction = ActiveMenu->ButtonLeaveFunction;
				if (ButtonLeaveFunction == NULL) ButtonLeaveFunction = DefaultLeaveFunction;
				(*ButtonLeaveFunction)(&ActiveMenu, &ActiveMenu->ActiveEntryIndex);
				break;
			}

			case GUI_ACTION_UP:
			{
				MenuModifyFunction ButtonUpFunction = ActiveMenu->ButtonUpFunction;
				if (ButtonUpFunction == NULL) ButtonUpFunction = DefaultUpFunction;
				(*ButtonUpFunction)(&ActiveMenu, &ActiveMenu->ActiveEntryIndex);
				break;
			}

			case GUI_ACTION_DOWN:
			{
				MenuModifyFunction ButtonDownFunction = ActiveMenu->ButtonDownFunction;
				if (ButtonDownFunction == NULL) ButtonDownFunction = DefaultDownFunction;
				(*ButtonDownFunction)(&ActiveMenu, &ActiveMenu->ActiveEntryIndex);
				break;
			}

			case GUI_ACTION_LEFT:
				if (ActiveMenu->Entries[ActiveMenu->ActiveEntryIndex] != NULL)
				{
					MenuEntryFunction ButtonLeftFunction = ActiveMenu->Entries[ActiveMenu->ActiveEntryIndex]->ButtonLeftFunction;
					if (ButtonLeftFunction == NULL) ButtonLeftFunction = DefaultLeftFunction;
					(*ButtonLeftFunction)(ActiveMenu, ActiveMenu->Entries[ActiveMenu->ActiveEntryIndex]);
				}
				break;

			case GUI_ACTION_RIGHT:
				if (ActiveMenu->Entries[ActiveMenu->ActiveEntryIndex] != NULL)
				{
					MenuEntryFunction ButtonRightFunction = ActiveMenu->Entries[ActiveMenu->ActiveEntryIndex]->ButtonRightFunction;
					if (ButtonRightFunction == NULL) ButtonRightFunction = DefaultRightFunction;
					(*ButtonRightFunction)(ActiveMenu, ActiveMenu->Entries[ActiveMenu->ActiveEntryIndex]);
				}
				break;

			case GUI_ACTION_ALTERNATE:
				if (IsGameLoaded && ActiveMenu->AlternateVersion != NULL)
					ActiveMenu = ActiveMenu->AlternateVersion;
				break;

			default:
				break;
		}

		// Possibly finalise this menu and activate and initialise a new one.
		while (ActiveMenu != PreviousMenu)
		{
			if (PreviousMenu->EndFunction != NULL)
				(*(PreviousMenu->EndFunction))(PreviousMenu);

			// Save settings if they have changed.
			void* Global = ReGBA_PreserveMenuSettings(&MainMenu);

			if (!ReGBA_AreMenuSettingsEqual(&MainMenu, PreviousGlobal, Global))
			{
				ReGBA_SaveSettings("global_config", false);
			}
			free(PreviousGlobal);
			PreviousGlobal = Global;

			void* PerGame = ReGBA_PreserveMenuSettings(MainMenu.AlternateVersion);
			if (!ReGBA_AreMenuSettingsEqual(MainMenu.AlternateVersion, PreviousPerGame, PerGame) && IsGameLoaded)
			{
				char FileNameNoExt[MAX_PATH + 1];
				GetFileNameNoExtension(FileNameNoExt, CurrentGamePath);
				ReGBA_SaveSettings(FileNameNoExt, true);
			}
			free(PreviousPerGame);
			PreviousPerGame = PerGame;

			// Keep moving down until a menu entry can be focused, if
			// the first one can't be.
			if (ActiveMenu != NULL && ActiveMenu->Entries[ActiveMenu->ActiveEntryIndex] != NULL)
			{
				uint32_t Sentinel = ActiveMenu->ActiveEntryIndex;
				MenuEntryCanFocusFunction CanFocusFunction = ActiveMenu->Entries[ActiveMenu->ActiveEntryIndex]->CanFocusFunction;
				if (CanFocusFunction == NULL) CanFocusFunction = DefaultCanFocusFunction;
				while (!(*CanFocusFunction)(ActiveMenu, ActiveMenu->Entries[ActiveMenu->ActiveEntryIndex]))
				{
					MoveDown(ActiveMenu, &ActiveMenu->ActiveEntryIndex);
					if (ActiveMenu->ActiveEntryIndex == Sentinel)
					{
						// If we went through all of them, we cannot focus anything.
						// Place the focus on the NULL entry.
						ActiveMenu->ActiveEntryIndex = FindNullEntry(ActiveMenu);
						break;
					}
					CanFocusFunction = ActiveMenu->Entries[ActiveMenu->ActiveEntryIndex]->CanFocusFunction;
					if (CanFocusFunction == NULL) CanFocusFunction = DefaultCanFocusFunction;
				}
			}

			PreviousMenu = ActiveMenu;
			if (ActiveMenu != NULL && ActiveMenu->InitFunction != NULL)
				(*(ActiveMenu->InitFunction))(&ActiveMenu);
		}
	}

	free(PreviousGlobal);
	free(PreviousPerGame);

	// Avoid leaving the menu with GBA keys pressed (namely the one bound to
	// the native exit button, B).
	while (ReGBA_GetPressedButtons() != 0)
	{
		;//usleep(5000);
	}

	clear_screen(COLOR_BACKGROUND);
	
	ReGBA_VideoFlip();
	
	SetGameResolution();

	clear_screens(); 

	StatsStopFPS();
	timespec Now;
	clock_gettime(&Now);
	
	Stats.LastFPSCalculationTime = Now;
	if (MainMenu.UserData != NULL)
		free(MainMenu.UserData);
	return main_ret;
}
