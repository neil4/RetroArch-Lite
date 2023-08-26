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

#define RGUI_TERM_START_X        (frame_buf->width / 20)
#define RGUI_TERM_START_Y        (frame_buf->height / 9)
#define RGUI_TERM_WIDTH          ((frame_buf->width - RGUI_TERM_START_X - RGUI_TERM_START_X) / (FONT_WIDTH_STRIDE))
#define RGUI_TERM_HEIGHT         ((frame_buf->height - RGUI_TERM_START_Y - RGUI_TERM_START_Y) / (FONT_HEIGHT_STRIDE))

#define NUM_PARTICLES 256

/* default colors (clean green) */
#define rgui_hover_32b_default 0xFF64FF64
#define rgui_normal_32b_default 0xFFFFFFFF
#define rgui_title_32b_default 0xFF64FF64
#define rgui_bg_dark_32b_default 0xC0303030
#define rgui_bg_light_32b_default 0xC0303030
#define rgui_border_dark_32b_default 0xC0408040
#define rgui_border_light_32b_default 0xC0408040
#define rgui_particle_32b_default 0xC0879E87

/* A 'particle' is just 4 float variables that can
 * be used for any purpose - e.g.:
 * > a = x pos
 * > b = y pos
 * > c = x velocity
 * or:
 * > a = radius
 * > b = theta
 * etc. */
typedef struct
{
   float a;
   float b;
   float c;
   float d;
} rgui_particle_t;

typedef struct
{
   char *path;
   uint16_t data[RGUI_WIDTH * RGUI_HEIGHT];
} wallpaper_t;

struct enum_lut rgui_particle_effect_lut[NUM_RGUI_PARTICLE_EFFECTS] = {
   { "OFF", RGUI_PARTICLE_EFFECT_NONE },
   { "Snow (Light)", RGUI_PARTICLE_EFFECT_SNOW },
   { "Snow (Heavy)", RGUI_PARTICLE_EFFECT_SNOW_ALT },
   { "Rain", RGUI_PARTICLE_EFFECT_RAIN },
   { "Vortex", RGUI_PARTICLE_EFFECT_VORTEX },
   { "Star Field", RGUI_PARTICLE_EFFECT_STARFIELD },
};

static wallpaper_t rgui_wallpaper = {NULL, {0}};
static uint8_t rgui_wallpaper_orig_alpha[RGUI_WIDTH * RGUI_HEIGHT];
static char rgui_loaded_theme[PATH_MAX_LENGTH];
static bool rgui_wallpaper_valid;

/* theme colors (argb32)*/
static uint32_t rgui_hover_32b;
static uint32_t rgui_normal_32b;
static uint32_t rgui_title_32b;
static uint32_t rgui_bg_dark_32b;
static uint32_t rgui_bg_light_32b;
static uint32_t rgui_border_dark_32b;
static uint32_t rgui_border_light_32b;
static uint32_t rgui_particle_32b;

/* in-use colors (rgba4444) */
static uint16_t rgui_hover_16b;
static uint16_t rgui_normal_16b;
static uint16_t rgui_title_16b;
static uint16_t rgui_bg_dark_16b;
static uint16_t rgui_bg_light_16b;
static uint16_t rgui_border_dark_16b;
static uint16_t rgui_border_light_16b;
static uint16_t rgui_particle_16b;

static uint8_t thick_bg_pattern;
static uint8_t thick_bd_pattern;

static unsigned particle_effect;
static rgui_particle_t particles[NUM_PARTICLES] = {{ 0.0f }};
static float particle_effect_speed;

static INLINE uint16_t argb32_to_rgba4444(uint32_t col)
{
   unsigned a = ((col >> 24) & 0xff) >> 4;
   unsigned r = ((col >> 16) & 0xff) >> 4;
   unsigned g = ((col >> 8) & 0xff)  >> 4;
   unsigned b =  (col & 0xff)        >> 4;
   return (r << 12) | (g << 8) | (b << 4) | a;
}

static void rgui_update_colors(void)
{
   rgui_hover_16b = argb32_to_rgba4444(rgui_hover_32b);
   rgui_normal_16b = argb32_to_rgba4444(rgui_normal_32b);
   rgui_title_16b = argb32_to_rgba4444(rgui_title_32b);
   rgui_bg_dark_16b = argb32_to_rgba4444(rgui_bg_dark_32b);
   rgui_bg_light_16b = argb32_to_rgba4444(rgui_bg_light_32b);
   rgui_border_dark_16b = argb32_to_rgba4444(rgui_border_dark_32b);
   rgui_border_light_16b = argb32_to_rgba4444(rgui_border_light_32b);
   rgui_particle_16b = argb32_to_rgba4444(rgui_particle_32b);
}

static void rgui_set_default_colors(void)
{
   rgui_hover_32b = rgui_hover_32b_default;
   rgui_normal_32b = rgui_normal_32b_default;
   rgui_title_32b = rgui_title_32b_default;
   rgui_bg_dark_32b = rgui_bg_dark_32b_default;
   rgui_bg_light_32b = rgui_bg_light_32b_default;
   rgui_border_dark_32b = rgui_border_dark_32b_default;
   rgui_border_light_32b = rgui_border_light_32b_default;
   rgui_particle_32b = rgui_particle_32b_default;
   
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

static INLINE uint16_t rgui_bg_filler(unsigned x, unsigned y)
{
   unsigned select = ((x >> thick_bg_pattern) + (y >> thick_bg_pattern)) & 1;
   return (select == 0) ? rgui_bg_dark_16b : rgui_bg_light_16b;
}

static INLINE uint16_t rgui_border_filler(unsigned x, unsigned y)
{
   unsigned select = ((x >> thick_bd_pattern) + (y >> thick_bd_pattern)) & 1;
   return (select == 0) ? rgui_border_dark_16b : rgui_border_light_16b;
}

/* Returns true if particle is on screen */
static INLINE bool rgui_draw_particle(
      uint16_t *data,
      unsigned fb_width, unsigned fb_height,
      int x, int y,
      unsigned width, unsigned height,
      uint16_t color)
{
   unsigned x_index, y_index;
   
   /* This great convoluted mess just saves us
    * having to perform comparisons on every
    * iteration of the for loops... */
   int x_start = x > 0 ? x : 0;
   int y_start = y > 0 ? y : 0;
   int x_end = x + width;
   int y_end = y + height;
   
   x_start = x_start <= (int)fb_width  ? x_start : fb_width;
   y_start = y_start <= (int)fb_height ? y_start : fb_height;
   
   x_end = x_end >  0        ? x_end : 0;
   x_end = x_end <= (int)fb_width ? x_end : fb_width;
   
   y_end = y_end >  0         ? y_end : 0;
   y_end = y_end <= (int)fb_height ? y_end : fb_height;
   
   for (y_index = (unsigned)y_start; y_index < (unsigned)y_end; y_index++)
   {
      uint16_t *data_ptr = data + (y_index * fb_width);
      for (x_index = (unsigned)x_start; x_index < (unsigned)x_end; x_index++)
         *(data_ptr + x_index) = color;
   }
   
   return (x_end > x_start) && (y_end > y_start);
}

static void rgui_init_particle_effect(menu_framebuf_t *frame_buf)
{
   size_t i;
   
   /* Sanity check */
   if (!frame_buf)
      return;
   
   switch (particle_effect)
   {
      case RGUI_PARTICLE_EFFECT_SNOW:
      case RGUI_PARTICLE_EFFECT_SNOW_ALT:
         {
            for (i = 0; i < NUM_PARTICLES; i++)
            {
               rgui_particle_t *particle = &particles[i];
               
               particle->a = (float)(rand() % frame_buf->width);
               particle->b = (float)(rand() % frame_buf->height);
               particle->c = (float)(rand() % 64 - 16) * 0.1f;
               particle->d = (float)(rand() % 64 - 48) * 0.1f;
            }
         }
         break;
      case RGUI_PARTICLE_EFFECT_RAIN:
         {
            uint8_t weights[] = { /* 60 entries */
               2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
               3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
               4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
               5, 5, 5, 5, 5, 5, 5, 5,
               6, 6, 6, 6, 6, 6,
               7, 7, 7, 7,
               8, 8, 8,
               9, 9,
               10};
            unsigned num_drops = (unsigned)(0.85f * ((float)frame_buf->width / (float)RGUI_WIDTH) * (float)NUM_PARTICLES);
            
            num_drops = num_drops < NUM_PARTICLES ? num_drops : NUM_PARTICLES;
            
            for (i = 0; i < num_drops; i++)
            {
               rgui_particle_t *particle = &particles[i];
               
               /* x pos */
               particle->a = (float)(rand() % (frame_buf->width / 3)) * 3.0f;
               /* y pos */
               particle->b = (float)(rand() % frame_buf->height);
               /* drop length */
               particle->c = (float)weights[(unsigned)(rand() % 60)];
               /* drop speed (larger drops fall faster) */
               particle->d = (particle->c / 12.0f) * (0.5f + ((float)(rand() % 150) / 200.0f));
            }
         }
         break;
      case RGUI_PARTICLE_EFFECT_VORTEX:
         {
            float max_radius         = (float)sqrt((double)((frame_buf->width * frame_buf->width) + (frame_buf->height * frame_buf->height))) / 2.0f;
            float one_degree_radians = M_PI / 360.0f;
            
            for (i = 0; i < NUM_PARTICLES; i++)
            {
               rgui_particle_t *particle = &particles[i];
               
               /* radius */
               particle->a = 1.0f + (((float)rand() / (float)RAND_MAX) * max_radius);
               /* theta */
               particle->b = ((float)rand() / (float)RAND_MAX) * 2.0f * M_PI;
               /* radial speed */
               particle->c = (float)((rand() % 100) + 1) * 0.001f;
               /* rotational speed */
               particle->d = (((float)((rand() % 50) + 1) / 200.0f) + 0.1f) * one_degree_radians;
            }
         }
         break;
      case RGUI_PARTICLE_EFFECT_STARFIELD:
         {
            for (i = 0; i < NUM_PARTICLES; i++)
            {
               rgui_particle_t *particle = &particles[i];
               
               /* x pos */
               particle->a = (float)(rand() % frame_buf->width);
               /* y pos */
               particle->b = (float)(rand() % frame_buf->height);
               /* depth */
               particle->c = (float)frame_buf->width;
               /* speed */
               particle->d = 1.0f + ((float)(rand() % 20) * 0.01f);
            }
         }
         break;
      default:
         /* Do nothing... */
         break;
   }
}

static void rgui_render_particle_effect(menu_framebuf_t *frame_buf)
{
   unsigned fb_width = frame_buf->width;
   unsigned fb_height = frame_buf->height;
   uint16_t *data = frame_buf->data;
   size_t i;
   
   /* Sanity check */
   if (!frame_buf || !frame_buf->data)
      return;
   
   /* Note: It would be more elegant to have 'update' and 'draw'
    * as separate functions, since 'update' is the part that
    * varies with particle effect whereas 'draw' is always
    * pretty much the same. However, this has the following
    * disadvantages:
    * - It means we have to loop through all particles twice,
    *   and given that we're already using a heap of CPU cycles
    *   to draw these effects any further performance overheads
    *   are to be avoided
    * - It locks us into a particular draw style. e.g. What if
    *   an effect calls for round particles, instead of square
    *   ones? This would make a mess of any 'standardised'
    *   drawing
    * So we go with the simple option of having the entire
    * update/draw sequence here. This results in some code
    * repetition, but it has better performance and allows for
    * complete flexibility */
   
   switch (particle_effect)
   {
      case RGUI_PARTICLE_EFFECT_SNOW:
      case RGUI_PARTICLE_EFFECT_SNOW_ALT:
         {
            unsigned particle_size;
            bool on_screen;
            
            for (i = 0; i < NUM_PARTICLES; i++)
            {
               rgui_particle_t *particle = &particles[i];
               
               /* Update particle 'speed' */
               particle->c = particle->c + (float)(rand() % 16 - 9) * 0.01f;
               particle->d = particle->d + (float)(rand() % 16 - 7) * 0.01f;
               
               particle->c = (particle->c < -0.4f) ? -0.4f : particle->c;
               particle->c = (particle->c >  0.1f) ?  0.1f : particle->c;
               
               particle->d = (particle->d < -0.1f) ? -0.1f : particle->d;
               particle->d = (particle->d >  0.4f) ?  0.4f : particle->d;
               
               /* Update particle location */
               particle->a = fmod(particle->a + particle_effect_speed * particle->c, fb_width);
               particle->b = fmod(particle->b + particle_effect_speed * particle->d, fb_height);
               
               /* Get particle size */
               particle_size = 1;
               if (particle_effect == RGUI_PARTICLE_EFFECT_SNOW_ALT)
               {
                  /* Gives the following distribution:
                   * 1x1: 96
                   * 2x2: 128
                   * 3x3: 32 */
                  if (!(i & 0x2))
                     particle_size = 2;
                  else if ((i & 0x7) == 0x7)
                     particle_size = 3;
               }
               
               /* Draw particle */
               on_screen = rgui_draw_particle(data, fb_width, fb_height,
                                 (int)particle->a, (int)particle->b,
                                 particle_size, particle_size, rgui_particle_16b);
               
               /* Reset particle if it has fallen off screen */
               if (!on_screen)
               {
                  particle->a = (particle->a < 0.0f) ? (particle->a + (float)fb_width)  : particle->a;
                  particle->b = (particle->b < 0.0f) ? (particle->b + (float)fb_height) : particle->b;
               }
            }
         }
         break;
      case RGUI_PARTICLE_EFFECT_RAIN:
         {
            uint8_t weights[] = { /* 60 entries */
               2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
               3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
               4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
               5, 5, 5, 5, 5, 5, 5, 5,
               6, 6, 6, 6, 6, 6,
               7, 7, 7, 7,
               8, 8, 8,
               9, 9,
               10};
            unsigned num_drops = (unsigned)(0.85f * ((float)fb_width / (float)RGUI_WIDTH) * (float)NUM_PARTICLES);
            bool on_screen;
            
            num_drops = num_drops < NUM_PARTICLES ? num_drops : NUM_PARTICLES;
            
            for (i = 0; i < num_drops; i++)
            {
               rgui_particle_t *particle = &particles[i];
               
               /* Draw particle */
               on_screen = rgui_draw_particle(data, fb_width, fb_height,
                                 (int)particle->a, (int)particle->b,
                                 2, (unsigned)particle->c, rgui_particle_16b);
               
               /* Update y pos */
               particle->b += particle->d * particle_effect_speed;
               
               /* Reset particle if it has fallen off the bottom of the screen */
               if (!on_screen)
               {
                  /* x pos */
                  particle->a = (float)(rand() % (fb_width / 3)) * 3.0f;
                  /* y pos */
                  particle->b = 0.0f;
                  /* drop length */
                  particle->c = (float)weights[(unsigned)(rand() % 60)];
                  /* drop speed (larger drops fall faster) */
                  particle->d = (particle->c / 12.0f) * (0.5f + ((float)(rand() % 150) / 200.0f));
               }
            }
         }
         break;
      case RGUI_PARTICLE_EFFECT_VORTEX:
         {
            float max_radius         = (float)sqrt((double)((fb_width * fb_width) + (fb_height * fb_height))) / 2.0f;
            float one_degree_radians = M_PI / 360.0f;
            int x_centre             = (int)(fb_width >> 1);
            int y_centre             = (int)(fb_height >> 1);
            unsigned particle_size;
            float r_speed, theta_speed;
            int x, y;
            
            for (i = 0; i < NUM_PARTICLES; i++)
            {
               rgui_particle_t *particle = &particles[i];
               
               /* Get particle location */
               x = (int)(particle->a * cos(particle->b)) + x_centre;
               y = (int)(particle->a * sin(particle->b)) + y_centre;
               
               /* Get particle size */
               particle_size = 1 + (unsigned)(((1.0f - ((max_radius - particle->a) / max_radius)) * 3.5f) + 0.5f);
               
               /* Draw particle */
               rgui_draw_particle(data, fb_width, fb_height,
                     x, y, particle_size, particle_size, rgui_particle_16b);
               
               /* Update particle speed */
               r_speed     = particle->c * particle_effect_speed;
               theta_speed = particle->d * particle_effect_speed;
               if ((particle->a > 0.0f) && (particle->a < (float)fb_height))
               {
                  float base_scale_factor = ((float)fb_height - particle->a) / (float)fb_height;
                  r_speed     *= 1.0f + (base_scale_factor * 8.0f);
                  theta_speed *= 1.0f + (base_scale_factor * base_scale_factor * 6.0f);
               }
               particle->a -= r_speed;
               particle->b += theta_speed;
               
               /* Reset particle if it has reached the centre of the screen */
               if (particle->a < 0.0f)
               {
                  /* radius
                   * Note: In theory, this should be:
                   * > particle->a = max_radius;
                   * ...but it turns out that spawning new particles at random
                   * locations produces a more visually appealing result... */
                  particle->a = 1.0f + (((float)rand() / (float)RAND_MAX) * max_radius);
                  /* theta */
                  particle->b = ((float)rand() / (float)RAND_MAX) * 2.0f * M_PI;
                  /* radial speed */
                  particle->c = (float)((rand() % 100) + 1) * 0.001f;
                  /* rotational speed */
                  particle->d = (((float)((rand() % 50) + 1) / 200.0f) + 0.1f) * one_degree_radians;
               }
            }
         }
         break;
      case RGUI_PARTICLE_EFFECT_STARFIELD:
         {
            float focal_length = (float)fb_width * 2.0f;
            int x_centre       = (int)(fb_width >> 1);
            int y_centre       = (int)(fb_height >> 1);
            unsigned particle_size;
            int x, y;
            bool on_screen;
            
            /* Based on an example found here:
             * https://codepen.io/nodws/pen/pejBNb */
            for (i = 0; i < NUM_PARTICLES; i++)
            {
               rgui_particle_t *particle = &particles[i];
               
               /* Get particle location */
               x = (int)((particle->a - (float)x_centre) * (focal_length / particle->c));
               x += x_centre;
               
               y = (int)((particle->b - (float)y_centre) * (focal_length / particle->c));
               y += y_centre;
               
               /* Get particle size */
               particle_size = (unsigned)(focal_length / (2.0f * particle->c));
               
               /* Draw particle */
               on_screen = rgui_draw_particle(data, fb_width, fb_height,
                                 x, y, particle_size, particle_size, rgui_particle_16b);
               
               /* Update depth */
               particle->c -= particle->d * particle_effect_speed;
               
               /* Reset particle if it has:
                * - Dropped off the edge of the screen
                * - Reached the screen depth
                * - Grown larger than 16 pixels across
                *   (this is an arbitrary limit, set to reduce overall
                *   performance impact - i.e. larger particles are slower
                *   to draw, and without setting a limit they can fill the screen...) */
               if (!on_screen || (particle->c <= 0.0f) || particle_size > 16)
               {
                  /* x pos */
                  particle->a = (float)(rand() % fb_width);
                  /* y pos */
                  particle->b = (float)(rand() % fb_height);
                  /* depth */
                  particle->c = (float)fb_width;
                  /* speed */
                  particle->d = 1.0f + ((float)(rand() % 20) * 0.01f);
               }
            }
         }
         break;
      default:
         /* Do nothing... */
         break;
   }
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
   config_get_hex(conf, "rgui_entry_normal_color", &rgui_normal_32b);
   config_get_hex(conf, "rgui_entry_hover_color", &rgui_hover_32b);
   config_get_hex(conf, "rgui_title_color", &rgui_title_32b);
   config_get_hex(conf, "rgui_bg_dark_color", &rgui_bg_dark_32b);
   config_get_hex(conf, "rgui_bg_light_color", &rgui_bg_light_32b);
   config_get_hex(conf, "rgui_border_dark_color", &rgui_border_dark_32b);
   config_get_hex(conf, "rgui_border_light_color", &rgui_border_light_32b);
   config_get_hex(conf, "rgui_particle_color", &rgui_particle_32b);
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

   strlcpy(rgui_loaded_theme, settings->menu.theme, PATH_MAX_LENGTH);
   
   if (conf)
      config_file_free(conf);
   conf = NULL;
}

static void rgui_adjust_wallpaper_alpha(void)
{
   settings_t *settings = config_get_ptr();
   global_t *global     = global_get_ptr();
   uint16_t alpha;
   float scale;
   unsigned i;

   scale = global->libretro_dummy ?
      1.0f : settings->menu.wallpaper_opacity;

   for (i = 0; i < RGUI_WIDTH * RGUI_HEIGHT; i++)
   {
      alpha = scale * rgui_wallpaper_orig_alpha[i];
      rgui_wallpaper.data[i] = (rgui_wallpaper.data[i] & 0xfff0) | alpha;
   }
}

static INLINE void rgui_check_update(settings_t *settings,
                                     menu_framebuf_t *frame_buf)
{
   global_t* global = global_get_ptr();
   
   if (global->menu.theme_update_flag)
   {
      thick_bg_pattern = settings->menu.rgui_thick_bg_checkerboard ? 1 : 0;
      thick_bd_pattern = settings->menu.rgui_thick_bd_checkerboard ? 1 : 0;
      
      if (strncmp(rgui_loaded_theme, settings->menu.theme, PATH_MAX_LENGTH))
      {
         rgui_load_theme(settings, frame_buf);
         strlcpy(rgui_loaded_theme, settings->menu.theme, PATH_MAX_LENGTH);
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
      
      if (particle_effect != settings->menu.rgui_particle_effect)
      {
         particle_effect = settings->menu.rgui_particle_effect;
         if (particle_effect != RGUI_PARTICLE_EFFECT_NONE)
            rgui_init_particle_effect(frame_buf);
         else
            menu_entries_set_refresh();
      }

      menu_update_ticker_speed();
      particle_effect_speed = settings->menu.rgui_particle_effect_speed_factor;

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

static void blit_line(const char *message, unsigned message_len,
      int x, int y, int x_offset, uint16_t color)
{
   unsigned i, j;
   int x_start = x;
   int x_end   = x_start + message_len * FONT_WIDTH_STRIDE;
   menu_framebuf_t *frame_buf = menu_display_fb_get_ptr();
   menu_display_t  *disp      = menu_display_get_ptr();

   x += x_offset;
   while (*message)
   {
      if (*message != ' ')
      {
         for (i = 0; i < FONT_WIDTH; i++)
         {
            if (x + i < x_start || x + i > x_end)
               continue;

            for (j = 0; j < FONT_HEIGHT; j++)
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

static void rgui_render_wallpaper(menu_framebuf_t *frame_buf)
{
   if (frame_buf)
   {
      /* Sanity check */
      if ((frame_buf->width != RGUI_WIDTH)
          || (frame_buf->height != RGUI_HEIGHT)
          || (frame_buf->pitch != RGUI_WIDTH << 1) )
      {
         rgui_wallpaper_valid = false;
         return;
      }

      /* Copy wallpaper to framebuffer */
      memcpy(frame_buf->data, rgui_wallpaper.data,
             RGUI_WIDTH * RGUI_HEIGHT * sizeof(uint16_t));

      return;
   }

   rgui_wallpaper_valid = false;
   return;
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
   
   if (rgui_wallpaper_valid)
      rgui_render_wallpaper(frame_buf);
   
   if (!rgui_wallpaper_valid)
   {  /* render pattern if no wallpaper */
      pitch_in_pixels = frame_buf->pitch >> 1;
      size            = frame_buf->pitch * 4;
      src             = frame_buf->data + pitch_in_pixels * frame_buf->height;
      dst             = frame_buf->data;

      while (dst < src)
      {
         memcpy(dst, src, size);
         dst += pitch_in_pixels * 4;
      }
   }
   
   if (particle_effect != RGUI_PARTICLE_EFFECT_NONE)
      rgui_render_particle_effect(frame_buf);

   if (!rgui_wallpaper_valid)
   {
      fill_rect(frame_buf, 5, 5, frame_buf->width - 10, 5, rgui_border_filler);
      fill_rect(frame_buf, 5, frame_buf->height - 10,
            frame_buf->width - 10, 5,
            rgui_border_filler);

      fill_rect(frame_buf, 5, 5, 5, frame_buf->height - 10, rgui_border_filler);
      fill_rect(frame_buf, frame_buf->width - 10, 5, 5,
            frame_buf->height - 10,
            rgui_border_filler);
   }
}

static void rgui_render_messagebox(const char *message)
{
   size_t i, num_lines;
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
   num_lines    = min(list->size, (frame_buf->height - (6 + 10)) / FONT_HEIGHT_STRIDE);

   for (i = 0; i < num_lines; i++)
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

   height = FONT_HEIGHT_STRIDE * num_lines + 6 + 10;
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

   for (i = 0; i < num_lines; i++)
   {
      const char *msg = list->elems[i].data;
      unsigned msglen = min(strlen(msg), RGUI_TERM_WIDTH);
      int offset_x    = FONT_WIDTH_STRIDE * (glyphs_width - msglen) / 2;
      int offset_y    = FONT_HEIGHT_STRIDE * i;
      blit_line(msg, msglen, x + 8 + offset_x, y + 8 + offset_y,
            0, rgui_normal_16b);
   }

end:
   string_list_free(list);
}

static void rgui_blit_cursor(menu_handle_t *menu)
{
   menu_input_t *menu_input = menu_input_get_ptr();

   int16_t x = menu_input->mouse.x;
   int16_t y = menu_input->mouse.y;

   color_rect(menu, x, y - 5, 1, 11, rgui_normal_16b);
   color_rect(menu, x - 5, y, 11, 1, rgui_normal_16b);
}

static void rgui_render(void)
{
   static menu_entry_t entry;
   unsigned title_x, y;
   size_t i, end;
   int bottom;
   int title_w;
   size_t len = 0;
   uint16_t color;
   float offset;
   char title[NAME_MAX_LENGTH];
   char title_buf[NAME_MAX_LENGTH];
   char timedate[32];
   menu_handle_t *menu            = menu_driver_get_ptr();
   menu_input_t *menu_input       = menu_input_get_ptr();
   menu_display_t *disp           = menu_display_get_ptr();
   menu_framebuf_t *frame_buf     = menu_display_fb_get_ptr();
   menu_navigation_t *nav         = menu_navigation_get_ptr();
   runloop_t *runloop             = rarch_main_get_ptr();
   driver_t *driver               = driver_get_ptr();
   settings_t *settings           = config_get_ptr();
   menu_animation_t *anim         = menu_animation_get_ptr();
   uint64_t frame_count           = video_state_get_frame_count();

   title[0]     = '\0';
   title_buf[0] = '\0';
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
      menu_input->pointer.ptr = (menu_input->pointer.y - 4) / 11 - 2 + menu_entries_get_start();

      if (menu_input->pointer.dragging)
      {
         menu->scroll_y += menu_input->pointer.dy;
         menu_entries_set_start(-menu->scroll_y / 11);
         if (menu->scroll_y > 0)
            menu->scroll_y = 0;
      }
   }
   
   if (settings->menu.mouse.enable)
      menu_input->mouse.ptr = (menu_input->mouse.y - 4) / 11 - 2 + menu_entries_get_start();

   /* Do not scroll if all items are visible. */
   if (menu_entries_get_end() <= RGUI_TERM_HEIGHT)
      menu_entries_set_start(0);

   bottom = menu_entries_get_end() - RGUI_TERM_HEIGHT;
   if (menu_entries_get_start() > bottom)
   {
      menu_entries_set_start(bottom);
      menu->scroll_y = -bottom * 11;
   }

   end = ((menu_entries_get_start() + RGUI_TERM_HEIGHT) <= (menu_entries_get_end())) ?
      menu_entries_get_start() + RGUI_TERM_HEIGHT : menu_entries_get_end();

   rgui_render_background();

   menu_entries_get_title(title, sizeof(title));

   title_w = RGUI_TERM_WIDTH - 10;
   offset = menu_animation_ticker_line(title_buf, title_w,
         frame_count, title, true);

   if (menu_entries_show_back())
      blit_line("BACK", 4, RGUI_TERM_START_X,
            RGUI_TERM_START_Y - FONT_HEIGHT_STRIDE,
            0, rgui_title_16b);

   blit_line(title_buf, title_w,
         RGUI_TERM_START_X + (RGUI_TERM_WIDTH - strlen(title_buf)) * FONT_WIDTH_STRIDE / 2,
         RGUI_TERM_START_Y - FONT_HEIGHT_STRIDE,
         FONT_WIDTH_STRIDE * offset, rgui_title_16b);

   if (settings->menu.timedate_mode)
   {
      len = menu_display_timedate(timedate, sizeof(timedate),
                  settings->menu.timedate_mode);
      blit_line(timedate, len,
            RGUI_WIDTH - RGUI_TERM_START_X - len * FONT_WIDTH_STRIDE,
            (RGUI_TERM_HEIGHT * FONT_HEIGHT_STRIDE) + RGUI_TERM_START_Y + 2,
            0, rgui_hover_16b);
   }

   if (settings->menu.core_enable)
   {
      menu_entries_get_core_title(title, sizeof(title));
      title_w = RGUI_TERM_WIDTH - len - 2;
      offset = menu_animation_ticker_line(title_buf,
            title_w, frame_count, title, true);

      blit_line(title_buf, title_w,
            RGUI_TERM_START_X,
            (RGUI_TERM_HEIGHT * FONT_HEIGHT_STRIDE) + RGUI_TERM_START_Y + 2,
            FONT_WIDTH_STRIDE * offset, rgui_hover_16b);
   }

   title_x = RGUI_TERM_START_X + FONT_WIDTH_STRIDE * 2;
   y = RGUI_TERM_START_Y;
   i = menu_entries_get_start();

   for (; i < end; i++, y += FONT_HEIGHT_STRIDE)
   {
      char entry_title_buf[NAME_MAX_LENGTH];
      char type_str_buf[NAME_MAX_LENGTH];
      unsigned entry_spacing;
      bool entry_selected;

      menu_entry_get(&entry, i, NULL, true);
      entry_spacing  = entry.spacing;
      entry_selected = menu_entry_is_currently_selected(i);

      if (i > (nav->selection_ptr + 100))
         continue;

      entry_title_buf[0] = '\0';
      type_str_buf[0]    = '\0';

      /* cursor */
      if (entry_selected)
      {
         color = rgui_hover_16b;
         blit_line(">", 1, RGUI_TERM_START_X, y, 0, color);
      }
      else
         color = rgui_normal_16b;

      /* entry title */
      title_w = RGUI_TERM_WIDTH - (entry_spacing + 1 + 2);
      offset = menu_animation_ticker_line(entry_title_buf,
            title_w, frame_count, entry.path, entry_selected);

      blit_line(entry_title_buf, title_w, title_x, y,
            FONT_WIDTH_STRIDE * offset, color);

      /* entry value */
      offset = menu_animation_ticker_line(type_str_buf,
            entry_spacing, frame_count, entry.value, entry_selected);

      blit_line(type_str_buf, entry_spacing,
            title_x + FONT_WIDTH_STRIDE * (title_w + 1), y,
            FONT_WIDTH_STRIDE * offset, color);
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
      char msg[NAME_MAX_LENGTH];
      const char *str = *menu_input->keyboard.buffer;
      unsigned pos;

      if (!str)
         str = "";

      /* Assume msg is larger than keyboard.label */
      pos = strlcpy(msg, menu_input->keyboard.label, sizeof(msg));
      msg[pos++] = '\n';
      strlcpy(msg + pos, str, sizeof(msg) - pos);

      rgui_render_messagebox(msg);
   }
   else
      rgui_check_update(settings, frame_buf);
  
   if (settings->menu.mouse.enable && menu_input->mouse.show)
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

   /* 4 extra lines to cache the checkered background */
   frame_buf->data = (uint16_t*)calloc(400 * (RGUI_HEIGHT + 4), sizeof(uint16_t));

   if (!frame_buf->data)
      goto error;

   frame_buf->width                   = RGUI_WIDTH;
   frame_buf->height                  = RGUI_HEIGHT;
   menu->display.header_height        = FONT_HEIGHT_STRIDE * 2 + 1;
   frame_buf->pitch                   = frame_buf->width * sizeof(uint16_t);

   menu_entries_set_start(0);

   ret = rguidisp_init_font(menu);

   if (!ret)
   {
      RARCH_ERR("No font bitmap or binary, abort");
      goto error;
   }
   
   rgui_set_default_colors();
   thick_bg_pattern = settings->menu.rgui_thick_bg_checkerboard ? 1 : 0;
   thick_bd_pattern = settings->menu.rgui_thick_bd_checkerboard ? 1 : 0;
   
   particle_effect = settings->menu.rgui_particle_effect;
   particle_effect_speed = settings->menu.rgui_particle_effect_speed_factor;
   if (particle_effect != RGUI_PARTICLE_EFFECT_NONE)
      rgui_init_particle_effect(frame_buf);
   
   fill_rect(frame_buf, 0, frame_buf->height,
         frame_buf->width, 4, rgui_bg_filler);
   
   if (*settings->menu.theme)
      rgui_load_theme(settings, frame_buf);
   else if (global->menu.wallpaper[0])
     rarch_main_data_msg_queue_push(DATA_TYPE_IMAGE, global->menu.wallpaper,
                                    "cb_menu_wallpaper", 0, 1,true);

   menu_update_ticker_speed();

   global->menu.msg_box_width = RGUI_TERM_WIDTH;

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
   global_t* global = global_get_ptr();
   menu_handle_t *menu = menu_driver_get_ptr();
   menu_framebuf_t *frame_buf = menu_display_fb_get_ptr();

   if (!menu)
      return;

   if (!global->menu.force_dirty
       && particle_effect == RGUI_PARTICLE_EFFECT_NONE)
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
   if (!image->pixels || (image->width != RGUI_WIDTH) || (image->height != RGUI_HEIGHT))
      return;

   /* Copy image to wallpaper buffer, performing pixel format conversion */
   for (x = 0; x < RGUI_WIDTH; x++)
   {
      for (y = 0; y < RGUI_HEIGHT; y++)
      {
         rgui_wallpaper.data[x + (y * RGUI_WIDTH)] =
            argb32_to_rgba4444(image->pixels[x + (y * RGUI_WIDTH)]);
      }
   }

   for (x = 0; x < RGUI_WIDTH * RGUI_HEIGHT; x++)
      rgui_wallpaper_orig_alpha[x] = rgui_wallpaper.data[x] & 0xf;

   rgui_adjust_wallpaper_alpha();
   rgui_wallpaper_valid = true;
   menu_display_fb_set_dirty();
   menu_entries_set_refresh();
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