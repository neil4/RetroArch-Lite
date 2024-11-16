/*  RetroArch - A frontend for libretro.
 *  2019-2023 - Neil Fore
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
 */

#include "dynamic.h"
#include "runloop.h"
#include "preempt.h"

#define PREEMPT_NEXT_PTR(x) ((x + 1) % preempt->frames)

struct preempt_data
{
   struct retro_callbacks cbs;

   /* Savestate buffer */
   void* buffer[MAX_PREEMPT_FRAMES];
   size_t state_size;

   /* States saved since buffer init/reset */
   uint64_t states_saved;

   /* Input states. Replays triggered on changes */
   int16_t joypad_state[MAX_USERS];
   int16_t analog_state[MAX_USERS][20];
   int16_t ptrdev_state[MAX_USERS][4];

   /* Mask of analog states requested */
   uint32_t analog_mask[MAX_USERS];
   /* Pointing device requested */
   uint8_t ptr_dev_needed[MAX_USERS];
   /* Pointing device ID of ptrdev_state */
   uint8_t ptr_dev_polled[MAX_USERS];

   bool in_replay;
   bool in_preframe;

   /* Number of latency frames to remove */
   uint8_t frames;

   /* Buffer start index for replays */
   uint8_t start_ptr;
};

static bool preempt_allocating_mem;

/**
 * preempt_in_preframe:
 * @preempt           : pointer to preempt object
 * 
 * Returns: true if audio & video should be skipped and fast savestates used
 **/
bool preempt_in_preframe(preempt_t *preempt)
{
   if (!preempt)
      return preempt_allocating_mem;
   return preempt->in_preframe;
}

void input_poll_preempt(void)
{
   /* no-op. Polling is done in preempt_input_poll */
}

static INLINE bool preempt_ptr_input_dirty(preempt_t *preempt,
      retro_input_state_t state_cb, unsigned device, unsigned port)
{
   int16_t state[4]  = {0};
   unsigned count_id = 0;
   unsigned x_id     = 0;
   unsigned id, max_id;

   switch (device)
   {
      case RETRO_DEVICE_MOUSE:
         max_id = RETRO_DEVICE_ID_MOUSE_BUTTON_5;
         break;
      case RETRO_DEVICE_LIGHTGUN:
         x_id   = RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X;
         max_id = RETRO_DEVICE_ID_LIGHTGUN_DPAD_RIGHT;
         break;
      case RETRO_DEVICE_POINTER:
         max_id   = RETRO_DEVICE_ID_POINTER_PRESSED;
         count_id = RETRO_DEVICE_ID_POINTER_COUNT;
         break;
      default:
         return false;
   }

   /* x, y */
   state[0] = state_cb(port, device, 0, x_id    );
   state[1] = state_cb(port, device, 0, x_id + 1);

   /* buttons */
   for (id = 2; id <= max_id; id++)
      state[2] |= state_cb(port, device, 0, id) ? 1 << id : 0;

   /* ptr count */
   if (count_id)
      state[3] = state_cb(port, device, 0, count_id);

   if (memcmp(preempt->ptrdev_state[port], state, sizeof(state)) == 0)
      return false;

   memcpy(preempt->ptrdev_state[port], state, sizeof(state));
   return true;
}

static INLINE bool preempt_analog_input_dirty(preempt_t *preempt,
      retro_input_state_t state_cb, unsigned port)
{
   int16_t state[20] = {0};
   uint8_t base, i;

   /* axes */
   for (i = 0; i < 2; i++)
   {
      base = i * 2;
      if (preempt->analog_mask[port] & (1 << (base    )))
         state[base    ] = state_cb(port, RETRO_DEVICE_ANALOG, i, 0);
      if (preempt->analog_mask[port] & (1 << (base + 1)))
         state[base + 1] = state_cb(port, RETRO_DEVICE_ANALOG, i, 1);
   }

   /* buttons */
   if (preempt->analog_mask[port] & 0xfff0)
   {
      for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
      {
         if (preempt->analog_mask[port] & (1 << (i + 4)))
            state[i + 4] = state_cb(port, RETRO_DEVICE_ANALOG,
                  RETRO_DEVICE_INDEX_ANALOG_BUTTON, i);
      }
   }

   if (memcmp(preempt->analog_state[port], state, sizeof(state)) == 0)
      return false;

   memcpy(preempt->analog_state[port], state, sizeof(state));
   return true;
}

static INLINE void preempt_input_poll(preempt_t *preempt)
{
   retro_input_state_t state_cb = preempt->cbs.state_cb;
   unsigned max_users = config_get_ptr()->input.max_users;
   unsigned p;

   preempt->cbs.poll_cb();

   /* Check for input state changes */
   for (p = 0; p < max_users; p++)
   {
      /* Check full digital joypad */
      uint16_t joypad_state = state_cb(p, RETRO_DEVICE_JOYPAD,
            0, RETRO_DEVICE_ID_JOYPAD_MASK);
      if (joypad_state != preempt->joypad_state[p])
      {
         preempt->joypad_state[p] = joypad_state;
         preempt->in_replay = true;
      }

      /* Check requested analogs */
      if (preempt->analog_mask[p] &&
            preempt_analog_input_dirty(preempt, state_cb, p))
      {
         preempt->in_replay = true;
         preempt->analog_mask[p] = 0;
      }

      /* Check requested pointing device */
      if (preempt->ptr_dev_needed[p])
      {
         if (preempt_ptr_input_dirty(
               preempt, state_cb, preempt->ptr_dev_needed[p], p))
            preempt->in_replay = true;

         preempt->ptr_dev_polled[p] = preempt->ptr_dev_needed[p];
         preempt->ptr_dev_needed[p] = RETRO_DEVICE_NONE;
      }
   }
}

int16_t input_state_preempt(unsigned port, unsigned device,
      unsigned index, unsigned id)
{
   driver_t *driver   = driver_get_ptr();
   preempt_t *preempt = (preempt_t*)driver->preempt_data;
   unsigned dev_class = device & RETRO_DEVICE_MASK;

   switch (dev_class)
   {
      case RETRO_DEVICE_JOYPAD:
         /* Shortcut callback */
         if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
            return preempt->joypad_state[port];
         else
            return preempt->joypad_state[port] & (1 << id) ? 1 : 0;
      case RETRO_DEVICE_ANALOG:
         /* Add requested input to mask */
         preempt->analog_mask[port] |= (1 << (id + index * 2));
         break;
      case RETRO_DEVICE_LIGHTGUN:
      case RETRO_DEVICE_POINTER:
         /* Set pointing device for this port */
         preempt->ptr_dev_needed[port] = dev_class;
         break;
      case RETRO_DEVICE_MOUSE:
         /* Return stored x,y */
         if (id <= RETRO_DEVICE_ID_MOUSE_Y)
         {
            preempt->ptr_dev_needed[port] = dev_class;
            if (preempt->ptr_dev_polled[port] == dev_class)
               return preempt->ptrdev_state[port][id];
         }
         break;
      default:
         break;
   }

   return preempt->cbs.state_cb(port, device, index, id);
}

static bool preempt_alloc_buffer(preempt_t *preempt)
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

   return true;
}

/**
 * preempt_free:
 * @preempt    : pointer to preempt_t object
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
static preempt_t *preempt_new(void)
{
   settings_t *settings = config_get_ptr();
   preempt_t  *preempt  = (preempt_t*)calloc(1, sizeof(*preempt));
   if (!preempt)
      return NULL;
   
   preempt->frames = settings->preempt_frames;

   if (!preempt_alloc_buffer(preempt))
   {
      preempt_free(preempt);
      preempt = NULL;
   }

   return preempt;
}

/**
 * preempt_serialize_or_realloc
 *
 * Attempt to handle variable savestate size
 */
static INLINE bool preempt_serialize_or_realloc(preempt_t *preempt)
{
   if (pretro_serialize(
         preempt->buffer[preempt->start_ptr], preempt->state_size))
   {
      preempt->states_saved++;
      return true;
   }

   if (preempt->state_size < pretro_serialize_size())
      return event_command(EVENT_CMD_PREEMPT_UPDATE);

   return false;
}

/**
 * preempt_pre_frame:
 * @preempt         : pointer to preempt_t object
 *
 * Pre-frame for preemptive frames.
 * Called before retro_run().
 **/
void preempt_pre_frame(preempt_t *preempt)
{
   driver_t *driver     = driver_get_ptr();
   preempt->in_preframe = true;
   preempt_input_poll(preempt);
   const char *failed_str;
   uint8_t replay_ptr;
   
   if (preempt->in_replay
         && preempt->states_saved >= preempt->frames)
   {
      /* Suspend A/V and run preemptive frames */
      driver->audio_suspended = true;
      driver->video_active    = false;

      if (!pretro_unserialize(
            preempt->buffer[preempt->start_ptr], preempt->state_size))
      {
         failed_str = "Failed to Load State for Preemptive Frames.";
         goto error;
      }

      pretro_run();
      replay_ptr = PREEMPT_NEXT_PTR(preempt->start_ptr);

      while (replay_ptr != preempt->start_ptr)
      {
         if (!pretro_serialize(
               preempt->buffer[replay_ptr], preempt->state_size))
         {
            failed_str = "Failed to Save State for Preemptive Frames.";
            goto error;
         }

         pretro_run();
         replay_ptr = PREEMPT_NEXT_PTR(replay_ptr);
      }

      preempt->in_replay      = false;
      driver->audio_suspended = false;
      driver->video_active    = true;
   }
   
   /* Save current state and update start_ptr to point to oldest state. */
   if (!preempt_serialize_or_realloc(preempt))
   {
      failed_str = "Failed to Save State for Preemptive Frames.";
      goto error;
   }

   preempt->start_ptr   = PREEMPT_NEXT_PTR(preempt->start_ptr);
   preempt->in_preframe = false;
   return;

error:
   driver->audio_suspended = false;
   driver->video_active    = true;
   rarch_main_msg_queue_push(failed_str, 0, 180, false);
   preempt_deinit();
}

void preempt_deinit(void)
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
 * preempt_init:
 *
 * Creates buffers and sets callbacks. Skips if preempt_frames == 0.
 *
 * Returns: true on success, false on failure
 **/
bool preempt_init(void)
{
   driver_t   *driver   = driver_get_ptr();
   settings_t *settings = config_get_ptr();
   global_t   *global   = global_get_ptr();
   preempt_t  *preempt  = NULL;
   
   if (settings->preempt_frames == 0
         || !global->content_is_init || global->libretro_dummy)
      return false;
   
   if (driver->netplay_data)
   {
      RARCH_WARN("Cannot use Preemptive Frames during Netplay.\n");
      return false;
   }

   /* Run at least one frame before attempting
    * pretro_serialize_size or pretro_serialize */
   if (video_state_get_frame_count() == 0)
      pretro_run();

   if (pretro_serialize_size() == 0)
   {
      RARCH_WARN("Preemptive Frames init failed. "
            "Core does not support savestates.\n");
      rarch_main_msg_queue_push("Preemptive Frames init failed.\n"
            "Core does not support savestates.", 0, 180, false);
      return false;
   }

   RARCH_LOG("Initializing Preemptive Frames.\n");

   preempt_allocating_mem = true;
   driver->preempt_data = preempt_new();
   preempt_allocating_mem = false;

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
 * preempt_reset_buffer
 * 
 * Forces preempt to refill its state buffer before replaying frames.
 */
void preempt_reset_buffer(preempt_t *preempt)
{
   if (preempt)
      preempt->states_saved = 0;
}
