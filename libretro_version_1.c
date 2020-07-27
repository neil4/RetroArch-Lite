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

#include <boolean.h>
#include "libretro.h"
#include "dynamic.h"
#include "libretro_version_1.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <retro_inline.h>
#include <ctype.h>
#include <errno.h>
#include "general.h"
#include "runloop.h"
#include "runloop_data.h"
#include "retroarch.h"
#include "performance.h"
#include "input/keyboard_line.h"
#include "input/input_remapping.h"
#include "audio/audio_driver.h"
#include "audio/audio_utils.h"
#include "retroarch_logger.h"
#include "record/record_driver.h"
#include "intl/intl.h"
#include "input/input_common.h"
#include "preempt.h"

#ifdef HAVE_NETPLAY
#include "netplay.h"
#endif

const struct retro_keybind *libretro_input_binds[MAX_USERS];

static bool video_frame_scale(const void *data,
      unsigned width, unsigned height,
      size_t pitch)
{
   driver_t *driver = driver_get_ptr();

   if (!data)
      return false;
   if (video_driver_get_pixel_format() != RETRO_PIXEL_FORMAT_0RGB1555)
      return false;
   if (data == RETRO_HW_FRAME_BUFFER_VALID)
      return false;

   driver->scaler.in_width      = width;
   driver->scaler.in_height     = height;
   driver->scaler.out_width     = width;
   driver->scaler.out_height    = height;
   driver->scaler.in_stride     = pitch;
   driver->scaler.out_stride    = width * sizeof(uint16_t);

   scaler_ctx_scale(&driver->scaler, driver->scaler_out, data);
   
   return true;
}

/**
 * video_frame:
 * @data                 : pointer to data of the video frame.
 * @width                : width of the video frame.
 * @height               : height of the video frame.
 * @pitch                : pitch of the video frame.
 *
 * Video frame render callback function.
 **/
static void video_frame(const void *data, unsigned width,
      unsigned height, size_t pitch)
{
   unsigned output_width  = 0, output_height = 0, output_pitch = 0;
   const char *msg      = NULL;
   driver_t  *driver    = driver_get_ptr();
   global_t  *global    = global_get_ptr();
   settings_t *settings = config_get_ptr();

   if (!driver->video_active)
      return;

   if (video_frame_scale(data, width, height, pitch))
   {
      data                        = driver->scaler_out;
      pitch                       = driver->scaler.out_stride;
   }

   video_driver_cached_frame_set(data, width, height, pitch);

   /* Slightly messy code,
    * but we really need to do processing before blocking on VSync
    * for best possible scheduling.
    */
   if ((!video_driver_frame_filter_alive()
            || !settings->video.post_filter_record || !data
            || global->record.gpu_buffer)
      )
      recording_dump_frame(data, width, height, pitch);

   msg                = rarch_main_msg_queue_pull();

   *driver->current_msg = 0;

   if (msg)
      strlcpy(driver->current_msg, msg, sizeof(driver->current_msg));

   if (video_driver_frame_filter(data, width, height, pitch,
            &output_width, &output_height, &output_pitch))
   {
      data   = video_driver_frame_filter_get_buf_ptr();
      width  = output_width;
      height = output_height;
      pitch  = output_pitch;
   }

   if (!video_driver_frame(data, width, height, pitch, driver->current_msg))
      driver->video_active = false;
}

/**
 * input_state:
 * @port                 : user number.
 * @device               : device identifier of user.
 * @idx                  : index value of user.
 * @id                   : identifier of key pressed by user.
 *
 * Input state callback function.
 *
 * Returns: Non-zero if the given key (identified by @id) was pressed by the user
 * (assigned to @port).
 **/
static int16_t input_state(unsigned port, unsigned device,
      unsigned idx, unsigned id)
{
   int16_t res                    = 0;
   settings_t *settings           = config_get_ptr();
   driver_t *driver               = driver_get_ptr();
   global_t *global               = global_get_ptr();
   
   /* Enforce max_users unless using a lightgun, which could be on another port */
   if (port >= settings->input.max_users
#ifdef HAVE_OVERLAY
      && !input_overlay_lightgun_active()
#endif
      ) return 0;

   device &= RETRO_DEVICE_MASK;

   if (global->bsv.movie && global->bsv.movie_playback)
   {
      int16_t ret;
      if (bsv_movie_get_input(global->bsv.movie, &ret))
         return ret;

      global->bsv.movie_end = true;
   }
   
   if (id == RETRO_DEVICE_ID_JOYPAD_MASK
       && device == RETRO_DEVICE_JOYPAD)
   {
      unsigned i;

      for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
         if (input_state(port, device, idx, i))
            res |= (1 << i);
      return res;
   }

   if (settings->input.remap_binds_enable)
      input_remapping_state(port, &device, &idx, &id);

   if (!driver->block_libretro_input)
   {
      if (id < RARCH_CUSTOM_BIND_LIST_END || device == RETRO_DEVICE_KEYBOARD)
         res = input_driver_state(libretro_input_binds, port, device, idx, id);

#ifdef HAVE_OVERLAY
      if (settings->input.overlay_enable)
      {
         if (port == 0)
         {
            switch (device)
            {
               case RETRO_DEVICE_JOYPAD:
                  if (driver->overlay_state.buttons & (UINT64_C(1) << id))
                     res |= 1;
                  break;
               case RETRO_DEVICE_KEYBOARD:
                  if (id < RETROK_LAST)
                  {
                     if (OVERLAY_GET_KEY(&driver->overlay_state, id))
                        res |= 1;
                  }
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
         if (device == RETRO_DEVICE_LIGHTGUN && input_overlay_lightgun_active())
         {
            switch(id)
            {
               case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X:
                  res = driver->overlay_state.lightgun_x;
                  break;
               case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y:
                  res = driver->overlay_state.lightgun_y;
                  break;
               case RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN:
               case RETRO_DEVICE_ID_LIGHTGUN_RELOAD:
                  if (driver->overlay_state.buttons
                      & (UINT64_C(1) << RARCH_LIGHTGUN_RELOAD))
                     res = true;
                  break;
               case RETRO_DEVICE_ID_LIGHTGUN_AUX_A:
                  if (driver->overlay_state.buttons
                      & (UINT64_C(1) << RARCH_LIGHTGUN_AUX_A))
                     res = true;
                  break;
               case RETRO_DEVICE_ID_LIGHTGUN_AUX_B:
                  if (driver->overlay_state.buttons
                      & (UINT64_C(1) << RARCH_LIGHTGUN_AUX_B))
                     res = true;
                  break;
               case RETRO_DEVICE_ID_LIGHTGUN_AUX_C:
                  if (driver->overlay_state.buttons
                      & (UINT64_C(1) << RARCH_LIGHTGUN_AUX_C))
                     res = true;
                  break;
               case RETRO_DEVICE_ID_LIGHTGUN_TRIGGER:
                  if (global->overlay_lightgun_autotrigger)
                     res = driver->overlay_state.lightgun_ptr_active;
                  else if (driver->overlay_state.buttons
                           & (UINT64_C(1) << RARCH_LIGHTGUN_TRIGGER))
                     res = true;
                  break;
               case RETRO_DEVICE_ID_LIGHTGUN_START:
               case RETRO_DEVICE_ID_LIGHTGUN_PAUSE:
                  if (driver->overlay_state.buttons
                      & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_START))
                     res = true;
                  break;
               case RETRO_DEVICE_ID_LIGHTGUN_SELECT:
                  if (driver->overlay_state.buttons
                      & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_SELECT))
                     res = true;
                  break;
               case RETRO_DEVICE_ID_LIGHTGUN_DPAD_UP:
                  if (driver->overlay_state.buttons
                      & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_UP))
                     res = true;
                  break;
               case RETRO_DEVICE_ID_LIGHTGUN_DPAD_DOWN:
                  if (driver->overlay_state.buttons
                      & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_DOWN))
                     res = true;
                  break;
               case RETRO_DEVICE_ID_LIGHTGUN_DPAD_LEFT:
                  if (driver->overlay_state.buttons
                      & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_LEFT))
                     res = true;
                  break;
               case RETRO_DEVICE_ID_LIGHTGUN_DPAD_RIGHT:
                  if (driver->overlay_state.buttons
                      & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_RIGHT))
                     res = true;
                  break;
            }
         }
      }
#endif
   }

   /* flushing_input will be cleared in rarch_main_iterate. */
   if (driver->flushing_input)
      res = 0;

   if (global->bsv.movie && !global->bsv.movie_playback)
      bsv_movie_set_input(global->bsv.movie, res);

   return res;
}


#ifdef HAVE_OVERLAY
/*
 * input_poll_overlay:
 * @overlay_device : pointer to overlay 
 *
 * Poll pressed buttons/keys on currently active overlay.
 **/
static INLINE void input_poll_overlay(input_overlay_t *overlay_device, float opacity)
{
   unsigned i, j, device;
   uint16_t key_mod                 = 0;
   driver_t *driver                 = driver_get_ptr();
   settings_t *settings             = config_get_ptr();
   input_overlay_state_t *state     = &driver->overlay_state;
   input_overlay_state_t *old_state = &driver->old_overlay_state;
   
   unsigned ptr_count;
   static unsigned old_ptr_count = 0;

   if (overlay_device->state != OVERLAY_STATUS_ALIVE)
      return;

   memcpy(old_state, state, sizeof(input_overlay_state_t));
   memset(state, 0, sizeof(input_overlay_state_t));
   if (input_overlay_lightgun_active())
   {
      state->lightgun_x = old_state->lightgun_x;
      state->lightgun_y = old_state->lightgun_y;
   }

   device = input_overlay_full_screen(overlay_device) ?
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

      set_overlay_pointer_index(i);
      input_overlay_poll(overlay_device, &polled_data, x, y);

      state->buttons |= polled_data.buttons;

      if (input_overlay_lightgun_active() && polled_data.buttons == 0ULL
          && !overlay_device->blocked)
      {
         /* Assume this is the lightgun pointer if all buttons were missed */
         if (!state->lightgun_ptr_active)
         {
            state->lightgun_x
               = input_driver_state(NULL, 0, RETRO_DEVICE_POINTER, i,
                                    RETRO_DEVICE_ID_POINTER_X);
            state->lightgun_y
               = input_driver_state(NULL, 0, RETRO_DEVICE_POINTER, i,
                                    RETRO_DEVICE_ID_POINTER_Y);
            state->lightgun_ptr_active = true;
         }
         else /* 2nd lightgun pointer reloads */
         {
            state->buttons |= (1ULL << RARCH_LIGHTGUN_RELOAD);
            /* suppress haptic feedback */
            old_state->buttons |= (1ULL << RARCH_LIGHTGUN_RELOAD);
         }
      }

      for (j = 0; j < ARRAY_SIZE(state->keys); j++)
         state->keys[j] |= polled_data.keys[j];

      /* Fingers pressed later take prio and matched up
       * with overlay poll priorities. */
      for (j = 0; j < 4; j++)
         if (polled_data.analog[j])
            state->analog[j] = polled_data.analog[j];
   }
   ptr_count = i;

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
               input_keyboard_event(new_bits & (1 << j),
                     i * 32 + j, 0, key_mod, RETRO_DEVICE_POINTER);
      }
   }

   /* Map "analog" buttons to analog axes like regular input drivers do. */
   for (j = 0; j < 4; j++)
   {
      unsigned bind_plus  = RARCH_ANALOG_LEFT_X_PLUS + 2 * j;
      unsigned bind_minus = bind_plus + 1;

      if (state->analog[j])
         continue;

      if (state->buttons & (1ULL << bind_plus))
         state->analog[j] += 0x7fff;
      if (state->buttons & (1ULL << bind_minus))
         state->analog[j] -= 0x7fff;
   }

   /* Check for analog_dpad_mode.
    * Map analogs to d-pad buttons when configured. */
   switch (settings->input.analog_dpad_mode[0])
   {
      case ANALOG_DPAD_LSTICK:
      case ANALOG_DPAD_RSTICK:
      {
         float analog_x, analog_y;
         unsigned analog_base = 2;

         if (settings->input.analog_dpad_mode[0] == ANALOG_DPAD_LSTICK)
            analog_base = 0;

         analog_x = (float)state->analog[analog_base + 0] / 0x7fff;
         analog_y = (float)state->analog[analog_base + 1] / 0x7fff;

         if (analog_x <= -settings->input.axis_threshold)
            state->buttons |= (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT);
         if (analog_x >=  settings->input.axis_threshold)
            state->buttons |= (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT);
         if (analog_y <= -settings->input.axis_threshold)
            state->buttons |= (1ULL << RETRO_DEVICE_ID_JOYPAD_UP);
         if (analog_y >=  settings->input.axis_threshold)
            state->buttons |= (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN);
         break;
      }

      default:
         if (menu_driver_alive())
         {
            state->buttons |= menu_analog_dpad_state(state->analog[0],
                                                     state->analog[1]);
         }
         break;
   }

   if (ptr_count)
      input_overlay_post_poll(overlay_device, opacity);
   else
      input_overlay_poll_clear(overlay_device, opacity);

   /* haptic feedback on button presses or direction changes */
   if ( driver->input->overlay_haptic_feedback
        && ptr_count >= old_ptr_count
        && state->buttons != old_state->buttons
        && !overlay_device->blocked )
   {
      driver->input->overlay_haptic_feedback();
   }
   
   old_ptr_count = ptr_count;
}
#endif

/**
 * input_poll:
 *
 * Input polling callback function.
 **/
static void input_poll(void)
{
   driver_t *driver               = driver_get_ptr();
   settings_t *settings           = config_get_ptr();

   input_driver_poll();

#ifdef HAVE_OVERLAY
   if (driver->overlay)
      input_poll_overlay(driver->overlay,
            settings->input.overlay_opacity);
#endif

#ifdef HAVE_COMMAND
   if (driver->command)
      rarch_cmd_poll(driver->command);
#endif
}

/**
 * retro_set_default_callbacks:
 * @data           : pointer to retro_callbacks object
 *
 * Binds the libretro callbacks to default callback functions.
 **/
void retro_set_default_callbacks(void *data)
{
   struct retro_callbacks *cbs = (struct retro_callbacks*)data;

   if (!cbs)
      return;

   cbs->frame_cb        = video_frame;
   cbs->sample_cb       = audio_driver_sample;
   cbs->sample_batch_cb = audio_driver_sample_batch;
   cbs->state_cb        = input_state;
   cbs->poll_cb         = input_poll;
}

/**
 * retro_init_libretro_cbs:
 * @data           : pointer to retro_callbacks object
 *
 * Initializes libretro callbacks, and binds the libretro callbacks 
 * to default callback functions.
 **/
void retro_init_libretro_cbs(void *data)
{
   struct retro_callbacks *cbs = (struct retro_callbacks*)data;
   driver_t *driver = driver_get_ptr();
   global_t *global = global_get_ptr();
   settings_t *settings = config_get_ptr();
   int i;

   if (!cbs)
      return;

   (void)driver;
   (void)global;

   pretro_set_video_refresh(video_frame);
   pretro_set_audio_sample(audio_driver_sample);
   pretro_set_audio_sample_batch(audio_driver_sample_batch);
   pretro_set_input_state(input_state);
   pretro_set_input_poll(input_poll);

   retro_set_default_callbacks(cbs);

   for (i = 0; i < MAX_USERS; i++)
      libretro_input_binds[i] = settings->input.binds[i];

#ifdef HAVE_NETPLAY
   if (driver->netplay_data)
   {
      pretro_set_video_refresh(video_frame_net);
      pretro_set_audio_sample(audio_sample_net);
      pretro_set_audio_sample_batch(audio_sample_batch_net);
      pretro_set_input_state(input_state_net);
   }
   else
#endif
   if (driver->preempt_data)
   {
      pretro_set_video_refresh(video_frame_preempt);
      pretro_set_audio_sample(audio_sample_preempt);
      pretro_set_audio_sample_batch(audio_sample_batch_preempt);
      pretro_set_input_poll(input_poll_preempt);
      pretro_set_input_state(input_state_preempt);
   }
}

/**
 * retro_set_rewind_callbacks:
 *
 * Sets the audio sampling callbacks based on whether or not
 * rewinding is currently activated.
 **/
void retro_set_rewind_callbacks(void)
{
   global_t *global = global_get_ptr();

   if (global->rewind.frame_is_reverse)
   {
      pretro_set_audio_sample(audio_driver_sample_rewind);
      pretro_set_audio_sample_batch(audio_driver_sample_batch_rewind);
   }
   else
   {
      pretro_set_audio_sample(audio_driver_sample);
      pretro_set_audio_sample_batch(audio_driver_sample_batch);
   }
}
