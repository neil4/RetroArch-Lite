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

#define MAX_REMAP_DESCS 20

const struct retro_input_descriptor default_rid[MAX_REMAP_DESCS] = {
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

   config_file_free(conf);

   return true;
}

int remap_file_load_auto()
{
  /* Look for ROM, Directory, then Core specific remap */
   char directory[PATH_MAX_LENGTH]  = {0};
   char buf[NAME_MAX_LENGTH]        = {0};
   char fullpath[PATH_MAX_LENGTH]   = {0};
   global_t *global                 = global_get_ptr();
   settings_t *settings             = config_get_ptr();
   
   if (!global || !settings)
      return 0;

   fill_pathname_join(directory, settings->input_remapping_directory,
                      global->libretro_name, PATH_MAX_LENGTH);

   /* ROM remap path */
   fill_pathname_join(fullpath, directory,
                      path_basename(global->basename), PATH_MAX_LENGTH);
   strlcat(fullpath, ".rmp", PATH_MAX_LENGTH);

   if(!path_file_exists(fullpath))
   {
      /* Directory remap path */
      if (!path_parent_dir_name(buf, global->basename))
         strcpy(buf, "root");
      fill_pathname_join(fullpath, directory, buf, PATH_MAX_LENGTH);
      strlcat(fullpath, ".rmp", PATH_MAX_LENGTH);

      if(!path_file_exists(fullpath))
      {
         /* Core remap path */
         fill_pathname_join(fullpath, directory,
                            global->libretro_name, PATH_MAX_LENGTH);
         strlcat(fullpath, ".rmp", PATH_MAX_LENGTH);
      }
   }
   
   if ( path_file_exists(fullpath)
        && input_remapping_load_file(fullpath) )
      strlcpy(settings->input.remapping_path, fullpath, PATH_MAX_LENGTH);
   else
   {  /* fall back to default mapping */
      settings->input.remapping_path[0] = '\0';
      input_remapping_set_defaults();
   }

   return 0;
}

/**
 * input_remapping_save_file:
 * @path                     : Path to remapping file (relative path).
 *
 * Saves remapping values to file.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool input_remapping_save_file(const char *path)
{
   bool ret;
   unsigned i, j;
   char buf[PATH_MAX_LENGTH]         = {0};
   char remap_file[PATH_MAX_LENGTH]  = {0};
   config_file_t               *conf = NULL;
   settings_t              *settings = config_get_ptr();

   fill_pathname_join(buf, settings->input_remapping_directory,
         path, sizeof(buf));

   fill_pathname_noext(remap_file, buf, ".rmp", sizeof(remap_file));

   conf = config_file_new(remap_file);

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

   ret = config_file_write(conf, remap_file);
   config_file_free(conf);
   
   if (ret)
      strlcpy(settings->input.remapping_path, remap_file, PATH_MAX_LENGTH);

   return ret;
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
      memcpy(&desc[i * MAX_REMAP_DESCS], default_rid, sizeof(default_rid));
      for (j = 0; j < MAX_REMAP_DESCS; j++)
         desc[j + i * MAX_REMAP_DESCS].port = i;
   }
   memset(&desc[MAX_USERS * MAX_REMAP_DESCS],
          0, sizeof(struct retro_input_descriptor));

   rarch_environment_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);
}
