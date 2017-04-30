/* gameplaySP
 *
 * Copyright (C) 2006 Exophase <exophase@gmail.com>
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

struct StringCut {
	uint32_t Start;  // Starting character index of the cut, inclusive.
	uint32_t End;    // Ending character index of the cut, exclusive.
};

uint16_t *GBAScreen;
uint32_t GBAScreenPitch = GBA_SCREEN_WIDTH;

uint16_t _GBAScreen[GBA_SCREEN_WIDTH * GBA_SCREEN_HEIGHT] __attribute__((aligned(128)));

#define MENU_WIDTH  720
#define MENU_HEIGHT 480

volatile unsigned int VideoFastForwarded;
uint_fast8_t AudioFrameskip = 0;
uint_fast8_t AudioFrameskipControl = 0;
uint_fast8_t SufficientAudioControl = 0;
uint_fast8_t UserFrameskipControl = 0;

uint_fast8_t FramesBordered = 0;

video_mode_type PerGameVideoMode = mode_auto;
video_mode_type VideoMode = mode_auto;
video_mode_type OldVideoMode = mode_auto;

video_ratio_type PerGameScreenRatio = fullscreen;
video_ratio_type ScreenRatio = fullscreen;
video_ratio_type OldScreenRatio = fullscreen;

video_filter_type PerGameVideoFilter = linear;
video_filter_type VideoFilter = linear;

menu_res_type MenuRes = menu2x;
menu_res_type PerGameMenuRes = menu2x;

uint32_t ScreenPosX = 0;
uint32_t ScreenPosY = 0;

uint32_t CurrentScreenPosX = 0;
uint32_t CurrentScreenPosY = 0;

#define COLOR_PROGRESS_BACKGROUND   RGB888_TO_RGB565(  0,   0,   0)
#define COLOR_PROGRESS_TEXT_CONTENT RGB888_TO_RGB565(255, 255, 255)
#define COLOR_PROGRESS_TEXT_OUTLINE RGB888_TO_RGB565(  0,   0,   0)
#define COLOR_PROGRESS_CONTENT      RGB888_TO_RGB565(  0, 128, 255)
#define COLOR_PROGRESS_OUTLINE      RGB888_TO_RGB565(255, 255, 255)

#define PROGRESS_WIDTH 240
#define PROGRESS_HEIGHT 18

static bool InFileAction = false;
static enum ReGBA_FileAction CurrentFileAction;
static struct timespec LastProgressUpdate;

GSGLOBAL *gsGlobal;
GSTEXTURE gsTexture;

static const u64 TEXTURE_RGBAQ = GS_SETREG_RGBAQ(0x80,0x80,0x80,0x80,0x00);
static const u64 Black = GS_SETREG_RGBAQ(0x00,0x00,0x00,0x80,0x00);

u8 menu_res = 0;

void memset64( void * dest, uint64_t value, uintptr_t size )
{
  uintptr_t i;
  
  for(i = 0; i < (size & (~7)); i+=8 )
  {
	memcpy( ((char*)dest) + i, &value, 8 );
  }  
  for( ; i < size; i++ )
  {
    ((char*)dest)[i] = ((char*)&value)[i&7];
  }  
}

void clear_screen(uint16_t color)
{
	if (gsTexture.Mem == NULL)
	return;
	
	u64 color64 = 0;
	
	color64 |= (u64)color | (u64)color << 16 | (u64)color << 32 | (u64)color << 48 ;
	
	memset64(gsTexture.Mem, color64, gsTexture.Width * gsTexture.Height * 2);
}

void draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t color)
{
	int i;
	u64 color64 = 0;
	
	if(x + w > gsTexture.Width || y + h > gsTexture.Height)
	return;
	
	//printf("x %d y %d w %d h %d\n", x, y, w, h);
	
	color64 |= (u64)color | (u64)color << 16 | (u64)color << 32 | (u64)color << 48;
	
	for(i = 0; i < h; i++)
	{
		//memset64(gsTexture->Mem + gsTexture->Width / 2 * (y + i) + x / 2 + x % 2, color64, w * 2);
		memset64((u8 *)gsTexture.Mem + gsTexture.Width * 2 * (y + i) + x * 2, color64, w * 2);
	}
}

void clear_screens()
{
	gsKit_clear(gsGlobal, Black);
	gsKit_queue_exec(gsGlobal);
	gsKit_sync_flip(gsGlobal);
	
	gsKit_clear(gsGlobal, Black);
	gsKit_queue_exec(gsGlobal);
	gsKit_sync_flip(gsGlobal);
}

void gsInit()
{
    dmaKit_init(D_CTRL_RELE_OFF,D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC, D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);

    dmaKit_chan_init(DMA_CHANNEL_GIF);
    
	if(gsGlobal != NULL)
	gsKit_deinit_global(gsGlobal);
	
	gsGlobal = gsKit_init_global();
	
	gsGlobal->PSM = GS_PSM_CT16;
	
	gsKit_init_screen(gsGlobal);
	gsKit_mode_switch(gsGlobal, GS_ONESHOT);
	clear_screens();
}

void gsReload()
{
    gsGlobal->PSM = GS_PSM_CT16;  
    gsGlobal->DoubleBuffering = GS_SETTING_OFF;
    gsGlobal->ZBuffering = GS_SETTING_OFF;
	gsGlobal->PrimAlphaEnable = GS_SETTING_OFF;
	
	video_mode_type ResolvedVideoMode = ResolveSetting(VideoMode, PerGameVideoMode);
	
	if(ResolvedVideoMode == mode_auto)
	{
		if(gsKit_detect_signal() == GS_MODE_NTSC)
			ResolvedVideoMode = ntsc;
		else
			ResolvedVideoMode = pal;
	}
	
	switch(ResolvedVideoMode)
	{
		case pal:
			gsGlobal->Mode = GS_MODE_PAL;
			gsGlobal->Interlace = GS_INTERLACED;
			gsGlobal->Field = GS_FIELD;
			gsGlobal->Width = 640;
			gsGlobal->Height = 512;
			break;
		case ntsc:
			gsGlobal->Mode = GS_MODE_NTSC;
			gsGlobal->Interlace = GS_INTERLACED;
			gsGlobal->Field = GS_FIELD;
			gsGlobal->Width = 640;
			gsGlobal->Height = 448;
			break;
		case dtv_480p:
			gsGlobal->Mode = GS_MODE_DTV_480P;
			gsGlobal->Interlace = GS_NONINTERLACED;
			gsGlobal->Field = GS_FRAME;
			gsGlobal->Width = 640;
			gsGlobal->Height = 480;
			break;
		case dtv_720p:
			gsGlobal->Mode = GS_MODE_DTV_720P;
			gsGlobal->Interlace = GS_NONINTERLACED;
			gsGlobal->Field = GS_FRAME;
			gsGlobal->Width = 1280;
			gsGlobal->Height = 720;
			break;
		case dtv_1080i:
			gsGlobal->Mode = GS_MODE_DTV_1080I;
			gsGlobal->Interlace = GS_INTERLACED;
			gsGlobal->Field = GS_FRAME;
			gsGlobal->Width = 1920;
			gsGlobal->Height = 1080;
			gsGlobal->Dithering = GS_SETTING_ON;
			break;
		default:
			break;
	}

	if((gsGlobal->Interlace == GS_INTERLACED) && (gsGlobal->Field == GS_FRAME))
		gsGlobal->Height /= 2;

	OldVideoMode = ResolvedVideoMode;
    
	gsKit_reset_screen(gsGlobal);
	
	uint32_t ResolvedScreenPosX = ResolveSetting(ScreenPosX, 0);
	uint32_t ResolvedScreenPosY = ResolveSetting(ScreenPosY, 0);
	
	if(ResolvedScreenPosX == 0 || ResolvedScreenPosY == 0)
	{
		ScreenPosX = gsGlobal->StartX;
		ScreenPosY = gsGlobal->StartY;
	}
	else
	{
		gsKit_set_display_offset(gsGlobal, ScreenPosX - gsGlobal->StartX, ScreenPosY - gsGlobal->StartY);
		
		CurrentScreenPosX = ScreenPosX;
    	CurrentScreenPosY = ScreenPosY;
	}
	
	gsKit_mode_switch(gsGlobal, GS_ONESHOT);
}

void gsDeinit()
{
	clear_screens();

	gsKit_vram_clear(gsGlobal);
}

static float center_x, center_y, x2, y2;

void gsTex(int width, int height, GSTEXTURE *gsTex)
{
    int size;
    float ratio, w_ratio, h_ratio, y_fix = 1.0f;

    gsTex->Height = height;
    gsTex->Width = width;
    gsTex->PSM = GS_PSM_CT16;
	
	video_filter_type ResolvedVideoFilter = ResolveSetting(VideoFilter, PerGameVideoFilter);
	if(ResolvedVideoFilter == nearest)
    	gsTex->Filter = GS_FILTER_NEAREST;
	else
		gsTex->Filter = GS_FILTER_LINEAR;
    
	size = gsKit_texture_size(gsTex->Width, gsTex->Height, gsTex->PSM);
    
	if(gsTex->Mem == NULL)
	{
    	gsTex->Mem = memalign(128, size);
    	gsTex->Vram = gsKit_vram_alloc(gsGlobal, size, GSKIT_ALLOC_USERBUFFER);
	}
    
    memset(gsTex->Mem, 0, size);
	gsKit_setup_tbw(gsTex);
	
	if((gsGlobal->Interlace == GS_INTERLACED) && (gsGlobal->Field == GS_FRAME))
		y_fix = 0.5f;
	
	w_ratio = (float)gsGlobal->Width / (float)gsTexture.Width;
	h_ratio = (float)(gsGlobal->Height / y_fix) / (float)gsTexture.Height;
	ratio = (w_ratio <= h_ratio) ? w_ratio : h_ratio;
	
	center_x = ((float)gsGlobal->Width - ((float)gsTexture.Width * ratio)) / 2;
	center_y = ((float)(gsGlobal->Height / y_fix) - ((float)gsTexture.Height * ratio)) / 2;
	
	video_ratio_type ResolvedScreenRatio = ResolveSetting(ScreenRatio, PerGameScreenRatio);
	if(ResolvedScreenRatio == fullscreen || menu_res)
	{
		x2 = (float)gsGlobal->Width;
		y2 = (float)gsGlobal->Height;
		center_x = 0.0f;
		center_y = 0.0f;
	}
	else
	{
		x2 = (float)gsTexture.Width * ratio + center_x;
		y2 = (float)gsTexture.Height * y_fix * ratio + center_y;
	}
	
	if(OldScreenRatio != ResolvedScreenRatio)
		clear_screens();
		
	OldScreenRatio = ResolvedScreenRatio;
}

void ReGBA_VideoFlip()
{
	SyncDCache(gsTexture.Mem, (void*)((unsigned int)gsTexture.Mem+gsKit_texture_size_ee(gsTexture.Width, gsTexture.Height, gsTexture.PSM)));
	gsKit_texture_send_inline(gsGlobal, gsTexture.Mem, gsTexture.Width, gsTexture.Height, gsTexture.Vram, gsTexture.PSM, gsTexture.TBW, GS_CLUT_NONE);

	gsKit_prim_sprite_texture(gsGlobal, &gsTexture, center_x, center_y, // x1,y1
								0, 0, // u1,v1
						    	x2, y2, //x2,y2
						    	gsTexture.Width, gsTexture.Height, //u2, v2
						    	1.0f, TEXTURE_RGBAQ);
	
	gsKit_sync_flip(gsGlobal);
	gsKit_queue_exec(gsGlobal);
}

volatile int vblank_count = 0;

static int vblank_interrupt_handler(void)
{
	vblank_count++;
  
	ExitHandler();
  
	return 0;
}

void init_video()
{
	gsInit();

	gsKit_add_vsync_handler(&vblank_interrupt_handler);
	gsKit_vsync_nowait();
	
	gsTex(GBA_SCREEN_WIDTH * 3, GBA_SCREEN_HEIGHT * 3, &gsTexture);
	
	GBAScreen = _GBAScreen;
}

void reload_video()
{
	gsDeinit();
	gsReload();

	gsTex(GBA_SCREEN_WIDTH * 3, GBA_SCREEN_HEIGHT * 3, &gsTexture);
	
	GBAScreen = _GBAScreen;
	
	SetMenuResolution();
}

u8 use_scaler = 0;

void SetMenuResolution()
{
	int width, height;
	
	menu_res_type ResolvedMenuRes = ResolveSetting(MenuRes, PerGameMenuRes);	
	if(ResolvedMenuRes == menu3x)
	{
		width = GBA_SCREEN_WIDTH * 3;
		height = GBA_SCREEN_HEIGHT * 3;
	}
	else
	{
		width = GBA_SCREEN_WIDTH * 2;
		height = GBA_SCREEN_HEIGHT * 2;
	}

	use_scaler = 0;
	menu_res = 1;
	
	gsTex(width, height, &gsTexture);
}

void SetGameResolution()
{
	int width, height;
	
	GBAScreen = _GBAScreen;
	
	video_scale_type ResolvedScaleMode = ResolveSetting(ScaleMode, PerGameScaleMode);
	if(ResolvedScaleMode > 0 && ResolvedScaleMode <= 6)
	{
		width = GBA_SCREEN_WIDTH * 2;
		height = GBA_SCREEN_HEIGHT * 2;
		use_scaler = 1;
	}
	else if(ResolvedScaleMode > 6)
	{
		width = GBA_SCREEN_WIDTH * 3;
		height = GBA_SCREEN_HEIGHT * 3;
		use_scaler = 1;
	}
	else
	{
		width = GBA_SCREEN_WIDTH;
		height = GBA_SCREEN_HEIGHT;
		GBAScreen = (uint16_t *)gsTexture.Mem;
		use_scaler = 0;
	}
	
	menu_res = 0;
	vblank_count = 0;
	Stats.RenderedFPS = 0;
	Stats.EmulatedFPS = 0;
	
	gsTex(width, height, &gsTexture);
}

volatile float skip_frame = -1, skip_ctr = 0;

void ReGBA_RenderScreen(void)
{
	if(vblank_count >= (gsGlobal->Mode == GS_MODE_PAL ? 50 : 60))
	{
	//	sio_printf("Emu %d Rend %d\n", Stats.EmulatedFrames, Stats.RenderedFrames);
		
		Stats.RenderedFPS = Stats.RenderedFrames;
		Stats.EmulatedFPS = Stats.EmulatedFrames;
		
		if(Stats.EmulatedFrames < 59)
		{
			if(skip_frame == -1)
				skip_frame = 60.0 / (Stats.RenderedFrames - (60.0/(3600.0/(Stats.RenderedFrames*Stats.EmulatedFrames))));
			else
				skip_frame -= (60.0 - Stats.EmulatedFrames)/100;
		}
		else if(Stats.EmulatedFPS > 61)
		{
			skip_frame += (Stats.EmulatedFrames - 60.0)/100;
		}
		
		if(Stats.RenderedFPS > 58)
		{
			skip_frame = -1;
		}
		
		if(skip_frame <= 1.0)
		{
			skip_frame = -1;
		}
		
		//sio_printf("skip_frame %f frame %d\n", skip_frame, Stats.EmulatedFrames);
		
		skip_ctr = skip_frame;
		
		vblank_count = 0;

		Stats.RenderedFrames = 0;
		Stats.EmulatedFrames = 0;
	}
	
	if((int)skip_ctr == Stats.EmulatedFrames)
	{
		skip_ctr += skip_frame;
	
		//sio_printf("skip it %d\n", Stats.EmulatedFrames);
		AudioFrameskipControl = 1;
	}
	else
		AudioFrameskipControl = 0;


	if (ReGBA_IsRenderingNextFrame())
	{
	
		Stats.TotalRenderedFrames++;
		Stats.RenderedFrames++;
		
		video_scale_type ResolvedScaleMode = ResolveSetting(ScaleMode, PerGameScaleMode);
		
		if(ResolvedScaleMode > 0 && use_scaler)
		{
			scaler(ResolvedScaleMode, gsTexture.Mem, GBAScreen, GBA_SCREEN_WIDTH, GBA_SCREEN_HEIGHT, gsTexture.Width, gsTexture.Height);
		}
	
		ReGBA_DisplayFPS();
		ReGBA_VideoFlip();
		
		while (true) //fix the problem with this
		{
			unsigned int AudioFastForwardedCopy = AudioFastForwarded;
			unsigned int FramesAhead = (VideoFastForwarded >= AudioFastForwardedCopy)
				?  VideoFastForwarded - AudioFastForwardedCopy
				:  0x100 - (AudioFastForwardedCopy - VideoFastForwarded);
			uint32_t Quota = AUDIO_OUTPUT_BUFFER_SIZE * 3 * OUTPUT_FREQUENCY_DIVISOR + (uint32_t) (FramesAhead * (SOUND_FREQUENCY / 59.73f));
			if (ReGBA_GetAudioSamplesAvailable() <= Quota)
				break;
			else
				ReGBA_DiscardAudioSamples(Quota);
				
			//usleep(1000);
		}
	}

	if (ReGBA_GetAudioSamplesAvailable() < AUDIO_OUTPUT_BUFFER_SIZE * 2 * OUTPUT_FREQUENCY_DIVISOR)
	{
	
	//printf("*");
		//if (AudioFrameskip < MAX_AUTO_FRAMESKIP)
		//	AudioFrameskip++;
		SufficientAudioControl = 0;
	}
	else
	{
		SufficientAudioControl++;
		if (SufficientAudioControl >= 10)
		{
			SufficientAudioControl = 0;
			if (AudioFrameskip > 0)
				AudioFrameskip--;
		}
	}
	
	if (FastForwardFrameskip > 0 && FastForwardFrameskipControl > 0)
	{
		FastForwardFrameskipControl--;
		VideoFastForwarded = (VideoFastForwarded + 1) & 0xFF;
	}
	else
	{
		FastForwardFrameskipControl = FastForwardFrameskip;
		uint32_t ResolvedUserFrameskip = ResolveSetting(UserFrameskip, PerGameUserFrameskip);
		
		if (ResolvedUserFrameskip != 0)
		{
			if (UserFrameskipControl == 0)
				UserFrameskipControl = ResolvedUserFrameskip - 1;
			else
				UserFrameskipControl--;
		}
	}

	
	/*if (AudioFrameskipControl > 0)
		AudioFrameskipControl--;
	else
		AudioFrameskipControl = AudioFrameskip;*/
		
	//printf("%d\n", UserFrameskipControl);
}

uint16_t *copy_screen()
{
	/*uint32_t pitch = GBAScreenPitch;
	uint16_t *copy = malloc(GBA_SCREEN_WIDTH * GBA_SCREEN_HEIGHT * sizeof(uint16_t));
	uint16_t *dest_ptr = copy;
	uint16_t *src_ptr = GBAScreen;
	uint32_t x, y;

	for(y = 0; y < GBA_SCREEN_HEIGHT; y++)
	{
		memcpy(dest_ptr, src_ptr, GBA_SCREEN_WIDTH * sizeof(uint16_t));
		src_ptr += pitch;
		dest_ptr += GBA_SCREEN_WIDTH;
	}
*/  uint16_t *copy = malloc(GBA_SCREEN_WIDTH * GBA_SCREEN_HEIGHT * 2);
    memcpy(copy, GBAScreen, GBA_SCREEN_WIDTH * GBA_SCREEN_HEIGHT * 2);
	
	return copy;
}

void blit_to_screen(uint16_t *src, uint32_t w, uint32_t h, uint32_t dest_x, uint32_t dest_y)
{	
  uint32_t pitch = gsTexture.Width;
  uint16_t *dest_ptr = (uint16_t*)gsTexture.Mem + dest_x + (dest_y * pitch);
  uint16_t *src_ptr = src;
  uint32_t line_skip = pitch - w;
  uint32_t x, y;

  for(y = 0; y < h; y++)
  {
    for(x = 0; x < w; x++, src_ptr++, dest_ptr++)
    {
      *dest_ptr = *src_ptr;
    }
    dest_ptr += line_skip;
  }
}

static uint32_t CutString(const char* String, const uint32_t MaxWidth,
	struct StringCut* Cuts, uint32_t CutsAllocated)
{
	uint32_t Cut = 0;
	uint32_t CutStart = 0, Cur = 0, CutWidth = 0;
	uint32_t LastSpace = -1;
	bool SpaceInCut = false;
	while (String[Cur] != '\0')
	{
		if (String[Cur] != '\n')
		{
			if (String[Cur] == ' ')
			{
				LastSpace = Cur;
				SpaceInCut = true;
			}
			CutWidth += _font_width[(uint8_t) String[Cur]];
		}

		if (String[Cur] == '\n' || CutWidth > MaxWidth)
		{
			if (Cut < CutsAllocated)
				Cuts[Cut].Start = CutStart;
			if (String[Cur] == '\n')
			{
				if (Cut < CutsAllocated)
					Cuts[Cut].End = Cur;
			}
			else if (CutWidth > MaxWidth)
			{
				if (SpaceInCut)
				{
					if (Cut < CutsAllocated)
						Cuts[Cut].End = LastSpace;
					Cur = LastSpace;
				}
				else
				{
					if (Cut < CutsAllocated)
						Cuts[Cut].End = Cur;
					Cur--; // Next iteration redoes this character
				}
			}
			CutStart = Cur + 1;
			CutWidth = 0;
			SpaceInCut = false;
			Cut++;
		}
		Cur++;
	}

	if (Cut < CutsAllocated)
	{
		Cuts[Cut].Start = CutStart;
		Cuts[Cut].End = Cur;
	}
	return Cut + 1;
}

uint32_t GetSectionRenderedWidth(const char* String, const uint32_t Start, const uint32_t End)
{
	uint32_t Result = 0, i;
	for (i = Start; i < End; i++)
		Result += _font_width[(uint8_t) String[i]];
	return Result;
}

void PrintString(const char* String, uint16_t TextColor,
	void* Dest, uint32_t DestPitch, uint32_t X, uint32_t Y, uint32_t Width, uint32_t Height,
	enum HorizontalAlignment HorizontalAlignment, enum VerticalAlignment VerticalAlignment)
{
	struct StringCut* Cuts = malloc((Height / _font_height) * sizeof(struct StringCut));
	uint32_t CutCount = CutString(String, Width, Cuts, Height / _font_height), Cut;
	if (CutCount > Height / _font_height)
		CutCount = Height / _font_height;

	for (Cut = 0; Cut < CutCount; Cut++)
	{
		uint32_t TextWidth = GetSectionRenderedWidth(String, Cuts[Cut].Start, Cuts[Cut].End);
		uint32_t LineX, LineY;
		switch (HorizontalAlignment)
		{
			case LEFT:   LineX = X;                           break;
			case CENTER: LineX = (Width - TextWidth) / 2; break;
			case RIGHT:  LineX = (Width) - TextWidth - X;     break;
			default:     LineX = 0; /* shouldn't happen */    break;
		}
		switch (VerticalAlignment)
		{
			case TOP:
				LineY = Y + Cut * _font_height;
				break;
			case MIDDLE:
				LineY = (Height - CutCount * _font_height) / 2 + Cut * _font_height;
				break;
			case BOTTOM:
				LineY = (Y + Height) - (CutCount - Cut) * _font_height;
				break;
			default:
				LineY = 0; /* shouldn't happen */
				break;
		}

		uint32_t Cur;
		for (Cur = Cuts[Cut].Start; Cur < Cuts[Cut].End; Cur++)
		{
			uint32_t glyph_offset = (uint32_t) String[Cur] * _font_height;
			uint32_t glyph_width = _font_width[(uint8_t) String[Cur]];
			uint32_t glyph_column, glyph_row;
			uint16_t current_halfword;

			for(glyph_row = 0; glyph_row < _font_height; glyph_row++, glyph_offset++)
			{
				current_halfword = _font_bits[glyph_offset];
				for (glyph_column = 0; glyph_column < glyph_width; glyph_column++)
				{
					if ((current_halfword >> (15 - glyph_column)) & 0x01)
						*(uint16_t*) ((uint8_t*) Dest + (LineY + glyph_row) * DestPitch + (LineX + glyph_column) * sizeof(uint16_t)) = TextColor;
				}
			}

			LineX += glyph_width;
		}
	}

	free(Cuts);
}

uint32_t GetRenderedWidth(const char* str)
{
	struct StringCut* Cuts = malloc(sizeof(struct StringCut));
	uint32_t CutCount = CutString(str, UINT32_MAX, Cuts, 1);
	if (CutCount > 1)
	{
		Cuts = realloc(Cuts, CutCount * sizeof(struct StringCut));
		CutString(str, UINT32_MAX, Cuts, CutCount);
	}

	uint32_t Result = 0, LineWidth, Cut;
	for (Cut = 0; Cut < CutCount; Cut++)
	{
		LineWidth = 0;
		uint32_t Cur;
		for (Cur = Cuts[Cut].Start; Cur < Cuts[Cut].End; Cur++)
		{
			LineWidth += _font_width[(uint8_t) str[Cur]];
		}
		if (LineWidth > Result)
			Result = LineWidth;
	}

	free(Cuts);

	return Result;
}

uint32_t GetRenderedHeight(const char* str)
{
	return CutString(str, UINT32_MAX, NULL, 0) * _font_height;
}

void PrintStringOutline(const char* String, uint16_t TextColor, uint16_t OutlineColor,
	void* Dest, uint32_t DestPitch, uint32_t X, uint32_t Y, uint32_t Width, uint32_t Height,
	enum HorizontalAlignment HorizontalAlignment, enum VerticalAlignment VerticalAlignment)
{
	uint32_t sx, sy;
	for (sx = 0; sx <= 2; sx++)
		for (sy = 0; sy <= 2; sy++)
			if (!(sx == 1 && sy == 1))
				PrintString(String, OutlineColor, Dest, DestPitch, X + sx, Y + sy, Width - 2, Height - 2, HorizontalAlignment, VerticalAlignment);
	PrintString(String, TextColor, Dest, DestPitch, X + 1, Y + 1, Width - 2, Height - 2, HorizontalAlignment, VerticalAlignment);
}

static void ProgressUpdateInternal(uint32_t Current, uint32_t Total)
{
	char* Line;
	switch (CurrentFileAction)
	{
		case FILE_ACTION_LOAD_BIOS:
			Line = "Reading the GBA BIOS";
			break;
		case FILE_ACTION_LOAD_BATTERY:
			Line = "Reading saved data";
			break;
		case FILE_ACTION_SAVE_BATTERY:
			Line = "Writing saved data";
			break;
		case FILE_ACTION_LOAD_STATE:
			Line = "Reading saved state";
			break;
		case FILE_ACTION_SAVE_STATE:
			Line = "Writing saved state";
			break;
		case FILE_ACTION_LOAD_ROM_FROM_FILE:
			Line = "Reading ROM from a file";
			break;
		case FILE_ACTION_DECOMPRESS_ROM_TO_RAM:
			Line = "Decompressing ROM";
			break;
		case FILE_ACTION_DECOMPRESS_ROM_TO_FILE:
			Line = "Decompressing ROM into a file";
			break;
		case FILE_ACTION_APPLY_GAME_COMPATIBILITY:
			Line = "Applying compatibility fixes";
			break;
		case FILE_ACTION_LOAD_GLOBAL_SETTINGS:
			Line = "Reading global settings";
			break;
		case FILE_ACTION_SAVE_GLOBAL_SETTINGS:
			Line = "Writing global settings";
			break;
		case FILE_ACTION_LOAD_GAME_SETTINGS:
			Line = "Loading per-game settings";
			break;
		case FILE_ACTION_SAVE_GAME_SETTINGS:
			Line = "Writing per-game settings";
			break;
		default:
			Line = "File action ongoing";
			break;
	}
	
	clear_screen(COLOR_PROGRESS_BACKGROUND);

	draw_rect((gsTexture.Width - PROGRESS_WIDTH) / 2, (gsTexture.Height - PROGRESS_HEIGHT) / 2, PROGRESS_WIDTH, 1, COLOR_PROGRESS_OUTLINE);
	
	draw_rect((gsTexture.Width - PROGRESS_WIDTH) / 2, (gsTexture.Height - PROGRESS_HEIGHT) / 2 + PROGRESS_HEIGHT - 1, PROGRESS_WIDTH, 1, COLOR_PROGRESS_OUTLINE);
	
	draw_rect((gsTexture.Width - PROGRESS_WIDTH) / 2, (gsTexture.Height - PROGRESS_HEIGHT) / 2, 1, PROGRESS_HEIGHT, COLOR_PROGRESS_OUTLINE);
	
	draw_rect((gsTexture.Width + PROGRESS_WIDTH) / 2 - 1, (gsTexture.Height - PROGRESS_HEIGHT) / 2, 1, PROGRESS_HEIGHT, COLOR_PROGRESS_OUTLINE);
	
	draw_rect((gsTexture.Width - PROGRESS_WIDTH) / 2 + 1, (gsTexture.Height - PROGRESS_HEIGHT) / 2 + 1, (uint32_t) ((uint64_t) Current * (PROGRESS_WIDTH - 2) / Total), PROGRESS_HEIGHT - 2, COLOR_PROGRESS_CONTENT);

/*	SDL_Rect TopLine = { (gsTexture->Width - PROGRESS_WIDTH) / 2, (gsTexture->Height - PROGRESS_HEIGHT) / 2, PROGRESS_WIDTH, 1 };
	SDL_FillRect(OutputSurface, &TopLine, COLOR_PROGRESS_OUTLINE);

	SDL_Rect BottomLine = { (gsTexture->Width - PROGRESS_WIDTH) / 2, (gsTexture->Height - PROGRESS_HEIGHT) / 2 + PROGRESS_HEIGHT - 1, PROGRESS_WIDTH, 1 };
	SDL_FillRect(OutputSurface, &BottomLine, COLOR_PROGRESS_OUTLINE);

	SDL_Rect LeftLine = { (gsTexture->Width - PROGRESS_WIDTH) / 2, (gsTexture->Height - PROGRESS_HEIGHT) / 2, 1, PROGRESS_HEIGHT };
	SDL_FillRect(OutputSurface, &LeftLine, COLOR_PROGRESS_OUTLINE);

	SDL_Rect RightLine = { (gsTexture->Width + PROGRESS_WIDTH) / 2 - 1, (gsTexture->Height - PROGRESS_HEIGHT) / 2, 1, PROGRESS_HEIGHT };
	SDL_FillRect(OutputSurface, &RightLine, COLOR_PROGRESS_OUTLINE);

	SDL_Rect Content = { (gsTexture->Width - PROGRESS_WIDTH) / 2 + 1, (gsTexture->Height - PROGRESS_HEIGHT) / 2 + 1, (uint32_t) ((uint64_t) Current * (PROGRESS_WIDTH - 2) / Total), PROGRESS_HEIGHT - 2 };
	SDL_FillRect(OutputSurface, &Content, COLOR_PROGRESS_CONTENT);
*/
	PrintStringOutline(Line, COLOR_PROGRESS_TEXT_CONTENT, COLOR_PROGRESS_TEXT_OUTLINE, gsTexture.Mem, gsTexture.Width * 2, 0, 0, gsTexture.Width, gsTexture.Height, CENTER, MIDDLE);
	
	u8 temp = use_scaler;
	use_scaler = 0;
	
	ReGBA_VideoFlip();
	
	use_scaler = temp;
}

void ReGBA_ProgressInitialise(enum ReGBA_FileAction Action)
{
	pause_audio();

	if (Action == FILE_ACTION_SAVE_BATTERY)
		return; // Ignore this completely, because it flashes in-game
	clock_gettime(&LastProgressUpdate);
	CurrentFileAction = Action;
	InFileAction = true;

	ProgressUpdateInternal(0, 1);
}

void ReGBA_ProgressUpdate(uint32_t Current, uint32_t Total)
{
	struct timespec Now, Difference;
	clock_gettime(&Now);
	Difference = TimeDifference(LastProgressUpdate, Now);
	if (InFileAction &&
	    (Difference.tv_sec > 0 || Difference.tv_nsec > 50000000 || Current == Total)
	   )
	{
		ProgressUpdateInternal(Current, Total);
		LastProgressUpdate = Now;
	}
}

void ReGBA_ProgressFinalise()
{
	InFileAction = false;
}
