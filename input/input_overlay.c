/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *                2019 - Neil Fore
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
#include <string.h>
#include <math.h>

#include <compat/posix_string.h>
#include <file/file_path.h>
#include <string/string_list.h>
#include <clamping.h>
#include <rhash.h>

#include "input_overlay.h"
#include "input_common.h"
#include "../dynamic.h"

#define BOX_RADIAL       0x18df06d2U
#define BOX_RECT         0x7c9d4d93U

#define KEY_ANALOG_LEFT  0x56b92e81U
#define KEY_ANALOG_RIGHT 0x2e4dc654U

#define MAX_TOUCH 16

/* Extra "high level" bind IDs only for touchscreen overlays */
enum 
{
   RARCH_LIGHTGUN_TRIGGER = 0,
   RARCH_LIGHTGUN_AUX_A,
   RARCH_LIGHTGUN_AUX_B,
   RARCH_LIGHTGUN_AUX_C,
   RARCH_LIGHTGUN_START,
   RARCH_LIGHTGUN_SELECT,
   RARCH_LIGHTGUN_DPAD_UP,
   RARCH_LIGHTGUN_DPAD_DOWN,
   RARCH_LIGHTGUN_DPAD_LEFT,
   RARCH_LIGHTGUN_DPAD_RIGHT,
   RARCH_LIGHTGUN_RELOAD,
   RARCH_JOYPAD_DPAD_AREA,
   RARCH_ANALOG_DPAD_AREA,
   RARCH_JOYPAD_ABXY_AREA,
   RARCH_JOYPAD_ABRL_AREA,
   RARCH_JOYPAD_ABRL2_AREA,
   RARCH_JOYPAD_AB_AREA,
   
   RARCH_HIGHLEVEL_END,
   RARCH_HIGHLEVEL_END_NULL
};

static bool lightgun_active;

typedef struct overlay_adjust_data
{
   float x_aspect_factor;
   float x_center_shift;
   float x_bisect_shift;
   float y_aspect_factor;
   float y_center_shift;
   float display_aspect;
} overlay_adjust_data_t;

static overlay_adjust_data_t adj;

typedef struct ellipse_px
{
   float orientation[MAX_TOUCH];
   float major_px[MAX_TOUCH];
   float minor_px[MAX_TOUCH];
} ellipse_px_t;

static ellipse_px_t ellipse;
static uint8_t overlay_ptr_idx;

struct overlay_aspect_ratio_elem overlay_aspectratio_lut[OVERLAY_ASPECT_RATIO_END] = {
   { "1:2",           0.5f },
   { "9:16",          0.5625f },
   { "10:16",         0.625f },
   { "3:4",           0.75f },
   { "4:3",           1.33333333f },
   { "16:10",         1.6f },
   { "16:9",          1.77777778f },
   { "2:1",           2.0f },
   { "Auto",          1.0 },
};

static struct overlay_eight_way_vals eight_way_vals[NUM_EIGHT_WAY_TYPES];

/* Below: need 3 additional elements for duplicated (obsolete) values */
const struct input_bind_map input_highlevel_config_bind_map[RARCH_HIGHLEVEL_END_NULL+3] = {
      DECLARE_BIND(lightgun_trigger, RARCH_LIGHTGUN_TRIGGER, "Lightgun trigger"),
      DECLARE_BIND(lightgun_cursor,  RARCH_LIGHTGUN_AUX_A, "Lightgun cursor"),
      DECLARE_BIND(lightgun_aux_a,   RARCH_LIGHTGUN_AUX_A, "Lightgun aux A"),
      DECLARE_BIND(lightgun_turbo,   RARCH_LIGHTGUN_AUX_B, "Lightgun turbo"),
      DECLARE_BIND(lightgun_aux_b,   RARCH_LIGHTGUN_AUX_B, "Lightgun aux B"),
      DECLARE_BIND(lightgun_aux_c,   RARCH_LIGHTGUN_AUX_C, "Lightgun aux C"),
      DECLARE_BIND(lightgun_pause,   RARCH_LIGHTGUN_START, "Lightgun pause"),
      DECLARE_BIND(lightgun_start,   RARCH_LIGHTGUN_START, "Lightgun start"),
      DECLARE_BIND(lightgun_select,  RARCH_LIGHTGUN_SELECT, "Lightgun select"),
      DECLARE_BIND(lightgun_reload,  RARCH_LIGHTGUN_RELOAD, "Lightgun reload"),
      DECLARE_BIND(lightgun_up,      RARCH_LIGHTGUN_DPAD_UP, "Lightgun D-pad up"),
      DECLARE_BIND(lightgun_down,    RARCH_LIGHTGUN_DPAD_DOWN, "Lightgun D-pad down"),
      DECLARE_BIND(lightgun_left,    RARCH_LIGHTGUN_DPAD_LEFT, "Lightgun D-pad left"),
      DECLARE_BIND(lightgun_right,   RARCH_LIGHTGUN_DPAD_RIGHT, "Lightgun D-pad right"),
      DECLARE_BIND(dpad_area,        RARCH_JOYPAD_DPAD_AREA, "D-pad area"),
      DECLARE_BIND(analog_dpad_area, RARCH_ANALOG_DPAD_AREA, "Analog D-pad area"),
      DECLARE_BIND(abxy_area,        RARCH_JOYPAD_ABXY_AREA, "ABXY area"),
      DECLARE_BIND(abrl_area,        RARCH_JOYPAD_ABRL_AREA, "ABRL area"),
      DECLARE_BIND(abrl2_area,       RARCH_JOYPAD_ABRL2_AREA, "ABRL2 area"),
      DECLARE_BIND(ab_area,          RARCH_JOYPAD_AB_AREA, "AB area")
};


/**
 * set_ellipse
 * @param orientation radians clockwise from north, [-Pi/2, Pi/2]
 * @param major_px major axis in pixels
 * @param minor_px minor axis in pixels
 * 
 * Stores values for eight_way_state
 */
void set_ellipse(uint8_t idx,
                 const float orientation,
                 const float major_px,
                 const float minor_px )
{
   if (idx >= MAX_TOUCH)
      return;
   
   ellipse.orientation[idx] = orientation;
   ellipse.major_px[idx] = major_px;
   ellipse.minor_px[idx] = minor_px;
}

void reset_ellipse(uint8_t idx)
{
   ellipse.major_px[idx] = 0;
}

void set_overlay_pointer_index(uint8_t idx)
{
   overlay_ptr_idx = idx;
}


/**
 * input_overlay_scale:
 * @ol                    : Overlay handle.
 * @scale                 : Scaling factor.
 *
 * Scales overlay and all its associated descriptors
 * by a given scaling factor (@scale).
 **/
static void input_overlay_scale(struct overlay *ol, float scale)
{
   size_t i;

   if (!ol)
      return;

   if (ol->block_scale)
      scale = 1.0f;

   ol->scale = scale;
   ol->mod_w = ol->w * scale;
   ol->mod_h = ol->h * scale;
   ol->mod_x = ol->center_x +
      (ol->x - ol->center_x) * scale;
   ol->mod_y = ol->center_y +
      (ol->y - ol->center_y) * scale;

   for (i = 0; i < ol->size; i++)
   {
      float scale_w, scale_h, adj_center_x, adj_center_y;
      struct overlay_desc *desc = &ol->descs[i];

      if (!desc)
         continue;

      scale_w = ol->mod_w * desc->range_x;
      scale_h = ol->mod_h * desc->range_y;

      desc->mod_w = 2.0f * scale_w;
      desc->mod_h = 2.0f * scale_h;

      adj_center_x = ol->mod_x + desc->x * ol->mod_w;
      adj_center_y = ol->mod_y + desc->y * ol->mod_h;
      desc->mod_x = adj_center_x - scale_w;
      desc->mod_y = adj_center_y - scale_h;
   }
}

static unsigned input_overlay_auto_aspect_index(struct overlay *ol)
{
   size_t i, j;
   float image_aspect, ol_aspect;
   float avg_delta[OVERLAY_ASPECT_RATIO_END];
   float best_delta = 1e9;
   unsigned best_index = 0;
   
   if (!ol)
      return 0;

   for (i = 0; i < OVERLAY_ASPECT_RATIO_AUTO; i++)
   {
      avg_delta[i] = 0.0f;
      for (j = 0; j < ol->size; j++)
      {
         struct overlay_desc *desc = &ol->descs[j];
         if (!desc->image.width || !desc->image.height)
            continue;
         image_aspect = ((float)desc->image.width) / desc->image.height;
         ol_aspect = desc->range_x_orig / desc->range_y_orig;
         avg_delta[i] += overlay_aspectratio_lut[i].value * ol_aspect
                         - image_aspect;
      }
      avg_delta[i] /= ol->size;
   }
   
   for (i = 0; i < OVERLAY_ASPECT_RATIO_AUTO; i++)
   {
      if (fabs(avg_delta[i]) < best_delta)
      {
         best_delta = fabs(avg_delta[i]);
         best_index = i;
      }
      else break; /* overlay aspects are sorted */
   }
   return best_index;
}

/* Get values to adjust the overlay's aspect, re-center it, and then bisect it
 * to a wider display if possible
 */
static void update_aspect_x_y_globals(struct overlay *ol)
{
   unsigned screen_width, screen_height;
   float overlay_aspect, bisect_aspect;
   settings_t* settings = config_get_ptr();
   
   /* initialize to defaults */
   adj.x_aspect_factor      = 1.0f;
   adj.x_center_shift       = 0.0f;
   adj.x_bisect_shift       = 0.0f;
   adj.y_aspect_factor      = 1.0f;
   adj.y_center_shift       = 0.0f;
   
   if (settings->input.overlay_adjust_aspect == false)
      return;
   
   video_driver_get_size(&screen_width, &screen_height);
   adj.display_aspect = (float)screen_width / screen_height;

   if (settings->input.overlay_aspect_ratio_index == OVERLAY_ASPECT_RATIO_AUTO)
      overlay_aspect = overlay_aspectratio_lut
                       [input_overlay_auto_aspect_index(ol)].value;
   else
      overlay_aspect = overlay_aspectratio_lut
                       [settings->input.overlay_aspect_ratio_index].value;
   
   bisect_aspect = settings->input.overlay_bisect_aspect_ratio;
   if (bisect_aspect > adj.display_aspect)
      bisect_aspect = adj.display_aspect;

   if (adj.display_aspect > overlay_aspect * 1.01)
   {  /* get values to adjust overlay aspect, re-center x, and then bisect */
      adj.x_aspect_factor = overlay_aspect / adj.display_aspect;
      adj.x_center_shift = (1.0f - adj.x_aspect_factor) / 2.0f;
      if (bisect_aspect > overlay_aspect * 1.01)
      {
         adj.x_bisect_shift = (bisect_aspect/adj.display_aspect
                               + ( (1.0f - bisect_aspect/adj.display_aspect)
                                    / 2.0f ))
                              - (adj.x_aspect_factor + adj.x_center_shift);
      }
   }
   else if (overlay_aspect > adj.display_aspect * 1.01)
   {  /* overlay too wide; adjust its aspect and re-center y */
      adj.y_aspect_factor = adj.display_aspect / overlay_aspect;
      adj.y_center_shift = (1.0f - adj.y_aspect_factor) / 2.0f;
   }
}

static void input_overlay_desc_update_hitbox(struct overlay_desc *desc)
{
   if (!desc)
      return;
   
   desc->mod_x   = desc->x - desc->range_x;
   desc->mod_w   = 2.0f * desc->range_x;
   desc->mod_y   = desc->y - desc->range_y;
   desc->mod_h   = 2.0f * desc->range_y;

   desc->x_hitbox = ( (desc->x + desc->range_x * desc->reach_right)
                      + (desc->x - desc->range_x * desc->reach_left) ) / 2.0f;

   desc->y_hitbox = ( (desc->y + desc->range_y * desc->reach_down)
                      + (desc->y - desc->range_y * desc->reach_up) ) / 2.0f;

   desc->range_x_hitbox = ( desc->range_x * desc->reach_right
                            + desc->range_x * desc->reach_left ) / 2.0f;

   desc->range_y_hitbox = ( desc->range_y * desc->reach_down
                            + desc->range_y * desc->reach_up ) / 2.0f;
   
   desc->range_x_mod = desc->range_x_hitbox;
   desc->range_y_mod = desc->range_y_hitbox;
}

static void input_overlay_desc_adjust_aspect_and_vertical(struct overlay_desc *desc)
{
   settings_t* settings = config_get_ptr();
   
   if (!desc)
      return;
   
   /* adjust aspect */
   desc->x = desc->x_orig * adj.x_aspect_factor;
   desc->y = desc->y_orig * adj.y_aspect_factor;
   desc->range_x = desc->range_x_orig * adj.x_aspect_factor;
   desc->range_y = desc->range_y_orig * adj.y_aspect_factor;
   
   /* re-center and bisect */
   desc->x += adj.x_center_shift;
   if ( desc->x > 0.5f )
      desc->x += adj.x_bisect_shift;
   else if ( desc->x < 0.5f )
      desc->x -= adj.x_bisect_shift;
   desc->y += adj.y_center_shift;
   
   /* adjust vertical */
   desc->y -= settings->input.overlay_adjust_vertical;
   
   /* make sure the button isn't pushed off screen */
   if ( desc->y + desc->range_y > 1.0f )
      desc->y = 1.0f - desc->range_y;
   else if ( desc->y - desc->range_y < 0.0f)
      desc->y = desc->range_y;
   
   /* optionally clamp to edge */
   if (settings->input.overlay_adjust_vertical_lock_edges)
   {
      if (desc->y_orig + desc->range_y_orig > 0.99f)
      {
         float dist = adj.y_aspect_factor * (1.0f - desc->y_orig);
         desc->y = 1.0f - dist;
      }
      else if (desc->y_orig - desc->range_y_orig < 0.01f)
      {
         float dist = adj.y_aspect_factor * desc->y_orig;
         desc->y = dist;
      }
   }
}

void input_overlay_get_slope_limits( const float diagonal_sensitivity,
                                     float* high_slope, float* low_slope )
{
   /* Update the slope values to be used in eight_way_state. */
   
   /* Sensitivity is the ratio (as a percentage) of diagonals to normals.
    * 100% means 8-way symmetry; zero means no diagonals.
    * Convert to fraction of max diagonal angle, 45deg
    */
   float fraction = 2.0f * diagonal_sensitivity
                         / ( 100.0f + diagonal_sensitivity );
   
   float high_angle  /* 67.5 deg max */
   = ( fraction * (0.375*M_PI)
       + (1.0f - fraction) * (0.25*M_PI) );
   float low_angle   /* 22.5 deg min */
   = ( fraction * (0.125*M_PI)
       + (1.0f - fraction) * (0.25*M_PI) );
   
   *high_slope = tan(high_angle);
   *low_slope  = tan(low_angle);
}

/**
 * input_overlay_populate_8way_vals:
 *
 * Populate all values to be used in eight_way_state
 **/
void input_overlay_populate_8way_vals()
{
   static bool const_vals_populated = false;
   settings_t* settings             = config_get_ptr();
   
   if ( !const_vals_populated )
   {
      eight_way_vals[DPAD_AREA].up = UINT64_C(1)<<RETRO_DEVICE_ID_JOYPAD_UP;
      eight_way_vals[DPAD_AREA].down = UINT64_C(1)<<RETRO_DEVICE_ID_JOYPAD_DOWN;
      eight_way_vals[DPAD_AREA].left = UINT64_C(1)<<RETRO_DEVICE_ID_JOYPAD_LEFT;
      eight_way_vals[DPAD_AREA].right = UINT64_C(1)<<RETRO_DEVICE_ID_JOYPAD_RIGHT;

      eight_way_vals[ANALOG_DPAD_AREA].up = UINT64_C(1)<<RARCH_ANALOG_LEFT_Y_MINUS;
      eight_way_vals[ANALOG_DPAD_AREA].down = UINT64_C(1)<<RARCH_ANALOG_LEFT_Y_PLUS;
      eight_way_vals[ANALOG_DPAD_AREA].left = UINT64_C(1)<<RARCH_ANALOG_LEFT_X_MINUS;
      eight_way_vals[ANALOG_DPAD_AREA].right = UINT64_C(1)<<RARCH_ANALOG_LEFT_X_PLUS;

      eight_way_vals[ABXY_AREA].up = UINT64_C(1)<<RETRO_DEVICE_ID_JOYPAD_X;
      eight_way_vals[ABXY_AREA].down = UINT64_C(1)<<RETRO_DEVICE_ID_JOYPAD_B;
      eight_way_vals[ABXY_AREA].left = UINT64_C(1)<<RETRO_DEVICE_ID_JOYPAD_Y;
      eight_way_vals[ABXY_AREA].right = UINT64_C(1)<<RETRO_DEVICE_ID_JOYPAD_A;

      eight_way_vals[ABRL_AREA].up = UINT64_C(1)<<RETRO_DEVICE_ID_JOYPAD_R;
      eight_way_vals[ABRL_AREA].down = UINT64_C(1)<<RETRO_DEVICE_ID_JOYPAD_B;
      eight_way_vals[ABRL_AREA].left = UINT64_C(1)<<RETRO_DEVICE_ID_JOYPAD_L;
      eight_way_vals[ABRL_AREA].right = UINT64_C(1)<<RETRO_DEVICE_ID_JOYPAD_A;
      
      eight_way_vals[ABRL2_AREA].up = UINT64_C(1)<<RETRO_DEVICE_ID_JOYPAD_L;
      eight_way_vals[ABRL2_AREA].down = UINT64_C(1)<<RETRO_DEVICE_ID_JOYPAD_A;
      eight_way_vals[ABRL2_AREA].left = UINT64_C(1)<<RETRO_DEVICE_ID_JOYPAD_B;
      eight_way_vals[ABRL2_AREA].right = UINT64_C(1)<<RETRO_DEVICE_ID_JOYPAD_R;

      eight_way_vals[AB_AREA].up = UINT64_C(1)<<RETRO_DEVICE_ID_JOYPAD_B;
      eight_way_vals[AB_AREA].down = UINT64_C(1)<<RETRO_DEVICE_ID_JOYPAD_A;
      eight_way_vals[AB_AREA].left = UINT64_C(1)<<RETRO_DEVICE_ID_JOYPAD_B;
      eight_way_vals[AB_AREA].right = UINT64_C(1)<<RETRO_DEVICE_ID_JOYPAD_A;
      
      int i;
      for ( i = 0; i < NUM_EIGHT_WAY_TYPES; i++ )
      {
         eight_way_vals[i].up_left = eight_way_vals[i].up
                                     | eight_way_vals[i].left;
         eight_way_vals[i].up_right = eight_way_vals[i].up 
                                      | eight_way_vals[i].right;
         eight_way_vals[i].down_left = eight_way_vals[i].down
                                       | eight_way_vals[i].left;
         eight_way_vals[i].down_right = eight_way_vals[i].down
                                        | eight_way_vals[i].right;
      }
      
      const_vals_populated = true;
   }
   
   float dpad_slope_high, dpad_slope_low;
   float buttons_slope_high, buttons_slope_low;
   
   input_overlay_get_slope_limits(settings->input.dpad_diagonal_sensitivity,
                                  &dpad_slope_high, &dpad_slope_low);
   input_overlay_get_slope_limits(settings->input.abxy_diagonal_sensitivity,
                                  &buttons_slope_high, &buttons_slope_low);
   
   eight_way_vals[DPAD_AREA].slope_high = dpad_slope_high;
   eight_way_vals[DPAD_AREA].slope_low = dpad_slope_low;
   eight_way_vals[ANALOG_DPAD_AREA].slope_high = dpad_slope_high;
   eight_way_vals[ANALOG_DPAD_AREA].slope_low = dpad_slope_low;
   eight_way_vals[ABXY_AREA].slope_high = buttons_slope_high;
   eight_way_vals[ABXY_AREA].slope_low = buttons_slope_low;
   eight_way_vals[ABRL_AREA].slope_high = buttons_slope_high;
   eight_way_vals[ABRL_AREA].slope_low = buttons_slope_low;
   eight_way_vals[ABRL2_AREA].slope_high = buttons_slope_high;
   eight_way_vals[ABRL2_AREA].slope_low = buttons_slope_low;
   eight_way_vals[AB_AREA].slope_high = buttons_slope_high;
   eight_way_vals[AB_AREA].slope_low = buttons_slope_low;
}

/**
 * input_translate_str_to_highlevel_bind_id:
 * @str                            : String to translate to high level bind ID.
 *
 * Translate string representation to "high level" bind ID
 * High level IDs eventually translate to low level IDs
 *
 * Returns: Bind ID value on success, otherwise RARCH_BIND_LIST_END on not found.
 **/
static unsigned input_translate_str_to_highlevel_bind_id(const char *str)
{
   unsigned i;
   for (i = 0; input_highlevel_config_bind_map[i].valid; i++)
      if (!strcmp(str, input_highlevel_config_bind_map[i].base))
         return input_highlevel_config_bind_map[i].retro_key;
   return RARCH_HIGHLEVEL_END;
}

static void input_overlay_set_vertex_geom(input_overlay_t *ol)
{
   size_t i;

   if (!ol)
      return;
   if (ol->active->image.pixels)
      ol->iface->vertex_geom(ol->iface_data, 0,
            ol->active->mod_x, ol->active->mod_y,
            ol->active->mod_w, ol->active->mod_h);

   for (i = 0; i < ol->active->size; i++)
   {
      struct overlay_desc *desc = &ol->active->descs[i];

      if (!desc)
         continue;

      if (!desc->image.pixels)
         continue;

      ol->iface->vertex_geom(ol->iface_data, desc->image_index,
            desc->mod_x, desc->mod_y, desc->mod_w, desc->mod_h);
   }
}

/**
 * input_overlay_set_scale_factor:
 * @ol                    : Overlay handle.
 * @scale                 : Factor of scale to apply.
 *
 * Scales the overlay by a factor of scale.
 **/
void input_overlay_set_scale_factor(input_overlay_t *ol, float scale)
{
   size_t i;

   if (!ol)
      return;

   for (i = 0; i < ol->size; i++)
      input_overlay_scale(&ol->overlays[i], scale);

   input_overlay_set_vertex_geom(ol);
}

static void input_overlay_update_aspect_and_vertical_vals(input_overlay_t *ol,
                                                          unsigned i)
{
   size_t j;
   struct overlay_desc* desc;

   if (!ol || !ol->overlays)
      return;

   update_aspect_x_y_globals(&ol->overlays[i]);

   for (j = 0; j < ol->overlays[i].size; j++)
   {
      desc = &ol->overlays[i].descs[j];
      if (!ol->overlays[i].fullscreen_image)
         input_overlay_desc_adjust_aspect_and_vertical(desc);
      input_overlay_desc_update_hitbox(desc);
   }
}

void input_overlays_update_aspect_and_vertical(input_overlay_t *ol)
{
   size_t i;

   if (!ol || !ol->overlays)
      return;

   for (i = 0; i < ol->size; i++)
      input_overlay_update_aspect_and_vertical_vals(ol, i);

   input_overlay_set_vertex_geom(ol);
}

static void input_overlay_free_overlay(struct overlay *overlay)
{
   size_t i;

   if (!overlay)
      return;

   for (i = 0; i < overlay->size; i++)
      texture_image_free(&overlay->descs[i].image);

   free(overlay->load_images);
   free(overlay->descs);
   texture_image_free(&overlay->image);
}

static void input_overlay_free_overlays(input_overlay_t *ol)
{
   size_t i;

   if (!ol)
      return;

   for (i = 0; i < ol->size; i++)
      input_overlay_free_overlay(&ol->overlays[i]);

   free(ol->overlays);
}

static bool input_overlay_load_texture_image(struct overlay *overlay,
      struct texture_image *image, const char *path)
{
   struct texture_image img = {0};

   if (!texture_image_load(&img, path))
      return false;

   *image = img;
   overlay->load_images[overlay->load_images_size++] = *image;
   
   return true;
}

static bool input_overlay_load_desc_image(input_overlay_t *ol,
      struct overlay_desc *desc,
      struct overlay *input_overlay,
      unsigned ol_idx, unsigned desc_idx)
{
   char overlay_desc_image_key[64]  = {0};
   char image_path[PATH_MAX_LENGTH] = {0};
 
   snprintf(overlay_desc_image_key, sizeof(overlay_desc_image_key),
         "overlay%u_desc%u_overlay", ol_idx, desc_idx);

   if (config_get_path(ol->conf, overlay_desc_image_key,
            image_path, sizeof(image_path)))
   {
      char path[PATH_MAX_LENGTH] = {0};
      fill_pathname_resolve_relative(path, ol->overlay_path,
            image_path, sizeof(path));

      if (input_overlay_load_texture_image(input_overlay, &desc->image, path))
         desc->image_index = input_overlay->load_images_size - 1;
   }

   input_overlay->pos ++;

   return true;
}

static bool input_overlay_load_desc(input_overlay_t *ol,
      struct overlay_desc *desc,
      struct overlay *input_overlay,
      unsigned ol_idx, unsigned desc_idx,
      unsigned width, unsigned height,
      bool normalized, float alpha_mod, float range_mod)
{
   float width_mod, height_mod;
   uint32_t box_hash, key_hash;
   bool ret                             = true;
   bool by_pixel                        = false;
   char overlay_desc_key[64]            = {0};
   char conf_key[64]                    = {0};
   char overlay_desc_normalized_key[64] = {0};
   char overlay[256]                    = {0};
   char *save                           = NULL;
   char *key                            = NULL;
   struct string_list *list             = NULL;
   const char *x                        = NULL;
   const char *y                        = NULL;
   const char *box                      = NULL;

   snprintf(overlay_desc_key, sizeof(overlay_desc_key),
         "overlay%u_desc%u", ol_idx, desc_idx);

   snprintf(overlay_desc_normalized_key, sizeof(overlay_desc_normalized_key),
         "overlay%u_desc%u_normalized", ol_idx, desc_idx);
   config_get_bool(ol->conf, overlay_desc_normalized_key, &normalized);
   
   snprintf(conf_key, sizeof(conf_key), "overlay%u_overlay", ol->pos);
   input_overlay->fullscreen_image = config_get_string(ol->conf, conf_key, &key );
   if (key) free(key);

   by_pixel = !normalized;

   if (by_pixel && (width == 0 || height == 0))
   {
      RARCH_ERR("[Overlay]: Base overlay is not set and not using normalized coordinates.\n");
      return false;
   }

   if (!config_get_array(ol->conf, overlay_desc_key, overlay, sizeof(overlay)))
   {
      RARCH_ERR("[Overlay]: Didn't find key: %s.\n", overlay_desc_key);
      return false;
   }

   list = string_split(overlay, ", ");

   if (!list)
   {
      RARCH_ERR("[Overlay]: Failed to split overlay desc.\n");
      return false;
   }

   if (list->size < 6)
   {
      string_list_free(list);
      RARCH_ERR("[Overlay]: Overlay desc is invalid. Requires at least 6 tokens.\n");
      return false;
   }

   key = list->elems[0].data;
   x   = list->elems[1].data;
   y   = list->elems[2].data;
   box = list->elems[3].data;

   box_hash = djb2_calculate(box);
   key_hash = djb2_calculate(key);

   desc->key_mask = 0;

   switch (key_hash)
   {
      case KEY_ANALOG_LEFT:
         desc->type = OVERLAY_TYPE_ANALOG_LEFT;
         break;
      case KEY_ANALOG_RIGHT:
         desc->type = OVERLAY_TYPE_ANALOG_RIGHT;
         break;
      default:
         if (strstr(key, "retrok_") == key)
         {
            desc->type = OVERLAY_TYPE_KEYBOARD;
            desc->key_mask = input_translate_str_to_rk(key + 7);
         }
         else
         {
            const char *tmp = NULL;

            desc->type = OVERLAY_TYPE_BUTTONS;
            for (tmp = strtok_r(key, "|", &save); tmp; tmp = strtok_r(NULL, "|", &save))
            {
               if (strcmp(tmp, "nul") != 0)
               {
                  unsigned hlid = input_translate_str_to_highlevel_bind_id(tmp);
                  if ( hlid != RARCH_HIGHLEVEL_END )
                     desc->highlevel_mask |= UINT64_C(1) << hlid;
                  else
                     desc->key_mask |= UINT64_C(1) << input_translate_str_to_bind_id(tmp);
               }
            }

            if (desc->key_mask & (UINT64_C(1) << RARCH_OVERLAY_NEXT))
            {
               char overlay_target_key[64] = {0};

               snprintf(overlay_target_key, sizeof(overlay_target_key),
                     "overlay%u_desc%u_next_target", ol_idx, desc_idx);
               config_get_array(ol->conf, overlay_target_key,
                     desc->next_index_name, sizeof(desc->next_index_name));
            }
         }
         break;
   }

   width_mod  = 1.0f;
   height_mod = 1.0f;
   
   if (by_pixel)
   {
      width_mod  /= width;
      height_mod /= height;
   }
   
   desc->x_orig = (float)strtod(x, NULL) * width_mod;
   desc->y_orig = (float)strtod(y, NULL) * height_mod;
   desc->x = desc->x_orig;
   desc->y = desc->y_orig;

   switch (box_hash)
   {
      case BOX_RADIAL:
         desc->hitbox = OVERLAY_HITBOX_RADIAL;
         break;
      case BOX_RECT:
         desc->hitbox = OVERLAY_HITBOX_RECT;
         break;
      default:
         RARCH_ERR("[Overlay]: Hitbox type (%s) is invalid. Use \"radial\" or \"rect\".\n", box);
         ret = false;
         goto end;
   }

   switch (desc->type)
   {
      case OVERLAY_TYPE_ANALOG_LEFT:
      case OVERLAY_TYPE_ANALOG_RIGHT:
         {
            char overlay_analog_saturate_key[64] = {0};

            if (desc->hitbox != OVERLAY_HITBOX_RADIAL)
            {
               RARCH_ERR("[Overlay]: Analog hitbox type must be \"radial\".\n");
               ret = false;
               goto end;
            }

            snprintf(overlay_analog_saturate_key, sizeof(overlay_analog_saturate_key),
                  "overlay%u_desc%u_saturate_pct", ol_idx, desc_idx);
            if (!config_get_float(ol->conf, overlay_analog_saturate_key,
                     &desc->analog_saturate_pct))
               desc->analog_saturate_pct = 1.0f;
         }
         break;
      default:
         /* OVERLAY_TYPE_BUTTONS  - unhandled */
         /* OVERLAY_TYPE_KEYBOARD - unhandled */
         break;
   }
   
   desc->range_x_orig = (float)strtod(list->elems[4].data, NULL) * width_mod;
   desc->range_y_orig = (float)strtod(list->elems[5].data, NULL) * height_mod;
   desc->range_x = desc->range_x_orig;
   desc->range_y = desc->range_y_orig;

   snprintf(conf_key, sizeof(conf_key),
         "overlay%u_desc%u_alpha_mod", ol_idx, desc_idx);
   desc->alpha_mod = alpha_mod;
   config_get_float(ol->conf, conf_key, &desc->alpha_mod);

   snprintf(conf_key, sizeof(conf_key),
         "overlay%u_desc%u_range_mod", ol_idx, desc_idx);
   desc->range_mod = range_mod;
   config_get_float(ol->conf, conf_key, &desc->range_mod);
   
   snprintf(conf_key, sizeof(conf_key), "overlay%u_desc%u_reach_right", ol_idx, desc_idx);
   desc->reach_right = 1.0f;
   config_get_float(ol->conf, conf_key, &desc->reach_right);
   
   snprintf(conf_key, sizeof(conf_key), "overlay%u_desc%u_reach_left", ol_idx, desc_idx);
   desc->reach_left = 1.0f;
   config_get_float(ol->conf, conf_key, &desc->reach_left);
   
   snprintf(conf_key, sizeof(conf_key), "overlay%u_desc%u_reach_up", ol_idx, desc_idx);
   desc->reach_up = 1.0f;
   config_get_float(ol->conf, conf_key, &desc->reach_up);
   
   snprintf(conf_key, sizeof(conf_key), "overlay%u_desc%u_reach_down", ol_idx, desc_idx);
   desc->reach_down = 1.0f;
   config_get_float(ol->conf, conf_key, &desc->reach_down);

   input_overlay_desc_update_hitbox(desc);
   
   snprintf(conf_key, sizeof(conf_key),
         "overlay%u_desc%u_movable", ol_idx, desc_idx);
   desc->movable = false;
   desc->delta_x = 0.0f;
   desc->delta_y = 0.0f;
   config_get_bool(ol->conf, conf_key, &desc->movable);

   input_overlay->pos ++;

end:
   if (list)
      string_list_free(list);
   return ret;
}

static ssize_t input_overlay_find_index(const struct overlay *ol,
      const char *name, size_t size)
{
   size_t i;

   if (!ol)
      return -1;

   for (i = 0; i < size; i++)
   {
      if (!strcmp(ol[i].name, name))
         return i;
   }

   return -1;
}

static bool input_overlay_resolve_targets(struct overlay *ol,
      size_t idx, size_t size)
{
   size_t i;
   struct overlay *current = NULL;

   if (!ol)
      return false;
   
   current = (struct overlay*)&ol[idx];

   for (i = 0; i < current->size; i++)
   {
      const char *next = current->descs[i].next_index_name;

      if (*next)
      {
         ssize_t next_idx = input_overlay_find_index(ol, next, size);

         if (next_idx < 0)
         {
            RARCH_ERR("[Overlay]: Couldn't find overlay called: \"%s\".\n",
                  next);
            return false;
         }

         current->descs[i].next_index = next_idx;
      }
      else
         current->descs[i].next_index = (idx + 1) % size;
   }

   return true;
}

static void input_overlay_load_active(input_overlay_t *ol,
      float opacity)
{
   if (!ol)
      return;

   ol->iface->load(ol->iface_data, ol->active->load_images,
         ol->active->load_images_size);

   input_overlay_set_alpha_mod(ol, opacity);
   input_overlay_set_vertex_geom(ol);
   ol->iface->full_screen(ol->iface_data, ol->active->full_screen);
}

bool input_overlay_load_overlays_resolve_iterate(input_overlay_t *ol)
{
   bool not_done = true;

   if (!ol)
      return false;

   not_done = ol->resolve_pos < ol->size;

   if (!not_done)
   {
      ol->state = OVERLAY_STATUS_DEFERRED_DONE;
      return true;
   }

   if (!input_overlay_resolve_targets(ol->overlays, ol->resolve_pos, ol->size))
   {
      RARCH_ERR("[Overlay]: Failed to resolve next targets.\n");
      goto error;
   }

   if (ol->resolve_pos == 0)
   {  
      ol->active = &ol->overlays[0];
      
      input_overlay_load_active(ol, ol->deferred.opacity);
      input_overlay_enable(ol, ol->deferred.enable);
   }

   ol->resolve_pos += 1;

   return true;
error:
   ol->state = OVERLAY_STATUS_DEFERRED_ERROR;

   return false;
}


static bool input_overlay_load_overlay_image_done(struct overlay *overlay)
{
   overlay->pos = 0;
   /* Divide iteration steps by half of total descs if size is even,
    * otherwise default to 8 (arbitrary value for now to speed things up). */
   overlay->pos_increment = (overlay->size / 2) ? (overlay->size / 2) : 8;

#if 0
   RARCH_LOG("pos increment: %u\n", overlay->pos_increment);
#endif

   return true;
}

bool input_overlay_load_overlays_iterate(input_overlay_t *ol)
{
   size_t i                = 0;
   bool not_done           = true;
   struct overlay *overlay = NULL;
   
   if (!ol)
      return false;

   overlay = &ol->overlays[ol->pos];

   not_done = ol->pos < ol->size;

   if (!not_done)
   {
      ol->state = OVERLAY_STATUS_DEFERRED_LOADING_RESOLVE;
      return true;
   }

   switch (ol->loading_status)
   {
      case OVERLAY_IMAGE_TRANSFER_NONE:
      case OVERLAY_IMAGE_TRANSFER_BUSY:
         ol->loading_status = OVERLAY_IMAGE_TRANSFER_DONE;
         break;
      case OVERLAY_IMAGE_TRANSFER_DONE:
         input_overlay_load_overlay_image_done(&ol->overlays[ol->pos]);
         ol->loading_status = OVERLAY_IMAGE_TRANSFER_DESC_IMAGE_ITERATE;
         ol->overlays[ol->pos].pos = 0;
         break;
      case OVERLAY_IMAGE_TRANSFER_DESC_IMAGE_ITERATE:
         for (i = 0; i < overlay->pos_increment; i++)
         {
            if (overlay->pos < overlay->size)
            {
               input_overlay_load_desc_image(ol,
                     &overlay->descs[overlay->pos], overlay,
                     ol->pos, overlay->pos);
            }
            else
            {
               overlay->pos       = 0;
               ol->loading_status = OVERLAY_IMAGE_TRANSFER_DESC_ITERATE;
               break;
            }

         }
         break;
      case OVERLAY_IMAGE_TRANSFER_DESC_ITERATE:
         for (i = 0; i < overlay->pos_increment; i++)
         {
            if (overlay->pos < overlay->size)
            {
               if (!input_overlay_load_desc(ol, &overlay->descs[overlay->pos], overlay,
                        ol->pos, overlay->pos,
                        overlay->image.width, overlay->image.height,
                        overlay->config.normalized,
                        overlay->config.alpha_mod, overlay->config.range_mod))
               {
                  RARCH_ERR("[Overlay]: Failed to load overlay descs for overlay #%u.\n",
                        (unsigned)overlay->pos);
                  goto error;
               }
            }
            else
            {
               overlay->pos       = 0;
               ol->loading_status = OVERLAY_IMAGE_TRANSFER_DESC_DONE;
               break;
            }
         }
         break;
      case OVERLAY_IMAGE_TRANSFER_DESC_DONE:
         if (ol->pos == 0)
            input_overlay_load_overlays_resolve_iterate(ol);
         if (!driver_get_ptr()->osk_enable)
         {
            input_overlay_update_aspect_and_vertical_vals(ol, ol->pos);
            input_overlay_set_vertex_geom(ol);
         }
         ol->pos += 1;
         ol->loading_status = OVERLAY_IMAGE_TRANSFER_NONE;
         break;
      case OVERLAY_IMAGE_TRANSFER_ERROR:
         goto error;
   }

   return true;

error:
   ol->state = OVERLAY_STATUS_DEFERRED_ERROR;

   return false;
}


bool input_overlay_load_overlays(input_overlay_t *ol)
{
   unsigned i;

   for (i = 0; i < ol->pos_increment; i++, ol->pos++)
   {
      char conf_key[64]                  = {0};
      char overlay_full_screen_key[64]   = {0};
      char overlay_lightgun_key[64]      = {0};
      struct overlay            *overlay = NULL;
      bool                       to_cont = ol->pos < ol->size;
      
      if (!to_cont)
      {
         ol->pos   = 0;
         ol->state = OVERLAY_STATUS_DEFERRED_LOADING;
         break;
      }

      overlay = &ol->overlays[ol->pos];

      if (!overlay)
         continue;

      snprintf(overlay->config.descs.key,
            sizeof(overlay->config.descs.key), "overlay%u_descs", ol->pos);

      if (!config_get_uint(ol->conf, overlay->config.descs.key, &overlay->config.descs.size))
      {
         RARCH_ERR("[Overlay]: Failed to read number of descs from config key: %s.\n",
               overlay->config.descs.key);
         goto error;
      }

      overlay->descs = (struct overlay_desc*)
         calloc(overlay->config.descs.size, sizeof(*overlay->descs));

      if (!overlay->descs)
      {
         RARCH_ERR("[Overlay]: Failed to allocate descs.\n");
         goto error;
      }

      overlay->size = overlay->config.descs.size;

      snprintf(overlay_full_screen_key, sizeof(overlay_full_screen_key),
            "overlay%u_full_screen", ol->pos);
      overlay->full_screen = false;
      config_get_bool(ol->conf, overlay_full_screen_key, &overlay->full_screen);
      
      snprintf(overlay_lightgun_key, sizeof(overlay_lightgun_key),
            "overlay%u_lightgun", ol->pos);
      overlay->lightgun_overlay = false;
      config_get_bool(ol->conf, overlay_lightgun_key, &overlay->lightgun_overlay);

      overlay->config.normalized = false;
      overlay->config.alpha_mod  = 1.0f;
      overlay->config.range_mod  = 1.0f;

      snprintf(conf_key, sizeof(conf_key),
            "overlay%u_normalized", ol->pos);
      config_get_bool(ol->conf, conf_key, &overlay->config.normalized);

      snprintf(conf_key, sizeof(conf_key), "overlay%u_alpha_mod", ol->pos);
      config_get_float(ol->conf, conf_key, &overlay->config.alpha_mod);

      snprintf(conf_key, sizeof(conf_key), "overlay%u_range_mod", ol->pos);
      config_get_float(ol->conf, conf_key, &overlay->config.range_mod);

      /* Precache load image array for simplicity. */
      overlay->load_images = (struct texture_image*)
         calloc(1 + overlay->size, sizeof(struct texture_image));

      if (!overlay->load_images)
      {
         RARCH_ERR("[Overlay]: Failed to allocate load_images.\n");
         goto error;
      }

      snprintf(overlay->config.paths.key, sizeof(overlay->config.paths.key),
            "overlay%u_overlay", ol->pos);
      
      config_get_path(ol->conf, overlay->config.paths.key,
               overlay->config.paths.path, sizeof(overlay->config.paths.path));

      if (overlay->config.paths.path[0] != '\0')
      {
         char overlay_resolved_path[PATH_MAX_LENGTH] = {0};

         fill_pathname_resolve_relative(overlay_resolved_path, ol->overlay_path,
               overlay->config.paths.path, sizeof(overlay_resolved_path));

         if (!input_overlay_load_texture_image(overlay, &overlay->image, overlay_resolved_path))
         {
            RARCH_ERR("[Overlay]: Failed to load image: %s.\n",
                  overlay_resolved_path);
            ol->loading_status = OVERLAY_IMAGE_TRANSFER_ERROR;
            goto error;
         }

      }

      snprintf(overlay->config.names.key, sizeof(overlay->config.names.key),
            "overlay%u_name", ol->pos);
      config_get_array(ol->conf, overlay->config.names.key,
            overlay->name, sizeof(overlay->name));

      /* By default, we stretch the overlay out in full. */
      overlay->x = overlay->y = 0.0f;
      overlay->w = overlay->h = 1.0f;

      snprintf(overlay->config.rect.key, sizeof(overlay->config.rect.key),
            "overlay%u_rect", ol->pos);

      if (config_get_array(ol->conf, overlay->config.rect.key,
               overlay->config.rect.array, sizeof(overlay->config.rect.array)))
      {
         struct string_list *list = string_split(overlay->config.rect.array, ", ");

         if (!list || list->size < 4)
         {
            RARCH_ERR("[Overlay]: Failed to split rect \"%s\" into at least four tokens.\n",
                  overlay->config.rect.array);
            string_list_free(list);
            goto error;
         }

         overlay->x = (float)strtod(list->elems[0].data, NULL);
         overlay->y = (float)strtod(list->elems[1].data, NULL);
         overlay->w = (float)strtod(list->elems[2].data, NULL);
         overlay->h = (float)strtod(list->elems[3].data, NULL);
         string_list_free(list);
      }

      /* Assume for now that scaling center is in the middle.
       * TODO: Make this configurable. */
      overlay->block_scale = false;
      overlay->center_x = overlay->x + 0.5f * overlay->w;
      overlay->center_y = overlay->y + 0.5f * overlay->h;
   }


   return true;

error:
   ol->pos   = 0;
   ol->state = OVERLAY_STATUS_DEFERRED_ERROR;

   return false;
}


bool input_overlay_new_done(input_overlay_t *ol)
{
   if (!ol)
      return false;

   input_overlay_set_alpha_mod(ol, ol->deferred.opacity);
   input_overlay_set_scale_factor(ol, ol->deferred.scale_factor);
   ol->next_index = (ol->index + 1) % ol->size;

   ol->state = OVERLAY_STATUS_ALIVE;

   if (ol->conf)
      config_file_free(ol->conf);
   ol->conf = NULL;

   return true;
}

static bool input_overlay_load_overlays_init(input_overlay_t *ol)
{
   if (!config_get_uint(ol->conf, "overlays", &ol->config.overlays.size))
   {
      RARCH_ERR("overlays variable not defined in config.\n");
      goto error;
   }

   if (!ol->config.overlays.size)
      goto error;

   ol->overlays = (struct overlay*)calloc(
         ol->config.overlays.size, sizeof(*ol->overlays));
   if (!ol->overlays)
      goto error;

   ol->size          = ol->config.overlays.size;
   ol->pos           = 0;
   ol->resolve_pos   = 0;
   ol->pos_increment = (ol->size / 4) ? (ol->size / 4) : 4;

   return true;

error:
   ol->state = OVERLAY_STATUS_DEFERRED_ERROR;

   return false;
}

/**
 * input_overlay_new:
 * @path                  : Path to overlay file.
 * @enable                : Enable the overlay after initializing it?
 *
 * Creates and initializes an overlay handle.
 *
 * Returns: Overlay handle on success, otherwise NULL.
 **/
input_overlay_t *input_overlay_new(const char *path, bool enable,
      float opacity, float scale_factor)
{
   input_overlay_t *ol = (input_overlay_t*)calloc(1, sizeof(*ol));
   driver_t *driver    = driver_get_ptr();

   if (!ol)
      goto error;

   ol->overlay_path    = strdup(path);
   if (!ol->overlay_path)
   {
      free(ol);
      return NULL;
   }

   ol->conf            = config_file_new(ol->overlay_path);

   if (!ol->conf)
      goto error;

   if (!video_driver_overlay_interface(&ol->iface))
   {
      RARCH_ERR("Overlay interface is not present in video driver.\n");
      goto error;
   }

   ol->iface_data = driver->video_data;

   if (!ol->iface)
      goto error;

   ol->state                 = OVERLAY_STATUS_DEFERRED_LOAD;
   ol->deferred.enable       = enable;
   ol->deferred.opacity      = opacity;
   ol->deferred.scale_factor = scale_factor;
   ol->pos                   = 0;

   input_overlay_load_overlays_init(ol);
   
   input_overlay_populate_8way_vals();

   return ol;

error:
   input_overlay_free(ol);
   return NULL;
}

/**
 * input_overlay_enable:
 * @ol                    : Overlay handle.
 * @enable                : Enable or disable the overlay
 *
 * Enable or disable the overlay.
 **/
void input_overlay_enable(input_overlay_t *ol, bool enable)
{
   if (!ol)
      return;
   ol->enable = enable;
   ol->iface->enable(ol->iface_data, enable);
}

/**
 * eight_way_direction
 * @param vals for an eight-way area descriptor
 * @param x_offset relative to 8-way center, normalized as fraction of screen height
 * @param y_offset relative to 8-way center, normalized as fraction of screen height
 * @return input state representing the offset direction as @vals
 */
static inline uint64_t eight_way_direction(struct overlay_eight_way_vals* vals,
                                           float x_offset,
                                           const float y_offset)
{
   if (x_offset == 0.0f)
     x_offset = 0.000001f;
   float abs_slope = fabs(y_offset / x_offset);
   
   if (x_offset > 0.0f)
   {
      if (y_offset > 0.0f)
      { /* Q1 */
         if (abs_slope > vals->slope_high)
            return vals->up;
         else if (abs_slope < vals->slope_low)
            return vals->right;
         else return vals->up_right;
      }
      else
      { /* Q4 */
         if (abs_slope > vals->slope_high)
            return vals->down;
         else if (abs_slope < vals->slope_low)
            return vals->right;
         else return vals->down_right;
      }
   }
   else
   {
      if (y_offset > 0.0f)
      { /* Q2 */
         if (abs_slope > vals->slope_high)
            return vals->up;
         else if (abs_slope < vals->slope_low)
            return vals->left;
         else return vals->up_left;
      }
      else
      { /* Q3 */
         if (abs_slope > vals->slope_high)
            return vals->down;
         else if (abs_slope < vals->slope_low)
            return vals->left;
         else return vals->down_left;
      }
   }
   
   return UINT64_C(0);
}

static inline uint64_t four_way_direction(struct overlay_eight_way_vals* vals,
                                          float x_offset,
                                          const float y_offset)
{
   if (x_offset == 0.0f)
     x_offset = 0.000001f;
   float abs_slope = fabs(y_offset / x_offset);
   
   if (x_offset > 0.0f)
   {
      if (y_offset > 0.0f)
      { /* Q1 */
         if (abs_slope > 1.0f)
            return vals->up;
         else return vals->right;
      }
      else
      { /* Q4 */
         if (abs_slope > 1.0f)
            return vals->down;
         else return vals->right;
      }
   }
   else
   {
      if (y_offset > 0.0f)
      { /* Q2 */
         if (abs_slope > 1.0f)
            return vals->up;
         else return vals->left;
      }
      else
      { /* Q3 */
         if (abs_slope > 1.0f)
            return vals->down;
         else return vals->left;
      }
   }
   
   return UINT64_C(0);
}

/**
 * eight_way_ellipse_coverage:
 * @param vals for an eight-way area descriptor
 * @param x_ellipse_offset relative to 8-way center, normalized as fraction of screen height
 * @param y_ellipse_offset relative to 8-way center, normalized as fraction of screen height
 * @return the input state representing the @vals covered by ellipse area
 * 
 * Requires the input driver to call set_ellipse during poll.
 * Approximates ellipse as a diamond and checks vertex overlap with @vals.
 */
static inline uint64_t eight_way_ellipse_coverage(struct overlay_eight_way_vals* vals,
                                                  const float x_ellipse_offset,
                                                  const float y_ellipse_offset)
{
   settings_t* settings = config_get_ptr();
   float radius_major, radius_minor;
   unsigned screen_width, screen_height;
   float x_offset, y_offset;
   float x_major_offset, y_major_offset;
   float x_minor_offset, y_minor_offset;
   float major_angle;
   float sin_major, cos_major, sin_minor, cos_minor;
   float boost;
   uint64_t state = UINT64_C(0);
   
   /* for pointer tools */
   if (ellipse.major_px[overlay_ptr_idx] == 0)
      return four_way_direction(vals, x_ellipse_offset, y_ellipse_offset);

   /* hack for inaccurate touchscreens */
   boost = settings->input.touch_ellipse_magnify;

   /* normalize radii by screen height to keep aspect ratio */
   video_driver_get_size(&screen_width, &screen_height);
   radius_major = boost * ellipse.major_px[overlay_ptr_idx] / (2*screen_height);
   radius_minor = boost * ellipse.minor_px[overlay_ptr_idx] / (2*screen_height);
   
   /* get axis endpoints */
   major_angle = ellipse.orientation[overlay_ptr_idx] > 0 ?
      ((float)M_PI/2 - ellipse.orientation[overlay_ptr_idx])
      : ((float)(-M_PI)/2 - ellipse.orientation[overlay_ptr_idx]);
   sin_major = sin(major_angle);
   cos_major = cos(major_angle);
   sin_minor = major_angle > 0 ? cos_major : -cos_major;
   cos_minor = major_angle > 0 ? -sin_major : sin_major;
   
   x_major_offset = radius_major * cos_major;
   y_major_offset = radius_major * sin_major;
   x_minor_offset = radius_minor * cos_minor;
   y_minor_offset = radius_minor * sin_minor;
   
   /* major axis endpoint 1 */
   x_offset = x_ellipse_offset + x_major_offset;
   y_offset = y_ellipse_offset + y_major_offset;
   state |= four_way_direction(vals, x_offset, y_offset);
   
   /* major axis endpoint 2 */
   x_offset = x_ellipse_offset - x_major_offset;
   y_offset = y_ellipse_offset - y_major_offset;
   state |= four_way_direction(vals, x_offset, y_offset);

   /* minor axis endpoint 1 */
   x_offset = x_ellipse_offset + x_minor_offset;
   y_offset = y_ellipse_offset + y_minor_offset;
   state |= four_way_direction(vals, x_offset, y_offset);

   /* minor axis endpoint 2 */
   x_offset = x_ellipse_offset - x_minor_offset;
   y_offset = y_ellipse_offset - y_minor_offset;
   state |= four_way_direction(vals, x_offset, y_offset);
   
   return state;
}

/**
 * eight_way_state:
 * @desc_ptr              : Overlay descriptor handle.
 * @type                  : Type of eight-way area. See overlay_eight_way_type enum
 * @x                     : X coordinate value.
 * @y                     : Y coordinate value.
 *
 * Returns the low level input state based on @x, @y, and ellipse_px valuess
 **/
static inline uint64_t eight_way_state(const struct overlay_desc *desc_ptr,
                                       unsigned area_type,
                                       const float x, const float y)
{
   settings_t* settings = config_get_ptr();
   uint64_t state = 0;
   struct overlay_eight_way_vals* vals = &eight_way_vals[area_type];
   
   float x_offset = (x - desc_ptr->x) * adj.display_aspect;
   float y_offset = (desc_ptr->y - y);
   unsigned method = area_type < ABXY_AREA ?
                     settings->input.dpad_method : settings->input.abxy_method;
   
   if (method != TOUCH_AREA)
      state |= eight_way_direction(vals, x_offset, y_offset);
   
   if (method != VECTOR)
      state |= eight_way_ellipse_coverage(vals, x_offset, y_offset);
   
   return state;
}

/**
 * translate_highlevel_mask:
 * @desc_ptr              : Overlay descriptor handle.
 * @out                   : Polled output data.
 * @x                     : X coordinate value.
 * @y                     : Y coordinate value.
 *
 * Translate high level input into something usable.
 * For now, "high level" means multi-button areas and lightgun buttons
 *
 **/
void translate_highlevel_mask(const struct overlay_desc *desc_ptr,
                              input_overlay_state_t *out,
                              const float x, const float y)
{
   const static uint64_t lightgun_mask
      = (UINT64_C(1) << RARCH_LIGHTGUN_RELOAD)
      | (UINT64_C(1) << RARCH_LIGHTGUN_TRIGGER)
      | (UINT64_C(1) << RARCH_LIGHTGUN_AUX_A)
      | (UINT64_C(1) << RARCH_LIGHTGUN_AUX_B)
      | (UINT64_C(1) << RARCH_LIGHTGUN_START)
      | (UINT64_C(1) << RARCH_LIGHTGUN_SELECT)
      | (UINT64_C(1) << RARCH_LIGHTGUN_AUX_C)
      | (UINT64_C(1) << RARCH_LIGHTGUN_DPAD_UP)
      | (UINT64_C(1) << RARCH_LIGHTGUN_DPAD_DOWN)
      | (UINT64_C(1) << RARCH_LIGHTGUN_DPAD_LEFT)
      | (UINT64_C(1) << RARCH_LIGHTGUN_DPAD_RIGHT);
   const static uint64_t multibutton_mask
      = (UINT64_C(1) << RARCH_JOYPAD_DPAD_AREA)
      | (UINT64_C(1) << RARCH_ANALOG_DPAD_AREA)
      | (UINT64_C(1) << RARCH_JOYPAD_ABXY_AREA)
      | (UINT64_C(1) << RARCH_JOYPAD_ABRL_AREA)
      | (UINT64_C(1) << RARCH_JOYPAD_ABRL2_AREA)
      | (UINT64_C(1) << RARCH_JOYPAD_AB_AREA);
   
   uint64_t mask = desc_ptr->highlevel_mask;
   
   if ( lightgun_active )
   {
      switch (mask & lightgun_mask)
      {  /* expected cases */
      case (UINT64_C(0)):
         break;
      case (UINT64_C(1) << RARCH_LIGHTGUN_AUX_A):
         out->lightgun_buttons |= (1<<RETRO_DEVICE_ID_LIGHTGUN_AUX_A);
         break;
      case (UINT64_C(1) << RARCH_LIGHTGUN_AUX_B):
         out->lightgun_buttons |= (1<<RETRO_DEVICE_ID_LIGHTGUN_AUX_B);
         break;
      case (UINT64_C(1) << RARCH_LIGHTGUN_AUX_C):
         out->lightgun_buttons |= (1<<RETRO_DEVICE_ID_LIGHTGUN_AUX_C);
         break;
      case (UINT64_C(1) << RARCH_LIGHTGUN_DPAD_UP):
         out->lightgun_buttons |= (1<<RETRO_DEVICE_ID_LIGHTGUN_DPAD_UP);
         break;
      case ((UINT64_C(1) << RARCH_LIGHTGUN_DPAD_UP) | (UINT64_C(1) << RARCH_LIGHTGUN_DPAD_RIGHT)):
         out->lightgun_buttons |= (1<<RETRO_DEVICE_ID_LIGHTGUN_DPAD_UP)
                                  | (1<<RETRO_DEVICE_ID_LIGHTGUN_DPAD_RIGHT);
         break;
      case ((UINT64_C(1) << RARCH_LIGHTGUN_DPAD_UP) | (UINT64_C(1) << RARCH_LIGHTGUN_DPAD_LEFT)):
         out->lightgun_buttons |= (1<<RETRO_DEVICE_ID_LIGHTGUN_DPAD_UP)
                                  | (1<<RETRO_DEVICE_ID_LIGHTGUN_DPAD_LEFT);
         break;
      case (UINT64_C(1) << RARCH_LIGHTGUN_DPAD_DOWN):
         out->lightgun_buttons |= (1<<RETRO_DEVICE_ID_LIGHTGUN_DPAD_DOWN);
         break;
      case ((UINT64_C(1) << RARCH_LIGHTGUN_DPAD_DOWN) | (UINT64_C(1) << RARCH_LIGHTGUN_DPAD_LEFT)):
         out->lightgun_buttons |= (1<<RETRO_DEVICE_ID_LIGHTGUN_DPAD_DOWN)
                                  | (1<<RETRO_DEVICE_ID_LIGHTGUN_DPAD_LEFT);
         break;
      case ((UINT64_C(1) << RARCH_LIGHTGUN_DPAD_DOWN) | (UINT64_C(1) << RARCH_LIGHTGUN_DPAD_RIGHT)):
         out->lightgun_buttons |= (1<<RETRO_DEVICE_ID_LIGHTGUN_DPAD_DOWN)
                                  | (1<<RETRO_DEVICE_ID_LIGHTGUN_DPAD_LEFT);
         break;
      case(UINT64_C(1) << RARCH_LIGHTGUN_DPAD_LEFT):
         out->lightgun_buttons |= (1<<RETRO_DEVICE_ID_LIGHTGUN_DPAD_LEFT);
         break;
      case (UINT64_C(1) << RARCH_LIGHTGUN_DPAD_RIGHT):
         out->lightgun_buttons |= (1<<RETRO_DEVICE_ID_LIGHTGUN_DPAD_RIGHT);
         break;
      case (UINT64_C(1) << RARCH_LIGHTGUN_SELECT):
         out->lightgun_buttons |= (1<<RETRO_DEVICE_ID_LIGHTGUN_SELECT);
         break;
      case (UINT64_C(1) << RARCH_LIGHTGUN_START): /* set start & pause */
         out->lightgun_buttons |= (3<<RETRO_DEVICE_ID_LIGHTGUN_PAUSE);
         break;
      case (UINT64_C(1) << RARCH_LIGHTGUN_TRIGGER):
         out->lightgun_buttons |= (1<<RETRO_DEVICE_ID_LIGHTGUN_TRIGGER);
         break;
      case (UINT64_C(1) << RARCH_LIGHTGUN_RELOAD):
         out->lightgun_buttons |= (1<<RARCH_LIGHTGUN_BIT_RELOAD);
         break;
      default:  /* overlapping buttons */
         if ( mask & (UINT64_C(1) << RARCH_LIGHTGUN_AUX_A) )
            out->lightgun_buttons |= (1<<RETRO_DEVICE_ID_LIGHTGUN_AUX_A);
         if ( mask & (UINT64_C(1) << RARCH_LIGHTGUN_AUX_B) )
            out->lightgun_buttons |= (1<<RETRO_DEVICE_ID_LIGHTGUN_AUX_B);
         if ( mask & (UINT64_C(1) << RARCH_LIGHTGUN_AUX_C) )
            out->lightgun_buttons |= (1<<RETRO_DEVICE_ID_LIGHTGUN_AUX_C);
         if ( mask & (UINT64_C(1) << RARCH_LIGHTGUN_DPAD_UP) )
            out->lightgun_buttons |= (1<<RETRO_DEVICE_ID_LIGHTGUN_DPAD_UP);
         if ( mask & (UINT64_C(1) << RARCH_LIGHTGUN_DPAD_DOWN) )
            out->lightgun_buttons |= (1<<RETRO_DEVICE_ID_LIGHTGUN_DPAD_DOWN);
         if ( mask & (UINT64_C(1) << RARCH_LIGHTGUN_DPAD_LEFT) )
            out->lightgun_buttons |= (1<<RETRO_DEVICE_ID_LIGHTGUN_DPAD_LEFT);
         if ( mask & (UINT64_C(1) << RARCH_LIGHTGUN_DPAD_RIGHT) )
            out->lightgun_buttons |= (1<<RETRO_DEVICE_ID_LIGHTGUN_DPAD_RIGHT);
         if ( mask & (UINT64_C(1) << RARCH_LIGHTGUN_SELECT) )
            out->lightgun_buttons |= (1<<RETRO_DEVICE_ID_LIGHTGUN_SELECT);
         if ( mask & (UINT64_C(1) << RARCH_LIGHTGUN_START) )
            out->lightgun_buttons |= (3<<RETRO_DEVICE_ID_LIGHTGUN_PAUSE);
         if ( mask & (UINT64_C(1) << RARCH_LIGHTGUN_TRIGGER) )
            out->lightgun_buttons |= (1<<RETRO_DEVICE_ID_LIGHTGUN_TRIGGER);
         if ( mask & (UINT64_C(1) << RARCH_LIGHTGUN_RELOAD) )
            out->lightgun_buttons |= (1<<RARCH_LIGHTGUN_BIT_RELOAD);
      }
   }

   switch (mask & multibutton_mask)
   {  /* expected cases */
   case (UINT64_C(0)):
      break;
   case (UINT64_C(1) << RARCH_ANALOG_DPAD_AREA):
      out->buttons |= eight_way_state(desc_ptr, ANALOG_DPAD_AREA, x, y);
      break;
   case (UINT64_C(1) << RARCH_JOYPAD_DPAD_AREA):
      out->buttons |= eight_way_state(desc_ptr, DPAD_AREA, x, y);
      break;
   case ((UINT64_C(1) << RARCH_JOYPAD_DPAD_AREA) | (UINT64_C(1) << RARCH_ANALOG_DPAD_AREA)):
      out->buttons |= eight_way_state(desc_ptr, ANALOG_DPAD_AREA, x, y)
                      | eight_way_state(desc_ptr, DPAD_AREA, x, y);
      break;
   case (UINT64_C(1) << RARCH_JOYPAD_ABXY_AREA):
      out->buttons |= eight_way_state(desc_ptr, ABXY_AREA, x, y);
      break;
   case (UINT64_C(1) << RARCH_JOYPAD_ABRL_AREA):
      out->buttons |= eight_way_state(desc_ptr, ABRL_AREA, x, y);
      break;
   case (UINT64_C(1) << RARCH_JOYPAD_ABRL2_AREA):
      out->buttons |= eight_way_state(desc_ptr, ABRL2_AREA, x, y);
      break;
   case (UINT64_C(1) << RARCH_JOYPAD_AB_AREA):
      out->buttons |= eight_way_state(desc_ptr, AB_AREA, x, y);
      break;
   default:  /* overlapping buttons */
      if ( mask & ( UINT64_C(1) << RARCH_ANALOG_DPAD_AREA ) )
         out->buttons |= eight_way_state(desc_ptr, ANALOG_DPAD_AREA, x, y);
      if ( mask & ( UINT64_C(1) << RARCH_JOYPAD_DPAD_AREA ) )
         out->buttons |= eight_way_state(desc_ptr, DPAD_AREA, x, y);
      if ( mask & ( UINT64_C(1) << RARCH_JOYPAD_ABXY_AREA ) )
         out->buttons |= eight_way_state(desc_ptr, ABXY_AREA, x, y);
      if ( mask & ( UINT64_C(1) << RARCH_JOYPAD_ABRL_AREA ) )
         out->buttons |= eight_way_state(desc_ptr, ABRL_AREA, x, y);
      if ( mask & ( UINT64_C(1) << RARCH_JOYPAD_ABRL2_AREA ) )
         out->buttons |= eight_way_state(desc_ptr, ABRL2_AREA, x, y);
      if ( mask & (UINT64_C(1) << RARCH_JOYPAD_AB_AREA ) )
         out->buttons |= eight_way_state(desc_ptr, AB_AREA, x, y);
   }
}


/**
 * input_overlay_undo_meta_overlap
 * @param out                 : overlay state for a single pointer
 *
 * Disallows simultaneous use of meta keys with other controls. Meta keys take
 * priority unless other controls were already active.
 */
static inline void input_overlay_undo_meta_overlap(input_overlay_state_t* out)
{
   input_overlay_state_t* old_state;
   uint64_t active_meta = out->buttons & META_KEY_MASK;
   uint64_t active_other = out->buttons & ~META_KEY_MASK;
   
   if (active_meta && (active_other || (uint64_t)*out->analog))
   {
      old_state = &driver_get_ptr()->old_overlay_state;

      if ( (active_other & old_state->buttons)
           || ((uint32_t)out->analog[0] && (uint32_t)old_state->analog[0])
           || ((uint32_t)out->analog[2] && (uint32_t)old_state->analog[2]) )
        out->buttons = active_other;
      else
      {
         out->buttons = active_meta;
         memset(out->analog, 0, sizeof(4*sizeof(int16_t)));
      }
   }
}

/**
 * inside_hitbox:
 * @desc                  : Overlay descriptor handle.
 * @x                     : X coordinate value.
 * @y                     : Y coordinate value.
 *
 * Check whether the given @x and @y coordinates of the overlay
 * descriptor @desc is inside the overlay descriptor's hitbox.
 *
 * Returns: true (1) if X, Y coordinates are inside a hitbox, otherwise false (0). 
 **/
static bool inside_hitbox(const struct overlay_desc *desc, float x, float y)
{
   if (!desc)
      return false;

   switch (desc->hitbox)
   {
      case OVERLAY_HITBOX_RADIAL:
      {
         /* Ellipse. */
         float x_dist = (x - desc->x_hitbox) / desc->range_x_mod;
         float y_dist = (y - desc->y_hitbox) / desc->range_y_mod;
         float sq_dist = x_dist * x_dist + y_dist * y_dist;
         return (sq_dist <= 1.0f);
      }

      case OVERLAY_HITBOX_RECT:
         return (fabs(x - desc->x_hitbox) <= desc->range_x_mod) &&
            (fabs(y - desc->y_hitbox) <= desc->range_y_mod);
   }

   return false;
}

/**
 * input_overlay_poll:
 * @ol                    : Overlay handle.
 * @out                   : Polled output data.
 * @norm_x                : Normalized X coordinate.
 * @norm_y                : Normalized Y coordinate.
 *
 * Polls input overlay.
 *
 * @norm_x and @norm_y are the result of
 * input_translate_coord_viewport().
 **/
void input_overlay_poll(input_overlay_t *ol, input_overlay_state_t *out,
                        int16_t norm_x, int16_t norm_y)
{
   size_t i;
   float x, y;

   memset(out, 0, sizeof(*out));

   if (!ol->enable)
   {
      ol->blocked = false;
      return;
   }
   if (ol->blocked)
      return;

   /* norm_x and norm_y is in [-0x7fff, 0x7fff] range,
    * like RETRO_DEVICE_POINTER. */
   x = (float)(norm_x + 0x7fff) / 0xffff;
   y = (float)(norm_y + 0x7fff) / 0xffff;

   x -= ol->active->mod_x;
   y -= ol->active->mod_y;
   x /= ol->active->mod_w;
   y /= ol->active->mod_h;

   for (i = 0; i < ol->active->size; i++)
   {
      struct overlay_desc *desc = &ol->active->descs[i];

      if (!desc)
         continue;
      if (!inside_hitbox(desc, x, y))
         continue;

      desc->updated = true;

      if (desc->type == OVERLAY_TYPE_BUTTONS)
      {
         out->buttons |= desc->key_mask;
         translate_highlevel_mask(desc, out, x, y);

         if (desc->key_mask & (UINT64_C(1) << RARCH_OVERLAY_NEXT))
            ol->next_index = desc->next_index;
      }
      else if (desc->type == OVERLAY_TYPE_KEYBOARD)
      {
         if (desc->key_mask < RETROK_LAST)
            OVERLAY_SET_KEY(out, desc->key_mask);
      }
      else
      {
         /* Ignore anything overlapping an analog area when its hitbox is extended */
         bool ignore_other = (desc->range_x_mod > desc->range_x_hitbox);
         if (ignore_other)
            memset(out,0,sizeof(*out));
         
         float x_dist    = x - desc->x;
         float y_dist    = y - desc->y;
         float x_val     = x_dist / desc->range_x;
         float y_val     = y_dist / desc->range_y;
         float x_val_sat = x_val / desc->analog_saturate_pct;
         float y_val_sat = y_val / desc->analog_saturate_pct;

         unsigned int base = (desc->type == OVERLAY_TYPE_ANALOG_RIGHT) ? 2 : 0;

         out->analog[base + 0] = clamp_float(x_val_sat, -1.0f, 1.0f) * 32767.0f;
         out->analog[base + 1] = clamp_float(y_val_sat, -1.0f, 1.0f) * 32767.0f;
         
         if (desc->movable)
         {
            desc->delta_x = clamp_float(x_dist, -desc->range_x, desc->range_x)
               * ol->active->mod_w;
            desc->delta_y = clamp_float(y_dist, -desc->range_y, desc->range_y)
               * ol->active->mod_h;
         }
         
         if (ignore_other)
            break;
      }
   }
   
   input_overlay_undo_meta_overlap(out);
}

/**
 * input_overlay_update_desc_geom:
 * @ol                    : overlay handle.
 * @desc                  : overlay descriptors handle.
 * 
 * Update input overlay descriptors' vertex geometry.
 **/
static void input_overlay_update_desc_geom(input_overlay_t *ol,
      struct overlay_desc *desc)
{
   if (!desc || !desc->image.pixels)
      return;
   if (!desc->movable)
      return;

   ol->iface->vertex_geom(ol->iface_data, desc->image_index,
      desc->mod_x + desc->delta_x, desc->mod_y + desc->delta_y,
      desc->mod_w, desc->mod_h);

   desc->delta_x = 0.0f;
   desc->delta_y = 0.0f;
}

/**
 * input_overlay_post_poll:
 * @ol                    : overlay handle
 *
 * Called after all the input_overlay_poll() calls to
 * update the range modifiers for pressed/unpressed regions
 * and alpha mods.
 **/
void input_overlay_post_poll(input_overlay_t *ol, float opacity)
{
   size_t i;

   if (!ol)
      return;

   input_overlay_set_alpha_mod(ol, opacity);

   for (i = 0; i < ol->active->size; i++)
   {
      struct overlay_desc *desc = &ol->active->descs[i];

      if (!desc)
         continue;

      if (desc->updated)
      {
         /* If pressed this frame, change the hitbox. */
         desc->range_x_mod = desc->range_x_hitbox * desc->range_mod;
         desc->range_y_mod = desc->range_y_hitbox * desc->range_mod;

         if (desc->image.pixels)
            ol->iface->set_alpha(ol->iface_data, desc->image_index,
                  desc->alpha_mod * opacity);
      }
      else
      {
         desc->range_x_mod = desc->range_x_hitbox;
         desc->range_y_mod = desc->range_y_hitbox;
      }

      input_overlay_update_desc_geom(ol, desc);
      desc->updated = false;
   }
}

/**
 * input_overlay_poll_clear:
 * @ol                    : overlay handle
 *
 * Call when there is nothing to poll. Allows overlay to
 * clear certain state.
 **/
void input_overlay_poll_clear(input_overlay_t *ol, float opacity)
{
   size_t i;

   if (!ol)
      return;

   ol->blocked = false;

   input_overlay_set_alpha_mod(ol, opacity);

   for (i = 0; i < ol->active->size; i++)
   {
      struct overlay_desc *desc = &ol->active->descs[i];

      if (!desc)
         continue;

      desc->range_x_mod = desc->range_x_hitbox;
      desc->range_y_mod = desc->range_y_hitbox;
      desc->updated = false;

      desc->delta_x = 0.0f;
      desc->delta_y = 0.0f;
      input_overlay_update_desc_geom(ol, desc);
   }
}

/**
 * input_overlay_connect_lightgun
 * @param ol            : overlay handle
 * 
 * If overlay#_lightgun is true (in the .cfg file) for the active overlay,
 * connects overlay's input to the first lightgun device selected in Input
 * Settings, or the first found in the core if none is selected.
 * Then sets autotrigger if no trigger descriptor found.
 */
static void input_overlay_connect_lightgun(input_overlay_t *ol)
{
   global_t*   global   = global_get_ptr();
   settings_t* settings = config_get_ptr();
   static int old_port = -1;
   int port, i;
   char msg[64];
   struct retro_controller_info rci;
   bool generic = false;
   
   /* disconnect any connected lightgun */
   if (lightgun_active)
   {
      pretro_set_controller_port_device
         (old_port, settings->input.libretro_device[old_port]);
      lightgun_active = false;
   }
   
   if (ol->active->lightgun_overlay)
   {
      /* Search available ports. If a lightgun device is selected, use it. */
      for (port = 0; port < global->system.num_ports; port++)
      {
         if ( (RETRO_DEVICE_MASK & settings->input.libretro_device[port])
                 == RETRO_DEVICE_LIGHTGUN )
         {
            lightgun_active = true;
            break;
         }
      }

      /* If already connected, just get the device name */
      if (lightgun_active)
      {
         rci = global->system.ports[port];
         for (i = 0; i < rci.num_types; i++)
            if (rci.types[i].id == settings->input.libretro_device[port])
               break;
      }
      else for (port = 0; port < global->system.num_ports; port++)
      {  /* Otherwise, connect the first lightgun device found in this core. */
         rci = global->system.ports[port];
         for (i = 0; i < rci.num_types; i++)
         {
            if ((RETRO_DEVICE_MASK & rci.types[i].id) == RETRO_DEVICE_LIGHTGUN)
            {
               pretro_set_controller_port_device(port, rci.types[i].id);
               lightgun_active = true;
               break;
            }
         }
         if (lightgun_active) break;
      }

      if (!lightgun_active)
      {  /* Fall back to generic lightgun */
         port = 0;
         lightgun_active = true;
         generic = true;
      }
   }
   
   if (lightgun_active)
   {
      old_port = port;
      
      /* Notify user */
      snprintf(msg, 64, "%s active", generic ? "Lightgun" : rci.types[i].desc);
      rarch_main_msg_queue_push(msg, 2, 180, true);
      
      /* Set autotrigger if no trigger descriptor found */
      global->overlay_lightgun_autotrigger = true;
      for (i = 0; i < ol->active->size; i++)
      {
         if (ol->active->descs[i].highlevel_mask
             & (UINT64_C(1) << RARCH_LIGHTGUN_TRIGGER))
         {
            global->overlay_lightgun_autotrigger = false;
            break;
         }
      }
   }
}

/**
 * input_overlay_next:
 * @ol                    : Overlay handle.
 *
 * Switch to the next available overlay
 * screen.
 **/
void input_overlay_next(input_overlay_t *ol, float opacity)
{  
   if (!ol)
      return;

   ol->index = ol->next_index;
   ol->active = &ol->overlays[ol->index];

   input_overlay_load_active(ol, opacity);
   input_overlay_connect_lightgun(ol);
   
   ol->blocked = true;
   ol->next_index = (ol->index + 1) % ol->size;
}

/**
 * input_overlay_full_screen:
 * @ol                    : Overlay handle.
 *
 * Checks if the overlay is fullscreen.
 *
 * Returns: true (1) if overlay is fullscreen, otherwise false (0).
 **/
bool input_overlay_full_screen(input_overlay_t *ol)
{
   if (!ol)
      return false;
   return ol->active->full_screen;
}

/**
 * input_overlay_free:
 * @ol                    : Overlay handle.
 *
 * Frees overlay handle.
 **/
void input_overlay_free(input_overlay_t *ol)
{
   if (!ol)
      return;

   input_overlay_free_overlays(ol);

   if (ol->conf)
      config_file_free(ol->conf);
   ol->conf = NULL;

   if (ol->iface)
      ol->iface->enable(ol->iface_data, false);

   free(ol->overlay_path);
   free(ol);
}

/**
 * input_overlay_set_alpha_mod:
 * @ol                    : Overlay handle.
 * @mod                   : New modulating factor to apply.
 *
 * Sets a modulating factor for alpha channel. Default is 1.0.
 * The alpha factor is applied for all overlays.
 **/
void input_overlay_set_alpha_mod(input_overlay_t *ol, float mod)
{
   unsigned i;

   if (!ol)
      return;

   for (i = 0; i < ol->active->load_images_size; i++)
      ol->iface->set_alpha(ol->iface_data, i, mod);
}

bool input_overlay_lightgun_active()
{
   return lightgun_active;
}
