/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
 *  Copyright (C) 2014-2015 - Jay McCarthy
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

#include <file/file_path.h>
#include <retro_inline.h>
#include <string/stdstring.h>

#include <rhash.h>

#include "configuration.h"
#include "dynamic.h"
#include "performance.h"
#include "retroarch_logger.h"
#include "intl/intl.h"
#include "retroarch.h"
#include "runloop.h"
#include "runloop_data.h"
#include "preempt.h"

#include "input/keyboard_line.h"
#include "input/input_common.h"

#include "menu/menu.h"

#ifdef HAVE_NETPLAY
#include "netplay.h"
#endif

static struct runloop *g_runloop = NULL;
static struct global *g_extern   = NULL;

#ifdef HAVE_THREADS
static slock_t *mq_lock = NULL;
#endif

#define QUIT_CONFIRM_MSG "Press again to quit..."

/**
 * check_pause:
 * @pressed              : was libretro pause key pressed?
 * @frameadvance_pressed : was frameadvance key pressed?
 *
 * Check if libretro pause key was pressed. If so, pause or 
 * unpause the libretro core.
 *
 * Returns: true if libretro pause key was toggled, otherwise false.
 **/
static bool check_pause(bool pause_pressed, bool frameadvance_pressed)
{
   runloop_t *runloop       = rarch_main_get_ptr();
   static bool old_focus    = true;
   bool focus               = true;
   enum event_command cmd   = EVENT_CMD_NONE;
   bool old_is_paused       = runloop->is_paused;
   settings_t *settings     = config_get_ptr();
   unsigned long frame_count;
   static char msg[32];

   if (settings->pause_nonactive)
      focus = video_driver_has_focus();

   if (focus)
   {
      /* FRAMEADVANCE will set us into pause mode. */
      if (frameadvance_pressed)
      {
         pause_pressed |= !old_is_paused;
         frame_count = video_state_get_frame_count() + (pause_pressed ? 1:0);
         snprintf(msg, sizeof(msg), "Frame %lu", frame_count);
         rarch_main_msg_queue_push(msg, 1, 0, true);
      }

      if (pause_pressed)
      {
         cmd = EVENT_CMD_PAUSE_TOGGLE;
         if (!old_is_paused && !frameadvance_pressed)
            rarch_main_msg_queue_push("Paused", 1, 0, true);
      }
      else if (!old_focus)
         cmd = EVENT_CMD_UNPAUSE;
   }
   else if (old_focus)
      cmd = EVENT_CMD_PAUSE;

   old_focus = focus;

   if (cmd != EVENT_CMD_NONE)
      event_command(cmd);

   if (runloop->is_paused == old_is_paused)
      return false;

   return true;
}

/**
 * check_fast_forward_button:
 * @fastforward_pressed  : is fastforward key pressed?
 * @hold_pressed         : is fastforward key pressed and held?
 * @old_hold_pressed     : was fastforward key pressed and held the last frame?
 *
 * Checks if the fast forward key has been pressed for this frame. 
 *
 **/
static void check_fast_forward_button(bool fastforward_pressed,
      bool hold_pressed, bool old_hold_pressed)
{
   driver_t *driver = driver_get_ptr();

   /* To avoid continuous switching if we hold the button down, we require
    * that the button must go from pressed to unpressed back to pressed 
    * to be able to toggle between then.
    */
   if (fastforward_pressed)
      driver->nonblock_state = !driver->nonblock_state;
   else if (old_hold_pressed != hold_pressed)
      driver->nonblock_state = hold_pressed;
   else
      return;

   driver_set_nonblock_state(driver->nonblock_state);
   if (driver->nonblock_state)
      rarch_main_msg_queue_push("Fast forward", 0, 0, true);
   else
      rarch_main_msg_queue_push("", 0, 1, true);
}

/**
 * check_stateslots:
 * @pressed_increase     : is state slot increase key pressed?
 * @pressed_decrease     : is state slot decrease key pressed?
 *
 * Checks if the state increase/decrease keys have been pressed 
 * for this frame. 
 **/
static void check_stateslots(bool pressed_increase, bool pressed_decrease)
{
   char msg[32];
   settings_t *settings      = config_get_ptr();

   /* Save state slots */
   if (pressed_increase)
      settings->state_slot++;
   else if (pressed_decrease)
   {
      if (settings->state_slot > 0)
         settings->state_slot--;
   }
   else
      return;

   snprintf(msg, sizeof(msg), "State slot: %d",
         settings->state_slot);

   rarch_main_msg_queue_push(msg, 1, 180, true);

   RARCH_LOG("%s\n", msg);
}

/**
 * check_rewind:
 * @pressed              : was rewind key pressed or held?
 *
 * Checks if rewind toggle/hold was being pressed and/or held.
 **/
static void check_rewind(bool pressed)
{
   static bool first = true;
   global_t *global  = global_get_ptr();

   if (global->rewind.frame_is_reverse)
   {
      audio_driver_frame_is_reverse();
      global->rewind.frame_is_reverse = false;
   }

   if (first)
   {
      first = false;
      return;
   }

   if (!global->rewind.state)
      return;

   if (pressed)
   {
      const void *buf    = NULL;
      runloop_t *runloop = rarch_main_get_ptr();

      if (state_manager_pop(global->rewind.state, &buf))
      {
         global->rewind.frame_is_reverse = true;
         audio_driver_setup_rewind();

         rarch_main_msg_queue_push(RETRO_MSG_REWINDING, 0,
               runloop->is_paused ? 1 : 30, true);
         pretro_unserialize(buf, global->rewind.size);
      }
      else
         rarch_main_msg_queue_push(RETRO_MSG_REWIND_REACHED_END,
               0, 30, true);
   }
   else
   {
      static unsigned cnt      = 0;
      settings_t *settings     = config_get_ptr();

      cnt = (cnt + 1) % (settings->rewind_granularity ?
            settings->rewind_granularity : 1); /* Avoid possible SIGFPE. */

      if (cnt == 0)
      {
         void *state = NULL;
         state_manager_push_where(global->rewind.state, &state);
         pretro_serialize(state, global->rewind.size);
         state_manager_push_do(global->rewind.state);
      }
   }

   retro_set_rewind_callbacks();
}

/**
 * check_slowmotion:
 * @slowmotion_pressed   : was slow motion key pressed or held?
 *
 * Checks if slowmotion toggle/hold was being pressed and/or held.
 **/
static void check_slowmotion(bool slowmotion_pressed)
{
   runloop_t *runloop       = rarch_main_get_ptr();
   settings_t *settings     = config_get_ptr();
   global_t *global         = global_get_ptr();

   runloop->is_slowmotion   = slowmotion_pressed;

   if (!runloop->is_slowmotion)
      return;

   if (settings->video.black_frame_insertion)
      video_driver_cached_frame();

   rarch_main_msg_queue_push(global->rewind.frame_is_reverse ?
         "Slow motion rewind" : "Slow motion", 0, 1, true);
}

#define SHADER_EXT_GLSL      0x7c976537U
#define SHADER_EXT_GLSLP     0x0f840c87U
#define SHADER_EXT_CG        0x0059776fU
#define SHADER_EXT_CGP       0x0b8865bfU

/**
 * check_shader_dir:
 * @pressed_next         : was next shader key pressed?
 * @pressed_previous     : was previous shader key pressed?
 *
 * Checks if any one of the shader keys has been pressed for this frame: 
 * a) Next shader index.
 * b) Previous shader index.
 *
 * Will also immediately apply the shader.
 **/
static void check_shader_dir(bool pressed_next, bool pressed_prev)
{
   uint32_t ext_hash;
   char *msg                   = NULL;
   const char *shader          = NULL;
   const char *ext             = NULL;
   enum rarch_shader_type type = RARCH_SHADER_NONE;
   global_t *global            = global_get_ptr();

   if (!global->shader_dir.list)
      return;

   if (pressed_next)
   {
      global->shader_dir.ptr = (global->shader_dir.ptr + 1) %
         global->shader_dir.list->size;
   }
   else if (pressed_prev)
   {
      if (global->shader_dir.ptr == 0)
         global->shader_dir.ptr = global->shader_dir.list->size - 1;
      else
         global->shader_dir.ptr--;
   }
   else
      return;

   shader   = global->shader_dir.list->elems[global->shader_dir.ptr].data;
   ext      = path_get_extension(shader);
   ext_hash = djb2_calculate(ext);

   switch (ext_hash)
   {
      case SHADER_EXT_GLSL:
      case SHADER_EXT_GLSLP:
         type = RARCH_SHADER_GLSL;
         break;
      case SHADER_EXT_CG:
      case SHADER_EXT_CGP:
         type = RARCH_SHADER_CG;
         break;
      default:
         return;
   }

   msg = string_alloc(PATH_MAX_LENGTH);
   snprintf(msg, PATH_MAX_LENGTH, "Shader #%u: \"%s\"",
         (unsigned)global->shader_dir.ptr, path_basename(shader));
   rarch_main_msg_queue_push(msg, 1, 120, true);
   RARCH_LOG("Applying shader \"%s\".\n", shader);

   if (!video_driver_set_shader(type, shader))
      RARCH_WARN("Failed to apply shader.\n");

   free(msg);
}

static void do_state_check_menu_toggle(void)
{
   global_t *global   = global_get_ptr();

   if (menu_driver_alive())
   {
      if (global->main_is_init && !global->libretro_dummy)
         rarch_main_set_state(RARCH_ACTION_STATE_MENU_RUNNING_FINISHED);
      return;
   }

   rarch_main_set_state(RARCH_ACTION_STATE_MENU_RUNNING);
}

/**
 * do_pre_state_checks:
 *
 * Checks for state changes in this frame.
 *
 * Unlike do_state_checks(), this is performed for both
 * the menu and the regular loop.
 *
 * Returns: 0.
 **/
static int do_pre_state_checks(event_cmd_state_t *cmd)
{
   runloop_t *runloop        = rarch_main_get_ptr();
   global_t *global          = global_get_ptr();

   if (cmd->overlay_next_pressed)
      event_command(EVENT_CMD_OVERLAY_NEXT);

   if (!runloop->is_paused || menu_driver_alive())
   {
      if (cmd->fullscreen_toggle)
         event_command(EVENT_CMD_FULLSCREEN_TOGGLE);
   }

   if (cmd->grab_mouse_pressed)
      event_command(EVENT_CMD_GRAB_MOUSE_TOGGLE);

   if (cmd->kbd_focus_toggle_pressed)
      event_command(EVENT_CMD_KEYBOARD_FOCUS_TOGGLE);

   if (cmd->menu_pressed || (global->libretro_dummy))
      do_state_check_menu_toggle();

   return 0;
}

#ifdef HAVE_NETPLAY
static int do_netplay_state_checks(
      bool netplay_flip_pressed)
{
   if (netplay_flip_pressed)
      event_command(EVENT_CMD_NETPLAY_FLIP_PLAYERS);
   return 0;
}
#endif

static int do_pause_state_checks(
      bool frameadvance_pressed,
      bool fullscreen_toggle_pressed,
      bool rewind_pressed)
{
   runloop_t *runloop        = rarch_main_get_ptr();
   bool check_is_oneshot     = frameadvance_pressed || rewind_pressed;

   if (!runloop || !runloop->is_paused)
      return 0;

   if (fullscreen_toggle_pressed)
   {
      event_command(EVENT_CMD_FULLSCREEN_TOGGLE);
      video_driver_cached_frame();
   }

   if (!check_is_oneshot)
      return 1;

   return 0;
}

/**
 * do_state_checks:
 *
 * Checks for state changes in this frame.
 *
 * Returns: 1 if RetroArch is in pause mode, 0 otherwise.
 **/
static int do_state_checks(event_cmd_state_t *cmd)
{
   driver_t  *driver         = driver_get_ptr();
   runloop_t *runloop        = rarch_main_get_ptr();
   global_t  *global         = global_get_ptr();

   (void)driver;

   if (runloop->is_idle)
      return 1;

   if (cmd->screenshot_pressed)
      event_command(EVENT_CMD_TAKE_SCREENSHOT);

   if (cmd->mute_pressed)
      event_command(EVENT_CMD_AUDIO_MUTE_TOGGLE);

   if (cmd->osk_pressed)
   {
      driver_t *driver     = driver_get_ptr();

      if (driver)
         driver->keyboard_linefeed_enable = !driver->keyboard_linefeed_enable;
   }

   if (cmd->advanced_toggle_pressed)
      event_command(EVENT_CMD_ADVANCED_SETTINGS_TOGGLE);

#ifdef HAVE_NETPLAY
   if (driver->netplay_data)
      return do_netplay_state_checks(cmd->netplay_flip_pressed);
#endif

   if (!menu_driver_alive())
   {
      check_pause(cmd->pause_pressed, cmd->frameadvance_pressed);

      if (do_pause_state_checks(
               cmd->frameadvance_pressed,
               cmd->fullscreen_toggle,
               cmd->rewind_pressed))
         return 1;

      check_fast_forward_button(
            cmd->fastforward_pressed, cmd->hold_pressed, cmd->old_hold_pressed);
   }

   check_stateslots(cmd->state_slot_increase, cmd->state_slot_decrease);

   if (cmd->save_state_pressed)
      event_command(EVENT_CMD_SAVE_STATE);
   else if (cmd->load_state_pressed)
      event_command(EVENT_CMD_LOAD_STATE);

   check_rewind(cmd->rewind_pressed);
   check_slowmotion(cmd->slowmotion_pressed);

   check_shader_dir(cmd->shader_next_pressed, cmd->shader_prev_pressed);

   if (cmd->disk_eject_pressed)
      event_command(EVENT_CMD_DISK_EJECT_TOGGLE);
   else if (cmd->disk_next_pressed)
      event_command(EVENT_CMD_DISK_NEXT);
   else if (cmd->disk_prev_pressed)
      event_command(EVENT_CMD_DISK_PREV);

   if (cmd->reset_pressed)
      event_command(EVENT_CMD_RESET);

   if (global->cheat)
   {
      if (cmd->cheat_index_plus_pressed)
         cheat_manager_index_next(global->cheat);
      else if (cmd->cheat_index_minus_pressed)
         cheat_manager_index_prev(global->cheat);
      else if (cmd->cheat_toggle_pressed)
         cheat_manager_toggle(global->cheat);
   }

   return 0;
}

/**
 * time_to_exit:
 *
 * rarch_main_iterate() checks this to see if it's time to
 * exit out of the main loop.
 *
 * Reasons for exiting:
 * a) Shutdown environment callback was invoked.
 * b) Quit key was pressed.
 * c) Frame count exceeds or equals maximum amount of frames to run.
 * d) Video driver no longer alive.
 *
 * Returns: 1 if any of the above conditions are true, otherwise 0.
 **/
static INLINE int time_to_exit(event_cmd_state_t *cmd)
{
   runloop_t *runloop            = rarch_main_get_ptr();
   global_t  *global             = global_get_ptr();
   bool shutdown_pressed         = global->system.shutdown;
   bool video_alive              = video_driver_is_alive();
   bool frame_count_end          = (runloop->frames.video.max && 
         video_state_get_frame_count() >= runloop->frames.video.max);
   bool quit_key_confirmed       = false;
   const char *msg;

   if (cmd->quit_key_pressed)
   {
      msg = rarch_main_msg_queue_pull();
      if (msg && !strcmp(msg, QUIT_CONFIRM_MSG))
         quit_key_confirmed = true;
      else
         rarch_main_msg_queue_push(QUIT_CONFIRM_MSG, 10, 120, true);
   }

   if (shutdown_pressed || quit_key_confirmed || frame_count_end
         || !video_alive)
   {
      global->system.shutdown = true;
      return 1;
   }
   return 0;
}

/**
 * rarch_update_frame_time:
 *
 * Updates frame timing if frame timing callback is in use by the core.
 **/
static void rarch_update_frame_time(void)
{
   runloop_t *runloop       = rarch_main_get_ptr();
   driver_t *driver         = driver_get_ptr();
   settings_t *settings     = config_get_ptr();
   retro_time_t curr_time   = rarch_get_time_usec();
   global_t  *global        = global_get_ptr();
   retro_time_t delta       = curr_time - global->system.frame_time_last;
   bool is_locked_fps       = runloop->is_paused || driver->nonblock_state;

   is_locked_fps         |= !!driver->recording_data;

   if (!global->system.frame_time_last || is_locked_fps)
      delta = global->system.frame_time.reference;

   if (!is_locked_fps && runloop->is_slowmotion)
      delta /= settings->slowmotion_ratio;

   global->system.frame_time_last = curr_time;

   if (is_locked_fps)
      delta = 0;

   global->system.frame_time.callback(delta);
}


/**
 * rarch_limit_frame_time:
 *
 * Throttle core speed or fastforward speed.
 **/
static void rarch_limit_frame_time(void)
{
   retro_time_t target                  = 0;
   retro_time_t to_sleep_ms             = 0;
   runloop_t *runloop                   = rarch_main_get_ptr();
   settings_t *settings                 = config_get_ptr();
   driver_t *driver                     = driver_get_ptr();
   retro_time_t current                 = rarch_get_time_usec();
   double mft_f;

   double throttled_fps = settings->throttle_using_core_fps ?
                          video_viewport_get_system_av_info()->timing.fps
                          : settings->video.refresh_rate;
   
   if (menu_driver_alive() && settings->menu.pause_libretro)
      mft_f = 1000000.0f / 60.5f;  /* try to rely on vsync */
   else if (runloop->is_slowmotion)
      mft_f = settings->slowmotion_ratio * (1000000.0f / throttled_fps);
   else if (driver->nonblock_state)
   {
      if (settings->fastforward_ratio > 1.0f)
         mft_f = 1000000.0f / (throttled_fps * settings->fastforward_ratio);
      else
         return;
   }
   else if (settings->core_throttle_enable)
      mft_f = 1000000.0f / throttled_fps;
   else
      return;

   runloop->frames.limit.minimum_time = (retro_time_t) roundf(mft_f);

   target      = runloop->frames.limit.last_time
                    + runloop->frames.limit.minimum_time;
   to_sleep_ms = (target - current) / 1000;

   if (to_sleep_ms <= 0)
   {
      runloop->frames.limit.last_time = rarch_get_time_usec();
      return;
   }

   rarch_sleep((unsigned int)to_sleep_ms);

   runloop->frames.limit.last_time = target;
}

/**
 * check_block_hotkey:
 * @enable_hotkey        : Is hotkey enable key enabled?
 *
 * Checks if 'hotkey enable' key is pressed.
 **/
static bool check_block_hotkey(bool enable_hotkey)
{
   bool use_hotkey_enable;
   settings_t *settings             = config_get_ptr();
   driver_t *driver                 = driver_get_ptr();
   const struct retro_keybind *bind = 
      &settings->input.binds[0][RARCH_ENABLE_HOTKEY];
   const struct retro_keybind *autoconf_bind = 
      &settings->input.autoconf_binds[0][RARCH_ENABLE_HOTKEY];

   /* If we haven't bound anything to this, 
    * always allow hotkeys. */
   use_hotkey_enable                = 
      bind->key != RETROK_UNKNOWN ||
      bind->joykey != NO_BTN ||
      bind->joyaxis != AXIS_NONE ||
      autoconf_bind->key != RETROK_UNKNOWN ||
      autoconf_bind->joykey != NO_BTN ||
      autoconf_bind->joyaxis != AXIS_NONE;

   driver->block_hotkey             = 
      (input_driver_keyboard_mapping_is_blocked()
         && menu_driver_alive())
      || (use_hotkey_enable && !enable_hotkey);

   /* If we hold ENABLE_HOTKEY button, block all libretro input to allow 
    * hotkeys to be bound to same keys as RetroPad. */
   return (use_hotkey_enable && enable_hotkey);
}

/**
 * input_keys_pressed:
 *
 * Grab an input sample for this frame.
 *
 * TODO: In case RARCH_BIND_LIST_END starts exceeding 64,
 * and you need a bitmask of more than 64 entries, reimplement
 * it to use something like rarch_bits_t.
 *
 * Returns: Input sample containing a mask of all pressed keys.
 */
static INLINE retro_input_t input_keys_pressed(void)
{
   driver_t *driver         = driver_get_ptr();

   if (!driver->input || !driver->input_data)
      return 0;

   driver->block_libretro_input = check_block_hotkey(
         input_driver_key_pressed(RARCH_ENABLE_HOTKEY));

   return input_driver_keys_pressed();
}

/**
 * input_flush:
 * @input                : input sample for this frame
 *
 * Resets input sample.
 *
 * Returns: always true (1).
 **/
static bool input_flush(retro_input_t *input)
{
   runloop_t *runloop = rarch_main_get_ptr();

   *input = 0;

   /* If core was paused before entering menu, evoke
    * pause toggle to wake it up. */
   if (runloop->is_paused)
      BIT64_SET(*input, RARCH_PAUSE_TOGGLE);

   return true;
}

/**
 * rarch_main_iterate_quit:
 *
 * Quits out of RetroArch main loop.
 *
 * On special case, loads dummy core 
 * instead of exiting RetroArch completely.
 * Aborts core shutdown if invoked.
 *
 * Returns: -1 if we are about to quit, otherwise 0.
 **/
static int rarch_main_iterate_quit(void)
{
   settings_t *settings     = config_get_ptr();
   global_t   *global       = global_get_ptr();

   if (global->core_shutdown_initiated
         && settings->load_dummy_on_core_shutdown)
   {
      if (!event_command(EVENT_CMD_PREPARE_DUMMY))
         return -1;

      /* Reload core without starting */
      event_command(EVENT_CMD_LOAD_CORE);
      event_command(EVENT_CMD_OVERLAY_LOAD);
      menu_reset();

      global->core_shutdown_initiated = false;

      return 0;
   }
   
   return -1;
}

#ifdef HAVE_OVERLAY
static void rarch_main_iterate_linefeed_overlay(void)
{
   driver_t *driver = driver_get_ptr();

   if (driver->osk_enable && !driver->keyboard_linefeed_enable)
      driver->osk_enable = false;
   else if (!driver->osk_enable && driver->keyboard_linefeed_enable)
      driver->osk_enable = true;
   else
      return;

   event_command(EVENT_CMD_OVERLAY_LOAD);
   if (driver->overlay)
      driver->overlay->blocked = true;
}
#endif

const char *rarch_main_msg_queue_pull(void)
{
   runloop_t *runloop = rarch_main_get_ptr();
   const char *ret = NULL;

   if (!runloop)
      return NULL;

#ifdef HAVE_THREADS
   slock_lock(mq_lock);
#endif

   ret = msg_queue_pull(runloop->msg_queue);

#ifdef HAVE_THREADS
   slock_unlock(mq_lock);
#endif

   return ret;
}

void rarch_main_msg_queue_push(const char *msg, unsigned prio, unsigned duration,
      bool flush)
{
   runloop_t *runloop = rarch_main_get_ptr();
   if (!runloop->msg_queue)
      return;

#ifdef HAVE_THREADS
   slock_lock(mq_lock);
#endif

   if (flush)
      msg_queue_clear(runloop->msg_queue);
   msg_queue_push(runloop->msg_queue, msg, prio, duration);

#ifdef HAVE_THREADS
   slock_unlock(mq_lock);
#endif
}

void rarch_main_msg_queue_free(void)
{
   runloop_t *runloop = rarch_main_get_ptr();
   if (!runloop)
      return;

   if (runloop->msg_queue)
   {
#ifdef HAVE_THREADS
   slock_lock(mq_lock);
#endif

      msg_queue_free(runloop->msg_queue);

#ifdef HAVE_THREADS
      slock_unlock(mq_lock);
      slock_free(mq_lock);
#endif
   }

   runloop->msg_queue = NULL;
}

void rarch_main_msg_queue_init(void)
{
   runloop_t *runloop = rarch_main_get_ptr();
   if (!runloop)
      return;

   if (!runloop->msg_queue)
   {
      runloop->msg_queue = msg_queue_new(8);
      rarch_assert(runloop->msg_queue);

#ifdef HAVE_THREADS
      mq_lock = slock_new();
      rarch_assert(mq_lock);
#endif
   }
}

global_t *global_get_ptr(void)
{
   return g_extern;
}

runloop_t *rarch_main_get_ptr(void)
{
   return g_runloop;
}

void rarch_main_state_free(void)
{
   if (!g_runloop)
      return;

   free(g_runloop);
   g_runloop = NULL;
}

void rarch_main_global_free(void)
{
   event_command(EVENT_CMD_TEMPORARY_CONTENT_DEINIT);
   event_command(EVENT_CMD_SUBSYSTEM_FULLPATHS_DEINIT);
   event_command(EVENT_CMD_RECORD_DEINIT);
   event_command(EVENT_CMD_LOG_FILE_DEINIT);

   if (!g_extern)
      return;

   free(g_extern);
   g_extern = NULL;
}

bool rarch_main_verbosity(void)
{
   global_t *global = global_get_ptr();
   if (!global)
      return false;
   return global->verbosity;
}

FILE *rarch_main_log_file(void)
{
   global_t *global = global_get_ptr();
   if (!global)
      return NULL;
   return global->log_file;
}

static global_t *rarch_main_global_new(void)
{
   global_t *global = (global_t*)calloc(1, sizeof(global_t));

   if (!global)
      return NULL;

   return global;
}

static runloop_t *rarch_main_state_init(void)
{
   runloop_t *runloop = (runloop_t*)calloc(1, sizeof(runloop_t));

   if (!runloop)
      return NULL;

   return runloop;
}

void rarch_main_clear_state(void)
{
   driver_clear_state();

   rarch_main_state_free();
   g_runloop = rarch_main_state_init();

   rarch_main_global_free();
   g_extern  = rarch_main_global_new();
}

bool rarch_main_is_idle(void)
{
   runloop_t *runloop = rarch_main_get_ptr();
   if (!runloop)
      return false;
   return runloop->is_idle;
}

static void rarch_main_cmd_get_state(event_cmd_state_t *cmd,
      retro_input_t input, retro_input_t old_input,
      retro_input_t trigger_input)
{
   if (!cmd)
      return;

   cmd->fullscreen_toggle           = BIT64_GET(trigger_input, RARCH_FULLSCREEN_TOGGLE_KEY);
   cmd->overlay_next_pressed        = BIT64_GET(trigger_input, RARCH_OVERLAY_NEXT);
   cmd->grab_mouse_pressed          = BIT64_GET(trigger_input, RARCH_GRAB_MOUSE_TOGGLE);
   cmd->menu_pressed                = BIT64_GET(trigger_input, RARCH_MENU_TOGGLE);
   cmd->quit_key_pressed            = BIT64_GET(trigger_input, RARCH_QUIT_KEY);
   cmd->screenshot_pressed          = BIT64_GET(trigger_input, RARCH_SCREENSHOT);
   cmd->mute_pressed                = BIT64_GET(trigger_input, RARCH_MUTE);
   cmd->osk_pressed                 = BIT64_GET(trigger_input, RARCH_OSK);
   cmd->advanced_toggle_pressed     = BIT64_GET(trigger_input, RARCH_ADVANCED_TOGGLE);
   cmd->reset_pressed               = BIT64_GET(trigger_input, RARCH_RESET);
   cmd->disk_prev_pressed           = BIT64_GET(trigger_input, RARCH_DISK_PREV);
   cmd->disk_next_pressed           = BIT64_GET(trigger_input, RARCH_DISK_NEXT);
   cmd->disk_eject_pressed          = BIT64_GET(trigger_input, RARCH_DISK_EJECT_TOGGLE);
   cmd->save_state_pressed          = BIT64_GET(trigger_input, RARCH_SAVE_STATE_KEY);
   cmd->load_state_pressed          = BIT64_GET(trigger_input, RARCH_LOAD_STATE_KEY);
   cmd->slowmotion_pressed          = BIT64_GET(input, RARCH_SLOWMOTION);
   cmd->shader_next_pressed         = BIT64_GET(trigger_input, RARCH_SHADER_NEXT);
   cmd->shader_prev_pressed         = BIT64_GET(trigger_input, RARCH_SHADER_PREV);
   cmd->fastforward_pressed         = BIT64_GET(trigger_input, RARCH_FAST_FORWARD_KEY);
   cmd->hold_pressed                = BIT64_GET(input, RARCH_FAST_FORWARD_HOLD_KEY);
   cmd->old_hold_pressed            = BIT64_GET(old_input, RARCH_FAST_FORWARD_HOLD_KEY);
   cmd->state_slot_increase         = BIT64_GET(trigger_input, RARCH_STATE_SLOT_PLUS);
   cmd->state_slot_decrease         = BIT64_GET(trigger_input, RARCH_STATE_SLOT_MINUS);
   cmd->pause_pressed               = BIT64_GET(trigger_input, RARCH_PAUSE_TOGGLE);
   cmd->frameadvance_pressed        = BIT64_GET(trigger_input, RARCH_FRAMEADVANCE);
   cmd->rewind_pressed              = BIT64_GET(input,         RARCH_REWIND);
   cmd->netplay_flip_pressed        = BIT64_GET(trigger_input, RARCH_NETPLAY_FLIP);
   cmd->cheat_index_plus_pressed    = BIT64_GET(trigger_input, RARCH_CHEAT_INDEX_PLUS);
   cmd->cheat_index_minus_pressed   = BIT64_GET(trigger_input, RARCH_CHEAT_INDEX_MINUS);
   cmd->cheat_toggle_pressed        = BIT64_GET(trigger_input, RARCH_CHEAT_TOGGLE);
   cmd->kbd_focus_toggle_pressed    = BIT64_GET(trigger_input, RARCH_TOGGLE_KEYBOARD_FOCUS);
}

/**
 * rarch_main_iterate:
 *
 * Run Libretro core in RetroArch for one frame.
 *
 * Returns: 0 on success, 1 if we have to wait until button input in order
 * to wake up the loop, -1 if we forcibly quit out of the RetroArch iteration loop. 
 **/
int rarch_main_iterate(void)
{
   retro_input_t trigger_input;
   event_cmd_state_t    cmd        = {0};
   int ret                         = 0;
   static retro_input_t last_input = 0;
   retro_input_t old_input         = last_input;
   retro_input_t input             = input_keys_pressed();
   last_input                      = input;
   driver_t *driver                = driver_get_ptr();
   settings_t *settings            = config_get_ptr();
   global_t   *global              = global_get_ptr();

   driver->input_polled = false;

   if (driver->flushing_input)
      driver->flushing_input = (input) ? input_flush(&input) : false;

   trigger_input = input & ~old_input;

   rarch_main_cmd_get_state(&cmd, input, old_input, trigger_input);

   if (time_to_exit(&cmd))
      return rarch_main_iterate_quit();

   if (global->system.frame_time.callback)
      rarch_update_frame_time();

   do_pre_state_checks(&cmd);

#ifdef HAVE_OVERLAY
   rarch_main_iterate_linefeed_overlay();
#endif
   
   if (do_state_checks(&cmd))
   {
      /* RetroArch has been paused */
      driver->retro_ctx.poll_cb();
      rarch_sleep(10);
      
      return 1;
   }

   if (menu_driver_alive())
   {
      if (menu_iterate(input, old_input, trigger_input) == -1)
         rarch_main_set_state(RARCH_ACTION_STATE_MENU_RUNNING_FINISHED);

      if (!input && settings->menu.pause_libretro)
        ret = 1;
      goto success;
   }

   if (global->exec)
   {
      global->exec = false;
      return rarch_main_iterate_quit();
   }

#if defined(HAVE_THREADS)
   lock_autosave();
#endif

   if ((settings->video.frame_delay > 0) && !driver->nonblock_state)
      rarch_sleep(settings->video.frame_delay);

   if (driver->preempt_data)
      preempt_pre_frame((preempt_t*)driver->preempt_data);
#ifdef HAVE_NETPLAY
   else if (driver->netplay_data)
      netplay_pre_frame((netplay_t*)driver->netplay_data);
#endif

   /* Run libretro for one frame. */
   pretro_run();

#ifdef HAVE_NETPLAY
   if (driver->netplay_data)
      netplay_post_frame((netplay_t*)driver->netplay_data);
#endif

#if defined(HAVE_THREADS)
   unlock_autosave();
#endif

success:
   rarch_limit_frame_time();

   if (!driver->input_polled)
      driver->retro_ctx.poll_cb();

   return ret;
}
