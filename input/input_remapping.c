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

#include "input_remapping.h"

#include <rhash.h>
#include <file/config_file.h>
#include <file/file_path.h>

#include "../general.h"
#include "../dynamic.h"
#include "../libretro.h"
#include "input_keymaps.h"
#include "input_joypad_to_keyboard.h"

#define DEFAULT_NUM_REMAPS 20

unsigned input_remapping_scope = THIS_CORE;
bool input_remapping_touched;

const struct retro_input_descriptor default_rid[DEFAULT_NUM_REMAPS] = {
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,   "D-Pad Left" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,     "D-Pad Up" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,   "D-Pad Down" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT,  "D-Pad Right" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,      "B" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,      "A" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,      "X" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,      "Y" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,      "L" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2,     "L2" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3,     "L3" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,      "R" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2,     "R2" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3,     "R3" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START,  "Start" },
   { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X,  "Left Analog X" },
   { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y,  "Left Analog Y" },
   { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X, "Right Analog X" },
   { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y, "Right Analog Y" }
};

/**
 * input_remapping_load_file:
 * @path                     : Path to remapping file (absolute path).
 *
 * Loads a remap file from disk to memory.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool input_remapping_load_file(const char *path)
{
   unsigned i, j;
   config_file_t *conf  = config_file_new(path);
   settings_t *settings = config_get_ptr();

   if (!conf)
      return false;

   strlcpy(settings->input.remapping_path, path,
         sizeof(settings->input.remapping_path));

   for (i = 0; i < MAX_USERS; i++)
   {
      char buf[64]                                       = {0};
      char key_ident[RARCH_FIRST_CUSTOM_BIND + 4][128]   = {{0}};
      char key_strings[RARCH_FIRST_CUSTOM_BIND + 4][128] = { "b", "y", "select", "start",
         "up", "down", "left", "right", "a", "x", "l", "r", "l2", "r2", "l3", "r3", "l_x", "l_y", "r_x", "r_y" };

      snprintf(buf, sizeof(buf), "input_player%u", i + 1);

      for (j = 0; j < RARCH_FIRST_CUSTOM_BIND + 4; j++)
      {
         int key_remap = -1;

         snprintf(key_ident[j], sizeof(key_ident[j]), "%s_%s", buf, key_strings[j]);
         if (config_get_int(conf, key_ident[j], &key_remap) && key_remap < RARCH_FIRST_CUSTOM_BIND)
            settings->input.remap_ids[i][j] = key_remap;
      }

      for (j = 0; j < 4; j++)
      {
         int key_remap = -1;

         snprintf(key_ident[RARCH_FIRST_CUSTOM_BIND + j], sizeof(key_ident[RARCH_FIRST_CUSTOM_BIND + j]), "%s_%s", buf, key_strings[RARCH_FIRST_CUSTOM_BIND + j]);
         if (config_get_int(conf, key_ident[RARCH_FIRST_CUSTOM_BIND + j], &key_remap) && key_remap < 4)
            settings->input.remap_ids[i][RARCH_FIRST_CUSTOM_BIND + j] = key_remap;
      }
   }

   for (j = 0; j < JOYKBD_LIST_LEN; j++)
   {
      char buf[64]      = {0};
      char rk_buf[64]   = {0};
      enum retro_key rk = joykbd_bind_list[j].rk;
      int joy_id        = NO_BTN;

      input_keymaps_translate_rk_to_str(rk, rk_buf, sizeof(rk_buf));
      snprintf(buf, sizeof(buf), "input_keyboard_%s", rk_buf);

      if (config_get_int(conf, buf, &joy_id)
            && joy_id < NUM_JOYKBD_BTNS)
         input_joykbd_add_bind(rk, joy_id);
   }

   config_file_free(conf);

   input_remapping_touched = true;
   return true;
}

int remap_file_load_auto()
{
  /* Look for ROM, Directory, then Core specific remap */
   char path[PATH_MAX_LENGTH] = {0};
   settings_t *settings       = config_get_ptr();
   
   if (!settings)
      return 0;

   input_remapping_get_path(path, THIS_CONTENT_ONLY);
   if (path_file_exists(path))
   {
      input_remapping_scope = THIS_CONTENT_ONLY;
      goto load_remap;
   }

   input_remapping_get_path(path, THIS_CONTENT_DIR);
   if (path_file_exists(path))
   {
      input_remapping_scope = THIS_CONTENT_DIR;
      goto load_remap;
   }

   input_remapping_get_path(path, THIS_CORE);
   if (path_file_exists(path))
   {
      input_remapping_scope = THIS_CORE;
      goto load_remap;
   }

load_remap:
   input_remapping_set_defaults();

   if (!path_file_exists(path) || !input_remapping_load_file(path))
   {
      settings->input.remapping_path[0] = '\0';
      input_remapping_scope = THIS_CORE;
   }

   input_remapping_touched = false;
   return 0;
}

/**
 * input_remapping_save_file:
 * @path                    : Path to remapping file.
 *
 * Saves remapping values to file.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
static bool input_remapping_save_file(const char *path)
{
   bool ret;
   unsigned i, j;
   char buf[PATH_MAX_LENGTH] = {0};
   config_file_t *conf       = NULL;
   settings_t    *settings   = config_get_ptr();

   conf = config_file_new(path);

   if (!conf)
   {
      conf = config_file_new(NULL);
	  if (!conf)
	     return false;
   }

   for (i = 0; i < settings->input.max_users; i++)
   {
      char key_ident[RARCH_FIRST_CUSTOM_BIND + 4][128]   = {{0}};
      char key_strings[RARCH_FIRST_CUSTOM_BIND + 4][128] = { "b", "y", "select", "start",
         "up", "down", "left", "right", "a", "x", "l", "r", "l2", "r2", "l3", "r3", "l_x", "l_y", "r_x", "r_y" };

      snprintf(buf, sizeof(buf), "input_player%u", i + 1);

      for (j = 0; j < RARCH_FIRST_CUSTOM_BIND + 4; j++)
      {
         snprintf(key_ident[j], sizeof(key_ident[j]), "%s_%s", buf, key_strings[j]);
         config_set_int(conf, key_ident[j], settings->input.remap_ids[i][j]);
      }
   }

   for (j = 0; j < JOYKBD_LIST_LEN; j++)
   {
      char rk_buf[64]   = {0};
      enum retro_key rk = joykbd_bind_list[j].rk;
      uint16_t btn      = joykbd_bind_list[j].btn;

      input_keymaps_translate_rk_to_str(rk, rk_buf, sizeof(rk_buf));
      snprintf(buf, sizeof(buf), "input_keyboard_%s", rk_buf);

      if (btn < NUM_JOYKBD_BTNS)
         config_set_int(conf, buf, btn);
      else
         config_remove_entry(conf, buf);
   }

   ret = config_file_write(conf, path);
   config_file_free(conf);
   
   if (ret)
      strlcpy(settings->input.remapping_path, path, PATH_MAX_LENGTH);

   return ret;
}

static void input_remapping_delete_unscoped()
{
   char path[PATH_MAX_LENGTH] = {0};

   if (input_remapping_scope < THIS_CONTENT_ONLY)
   {
      input_remapping_get_path(path, THIS_CONTENT_ONLY);
      remove(path);
   }

   if (input_remapping_scope < THIS_CONTENT_DIR)
   {
      input_remapping_get_path(path, THIS_CONTENT_DIR);
      remove(path);
   }
}

/**
 * input_remapping_save:
 *
 * Saves remapping values to file based on input_remapping_scope.
 * Also deletes remap files as necessary if input_remapping_scope was changed.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool input_remapping_save()
{
   char path[PATH_MAX_LENGTH] = {0};

   input_remapping_get_path(path, input_remapping_scope);
   if (input_remapping_save_file(path))
   {
      input_remapping_delete_unscoped();
      input_remapping_touched = false;
      return true;
   }

   return false;
}

void input_remapping_set_defaults(void)
{
   unsigned i, j;
   settings_t *settings = config_get_ptr();

   for (i = 0; i < MAX_USERS; i++)
   {
      for (j = 0; j < RARCH_FIRST_CUSTOM_BIND; j++)
         settings->input.remap_ids[i][j] = settings->input.binds[i][j].id;
      for (j = 0; j < 4; j++)
         settings->input.remap_ids[i][RARCH_FIRST_CUSTOM_BIND + j] = j;
   }

   input_joykbd_init_binds();
}

void input_remapping_state(unsigned port,
      unsigned *device, unsigned *idx, unsigned *id)
{
   settings_t *settings = config_get_ptr();

   switch (*device)
   {
      case RETRO_DEVICE_JOYPAD:
         if (*id < RARCH_FIRST_CUSTOM_BIND)
            *id = settings->input.remap_ids[port][*id];
         break;
      case RETRO_DEVICE_ANALOG:
         if (*idx < 2 && *id < 2)
         {
            unsigned new_id = RARCH_FIRST_CUSTOM_BIND + (*idx * 2 + *id);

            new_id = settings->input.remap_ids[port][new_id];
            *idx   = (new_id & 2) >> 1;
            *id    = new_id & 1;
         }
         break;
   }
}

void input_remapping_set_default_desc()
{
   struct retro_input_descriptor desc[MAX_USERS * sizeof(default_rid) + 1];
   unsigned i, j;

   for (i = 0; i < MAX_USERS; i++)
   {
      memcpy(&desc[i * DEFAULT_NUM_REMAPS], default_rid, sizeof(default_rid));
      for (j = 0; j < DEFAULT_NUM_REMAPS; j++)
         desc[j + i * DEFAULT_NUM_REMAPS].port = i;
   }
   memset(&desc[MAX_USERS * DEFAULT_NUM_REMAPS],
          0, sizeof(struct retro_input_descriptor));

   rarch_environment_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);
}

void input_remapping_get_path(char* path, unsigned scope)
{
   global_t *global     = global_get_ptr();
   settings_t *settings = config_get_ptr();
   char buf[NAME_MAX_LENGTH];

   if (!global || !settings)
      return;

   fill_pathname_join(path, settings->input_remapping_directory,
         global->libretro_name, PATH_MAX_LENGTH);
   strlcat(path, path_default_slash(), PATH_MAX_LENGTH);

   switch(scope)
   {
      case THIS_CONTENT_ONLY:
         strlcat(path, path_basename(global->basename), PATH_MAX_LENGTH);
         break;

      case THIS_CONTENT_DIR:
         if (!path_parent_dir_name(buf, global->basename))
            strlcpy(buf, "root", NAME_MAX_LENGTH);
         strlcat(path, buf, PATH_MAX_LENGTH);
         break;

      case THIS_CORE:
         strlcat(path, global->libretro_name, PATH_MAX_LENGTH);
         break;

      default:
         return;
   }

   strlcat(path, ".rmp", PATH_MAX_LENGTH);
}

