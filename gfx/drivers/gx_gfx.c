/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "../../configuration.h"
#include "../../general.h"
#include "../drivers_font_renderer/bitmap.h"
#include "../../menu/menu_driver.h"
#include "../../menu/menu_display.h"
#include "../video_viewport.h"
#include "../video_monitor.h"

#ifdef HW_RVL
#include "../../wii/mem2_manager.h"
#endif

#include <gccore.h>
#include <ogcsys.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>

#include "ppc_asm.h"
#include "gx_gfx_inl.h"
#include "gx_sdk_defines.h"

struct gx_overlay_data
{
   GXTexObj tex;
   float tex_coord[8];
   float vertex_coord[8];
   float alpha_mod;
};

typedef struct gx_video
{
   uint64_t frame_count;
   bool should_resize;
   bool double_strike;
   bool rgb32;
   uint32_t *menu_data; /* FIXME: Should be const uint16_t*. */
   bool menu_texture_enable;
   video_viewport_t vp;
   unsigned scale;
#ifdef HAVE_OVERLAY
   struct gx_overlay_data *overlay;
   unsigned overlays;
   bool overlay_enable;
   bool overlay_full_screen;
#endif
} gx_video_t;

#define SYSMEM1_SIZE 0x01800000

void *g_framebuf[2];
unsigned g_current_framebuf;

bool g_vsync;
OSCond g_video_cond;
volatile bool g_draw_done;
uint32_t g_orientation;

static struct
{
   uint32_t *data; /* needs to be resizable. */
   unsigned width;
   unsigned height;
   GXTexObj obj;
} g_tex;

static struct
{
   uint32_t data[240 * 200];
   GXTexObj obj;
} menu_tex ATTRIBUTE_ALIGN(32);

uint8_t gx_fifo[256 * 1024] ATTRIBUTE_ALIGN(32);
uint8_t display_list[1024] ATTRIBUTE_ALIGN(32);
uint16_t gx_width, gx_height;
size_t display_list_size;
GXRModeObj gx_mode;
unsigned gx_old_width, gx_old_height;

static u8 _gxtexmode0ids[8] = {0x80,0x81,0x82,0x83,0xA0,0xA1,0xA2,0xA3};
static u8 _gxtexmode1ids[8] = {0x84,0x85,0x86,0x87,0xA4,0xA5,0xA6,0xA7};
static u8 _gxteximg0ids[8] = {0x88,0x89,0x8A,0x8B,0xA8,0xA9,0xAA,0xAB};
static u8 _gxteximg3ids[8] = {0x94,0x95,0x96,0x97,0xB4,0xB5,0xB6,0xB7};

float verts[16] ATTRIBUTE_ALIGN(32) = {
   -1,  1, -0.5,
    1,  1, -0.5,
   -1, -1, -0.5,
    1, -1, -0.5,
};

float vertex_ptr[8] ATTRIBUTE_ALIGN(32) = {
   0, 0,
   1, 0,
   0, 1,
   1, 1,
};

u8 color_ptr[16] ATTRIBUTE_ALIGN(32)  = {
   0xFF, 0xFF, 0xFF, 0xFF,
   0xFF, 0xFF, 0xFF, 0xFF,
   0xFF, 0xFF, 0xFF, 0xFF,
   0xFF, 0xFF, 0xFF, 0xFF,
};

enum
{
   GX_RESOLUTIONS_512_192 = 0,
   GX_RESOLUTIONS_598_200,
   GX_RESOLUTIONS_640_200,
   GX_RESOLUTIONS_384_224,
   GX_RESOLUTIONS_448_224,
   GX_RESOLUTIONS_480_224,
   GX_RESOLUTIONS_512_224,
   GX_RESOLUTIONS_576_224,
   GX_RESOLUTIONS_608_224,
   GX_RESOLUTIONS_640_224,
   GX_RESOLUTIONS_340_232,
   GX_RESOLUTIONS_512_232,
   GX_RESOLUTIONS_512_236,
   GX_RESOLUTIONS_336_240,
   GX_RESOLUTIONS_352_240,
   GX_RESOLUTIONS_384_240,
   GX_RESOLUTIONS_512_240,
   GX_RESOLUTIONS_530_240,
   GX_RESOLUTIONS_608_240,
   GX_RESOLUTIONS_640_240,
   GX_RESOLUTIONS_512_384,
   GX_RESOLUTIONS_598_400,
   GX_RESOLUTIONS_640_400,
   GX_RESOLUTIONS_384_448,
   GX_RESOLUTIONS_448_448,
   GX_RESOLUTIONS_480_448,
   GX_RESOLUTIONS_512_448,
   GX_RESOLUTIONS_576_448,
   GX_RESOLUTIONS_608_448,
   GX_RESOLUTIONS_640_448,
   GX_RESOLUTIONS_340_464,
   GX_RESOLUTIONS_512_464,
   GX_RESOLUTIONS_512_472,
   GX_RESOLUTIONS_352_480,
   GX_RESOLUTIONS_384_480,
   GX_RESOLUTIONS_512_480,
   GX_RESOLUTIONS_530_480,
   GX_RESOLUTIONS_608_480,
   GX_RESOLUTIONS_640_480,
   GX_RESOLUTIONS_LAST,
};

static unsigned menu_current_gx_resolution = GX_RESOLUTIONS_640_480;

unsigned menu_gx_resolutions[GX_RESOLUTIONS_LAST][2] = {
   { 512, 192 },
   { 598, 200 },
   { 640, 200 },
   { 384, 224 },
   { 448, 224 },
   { 480, 224 },
   { 512, 224 },
   { 576, 224 },
   { 608, 224 },
   { 640, 224 },
   { 340, 232 },
   { 512, 232 },
   { 512, 236 },
   { 336, 240 },
   { 352, 240 },
   { 384, 240 },
   { 512, 240 },
   { 530, 240 },
   { 608, 240 },
   { 640, 240 },
   { 512, 384 },
   { 598, 400 },
   { 640, 400 },
   { 384, 448 },
   { 448, 448 },
   { 480, 448 },
   { 512, 448 },
   { 576, 448 },
   { 608, 448 },
   { 640, 448 },
   { 340, 464 },
   { 512, 464 },
   { 512, 472 },
   { 352, 480 },
   { 384, 480 },
   { 512, 480 },
   { 530, 480 },
   { 608, 480 },
   { 640, 480 },
};

extern unsigned menu_current_gx_resolution;

static void retrace_callback(u32 retrace_count)
{
   (void)retrace_count;
   g_draw_done = true;
   OSSignalCond(g_video_cond);
}

#ifdef HAVE_OVERLAY
static void gx_render_overlay(void *data);
static void gx_free_overlay(gx_video_t *gx)
{
   free(gx->overlay);
   gx->overlay = NULL;
   gx->overlays = 0;
   GX_InvalidateTexAll();
}
#endif

static void gx_set_video_mode(void *data, unsigned fbWidth, unsigned lines,
      bool fullscreen)
{
   bool progressive;
   unsigned modetype, level, viHeightMultiplier, viWidth, tvmode,
            max_width, max_height, i;
   gx_video_t *gx             = (gx_video_t*)data;
   menu_framebuf_t *frame_buf = menu_display_fb_get_ptr();
   settings_t *settings       = config_get_ptr();

   (void)level;

   _CPU_ISR_Disable(level);
   VISetBlack(true);
   VIFlush();
   viHeightMultiplier = 1;
   viWidth    = settings->video.viwidth;
#if defined(HW_RVL)
   progressive = CONF_GetProgressiveScan() > 0 && VIDEO_HaveComponentCable();
   switch (CONF_GetVideo())
   {
      case CONF_VIDEO_PAL:
         if (CONF_GetEuRGB60() > 0)
            tvmode = VI_EURGB60;
         else
            tvmode = VI_PAL;
         break;
      case CONF_VIDEO_MPAL:
         tvmode = VI_MPAL;
         break;
      default:
         tvmode = VI_NTSC;
         break;
   }
#else
   progressive = VIDEO_HaveComponentCable();
   tvmode = VIDEO_GetCurrentTvMode();
#endif
   switch (tvmode)
   {
      case VI_PAL:
         max_width = VI_MAX_WIDTH_PAL;
         max_height = /* VI_MAX_HEIGHT_PAL */ 574;
         break;
      case VI_MPAL:
         max_width = VI_MAX_WIDTH_MPAL;
         max_height = VI_MAX_HEIGHT_MPAL;
         break;
      case VI_EURGB60:
         max_width = VI_MAX_WIDTH_NTSC;
         max_height = VI_MAX_HEIGHT_NTSC;
         break;
      default:
         tvmode = VI_NTSC;
         max_width = VI_MAX_WIDTH_EURGB60;
         max_height = VI_MAX_HEIGHT_EURGB60;
         break;
   }

   if (lines == 0 || fbWidth == 0)
   {
      GXRModeObj tmp_mode;
      VIDEO_GetPreferredMode(&tmp_mode);
      fbWidth = tmp_mode.fbWidth;
      lines = tmp_mode.xfbHeight;
   }

   if (lines <= max_height / 2)
   {
      modetype = VI_NON_INTERLACE;
      viHeightMultiplier = 2;
   }
   else
   {
      modetype = (progressive) ? VI_PROGRESSIVE : VI_INTERLACE;
   }

   if (lines > max_height)
      lines = max_height;

   if (fbWidth > max_width)
      fbWidth = max_width;

   gx_mode.viTVMode = VI_TVMODE(tvmode, modetype);
   gx_mode.fbWidth = fbWidth;
   gx_mode.efbHeight = min(lines, 480);

   if (modetype == VI_NON_INTERLACE && lines > max_height / 2)
      gx_mode.xfbHeight = max_height / 2;
   else if (modetype != VI_NON_INTERLACE && lines > max_height)
      gx_mode.xfbHeight = max_height;
   else
      gx_mode.xfbHeight = lines;

   gx_mode.viWidth = viWidth;
   gx_mode.viHeight = gx_mode.xfbHeight * viHeightMultiplier;
   gx_mode.viXOrigin = (max_width - gx_mode.viWidth) / 2;
   gx_mode.viYOrigin = 
      (max_height - gx_mode.viHeight) / (2 * viHeightMultiplier);
   gx_mode.xfbMode = modetype == VI_INTERLACE ? VI_XFBMODE_DF : VI_XFBMODE_SF;
   gx_mode.field_rendering = GX_FALSE;
   gx_mode.aa = GX_FALSE;

   for (i = 0; i < 12; i++)
      gx_mode.sample_pattern[i][0] = gx_mode.sample_pattern[i][1] = 6;

   if (modetype == VI_INTERLACE)
   {
      gx_mode.vfilter[0] = 8;
      gx_mode.vfilter[1] = 8;
      gx_mode.vfilter[2] = 10;
      gx_mode.vfilter[3] = 12;
      gx_mode.vfilter[4] = 10;
      gx_mode.vfilter[5] = 8;
      gx_mode.vfilter[6] = 8;
   }
   else
   {
      gx_mode.vfilter[0] = 0;
      gx_mode.vfilter[1] = 0;
      gx_mode.vfilter[2] = 21;
      gx_mode.vfilter[3] = 22;
      gx_mode.vfilter[4] = 21;
      gx_mode.vfilter[5] = 0;
      gx_mode.vfilter[6] = 0;
   }

   gx->vp.full_width = gx_mode.fbWidth;
   gx->vp.full_height = gx_mode.xfbHeight;
   gx->double_strike = (modetype == VI_NON_INTERLACE);
   gx->should_resize = true;

   VIConfigure(&gx_mode);
   VIDEO_ClearFrameBuffer(&gx_mode, g_framebuf[0], COLOR_BLACK);
   VIDEO_ClearFrameBuffer(&gx_mode, g_framebuf[1], COLOR_BLACK);
   VISetNextFrameBuffer(g_framebuf[0]);
   VISetPostRetraceCallback(retrace_callback);
   VISetBlack(false);
   VIFlush();

   GX_SetViewportJitter(0, 0, gx_mode.fbWidth, gx_mode.efbHeight, 0, 1, 1);
   GX_SetDispCopySrc(0, 0, gx_mode.fbWidth, gx_mode.efbHeight);

   f32 y_scale = GX_GetYScaleFactor(gx_mode.efbHeight, gx_mode.xfbHeight);
   u16 xfbWidth = VIPadFrameBufferWidth(gx_mode.fbWidth);
   u16 xfbHeight = GX_SetDispCopyYScale(y_scale);
   (void)xfbHeight;
   GX_SetDispCopyDst(xfbWidth, xfbHeight);

   GX_SetCopyFilter(gx_mode.aa, gx_mode.sample_pattern,
         (gx_mode.xfbMode == VI_XFBMODE_SF) ? GX_FALSE : settings->video.vfilter,
         gx_mode.vfilter);
   GXColor color = { 0, 0, 0, 0xff };
   GX_SetCopyClear(color, GX_MAX_Z24);
   GX_SetFieldMode(gx_mode.field_rendering,
         (gx_mode.viHeight == 2 * gx_mode.xfbHeight) ? GX_ENABLE : GX_DISABLE);
   GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);
   GX_InvalidateTexAll();
   GX_Flush();
   _CPU_ISR_Restore(level);

   RARCH_LOG("GX Resolution: %dx%d (%s)\n", gx_mode.fbWidth,
         gx_mode.efbHeight, (gx_mode.viTVMode & 3) == VI_INTERLACE 
         ? "interlaced" : "progressive");

   if (frame_buf)
   {
      frame_buf->height = gx_mode.efbHeight / (gx->double_strike ? 1 : 2);
      frame_buf->height &= ~3;
      if (frame_buf->height > 240)
         frame_buf->height = 240;

      frame_buf->width = gx_mode.fbWidth / (gx_mode.fbWidth < 400 ? 1 : 2);
      frame_buf->width &= ~3;
      if (frame_buf->width > 400)
         frame_buf->width = 400;
      frame_buf->pitch = frame_buf->width * 2;
   }

   if (tvmode == VI_PAL)
   {
      if (modetype == VI_NON_INTERLACE)
         driver_set_refresh_rate(50.0801f);
      else
         driver_set_refresh_rate(50.0f);
   }
   else
   {
      if (modetype == VI_NON_INTERLACE)
         driver_set_refresh_rate(59.8261f);
      else
         driver_set_refresh_rate(59.94f);
   }

   /* custom viewports for older resolutions will most likely be corrupted, reset them */
   video_viewport_reset_custom();

   g_current_framebuf = 0;
}

static void gx_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   driver_t *driver = driver_get_ptr();
   gx_video_t *gx = (gx_video_t*)driver->video_data;
   const struct retro_system_av_info *av_info = 
      (const struct retro_system_av_info*)video_viewport_get_system_av_info();

   if (aspect_ratio_idx == ASPECT_RATIO_SQUARE)
      video_viewport_set_square_pixel(
            av_info->geometry.base_width,
            av_info->geometry.base_height);
   else if (aspect_ratio_idx == ASPECT_RATIO_CORE)
      video_viewport_set_core();
   else if (aspect_ratio_idx == ASPECT_RATIO_CONFIG)
      video_viewport_set_config();

   video_driver_set_aspect_ratio_value(
         aspectratio_lut[aspect_ratio_idx].value);

   if (!gx)
      return;

   gx->should_resize = true;
}

static void setup_video_mode(void *data)
{
   unsigned i;
   if (!g_framebuf[0])
      for (i = 0; i < 2; i++)
         g_framebuf[i] = MEM_K0_TO_K1(
               memalign(32, 640 * 576 * VI_DISPLAY_PIX_SZ));

   g_current_framebuf = 0;
   g_draw_done = true;
   g_orientation = ORIENTATION_NORMAL;
   OSInitThreadQueue(&g_video_cond);

   VIDEO_GetPreferredMode(&gx_mode);
   gx_set_video_mode(data, 0, 0, true);
}

static void init_texture(void *data, unsigned width, unsigned height)
{
   unsigned g_filter, menu_w, menu_h;
   struct __gx_regdef *__gx     = (struct __gx_regdef*)__gxregs;
   gx_video_t *gx               = (gx_video_t*)data;
   struct __gx_texobj *fb_ptr   = (struct __gx_texobj*)&g_tex.obj;
   struct __gx_texobj *menu_ptr = (struct __gx_texobj*)&menu_tex.obj;
   menu_handle_t *menu          = menu_driver_get_ptr();
   menu_framebuf_t *frame_buf   = menu_display_fb_get_ptr();
   settings_t *settings         = config_get_ptr();

   width &= ~3;
   height &= ~3;
   g_filter = settings->video.smooth ? GX_LINEAR : GX_NEAR;
   menu_w = 320;
   menu_h = 240;

   if (menu)
   {
      menu_w = frame_buf->width;
      menu_h = frame_buf->height;
   }

   __GX_InitTexObj(fb_ptr, g_tex.data, width, height,
         (gx->rgb32) ? GX_TF_RGBA8 : gx->menu_texture_enable ? 
         GX_TF_RGB5A3 : GX_TF_RGB565,
         GX_CLAMP, GX_CLAMP, GX_FALSE);
   __GX_InitTexObjFilterMode(fb_ptr, g_filter, g_filter);
   __GX_InitTexObj(menu_ptr, menu_tex.data, menu_w, menu_h,
         GX_TF_RGB5A3, GX_CLAMP, GX_CLAMP, GX_FALSE);
   __GX_InitTexObjFilterMode(menu_ptr, g_filter, g_filter);
   __GX_InvalidateTexAll(__gx);
}

static void init_vtx(void *data, const video_info_t *video)
{
   gx_video_t *gx = (gx_video_t*)data;

   GX_SetCullMode(GX_CULL_NONE);
   GX_SetClipMode(GX_CLIP_DISABLE);
   GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);
   GX_SetZMode(GX_ENABLE, GX_ALWAYS, GX_ENABLE);
   GX_SetColorUpdate(GX_TRUE);
   GX_SetAlphaUpdate(GX_FALSE);

   Mtx44 m;
   guOrtho(m, 1, -1, -1, 1, 0.4, 0.6);
   GX_LoadProjectionMtx(m, GX_ORTHOGRAPHIC);

   GX_ClearVtxDesc();
   GX_SetVtxDesc(GX_VA_POS, GX_INDEX8);
   GX_SetVtxDesc(GX_VA_TEX0, GX_INDEX8);
   GX_SetVtxDesc(GX_VA_CLR0, GX_INDEX8);

   GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
   GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
   GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
   GX_SetArray(GX_VA_POS, verts, 3 * sizeof(float));
   GX_SetArray(GX_VA_TEX0, vertex_ptr, 2 * sizeof(float));
   GX_SetArray(GX_VA_CLR0, color_ptr, 4 * sizeof(u8));

   GX_SetNumTexGens(1);
   GX_SetNumChans(1);
   GX_SetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_REG,
         GX_SRC_VTX, GX_LIGHTNULL, GX_DF_NONE, GX_AF_NONE);
   GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
   GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
   GX_InvVtxCache();

   GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA,
         GX_BL_INVSRCALPHA, GX_LO_CLEAR);

   if (gx->scale != video->input_scale ||
         gx->rgb32 != video->rgb32)
   {
      RARCH_LOG("[GX] reallocate texture\n");
      free(g_tex.data);
      g_tex.data = memalign(32,
            RARCH_SCALE_BASE * RARCH_SCALE_BASE * video->input_scale *
            video->input_scale * (video->rgb32 ? 4 : 2));
      g_tex.width = g_tex.height = RARCH_SCALE_BASE * video->input_scale;

      if (!g_tex.data)
      {
         RARCH_ERR("[GX] Error allocating video texture\n");
         exit(1);
      }
   }

   DCFlushRange(g_tex.data, g_tex.width *
         g_tex.height * video->rgb32 ? 4 : 2);

   gx->rgb32 = video->rgb32;
   gx->scale = video->input_scale;
   gx->should_resize = true;

   init_texture(data, g_tex.width, g_tex.height);
   GX_Flush();
}

static void build_disp_list(void)
{
   DCInvalidateRange(display_list, sizeof(display_list));
   GX_BeginDispList(display_list, sizeof(display_list));
   GX_Begin(GX_TRIANGLESTRIP, GX_VTXFMT0, 4);
   for (unsigned i = 0; i < 4; i++)
   {
      GX_Position1x8(i);
      GX_Color1x8(i);
      GX_TexCoord1x8(i);
   }
   GX_End();
   display_list_size = GX_EndDispList();
}

#if 0
#define TAKE_EFB_SCREENSHOT_ON_EXIT
#endif

#ifdef TAKE_EFB_SCREENSHOT_ON_EXIT

/* Adapted from code by Crayon for GRRLIB (http://code.google.com/p/grrlib) */
static void gx_efb_screenshot(void)
{
   int x, y;

   uint8_t tga_header[] = {0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x02, 0xE0, 0x01, 0x18, 0x00};
   FILE *out = fopen("/screenshot.tga", "wb");

   if (!out)
      return;

   fwrite(tga_header, 1, sizeof(tga_header), out);

   for (y = 479; y >= 0; --y)
   {
      uint8_t line[640 * 3];
      unsigned i = 0;
      for (x = 0; x < 640; x++)
      {
         GXColor color;
         GX_PeekARGB(x, y, &color);
         line[i++] = color.b;
         line[i++] = color.g;
         line[i++] = color.r;
      }
      fwrite(line, 1, sizeof(line), out);
   }

   fclose(out);
}

#endif

static void *gx_init(const video_info_t *video,
      const input_driver_t **input, void **input_data)
{
   gx_video_t *gx = (gx_video_t*)calloc(1, sizeof(gx_video_t));
   if (!gx)
      return NULL;

   void *gxinput = input_gx.init();
   *input = gxinput ? &input_gx : NULL;
   *input_data = gxinput;

   VIInit();
   GX_Init(gx_fifo, sizeof(gx_fifo));
   g_vsync = video->vsync;

   setup_video_mode(gx);
   init_vtx(gx, video);
   build_disp_list();

   gx->vp.full_width = gx_mode.fbWidth;
   gx->vp.full_height = gx_mode.xfbHeight;
   gx->should_resize = true;
   gx_old_width = gx_old_height = 0;

   return gx;
}

#define ASM_BLITTER

#ifdef ASM_BLITTER

static void update_texture_asm(const uint32_t *src, const uint32_t *dst,
      unsigned width, unsigned height, unsigned pitch)
{
   register uint32_t tmp0, tmp1, tmp2, tmp3, line2, line2b, 
            line3, line3b, line4, line4b, line5;

   asm volatile (
      "     srwi     %[width],   %[width],   2           \n"
      "     srwi     %[height],  %[height],  2           \n"
      "     subi     %[tmp3],    %[dst],     4           \n"
      "     mr       %[dst],     %[tmp3]                 \n"
      "     subi     %[dst],     %[dst],     4           \n"
      "     mr       %[line2],   %[pitch]                \n"
      "     addi     %[line2b],  %[line2],   4           \n"
      "     mulli    %[line3],   %[pitch],   2           \n"
      "     addi     %[line3b],  %[line3],   4           \n"
      "     mulli    %[line4],   %[pitch],   3           \n"
      "     addi     %[line4b],  %[line4],   4           \n"
      "     mulli    %[line5],   %[pitch],   4           \n"

      "2:   mtctr    %[width]                            \n"
      "     mr       %[tmp0],    %[src]                  \n"

      "1:   lwz      %[tmp1],    0(%[src])               \n"
      "     stwu     %[tmp1],    8(%[dst])               \n"
      "     lwz      %[tmp2],    4(%[src])               \n"
      "     stwu     %[tmp2],    8(%[tmp3])              \n"

      "     lwzx     %[tmp1],    %[line2],   %[src]      \n"
      "     stwu     %[tmp1],    8(%[dst])               \n"
      "     lwzx     %[tmp2],    %[line2b],  %[src]      \n"
      "     stwu     %[tmp2],    8(%[tmp3])              \n"

      "     lwzx     %[tmp1],    %[line3],   %[src]      \n"
      "     stwu     %[tmp1],    8(%[dst])               \n"
      "     lwzx     %[tmp2],    %[line3b],  %[src]      \n"
      "     stwu     %[tmp2],    8(%[tmp3])              \n"

      "     lwzx     %[tmp1],    %[line4],   %[src]      \n"
      "     stwu     %[tmp1],    8(%[dst])               \n"
      "     lwzx     %[tmp2],    %[line4b],  %[src]      \n"
      "     stwu     %[tmp2],    8(%[tmp3])              \n"

      "     addi     %[src],     %[src],     8           \n"
      "     bdnz     1b                                  \n"

      "     add      %[src],     %[tmp0],    %[line5]    \n"
      "     subic.   %[height],  %[height],  1           \n"
      "     bne      2b                                  \n"
      :  [tmp0]   "=&b" (tmp0),
         [tmp1]   "=&b" (tmp1),
         [tmp2]   "=&b" (tmp2),
         [tmp3]   "=&b" (tmp3),
         [line2]  "=&b" (line2),
         [line2b] "=&b" (line2b),
         [line3]  "=&b" (line3),
         [line3b] "=&b" (line3b),
         [line4]  "=&b" (line4),
         [line4b] "=&b" (line4b),
         [line5]  "=&b" (line5),
         [dst]    "+&b"  (dst)
      :  [src]    "b"   (src),
         [width]  "b"   (width),
         [height] "b"   (height),
         [pitch]  "b"   (pitch)
      :  "cc"
   );
}

#endif

#define BLIT_LINE_16(off) \
{ \
   const uint32_t *tmp_src = src; \
   uint32_t *tmp_dst = dst; \
   for (unsigned x = 0; x < width2 >> 1; x++, tmp_src += 2, tmp_dst += 8) \
   { \
      tmp_dst[ 0 + off] = BLIT_LINE_16_CONV(tmp_src[0]); \
      tmp_dst[ 1 + off] = BLIT_LINE_16_CONV(tmp_src[1]); \
   } \
   src += tmp_pitch; \
}

#define BLIT_LINE_32(off) \
{ \
   const uint16_t *tmp_src = src; \
   uint16_t *tmp_dst = dst; \
   for (unsigned x = 0; x < width2 >> 3; x++, tmp_src += 8, tmp_dst += 32) \
   { \
      tmp_dst[  0 + off] = tmp_src[0] | 0xFF00; \
      tmp_dst[ 16 + off] = tmp_src[1]; \
      tmp_dst[  1 + off] = tmp_src[2] | 0xFF00; \
      tmp_dst[ 17 + off] = tmp_src[3]; \
      tmp_dst[  2 + off] = tmp_src[4] | 0xFF00; \
      tmp_dst[ 18 + off] = tmp_src[5]; \
      tmp_dst[  3 + off] = tmp_src[6] | 0xFF00; \
      tmp_dst[ 19 + off] = tmp_src[7]; \
   } \
   src += tmp_pitch; \
}

static void convert_texture16(const uint32_t *_src, uint32_t *_dst,
      unsigned width, unsigned height, unsigned pitch)
{
#ifdef ASM_BLITTER
   width &= ~3;
   height &= ~3;
   update_texture_asm(_src, _dst, width, height, pitch);
#else
   width &= ~3;
   height &= ~3;
   unsigned tmp_pitch = pitch >> 2;
   unsigned width2 = width >> 1;

   /* Texture data is 4x4 tiled @ 16bpp.
    * Use 32-bit to transfer more data per cycle.
    */
   const uint32_t *src = _src;
   uint32_t *dst = _dst;
   for (unsigned i = 0; i < height; i += 4, dst += 4 * width2)
   {
#define BLIT_LINE_16_CONV(x) x
         BLIT_LINE_16(0)
         BLIT_LINE_16(2)
         BLIT_LINE_16(4)
         BLIT_LINE_16(6)
#undef BLIT_LINE_16_CONV
   }
#endif
}

static void convert_texture16_conv(const uint32_t *_src, uint32_t *_dst,
      unsigned width, unsigned height, unsigned pitch)
{
   unsigned i, tmp_pitch, width2;
   width &= ~3;
   height &= ~3;
   tmp_pitch = pitch >> 2;
   width2 = width >> 1;

   const uint32_t *src = (const uint32_t*)_src;
   uint32_t *dst = (uint32_t*)_dst;
   for (i = 0; i < height; i += 4, dst += 4 * width2)
   {
#define BLIT_LINE_16_CONV(x) (0x80008000 | (((x) & 0xFFC0FFC0) >> 1) | ((x) & 0x001F001F))
         BLIT_LINE_16(0)
         BLIT_LINE_16(2)
         BLIT_LINE_16(4)
         BLIT_LINE_16(6)
#undef BLIT_LINE_16_CONV
   }
}

static void convert_texture32(const uint32_t *_src, uint32_t *_dst,
      unsigned width, unsigned height, unsigned pitch)
{
   unsigned i, tmp_pitch, width2;
   width &= ~3;
   height &= ~3;
   tmp_pitch = pitch >> 1;
   width2 = width << 1;

   const uint16_t *src = (uint16_t *) _src;
   uint16_t *dst = (uint16_t *) _dst;
   for (i = 0; i < height; i += 4, dst += 4 * width2)
   {
      BLIT_LINE_32(0)
      BLIT_LINE_32(4)
      BLIT_LINE_32(8)
      BLIT_LINE_32(12)
   }
}

static void gx_resize(void *data)
{
   gx_video_t *gx = (gx_video_t*)data;
   int x = 0, y = 0;
   unsigned width = gx->vp.full_width, height = gx->vp.full_height;
   settings_t *settings = config_get_ptr();
   const global_t *global     = (const global_t*)global_get_ptr();
   struct video_viewport *custom_vp = video_viewport_get_custom();

#ifdef HW_RVL
   VIDEO_SetTrapFilter(global->console.softfilter_enable);
#endif
   GX_SetDispCopyGamma(global->console.screen.gamma_correction);

   if (gx_mode.efbHeight >= 240) /* ignore this for custom resolutions */
   {
      float desired_aspect = video_driver_get_aspect_ratio();
      if (desired_aspect == 0.0)
         desired_aspect = 1.0;
#ifdef HW_RVL
      float device_aspect = CONF_GetAspectRatio() == CONF_ASPECT_4_3 ?
         4.0 / 3.0 : 16.0 / 9.0;
#else
      float device_aspect = 4.0 / 3.0;
#endif
      if (g_orientation == ORIENTATION_VERTICAL ||
            g_orientation == ORIENTATION_FLIPPED_ROTATED)
         desired_aspect = 1.0 / desired_aspect;
      float delta;

#ifdef RARCH_CONSOLE
      if (settings->video.aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
      {
         if (!custom_vp->width || !custom_vp->height)
         {
            custom_vp->x = 0;
            custom_vp->y = 0;
            custom_vp->width = gx->vp.full_width;
            custom_vp->height = gx->vp.full_height;
         }

         x      = custom_vp->x;
         y      = custom_vp->y;
         width  = custom_vp->width;
         height = custom_vp->height;
      }
      else
#endif
      {
         if (fabs(device_aspect - desired_aspect) < 0.0001)
         {
            /* If the aspect ratios of screen and desired aspect ratio 
             * are sufficiently equal (floating point stuff), 
             * assume they are actually equal. */
         }
         else if (device_aspect > desired_aspect)
         {
            delta = (desired_aspect / device_aspect - 1.0) / 2.0 + 0.5;
            x     = (unsigned)(width * (0.5 - delta));
            width = (unsigned)(2.0 * width * delta);
         }
         else
         {
            delta  = (device_aspect / desired_aspect - 1.0) / 2.0 + 0.5;
            y      = (unsigned)(height * (0.5 - delta));
            height = (unsigned)(2.0 * height * delta);
         }
      }
   }

   gx->vp.x      = x;
   gx->vp.y      = y;
   gx->vp.width  = width;
   gx->vp.height = height;

   GX_SetViewportJitter(x, y, width, height, 0, 1, 1);

   Mtx44 m1, m2;
   float top = 1, bottom = -1, left = -1, right = 1;

   guOrtho(m1, top, bottom, left, right, 0, 1);
   GX_LoadPosMtxImm(m1, GX_PNMTX1);

   unsigned degrees;
   switch(g_orientation)
   {
      case ORIENTATION_VERTICAL:
         degrees = 90;
         break;
      case ORIENTATION_FLIPPED:
         degrees = 180;
         break;
      case ORIENTATION_FLIPPED_ROTATED:
         degrees = 270;
         break;
      default:
         degrees = 0;
         break;
   }
   guMtxIdentity(m2);
   guMtxRotDeg(m2, 'Z', degrees);
   guMtxConcat(m1, m2, m1);
   GX_LoadPosMtxImm(m1, GX_PNMTX0);

   init_texture(data, 4, 4);
   gx_old_width = gx_old_height = 0;
   gx->should_resize = false;
}

static void gx_blit_line(unsigned x, unsigned y, const char *message)
{
   driver_t *driver = driver_get_ptr();
   gx_video_t *gx = (gx_video_t*)driver->video_data;

   const GXColor b = {
      .r = 0x00,
      .g = 0x00,
      .b = 0x00,
      .a = 0xff
   };
   const GXColor w = {
      .r = 0xff,
      .g = 0xff,
      .b = 0xff,
      .a = 0xff
   };

   unsigned h;

   if (!*message)
      return;

   bool double_width = gx_mode.fbWidth > 400;
   unsigned width = (double_width ? 2 : 1);
   unsigned height = FONT_HEIGHT * (gx->double_strike ? 1 : 2);
   for (h = 0; h < height; h++)
   {
      GX_PokeARGB(x, y + h, b);
      if (double_width)
      {
         GX_PokeARGB(x + 1, y + h, b);
      }
   }

   x += (double_width ? 2 : 1);

   while (*message)
   {
      for (unsigned j = 0; j < FONT_HEIGHT; j++)
      {
         for (unsigned i = 0; i < FONT_WIDTH; i++)
         {
            GXColor c;
            uint8_t     rem = 1 << ((i + j * FONT_WIDTH) & 7);
            unsigned offset = (i + j * FONT_WIDTH) >> 3;
            bool col = (bitmap_bin[FONT_OFFSET((unsigned char) *message) + offset] & rem);

            if (col)
               c = w;
            else
               c = b;

            if (!gx->double_strike)
            {
               GX_PokeARGB(x + (i * width),     y + (j * 2),     c);
               if (double_width)
               {
                  GX_PokeARGB(x + (i * width) + 1, y + (j * 2),     c);
                  GX_PokeARGB(x + (i * width) + 1, y + (j * 2) + 1, c);
               }
               GX_PokeARGB(x + (i * width),     y + (j * 2) + 1, c);
            }
            else
            {
               GX_PokeARGB(x + (i * width),     y + j, c);
               if (double_width)
               {
                  GX_PokeARGB(x + (i * width) + 1, y + j, c);
               }
            }
         }
      }

      for (unsigned h = 0; h < height; h++)
      {
         GX_PokeARGB(x + (FONT_WIDTH * width), y + h, b);
         if (double_width)
         {
            GX_PokeARGB(x + (FONT_WIDTH * width) + 1, y + h, b);
         }
      }

      x += FONT_WIDTH_STRIDE * (double_width ? 2 : 1);
      message++;
   }
}

static bool gx_frame(void *data, const void *frame,
      unsigned width, unsigned height, unsigned pitch,
      const char *msg)
{
   char fps_txt[128]         = {0};
   char fps_text_buf[128]   = {0};
   gx_video_t *gx           = (gx_video_t*)data;
   struct __gx_regdef *__gx = (struct __gx_regdef*)__gxregs;
   u8 clear_efb             = GX_FALSE;
   settings_t *settings     = config_get_ptr();

   if(!gx || (!frame && !gx->menu_texture_enable))
      return true;

   if (!frame)
      width = height = 4; /* draw a black square in the background */

   if(gx->should_resize)
   {
      gx_resize(gx);
      clear_efb = GX_TRUE;
   }

   while (((g_vsync || gx->menu_texture_enable)) && !g_draw_done)
      OSSleepThread(g_video_cond);

   width = min(g_tex.width, width);
   height = min(g_tex.height, height);

   if (width != gx_old_width || height != gx_old_height)
   {
      init_texture(data, width, height);
      gx_old_width = width;
      gx_old_height = height;
   }

   g_draw_done = false;
   g_current_framebuf ^= 1;


   if (frame)
   {
      if (gx->rgb32)
         convert_texture32(frame, g_tex.data, width, height, pitch);
      else if (gx->menu_texture_enable)
         convert_texture16_conv(frame, g_tex.data, width, height, pitch);
      else
         convert_texture16(frame, g_tex.data, width, height, pitch);
      DCFlushRange(g_tex.data, height * (width << (gx->rgb32 ? 2 : 1)));
   }

   if (gx->menu_texture_enable && gx->menu_data)
   {
      menu_framebuf_t *frame_buf   = menu_display_fb_get_ptr();

      if (frame_buf)
      {
         convert_texture16(gx->menu_data, menu_tex.data,
               frame_buf->width,
               frame_buf->height,
               frame_buf->pitch);
         DCFlushRange(menu_tex.data,
               frame_buf->width * 
               frame_buf->pitch);
      }
   }

   __GX_InvalidateTexAll(__gx);

   __GX_SetCurrentMtx(__gx, GX_PNMTX0);
   __GX_LoadTexObj(&g_tex.obj, GX_TEXMAP0);
   __GX_CallDispList(__gx, display_list, display_list_size);

   if (gx->menu_texture_enable)
   {
      __GX_SetCurrentMtx(__gx, GX_PNMTX1);
      GX_LoadTexObj(&menu_tex.obj, GX_TEXMAP0);
      __GX_CallDispList(__gx, display_list, display_list_size);
   }

#ifdef HAVE_OVERLAY
   if (gx->overlay_enable)
      gx_render_overlay(gx);
#endif

   GX_DrawDone();

   video_monitor_get_fps(fps_txt, sizeof(fps_txt),
         fps_text_buf, sizeof(fps_text_buf));

   if (settings->fps_show)
   {
      char mem1_txt[128] = {0};
      char mem2_txt[128] = {0};
      unsigned x         = 15;
      unsigned y         = 35;

      (void)mem2_txt;

      gx_blit_line(x, y, fps_text_buf);
      y += FONT_HEIGHT * (gx->double_strike ? 1 : 2);
      snprintf(mem1_txt, sizeof(mem1_txt), "MEM1: %8d / %8d",
            SYSMEM1_SIZE - SYS_GetArena1Size(), SYSMEM1_SIZE);
      gx_blit_line(x, y, mem1_txt);
#ifdef HW_RVL
      y += FONT_HEIGHT * (gx->double_strike ? 1 : 2);
      snprintf(mem2_txt, sizeof(mem2_txt), "MEM2: %8d / %8d",
            gx_mem2_used(), gx_mem2_total());
      gx_blit_line(x, y, mem2_txt);
#endif
   }

   if (msg && !gx->menu_texture_enable)
   {
      unsigned x = 7 * (gx->double_strike ? 1 : 2);
      unsigned y = gx->vp.full_height - (35 * (gx->double_strike ? 1 : 2));
      gx_blit_line(x, y, msg);
      clear_efb = GX_TRUE;
   }

   __GX_CopyDisp(__gx, g_framebuf[g_current_framebuf], clear_efb);
   __GX_Flush(__gx);
   VISetNextFrameBuffer(g_framebuf[g_current_framebuf]);
   VIFlush();

   gx->frame_count++;

   return true;
}

static void gx_set_nonblock_state(void *data, bool state)
{
   (void)data;
   g_vsync = !state;
}

static bool gx_alive(void *data)
{
   (void)data;
   return true;
}

static bool gx_focus(void *data)
{
   (void)data;
   return true;
}

static bool gx_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;

   return false;
}

static bool gx_has_windowed(void *data)
{
   (void)data;
   return false;
}

static void gx_free(void *data)
{
   driver_t *driver = driver_get_ptr();
   gx_video_t *gx = (gx_video_t*)driver->video_data;

#ifdef HAVE_OVERLAY
   gx_free_overlay(gx);
#endif

   GX_DrawDone();
   GX_AbortFrame();
   GX_Flush();
   VISetBlack(true);
   VIFlush();
   VIWaitForRetrace();

   free(data);
}

static void gx_set_rotation(void *data, unsigned orientation)
{
   gx_video_t *gx = (gx_video_t*)data;
   g_orientation = orientation;

   if (gx)
      gx->should_resize = true;
}

static void gx_set_texture_frame(void *data, const void *frame,
      bool rgb32, unsigned width, unsigned height, float alpha)
{
   (void)rgb32;
   (void)width;
   (void)height;
   (void)alpha;

   gx_video_t *gx = (gx_video_t*)data;

   if (gx)
      gx->menu_data = (uint32_t*)frame;
}

static void gx_set_texture_enable(void *data, bool enable, bool full_screen)
{
   gx_video_t *gx = (gx_video_t*)data;

   (void)full_screen;

   if (gx)
   {
      gx->menu_texture_enable = enable;
      /* need to make sure the game texture is the right pixel 
       * format for menu overlay. */
      gx->should_resize = true;
   }
}

static void gx_apply_state_changes(void *data)
{
   gx_video_t *gx = (gx_video_t*)data;

   if (gx)
      gx->should_resize = true;
}

static void gx_viewport_info(void *data, struct video_viewport *vp)
{
   gx_video_t *gx = (gx_video_t*)data;
   *vp = gx->vp;
}

static bool gx_read_viewport(void *data, uint8_t *buffer)
{
   (void)data;
   (void)buffer;
   return true;
}

static void gx_get_video_output_size(void *data, unsigned *width, unsigned *height)
{
   (void)data;

   *width  = menu_gx_resolutions[menu_current_gx_resolution][0];
   *height = menu_gx_resolutions[menu_current_gx_resolution][1];
}

static void gx_get_video_output_prev(void *data)
{
   if (menu_current_gx_resolution > 0)
      menu_current_gx_resolution--;
}

static void gx_get_video_output_next(void *data)
{
   if (menu_current_gx_resolution < GX_RESOLUTIONS_LAST - 1)
   {
#ifdef HW_RVL
      if ((menu_current_gx_resolution + 1) > GX_RESOLUTIONS_640_480)
         if (CONF_GetVideo() != CONF_VIDEO_PAL)
            return;
#endif

      menu_current_gx_resolution++;
   }
}

static uint64_t gx_get_frame_count(void *data)
{
   gx_video_t *gx = (gx_video_t*)data;
   if (!gx)
      return 0;
   return gx->frame_count;
}

static const video_poke_interface_t gx_poke_interface = {
   gx_get_frame_count,
   gx_set_video_mode,
   NULL,
   gx_get_video_output_size,
   gx_get_video_output_prev,
   gx_get_video_output_next,
   NULL,
   NULL,
   gx_set_aspect_ratio,
   gx_apply_state_changes,
   gx_set_texture_frame,
   gx_set_texture_enable,
};

static void gx_get_poke_interface(void *data, const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &gx_poke_interface;
}

#ifdef HAVE_OVERLAY
static void gx_overlay_tex_geom(void *data, unsigned image, float x, float y, float w, float h);
static void gx_overlay_vertex_geom(void *data, unsigned image, float x, float y, float w, float h);
static bool gx_overlay_load(void *data, const struct texture_image *images, unsigned num_images)
{
   unsigned i;
   gx_video_t *gx = (gx_video_t*)data;

   gx_free_overlay(gx);
   gx->overlay = (struct gx_overlay_data*)calloc(num_images, sizeof(*gx->overlay));
   if (!gx->overlay)
      return false;

   gx->overlays = num_images;

   for (i = 0; i < num_images; i++)
   {
      struct gx_overlay_data *o = (struct gx_overlay_data*)&gx->overlay[i];
      GX_InitTexObj(&o->tex, images[i].pixels, images[i].width,
            images[i].height,
            GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
      GX_InitTexObjFilterMode(&g_tex.obj, GX_LINEAR, GX_LINEAR);
      DCFlushRange(images[i].pixels, images[i].width *
            images[i].height * sizeof(uint32_t));
      gx_overlay_tex_geom(gx, i, 0, 0, 1, 1); /* Default. Stretch to whole screen. */
      gx_overlay_vertex_geom(gx, i, 0, 0, 1, 1);
      gx->overlay[i].alpha_mod = 1.0f;
   }

   GX_InvalidateTexAll();
   return true;
}

static void gx_overlay_tex_geom(void *data, unsigned image,
      float x, float y, float w, float h)
{
   gx_video_t *gx = (gx_video_t*)data;
   struct gx_overlay_data *o;
   
   o = NULL;

   if (gx)
      o = (struct gx_overlay_data*)&gx->overlay[image];

   if (o)
   {
      o->tex_coord[0] = x;
      o->tex_coord[1] = y;
      o->tex_coord[2] = x + w;
      o->tex_coord[3] = y;
      o->tex_coord[4] = x;
      o->tex_coord[5] = y + h;
      o->tex_coord[6] = x + w;
      o->tex_coord[7] = y + h;
   }
}

static void gx_overlay_vertex_geom(void *data, unsigned image,
         float x, float y, float w, float h)
{
   gx_video_t *gx = (gx_video_t*)data;
   struct gx_overlay_data *o;
   
   o = NULL;
   
   /* Flipped, so we preserve top-down semantics. */
   y = 1.0f - y;
   h = -h;

   /* expand from 0 - 1 to -1 - 1 */
   x = (x * 2.0f) - 1.0f;
   y = (y * 2.0f) - 1.0f;
   w = (w * 2.0f);
   h = (h * 2.0f);

   if (gx)
      o = (struct gx_overlay_data*)&gx->overlay[image];

   if (o)
   {
      o->vertex_coord[0] = x;
      o->vertex_coord[1] = y;
      o->vertex_coord[2] = x + w;
      o->vertex_coord[3] = y;
      o->vertex_coord[4] = x;
      o->vertex_coord[5] = y + h;
      o->vertex_coord[6] = x + w;
      o->vertex_coord[7] = y + h;
   }
}

static void gx_overlay_enable(void *data, bool state)
{
   gx_video_t *gx = (gx_video_t*)data;
   gx->overlay_enable = state;
}

static void gx_overlay_full_screen(void *data, bool enable)
{
   gx_video_t *gx = (gx_video_t*)data;
   gx->overlay_full_screen = enable;
}

static void gx_overlay_set_alpha(void *data, unsigned image, float mod)
{
   gx_video_t *gx = (gx_video_t*)data;
   gx->overlay[image].alpha_mod = mod;
}

static void gx_render_overlay(void *data)
{
   gx_video_t *gx = (gx_video_t*)data;

   GX_SetCurrentMtx(GX_PNMTX1);
   GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
   GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
   GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

   for (unsigned i = 0; i < gx->overlays; i++)
   {
      GX_LoadTexObj(&gx->overlay[i].tex, GX_TEXMAP0);

      GX_Begin(GX_TRIANGLESTRIP, GX_VTXFMT0, 4);
      GX_Position3f32(gx->overlay[i].vertex_coord[0],
            gx->overlay[i].vertex_coord[1],  -0.5);
      GX_Color4u8(255, 255, 255, (u8)(gx->overlay[i].alpha_mod * 255.0f));
      GX_TexCoord2f32(gx->overlay[i].tex_coord[0],
            gx->overlay[i].tex_coord[1]);

      GX_Position3f32(gx->overlay[i].vertex_coord[2],
            gx->overlay[i].vertex_coord[3],  -0.5);
      GX_Color4u8(255, 255, 255, (u8)(gx->overlay[i].alpha_mod * 255.0f));
      GX_TexCoord2f32(gx->overlay[i].tex_coord[2],
            gx->overlay[i].tex_coord[3]);

      GX_Position3f32(gx->overlay[i].vertex_coord[4],
            gx->overlay[i].vertex_coord[5],  -0.5);
      GX_Color4u8(255, 255, 255, (u8)(gx->overlay[i].alpha_mod * 255.0f));
      GX_TexCoord2f32(gx->overlay[i].tex_coord[4],
            gx->overlay[i].tex_coord[5]);

      GX_Position3f32(gx->overlay[i].vertex_coord[6],
            gx->overlay[i].vertex_coord[7],  -0.5);
      GX_Color4u8(255, 255, 255, (u8)(gx->overlay[i].alpha_mod * 255.0f));
      GX_TexCoord2f32(gx->overlay[i].tex_coord[6],
            gx->overlay[i].tex_coord[7]);
      GX_End();
   }

   GX_SetVtxDesc(GX_VA_POS, GX_INDEX8);
   GX_SetVtxDesc(GX_VA_TEX0, GX_INDEX8);
   GX_SetVtxDesc(GX_VA_CLR0, GX_INDEX8);
}

static const video_overlay_interface_t gx_overlay_interface = {
   gx_overlay_enable,
   gx_overlay_load,
   gx_overlay_tex_geom,
   gx_overlay_vertex_geom,
   gx_overlay_full_screen,
   gx_overlay_set_alpha,
};

static void gx_get_overlay_interface(void *data, const video_overlay_interface_t **iface)
{
   (void)data;
   *iface = &gx_overlay_interface;
}
#endif

static bool gx_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   (void)data;
   (void)type;
   (void)path;

   return false; 
}

video_driver_t video_gx = {
   gx_init,
   gx_frame,
   gx_set_nonblock_state,
   gx_alive,
   gx_focus,
   gx_suppress_screensaver,
   gx_has_windowed,
   gx_set_shader,
   gx_free,
   "gx",
   NULL, /* set_viewport */
   gx_set_rotation,
   gx_viewport_info,
   gx_read_viewport,
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   gx_get_overlay_interface,
#endif
   gx_get_poke_interface,
};
