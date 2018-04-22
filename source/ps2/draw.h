#ifndef __DRAW_H__
#define __DRAW_H__

#include "port.h"

extern uint_fast8_t AudioFrameskip;
extern uint_fast8_t AudioFrameskipControl;
extern uint_fast8_t UserFrameskipControl;

extern GSGLOBAL *gsGlobal;
extern GSTEXTURE gsTexture;

// Incremented by the video thread when it decides to skip a frame due to
// fast forwarding.
extern volatile unsigned int VideoFastForwarded;

// How many back buffers have had ApplyScaleMode called upon them?
// At 3, even a triple-buffered surface has had ApplyScaleMode called on
// all back buffers, so stopping remanence with ApplyScaleMode is wasteful.
extern uint_fast8_t FramesBordered;

enum HorizontalAlignment {
	LEFT,
	CENTER,
	RIGHT
};

enum VerticalAlignment {
	TOP,
	MIDDLE,
	BOTTOM
};

typedef enum
{
  mode_auto,
  pal,
  ntsc,
  dtv_480p,
  dtv_720p,
  dtv_1080i,
} video_mode_type;

typedef enum
{
  fullscreen,
  original,
} video_ratio_type;

typedef enum
{
  linear,
  nearest,
} video_filter_type;

typedef enum
{
  menu2x,
  menu3x,
} menu_res_type;

extern video_filter_type PerGameVideoFilter;
extern video_filter_type VideoFilter;

extern video_ratio_type PerGameScreenRatio;
extern video_ratio_type ScreenRatio;

extern video_mode_type PerGameVideoMode;
extern video_mode_type VideoMode;
extern video_mode_type OldVideoMode;

extern menu_res_type MenuRes;
extern menu_res_type PerGameMenuRes;

extern uint32_t ScreenPosX;
extern uint32_t ScreenPosY;

extern uint32_t CurrentScreenPosX;
extern uint32_t CurrentScreenPosY;
extern uint32_t ScreenOverscanX;
extern uint32_t CurrentScreenOverscanX;
extern uint32_t ScreenOverscanY;
extern uint32_t CurrentScreenOverscanY;

void init_video();
void calibrate_screen_pos();
void clear_screens();

/*
 * Tells the drawing subsystem that it should start calling ApplyScaleMode
 * again to stop image remanence. This is called by the menu and progress
 * functions to indicate that their screens may have created artifacts around
 * the GBA screen.
 */
extern void ScaleModeUnapplied();

//extern void gba_render_half(uint16_t* Dest, uint16_t* Src, uint32_t DestX, uint32_t DestY,
//	uint32_t SrcPitch, uint32_t DestPitch);

void PrintString(const char* String, uint16_t TextColor,
	void* Dest, uint32_t DestPitch, uint32_t X, uint32_t Y, uint32_t Width, uint32_t Height,
	enum HorizontalAlignment HorizontalAlignment, enum VerticalAlignment VerticalAlignment);

void PrintStringOutline(const char* String, uint16_t TextColor, uint16_t OutlineColor,
	void* Dest, uint32_t DestPitch, uint32_t X, uint32_t Y, uint32_t Width, uint32_t Height,
	enum HorizontalAlignment HorizontalAlignment, enum VerticalAlignment VerticalAlignment);

extern uint32_t GetRenderedWidth(const char* str);

extern uint32_t GetRenderedHeight(const char* str);

void ReGBA_VideoFlip();

void SetMenuResolution();

void SetGameResolution();

void clear_screen(uint16_t color);
void draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t color);
void blit_to_screen(uint16_t *src, uint32_t w, uint32_t h, uint32_t dest_x, uint32_t dest_y);
void reload_video();

/*
 * Returns a new allocation containing a copy of the GBA screen. Its pixels
 * and lines are packed (the pitch is 480 bytes), and its pixel format is
 * XBGR 1555.
 * Output assertions:
 *   The returned pointer is non-NULL. If it is NULL, then this is a fatal
 *   error.
 */
extern uint16_t* copy_screen();

/*#define RGB888_TO_RGB565(r, g, b) ( \
  (((r) & 0xF8) << 8) | \
  (((g) & 0xFC) << 3) | \
  (((b) & 0xF8) >> 3) \
  )*/
  
#define RGB888_TO_RGB565(r, g, b) ( \
  (((b) & 0xF8) << 8) | \
  (((g) & 0xFC) << 3) | \
  (((r) & 0xF8) >> 3) \
  )

#endif /* __DRAW_H__ */
