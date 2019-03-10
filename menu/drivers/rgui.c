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

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

#include <string/string_list.h>
#include <compat/posix_string.h>
#include <file/file_path.h>
#include <retro_inline.h>

#include "../menu.h"
#include "../menu_animation.h"
#include "../menu_entry.h"
#include "../menu_display.h"

#include "../../configuration.h"
#include "../../runloop.h"
#include "../../runloop_data.h"
#include "../../gfx/drivers_font_renderer/bitmap.h"

#include "shared.h"
#include "rgui.h"

#define RGUI_TERM_START_X        (frame_buf->width / 21)
#define RGUI_TERM_START_Y        (frame_buf->height / 9)
#define RGUI_TERM_WIDTH          (((frame_buf->width - RGUI_TERM_START_X - RGUI_TERM_START_X) / (FONT_WIDTH_STRIDE)))
#define RGUI_TERM_HEIGHT         (((frame_buf->height - RGUI_TERM_START_Y - RGUI_TERM_START_X) / (FONT_HEIGHT_STRIDE)) - 1)

#define WALLPAPER_WIDTH 320
#define WALLPAPER_HEIGHT 240

typedef struct
{
   char *path;
   uint16_t data[WALLPAPER_WIDTH * WALLPAPER_HEIGHT];
} wallpaper_t;

static wallpaper_t wallpaper = {NULL, {0}};
static char loaded_theme[PATH_MAX_LENGTH];

/* in-use colors */
static uint16_t hover_color;
static uint16_t normal_color;
static uint16_t title_color;
static uint16_t bg_dark_color;
static uint16_t bg_light_color;
static uint16_t border_dark_color;
static uint16_t border_light_color;

static INLINE uint16_t argb32_to_rgba4444(uint32_t col)
{
   unsigned a = ((col >> 24) & 0xff) >> 4;
   unsigned r = ((col >> 16) & 0xff) >> 4;
   unsigned g = ((col >> 8) & 0xff)  >> 4;
   unsigned b =  (col & 0xff)        >> 4;
   return (r << 12) | (g << 8) | (b << 4) | a;
}

static void rgui_update_colors()
{
   hover_color = argb32_to_rgba4444(rgui_hover_color);
   normal_color = argb32_to_rgba4444(rgui_normal_color);
   title_color = argb32_to_rgba4444(rgui_title_color);
   bg_dark_color = argb32_to_rgba4444(rgui_bg_dark_color);
   bg_light_color = argb32_to_rgba4444(rgui_bg_light_color);
   border_dark_color = argb32_to_rgba4444(rgui_border_dark_color);
   border_light_color = argb32_to_rgba4444(rgui_border_light_color);
}

static void rgui_set_default_colors()
{
   rgui_hover_color = rgui_hover_color_default;
   rgui_normal_color = rgui_normal_color_default;
   rgui_title_color = rgui_title_color_default;
   rgui_bg_dark_color = rgui_bg_dark_color_default;
   rgui_bg_light_color = rgui_bg_light_color_default;
   rgui_border_dark_color = rgui_border_dark_color_default;
   rgui_border_light_color = rgui_border_light_color_default;
   
   rgui_update_colors();
}

static void fill_rect(menu_framebuf_t *frame_buf,
      unsigned x, unsigned y,
      unsigned width, unsigned height,
      uint16_t (*col)(unsigned x, unsigned y))
{
   unsigned i, j;
    
   if (!frame_buf->data || !col)
      return;
    
   for (j = y; j < y + height; j++)
      for (i = x; i < x + width; i++)
         frame_buf->data[j * (frame_buf->pitch >> 1) + i] = col(i, j);
}

static uint16_t rgui_bg_filler(unsigned x, unsigned y)
{
   unsigned select = ((x >> 1) + (y >> 1)) & 1;
   return (select == 0) ? bg_dark_color : bg_light_color;
}

static uint16_t rgui_border_filler(unsigned x, unsigned y)
{
   unsigned select = ((x >> 1) + (y >> 1)) & 1;
   return (select == 0) ? border_dark_color : border_light_color;
}

static void rgui_load_theme(settings_t *settings, menu_framebuf_t *frame_buf)
{
   global_t *global           = global_get_ptr();
   config_file_t *conf        = NULL;
   char wallpaper_file[PATH_MAX_LENGTH];
   
   global->menu.wallpaper[0] = '\0';
   wallpaper_file[0] = '\0';
   rgui_wallpaper_valid = false;
   rgui_set_default_colors();
   
   /* Open config file */
   conf = config_file_new(settings->menu.theme);
   if (!conf)
      return;

   /* Parse config file. */
   config_get_hex(conf, "rgui_entry_normal_color", &rgui_normal_color);
   config_get_hex(conf, "rgui_entry_hover_color", &rgui_hover_color);
   config_get_hex(conf, "rgui_title_color", &rgui_title_color);
   config_get_hex(conf, "rgui_bg_dark_color", &rgui_bg_dark_color);
   config_get_hex(conf, "rgui_bg_light_color", &rgui_bg_light_color);
   config_get_hex(conf, "rgui_border_dark_color", &rgui_border_dark_color);
   config_get_hex(conf, "rgui_border_light_color", &rgui_border_light_color);
   config_get_array(conf, "rgui_wallpaper", wallpaper_file, PATH_MAX_LENGTH);

   rgui_update_colors();

   /* Load wallpaper if present */
   if (wallpaper_file[0] != '\0')
   {
      fill_pathname_resolve_relative(global->menu.wallpaper,
                                     settings->menu.theme, wallpaper_file,
                                     PATH_MAX_LENGTH);
      rarch_main_data_msg_queue_push(DATA_TYPE_IMAGE, global->menu.wallpaper,
                                     "cb_menu_wallpaper", 0, 1,true);
   }
   else
      fill_rect(frame_buf, 0, frame_buf->height, frame_buf->width, 4,
                rgui_bg_filler);

   strlcpy(loaded_theme, settings->menu.theme, PATH_MAX_LENGTH);
   
   if (conf)
      config_file_free(conf);
   conf = NULL;
}

static void rgui_adjust_wallpaper_alpha()
{
   settings_t *settings = config_get_ptr();
   uint16_t alpha;
   unsigned i;
   
   alpha = (uint16_t)(settings->menu.wallpaper_opacity * 0xf);
   
   for (i = 0; i < WALLPAPER_WIDTH * WALLPAPER_HEIGHT; i++)
      wallpaper.data[i] = (wallpaper.data[i] & 0xfff0) | alpha;
}

static inline void rgui_check_update(settings_t *settings,
                                     menu_framebuf_t *frame_buf)
{
   global_t* global = global_get_ptr();
   
   if (global->menu.theme_update_flag)
   {
      if (strncmp(loaded_theme, settings->menu.theme, PATH_MAX_LENGTH))
      {
         rgui_load_theme(settings, frame_buf);
         strlcpy(loaded_theme, settings->menu.theme, PATH_MAX_LENGTH);
      }
      else
      {
         if (global->menu.wallpaper[0] == '\0')
            rgui_wallpaper_valid = false;
         
         if (rgui_wallpaper_valid)
            rgui_adjust_wallpaper_alpha();
         
         rgui_update_colors();
         fill_rect(frame_buf, 0, frame_buf->height, frame_buf->width, 4,
                   rgui_bg_filler);
      }
      
      global->menu.theme_update_flag = false;
   }
}

static void rgui_copy_glyph(uint8_t *glyph, const uint8_t *buf)
{
   int x, y;

   if (!glyph)
      return;

   for (y = 0; y < FONT_HEIGHT; y++)
   {
      for (x = 0; x < FONT_WIDTH; x++)
      {
         uint32_t col =
            ((uint32_t)buf[3 * (-y * 256 + x) + 0] << 0) |
            ((uint32_t)buf[3 * (-y * 256 + x) + 1] << 8) |
            ((uint32_t)buf[3 * (-y * 256 + x) + 2] << 16);

         uint8_t rem     = 1 << ((x + y * FONT_WIDTH) & 7);
         unsigned offset = (x + y * FONT_WIDTH) >> 3;

         if (col != 0xff)
            glyph[offset] |= rem;
      }
   }
}

static void color_rect(menu_handle_t *menu,
      unsigned x, unsigned y,
      unsigned width, unsigned height,
      uint16_t color)
{
   unsigned i, j;
   menu_framebuf_t *frame_buf = menu_display_fb_get_ptr();

   if (!frame_buf || !frame_buf->data)
      return;

   for (j = y; j < y + height; j++)
      for (i = x; i < x + width; i++)
         if (i < frame_buf->width && j < frame_buf->height)
            frame_buf->data[j * (frame_buf->pitch >> 1) + i] = color;
}

static void blit_line(menu_handle_t *menu, int x, int y,
      const char *message, uint16_t color)
{
   unsigned i, j;
   menu_framebuf_t *frame_buf = menu_display_fb_get_ptr();
   menu_display_t  *disp      = menu_display_get_ptr();

   while (*message)
   {
      for (j = 0; j < FONT_HEIGHT; j++)
      {
         for (i = 0; i < FONT_WIDTH; i++)
         {
            uint8_t rem = 1 << ((i + j * FONT_WIDTH) & 7);
            int offset  = (i + j * FONT_WIDTH) >> 3;
            bool col    = (disp->font.framebuf[FONT_OFFSET
                  ((unsigned char)*message) + offset] & rem);

            if (!col)
               continue;

            frame_buf->data[(y + j) *
               (frame_buf->pitch >> 1) + (x + i)] = color;
         }
      }

      x += FONT_WIDTH_STRIDE;
      message++;
   }
}

static bool init_font(menu_handle_t *menu, const uint8_t *font_bmp_buf)
{
   unsigned i;
   uint8_t *font = (uint8_t *) calloc(1, FONT_OFFSET(256));

   if (!font)
   {
      RARCH_ERR("Font memory allocation failed.\n");
      return false;
   }

   menu->display.font.alloc_framebuf = true;
   for (i = 0; i < 256; i++)
   {
      unsigned y = i / 16;
      unsigned x = i % 16;
      rgui_copy_glyph(&font[FONT_OFFSET(i)],
            font_bmp_buf + 54 + 3 * (256 * (255 - 16 * y) + 16 * x));
   }

   menu->display.font.framebuf = font;
   return true;
}

static bool rguidisp_init_font(menu_handle_t *menu)
{
   const uint8_t *font_bmp_buf = NULL;
   const uint8_t *font_bin_buf = bitmap_bin;

   if (!menu)
      return false;

   if (font_bmp_buf)
      return init_font(menu, font_bmp_buf);

   if (!font_bin_buf)
      return false;

   menu->display.font.framebuf = font_bin_buf;

   return true;
}

static void rgui_render_background(void)
{
   size_t pitch_in_pixels, size;
   uint16_t             *src  = NULL;
   uint16_t             *dst  = NULL;
   menu_handle_t        *menu = menu_driver_get_ptr();
   menu_framebuf_t *frame_buf = menu_display_fb_get_ptr();
   if (!menu)
      return;

   pitch_in_pixels = frame_buf->pitch >> 1;
   size            = frame_buf->pitch * 4;
   src             = frame_buf->data + pitch_in_pixels * frame_buf->height;
   dst             = frame_buf->data;

   while (dst < src)
   {
      memcpy(dst, src, size);
      dst += pitch_in_pixels * 4;
   }

   fill_rect(frame_buf, 5, 5, frame_buf->width - 10, 5, rgui_border_filler);
   fill_rect(frame_buf, 5, frame_buf->height - 10,
         frame_buf->width - 10, 5,
         rgui_border_filler);

   fill_rect(frame_buf, 5, 5, 5, frame_buf->height - 10, rgui_border_filler);
   fill_rect(frame_buf, frame_buf->width - 10, 5, 5,
         frame_buf->height - 10,
         rgui_border_filler);
}

static void rgui_render_messagebox(const char *message)
{
   size_t i;
   int x, y;
   unsigned width, glyphs_width, height;
   struct string_list *list   = NULL;
   menu_handle_t *menu        = menu_driver_get_ptr();
   menu_framebuf_t *frame_buf = menu_display_fb_get_ptr();
   settings_t *settings       = config_get_ptr();

   if (!menu)
      return;
   if (!message || !*message)
      return;

   (void)settings;

   list = string_split(message, "\n");
   if (!list)
      return;
   if (list->elems == 0)
      goto end;

   width        = 0;
   glyphs_width = 0;

   for (i = 0; i < list->size; i++)
   {
      unsigned line_width;
      char     *msg   = list->elems[i].data;
      unsigned msglen = strlen(msg);

      if (msglen > RGUI_TERM_WIDTH)
      {
         msg[RGUI_TERM_WIDTH - 2] = '.';
         msg[RGUI_TERM_WIDTH - 1] = '.';
         msg[RGUI_TERM_WIDTH - 0] = '.';
         msg[RGUI_TERM_WIDTH + 1] = '\0';
         msglen = RGUI_TERM_WIDTH;
      }

      line_width   = msglen * FONT_WIDTH_STRIDE - 1 + 6 + 10;
      width        = max(width, line_width);
      glyphs_width = max(glyphs_width, msglen);
   }

   height = FONT_HEIGHT_STRIDE * list->size + 6 + 10;
   x      = (frame_buf->width - width) / 2;
   y      = (frame_buf->height - height) / 2;

   fill_rect(frame_buf, x + 5, y + 5, width - 10,
         height - 10, rgui_bg_filler);
   fill_rect(frame_buf, x, y, width - 5, 5, rgui_border_filler);
   fill_rect(frame_buf, x + width - 5, y, 5,
         height - 5, rgui_border_filler);
   fill_rect(frame_buf, x + 5, y + height - 5,
         width - 5, 5, rgui_border_filler);
   fill_rect(frame_buf, x, y + 5, 5,
         height - 5, rgui_border_filler);

   for (i = 0; i < list->size; i++)
   {
      const char *msg = list->elems[i].data;
      int offset_x    = FONT_WIDTH_STRIDE * (glyphs_width - strlen(msg)) / 2;
      int offset_y    = FONT_HEIGHT_STRIDE * i;
      blit_line(menu, x + 8 + offset_x, y + 8 + offset_y, msg,
                normal_color);
   }

end:
   string_list_free(list);
}

static void rgui_blit_cursor(menu_handle_t *menu)
{
   menu_input_t *menu_input = menu_input_get_ptr();

   int16_t x = menu_input->mouse.x;
   int16_t y = menu_input->mouse.y;

   color_rect(menu, x, y - 5, 1, 11, 0xFFFF);
   color_rect(menu, x - 5, y, 11, 1, 0xFFFF);
}

static bool rgui_render_wallpaper(menu_framebuf_t *frame_buf)
{
   if (frame_buf)
   {
      /* Sanity check */
      if ((frame_buf->width != WALLPAPER_WIDTH)
          || (frame_buf->height != WALLPAPER_HEIGHT)
          || (frame_buf->pitch != WALLPAPER_WIDTH << 1) )
      {
         rgui_wallpaper_valid = false;
         return false;
      }

      /* Copy wallpaper to framebuffer */
      memcpy(frame_buf->data, wallpaper.data, WALLPAPER_WIDTH * WALLPAPER_HEIGHT * sizeof(uint16_t));

      return true;
   }

   rgui_wallpaper_valid = false;
   return false;
}

static void rgui_render(void)
{
   unsigned x, y;
   size_t i, end;
   int bottom;
   char title[256];
   char title_buf[256];
   char title_msg[64];
   char timedate[PATH_MAX_LENGTH];
   menu_handle_t *menu            = menu_driver_get_ptr();
   menu_input_t *menu_input       = menu_input_get_ptr();
   menu_display_t *disp           = menu_display_get_ptr();
   menu_framebuf_t *frame_buf     = menu_display_fb_get_ptr();
   menu_navigation_t *nav         = menu_navigation_get_ptr();
   runloop_t *runloop             = rarch_main_get_ptr();
   driver_t *driver               = driver_get_ptr();
   settings_t *settings           = config_get_ptr();
   menu_animation_t *anim         = menu_animation_get_ptr();
   uint64_t frame_count           = video_driver_get_frame_count();

   title[0]     = '\0';
   title_buf[0] = '\0';
   title_msg[0] = '\0';
   timedate[0]  = '\0';

   (void)driver;

   if (!menu)
      return;

   if (menu_entries_needs_refresh() && menu_driver_alive() && !disp->msg_force)
      return;

   if (runloop->is_idle)
      return;

   if (!menu_display_update_pending())
      return;

   /* ensures the framebuffer will be rendered on the screen */
   menu_display_fb_set_dirty();
   anim->is_active           = false;
   anim->label.is_updated    = false;

   if (settings->menu.pointer.enable)
   {
      menu_input->pointer.ptr = menu_input->pointer.y / 11 - 2 + menu_entries_get_start();

      if (menu_input->pointer.dragging)
      {
         menu->scroll_y += menu_input->pointer.dy;
         menu_entries_set_start(-menu->scroll_y / 11 + 2);
         if (menu->scroll_y > 0)
            menu->scroll_y = 0;
      }
   }
   
   if (settings->menu.mouse.enable)
   {
      if (menu_input->mouse.scrolldown 
            && (menu_entries_get_start() < menu_entries_get_end() - RGUI_TERM_HEIGHT))
         menu_entries_set_start(menu_entries_get_start() + 1);

      if (menu_input->mouse.scrollup && (menu_entries_get_start() > 0))
         menu_entries_set_start(menu_entries_get_start() - 1);

      menu_input->mouse.ptr = menu_input->mouse.y / 11 - 2 + menu_entries_get_start();
   }

   /* Do not scroll if all items are visible. */
   if (menu_entries_get_end() <= RGUI_TERM_HEIGHT)
      menu_entries_set_start(0);

   bottom = menu_entries_get_end() - RGUI_TERM_HEIGHT;
   if (menu_entries_get_start() > bottom)
      menu_entries_set_start(bottom);

   end = ((menu_entries_get_start() + RGUI_TERM_HEIGHT) <= (menu_entries_get_end())) ?
      menu_entries_get_start() + RGUI_TERM_HEIGHT : menu_entries_get_end();

   if (!rgui_wallpaper_valid || !rgui_render_wallpaper(frame_buf))
      rgui_render_background();

#if 0
   RARCH_LOG("Dir is: %s\n", label);
#endif

   menu_entries_get_title(title, sizeof(title));

   menu_animation_ticker_line(title_buf, RGUI_TERM_WIDTH - 10,
         frame_count / RGUI_TERM_START_X, title, true);

   if (menu_entries_show_back())
      blit_line(menu,
            RGUI_TERM_START_X, RGUI_TERM_START_X,
            "BACK", title_color);

   blit_line(menu,
         RGUI_TERM_START_X + (RGUI_TERM_WIDTH - strlen(title_buf)) * FONT_WIDTH_STRIDE / 2,
         RGUI_TERM_START_X, title_buf, title_color);

   if (settings->menu.core_enable)
   {
      menu_entries_get_core_title(title_msg, sizeof(title_msg));
      blit_line(menu,
            RGUI_TERM_START_X,
            (RGUI_TERM_HEIGHT * FONT_HEIGHT_STRIDE) +
            RGUI_TERM_START_Y + 2, title_msg, hover_color);
   }

   if (settings->menu.timedate_enable)
   {
      menu_display_timedate(timedate, sizeof(timedate), 3);

      blit_line(menu,
            RGUI_TERM_WIDTH * FONT_WIDTH_STRIDE - RGUI_TERM_START_X,
            (RGUI_TERM_HEIGHT * FONT_HEIGHT_STRIDE) +
            RGUI_TERM_START_Y + 2, timedate, hover_color);
   }

   x = RGUI_TERM_START_X;
   y = RGUI_TERM_START_Y;
   i = menu_entries_get_start();

   for (; i < end; i++, y += FONT_HEIGHT_STRIDE)
   {
      char entry_path[PATH_MAX_LENGTH];
      char entry_value[PATH_MAX_LENGTH];
      char message[PATH_MAX_LENGTH];
      char entry_title_buf[PATH_MAX_LENGTH];
      char type_str_buf[PATH_MAX_LENGTH];
      unsigned entry_spacing                = menu_entry_get_spacing(i);
      bool entry_selected                   = menu_entry_is_currently_selected(i);

      if (i > (nav->selection_ptr + 100))
         continue;

      entry_path[0]      = '\0';
      entry_value[0]     = '\0';
      message[0]         = '\0';
      entry_title_buf[0] = '\0';
      type_str_buf[0]    = '\0';

      menu_entry_get_value(i, entry_value, sizeof(entry_value));
      menu_entry_get_path(i, entry_path, sizeof(entry_path));

      menu_animation_ticker_line(entry_title_buf, RGUI_TERM_WIDTH - (entry_spacing + 1 + 2),
            frame_count / RGUI_TERM_START_X, entry_path, entry_selected);
      menu_animation_ticker_line(type_str_buf, entry_spacing,
            frame_count / RGUI_TERM_START_X,
            entry_value, entry_selected);

      snprintf(message, sizeof(message), "%c %-*.*s %-*s",
            entry_selected ? '>' : ' ',
            RGUI_TERM_WIDTH - (entry_spacing + 1 + 2),
            RGUI_TERM_WIDTH - (entry_spacing + 1 + 2),
            entry_title_buf,
            entry_spacing,
            type_str_buf);

      blit_line(menu, x, y, message,
                entry_selected ? hover_color : normal_color);
   }

#ifdef GEKKO
   const char *message_queue;

   if (disp->msg_force)
   {
      message_queue = rarch_main_msg_queue_pull();
      disp->msg_force = false;
   }
   else
      message_queue = driver->current_msg;

   rgui_render_messagebox( message_queue);
#endif

   if (menu_input->keyboard.display)
   {
      char msg[PATH_MAX_LENGTH] = {0};
      const char           *str = *menu_input->keyboard.buffer;

      if (!str)
         str = "";
      snprintf(msg, sizeof(msg), "%s\n%s", menu_input->keyboard.label, str);
      rgui_render_messagebox(msg);
   }
   else // Temp colors are updated through a message box, so defer until closed
      rgui_check_update(settings, frame_buf);
  
   if (settings->menu.mouse.enable)
      rgui_blit_cursor(menu);
}

static void *rgui_init(void)
{
   bool                   ret = false;
   settings_t       *settings = config_get_ptr();
   global_t           *global = global_get_ptr();
   menu_framebuf_t *frame_buf = NULL;
   menu_handle_t        *menu = (menu_handle_t*)calloc(1, sizeof(*menu));
   
   if (!menu || !settings)
      return NULL;

   frame_buf                  = &menu->display.frame_buf;

   /* 4 extra lines to cache  the checked background */
   frame_buf->data = (uint16_t*)calloc(400 * (240 + 4), sizeof(uint16_t));

   if (!frame_buf->data)
      goto error;

   frame_buf->width                   = 320;
   frame_buf->height                  = 240;
   menu->display.header_height        = FONT_HEIGHT_STRIDE * 2;
   frame_buf->pitch                   = frame_buf->width * sizeof(uint16_t);

   menu_entries_set_start(0);

   ret = rguidisp_init_font(menu);

   if (!ret)
   {
      RARCH_ERR("No font bitmap or binary, abort");
      goto error;
   }
   
   rgui_set_default_colors();
   
   fill_rect(frame_buf, 0, frame_buf->height,
         frame_buf->width, 4, rgui_bg_filler);
   
   if (*settings->menu.theme)
      rgui_load_theme(settings, frame_buf);
   else if (global->menu.wallpaper[0])
     rarch_main_data_msg_queue_push(DATA_TYPE_IMAGE, global->menu.wallpaper,
                                    "cb_menu_wallpaper", 0, 1,true);

   return menu;

error:
   if (menu)
   {
      if (frame_buf->data)
         free(frame_buf->data);
      frame_buf->data = NULL;
      if (menu->userdata)
         free(menu->userdata);
      menu->userdata = NULL;
      free(menu);
   }
   return NULL;
}

static void rgui_free(void *data)
{
   menu_handle_t  *menu = (menu_handle_t*)data;
   menu_display_t *disp = menu ? &menu->display : NULL;

   if (!menu || !disp)
      return;

   if (menu->userdata)
      free(menu->userdata);
   menu->userdata = NULL;

   if (disp->font.alloc_framebuf)
      free((uint8_t*)disp->font.framebuf);
   disp->font.alloc_framebuf = NULL;
}

static void rgui_set_texture(void)
{
   menu_handle_t *menu = menu_driver_get_ptr();
   menu_framebuf_t *frame_buf = menu_display_fb_get_ptr();

   if (!menu)
      return;

   menu_display_fb_unset_dirty();

   video_driver_set_texture_frame(
         frame_buf->data,
         false,
         frame_buf->width,
         frame_buf->height,
         1.0f);
}

static void rgui_navigation_clear(bool pending_push)
{
   menu_handle_t *menu = menu_driver_get_ptr();
   if (!menu)
      return;

   menu_entries_set_start(0);
   menu->scroll_y = 0;
}

static void rgui_navigation_set(bool scroll)
{
   size_t end;
   menu_handle_t *menu            = menu_driver_get_ptr();
   menu_framebuf_t *frame_buf     = menu_display_fb_get_ptr();
   menu_navigation_t *nav         = menu_navigation_get_ptr();
   if (!menu)
      return;

   end = menu_entries_get_end();

   if (!scroll)
      return;

   if (nav->selection_ptr < RGUI_TERM_HEIGHT/2)
      menu_entries_set_start(0);
   else if (nav->selection_ptr >= RGUI_TERM_HEIGHT/2
         && nav->selection_ptr < (end - RGUI_TERM_HEIGHT/2))
      menu_entries_set_start(nav->selection_ptr - RGUI_TERM_HEIGHT/2);
   else if (nav->selection_ptr >= (end - RGUI_TERM_HEIGHT/2))
      menu_entries_set_start(end - RGUI_TERM_HEIGHT);
}

static void rgui_navigation_set_last(void)
{
   menu_handle_t *menu = menu_driver_get_ptr();
   if (menu)
      rgui_navigation_set(true);
}

static void rgui_navigation_descend_alphabet(size_t *unused)
{
   menu_handle_t *menu = menu_driver_get_ptr();
   if (menu)
      rgui_navigation_set(true);
}

static void rgui_navigation_ascend_alphabet(size_t *unused)
{
   menu_handle_t *menu = menu_driver_get_ptr();
   if (menu)
      rgui_navigation_set(true);
}

static void rgui_populate_entries(const char *path,
      const char *label, unsigned k)
{
   menu_handle_t *menu = menu_driver_get_ptr();
   if (menu)
      rgui_navigation_set(true);
}

static void process_wallpaper(struct texture_image *image)
{
   unsigned x, y;
   /* Sanity check */
   if (!image->pixels || (image->width != WALLPAPER_WIDTH) || (image->height != WALLPAPER_HEIGHT))
      return;

   /* Copy image to wallpaper buffer, performing pixel format conversion */
   for (x = 0; x < WALLPAPER_WIDTH; x++)
   {
      for (y = 0; y < WALLPAPER_HEIGHT; y++)
      {
         wallpaper.data[x + (y * WALLPAPER_WIDTH)] =
            argb32_to_rgba4444(image->pixels[x + (y * WALLPAPER_WIDTH)]);
      }
   }
   
   rgui_adjust_wallpaper_alpha();
   rgui_wallpaper_valid = true;
   menu_display_fb_set_dirty();
}

static bool rgui_load_image(void *data, menu_image_type_t type)
{
   if (!data)
      return false;

   switch (type)
   {
      case MENU_IMAGE_WALLPAPER:
         {
            struct texture_image *image = (struct texture_image*)data;
            process_wallpaper(image);
         }
         break;
      default:
         break;
   }

   return true;
}

menu_ctx_driver_t menu_ctx_rgui = {
   rgui_set_texture,
   rgui_render_messagebox,
   rgui_render,
   NULL,
   rgui_init,
   rgui_free,
   NULL,
   NULL,
   rgui_populate_entries,
   NULL,
   rgui_navigation_clear,
   NULL,
   NULL,
   rgui_navigation_set,
   rgui_navigation_set_last,
   rgui_navigation_descend_alphabet,
   rgui_navigation_ascend_alphabet,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   rgui_load_image,
   "rgui",
   NULL,
};