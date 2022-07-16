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
#include "input_keymaps.h"
#include "../dynamic.h"
#include "../performance.h"

#define BOX_RADIAL       0x18df06d2U
#define BOX_RECT         0x7c9d4d93U

#define KEY_ANALOG_LEFT  0x56b92e81U
#define KEY_ANALOG_RIGHT 0x2e4dc654U

#define MAX_TOUCH 16

static bool overlay_lightgun_active;
static bool overlay_mouse_active;
static bool overlay_adjust_needed;
static float disp_aspect = 1.778f;

typedef struct overlay_aspect_mod_vals
{
   float w;
   float x_center_shift;
   float x_bisect_shift;
   float h;
   float y_center_shift;
} overlay_aspect_mod_vals_t;

typedef struct overlay_mouse_vals
{
   float scale_x;
   float scale_y;

   int16_t prev_x;
   int16_t prev_y;

   uint8_t click;
   uint8_t hold;
} overlay_mouse_vals_t;

typedef struct ellipse_px
{
   float orientation[MAX_TOUCH];
   float major_px[MAX_TOUCH];
   float minor_px[MAX_TOUCH];
} ellipse_px_t;

static overlay_aspect_mod_vals_t ol_ar_mod;
static overlay_mouse_vals_t ol_mouse;
static ellipse_px_t ol_ellipse;

const struct overlay_eightway_vals menu_analog_vals = {
   UINT64_C(1)<<RETRO_DEVICE_ID_JOYPAD_UP,
   UINT64_C(1)<<RETRO_DEVICE_ID_JOYPAD_RIGHT,
   UINT64_C(1)<<RETRO_DEVICE_ID_JOYPAD_DOWN,
   UINT64_C(1)<<RETRO_DEVICE_ID_JOYPAD_LEFT,
   0, 0, 0, 0, NULL, NULL
};

struct overlay_aspect_ratio_elem overlay_aspectratio_lut[OVERLAY_ASPECT_RATIO_END] = {
   { "1:2",           0.5f },
   { "9:16",          0.5625f },
   { "10:16",         0.625f },
   { "3:4",           0.75f },
   { "4:3",           1.3333f },
   { "16:10",         1.6f },
   { "16:9",          1.7778f },
   { "2:1",           2.0f },
   { "Auto (Index)",  1.0f },
   { "Auto (Free)",   1.0f },
};

static float overlay_eightway_dpad_slope_high, overlay_eightway_dpad_slope_low;
static float overlay_eightway_abxy_slope_high, overlay_eightway_abxy_slope_low;

/**
 * input_overlay_set_ellipse:
 * @idx                     : pointer index
 * @orientation             : radians clockwise from north, [-Pi/2, Pi/2]
 * @major_px                : major axis in pixels
 * @minor_px                : minor axis in pixels
 *
 * Called by input driver to store touch area vals for eightway-area descs.
 */
void input_overlay_set_ellipse(uint8_t idx,
                               const float orientation,
                               const float major_px,
                               const float minor_px)
{
   if (idx >= MAX_TOUCH)
      return;
   
   ol_ellipse.orientation[idx] = orientation;
   ol_ellipse.major_px[idx] = major_px;
   ol_ellipse.minor_px[idx] = minor_px;
}

/**
 * input_overlay_reset_ellipse:
 * @idx                       : pointer index
 *
 * Called by input driver to indicate no touch area information for @idx.
 */
void input_overlay_reset_ellipse(uint8_t idx)
{
   ol_ellipse.major_px[idx] = 0;
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

   if (ol->block_scale || ol->fullscreen_image || driver_get_ptr()->osk_enable)
      scale = 1.0f;

   ol->scale   = scale;
   ol->scale_w = ol->w * scale;
   ol->scale_h = ol->h * scale;
   ol->scale_x = ol->center_x +
      (ol->x - ol->center_x) * scale;
   ol->scale_y = ol->center_y +
      (ol->y - ol->center_y) * scale;

   for (i = 0; i < ol->size; i++)
   {
      float adj_range_x, adj_range_y, adj_center_x, adj_center_y;
      struct overlay_desc *desc = &ol->descs[i];

      if (!desc)
         continue;

      adj_range_x = ol->scale_w * desc->range_x;
      adj_range_y = ol->scale_h * desc->range_y;

      desc->image_w = 2.0f * adj_range_x;
      desc->image_h = 2.0f * adj_range_y;

      adj_center_x = ol->scale_x + desc->x * ol->scale_w;
      adj_center_y = ol->scale_y + desc->y * ol->scale_h;
      desc->image_x = adj_center_x - adj_range_x;
      desc->image_y = adj_center_y - adj_range_y;
   }
}

static unsigned input_overlay_auto_aspect_index(struct overlay *ol)
{
   size_t i, j;
   float image_aspect, desc_ratio, ol_ratio;
   float avg_delta[OVERLAY_ASPECT_RATIO_AUTO_INDEX];
   float best_delta = 1e9;
   unsigned best_index = 0;
   unsigned num_images;

   if (!ol)
      return 0;
   ol_ratio = ol->w / ol->h;

   for (i = 0; i < OVERLAY_ASPECT_RATIO_AUTO_INDEX; i++)
   {
      avg_delta[i] = 0.0f;
      num_images = 0;
      for (j = 0; j < ol->size; j++)
      {
         struct overlay_desc *desc = &ol->descs[j];
         if (!desc->image.width || !desc->image.height)
            continue;
         num_images++;
         image_aspect = ((float)desc->image.width) / desc->image.height;
         desc_ratio = ol_ratio * (desc->range_x_orig / desc->range_y_orig);
         avg_delta[i] += overlay_aspectratio_lut[i].value * desc_ratio
                         - image_aspect;
      }
      if (num_images)
         avg_delta[i] /= num_images;
   }

   for (i = 0; i < OVERLAY_ASPECT_RATIO_AUTO_INDEX; i++)
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

static float input_overlay_auto_aspect(struct overlay *ol)
{
   size_t i;
   float image_aspect, desc_ratio, ol_ratio;
   float best_aspect = 0.0f;
   unsigned num_images = 0;

   if (!ol)
      return 0.0f;
   ol_ratio = ol->w / ol->h;

   for (i = 0; i < ol->size; i++)
   {
      struct overlay_desc *desc = &ol->descs[i];
      if (!desc->image.width || !desc->image.height)
         continue;
      num_images++;
      image_aspect = ((float)desc->image.width) / desc->image.height;
      desc_ratio = ol_ratio * (desc->range_x_orig / desc->range_y_orig);
      best_aspect += image_aspect / desc_ratio;
   }

   if (num_images)
      best_aspect /= num_images;
   else
      return 1.7778f;

   return best_aspect;
}

/* Get values to adjust the overlay's aspect, re-center it, and then bisect it
 * to a wider display if possible
 */
static void input_overlay_update_aspect_ratio_vals(struct overlay *ol)
{
   unsigned disp_width, disp_height;
   float ol_aspect, bisect_aspect, max_bisect, bisect_w;
   settings_t* settings = config_get_ptr();
   
   /* initialize AR mod vals to defaults */
   ol_ar_mod.w              = 1.0f;
   ol_ar_mod.x_center_shift = 0.0f;
   ol_ar_mod.x_bisect_shift = 0.0f;
   ol_ar_mod.h              = 1.0f;
   ol_ar_mod.y_center_shift = 0.0f;

   video_driver_get_size(&disp_width, &disp_height);
   disp_aspect = (float)disp_width / disp_height;

   if (settings->input.overlay_aspect_ratio_index ==
            OVERLAY_ASPECT_RATIO_AUTO_FREE)
      ol_aspect = input_overlay_auto_aspect(ol);
   else if (settings->input.overlay_aspect_ratio_index ==
            OVERLAY_ASPECT_RATIO_AUTO_INDEX)
      ol_aspect = overlay_aspectratio_lut
                        [input_overlay_auto_aspect_index(ol)].value;
   else
      ol_aspect = overlay_aspectratio_lut
                        [settings->input.overlay_aspect_ratio_index].value;
   
   if (disp_aspect > ol_aspect * 1.001)
   {
      ol_ar_mod.w = ol_aspect / disp_aspect;
      ol_ar_mod.x_center_shift = (1.0f - ol_ar_mod.w) / 2.0f;
   }
   else if (ol_aspect > disp_aspect * 1.001)
   {
      ol_ar_mod.h = disp_aspect / ol_aspect;
      ol_ar_mod.y_center_shift = (1.0f - ol_ar_mod.h) / 2.0f;
   }

   /* adjust for scale to keep bisect aspect setting relative to display */
   bisect_aspect = settings->input.overlay_bisect_aspect_ratio
                   / settings->input.overlay_scale;
   max_bisect = disp_aspect / settings->input.overlay_scale;
   bisect_aspect = min(bisect_aspect, max_bisect);
   if (bisect_aspect > ol_aspect * ol_ar_mod.h)
   {
      bisect_w = bisect_aspect / disp_aspect;
      ol_ar_mod.x_bisect_shift = (bisect_w - ol_ar_mod.w) / 2.0f;
   }
}

static void input_overlay_desc_init_imagebox(struct overlay_desc *desc)
{
   if (!desc)
      return;

   desc->image_x = desc->x - desc->range_x;
   desc->image_w = 2.0f * desc->range_x;
   desc->image_y = desc->y - desc->range_y;
   desc->image_h = 2.0f * desc->range_y;
}

static void input_overlay_desc_init_hitbox(struct overlay_desc *desc)
{
   if (!desc)
      return;

   desc->x_hitbox = ((desc->x + desc->range_x * desc->reach_right)
         + (desc->x - desc->range_x * desc->reach_left)) / 2.0f;

   desc->y_hitbox = ((desc->y + desc->range_y * desc->reach_down)
         + (desc->y - desc->range_y * desc->reach_up)) / 2.0f;

   desc->range_x_hitbox = (desc->range_x * desc->reach_right
         + desc->range_x * desc->reach_left) / 2.0f;

   desc->range_y_hitbox = (desc->range_y * desc->reach_down
         + desc->range_y * desc->reach_up) / 2.0f;
   
   desc->range_x_mod = desc->range_x_hitbox;
   desc->range_y_mod = desc->range_y_hitbox;
}

static void input_overlay_desc_adjust_aspect_and_shift(struct overlay_desc *desc)
{
   settings_t *settings = config_get_ptr();
   float upper_bound = 0.5f + (0.5f * (1.0f / settings->input.overlay_scale));
   float lower_bound = 0.5f - (0.5f * (1.0f / settings->input.overlay_scale));
   
   if (!desc)
      return;
   
   if (settings->input.overlay_adjust_aspect)
   {
      /* adjust aspect */
      desc->x = desc->x_orig * ol_ar_mod.w;
      desc->y = desc->y_orig * ol_ar_mod.h;
      desc->range_x = desc->range_x_orig * ol_ar_mod.w;
      desc->range_y = desc->range_y_orig * ol_ar_mod.h;

      /* re-center and bisect */
      desc->x += ol_ar_mod.x_center_shift;
      if (desc->x > 0.5001f)
         desc->x += ol_ar_mod.x_bisect_shift;
      else if (desc->x < 0.4999f)
         desc->x -= ol_ar_mod.x_bisect_shift;
      desc->y += ol_ar_mod.y_center_shift;
   }
   else
   {
      desc->x = desc->x_orig;
      desc->y = desc->y_orig;
      desc->range_x = desc->range_x_orig;
      desc->range_y = desc->range_y_orig;
   }
   
   /* adjust vertical */
   desc->y -= settings->input.overlay_shift_y;
   
   /* make sure the button isn't pushed off screen */
   if (desc->y + desc->range_y > upper_bound)
      desc->y = upper_bound - desc->range_y;
   else if (desc->y - desc->range_y < lower_bound)
      desc->y = lower_bound + desc->range_y;
   
   /* optionally clamp to edge */
   if (settings->input.overlay_shift_y_lock_edges)
   {
      if (desc->y_orig + desc->range_y_orig > 0.99f)
         desc->y = upper_bound - desc->range_y;
      else if (desc->y_orig - desc->range_y_orig < 0.01f)
         desc->y = lower_bound + desc->range_y;
   }

   /* adjust horizontal */
   desc->x += settings->input.overlay_shift_x;

   /* make sure the button isn't entirely pushed off screen */
   if (desc->x > upper_bound)
      desc->x = upper_bound;
   else if (desc->x < lower_bound)
      desc->x = lower_bound;
}

static void input_overlay_get_slope_limits(
      const unsigned diagonal_sensitivity, float* high_slope, float* low_slope)
{
   /* Update the slope values to be used in eightway_state. */
   
   /* Sensitivity is the ratio (as a percentage) of diagonals to normals.
    * 100% means 8-way symmetry; zero means no diagonals.
    * Convert to fraction of max diagonal angle, 45deg
    */
   float fraction = 2.0f * diagonal_sensitivity
                         / (100.0f + diagonal_sensitivity);
   
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
 * input_overlay_update_eightway_diag_sens:
 *
 * Updates diagonal sensitivity for all eightway_vals
 **/
void input_overlay_update_eightway_diag_sens()
{
   settings_t* settings = config_get_ptr();

   input_overlay_get_slope_limits(settings->input.overlay_dpad_diag_sens,
                                  &overlay_eightway_dpad_slope_high,
                                  &overlay_eightway_dpad_slope_low);
   input_overlay_get_slope_limits(settings->input.overlay_abxy_diag_sens,
                                  &overlay_eightway_abxy_slope_high,
                                  &overlay_eightway_abxy_slope_low);
}

static void input_overlay_desc_populate_eightway_vals(config_file_t *ol_conf,
      struct overlay_desc *desc, unsigned ol_idx, unsigned desc_idx)
{
   struct overlay_eightway_vals* vals;
   char conf_key[64];
   char *str, *tok;
   char *save = NULL;

   desc->eightway_vals = calloc(1, sizeof(struct overlay_eightway_vals));
   vals = desc->eightway_vals;

   if (desc->key_mask == (UINT64_C(1) << RARCH_JOYPAD_DPAD_AREA))
   {
      vals->up = UINT64_C(1)<<RETRO_DEVICE_ID_JOYPAD_UP;
      vals->down = UINT64_C(1)<<RETRO_DEVICE_ID_JOYPAD_DOWN;
      vals->left = UINT64_C(1)<<RETRO_DEVICE_ID_JOYPAD_LEFT;
      vals->right = UINT64_C(1)<<RETRO_DEVICE_ID_JOYPAD_RIGHT;
      vals->p_slope_high = &overlay_eightway_dpad_slope_high;
      vals->p_slope_low = &overlay_eightway_dpad_slope_low;
   }
   else if (desc->key_mask == (UINT64_C(1) << RARCH_JOYPAD_ABXY_AREA))
   {
      vals->up = UINT64_C(1)<<RETRO_DEVICE_ID_JOYPAD_X;
      vals->down = UINT64_C(1)<<RETRO_DEVICE_ID_JOYPAD_B;
      vals->left = UINT64_C(1)<<RETRO_DEVICE_ID_JOYPAD_Y;
      vals->right = UINT64_C(1)<<RETRO_DEVICE_ID_JOYPAD_A;
      vals->p_slope_high = &overlay_eightway_abxy_slope_high;
      vals->p_slope_low = &overlay_eightway_abxy_slope_low;
   }

   /* redefined vals can be specified in config file */
   snprintf(conf_key, sizeof(conf_key), "overlay%u_desc%u_up", ol_idx, desc_idx);
   if (config_get_string(ol_conf, conf_key, &str))
   {
      vals->up = 0;
      for (tok = strtok_r(str, "|", &save); tok; tok = strtok_r(NULL, "|", &save))
         vals->up |= UINT64_C(1) << input_translate_str_to_bind_id(tok);
      free(str);
   }
   
   snprintf(conf_key, sizeof(conf_key), "overlay%u_desc%u_down", ol_idx, desc_idx);
   if (config_get_string(ol_conf, conf_key, &str))
   {
      vals->down = 0;
      for (tok = strtok_r(str, "|", &save); tok; tok = strtok_r(NULL, "|", &save))
         vals->down |= UINT64_C(1) << input_translate_str_to_bind_id(tok);
      free(str);
   }

   snprintf(conf_key, sizeof(conf_key), "overlay%u_desc%u_right", ol_idx, desc_idx);
   if (config_get_string(ol_conf, conf_key, &str))
   {
      vals->right = 0;
      for (tok = strtok_r(str, "|", &save); tok; tok = strtok_r(NULL, "|", &save))
         vals->right |= UINT64_C(1) << input_translate_str_to_bind_id(tok);
      free(str);
   }
   
   snprintf(conf_key, sizeof(conf_key), "overlay%u_desc%u_left", ol_idx, desc_idx);
   if (config_get_string(ol_conf, conf_key, &str))
   {
      vals->left = 0;
      for (tok = strtok_r(str, "|", &save); tok; tok = strtok_r(NULL, "|", &save))
         vals->left |= UINT64_C(1) << input_translate_str_to_bind_id(tok);
      free(str);
   }

   vals->up_left = vals->up | vals->left;
   vals->up_right = vals->up | vals->right;
   vals->down_left = vals->down | vals->left;
   vals->down_right = vals->down | vals->right;
}

static void input_overlay_set_vertex_geom(input_overlay_t *ol)
{
   size_t i;

   if (!ol)
      return;
   if (ol->active->image.pixels)
      ol->iface->vertex_geom(ol->iface_data, 0,
            ol->active->scale_x, ol->active->scale_y,
            ol->active->scale_w, ol->active->scale_h);

   for (i = 0; i < ol->active->size; i++)
   {
      struct overlay_desc *desc = &ol->active->descs[i];

      if (!desc)
         continue;

      if (!desc->image.pixels)
         continue;

      ol->iface->vertex_geom(ol->iface_data, desc->image_index,
            desc->image_x, desc->image_y, desc->image_w, desc->image_h);
   }
}

static void input_overlay_update_aspect_and_shift(struct overlay *ol)
{
   struct overlay_desc* desc;
   size_t i;

   if (!ol || ol->fullscreen_image || driver_get_ptr()->osk_enable)
      return;

   input_overlay_update_aspect_ratio_vals(ol);

   for (i = 0; i < ol->size; i++)
   {
      desc = &ol->descs[i];
      input_overlay_desc_adjust_aspect_and_shift(desc);
      input_overlay_desc_init_imagebox(desc);
      input_overlay_desc_init_hitbox(desc);
   }
}

void input_overlays_update_aspect_shift_scale(input_overlay_t *ol)
{
   float scale = config_get_ptr()->input.overlay_scale;
   size_t i;

   if (!ol || !ol->active)
      return;

   for (i = 0; i < ol->size; i++)
   {
      input_overlay_update_aspect_and_shift(&ol->overlays[i]);
      input_overlay_scale(&ol->overlays[i], scale);
   }

   input_overlay_set_vertex_geom(ol);
}

static void input_overlay_free_overlay(struct overlay *overlay)
{
   size_t i;

   if (!overlay)
      return;

   for (i = 0; i < overlay->size; i++)
   {
      texture_image_free(&overlay->descs[i].image);
      if (overlay->descs[i].eightway_vals)
         free(overlay->descs[i].eightway_vals);
   }

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
   char overlay_desc_image_key[64];
   char image_path[PATH_MAX_LENGTH];

   overlay_desc_image_key[0] = '\0';
   image_path[0] = '\0';

   snprintf(overlay_desc_image_key, sizeof(overlay_desc_image_key),
         "overlay%u_desc%u_overlay", ol_idx, desc_idx);

   if (config_get_path(ol->conf, overlay_desc_image_key,
            image_path, sizeof(image_path)))
   {
      char path[PATH_MAX_LENGTH];
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
   char overlay_desc_key[64];
   char conf_key[64];
   char overlay_desc_normalized_key[64];
   char overlay[256];
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

   if (!normalized && (width == 0 || height == 0))
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
                  desc->key_mask |= UINT64_C(1) << input_translate_str_to_bind_id(tmp);
            }

            if (desc->key_mask & (UINT64_C(1) << RARCH_OVERLAY_NEXT))
            {
               snprintf(conf_key, sizeof(conf_key),
                     "overlay%u_desc%u_next_target", ol_idx, desc_idx);
               config_get_array(ol->conf, conf_key,
                     desc->next_index_name, sizeof(desc->next_index_name));
            }
         }
         break;
   }

   width_mod  = 1.0f;
   height_mod = 1.0f;
   
   if (!normalized)
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
            char overlay_analog_saturate_key[64];

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
         "overlay%u_desc%u_exclusive", ol_idx, desc_idx);
   /* Default meta keys to exclusive */
   desc->exclusive = (desc->key_mask & META_KEY_MASK);
   config_get_bool(ol->conf, conf_key, &desc->exclusive);

   snprintf(conf_key, sizeof(conf_key),
         "overlay%u_desc%u_alpha_mod", ol_idx, desc_idx);
   desc->alpha_mod = alpha_mod;
   config_get_float(ol->conf, conf_key, &desc->alpha_mod);

   snprintf(conf_key, sizeof(conf_key),
         "overlay%u_desc%u_range_mod", ol_idx, desc_idx);
   desc->range_mod = range_mod;
   config_get_float(ol->conf, conf_key, &desc->range_mod);
   
   snprintf(conf_key, sizeof(conf_key),
         "overlay%u_desc%u_range_mod_exclusive", ol_idx, desc_idx);
   desc->range_mod_exclusive = false;
   config_get_bool(ol->conf, conf_key, &desc->range_mod_exclusive);
   
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

   input_overlay_desc_init_imagebox(desc);
   input_overlay_desc_init_hitbox(desc);
   
   snprintf(conf_key, sizeof(conf_key),
         "overlay%u_desc%u_movable", ol_idx, desc_idx);
   desc->movable = false;
   desc->delta_x = 0.0f;
   desc->delta_y = 0.0f;
   config_get_bool(ol->conf, conf_key, &desc->movable);
   
   if ((desc->key_mask & MULTIBUTTON_AREA_MASK) != 0)
      input_overlay_desc_populate_eightway_vals(ol->conf, desc, ol_idx, desc_idx);

   /* show keyboard overlay choice in menu */
   if (desc->key_mask & (UINT64_C(1) << RARCH_OSK))
   {
      ol->has_osk_key = true;
      menu_entries_set_refresh();
   }

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

static void input_overlay_load_active(input_overlay_t *ol)
{
   if (!ol)
      return;

   ol->iface->load(ol->iface_data, ol->active->load_images,
         ol->active->load_images_size);

   input_overlay_set_alpha(ol);
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

   if (ol->resolve_pos == ol->index)
   {  
      ol->active = &ol->overlays[ol->index];
      
      input_overlay_load_active(ol);
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
         input_overlay_update_aspect_and_shift(&ol->overlays[ol->pos]);
         input_overlay_scale(&ol->overlays[ol->pos], ol->deferred.scale_factor);

         if (ol->pos == 0)
            input_overlay_load_overlays_resolve_iterate(ol);
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
      char conf_key[64];
      char overlay_full_screen_key[64];
      char overlay_ptr_key[64];
      struct overlay *overlay = NULL;
      bool            to_cont = ol->pos < ol->size;
      
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
      
      snprintf(overlay_ptr_key, sizeof(overlay_ptr_key),
            "overlay%u_lightgun", ol->pos);
      overlay->lightgun_overlay = false;
      config_get_bool(ol->conf, overlay_ptr_key, &overlay->lightgun_overlay);

      snprintf(overlay_ptr_key, sizeof(overlay_ptr_key),
            "overlay%u_mouse", ol->pos);
      overlay->mouse_overlay = false;
      config_get_bool(ol->conf, overlay_ptr_key, &overlay->mouse_overlay);
      ol->has_mouse |= overlay->mouse_overlay;

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
         char overlay_resolved_path[PATH_MAX_LENGTH];

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
   static int old_port;
   int port, i;
   char msg[64];
   struct retro_controller_info rci;
   bool generic = false;

   if (overlay_lightgun_active)
   {  /* Reconnect previous device */
      if (old_port < global->system.num_ports)
         pretro_set_controller_port_device(
               old_port, settings->input.libretro_device[old_port]);
      overlay_lightgun_active = false;
   }

   if (ol->active->lightgun_overlay)
   {
      /* Search available ports. If a lightgun device is selected, use it. */
      for (port = 0; port < global->system.num_ports; port++)
      {
         if ( (RETRO_DEVICE_MASK & settings->input.libretro_device[port])
                 == RETRO_DEVICE_LIGHTGUN )
         {
            overlay_lightgun_active = true;
            break;
         }
      }

      /* If already connected, just get the device name */
      if (overlay_lightgun_active)
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
               overlay_lightgun_active = true;
               break;
            }
         }
         if (overlay_lightgun_active)
            break;
      }

      if (!overlay_lightgun_active)
      {  /* Fall back to generic lightgun */
         port = 0;
         overlay_lightgun_active = true;
         generic = true;
      }
   }

   if (overlay_lightgun_active)
   {
      old_port = port;

      /* Notify user */
      snprintf(msg, 60, "%s active", generic ? "Lightgun" : rci.types[i].desc);
      rarch_main_msg_queue_push(msg, 2, 180, true);

      /* Set autotrigger if no trigger descriptor found */
      global->overlay_lightgun_autotrigger = true;
      for (i = 0; i < ol->active->size; i++)
      {
         if (ol->active->descs[i].key_mask
             & (UINT64_C(1) << RARCH_LIGHTGUN_TRIGGER))
         {
            global->overlay_lightgun_autotrigger = false;
            break;
         }
      }
   }
}

static void input_overlay_update_mouse_scale()
{
   struct retro_system_av_info* av_info = video_viewport_get_system_av_info();
   float content_aspect, adj_x, adj_y;

   if (av_info)
   {
      const struct retro_game_geometry *geom =
         (const struct retro_game_geometry*)&av_info->geometry;

      content_aspect = (float)geom->base_width / geom->base_height;

      adj_x = disp_aspect > content_aspect ?
         (disp_aspect / content_aspect) : 1.0f;
      adj_y = content_aspect > disp_aspect ?
         (content_aspect / disp_aspect) : 1.0f;

      ol_mouse.scale_x = (adj_x * geom->base_width) / (float)0x7fff;
      ol_mouse.scale_y = (adj_y * geom->base_height) / (float)0x7fff;
   }
}

static void input_overlay_connect_mouse(input_overlay_t *ol)
{
   bool old_mouse_active = overlay_mouse_active;
   overlay_mouse_active = ol->active->mouse_overlay;

   if (overlay_mouse_active && !old_mouse_active)
   {
      ol_mouse.prev_x = 0;
      ol_mouse.prev_y = 0;
      input_overlay_update_mouse_scale();
      rarch_main_msg_queue_push("Mouse active", 2, 60, true);
   }
}

bool input_overlay_new_done(input_overlay_t *ol)
{
   if (!ol)
      return false;

   input_overlay_set_alpha(ol);
   ol->next_index = (ol->index + 1) % ol->size;

   ol->state = OVERLAY_STATUS_ALIVE;

   if (ol->conf)
      config_file_free(ol->conf);
   ol->conf = NULL;

   return true;
}

static bool input_overlay_load_overlays_init(input_overlay_t *ol)
{
   if (!config_get_uint(ol->conf, "overlays", &ol->size))
   {
      RARCH_ERR("overlays variable not defined in config.\n");
      goto error;
   }

   if (!ol->size)
      goto error;

   ol->overlays = (struct overlay*)calloc(ol->size, sizeof(*ol->overlays));
   if (!ol->overlays)
      goto error;

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
 * @path            : Path to overlay file.
 * @enable          : Enable the overlay after initializing it?
 *
 * Creates and initializes an overlay handle.
 *
 * Returns: Overlay handle on success, otherwise NULL.
 **/
input_overlay_t *input_overlay_new(const char *path, bool enable)
{
   input_overlay_t  *ol = (input_overlay_t*)calloc(1, sizeof(*ol));
   driver_t     *driver = driver_get_ptr();
   settings_t *settings = config_get_ptr();

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
   ol->deferred.scale_factor = settings->input.overlay_scale;

   input_overlay_load_overlays_init(ol);
   input_overlay_update_eightway_diag_sens();

   return ol;

error:
   input_overlay_free(ol);
   return NULL;
}

/**
 * input_overlay_load_cached:
 * @ol                      : Cached overlay handle.
 *
 * Loads and enables/disables a cached overlay.
 **/
void input_overlay_load_cached(input_overlay_t *ol, bool enable)
{
   driver_t *driver = driver_get_ptr();
   if (!ol)
      return;

   /* Make video interface current */
   ol->iface_data = driver->video_data;
   video_driver_overlay_interface(&ol->iface);

   /* Load last-used overlay */
   input_overlay_load_active(ol);

   /* Adjust to current settings and enable/disable */
   input_overlays_update_aspect_shift_scale(ol);
   input_overlay_enable(ol, enable);
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

   if (enable)
   {
      input_overlay_connect_lightgun(ol);
      input_overlay_connect_mouse(ol);
   }
}

/**
 * eightway_direction
 * @param vals for an eight-way area descriptor
 * @param x_offset relative to 8-way center, normalized as fraction of screen height
 * @param y_offset relative to 8-way center, normalized as fraction of screen height
 * @return input state representing the offset direction as @vals
 */
static INLINE uint64_t eightway_direction(const struct overlay_eightway_vals* vals,
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
         if (abs_slope > *vals->p_slope_high)
            return vals->up;
         else if (abs_slope < *vals->p_slope_low)
            return vals->right;
         else return vals->up_right;
      }
      else
      { /* Q4 */
         if (abs_slope > *vals->p_slope_high)
            return vals->down;
         else if (abs_slope < *vals->p_slope_low)
            return vals->right;
         else return vals->down_right;
      }
   }
   else
   {
      if (y_offset > 0.0f)
      { /* Q2 */
         if (abs_slope > *vals->p_slope_high)
            return vals->up;
         else if (abs_slope < *vals->p_slope_low)
            return vals->left;
         else return vals->up_left;
      }
      else
      { /* Q3 */
         if (abs_slope > *vals->p_slope_high)
            return vals->down;
         else if (abs_slope < *vals->p_slope_low)
            return vals->left;
         else return vals->down_left;
      }
   }
   
   return UINT64_C(0);
}

static INLINE uint64_t fourway_direction(const struct overlay_eightway_vals* vals,
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
         if (abs_slope < 1.0f)
            return vals->right;
         else return vals->up;
      }
      else
      { /* Q4 */
         if (abs_slope < 1.0f)
            return vals->right;
         else return vals->down;
      }
   }
   else
   {
      if (y_offset > 0.0f)
      { /* Q2 */
         if (abs_slope < 1.0f)
            return vals->left;
         else return vals->up;
      }
      else
      { /* Q3 */
         if (abs_slope < 1.0f)
            return vals->left;
         else return vals->down;
      }
   }
   
   return UINT64_C(0);
}

/**
 * eightway_ellipse_coverage:
 * @param vals for an eight-way area descriptor
 * @param x_ellipse_offset relative to 8-way center, normalized as fraction of screen height
 * @param y_ellipse_offset relative to 8-way center, normalized as fraction of screen height
 * @return the input state representing the @vals covered by ellipse area
 * 
 * Requires the input driver to call input_overlay_set_ellipse during poll.
 * Approximates ellipse as a diamond and checks vertex overlap with @vals.
 */
static INLINE uint64_t eightway_ellipse_coverage(const struct overlay_eightway_vals* vals,
                                                 const uint8_t ptr_idx,
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
   if (ol_ellipse.major_px[ptr_idx] == 0)
      return fourway_direction(vals, x_ellipse_offset, y_ellipse_offset);

   /* hack for inaccurate touchscreens */
   boost = settings->input.touch_ellipse_magnify;

   /* normalize radii by screen height to keep aspect ratio */
   video_driver_get_size(&screen_width, &screen_height);
   radius_major = boost * ol_ellipse.major_px[ptr_idx] / (2*screen_height);
   radius_minor = boost * ol_ellipse.minor_px[ptr_idx] / (2*screen_height);
   
   /* get axis endpoints */
   major_angle = ol_ellipse.orientation[ptr_idx] > 0 ?
      ((float)M_PI/2 - ol_ellipse.orientation[ptr_idx])
      : ((float)(-M_PI)/2 - ol_ellipse.orientation[ptr_idx]);
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
   state |= fourway_direction(vals, x_offset, y_offset);
   
   /* major axis endpoint 2 */
   x_offset = x_ellipse_offset - x_major_offset;
   y_offset = y_ellipse_offset - y_major_offset;
   state |= fourway_direction(vals, x_offset, y_offset);

   /* minor axis endpoint 1 */
   x_offset = x_ellipse_offset + x_minor_offset;
   y_offset = y_ellipse_offset + y_minor_offset;
   state |= fourway_direction(vals, x_offset, y_offset);

   /* minor axis endpoint 2 */
   x_offset = x_ellipse_offset - x_minor_offset;
   y_offset = y_ellipse_offset - y_minor_offset;
   state |= fourway_direction(vals, x_offset, y_offset);
   
   return state;
}

/**
 * eightway_state:
 * @desc_ptr              : Overlay descriptor handle.
 * @type                  : DPAD_AREA or ABXY_AREA
 * @x                     : X coordinate value.
 * @y                     : Y coordinate value.
 *
 * Returns the low level input state based on @x, @y, and ellipse_px values.
 **/
static INLINE uint64_t eightway_state(const struct overlay_desc *desc_ptr,
                                      unsigned area_type,
                                      const uint8_t ptr_idx,
                                      const float x, const float y)
{
   settings_t* settings = config_get_ptr();
   uint64_t state = 0;
   const struct overlay_eightway_vals* vals = desc_ptr->eightway_vals;
   
   float x_offset = (x - desc_ptr->x) * disp_aspect;
   float y_offset = (desc_ptr->y - y);
   unsigned method = area_type < ABXY_AREA ?
         settings->input.overlay_dpad_method : settings->input.overlay_abxy_method;
   
   if (method != TOUCH_AREA)
      state |= eightway_direction(vals, x_offset, y_offset);
   
   if (method != VECTOR)
      state |= eightway_ellipse_coverage(vals, ptr_idx, x_offset, y_offset);
   
   return state;
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
 * input_overlay_poll_descs:
 * @ol                    : Overlay handle.
 * @out                   : Polled output data.
 * @ptr_idx               : Pointer index.
 * @norm_x                : Normalized X coordinate.
 * @norm_y                : Normalized Y coordinate.
 *
 * Polls overlay descriptors for a single input pointer.
 *
 * @norm_x and @norm_y are the result of
 * input_translate_coord_viewport().
 *
 * Returns true if pointer is inside any hitbox.
 **/
static INLINE bool input_overlay_poll_descs(
      input_overlay_t *ol, input_overlay_state_t *out,
      const uint8_t ptr_idx, int16_t norm_x, int16_t norm_y)
{
   size_t i, j;
   float x, y;
   struct overlay_desc *descs = ol->active->descs;
   bool exclusive_desc_hit    = false;
   bool any_desc_hit          = false;

   memset(out, 0, sizeof(*out));

   if (!ol->enable)
   {
      ol->blocked = false;
      return false;
   }
   if (ol->blocked)
      return false;

   /* norm_x and norm_y is in [-0x7fff, 0x7fff] range,
    * like RETRO_DEVICE_POINTER. */
   x = (float)(norm_x + 0x7fff) / 0xffff;
   y = (float)(norm_y + 0x7fff) / 0xffff;

   x -= ol->active->scale_x;
   y -= ol->active->scale_y;
   x /= ol->active->scale_w;
   y /= ol->active->scale_h;

   for (i = 0; i < ol->active->size && !exclusive_desc_hit; i++)
   {
      struct overlay_desc *desc = &descs[i];

      if (!inside_hitbox(desc, x, y))
         continue;

      any_desc_hit = true;

      if (desc->exclusive
          || (desc->range_mod_exclusive &&
              desc->range_x_mod > desc->range_x_hitbox))
      {
         exclusive_desc_hit = true;
         memset(out, 0, sizeof(*out));
         for (j = 0; j < i; j++)
            descs[j].updated &= ~(1 << ptr_idx);
      }

      desc->updated |= (1 << ptr_idx);

      if (desc->type == OVERLAY_TYPE_BUTTONS)
      {
         out->buttons |= desc->key_mask;

         if (desc->key_mask & (UINT64_C(1) << RARCH_JOYPAD_DPAD_AREA))
            out->buttons |= eightway_state(desc, DPAD_AREA, ptr_idx, x, y);
         else if (desc->key_mask & (UINT64_C(1) << RARCH_JOYPAD_ABXY_AREA))
            out->buttons |= eightway_state(desc, ABXY_AREA, ptr_idx, x, y);
         else if (desc->key_mask & (UINT64_C(1) << RARCH_OVERLAY_NEXT))
            ol->next_index = desc->next_index;
      }
      else if (desc->type == OVERLAY_TYPE_KEYBOARD)
      {
         if (desc->key_mask < RETROK_LAST)
            OVERLAY_SET_KEY(out, desc->key_mask);
      }
      else
      {
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
               * ol->active->scale_w;
            desc->delta_y = clamp_float(y_dist, -desc->range_y, desc->range_y)
               * ol->active->scale_h;
         }
      }
   }

   return any_desc_hit;
}

static INLINE void input_overlay_poll_mouse()
{
   driver_t*              driver    = driver_get_ptr();
   settings_t*            settings  = config_get_ptr();
   input_overlay_state_t* state     = &driver->overlay_state;
   input_overlay_state_t* old_state = &driver->old_overlay_state;
   retro_time_t           now_usec  = rarch_get_time_usec();
   uint32_t               hold_zone = settings->input.overlay_mouse_hold_zone;
   retro_time_t           hold_usec = settings->input.overlay_mouse_hold_ms * 1000;
   bool                   can_drag  = settings->input.overlay_mouse_hold_to_drag;
   bool is_swipe, is_brief, is_long;

   static int x_start, y_start, peak_ptr_count;
   static retro_time_t start_usec;
   static bool ignore_new_buttons;

   if (state->ptr_count != old_state->ptr_count)
   {
      if (state->ptr_count)
      {  /* relocate main pointer */
         ol_mouse.prev_x = x_start = state->ptr_x;
         ol_mouse.prev_y = y_start = state->ptr_y;
      }

      if (state->ptr_count > old_state->ptr_count)
      {  /* pointer added */
         peak_ptr_count = max(state->ptr_count, peak_ptr_count);
         start_usec     = now_usec;
      }
      else if (ol_mouse.hold)
         ol_mouse.hold = 0x0;
   }
   else if (now_usec - start_usec > 500000)
      ol_mouse.click = 0x0; /* remove any stale click */

   is_swipe = abs(state->ptr_x - x_start) > hold_zone ||
              abs(state->ptr_y - y_start) > hold_zone;
   is_brief = (now_usec - start_usec) < 200000;
   is_long  = (now_usec - start_usec) > ((can_drag == true) ? hold_usec : 250000);

   if (!is_swipe && !ignore_new_buttons)
   {
      if (is_long && state->ptr_count && can_drag)
      {
         ol_mouse.hold = (1 << (state->ptr_count - 1));
         if (driver->input->overlay_haptic_feedback)
            driver->input->overlay_haptic_feedback();
      }
      else if (is_brief && !state->ptr_count)
         ol_mouse.click = (1 << (peak_ptr_count - 1));
   }

   if (!state->ptr_count)
   {
      ignore_new_buttons = false;
      peak_ptr_count = 0;
   }
   else if (is_long)
      ignore_new_buttons = true;
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
      desc->image_x + desc->delta_x, desc->image_y + desc->delta_y,
      desc->image_w, desc->image_h);

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
static INLINE void input_overlay_post_poll(input_overlay_t *ol)
{
   settings_t *settings = config_get_ptr();
   float opacity        = settings->input.overlay_opacity;
   size_t i;

   if (!ol)
      return;

   input_overlay_set_alpha(ol);

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
      desc->updated = 0;
   }
}

/**
 * input_overlay_poll_clear:
 * @ol                    : overlay handle
 *
 * Call when there is nothing to poll. Allows overlay to
 * clear certain state.
 **/
static INLINE void input_overlay_poll_clear(input_overlay_t *ol)
{
   size_t i;

   if (!ol)
      return;

   ol->blocked = false;

   input_overlay_set_alpha(ol);

   for (i = 0; i < ol->active->size; i++)
   {
      struct overlay_desc *desc = &ol->active->descs[i];

      if (!desc)
         continue;

      desc->range_x_mod = desc->range_x_hitbox;
      desc->range_y_mod = desc->range_y_hitbox;
      desc->updated = 0;

      desc->delta_x = 0.0f;
      desc->delta_y = 0.0f;
      input_overlay_update_desc_geom(ol, desc);
   }
   
   if (overlay_adjust_needed)
   {
      input_overlays_update_aspect_shift_scale(ol);
      input_overlay_update_mouse_scale();
      overlay_adjust_needed = false;
   }
}

/**
 * menu_analog_dpad_state:
 * @analog_x             : x axis value [-0x7fff, 0x7fff]
 * @analog_y             : y axis value [-0x7fff, 0x7fff]
 *
 * Returns 4-way d-pad state from analog axes for menu navigation.
 **/
static INLINE uint64_t menu_analog_dpad_state(const int16_t analog_x,
                                              const int16_t analog_y)
{
   if (abs(analog_x) > 0x2aaa || abs(analog_y) > 0x2aaa) /* 33% deadzone */
      return fourway_direction(&menu_analog_vals, analog_x, -analog_y);
   else
      return 0;
}

/*
 * input_overlay_poll:
 * @overlay_device : pointer to overlay
 *
 * Poll pressed buttons/keys on currently active overlay.
 **/
void input_overlay_poll(input_overlay_t *overlay_device)
{
   unsigned i, j, device, ptr_device;
   uint16_t key_mod                 = 0;
   driver_t *driver                 = driver_get_ptr();
   input_overlay_state_t *state     = &driver->overlay_state;
   input_overlay_state_t *old_state = &driver->old_overlay_state;

   unsigned input_count;
   static unsigned old_input_count = 0;

   if (overlay_device->state != OVERLAY_STATUS_ALIVE)
      return;

   memcpy(old_state, state, sizeof(input_overlay_state_t));
   memset(state, 0, sizeof(input_overlay_state_t));
   if (overlay_lightgun_active || overlay_mouse_active)
   {
      state->ptr_x = old_state->ptr_x;
      state->ptr_y = old_state->ptr_y;
   }

   device = overlay_device->active->full_screen ?
      RARCH_DEVICE_POINTER_SCREEN : RETRO_DEVICE_POINTER;

   for (i = 0;
         input_driver_state(NULL, 0, device, i, RETRO_DEVICE_ID_POINTER_PRESSED);
         i++)
   {
      input_overlay_state_t polled_data;

      int16_t x = input_driver_state(NULL, 0,
            device, i, RETRO_DEVICE_ID_POINTER_X);
      int16_t y = input_driver_state(NULL, 0,
            device, i, RETRO_DEVICE_ID_POINTER_Y);

      if (input_overlay_poll_descs(overlay_device, &polled_data, i, x, y))
      {
         state->buttons |= polled_data.buttons;

         for (j = 0; j < ARRAY_SIZE(state->keys); j++)
            state->keys[j] |= polled_data.keys[j];

         for (j = 0; j < 4; j++)
            if (polled_data.analog[j])
               state->analog[j] = polled_data.analog[j];
      }
      else if ((overlay_lightgun_active || overlay_mouse_active)
                  && !overlay_device->blocked)
      {  /* Assume mouse or lightgun pointer if all descriptors were missed */
         if (!state->ptr_count)
         {
            ptr_device = overlay_lightgun_active ?
                  RETRO_DEVICE_POINTER : RARCH_DEVICE_POINTER_SCREEN;
            state->ptr_x = input_driver_state(NULL, 0, ptr_device, i,
                  RETRO_DEVICE_ID_POINTER_X);
            state->ptr_y = input_driver_state(NULL, 0, ptr_device, i,
                  RETRO_DEVICE_ID_POINTER_Y);
         }

         state->ptr_count++;
      }
   }

   input_count = i;

   if (overlay_mouse_active)
      input_overlay_poll_mouse();

   if (OVERLAY_GET_KEY(state, RETROK_LSHIFT)
       || OVERLAY_GET_KEY(state, RETROK_RSHIFT))
      key_mod |= RETROKMOD_SHIFT;

   if (OVERLAY_GET_KEY(state, RETROK_LCTRL)
       || OVERLAY_GET_KEY(state, RETROK_RCTRL))
      key_mod |= RETROKMOD_CTRL;

   if (OVERLAY_GET_KEY(state, RETROK_LALT)
       || OVERLAY_GET_KEY(state, RETROK_RALT))
      key_mod |= RETROKMOD_ALT;

   if (OVERLAY_GET_KEY(state, RETROK_LMETA)
       || OVERLAY_GET_KEY(state, RETROK_RMETA))
      key_mod |= RETROKMOD_META;

   /* CAPSLOCK SCROLLOCK NUMLOCK */
   for (i = 0; i < ARRAY_SIZE(state->keys); i++)
   {
      if (state->keys[i] != old_state->keys[i])
      {
         uint32_t orig_bits = old_state->keys[i];
         uint32_t new_bits  = state->keys[i];

         for (j = 0; j < 32; j++)
            if ((orig_bits & (1 << j)) != (new_bits & (1 << j)))
            {
               unsigned rk = i * 32 + j;
               uint32_t c = input_keymaps_translate_rk_to_char(rk, key_mod);
               input_keyboard_event(new_bits & (1 << j), rk, c, key_mod);
            }
      }
   }

   /* Map "analog" buttons to analog axes like regular input drivers do. */
   for (j = 0; j < 4; j++)
   {
      unsigned bind_plus  = RARCH_ANALOG_LEFT_X_PLUS + 2 * j;
      unsigned bind_minus = bind_plus + 1;

      if (state->analog[j])
         continue;

      if (state->buttons & (UINT64_C(1) << bind_plus))
         state->analog[j] += 0x7fff;
      if (state->buttons & (UINT64_C(1) << bind_minus))
         state->analog[j] -= 0x7fff;
   }

   if (menu_driver_alive())
      state->buttons |= menu_analog_dpad_state(state->analog[0],
                                               state->analog[1]);

   if (input_count)
      input_overlay_post_poll(overlay_device);
   else
      input_overlay_poll_clear(overlay_device);

   /* haptic feedback on button presses or direction changes */
   if ( driver->input->overlay_haptic_feedback
        && state->buttons != old_state->buttons
        && input_count >= old_input_count
        && !overlay_device->blocked )
   {
      driver->input->overlay_haptic_feedback();
   }

   old_input_count = input_count;
}

static INLINE int16_t overlay_mouse_state(unsigned id)
{
   input_overlay_state_t *state = &driver_get_ptr()->overlay_state;
   int16_t res = 0;

   switch(id)
   {
      case RETRO_DEVICE_ID_MOUSE_X:
         res = (state->ptr_x - ol_mouse.prev_x) * ol_mouse.scale_x;
         ol_mouse.prev_x = state->ptr_x;
         break;
      case RETRO_DEVICE_ID_MOUSE_Y:
         res = (state->ptr_y - ol_mouse.prev_y) * ol_mouse.scale_y;
         ol_mouse.prev_y = state->ptr_y;
         break;
      case RETRO_DEVICE_ID_MOUSE_LEFT:
         res = (ol_mouse.click & 0x1) || (ol_mouse.hold & 0x1);
         ol_mouse.click &= ~0x1;
         break;
      case RETRO_DEVICE_ID_MOUSE_RIGHT:
         res = (ol_mouse.click & 0x2) || (ol_mouse.hold & 0x2);
         ol_mouse.click &= ~0x2;
         break;
      case RETRO_DEVICE_ID_MOUSE_MIDDLE:
         res = (ol_mouse.click & 0x4) || (ol_mouse.hold & 0x4);
         ol_mouse.click &= ~0x4;
         break;
   }

   return res;
}

static INLINE int16_t overlay_lightgun_state(unsigned id)
{
   global_t *global = global_get_ptr();
   driver_t *driver = driver_get_ptr();

   switch(id)
   {
      case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X:
         return driver->overlay_state.ptr_x;
      case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y:
         return driver->overlay_state.ptr_y;
      case RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN:
         if (abs(driver->overlay_state.ptr_y) >= 0x7d6f)
            return 1; /* else fall through */
      case RETRO_DEVICE_ID_LIGHTGUN_RELOAD:
         if (driver->overlay_state.ptr_count > 1)  /* 2nd pointer reloads */
            return 1;
         break;
      case RETRO_DEVICE_ID_LIGHTGUN_AUX_A:
         if (driver->overlay_state.buttons
             & (UINT64_C(1) << RARCH_LIGHTGUN_AUX_A))
            return 1;
         break;
      case RETRO_DEVICE_ID_LIGHTGUN_AUX_B:
         if (driver->overlay_state.buttons
             & (UINT64_C(1) << RARCH_LIGHTGUN_AUX_B))
            return 1;
         break;
      case RETRO_DEVICE_ID_LIGHTGUN_AUX_C:
         if (driver->overlay_state.buttons
             & (UINT64_C(1) << RARCH_LIGHTGUN_AUX_C))
            return 1;
         break;
      case RETRO_DEVICE_ID_LIGHTGUN_TRIGGER:
         if (global->overlay_lightgun_autotrigger)
            return driver->overlay_state.ptr_count == 1;
         if (driver->overlay_state.buttons
             & (UINT64_C(1) << RARCH_LIGHTGUN_TRIGGER))
            return 1;
         break;
      case RETRO_DEVICE_ID_LIGHTGUN_START:
      case RETRO_DEVICE_ID_LIGHTGUN_PAUSE:
         if (driver->overlay_state.buttons
             & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_START))
            return 1;
         break;
      case RETRO_DEVICE_ID_LIGHTGUN_SELECT:
         if (driver->overlay_state.buttons
             & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_SELECT))
            return 1;
         break;
      case RETRO_DEVICE_ID_LIGHTGUN_DPAD_UP:
         if (driver->overlay_state.buttons
             & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_UP))
            return 1;
         break;
      case RETRO_DEVICE_ID_LIGHTGUN_DPAD_DOWN:
         if (driver->overlay_state.buttons
             & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_DOWN))
            return 1;
         break;
      case RETRO_DEVICE_ID_LIGHTGUN_DPAD_LEFT:
         if (driver->overlay_state.buttons
             & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_LEFT))
            return 1;
         break;
      case RETRO_DEVICE_ID_LIGHTGUN_DPAD_RIGHT:
         if (driver->overlay_state.buttons
             & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_RIGHT))
            return 1;
         break;
   }
   return 0;
}

/**
 * input_state:
 * @port                 : user number.
 * @device_base          : base class of user device identifier.
 * @idx                  : index value of user.
 * @id                   : identifier of key pressed by user.
 *
 * Overlay Input state callback function.
 *
 * Returns: Non-zero if the given key (identified by @id) was pressed by the user
 * (assigned to @port).
 **/
int16_t input_overlay_state(unsigned port, unsigned device_base,
                            unsigned idx, unsigned id)
{
   int16_t res      = 0;
   driver_t *driver = driver_get_ptr();

   if (device_base == RETRO_DEVICE_LIGHTGUN && overlay_lightgun_active)
      res = overlay_lightgun_state(id);
   else if (port == 0)
   {
      switch (device_base)
      {
         case RETRO_DEVICE_JOYPAD:
             if (id < RARCH_CUSTOM_BIND_LIST_END &&
                   (driver->overlay_state.buttons & (UINT64_C(1) << id)))
               res = 1;
            break;
         case RETRO_DEVICE_KEYBOARD:
            if (id < RETROK_LAST)
            {
               if (OVERLAY_GET_KEY(&driver->overlay_state, id))
                  res = 1;
            }
            break;
         case RETRO_DEVICE_MOUSE:
            if (overlay_mouse_active)
               res = overlay_mouse_state(id);
            break;
         case RETRO_DEVICE_ANALOG:
            if (idx < 2 && id < 2)  /* axes only */
            {
               unsigned base = 0;

               if (idx == RETRO_DEVICE_INDEX_ANALOG_RIGHT)
                  base = 2;
               if (id == RETRO_DEVICE_ID_ANALOG_Y)
                  base += 1;
               if (driver->overlay_state.analog[base])
                  res = driver->overlay_state.analog[base];
            }
            break;
      }
   }

   return res;
}

/**
 * input_overlay_next:
 * @ol               : Overlay handle.
 *
 * Switch to the next available overlay
 * screen.
 **/
void input_overlay_next(input_overlay_t *ol)
{
   if (!ol)
      return;

   ol->index = ol->next_index;
   ol->active = &ol->overlays[ol->index];

   input_overlay_load_active(ol);

   input_overlay_connect_mouse(ol);
   input_overlay_connect_lightgun(ol);

   ol->blocked = true;
   ol->next_index = (ol->index + 1) % ol->size;
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
 * input_overlay_set_alpha:
 * @ol                    : Overlay handle.
 *
 * Sets the configured opacity for the active overlay.
 **/
void input_overlay_set_alpha(input_overlay_t *ol)
{
   float opacity = config_get_ptr()->input.overlay_opacity;
   unsigned i;

   if (!ol)
      return;

   for (i = 0; i < ol->active->load_images_size; i++)
      ol->iface->set_alpha(ol->iface_data, i, opacity);
}

void input_overlay_notify_video_updated()
{
   overlay_adjust_needed = true;
}
