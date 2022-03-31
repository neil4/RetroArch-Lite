/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#ifndef INPUT_OVERLAY_H__
#define INPUT_OVERLAY_H__

#include <stdint.h>
#include <boolean.h>
#include <libretro.h>
#include <formats/image.h>
#include <retro_miscellaneous.h>
#include <file/config_file.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OVERLAY_GET_KEY(state, key) (((state)->keys[(key) / 32] >> ((key) % 32)) & 1)
#define OVERLAY_SET_KEY(state, key) (state)->keys[(key) / 32] |= 1 << ((key) % 32)
#define OVERLAY_CLEAR_KEY(state, key) (state)->keys[(key) / 32] &= ~(1 << ((key) % 32))

#define OVERLAY_DEFAULT_VIBE -1

/* Overlay driver acts as a medium between input drivers 
 * and video driver.
 *
 * Coordinates are fetched from input driver, and an 
 * overlay with pressable actions are displayed on-screen.
 *
 * This interface requires that the video driver has support 
 * for the overlay interface.
 */

enum overlay_eightway_type
{
   DPAD_AREA = 0,
   ABXY_AREA,
   NUM_EIGHT_WAY_TYPES
};

struct overlay_eightway_vals
{
   uint64_t up;
   uint64_t right;
   uint64_t down;
   uint64_t left;
   uint64_t up_right;
   uint64_t down_right;
   uint64_t down_left;
   uint64_t up_left;
   float* p_slope_high;
   float* p_slope_low;
};

enum overlay_aspect_ratio
{
   OVERLAY_ASPECT_RATIO_1_2 = 0,
   OVERLAY_ASPECT_RATIO_9_16,
   OVERLAY_ASPECT_RATIO_10_16,
   OVERLAY_ASPECT_RATIO_3_4,
   OVERLAY_ASPECT_RATIO_4_3,
   OVERLAY_ASPECT_RATIO_16_10,
   OVERLAY_ASPECT_RATIO_16_9,
   OVERLAY_ASPECT_RATIO_2_1,
   OVERLAY_ASPECT_RATIO_AUTO_INDEX,
   OVERLAY_ASPECT_RATIO_AUTO_FREE,
   
   OVERLAY_ASPECT_RATIO_END
};

struct overlay_aspect_ratio_elem
{
   char name[64];
   float value;
};

extern struct overlay_aspect_ratio_elem overlay_aspectratio_lut[OVERLAY_ASPECT_RATIO_END];

enum eightway_method
{
   VECTOR = 0,
   TOUCH_AREA,
   VECTOR_AND_AREA
};

typedef struct video_overlay_interface
{
   void (*enable)(void *data, bool state);
   bool (*load)(void *data,
         const struct texture_image *images, unsigned num_images);
   void (*tex_geom)(void *data, unsigned image,
         float x, float y, float w, float h);
   void (*vertex_geom)(void *data, unsigned image,
         float x, float y, float w, float h);
   void (*full_screen)(void *data, bool enable);
   void (*set_alpha)(void *data, unsigned image, float mod);
} video_overlay_interface_t;

enum overlay_hitbox
{
   OVERLAY_HITBOX_RADIAL = 0,
   OVERLAY_HITBOX_RECT
};

enum overlay_type
{
   OVERLAY_TYPE_BUTTONS = 0,
   OVERLAY_TYPE_ANALOG_LEFT,
   OVERLAY_TYPE_ANALOG_RIGHT,
   OVERLAY_TYPE_KEYBOARD
};

enum overlay_status
{
   OVERLAY_STATUS_NONE = 0,
   OVERLAY_STATUS_DEFERRED_LOAD,
   OVERLAY_STATUS_DEFERRED_LOADING_IMAGE,
   OVERLAY_STATUS_DEFERRED_LOADING_IMAGE_PROCESS,
   OVERLAY_STATUS_DEFERRED_LOADING,
   OVERLAY_STATUS_DEFERRED_LOADING_RESOLVE,
   OVERLAY_STATUS_DEFERRED_DONE,
   OVERLAY_STATUS_DEFERRED_ERROR,
   OVERLAY_STATUS_ALIVE,
};

enum overlay_image_transfer_status
{
   OVERLAY_IMAGE_TRANSFER_NONE = 0,
   OVERLAY_IMAGE_TRANSFER_BUSY,
   OVERLAY_IMAGE_TRANSFER_DONE,
   OVERLAY_IMAGE_TRANSFER_DESC_IMAGE_ITERATE,
   OVERLAY_IMAGE_TRANSFER_DESC_ITERATE,
   OVERLAY_IMAGE_TRANSFER_DESC_DONE,
   OVERLAY_IMAGE_TRANSFER_ERROR,
};

struct overlay_desc
{
   float x, y;
   float x_orig, y_orig;
   float x_hitbox, y_hitbox;

   float range_x, range_y;  /* for image and analog input */
   float range_x_orig, range_y_orig;

   float reach_right, reach_left, reach_up, reach_down;
   float range_x_hitbox, range_y_hitbox;  /* initial hitbox range */

   float range_mod;
   float range_x_mod, range_y_mod;  /* extended hitbox range */

   enum overlay_hitbox hitbox;
   bool exclusive;            /* disallow other input in hitbox */
   bool range_mod_exclusive;  /* disallow other input in extended hitbox */
   uint16_t updated;

   enum overlay_type type;
   uint64_t key_mask;
   float analog_saturate_pct;

   unsigned next_index;
   char next_index_name[64];

   struct texture_image image;
   unsigned image_index;
   float image_x, image_y, image_w, image_h;
   float alpha_mod;
   float delta_x, delta_y;
   bool movable;

   struct overlay_eightway_vals *eightway_vals;
};

struct overlay
{
   struct overlay_desc *descs;
   size_t size;
   size_t pos;
   unsigned pos_increment;

   struct texture_image image;

   bool block_scale;
   float scale_x, scale_y, scale_w, scale_h;
   float x, y, w, h;
   float scale;
   float center_x, center_y;

   bool full_screen;
   bool fullscreen_image;
   
   bool lightgun_overlay;
   bool mouse_overlay;

   char name[64];

   struct
   {
      struct
      {
         char key[64];
         char path[PATH_MAX_LENGTH];
      } paths;

      struct
      {
         char key[64];
      } names;

      struct
      {
         char array[256];
         char key[64];
      } rect;

      struct
      {
         char key[64];
         unsigned size;
      } descs;

      bool normalized;
      float alpha_mod;
      float range_mod;
   } config;

   struct texture_image *load_images;
   unsigned load_images_size;
};

struct input_overlay
{
   void *iface_data;
   const video_overlay_interface_t *iface;
   bool enable;

   enum overlay_image_transfer_status loading_status;
   bool blocked;

   struct overlay *overlays;
   const struct overlay *active;
   unsigned index;
   unsigned size;
   unsigned pos;
   unsigned resolve_pos;
   unsigned pos_increment;

   unsigned next_index;
   char *overlay_path;
   config_file_t *conf;

   enum overlay_status state;

   bool has_osk_key;

   struct
   {
      bool enable;
      float scale_factor;
   } deferred;
};

typedef struct input_overlay input_overlay_t;

typedef struct input_overlay_state
{
   /* This is a bitmask of (1 << key_bind_id). */
   uint64_t buttons;
   /* Left X, Left Y, Right X, Right Y */
   int16_t analog[4]; 

   /* Mouse and Lightgun */
   int16_t ptr_x, ptr_y;
   uint8_t ptr_count;
   
   uint32_t keys[RETROK_LAST / 32 + 1];
} input_overlay_state_t;

/**
 * input_overlay_new:
 * @path                  : Path to overlay file.
 * @enable                : Enable the overlay after initializing it?
 *
 * Creates and initializes an overlay handle.
 *
 * Returns: Overlay handle on success, otherwise NULL.
 **/
input_overlay_t *input_overlay_new(const char *path, bool enable);

bool input_overlay_load_overlays(input_overlay_t *ol);

bool input_overlay_load_overlays_image_iterate(input_overlay_t *ol);

bool input_overlay_load_overlays_iterate(input_overlay_t *ol);

bool input_overlay_load_overlays_resolve_iterate(input_overlay_t *ol);

bool input_overlay_new_done(input_overlay_t *ol);

/**
 * input_overlay_set_ellipse:
 * @idx                     : pointer index
 * @orientation             : radians clockwise from north, [-Pi/2, Pi/2]
 * @major_px                : major axis in pixels
 * @minor_px                : minor axis in pixels
 *
 * Called by input driver to store touch area vals for eightway-area descs.
 */
void input_overlay_set_ellipse(uint8_t idx, const float orientation,
      const float major_px, const float minor_px);

/**
 * input_overlay_reset_ellipse:
 * @idx                       : pointer index
 *
 * Called by input driver to indicate no touch area information for @idx.
 */
void input_overlay_reset_ellipse(uint8_t idx);

/**
 * input_overlay_free:
 * @ol               : Overlay handle.
 *
 * Frees overlay handle.
 **/
void input_overlay_free(input_overlay_t *ol);

/**
 * input_overlay_enable:
 * @ol                 : Overlay handle.
 * @enable             : Enable or disable the overlay
 *
 * Enable or disable the overlay.
 **/
void input_overlay_enable(input_overlay_t *ol, bool enable);

/**
 * input_overlay_load_cached:
 * @ol                      : Cached overlay handle.
 *
 * Loads and enables/disables a cached overlay.
 **/
void input_overlay_load_cached(input_overlay_t *ol, bool enable);

/*
 * input_overlay_poll:
 * @overlay_device   : pointer to overlay
 *
 * Poll pressed buttons/keys on currently active overlay.
 **/
void input_overlay_poll(input_overlay_t *overlay_device);

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
      unsigned idx, unsigned id);

/**
 * input_overlay_set_alpha:
 * @ol                    : Overlay handle.
 *
 * Sets the configured opacity for the active overlay.
 **/
void input_overlay_set_alpha(input_overlay_t *ol);

/**
 * input_overlays_update_aspect_shift_scale:
 * @ol                                     : Overlay handle.
 *
 * Updates aspect ratio, shifts, and scale for all overlays.
 **/
void input_overlays_update_aspect_shift_scale(input_overlay_t *ol);

/**
 * input_overlay_next:
 * @ol : Overlay handle.
 *
 * Switch to the next available overlay
 * screen.
 **/
void input_overlay_next(input_overlay_t *ol);

/**
 * input_overlay_update_eightway_diag_sens:
 *
 * Convert diagonal sensitivity to slope values for 8way_state functions
 **/
void input_overlay_update_eightway_diag_sens();

void input_overlay_notify_video_updated();

#ifdef __cplusplus
}
#endif

#endif
