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
#include <string/stdstring.h>

#include "../general.h"
#include "../dynamic.h"
#include <libretro.h>
#include "input_keymaps.h"
#include "input_joypad_to_keyboard.h"

#define DEFAULT_NUM_REMAPS 20

unsigned input_remapping_scope = GLOBAL;
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

#define NUM_REMAP_BTNS 25
#define NUM_DIGITAL_REMAP_BTNS 17

/* Defines the order of selectable buttons */
const unsigned input_remapping_btn_order[NUM_REMAP_BTNS] = {
   NO_BTN,
   RETRO_DEVICE_ID_JOYPAD_A,
   RETRO_DEVICE_ID_JOYPAD_B,
   RETRO_DEVICE_ID_JOYPAD_X,
   RETRO_DEVICE_ID_JOYPAD_Y,
   RETRO_DEVICE_ID_JOYPAD_SELECT,
   RETRO_DEVICE_ID_JOYPAD_START,
   RETRO_DEVICE_ID_JOYPAD_LEFT,
   RETRO_DEVICE_ID_JOYPAD_RIGHT,
   RETRO_DEVICE_ID_JOYPAD_UP,
   RETRO_DEVICE_ID_JOYPAD_DOWN,
   RETRO_DEVICE_ID_JOYPAD_L,
   RETRO_DEVICE_ID_JOYPAD_R,
   RETRO_DEVICE_ID_JOYPAD_L2,
   RETRO_DEVICE_ID_JOYPAD_R2,
   RETRO_DEVICE_ID_JOYPAD_L3,
   RETRO_DEVICE_ID_JOYPAD_R3,
   RARCH_ANALOG_LEFT_X_MINUS,
   RARCH_ANALOG_LEFT_X_PLUS,
   RARCH_ANALOG_LEFT_Y_MINUS,
   RARCH_ANALOG_LEFT_Y_PLUS,
   RARCH_ANALOG_RIGHT_X_MINUS,
   RARCH_ANALOG_RIGHT_X_PLUS,
   RARCH_ANALOG_RIGHT_Y_MINUS,
   RARCH_ANALOG_RIGHT_Y_PLUS
};

unsigned input_remapping_next_id(unsigned id, bool digital_only)
{
   const int max_i = digital_only
         ? NUM_DIGITAL_REMAP_BTNS - 1
         : NUM_REMAP_BTNS - 1;
   int i = max_i;

   /* Find index of id. Default to 0 (NO_BTN) */
   while (i && id != input_remapping_btn_order[i])
      i--;

   /* Get next */
   return input_remapping_btn_order[min(i + 1, max_i)];
}

unsigned input_remapping_prev_id(unsigned id, bool digital_only)
{
   int i = digital_only
         ? NUM_DIGITAL_REMAP_BTNS - 1
         : NUM_REMAP_BTNS - 1;

   /* Find index of id. Default to 0 (NO_BTN) */
   while (i && id != input_remapping_btn_order[i])
      i--;

   /* Get previous */
   return input_remapping_btn_order[max(i - 1, 0)];
}

unsigned input_remapping_last_id(bool digital_only)
{
   const int num_btns = digital_only
         ? NUM_DIGITAL_REMAP_BTNS
         : NUM_REMAP_BTNS;

   return input_remapping_btn_order[num_btns - 1];
}

/**
 * input_remapping_load_file:
 * @path                    : Path to remapping file (absolute path).
 *
 * Loads a remap file from disk to memory.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool input_remapping_load_file(const char *path)
{
   unsigned i, j, k;
   config_file_t *conf        = config_file_new(path);
   global_t      *global      = global_get_ptr();
   struct input_struct *input = &config_get_ptr()->input;
   char buf[32];

   if (!conf)
      return false;

   strlcpy(input->remapping_path, path, sizeof(input->remapping_path));

   /* Libretro devices - load if from same core */
   if (strstr(path, global->libretro_name) > 0)
   {
      for (i = 0; i < MAX_USERS; i++)
      {
         snprintf(buf, sizeof(buf), "input_libretro_device_p%u", i + 1);
         config_get_uint(conf, buf, &input->libretro_device[i]);

         if (global->main_is_init && i < input->max_users)
            core_set_controller_port_device(i, input->libretro_device[i]);
      }
   }

   /* RetroPad remaps */
   for (i = 0; i < MAX_USERS; i++)
   {
      char key_ident[32];
      char key_strings[RARCH_FIRST_CUSTOM_BIND + 4][8] = {
         "b", "y", "select", "start", "up", "down", "left", "right", "a", "x",
         "l", "r", "l2", "r2", "l3", "r3",
         "l_x", "l_y", "r_x", "r_y" };
      int turbo_count    = 0;

      snprintf(buf, sizeof(buf), "input_player%u", i + 1);
      for (j = 0; j < RARCH_FIRST_CUSTOM_BIND + 4; j++)
      {
         int key_remap = -1;

         snprintf(key_ident, sizeof(key_ident), "%s_%s", buf, key_strings[j]);
         if (config_get_int(conf, key_ident, &key_remap))
            input->remap_ids[i][j] = key_remap;
      }

      /* Custom axis remaps */
      for(k = 0; k < 4; k++)
      {
         int key_remap = -1;
         j = RARCH_FIRST_CUSTOM_BIND + k;

         snprintf(key_ident, sizeof(key_ident), "%s_%s-", buf, key_strings[j]);
         if (config_get_int(conf, key_ident, &key_remap))
            input->custom_axis_ids[i][k][0] = key_remap;

         snprintf(key_ident, sizeof(key_ident), "%s_%s+", buf, key_strings[j]);
         if (config_get_int(conf, key_ident, &key_remap))
            input->custom_axis_ids[i][k][1] = key_remap;
      }

      /* RetroPad turbo mapping */
      for (j = 0; j < RARCH_FIRST_CUSTOM_BIND; j++)
      {
         int key_map = -1;

         if ( !((1 << j) & TURBO_ID_MASK) )
            continue;

         snprintf(key_ident, sizeof(key_ident), "%s_%s_turbo", buf,
               key_strings[j]);
         if (config_get_int(conf, key_ident, &key_map))
         {
            input->turbo_remap_id[i] = key_map;

            if (!turbo_count++)
               input->turbo_id[i] = j;
            else
            {
               input->turbo_id[i] = TURBO_ID_ALL;
               input->turbo_remap_id[i] = NO_BTN;
               break;
            }
         }
      }
   }

   /* RetroPad to Keyboard mapping */
   for (i = 0; i < JOYKBD_LIST_LEN; i++)
   {
      char rk_buf[32];
      enum retro_key rk = joykbd_bind_list[i].rk;
      int joy_id        = NO_BTN;

      input_keymaps_translate_rk_to_str(rk, rk_buf, sizeof(rk_buf));
      snprintf(buf, sizeof(buf), "input_keyboard_%s", rk_buf);

      if (config_get_int(conf, buf, &joy_id)
            && joy_id < NUM_JOYKBD_BTNS)
         input_joykbd_add_bind(rk, joy_id);
   }

   config_file_free(conf);

   input_joykbd_update_enabled();
   input_remapping_touched = true;
   return true;
}

void remap_file_load_auto(void)
{
   settings_t *settings = config_get_ptr();
   char *path;
   
   if (!settings)
      return;

   path = string_alloc(PATH_MAX_LENGTH);
   input_remapping_set_defaults();
   settings->input.remapping_path[0] = '\0';

   /* Look for ROM, Directory, then Core specific remap */
   input_remapping_get_path(path, THIS_CONTENT_ONLY);
   if (input_remapping_load_file(path))
   {
      input_remapping_scope = THIS_CONTENT_ONLY;
      goto end;
   }

   input_remapping_get_path(path, THIS_CONTENT_DIR);
   if (input_remapping_load_file(path))
   {
      input_remapping_scope = THIS_CONTENT_DIR;
      goto end;
   }

   input_remapping_get_path(path, THIS_CORE);
   input_remapping_load_file(path);
   input_remapping_scope = THIS_CORE;

end:
   input_remapping_touched = false;
   free(path);
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
   unsigned i, j, k;
   char buf[32];
   config_file_t *conf        = NULL;
   struct input_struct *input = &config_get_ptr()->input;
   bool turbo_all;

   if (!(conf = config_file_new(NULL)))
      return false;

   RARCH_LOG("Saving remap at path: \"%s\"\n", path);

   /* Libretro devices */
   for (i = 0; i < input->max_users; i++)
   {
      if (input->libretro_device[i] == RETRO_DEVICE_JOYPAD)
         continue;  /* skip unmodified */

      snprintf(buf, sizeof(buf), "input_libretro_device_p%u", i + 1);
      config_set_int(conf, buf, input->libretro_device[i]);
   }

   /* RetroPad remaps */
   for (i = 0; i < input->max_users; i++)
   {
      char key_ident[32];
      char key_strings[RARCH_FIRST_CUSTOM_BIND + 4][8] = {
         "b", "y", "select", "start", "up", "down", "left", "right", "a", "x",
         "l", "r", "l2", "r2", "l3", "r3",
         "l_x", "l_y", "r_x", "r_y" };

      snprintf(buf, sizeof(buf), "input_player%u", i + 1);
      for (j = 0; j < RARCH_FIRST_CUSTOM_BIND + 4; j++)
      {
         if (input->remap_ids[i][j] == (j < RARCH_FIRST_CUSTOM_BIND ?
               input->binds[i][j].id : j - RARCH_FIRST_CUSTOM_BIND))
            continue;  /* skip unmodified */

         snprintf(key_ident, sizeof(key_ident), "%s_%s", buf, key_strings[j]);
         config_set_int(conf, key_ident, input->remap_ids[i][j]);
      }

      /* Custom axis remaps */
      for(k = 0; k < 4; k++)
      {
         j = RARCH_FIRST_CUSTOM_BIND + k;

         if (input->custom_axis_ids[i][k][0] < RARCH_FIRST_CUSTOM_BIND)
         {
            snprintf(key_ident, sizeof(key_ident), "%s_%s-",
                  buf, key_strings[j]);
            config_set_int(conf, key_ident, input->custom_axis_ids[i][k][0]);
         }

         if (input->custom_axis_ids[i][k][1] < RARCH_FIRST_CUSTOM_BIND)
         {
            snprintf(key_ident, sizeof(key_ident), "%s_%s+",
                  buf, key_strings[j]);
            config_set_int(conf, key_ident, input->custom_axis_ids[i][k][1]);
         }
      }

      /* RetroPad turbo mapping */
      turbo_all = (input->turbo_id[i] == TURBO_ID_ALL);
      for (j = 0; j < RARCH_FIRST_CUSTOM_BIND; j++)
      {
         if ( !((1 << j) & TURBO_ID_MASK) )
            continue;

         snprintf(key_ident, sizeof(key_ident),
               "%s_%s_turbo", buf, key_strings[j]);

         if (!turbo_all && j == input->turbo_id[i])
            config_set_int(conf, key_ident, input->turbo_remap_id[i]);
         else if (turbo_all && input->remap_ids[i][j] < RARCH_FIRST_CUSTOM_BIND)
            config_set_int(conf, key_ident, input->remap_ids[i][j]);
      }
   }

   /* RetroPad to Keyboard mapping */
   for (i = 0; i < JOYKBD_LIST_LEN; i++)
   {
      char rk_buf[32];
      enum retro_key rk = joykbd_bind_list[i].rk;
      uint16_t btn      = joykbd_bind_list[i].btn;

      if (btn < NUM_JOYKBD_BTNS)
      {
         input_keymaps_translate_rk_to_str(rk, rk_buf, sizeof(rk_buf));
         snprintf(buf, sizeof(buf), "input_keyboard_%s", rk_buf);
         config_set_int(conf, buf, btn);
      }
   }

   ret = config_file_write(conf, path);
   config_file_free(conf);

   if (ret)
      strlcpy(input->remapping_path, path, PATH_MAX_LENGTH);

   return ret;
}

static void input_remapping_delete_unscoped(void)
{
   char *path = string_alloc(PATH_MAX_LENGTH);

   if (input_remapping_scope < THIS_CONTENT_ONLY)
   {
      input_remapping_get_path(path, THIS_CONTENT_ONLY);
      if (path_file_exists(path))
      {
         RARCH_LOG("Removing remap at path: \"%s\"\n", path);
         remove(path);
      }
   }

   if (input_remapping_scope < THIS_CONTENT_DIR)
   {
      input_remapping_get_path(path, THIS_CONTENT_DIR);
      if (path_file_exists(path))
      {
         RARCH_LOG("Removing remap at path: \"%s\"\n", path);
         remove(path);
      }
   }

   free(path);
}

/**
 * input_remapping_save:
 *
 * Saves remapping values to file based on input_remapping_scope.
 * Also deletes remap files as necessary if input_remapping_scope was changed.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool input_remapping_save(void)
{
   char *path = string_alloc(PATH_MAX_LENGTH);

   input_remapping_get_path(path, input_remapping_scope);
   if (input_remapping_save_file(path))
   {
      input_remapping_delete_unscoped();
      input_remapping_touched = false;
      free(path);
      return true;
   }

   free(path);
   return false;
}

void input_remapping_set_defaults(void)
{
   unsigned i, j;
   struct input_struct *input = &config_get_ptr()->input;

   for (i = 0; i < MAX_USERS; i++)
   {
      for (j = 0; j < RARCH_FIRST_CUSTOM_BIND; j++)
         input->remap_ids[i][j] = input->binds[i][j].id;
      for (j = 0; j < 4; j++)
      {
         input->remap_ids[i][RARCH_FIRST_CUSTOM_BIND + j] = j;
         input->custom_axis_ids[i][j][0] = NO_BTN;
         input->custom_axis_ids[i][j][1] = NO_BTN;
      }

      input->turbo_id[i] = NO_BTN;
      input->turbo_remap_id[i] = NO_BTN;
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
            unsigned axis_idx  = (*idx << 1) | *id;
            unsigned remap_idx = RARCH_FIRST_CUSTOM_BIND + axis_idx;
            unsigned remap_val = settings->input.remap_ids[port][remap_idx];

            if (remap_val == RARCH_ANALOG_CUSTOM_AXIS)
            {
               unsigned *const custom_axis_ids =
                     settings->input.custom_axis_ids[port][axis_idx];

               *id  = (custom_axis_ids[0] << 16) | custom_axis_ids[1];
               *idx |= INDEX_FLAG_CUSTOM_AXIS;
            }
            else
            {
               *idx = (remap_val & 0x2) >> 1;
               *id  = (remap_val == NO_BTN) ? NO_BTN : remap_val & 0x1;
            }
         }
         break;
   }
}

void input_remapping_set_default_desc(void)
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
   global_t   *global   = global_get_ptr();
   settings_t *settings = config_get_ptr();
   char filename[NAME_MAX_LENGTH];

   if (!get_scoped_config_filename(filename, scope, "rmp"))
   {
      *path = '\0';
      return;
   }

   fill_pathname_join(path, settings->input_remapping_directory,
         global->libretro_name, PATH_MAX_LENGTH);
   fill_pathname_slash(path, PATH_MAX_LENGTH);
   strlcat(path, filename, PATH_MAX_LENGTH);
}
