/*  RetroArch - A frontend for libretro.
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

#include <compat/strl.h>

#include "command_event.h"

#include "general.h"
#include "performance.h"
#include "runloop_data.h"
#include "runloop.h"
#include "dynamic.h"
#include "content.h"
#include "screenshot.h"
#include "intl/intl.h"
#include "retroarch.h"
#include "dir_list_special.h"
#include "preempt.h"

#include "configuration.h"
#include "input/input_remapping.h"
#include "input/input_joypad.h"

#include "menu/menu.h"
#include "menu/menu_display.h"
#include "menu/menu_shader.h"
#include "menu/menu_input.h"

#ifdef HAVE_NETPLAY
#include "netplay.h"
#endif

#ifdef HAVE_NETWORKING
#include <net/net_compat.h>
#endif

#ifdef HAVE_COMMAND
static void event_init_command(void)
{
   driver_t *driver     = driver_get_ptr();
   settings_t *settings = config_get_ptr();

   if (!settings->stdin_cmd_enable && !settings->network_cmd_enable)
      return;

   if (settings->stdin_cmd_enable && input_driver_grab_stdin())
   {
      RARCH_WARN("stdin command interface is desired, but input driver has already claimed stdin.\n"
            "Cannot use this command interface.\n");
   }

   if (!(driver->command = rarch_cmd_new(settings->stdin_cmd_enable
               && !input_driver_grab_stdin(),
               settings->network_cmd_enable, settings->network_cmd_port)))
      RARCH_ERR("Failed to initialize command interface.\n");
}
#endif

/**
 * event_free_temporary_content:
 *
 * Frees temporary content handle.
 **/
static void event_free_temporary_content(void)
{
   unsigned i;
   global_t *global = global_get_ptr();

   for (i = 0; i < global->temporary_content->size; i++)
   {
      const char *path = global->temporary_content->elems[i].data;

      RARCH_LOG("Removing temporary content file: %s.\n", path);
      if (remove(path) < 0)
         RARCH_ERR("Failed to remove temporary file: %s.\n", path);
   }
   string_list_free(global->temporary_content);
}

#if defined(HAVE_THREADS)
static void event_init_autosave(void)
{
   unsigned i;
   settings_t *settings = config_get_ptr();
   global_t   *global   = global_get_ptr();

   if (settings->autosave_interval < 1 || !global->savefiles)
      return;

   if (!(global->autosave = (autosave_t**)calloc(global->savefiles->size,
               sizeof(*global->autosave))))
      return;

   global->num_autosave = global->savefiles->size;

   for (i = 0; i < global->savefiles->size; i++)
   {
      const char *path = global->savefiles->elems[i].data;
      unsigned    type = global->savefiles->elems[i].attr.i;

      if (pretro_get_memory_size(type) <= 0)
         continue;

      global->autosave[i] = autosave_new(path,
            pretro_get_memory_data(type),
            pretro_get_memory_size(type),
            settings->autosave_interval);

      if (!global->autosave[i])
         RARCH_WARN(RETRO_LOG_INIT_AUTOSAVE_FAILED);
   }
}

static void event_deinit_autosave(void)
{
   unsigned i;
   global_t *global = global_get_ptr();

   for (i = 0; i < global->num_autosave; i++)
      autosave_free(global->autosave[i]);

   if (global->autosave)
      free(global->autosave);
   global->autosave     = NULL;

   global->num_autosave = 0;
}
#endif

static void event_save_files(void)
{
   unsigned i;
   global_t *global = global_get_ptr();

   if (!global->savefiles || !global->use_sram)
      return;

   for (i = 0; i < global->savefiles->size; i++)
   {
      unsigned type    = global->savefiles->elems[i].attr.i;
      const char *path = global->savefiles->elems[i].data;
      RARCH_LOG("Saving RAM type #%u to \"%s\".\n", type, path);
      save_ram_file(path, type);
   }
}

/**
 * event_disk_control_set_eject:
 * @new_state            : Eject or close the virtual drive tray.
 *                         false (0) : Close
 *                         true  (1) : Eject
 * @print_log            : Show message onscreen.
 *
 * Ejects/closes of the virtual drive tray.
 **/
static void event_disk_control_set_eject(bool new_state, bool print_log)
{
   char msg[NAME_MAX_LENGTH] = {0};
   global_t *global          = global_get_ptr();
   bool error                = false;
   const struct retro_disk_control_callback *control = 
      (const struct retro_disk_control_callback*)&global->system.disk_control;

   if (!control->get_num_images)
      return;

   *msg = '\0';

   if (control->set_eject_state(new_state))
      snprintf(msg, sizeof(msg), "%s virtual disc tray.",
            new_state ? "Ejected" : "Closed");
   else
   {
      error = true;
      snprintf(msg, sizeof(msg), "Failed to %s virtual disc tray.",
            new_state ? "eject" : "close");
   }

   if (*msg)
   {
      if (error)
         RARCH_ERR("%s\n", msg);
      else
         RARCH_LOG("%s\n", msg);

      /* Only noise in menu. */
      if (print_log)
         rarch_main_msg_queue_push(msg, 1, 180, true);
   }
}

/**
 * event_disk_control_append_image:
 * @path                 : Path to disk image. 
 *
 * Appends disk image to disk image list.
 **/
void event_disk_control_append_image(const char *path)
{
   unsigned new_idx;
   char msg[PATH_MAX_LENGTH]   = {0};
   struct retro_game_info info = {0};
   global_t *global = global_get_ptr();
   const struct retro_disk_control_callback *control = 
      (const struct retro_disk_control_callback*)&global->system.disk_control;

   event_disk_control_set_eject(true, false);

   control->add_image_index();
   new_idx = control->get_num_images();
   if (!new_idx)
      return;
   new_idx--;

   info.path = path;
   control->replace_image_index(new_idx, &info);
   control->set_image_index(new_idx);

   snprintf(msg, sizeof(msg), "Loaded disc: %s", path);
   RARCH_LOG("%s\n", msg);
   rarch_main_msg_queue_push(msg, 0, 180, true);

   event_command(EVENT_CMD_AUTOSAVE_DEINIT);

   /* TODO: Need to figure out what to do with subsystems case. */
   if (!*global->subsystem)
   {
      /* Update paths for our new image.
       * If we actually use append_image, we assume that we
       * started out in a single disk case, and that this way
       * of doing it makes the most sense. */
      rarch_set_paths(path);
      rarch_fill_pathnames();
   }

   event_command(EVENT_CMD_AUTOSAVE_INIT);

   event_disk_control_set_eject(false, false);
}

/**
 * event_check_disk_eject:
 * @control              : Handle to disk control handle.
 *
 * Perform disk eject (Core Disk Options).
 **/
static void event_check_disk_eject(
      const struct retro_disk_control_callback *control)
{
   bool new_state = !control->get_eject_state();
   event_disk_control_set_eject(new_state, true);
}

/**
 * event_disk_control_set_index:
 * @idx                : Index of disk to set as current.
 *
 * Sets current disk to @index.
 **/
static void event_disk_control_set_index(unsigned idx)
{
   unsigned num_disks;
   char msg[NAME_MAX_LENGTH] = {0};
   global_t *global          = global_get_ptr();
   const struct retro_disk_control_callback *control = 
      (const struct retro_disk_control_callback*)&global->system.disk_control;
   bool error = false;

   if (!control->get_num_images)
      return;

   *msg = '\0';

   num_disks = control->get_num_images();

   if (control->set_image_index(idx))
   {
      if (idx < num_disks)
         snprintf(msg, sizeof(msg), "Setting disc %u of %u in tray.",
               idx + 1, num_disks);
      else
         strlcpy(msg, "Removed disc from tray.", sizeof(msg));
   }
   else
   {
      if (idx < num_disks)
         snprintf(msg, sizeof(msg), "Failed to set disc %u of %u.",
               idx + 1, num_disks);
      else
         strlcpy(msg, "Failed to remove disc from tray.", sizeof(msg));
      error = true;
   }

   if (*msg)
   {
      if (error)
         RARCH_ERR("%s\n", msg);
      else
         RARCH_LOG("%s\n", msg);
      rarch_main_msg_queue_push(msg, 1, 180, true);
   }
}

/**
 * event_check_disk_prev:
 * @control              : Handle to disk control handle.
 *
 * Perform disk cycle to previous index action (Core Disk Options).
 **/
static void event_check_disk_prev(
      const struct retro_disk_control_callback *control)
{
   unsigned num_disks    = control->get_num_images();
   unsigned current      = control->get_image_index();
   bool disk_prev_enable = num_disks && num_disks != UINT_MAX;

   if (!disk_prev_enable)
   {
      RARCH_ERR("Got invalid disc index from libretro.\n");
      return;
   }

   if (current > 0)
      current--;
   event_disk_control_set_index(current);
}

/**
 * event_check_disk_next:
 * @control              : Handle to disk control handle.
 *
 * Perform disk cycle to next index action (Core Disk Options).
 **/
static void event_check_disk_next(
      const struct retro_disk_control_callback *control)
{
   unsigned num_disks        = control->get_num_images();
   unsigned current          = control->get_image_index();
   bool     disk_next_enable = num_disks && num_disks != UINT_MAX;

   if (!disk_next_enable)
   {
      RARCH_ERR("Got invalid disc index from libretro.\n");
      return;
   }

   if (current < num_disks - 1)
      current++;
   event_disk_control_set_index(current);
}

/**
 * event_set_volume:
 * @gain      : amount of gain to be applied to current volume level.
 *
 * Adjusts the current audio volume level.
 *
 **/
static void event_set_volume(float gain)
{
   char msg[NAME_MAX_LENGTH] = {0};
   settings_t *settings      = config_get_ptr();

   settings->audio.volume += gain;
   settings->audio.volume  = max(settings->audio.volume, -80.0f);
   settings->audio.volume  = min(settings->audio.volume, 12.0f);

   snprintf(msg, sizeof(msg), "Volume: %.1f dB", settings->audio.volume);
   rarch_main_msg_queue_push(msg, 1, 180, true);
   RARCH_LOG("%s\n", msg);

   audio_driver_set_volume_gain(db_to_gain(settings->audio.volume));
}

/**
 * event_init_controllers:
 *
 * Initialize libretro controllers.
 **/
static void event_init_controllers(void)
{
   unsigned i;
   settings_t *settings = config_get_ptr();
   global_t   *global   = global_get_ptr();

   /* Some cores do not properly range check port argument.
    * This is broken behavior of course, but avoid breaking
    * cores needlessly. */
   for (i = 0; i < global->system.num_ports; i++)
   {
      const char *ident = NULL;
      const struct retro_controller_description *desc = NULL;
      unsigned device = (i < settings->input.max_users) ?
            settings->input.libretro_device[i] : RETRO_DEVICE_NONE;

      desc = libretro_find_controller_description(
            &global->system.ports[i], device);

      if (desc)
         ident = desc->desc;

      if (!ident)
      {
         /* If we're trying to connect a completely unknown device,
          * revert back to JOYPAD. */

         if (device != RETRO_DEVICE_JOYPAD && device != RETRO_DEVICE_NONE)
         {
            /* Do not fix settings->input.libretro_device[i],
             * because any use of dummy core will reset this,
             * which is not a good idea. */
            RARCH_WARN("Input device ID %u is unknown to this libretro "
               "implementation. Using RETRO_DEVICE_JOYPAD.\n", device);
            device = RETRO_DEVICE_JOYPAD;
         }
         ident = "Joypad";
      }

      if (device != RETRO_DEVICE_NONE)
         RARCH_LOG("Connecting %s (ID: %u) to port %u.\n", ident, device, i+1);
      else
         RARCH_LOG("Disconnecting device from port %u.\n", i+1);

      pretro_set_controller_port_device(i, device);
   }
}


static void event_init_cheats(void)
{
   bool allow_cheats = true;
   driver_t *driver  = driver_get_ptr();

   (void)driver;

#ifdef HAVE_NETPLAY
   allow_cheats &= !driver->netplay_data;
#endif

   if (!allow_cheats)
      return;

   /* TODO/FIXME - add some stuff here. */
}

static bool event_load_save_files(void)
{
   unsigned i;
   global_t *global = global_get_ptr();

   if (!global->savefiles || global->sram_load_disable)
      return false;

   for (i = 0; i < global->savefiles->size; i++)
      load_ram_file(global->savefiles->elems[i].data,
            global->savefiles->elems[i].attr.i);

   return true;
}

static void event_load_auto_state(void)
{
   bool ret;
   char msg[PATH_MAX_LENGTH]                 = {0};
   char savestate_name_auto[PATH_MAX_LENGTH] = {0};
   settings_t *settings = config_get_ptr();
   global_t   *global   = global_get_ptr();

#ifdef HAVE_NETPLAY
   if (global->netplay_enable)
      return;
#endif

   if (!settings->savestate_auto_load)
      return;

   fill_pathname_noext(savestate_name_auto, global->savestate_name,
         ".auto", sizeof(savestate_name_auto));

   if (!path_file_exists(savestate_name_auto))
      return;

   ret = load_state(savestate_name_auto);

   RARCH_LOG("Found auto savestate in: %s\n", savestate_name_auto);

   snprintf(msg, sizeof(msg), "Auto-loading savestate from \"%s\" %s.",
         savestate_name_auto, ret ? "succeeded" : "failed");
   rarch_main_msg_queue_push(msg, 1, 180, false);
   RARCH_LOG("%s\n", msg);
}

static void event_set_savestate_auto_index(void)
{
   size_t i;
   char state_dir[PATH_MAX_LENGTH]  = {0};
   char state_base[PATH_MAX_LENGTH] = {0};
   struct string_list *dir_list     = NULL;
   unsigned max_idx                 = 0;
   settings_t *settings             = config_get_ptr();
   global_t   *global               = global_get_ptr();

   if (!settings->savestate_auto_index)
      return;

   /* Find the file in the same directory as global->savestate_name
    * with the largest numeral suffix.
    *
    * E.g. /foo/path/content.state, will try to find
    * /foo/path/content.state%d, where %d is the largest number available.
    */

   fill_pathname_basedir(state_dir, global->savestate_name,
         sizeof(state_dir));
   fill_pathname_base(state_base, global->savestate_name,
         sizeof(state_base));

   if (!(dir_list = dir_list_new_special(state_dir, DIR_LIST_PLAIN)))
      return;

   for (i = 0; i < dir_list->size; i++)
   {
      unsigned idx;
      char elem_base[PATH_MAX_LENGTH] = {0};
      const char *end                 = NULL;
      const char *dir_elem            = dir_list->elems[i].data;

      fill_pathname_base(elem_base, dir_elem, sizeof(elem_base));

      if (strstr(elem_base, state_base) != elem_base)
         continue;

      end = dir_elem + strlen(dir_elem);
      while ((end > dir_elem) && isdigit(end[-1]))
         end--;

      idx = strtoul(end, NULL, 0);
      if (idx > max_idx)
         max_idx = idx;
   }

   dir_list_free(dir_list);

   settings->state_slot = max_idx;
   RARCH_LOG("Found last state slot: #%d\n", settings->state_slot);
}

static bool event_init_content(void)
{
   global_t *global = global_get_ptr();
   settings_t *settings = config_get_ptr();

   /* No content to be loaded for dummy core,
    * just successfully exit. */
   if (global->libretro_dummy) 
      return true;

   scoped_config_files_load_auto();

   if (settings->auto_remaps_enable)
      remap_file_load_auto();

   if (!global->libretro_no_content)
      rarch_fill_pathnames();

   if (!init_content_file())
      return false;

   event_set_savestate_auto_index();

   if (event_load_save_files())
      RARCH_LOG("Skipping SRAM load.\n");

   event_load_auto_state();
   event_command(EVENT_CMD_NETPLAY_INIT);
   event_command(EVENT_CMD_PREEMPT_FRAMES_UPDATE);

   return true;
}

static bool event_init_core(void)
{
   global_t *global     = global_get_ptr();
   driver_t *driver     = driver_get_ptr();

   /* Reset video format to libretro's default if this is not a dummy core. */
   if (!global->libretro_dummy)
      video_driver_set_pixel_format(RETRO_PIXEL_FORMAT_0RGB1555);

   pretro_set_environment(rarch_environment_cb);

   /* per-core saves: reset redirection paths */
   set_paths_redirect();

   rarch_verify_api_version();
   pretro_init();

   global->use_sram = !global->libretro_dummy &&
      !global->libretro_no_content;

   if (!event_init_content())
      return false;

   retro_init_libretro_cbs(&driver->retro_ctx);
   rarch_init_system_av_info();

   return true;
}

static bool event_save_auto_state(void)
{
   bool ret;
   char savestate_name_auto[PATH_MAX_LENGTH] = {0};
   settings_t *settings = config_get_ptr();
   global_t   *global   = global_get_ptr();

   if (!settings->savestate_auto_save || global->libretro_dummy ||
       global->libretro_no_content)
       return false;

   fill_pathname_noext(savestate_name_auto, global->savestate_name,
         ".auto", sizeof(savestate_name_auto));

   ret = save_state(savestate_name_auto);
   RARCH_LOG("Auto save state to \"%s\" %s.\n", savestate_name_auto, ret ?
         "succeeded" : "failed");

   return true;
}

/**
 * event_save_state
 * @path            : Path to state.
 * @s               : Message.
 * @len             : Size of @s.
 *
 * Saves a state with path being @path.
 **/
static void event_save_state(const char *path,
      char *s, size_t len)
{
   settings_t *settings = config_get_ptr();

   if (!save_state(path))
   {
      snprintf(s, len, "Failed to save state to \"%s\".", path);
      return;
   }

   if (settings->state_slot < 0)
      snprintf(s, len, "Saved state to slot #-1 (auto).");
   else
      snprintf(s, len, "Saved state to slot #%d.", settings->state_slot);
}

/**
 * event_load_state
 * @path            : Path to state.
 * @s               : Message.
 * @len             : Size of @s.
 *
 * Loads a state with path being @path.
 **/
static void event_load_state(const char *path, char *s, size_t len)
{
   settings_t *settings = config_get_ptr();

   if (!load_state(path))
   {
      snprintf(s, len, "Failed to load state from \"%s\".", path);
      return;

   }

   if (settings->state_slot < 0)
      snprintf(s, len, "Loaded state from slot #-1 (auto).");
   else
      snprintf(s, len, "Loaded state from slot #%d.", settings->state_slot);
}

static void event_main_state(unsigned cmd)
{
   char path[PATH_MAX_LENGTH] = {0};
   char msg[PATH_MAX_LENGTH]  = {0};
   global_t *global           = global_get_ptr();
   settings_t *settings       = config_get_ptr();

   if (settings->state_slot > 0)
      snprintf(path, sizeof(path), "%s%d",
            global->savestate_name, settings->state_slot);
   else if (settings->state_slot < 0)
      snprintf(path, sizeof(path), "%s.auto",
            global->savestate_name);
   else
      strlcpy(path, global->savestate_name, sizeof(path));

   if (pretro_serialize_size())
   {
      if (cmd == EVENT_CMD_SAVE_STATE)
         event_save_state(path, msg, sizeof(msg));
      else if (cmd == EVENT_CMD_LOAD_STATE)
         event_load_state(path, msg, sizeof(msg));
   }
   else
      strlcpy(msg, "Core does not support save states.", sizeof(msg));

   rarch_main_msg_queue_push(msg, 2, 180, true);
   RARCH_LOG("%s\n", msg);
}

static bool event_update_system_info(struct retro_system_info *_info,
      bool *load_no_content)
{
   settings_t *settings = config_get_ptr();
   global_t   *global   = global_get_ptr();

#if defined(HAVE_DYNAMIC)
   if (!(*settings->libretro))
      return false;

   libretro_get_system_info(settings->libretro, _info,
         load_no_content);
#endif
   if (!global->core_info)
      return false;

   if (!core_info_list_get_info(global->core_info,
            global->core_info_current, settings->libretro))
      return false;

   return true;
}

/**
 * event_command:
 * @cmd                  : Event command index.
 *
 * Performs RetroArch event command with index @cmd.
 *
 * Returns: true (1) on success, otherwise false (0).
 **/
bool event_command(enum event_command cmd)
{
   unsigned i           = 0;
   bool boolean         = false;
   runloop_t *runloop   = rarch_main_get_ptr();
   driver_t  *driver    = driver_get_ptr();
   global_t  *global    = global_get_ptr();
   settings_t *settings = config_get_ptr();
   
   (void)i;

   switch (cmd)
   {
      case EVENT_CMD_LOAD_CONTENT_PERSIST:
#ifdef HAVE_DYNAMIC
         event_command(EVENT_CMD_LOAD_CORE);
#endif
         rarch_main_set_state(RARCH_ACTION_STATE_LOAD_CONTENT);

         if (global->content_is_init)
         {
            if (!global->libretro_no_content)
               global->max_scope = NUM_SETTING_SCOPES-1;
            runloop->is_paused = false;
            msg_queue_clear(runloop->msg_queue);
         }

         break;
      case EVENT_CMD_LOAD_CONTENT:
#ifdef HAVE_DYNAMIC
         event_command(EVENT_CMD_LOAD_CONTENT_PERSIST);
#else
         rarch_environment_cb(RETRO_ENVIRONMENT_SET_LIBRETRO_PATH,
               (void*)settings->libretro);
         rarch_environment_cb(RETRO_ENVIRONMENT_EXEC,
               (void*)global->fullpath);
         event_command(EVENT_CMD_QUIT);
#endif
         break;
      case EVENT_CMD_LOAD_CORE_DEINIT:
#ifdef HAVE_DYNAMIC
         libretro_free_system_info(&global->menu.info);
#endif
         break;
      case EVENT_CMD_LOAD_CORE_PERSIST:
         event_command(EVENT_CMD_LOAD_CORE_DEINIT);
         {
            menu_handle_t *menu = menu_driver_get_ptr();
            if (menu)
               event_update_system_info(&global->menu.info,
                     &menu->load_no_content);

            if (global->menu.info.valid_extensions
                  && *global->menu.info.valid_extensions)
               global->libretro_supports_content = true;
            else
               global->libretro_supports_content = false;
         }
         break;
      case EVENT_CMD_LOAD_CORE:
         rarch_update_configs();

         if (!*global->fullpath)
            core_config_file_load_auto();
         global->max_scope = THIS_CORE;

         event_command(EVENT_CMD_LOAD_CORE_PERSIST);
#ifndef HAVE_DYNAMIC
         event_command(EVENT_CMD_QUIT);
#endif
         break;
      case EVENT_CMD_LOAD_STATE:
         event_main_state(cmd);
#ifdef HAVE_NETPLAY
         if (driver->netplay_data && !netplay_send_savestate())
            netplay_disconnect();
         else
#endif
         if (driver->preempt_data &&
                !preempt_reset_buffer(driver->preempt_data))
            deinit_preempt();
         break;
      case EVENT_CMD_RESIZE_WINDOWED_SCALE:
         if (global->pending.windowed_scale == 0)
            return false;

         settings->video.scale = global->pending.windowed_scale;

         if (!settings->video.fullscreen)
            event_command(EVENT_CMD_REINIT);

         global->pending.windowed_scale = 0;
         break;
      case EVENT_CMD_MENU_TOGGLE:
         if (menu_driver_alive())
            rarch_main_set_state(RARCH_ACTION_STATE_MENU_RUNNING_FINISHED);
         else
            rarch_main_set_state(RARCH_ACTION_STATE_MENU_RUNNING);
         break;
      case EVENT_CMD_MENU_ENTRIES_REFRESH:
         menu_entries_set_refresh();
         break;
      case EVENT_CMD_CONTROLLERS_INIT:
         event_init_controllers();
         break;
      case EVENT_CMD_RESET:
         RARCH_LOG(RETRO_LOG_RESETTING_CONTENT);
         rarch_main_msg_queue_push("Reset.", 1, 120, true);
         pretro_reset();

         /* bSNES since v073r01 resets controllers to JOYPAD
          * after a reset, so just enforce it here. */
         event_command(EVENT_CMD_CONTROLLERS_INIT);
         if (menu_driver_alive())
            rarch_main_set_state(RARCH_ACTION_STATE_MENU_RUNNING_FINISHED);

#ifdef HAVE_NETPLAY
         if (driver->netplay_data && !netplay_send_savestate())
            netplay_disconnect();
         else
#endif
         if (driver->preempt_data &&
                !preempt_reset_buffer(driver->preempt_data))
            deinit_preempt();
         break;
      case EVENT_CMD_SAVE_STATE:
         if (settings->savestate_auto_index)
            settings->state_slot++;

         event_main_state(cmd);
         break;
      case EVENT_CMD_TAKE_SCREENSHOT:
         if (!take_screenshot())
            return false;
         break;
      case EVENT_CMD_PREPARE_DUMMY:
         {
            menu_handle_t *menu = menu_driver_get_ptr();
            if (menu)
               menu->load_no_content = false;

            rarch_main_data_deinit();

            *global->fullpath = '\0';

            rarch_main_set_state(RARCH_ACTION_STATE_LOAD_CONTENT);
            global->system.shutdown = false;
            global->content_is_init = false;
         }
         break;
      case EVENT_CMD_UNLOAD_CORE:
         *settings->libretro = '\0';
         global->max_scope = GLOBAL;

         rarch_update_configs();

         event_command(EVENT_CMD_PREPARE_DUMMY);
         event_command(EVENT_CMD_LOAD_CORE_DEINIT);

         menu_navigation_set(menu_navigation_get_ptr(), 0, true);
         break;
      case EVENT_CMD_QUIT:
         rarch_main_set_state(RARCH_ACTION_STATE_QUIT);
         break;
      case EVENT_CMD_REINIT:
         {
            const struct retro_hw_render_callback *hw_render =
               (const struct retro_hw_render_callback*)video_driver_callback();

            driver->video_cache_context     = hw_render->cache_context;
            driver->video_cache_context_ack = false;
            event_command(EVENT_CMD_RESET_CONTEXT);
            driver->video_cache_context     = false;

            /* Poll input to avoid possibly stale data to corrupt things. */
            input_driver_poll();

            menu_display_fb_set_dirty();

            if (menu_driver_alive())
               event_command(EVENT_CMD_VIDEO_SET_BLOCKING_STATE);
         }
         break;
      case EVENT_CMD_CHEATS_DEINIT:
         if (!global)
            break;

         if (global->cheat)
            cheat_manager_free(global->cheat);
         global->cheat = NULL;
         break;
      case EVENT_CMD_CHEATS_INIT:
         event_command(EVENT_CMD_CHEATS_DEINIT);
         event_init_cheats();
         break;
      case EVENT_CMD_REWIND_DEINIT:
         if (!global)
            break;
#ifdef HAVE_NETPLAY
         if (driver->netplay_data)
            return false;
#endif
         if (global->rewind.state)
            state_manager_free(global->rewind.state);
         global->rewind.state = NULL;
         break;
      case EVENT_CMD_REWIND_INIT:
         if (*settings->libretro)
            init_rewind();
         break;
      case EVENT_CMD_REWIND_TOGGLE:
         if (settings->rewind_enable)
            event_command(EVENT_CMD_REWIND_INIT);
         else
            event_command(EVENT_CMD_REWIND_DEINIT);
         break;
      case EVENT_CMD_AUTOSAVE_DEINIT:
#ifdef HAVE_THREADS
         event_deinit_autosave();
#endif
         break;
      case EVENT_CMD_AUTOSAVE_INIT:
         event_command(EVENT_CMD_AUTOSAVE_DEINIT);
#ifdef HAVE_THREADS
         event_init_autosave();
#endif
         break;
      case EVENT_CMD_AUTOSAVE_STATE:
         event_save_auto_state();
         break;
      case EVENT_CMD_AUDIO_STOP:
         if (!driver->audio_data)
            return false;
         if (!audio_driver_alive())
            return false;

         if (!audio_driver_stop())
            return false;
         break;
      case EVENT_CMD_AUDIO_START:
         if (!driver->audio_data || audio_driver_alive())
            return false;

         if (!settings->audio.mute_enable && !audio_driver_start())
         {
            RARCH_ERR("Failed to start audio driver. Will continue without audio.\n");
            driver->audio_active = false;
         }
         break;
      case EVENT_CMD_AUDIO_MUTE_TOGGLE:
         {
            const char *msg = !settings->audio.mute_enable ?
               "Audio muted." : "Audio unmuted.";

            if (!audio_driver_mute_toggle())
            {
               RARCH_ERR("Failed to unmute audio.\n");
               return false;
            }

            rarch_main_msg_queue_push(msg, 1, 180, true);
            RARCH_LOG("%s\n", msg);
         }
         break;
      case EVENT_CMD_OVERLAY_UNLOAD:
#ifdef HAVE_OVERLAY
         /* Disable and move to cache */
         input_overlay_enable(driver->overlay, false);
         if (driver->overlay)
            driver->overlay->iface = NULL;

         if (driver->overlay_cache)
            input_overlay_free(driver->overlay_cache);

         driver->overlay_cache = driver->overlay;
         driver->overlay       = NULL;

         memset(&driver->overlay_state, 0, sizeof(driver->overlay_state));
#endif
         break;
      case EVENT_CMD_OVERLAY_FREE_CACHED:
#ifdef HAVE_OVERLAY
         if (driver->overlay_cache)
            input_overlay_free(driver->overlay_cache);
         driver->overlay_cache = NULL;
#endif
         break;
      case EVENT_CMD_OVERLAY_SWAP_CACHED:
#ifdef HAVE_OVERLAY
      {
         input_overlay_t *overlay;
         bool enable = driver->osk_enable ?
               settings->input.osk_enable : settings->input.overlay_enable;

         input_overlay_enable(driver->overlay, false);
         if (driver->overlay)
            driver->overlay->iface = NULL;

         overlay               = driver->overlay_cache;
         driver->overlay_cache = driver->overlay;
         driver->overlay       = overlay;

         input_overlay_load_cached(overlay, enable);
         break;
      }
#endif
         break;
      case EVENT_CMD_OVERLAY_LOAD:
#ifdef HAVE_OVERLAY
      {
         const char *path;
         bool enable;

         if (driver->osk_enable)
         {
            if (!settings->input.osk_enable || !*settings->input.osk_overlay)
            {
               driver->keyboard_linefeed_enable = false;
               break;
            }
            path   = settings->input.osk_overlay;
            enable = true;
         }
         else
         {
            if (!*settings->input.overlay)
               break;
            path   = settings->input.overlay;
            enable = settings->input.overlay_enable;
         }

         /* Load from cache if possible. */
         if (driver->overlay_cache
               && driver->overlay_cache->state == OVERLAY_STATUS_ALIVE
               && !strcmp(path, driver->overlay_cache->overlay_path))
         {
            event_command(EVENT_CMD_OVERLAY_SWAP_CACHED);
            break;
         }
         else
            event_command(EVENT_CMD_OVERLAY_UNLOAD);

         driver->overlay = input_overlay_new(path, enable);

         if (!driver->overlay)
            RARCH_ERR("Failed to load overlay.\n");
      }
#endif
         break;
      case EVENT_CMD_OVERLAY_NEXT:
#ifdef HAVE_OVERLAY
         input_overlay_next(driver->overlay);
#endif
         break;
      case EVENT_CMD_DSP_FILTER_DEINIT:
         if (!global)
            break;

         audio_driver_dsp_filter_free();
         break;
      case EVENT_CMD_DSP_FILTER_INIT:
         event_command(EVENT_CMD_DSP_FILTER_DEINIT);
         if (!*settings->audio.dsp_plugin)
            break;
         audio_driver_dsp_filter_init(settings->audio.dsp_plugin);
         break;
      case EVENT_CMD_GPU_RECORD_DEINIT:
         if (!global)
            break;

         if (global->record.gpu_buffer)
            free(global->record.gpu_buffer);
         global->record.gpu_buffer = NULL;
         break;
      case EVENT_CMD_RECORD_DEINIT:
         if (!recording_deinit())
            return false;
         break;
      case EVENT_CMD_RECORD_INIT:
         if (!recording_init())
            return false;
         break;
      case EVENT_CMD_CORE_INFO_DEINIT:
         if (!global)
            break;

         if (global->core_info)
            core_info_list_free(global->core_info);
         global->core_info = NULL;
         break;
      case EVENT_CMD_DATA_RUNLOOP_FREE:
         rarch_main_data_free();
         break;
      case EVENT_CMD_CORE_INFO_INIT:
         event_command(EVENT_CMD_CORE_INFO_DEINIT);

         if (*settings->libretro_directory)
#ifdef EXTERNAL_LAUNCHER
            global->core_info = core_info_list_new(LAUNCHED_CORE);
#else
            global->core_info = core_info_list_new(INSTALLED_CORES);
#endif
         break;
      case EVENT_CMD_CORE_DEINIT:
         video_driver_free_hw_context();

         pretro_unload_game();
         pretro_deinit();
         
         event_command(EVENT_CMD_DRIVERS_DEINIT);
         
         pretro_set_environment(rarch_environment_cb);
         uninit_libretro_sym();
         break;
      case EVENT_CMD_CORE_INIT:
         if (!event_init_core())
            return false;
         break;
      case EVENT_CMD_VIDEO_APPLY_STATE_CHANGES:
         video_driver_apply_state_changes();
         break;
      case EVENT_CMD_VIDEO_SET_NONBLOCKING_STATE:
         boolean = true; /* fall-through */
      case EVENT_CMD_VIDEO_SET_BLOCKING_STATE:
         video_driver_set_nonblock_state(boolean);
         break;
      case EVENT_CMD_VIDEO_SET_ASPECT_RATIO:
         video_driver_set_aspect_ratio(settings->video.aspect_ratio_idx);
         break;
      case EVENT_CMD_AUDIO_SET_NONBLOCKING_STATE:
         boolean = true; /* fall-through */
      case EVENT_CMD_AUDIO_SET_BLOCKING_STATE:
         audio_driver_set_nonblock_state(boolean);
         break;
      case EVENT_CMD_OVERLAY_SET_SCALE_FACTOR:
      case EVENT_CMD_OVERLAY_UPDATE_ASPECT_AND_SHIFT:
#ifdef HAVE_OVERLAY
         input_overlays_update_aspect_shift_scale(driver->overlay);
#endif
         break;
      case EVENT_CMD_OVERLAY_SET_ALPHA:
#ifdef HAVE_OVERLAY
         input_overlay_set_alpha(driver->overlay);
#endif
         break;
      case EVENT_CMD_OVERLAY_UPDATE_EIGHTWAY_DIAG_SENS:
#ifdef HAVE_OVERLAY
         input_overlay_update_eightway_diag_sens();
#endif
         break;         
      case EVENT_CMD_DRIVERS_DEINIT:
         uninit_drivers(DRIVERS_CMD_ALL);
         break;
      case EVENT_CMD_DRIVERS_INIT:
         init_drivers(DRIVERS_CMD_ALL);
         break;
      case EVENT_CMD_AUDIO_REINIT:
         uninit_drivers(DRIVER_AUDIO);
         init_drivers(DRIVER_AUDIO);
         break;
      case EVENT_CMD_RESET_CONTEXT:
      {
         struct retro_hw_render_callback hwr_copy;
         struct retro_hw_render_callback *hwr = video_driver_callback();

         memcpy(&hwr_copy, hwr, sizeof(hwr_copy));
         event_command(EVENT_CMD_DRIVERS_DEINIT);
         memcpy(hwr, &hwr_copy, sizeof(hwr_copy));

         event_command(EVENT_CMD_DRIVERS_INIT);
         break;
      }
      case EVENT_CMD_QUIT_RETROARCH:
         rarch_main_set_state(RARCH_ACTION_STATE_FORCE_QUIT);
         break;
      case EVENT_CMD_RESUME:
         if (menu_driver_alive())
            rarch_main_set_state(RARCH_ACTION_STATE_MENU_RUNNING_FINISHED);
         break;
      case EVENT_CMD_RESTART_RETROARCH:
#if defined(GEKKO) && defined(HW_RVL)
         fill_pathname_join(global->fullpath, g_defaults.core_dir,
               SALAMANDER_FILE,
               sizeof(global->fullpath));
#endif
         if (driver->frontend_ctx && driver->frontend_ctx->set_fork)
            driver->frontend_ctx->set_fork(true, false);
         break;
      case EVENT_CMD_SHADERS_APPLY_CHANGES:
         menu_shader_manager_apply_changes();
         break;
      case EVENT_CMD_PAUSE_CHECKS:
         if (runloop->is_paused)
         {
            RARCH_LOG("Paused.\n");
            event_command(EVENT_CMD_AUDIO_STOP);
            video_driver_cached_frame();
         }
         else
         {
            RARCH_LOG("Unpaused.\n");
            event_command(EVENT_CMD_AUDIO_START);
            msg_queue_clear(runloop->msg_queue);
         }
         break;
      case EVENT_CMD_PAUSE_TOGGLE:
         runloop->is_paused = !runloop->is_paused;
         event_command(EVENT_CMD_PAUSE_CHECKS);
         break;
      case EVENT_CMD_UNPAUSE:
         runloop->is_paused = false;
         event_command(EVENT_CMD_PAUSE_CHECKS);
         break;
      case EVENT_CMD_PAUSE:
         runloop->is_paused = true;
         event_command(EVENT_CMD_PAUSE_CHECKS);
         break;
      case EVENT_CMD_MENU_PAUSE_LIBRETRO:
         if (menu_driver_alive())
         {
            if (settings->menu.pause_libretro)
               event_command(EVENT_CMD_AUDIO_STOP);
            else
               event_command(EVENT_CMD_AUDIO_START);
         }
         else
         {
            if (settings->menu.pause_libretro)
               event_command(EVENT_CMD_AUDIO_START);
         }
         break;
      case EVENT_CMD_SHADER_DIR_DEINIT:
         if (!global)
            break;

         dir_list_free(global->shader_dir.list);
         global->shader_dir.list = NULL;
         global->shader_dir.ptr  = 0;
         break;
      case EVENT_CMD_SHADER_DIR_INIT:
      {
         char shader_dir[PATH_MAX_LENGTH];

         event_command(EVENT_CMD_SHADER_DIR_DEINIT);

         if (!*settings->video.shader_dir && !*settings->video.shader_path)
            return false;

         fill_pathname_parent_dir(shader_dir, settings->video.shader_path,
                                  PATH_MAX_LENGTH);
         global->shader_dir.list = dir_list_new_special(shader_dir,
                                                        DIR_LIST_SHADERS);
         if (!global->shader_dir.list || global->shader_dir.list->size == 0)
            return false;

         global->shader_dir.ptr = 0;
         dir_list_sort(global->shader_dir.list, false);
         for (i = 0; i < global->shader_dir.list->size; i++)
         {
            RARCH_LOG("Found shader \"%s\"\n",
                  global->shader_dir.list->elems[i].data);
            if (!strcmp(settings->video.shader_path,
                        global->shader_dir.list->elems[i].data))
               global->shader_dir.ptr = i;
         }
         break;
      }
      case EVENT_CMD_SAVEFILES:
         event_save_files();
         break;
      case EVENT_CMD_SAVEFILES_DEINIT:
         if (!global)
            break;

         if (global->savefiles)
            string_list_free(global->savefiles);
         global->savefiles = NULL;
         break;
      case EVENT_CMD_SAVEFILES_INIT:
         global->use_sram = global->use_sram && !global->sram_save_disable
#ifdef HAVE_NETPLAY
            && (!driver->netplay_data || !global->netplay_is_client)
#endif
            ;

         if (!global->use_sram)
            RARCH_LOG("SRAM will not be saved.\n");

         if (global->use_sram)
            event_command(EVENT_CMD_AUTOSAVE_INIT);
         break;
      case EVENT_CMD_MSG_QUEUE_DEINIT:
         rarch_main_msg_queue_free();
         break;
      case EVENT_CMD_MSG_QUEUE_INIT:
         event_command(EVENT_CMD_MSG_QUEUE_DEINIT);
         rarch_main_msg_queue_init();
         rarch_main_data_init_queues();
         break;
      case EVENT_CMD_NETPLAY_TOGGLE:
#ifdef HAVE_NETPLAY
         if (driver->netplay_data && !global->netplay_enable)
         {
            event_command(EVENT_CMD_NETPLAY_DEINIT);
            rarch_main_msg_queue_push("Netplay has disconnected."
                                      " Will continue without connection.",
                                      0, 480, false);
         }  /* else, init on next launch */
#endif
         break;  
            
      case EVENT_CMD_NETPLAY_DEINIT:
#ifdef HAVE_NETPLAY
         deinit_netplay();
#endif
         break;
      case EVENT_CMD_NETWORK_DEINIT:
#ifdef HAVE_NETWORKING
         network_deinit();
#endif
         break;
      case EVENT_CMD_NETWORK_INIT:
#ifdef HAVE_NETWORKING
         network_init();
#endif
         break;
      case EVENT_CMD_NETPLAY_INIT:
         event_command(EVENT_CMD_NETPLAY_DEINIT);
#ifdef HAVE_NETPLAY
         if (!init_netplay())
            return false;
#endif
         break;
      case EVENT_CMD_NETPLAY_FLIP_PLAYERS:
#ifdef HAVE_NETPLAY
         {
            netplay_t *netplay = (netplay_t*)driver->netplay_data;
            if (!netplay)
               return false;
            netplay_flip_users(netplay);
         }
#endif
         break;
      case EVENT_CMD_PREEMPT_FRAMES_UPDATE:
         update_preempt_frames();
         break;
      case EVENT_CMD_FULLSCREEN_TOGGLE:
         if (!video_driver_has_windowed())
            return false;

         /* If we go fullscreen we drop all drivers and 
          * reinitialize to be safe. */
         settings->video.fullscreen = !settings->video.fullscreen;
         settings_touched = true;
         event_command(EVENT_CMD_REINIT);
         break;
      case EVENT_CMD_COMMAND_DEINIT:
#ifdef HAVE_COMMAND
         if (driver->command)
            rarch_cmd_free(driver->command);
         driver->command = NULL;
#endif
         break;
      case EVENT_CMD_COMMAND_INIT:
         event_command(EVENT_CMD_COMMAND_DEINIT);

#ifdef HAVE_COMMAND
         event_init_command();
#endif
         break;
      case EVENT_CMD_TEMPORARY_CONTENT_DEINIT:
         if (!global)
            break;

         if (global->temporary_content)
            event_free_temporary_content();
         global->temporary_content = NULL;
         break;
      case EVENT_CMD_SUBSYSTEM_FULLPATHS_DEINIT:
         if (!global)
            break;

         if (global->subsystem_fullpaths)
            string_list_free(global->subsystem_fullpaths);
         global->subsystem_fullpaths = NULL;
         break;
      case EVENT_CMD_LOG_FILE_DEINIT:
         if (!global)
            break;

         if (global->log_file && global->log_file != stderr)
            fclose(global->log_file);
         global->log_file = NULL;
         break;
      case EVENT_CMD_DISK_EJECT_TOGGLE:
         if (global->system.disk_control.get_num_images)
         {
            const struct retro_disk_control_callback *control = 
               (const struct retro_disk_control_callback*)
               &global->system.disk_control;

            if (control)
            {
               event_check_disk_eject(control);
               if (menu_driver_alive())
                  rarch_main_set_state(RARCH_ACTION_STATE_MENU_RUNNING_FINISHED);
            }
         }
         else
            rarch_main_msg_queue_push("Core does not support Disc Control.", 1, 120, true);
         break;
      case EVENT_CMD_DISK_NEXT:
         if (global->system.disk_control.get_num_images)
         {
            const struct retro_disk_control_callback *control = 
               (const struct retro_disk_control_callback*)
               &global->system.disk_control;

            if (!control)
               return false;

            if (!control->get_eject_state())
               return false;

            event_check_disk_next(control);
         }
         else
            rarch_main_msg_queue_push("Core does not support Disc Control.", 1, 120, true);
         break;
      case EVENT_CMD_DISK_PREV:
         if (global->system.disk_control.get_num_images)
         {
            const struct retro_disk_control_callback *control = 
               (const struct retro_disk_control_callback*)
               &global->system.disk_control;

            if (!control)
               return false;

            if (!control->get_eject_state())
               return false;

            event_check_disk_prev(control);
         }
         else
            rarch_main_msg_queue_push("Core does not support Disk Control.", 1, 120, true);
         break;
      case EVENT_CMD_RUMBLE_STOP:
         for (i = 0; i < MAX_USERS; i++)
         {
            input_driver_set_rumble_state(i, RETRO_RUMBLE_STRONG, 0);
            input_driver_set_rumble_state(i, RETRO_RUMBLE_WEAK, 0);
         }
         break;
      case EVENT_CMD_GRAB_MOUSE_TOGGLE:
         {
            static bool grab_mouse_state  = false;

            grab_mouse_state = !grab_mouse_state;

            if (!driver->input || !input_driver_grab_mouse(grab_mouse_state))
               return false;

            RARCH_LOG("Grab mouse state: %s.\n",
                  grab_mouse_state ? "yes" : "no");

            video_driver_show_mouse(!grab_mouse_state);
         }
         break;
      case EVENT_CMD_PERFCNT_REPORT_FRONTEND_LOG:
         rarch_perf_log();
         break;
      case EVENT_CMD_VOLUME_UP:
         event_set_volume(0.5f);
         break;
      case EVENT_CMD_VOLUME_DOWN:
         event_set_volume(-0.5f);
         break;
      case EVENT_CMD_KEYBOARD_FOCUS_TOGGLE:
         if (global->keyboard_focus)
         {
            global->keyboard_focus = false;
            rarch_main_msg_queue_push("Keyboard Focus Disabled", 1, 120, true);
         }
         else
         {
            global->keyboard_focus = true;
            rarch_main_msg_queue_push("Keyboard Focus Enabled", 1, 120, true);
         }
         RARCH_LOG("Keyboard Focus %s.\n",
               global->keyboard_focus ? "Enabled" : "Disabled");

         if (!menu_driver_alive())
            input_driver_keyboard_mapping_set_block(global->keyboard_focus);
         break;
      case EVENT_CMD_INPUT_UPDATE_ANALOG_DPAD_PARAMS:
         input_joypad_update_analog_dpad_params();
         break;
      case EVENT_CMD_NONE:
      default:
         goto error;
   }

   return true;

error:
   return false;
}
