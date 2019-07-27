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

#define PREEMPT_BUFFER_SIZE (MAX_PREEMPT_FRAMES + 1)
#define PREEMPT_NEXT_PTR(x) ((x + 1) % PREEMPT_BUFFER_SIZE)

struct preempt
{
   struct retro_callbacks cbs;

   void* buffer[PREEMPT_BUFFER_SIZE];
   size_t state_size;
   
   /* Last-used joypad state. Replays are triggered when this changes. */
   uint16_t joypad_state[MAX_USERS];

   /* Pointer to displayed frame (audio & video) */
   size_t av_ptr;
   /* Pointer to where replays will start.
    * Always preempt_frames behind av_ptr */
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
   static uint16_t new_joypad_state[MAX_USERS];
   unsigned i;

   preempt->cbs.poll_cb();
   
   /* Gather joypad states */
   for (i = 0; i < settings->input.max_users; i++)
   {
      new_joypad_state[i] = preempt->cbs.state_cb
            (i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK);

      if (new_joypad_state[i] != preempt->joypad_state[i])
      {  /* Input is dirty; trigger replays */
         preempt->in_replay = true;
         preempt->joypad_state[i] = new_joypad_state[i];
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

static bool preempt_init_buffers(preempt_t *preempt)
{
   unsigned i;

   if (!preempt)
      return false;

   if (!preempt->buffer)
      return false;

   preempt->state_size = pretro_serialize_size();

   for (i = 0; i < PREEMPT_BUFFER_SIZE; i++)
   {
      preempt->buffer[i] = malloc(preempt->state_size);
      if (!preempt->buffer[i])
         return false;
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

   for (i = 0; i < PREEMPT_BUFFER_SIZE; i++)
      free(preempt->buffer[i]);

   free(preempt);
}

/**
 * preempt_new:
 * @cb        : set by retro_set_default_callbacks
 *
 * Returns: new preempt handle.
 **/
static preempt_t *preempt_new()
{
   preempt_t *preempt = (preempt_t*)calloc(1, sizeof(*preempt));
   if (!preempt)
      return NULL;

   if (!preempt_init_buffers(preempt))
   {
      preempt_free(preempt);
      preempt = NULL;
   }

   return preempt;
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
   
   if (preempt->in_replay) /* replays start here */
      pretro_unserialize(preempt->buffer[preempt->start_ptr],
                         preempt->state_size);
   else
      pretro_serialize(preempt->buffer[preempt->av_ptr],
                       preempt->state_size);
   /* todo: disable autosave during replay? */
}

/**
 * preempt_post_frame:   
 * @preempt          : pointer to preempt object
 *
 * Post-frame for preempt.
 * Call this after running retro_run().
 **/
void preempt_post_frame(preempt_t *preempt)
{
   if (preempt->in_replay)
   {  /* Normal retro_run has already replayed the first frame */
      preempt->replay_ptr = PREEMPT_NEXT_PTR(preempt->start_ptr);
      while (preempt->replay_ptr != preempt->av_ptr)
      {
         pretro_serialize(preempt->buffer[preempt->replay_ptr],
                          preempt->state_size);
         pretro_run();
         preempt->replay_ptr = PREEMPT_NEXT_PTR(preempt->replay_ptr);
      }
      preempt->in_replay = false;
      pretro_serialize(preempt->buffer[preempt->replay_ptr],
                       preempt->state_size);
      pretro_run();
   }
   
   preempt->start_ptr = PREEMPT_NEXT_PTR(preempt->start_ptr);
   preempt->av_ptr = PREEMPT_NEXT_PTR(preempt->av_ptr);
}

void deinit_preempt(void)
{
   driver_t *driver   = driver_get_ptr();
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

   driver->preempt_data = (preempt_t*)preempt_new();
   if (!driver->preempt_data)
   {
      RARCH_WARN("Failed to initialize Preemptive Frames.\n");
      rarch_main_msg_queue_push("Failed to initialize Preemptive Frames.",
                                0, 180, false);
      return false;
   }

   preempt = (preempt_t*)driver->preempt_data;
   retro_set_default_callbacks(&preempt->cbs);

   return true;
}

/**
 * update_preempt_frames
 * 
 * Inits/Deinits/Reinits preempt as needed.
 * TODO: This is overkill if core & ROM have not changed.
 */
void update_preempt_frames()
{
   driver_t *driver = driver_get_ptr();

   deinit_preempt();
   init_preempt();
   if (driver->preempt_data)
      retro_init_libretro_cbs(&driver->retro_ctx);
}

/**
 * preempt_reset_buffer
 * 
 * Fills preempt buffer with current state, to prevent potentially loading a
 * bad state after init, reset, or user load-state.
 */
void preempt_reset_buffer(preempt_t *preempt)
{
   settings_t *settings = config_get_ptr();
   unsigned i;
   
   preempt->start_ptr = 0;
   preempt->av_ptr = settings->preempt_frames;
   
   pretro_serialize(preempt->buffer[0], preempt->state_size);
   
   for (i = 1; i < settings->preempt_frames; i++)
      memcpy(preempt->buffer[i], preempt->buffer[0], preempt->state_size);
}