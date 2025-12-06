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

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define OVERLAY_MAX_TOUCH 16
#define OVERLAY_MAX_BISECT 4.0f

#define OVERLAY_GET_KEY(state, key) (((state)->keys[(key) / 32] >> ((key) % 32)) & 1)
#define OVERLAY_SET_KEY(state, key) (state)->keys[(key) / 32] |= 1 << ((key) % 32)
#define OVERLAY_CLEAR_KEY(state, key) (state)->keys[(key) / 32] &= ~(1 << ((key) % 32))

#define OVERLAY_DEFAULT_VIBE -1
#define LIGHTGUN_TRIG_MAX_DELAY 15

/* Overlay driver acts as a medium between input drivers 
 * and video driver.
 *
 * Coordinates are fetched from input driver, and an 
 * overlay with pressable actions are displayed on-screen.
 *
 * This interface requires that the video driver has support 
 * for the overlay interface.
 */

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
   OVERLAY_ASPECT_RATIO_AUTO_CONFIG,
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

enum overlay_lightgun_action_type
{
   OVERLAY_LIGHTGUN_ACTION_TRIGGER = 0,
   OVERLAY_LIGHTGUN_ACTION_AUX_A,
   OVERLAY_LIGHTGUN_ACTION_AUX_B,
   OVERLAY_LIGHTGUN_ACTION_AUX_C,
   OVERLAY_LIGHTGUN_ACTION_RELOAD,
   OVERLAY_LIGHTGUN_ACTION_NONE,

   OVERLAY_LIGHTGUN_ACTION_END
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
   OVERLAY_HITBOX_RECT,
   OVERLAY_HITBOX_NONE
};

enum overlay_type
{
   OVERLAY_TYPE_BUTTONS = 0,
   OVERLAY_TYPE_ANALOG_LEFT,
   OVERLAY_TYPE_ANALOG_RIGHT,
   OVERLAY_TYPE_DPAD_AREA,
   OVERLAY_TYPE_ABXY_AREA,
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

   /* Range for image and analog input */
   float range_x, range_y;
   float range_x_orig, range_y_orig;

   /* These stretch range_x/y to create range_x/y_hitbox */
   float reach_right, reach_left, reach_up, reach_down;
   float range_x_hitbox, range_y_hitbox;

   /* Multiplies range_x/y_hitbox to create range_x/y_mod */
   float range_mod;
   float range_x_mod, range_y_mod;

   enum overlay_hitbox hitbox;
   /* If true, blocks input from overlapped hitboxes */
   bool exclusive;
   /* Similar, but only applies after range_mod takes effect */
   bool range_mod_exclusive;

   /* Nonzero if pressed. One bit per input pointer */
   uint16_t touch_mask;
   uint16_t old_touch_mask;

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
   struct overlay_desc *anchor;
};

struct overlay
{
   struct overlay_desc *descs;
   size_t size;
   size_t pos;

   struct texture_image image;

   bool block_scale;
   float scale_x, scale_y, scale_w, scale_h;
   float x, y, w, h;
   float scale;
   float center_x, center_y;

   bool full_screen;
   bool is_lightgun;

   char name[64];

   struct
   {
      bool normalized;
      float alpha_mod;
      float range_mod;
      float aspect_ratio;
   } config;

   struct texture_image *load_images;
   unsigned load_images_size;
};

struct input_overlay
{
   void *iface_data;
   const video_overlay_interface_t *iface;

   enum overlay_image_transfer_status loading_status;
   bool blocked;

   struct overlay *overlays;
   const struct overlay *active;
   unsigned index;
   unsigned size;
   unsigned pos;
   unsigned resolve_pos;

   unsigned next_index;
   char *path;
   config_file_t *conf;

   /* List of unique image paths and texture_image pointers.
    * Must be freed before overlays. */
   struct string_list *image_list;

   enum overlay_status state;

   bool has_osk_key;
   bool has_lightgun;
   bool has_movable_analog;

   struct
   {
      const struct overlay *active;
      float scale_factor;
   } deferred;

#ifdef HAVE_THREADS
   sthread_t *loader_thread;
   scond_t *loader_cond;
   slock_t *loader_mutex;
   void (*load_func)(void *);
   bool is_loading;
   bool loader_busy;
#endif
};

typedef struct input_overlay input_overlay_t;

/* Input state from overlay elements */
typedef struct input_overlay_state
{
   /* This is a bitmask of (1 << key_bind_id). */
   uint64_t buttons;
   /* Left X, Left Y, Right X, Right Y */
   int16_t analog[4]; 
   
   uint32_t keys[RETROK_LAST / 32 + 1];

   /* Raw touch pointers used for overlay input */
   struct
   {
      int16_t x;
      int16_t y;
   } touch[OVERLAY_MAX_TOUCH];
   int touch_count;
} input_overlay_state_t;

/* Truncated overlay state for hitbox polling only */
typedef struct input_overlay_button_state
{
   uint64_t buttons;
   int16_t analog[4];

   uint32_t keys[RETROK_LAST / 32 + 1];
} input_overlay_button_state_t;

/* Non-hitbox input state for pointer, mouse, and lightgun */
typedef struct input_overlay_pointer_state
{
   /* Viewport pointers */
   struct
   {
      int16_t x;
      int16_t y;
   } ptr[OVERLAY_MAX_TOUCH];

   uint8_t count;

   /* Mask of requested devices */
   uint8_t device_mask;

   struct ol_lightgun_st
   {
      unsigned multitouch_id;
      bool autotrigger;
   } lightgun;

   struct ol_mouse_st
   {
      float scale_x;
      float scale_y;

      /* Fullscreen pointer */
      int16_t x;
      int16_t y;
      int16_t prev_x;
      int16_t prev_y;

      int16_t swipe_thres_x;
      int16_t swipe_thres_y;

      uint8_t click;
      uint8_t hold;
   } mouse;
} input_overlay_pointer_state_t;

/**
 * input_overlay_new:
 * @path                  : Path to overlay file.
 *
 * Creates and initializes an overlay handle.
 *
 * Returns: Overlay handle on success, otherwise NULL.
 **/
input_overlay_t *input_overlay_new(const char *path);

void input_overlay_load_overlays(void *ol);

void input_overlay_load_overlays_iterate(void *ol);

void input_overlay_load_overlays_resolve_iterate(void *ol);

void input_overlay_load_active(input_overlay_t *ol);

bool input_overlay_new_done(input_overlay_t *ol);

void input_overlay_loader_iterate(input_overlay_t *ol,
      void (*load_func)(void *));

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
 * @device_class         : base class of user device identifier.
 * @idx                  : index value of user.
 * @id                   : pointer to identifier of key pressed by user.
 *
 * Overlay Input state callback function. Sets @id to NO_BTN if overlay
 * input should override lower level input.
 *
 * Returns: Non-zero if the given key (identified by @id) was pressed by the user
 * (assigned to @port).
 **/
int16_t input_overlay_state(unsigned port, unsigned device_base,
      unsigned idx, unsigned *id);

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
void input_overlay_update_eightway_diag_sens(void);

void input_overlay_notify_video_updated(void);

void input_overlay_update_mouse_scale(void);

#ifdef __cplusplus
}
#endif

#endif
