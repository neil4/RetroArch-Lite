/*  RetroArch - A frontend for libretro.
 *  2019 - Neil Fore
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


/* Preemptive Frames is meant to be a battery friendly substitute for Run-Ahead.
 *
 * Internally replays recent frames with updated input to hide latency.
 * For efficiency, only digital joypad updates will trigger replays.
 */

#include "dynamic.h"
#include "runloop.h"
#include "preempt.h"

#define PREEMPT_NEXT_PTR(x) ((x + 1) % preempt->frames)

struct preempt
{
   struct retro_callbacks cbs;

   void* buffer[MAX_PREEMPT_FRAMES];
   size_t frames;
   size_t state_size;
   
   /* Last-used joypad state. Replays are triggered when this changes. */
   uint16_t joypad_state[MAX_USERS];

   /* Pointer to where replays will start */
   size_t start_ptr;
   /* Pointer to current replay frame */
   size_t replay_ptr;

   bool in_replay;
};

/**
 * preempt_skip_av:
 * @preempt       : pointer to preempt object
 * 
 * Returns: true if audio & video will be skipped this frame, false if not
 **/
bool preempt_skip_av(preempt_t *preempt)
{
   if (!preempt)
      return false;
   return preempt->in_replay;
}

void input_poll_preempt(void)
{
   /* no-op. Polling is done in input_poll_preframe */
}

static void input_poll_preframe(void)
{
   driver_t *driver     = driver_get_ptr();
   preempt_t *preempt   = (preempt_t*)driver->preempt_data;
   settings_t *settings = config_get_ptr();
   uint16_t new_joypad_state;
   unsigned i;

   preempt->cbs.poll_cb();
   
   /* Gather joypad states */
   for (i = 0; i < settings->input.max_users; i++)
   {
      new_joypad_state = preempt->cbs.state_cb
            (i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK);

      if (new_joypad_state != preempt->joypad_state[i])
      {  /* Input is dirty; trigger replays */
         preempt->in_replay = true;
         preempt->joypad_state[i] = new_joypad_state;
      }
   }
}

int16_t input_state_preempt(unsigned port, unsigned device,
      unsigned idx, unsigned id)
{
   driver_t *driver = driver_get_ptr();
   preempt_t *preempt = (preempt_t*)driver->preempt_data;
   
   if (device == RETRO_DEVICE_JOYPAD)
   {
      if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
         return preempt->joypad_state[port];
      else
         return preempt->joypad_state[port] & (1 << id);
   }
   
   return preempt->cbs.state_cb(port, device, idx, id);
}

void video_frame_preempt(const void *data, unsigned width,
      unsigned height, size_t pitch)
{
   driver_t *driver = driver_get_ptr();
   preempt_t *preempt = (preempt_t*)driver->preempt_data;
   
   if (!preempt->in_replay)
      preempt->cbs.frame_cb(data, width, height, pitch);
}

void audio_sample_preempt(int16_t left, int16_t right)
{
   driver_t *driver = driver_get_ptr();
   preempt_t *preempt = (preempt_t*)driver->preempt_data;
   
   if (!preempt->in_replay)
      preempt->cbs.sample_cb(left, right);
}

size_t audio_sample_batch_preempt(const int16_t *data, size_t frames)
{
   driver_t *driver = driver_get_ptr();
   preempt_t *preempt = (preempt_t*)driver->preempt_data;
   
   if (!preempt->in_replay)
      return preempt->cbs.sample_batch_cb(data, frames);
   
   return frames;
}

static bool preempt_init_buffer(preempt_t *preempt)
{
   unsigned i;

   if (!preempt)
      return false;

   preempt->state_size = pretro_serialize_size();

   for (i = 0; i < preempt->frames; i++)
   {
      preempt->buffer[i] = malloc(preempt->state_size);
      if (!preempt->buffer[i])
      {
         RARCH_WARN("Failed to allocate memory for Preemptive Frames.\n");
         rarch_main_msg_queue_push("Failed to allocate memory for "
                                   "Preemptive Frames.", 0, 180, false);
         return false;
      }
   }

   preempt_reset_buffer(preempt);

   return true;
}

/**
 * preempt_free:
 * @preempt    : pointer to preempt object
 *
 * Frees preempt handle.
 **/
static void preempt_free(preempt_t *preempt)
{
   unsigned i;

   for (i = 0; i < preempt->frames; i++)
      free(preempt->buffer[i]);

   free(preempt);
}

/**
 * preempt_new:
 *
 * Returns: new preempt handle.
 **/
static preempt_t *preempt_new()
{
   settings_t *settings = config_get_ptr();
   preempt_t  *preempt  = (preempt_t*)calloc(1, sizeof(*preempt));
   if (!preempt)
      return NULL;
   
   preempt->frames = settings->preempt_frames;

   if (!preempt_init_buffer(preempt))
   {
      preempt_free(preempt);
      preempt = NULL;
   }

   return preempt;
}

static void preempt_update_serialize_size(preempt_t *preempt)
{
   size_t i;
   for (i = 0; i < preempt->frames; i++)
      free(preempt->buffer[i]);

   if (!preempt_init_buffer(preempt))
   {
      deinit_preempt();
      return;
   }

   preempt->in_replay = false;
}

/**
 * preempt_pre_frame:
 * @preempt         : pointer to preempt object
 *
 * Pre-frame for preempt.
 * Call this before running retro_run().
 **/
void preempt_pre_frame(preempt_t *preempt)
{
   input_poll_preframe();
   
   if (preempt->in_replay)
   {
      if (preempt->state_size < pretro_serialize_size())
      {
         preempt_update_serialize_size(preempt);
         return;
      }
      pretro_unserialize(preempt->buffer[preempt->start_ptr],
                         preempt->state_size);
      pretro_run();
      preempt->replay_ptr = PREEMPT_NEXT_PTR(preempt->start_ptr);

      while (preempt->replay_ptr != preempt->start_ptr)
      {
         pretro_serialize(preempt->buffer[preempt->replay_ptr],
                          preempt->state_size);
         pretro_run();
         preempt->replay_ptr = PREEMPT_NEXT_PTR(preempt->replay_ptr);
      }
      preempt->in_replay = false;
   }
   
   /* Save current state, and update start_ptr to point to oldest state. */
   pretro_serialize(preempt->buffer[preempt->start_ptr],
                    preempt->state_size);
   preempt->start_ptr = PREEMPT_NEXT_PTR(preempt->start_ptr);
}

void deinit_preempt(void)
{
   driver_t  *driver  = driver_get_ptr();
   preempt_t *preempt = (preempt_t*)driver->preempt_data;
   
   if (preempt)
   {
      preempt_free(preempt);
      driver->preempt_data = NULL;
      retro_init_libretro_cbs(&driver->retro_ctx);
   }
}

/**
 * init_preempt:
 *
 * Creates buffers and sets callbacks. Skips if preempt_frames == 0.
 *
 * Returns: true on success, false on failure
 **/
bool init_preempt(void)
{
   driver_t   *driver   = driver_get_ptr();
   settings_t *settings = config_get_ptr();
   global_t   *global   = global_get_ptr();
   preempt_t  *preempt  = NULL;
   
   if (settings->preempt_frames == 0 || !global->content_is_init)
      return false;
   
   if (driver->netplay_data)
   {  /* Netplay overrides the same libretro callbacks and takes priority. */
      RARCH_WARN("Cannot use Preemptive Frames during Netplay.\n");
      return false;
   }
   
   if (pretro_serialize_size() == 0)
   {
      RARCH_WARN("Preemptive Frames init failed. "
            "Core does not support savestates.\n");
      rarch_main_msg_queue_push("Preemptive Frames init failed.\n"
            "Core does not support savestates.", 0, 180, false);
      return false;
   }

   RARCH_LOG("Initializing Preemptive Frames.\n");

   driver->preempt_data = preempt_new();
   if (!driver->preempt_data)
   {
      RARCH_WARN("Failed to initialize Preemptive Frames.\n");
      return false;
   }

   preempt = (preempt_t*)driver->preempt_data;
   retro_set_default_callbacks(&preempt->cbs);
   retro_init_libretro_cbs(&driver->retro_ctx); /* usually redundant */

   return true;
}

/**
 * update_preempt_frames
 * 
 * Inits/Deinits/Reinits preempt as needed.
 */
void update_preempt_frames()
{
   deinit_preempt();
   init_preempt();
}

/**
 * preempt_reset_buffer
 * 
 * Fills preempt buffer with current state, to prevent potentially loading a
 * bad state after init, reset, or user load-state.
 */
void preempt_reset_buffer(preempt_t *preempt)
{
   unsigned i;
   
   preempt->start_ptr = 0;
   
   pretro_serialize(preempt->buffer[0], preempt->state_size);
   
   for (i = 1; i < preempt->frames; i++)
      memcpy(preempt->buffer[i], preempt->buffer[0], preempt->state_size);
}
