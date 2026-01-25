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

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "menu.h"
#include "menu_input.h"
#include "menu_display.h"
#include "menu_entry.h"
#include "menu_setting.h"
#include "menu_shader.h"
#include "menu_navigation.h"
#include "menu_hash.h"

#include "../general.h"
#include "../cheats.h"
#include "../performance.h"
#include "../input/input_joypad.h"
#include "../input/input_remapping.h"
#include "../input/input_common.h"

static unsigned menu_input_mouse_action(
      retro_input_t *input, retro_input_t *trigger_input);
static unsigned menu_input_pointer_action(void);

menu_input_t *menu_input_get_ptr(void)
{
   menu_handle_t *menu = menu_driver_get_ptr();
   if (!menu)
      return NULL;
   return &menu->input;
}

void menu_input_key_start_line(const char *label,
      const char *label_setting, unsigned type, unsigned idx,
      input_keyboard_line_complete_t cb)
{
   menu_handle_t    *menu   = menu_driver_get_ptr();
   menu_input_t *menu_input = menu_input_get_ptr();
   if (!menu || !menu_input)
      return;

   menu_input->keyboard.display       = true;
   menu_input->keyboard.label         = label;
   strlcpy(menu_input->keyboard.label_setting, label_setting,
         sizeof(menu_input->keyboard.label_setting));
   menu_input->keyboard.type          = type;
   menu_input->keyboard.idx           = idx;
   menu_input->keyboard.buffer        = input_keyboard_start_line(menu, cb);
}

static void menu_input_key_end_line(void)
{
   driver_t *driver         = driver_get_ptr();
   menu_input_t *menu_input = menu_input_get_ptr();
   if (!menu_input)
      return;

   menu_input->keyboard.display          = false;
   menu_input->keyboard.label            = NULL;
   menu_input->keyboard.label_setting[0] = '\0';

   /* Avoid triggering states on pressing return. */
   driver->flushing_input = true;
}

static bool menu_input_search_callback(void *userdata, const char *str)
{
   size_t idx = 0;
   menu_list_t *menu_list = menu_list_get_ptr();
   menu_navigation_t *nav = menu_navigation_get_ptr();

   if (!menu_list || !nav)
      return true;

   if (str && *str && file_list_search(menu_list->selection_buf, str, &idx))
         menu_navigation_set(nav, idx, true);

   menu_input_key_end_line();
   return true;
}

bool menu_input_st_uint_callback(void *userdata, const char *str)
{
   menu_input_t *menu_input = menu_input_get_ptr();

   if (!menu_input)
      return true;

   if (str && *str)
   {
      rarch_setting_t *current_setting = NULL;
      if ((current_setting = menu_setting_find(menu_input->keyboard.label_setting)))
         setting_set_with_string_representation(current_setting, str);
   }

   menu_input_key_end_line();
   return true;
}

bool menu_input_st_hex_callback(void *userdata, const char *str)
{
   menu_input_t *menu_input = menu_input_get_ptr();

   if (!menu_input)
      return true;

   if (str && *str)
   {
      rarch_setting_t *current_setting = NULL;
      if ((current_setting = menu_setting_find(menu_input->keyboard.label_setting)))
      {
         if (str[0] == '#')
            str++;
         *current_setting->value.unsigned_integer = strtoul(str, NULL, 16);
      }
   }

   menu_input_key_end_line();
   return true;
}

bool menu_input_st_string_callback(void *userdata, const char *str)
{
   menu_input_t *menu_input = menu_input_get_ptr();

   if (!menu_input)
      return true;

   if (str && *str)
   {
      global_t *global = global_get_ptr();
      rarch_setting_t *current_setting = menu_setting_find(menu_input->keyboard.label_setting);

      if (current_setting)
      {
         setting_set_with_string_representation(current_setting, str);
         menu_setting_generic(current_setting, false);
      }
      else
      {
         uint32_t hash_label = menu_hash_calculate(menu_input->keyboard.label_setting);

         switch (hash_label)
         {
            case MENU_LABEL_VIDEO_SHADER_PRESET_SAVE_AS:
               menu_shader_manager_save_preset(str, true);
               break;
            case MENU_LABEL_CHEAT_FILE_SAVE_AS:
               cheat_manager_save(global->cheat, str);
               break;
         }
      }
   }

   menu_input_key_end_line();
   return true;
}

bool menu_input_st_cheat_callback(void *userdata, const char *str)
{
   global_t *global         = global_get_ptr();
   cheat_manager_t *cheat   = global ? global->cheat : NULL;
   menu_input_t *menu_input = menu_input_get_ptr();

   (void)userdata;

   if (!menu_input || !cheat)
      return true;

   if (cheat && str && *str)
   {
      unsigned cheat_index = menu_input->keyboard.type - MENU_SETTINGS_CHEAT_BEGIN;

      cheat->cheats[cheat_index].code  = strdup(str);
      cheat->cheats[cheat_index].state = false;
   }

   /* Now ask for cheat name */
   menu_input->keyboard.label  = "Enter Cheat Name";
   menu_input->keyboard.buffer = input_keyboard_start_line(
         menu_driver_get_ptr(), menu_input_st_cheatname_callback);

   return false;
}

bool menu_input_st_cheatname_callback(void *userdata, const char *str)
{
   global_t *global         = global_get_ptr();
   cheat_manager_t *cheat   = global ? global->cheat : NULL;
   menu_input_t *menu_input = menu_input_get_ptr();

   (void)userdata;

   if (!menu_input || !cheat)
      return true;

   if (cheat && str && *str)
   {
      unsigned cheat_index = menu_input->keyboard.type - MENU_SETTINGS_CHEAT_BEGIN;
      cheat->cheats[cheat_index].desc = strdup(str);
   }

   menu_input_key_end_line();
   menu_entries_set_refresh();
   return true;
}

void menu_input_search_start(void)
{
   menu_handle_t      *menu = menu_driver_get_ptr();
   menu_input_t *menu_input = menu_input_get_ptr();
   if (!menu || !menu_input)
      return;

   menu_input->keyboard.display = true;
   menu_input->keyboard.label = "Search: ";
   menu_input->keyboard.buffer =
      input_keyboard_start_line(menu, menu_input_search_callback);
}

void menu_input_key_event(bool down, unsigned keycode,
      uint32_t character, uint16_t mod)
{
   (void)down;
   (void)keycode;
   (void)mod;
}

static void menu_input_poll_bind_joypad_state(struct menu_bind_state *state,
                                              bool return_skips)
{
   unsigned i, b, a, h, pad;
   const input_device_driver_t *joypad = input_driver_get_joypad_driver();
   settings_t *settings                = config_get_ptr();
   const unsigned *map                 = settings->input.joypad_map;
   char (*device_names)[64]            = settings->input.device_names;

   if (!state)
      return;
   
   memset(state->state, 0, sizeof(state->state));
   state->skip = (return_skips
         ? input_driver_state(NULL, 0, RETRO_DEVICE_KEYBOARD, 0, RETROK_RETURN)
         : false);

   if (!joypad)
   {
      RARCH_ERR("Cannot poll raw joypad state.");
      return;
   }

   if (joypad->poll)
      joypad->poll();

   for (i = 0, pad = map[i]; *device_names[pad]; pad = map[++i])
   {
      for (b = 0; b < MENU_MAX_BUTTONS; b++)
         state->state[pad].buttons[b] = input_joypad_button_raw(joypad, pad, b);

      for (a = 0; a < MENU_MAX_AXES; a++)
         state->state[pad].axes[a] = input_joypad_axis_raw(joypad, pad, a);

      for (h = 0; h < MENU_MAX_HATS; h++)
      {
         if (input_joypad_hat_raw(joypad, pad, HAT_UP_MASK, h))
            state->state[pad].hats[h] |= HAT_UP_MASK;
         if (input_joypad_hat_raw(joypad, pad, HAT_DOWN_MASK, h))
            state->state[pad].hats[h] |= HAT_DOWN_MASK;
         if (input_joypad_hat_raw(joypad, pad, HAT_LEFT_MASK, h))
            state->state[pad].hats[h] |= HAT_LEFT_MASK;
         if (input_joypad_hat_raw(joypad, pad, HAT_RIGHT_MASK, h))
            state->state[pad].hats[h] |= HAT_RIGHT_MASK;
      }
   }
}

static void menu_input_poll_bind_get_rested_axes(struct menu_bind_state *state)
{
   unsigned i, a, pad;
   const input_device_driver_t *joypad = input_driver_get_joypad_driver();
   settings_t *settings                = config_get_ptr();
   const unsigned *map                 = settings->input.joypad_map;
   char (*device_names)[64]            = settings->input.device_names;

   if (!state)
      return;

   if (!joypad)
   {
      RARCH_ERR("Cannot poll raw joypad state.");
      return;
   }

   for (i = 0, pad = map[i]; *device_names[pad]; pad = map[++i])
      for (a = 0; a < MENU_MAX_AXES; a++)
         state->axis_state[pad].rested_axes[a] =
             input_joypad_axis_raw(joypad, pad, a);
}

static bool menu_input_poll_find_trigger_pad(struct menu_bind_state *state,
      struct menu_bind_state *new_state, unsigned pad)
{
   unsigned a, b, h;
   const struct menu_bind_state_port *n = (const struct menu_bind_state_port*)
         &new_state->state[pad];
   const struct menu_bind_state_port *o = (const struct menu_bind_state_port*)
         &state->state[pad];

   for (b = 0; b < MENU_MAX_BUTTONS; b++)
   {
      bool iterate = n->buttons[b] && !o->buttons[b];

      if (!iterate)
         continue;

      state->target->joykey = b;
      state->target->joyaxis = AXIS_NONE;
      return true;
   }

   /* Axes are a bit tricky ... */
   for (a = 0; a < MENU_MAX_AXES; a++)
   {
      int locked_distance = abs(n->axes[a] -
            new_state->axis_state[pad].locked_axes[a]);
      int rested_distance = abs(n->axes[a] -
            new_state->axis_state[pad].rested_axes[a]);

      if (abs(n->axes[a]) >= 20000 &&
            locked_distance >= 20000 &&
            rested_distance >= 20000)
      {
         /* Take care of case where axis rests on +/- 0x7fff
          * (e.g. 360 controller on Linux) */
         state->target->joyaxis = n->axes[a] > 0 ? AXIS_POS(a) : AXIS_NEG(a);
         state->target->joykey  = NO_BTN;

         /* Lock the current axis */
         new_state->axis_state[pad].locked_axes[a]
               = n->axes[a] > 0 ? 0x7fff : -0x7fff;
         return true;
      }

      if (locked_distance >= 20000) /* Unlock the axis. */
         new_state->axis_state[pad].locked_axes[a] = 0;
   }

   for (h = 0; h < MENU_MAX_HATS; h++)
   {
      uint16_t      trigged = n->hats[h] & (~o->hats[h]);
      uint16_t sane_trigger = 0;

      if (trigged & HAT_UP_MASK)
         sane_trigger = HAT_UP_MASK;
      else if (trigged & HAT_DOWN_MASK)
         sane_trigger = HAT_DOWN_MASK;
      else if (trigged & HAT_LEFT_MASK)
         sane_trigger = HAT_LEFT_MASK;
      else if (trigged & HAT_RIGHT_MASK)
         sane_trigger = HAT_RIGHT_MASK;

      if (sane_trigger)
      {
         state->target->joykey = HAT_MAP(h, sane_trigger);
         state->target->joyaxis = AXIS_NONE;
         return true;
      }
   }

   return false;
}

static bool menu_input_poll_find_trigger(struct menu_bind_state *state,
      struct menu_bind_state *new_state)
{
   settings_t *settings = config_get_ptr();
   const unsigned  *map = settings->input.joypad_map;

   if (!state || !new_state)
      return false;

   if (*settings->input.device_names[map[state->user]]
         && menu_input_poll_find_trigger_pad(state, new_state, map[state->user]))
      return true;

   return false;
}

static bool menu_input_hotkey_bind_keyboard_cb(void *data, unsigned code)
{
   menu_input_t *menu_input = menu_input_get_ptr();

   if (!menu_input)
      return false;
   
   menu_input->binds.target->key = (enum retro_key)code;
   menu_input->binds.begin = menu_input->binds.last + 1;

   return (menu_input->binds.begin <= menu_input->binds.last);
}

static bool menu_input_retropad_bind_keyboard_cb(void *data, unsigned code)
{
   menu_input_t *menu_input = menu_input_get_ptr();
   retro_time_t time_since_cb;
   static retro_time_t last_cb_usec;
   static enum retro_key last_code = RETROK_UNKNOWN;

   if (!menu_input)
      return false;

   time_since_cb = rarch_get_time_usec() - last_cb_usec;
   last_cb_usec = rarch_get_time_usec();

   /* Guard against held or repeated keys */
   if (time_since_cb > 100000 || code != last_code)
   {
      unsigned next_id = input_remapping_next_id(
            menu_input->binds.target->id, false);

      last_code = code;
      menu_input->binds.target->key = (enum retro_key)code;
      menu_input->binds.begin++;
      menu_input->binds.target =
            &config_get_ptr()->input.binds[menu_input->binds.user][next_id];
      menu_input->binds.timeout_end = rarch_get_time_usec() +
         MENU_KEYBOARD_BIND_TIMEOUT_SECONDS_LONG * 1000000;  
   }

   return (menu_input->binds.begin <= menu_input->binds.last);
}

static int menu_input_set_bind_mode_common(rarch_setting_t  *setting,
      enum menu_input_bind_mode type)
{
   struct retro_keybind *keybind = NULL;
   settings_t     *settings      = config_get_ptr();
   menu_list_t        *menu_list = menu_list_get_ptr();
   menu_input_t      *menu_input = menu_input_get_ptr();
   menu_navigation_t       *nav  = menu_navigation_get_ptr();
   menu_displaylist_info_t *info;

   if (!setting)
      return -1;

   switch (type)
   {
      case MENU_INPUT_BIND_NONE:
         return -1;
      case MENU_INPUT_BIND_SINGLE:
         keybind = (struct retro_keybind*)setting->value.keybind;

         if (!keybind)
            return -1;

         info = menu_displaylist_info_new();

         menu_input->binds.begin  = setting->bind_type;
         menu_input->binds.last   = setting->bind_type;
         menu_input->binds.target = keybind;
         menu_input->binds.user   = setting->index_offset;

         info->list          = menu_list->menu_stack;
         info->type          = MENU_SETTINGS_CUSTOM_BIND_KEYBOARD;
         info->directory_ptr = nav->selection_ptr;
         strlcpy(info->label,
               menu_hash_to_str(MENU_LABEL_CUSTOM_BIND), sizeof(info->label));

         menu_displaylist_push_list(info, DISPLAYLIST_INFO);
         free(info);
         break;
      case MENU_INPUT_BIND_ALL:
         info = menu_displaylist_info_new();

         menu_input->binds.target = &settings->input.binds
               [setting->index_offset][input_remapping_btn_order[1]];
         menu_input->binds.begin  = MENU_SETTINGS_BIND_BEGIN;
         menu_input->binds.last   = MENU_SETTINGS_BIND_LAST;
         menu_input->binds.user   = setting->index_offset;

         info->list          = menu_list->menu_stack;
         info->type          = MENU_SETTINGS_CUSTOM_BIND_KEYBOARD;
         info->directory_ptr = nav->selection_ptr;
         strlcpy(info->label,
               menu_hash_to_str(MENU_LABEL_CUSTOM_BIND_ALL),
               sizeof(info->label));

         menu_displaylist_push_list(info, DISPLAYLIST_INFO);
         free(info);
         break;
   }
   return 0;
}

static int menu_input_set_timeout(enum menu_input_bind_mode type)
{
   menu_handle_t      *menu = menu_driver_get_ptr();
   menu_input_t *menu_input = menu_input_get_ptr();
   global_t         *global = global_get_ptr();
   
   int64_t time = (type == MENU_INPUT_BIND_SINGLE ?
                   MENU_KEYBOARD_BIND_TIMEOUT_SECONDS
                   : MENU_KEYBOARD_BIND_TIMEOUT_SECONDS_LONG);

   menu_input->binds.timeout_end = rarch_get_time_usec() + time * 1000000;
   if (type == MENU_INPUT_BIND_SINGLE)  /* hotkey: keyboard or joypad */
      input_keyboard_wait_keys(menu, menu_input_hotkey_bind_keyboard_cb);
   else if (global->menu.bind_mode_keyboard)  /* bind-all: keyboard mode */
      input_keyboard_wait_keys(menu, menu_input_retropad_bind_keyboard_cb);

   return 0;
}

int menu_input_set_keyboard_bind_mode(void *data,
      enum menu_input_bind_mode type)
{
   rarch_setting_t  *setting = (rarch_setting_t*)data;

   if (!setting)
      return -1;
   if (menu_input_set_bind_mode_common(setting, type) == -1)
      return -1;

   return menu_input_set_timeout(type);
}

int menu_input_set_input_device_bind_mode(void *data,
      enum menu_input_bind_mode type)
{
   menu_input_t *menu_input  = menu_input_get_ptr();
   rarch_setting_t  *setting = (rarch_setting_t*)data;

   if (!setting)
      return -1;
   if (menu_input_set_bind_mode_common(setting, type) == -1)
      return -1;

   menu_input_poll_bind_get_rested_axes(&menu_input->binds);
   menu_input_poll_bind_joypad_state(&menu_input->binds, false);

   return menu_input_set_timeout(type);
}

static int menu_input_bind_keyboard_stopcheck(void)
{
   menu_input_t *menu_input = menu_input_get_ptr();
   driver_t         *driver = driver_get_ptr();

   if (!menu_input)
      return -1;

   /* binds.begin is updated in keyboard_press callback. */
   if (menu_input->binds.begin > menu_input->binds.last)
   {
      /* Avoid new binds triggering things right away. */
      driver->flushing_input = true;
      input_keyboard_wait_keys_cancel();
      return 1;
   }

   return 0;
}

int menu_input_bind_iterate(uint32_t label_hash)
{
   int64_t current;
   struct menu_bind_state binds;
   char msg[NAME_MAX_LENGTH];
   char fmt[64];
   int timeout                  = 0;
   menu_input_t *menu_input     = menu_input_get_ptr();
   driver_t *driver             = driver_get_ptr();
   global_t *global             = global_get_ptr();
   bool hotkey_bind             = (label_hash == MENU_LABEL_CUSTOM_BIND);
   bool bind_mode_kb            = global->menu.bind_mode_keyboard && !hotkey_bind;

   static int64_t hold_usec     = 0;

   menu_driver_render();

   current = rarch_get_time_usec();
   timeout = (menu_input->binds.timeout_end - current) / 1000000;
   
   if (timeout <= 0)
   {
      menu_input->binds.begin = menu_input->binds.last + 1;
      input_keyboard_wait_keys_cancel();
      return 1;
   }

   if (hotkey_bind)
      strlcpy(fmt, "[%s]\nPress keyboard or joypad\n \n(timeout %d seconds)", 64);
   else if (bind_mode_kb)
      strlcpy(fmt, "[%s]\nPress keyboard\n \n(timeout %d seconds)", 64);
   else
      strlcpy(fmt, "[%s]\nPress joypad\n \n(RETURN to skip)\n(timeout %d seconds)", 64);

   snprintf(msg, sizeof(msg), fmt, input_config_bind_map[
         menu_input->binds.target->id].desc, timeout);
   menu_driver_render_messagebox(msg);

   if ( (bind_mode_kb || hotkey_bind)
        && menu_input_bind_keyboard_stopcheck() )
      return 1;
   
   /* Hack: Allow user time to let go of the button in odd cases */
   if (hold_usec > 0)
   {
      if (rarch_get_time_usec() > hold_usec)
         hold_usec = 0;
      return 0;
   }

   binds = menu_input->binds;

   input_driver_keyboard_mapping_set_block(true);
   if (!bind_mode_kb)
      menu_input_poll_bind_joypad_state(&binds, !hotkey_bind);

   if ((binds.skip && !menu_input->binds.skip) ||
         menu_input_poll_find_trigger(&menu_input->binds, &binds))
   {
      input_driver_keyboard_mapping_set_block(false);

      /* Avoid new binds triggering things right away. */
      driver->flushing_input = true;
      hold_usec = rarch_get_time_usec() + MENU_INPUT_BIND_HOLD_USEC;

      binds.begin++;
      
      if (hotkey_bind)
         input_keyboard_wait_keys_cancel();

      if (binds.begin > binds.last)
         return 1;

      binds.target = &config_get_ptr()->input.binds[binds.user][
            input_remapping_next_id(binds.target->id, false)];
      binds.timeout_end = rarch_get_time_usec() +
         MENU_KEYBOARD_BIND_TIMEOUT_SECONDS_LONG * 1000000;
   }
   menu_input->binds = binds;

   return 0;
}

static void menu_input_mouse(unsigned *action,
      retro_input_t *input, retro_input_t *trigger_input)
{
   video_viewport_t vp;
   const struct retro_keybind *binds[MAX_USERS];
   driver_t *driver          = driver_get_ptr();
   menu_animation_t *anim    = menu_animation_get_ptr();
   menu_input_t *menu_input  = menu_input_get_ptr();
   menu_framebuf_t *frame_buf= menu_display_fb_get_ptr();
   settings_t *settings      = config_get_ptr();

   static int16_t old_screen_x, old_screen_y;
   static retro_time_t input_usec;

#ifdef HAVE_OVERLAY
   if (driver->overlay && (driver->osk_enable || *settings->input.overlay))
      return;
#endif

   if (!menu_driver_viewport_info(&vp))
      return;

   menu_input->mouse.left       = input_driver_state(binds, 0, RETRO_DEVICE_MOUSE,
         0, RETRO_DEVICE_ID_MOUSE_LEFT);
   menu_input->mouse.right      = input_driver_state(binds, 0, RETRO_DEVICE_MOUSE,
         0, RETRO_DEVICE_ID_MOUSE_RIGHT);
   menu_input->mouse.middle     = input_driver_state(binds, 0, RETRO_DEVICE_MOUSE,
         0, RETRO_DEVICE_ID_MOUSE_MIDDLE);
   menu_input->mouse.btn4       = input_driver_state(binds, 0, RETRO_DEVICE_MOUSE,
         0, RETRO_DEVICE_ID_MOUSE_BUTTON_4);
   menu_input->mouse.btn5       = input_driver_state(binds, 0, RETRO_DEVICE_MOUSE,
         0, RETRO_DEVICE_ID_MOUSE_BUTTON_5);
   menu_input->mouse.wheelup    = input_driver_state(binds, 0, RETRO_DEVICE_MOUSE,
         0, RETRO_DEVICE_ID_MOUSE_WHEELUP);
   menu_input->mouse.wheeldown  = input_driver_state(binds, 0, RETRO_DEVICE_MOUSE,
         0, RETRO_DEVICE_ID_MOUSE_WHEELDOWN);
   menu_input->mouse.hwheelup   = input_driver_state(binds, 0, RETRO_DEVICE_MOUSE,
         0, RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP);
   menu_input->mouse.hwheeldown = input_driver_state(binds, 0, RETRO_DEVICE_MOUSE,
         0, RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN);
   menu_input->mouse.screen_x   = input_driver_state(binds, 0, RETRO_DEVICE_MOUSE,
         0, RETRO_DEVICE_ID_MOUSE_SCREEN_X);
   menu_input->mouse.screen_y   = input_driver_state(binds, 0, RETRO_DEVICE_MOUSE,
         0, RETRO_DEVICE_ID_MOUSE_SCREEN_Y);

   menu_input->mouse.x = ( ((int)menu_input->mouse.screen_x - vp.x)
                           * (int)frame_buf->width ) / (int)vp.width;
   menu_input->mouse.y = ( ((int)menu_input->mouse.screen_y - vp.y)
                           * (int)frame_buf->height ) / (int)vp.height;

   if (!settings->video.fullscreen
       && (menu_input->mouse.x < 0
           || menu_input->mouse.y < 0
           || menu_input->mouse.x > (int)frame_buf->width
           || menu_input->mouse.y > (int)frame_buf->height))
   {
      menu_input->mouse.show = false;
      anim->is_active = true;
      goto end;
   }

   if (menu_input->mouse.x < 5)
      menu_input->mouse.x       = 5;
   if (menu_input->mouse.y < 5)
      menu_input->mouse.y       = 5;
   if (menu_input->mouse.x > (int)frame_buf->width - 5)
      menu_input->mouse.x       = frame_buf->width - 5;
   if (menu_input->mouse.y > (int)frame_buf->height - 5)
      menu_input->mouse.y       = frame_buf->height - 5;

   if ( (menu_input->mouse.screen_x != old_screen_x) ||
        (menu_input->mouse.screen_y != old_screen_y) ||
        menu_input->mouse.left                       ||
        menu_input->mouse.middle                     ||
        menu_input->mouse.btn4                       ||
        menu_input->mouse.btn5                       ||
        menu_input->mouse.wheelup                    ||
        menu_input->mouse.wheeldown                  ||
        menu_input->mouse.hwheelup                   ||
        menu_input->mouse.hwheeldown )
   {
      anim->is_active = true;
      menu_input->mouse.show = true;
      input_usec = rarch_get_time_usec();
   }
   else if (rarch_get_time_usec() > input_usec + 4000000)
   {
      menu_input->mouse.show = false;
      anim->is_active = true;
   }

   end:
   old_screen_x = menu_input->mouse.screen_x;
   old_screen_y = menu_input->mouse.screen_y;

   if (*action == MENU_ACTION_NOOP)
      *action = menu_input_mouse_action(input, trigger_input);
}

static void menu_input_pointer(unsigned *action)
{
   video_viewport_t vp;
   int pointer_x, pointer_y, offset;
   const struct retro_keybind *binds[MAX_USERS] = {NULL};
   menu_input_t *menu_input   = menu_input_get_ptr();
   menu_animation_t *anim     = menu_animation_get_ptr();
   menu_framebuf_t *frame_buf = menu_display_fb_get_ptr();
   settings_t *settings       = config_get_ptr();

#ifdef HAVE_OVERLAY
   driver_t *driver           = driver_get_ptr();
   if (driver->overlay && (driver->osk_enable || *settings->input.overlay))
      goto reset_ptr_input;
#endif

   if ((settings->menu.mouse.enable && menu_input->mouse.show)
         || *action != MENU_ACTION_NOOP)
      goto reset_ptr_input;

   if (!menu_driver_viewport_info(&vp))
      return;

   menu_input->pointer.pressed = input_driver_state(binds, 0,
         RARCH_DEVICE_POINTER_SCREEN, 0, RETRO_DEVICE_ID_POINTER_PRESSED);

   pointer_x = input_driver_state(binds, 0, RARCH_DEVICE_POINTER_SCREEN,
         0, RETRO_DEVICE_ID_POINTER_X);
   pointer_y = input_driver_state(binds, 0, RARCH_DEVICE_POINTER_SCREEN,
         0, RETRO_DEVICE_ID_POINTER_Y);

   /* scale to framebuffer */
   pointer_x = ((pointer_x + 0x7fff) * (int)frame_buf->width) / 0xffff;
   pointer_y = ((pointer_y + 0x7fff) * (int)frame_buf->height) / 0xffff;

   /* scale from letterbox */
   if (vp.width < vp.full_width)
   {
      offset    = vp.x * (frame_buf->width / (float)vp.width);
      pointer_x = pointer_x * ((float)vp.full_width / vp.width) - offset;
   }
   if (vp.height < vp.full_height)
   {
      offset    = vp.y * (frame_buf->height / (float)vp.height);
      pointer_y = pointer_y * ((float)vp.full_height / vp.height) - offset;
   }

   menu_input->pointer.x = pointer_x;
   menu_input->pointer.y = pointer_y;

   if (menu_input->pointer.pressed || menu_input->pointer.oldpressed)
      anim->is_active = true;

   *action = menu_input_pointer_action();
   return;

reset_ptr_input:
   menu_input->pointer.oldpressed = 0;
   menu_input->pointer.dragging   = false;
   return;
}

static bool menu_input_value_can_step(size_t selected)
{
   menu_list_t *menu_list = menu_list_get_ptr();
   rarch_setting_t *setting;

   if (!menu_list)
      return false;

   setting = menu_setting_find(menu_list->selection_buf->list[selected].label);

   if (setting)
   {
      if (setting->type == ST_BOOL || setting->type == ST_INT ||
          setting->type == ST_UINT || setting->type == ST_FLOAT ||
          setting->type == ST_STRING)
         return true;
   }
   else
   {
      menu_entry_t entry;
      menu_entry_get(&entry, selected, NULL, false);

      if ( (entry.type >= MENU_SETTINGS_CORE_OPTION_START)
           || (entry.type >= MENU_SETTINGS_CHEAT_BEGIN &&
               entry.type <= MENU_SETTINGS_INPUT_JOYKBD_LIST_END)
           || (entry.type >= MENU_SETTINGS_SHADER_PARAMETER_0 &&
               entry.type <= MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_INDEX)
           || (!strcmp(entry.label,
                     menu_hash_to_str(MENU_LABEL_VIDEO_SHADER_NUM_PASSES)))
           || (!strcmp(entry.label,
                     menu_hash_to_str(MENU_LABEL_OPTIONS_SCOPE)))
           || (!strcmp(entry.label,
                     menu_hash_to_str(MENU_LABEL_REMAPPING_SCOPE)))
           || (!strcmp(entry.label,
                     menu_hash_to_str(MENU_LABEL_INPUT_TURBO_ID))) )
         return true;
   }

   return false;
}

static unsigned menu_input_mouse_action(
      retro_input_t *input, retro_input_t *trigger_input)
{
   global_t *global         = global_get_ptr();
   settings_t *settings     = config_get_ptr();
   menu_input_t *menu_input = menu_input_get_ptr();
   menu_list_t *menu_list   = menu_list_get_ptr();
   size_t selected          = menu_navigation_get_current_selection();
   menu_display_t *disp     = menu_display_get_ptr();
   menu_navigation_t *nav   = menu_navigation_get_ptr();
   retro_input_t lmb_input  = UINT64_C(0);

   if (menu_input->mouse.left)
   {
      bool trigger = false;

      if (!menu_input->mouse.oldleft)
      {
         trigger = true;
         menu_input->mouse.oldleft = true;

         if (menu_input->mouse.y < disp->header_height)
            return MENU_ACTION_CANCEL;
         else if (global->menu.block_push)
            return MENU_ACTION_NOOP;
      }

      if (menu_input->mouse.ptr == selected)
      {
         if (menu_input_value_can_step(selected))
         {
            if (menu_input->mouse.x > disp->frame_buf.width / 2)
               BIT64_SET(lmb_input, RETRO_DEVICE_ID_JOYPAD_RIGHT);
            else
               BIT64_SET(lmb_input, RETRO_DEVICE_ID_JOYPAD_LEFT);
         }
         else
            BIT64_SET(lmb_input, settings->menu_ok_btn);
      }
      else if (trigger
            && (menu_input->mouse.ptr <= menu_list_get_size(menu_list)-1))
      {
         menu_navigation_set(nav, menu_input->mouse.ptr, false);
         return MENU_ACTION_NOOP;
      }

      if (trigger)
         *trigger_input |= lmb_input;
      *input |= lmb_input;
   }
   else
      menu_input->mouse.oldleft = false;

   if (menu_input->mouse.right)
   {
      if (!menu_input->mouse.oldright)
      {
         menu_input->mouse.oldright = true;
         BIT64_SET(*trigger_input, settings->menu_cancel_btn);
      }
   }
   else
      menu_input->mouse.oldright = false;

   if (menu_input->mouse.middle)
   {
      BIT64_SET(*input, settings->menu_default_btn);
      if (!menu_input->mouse.oldmiddle)
      {
         menu_input->mouse.oldmiddle = true;
         BIT64_SET(*trigger_input, settings->menu_default_btn);
      }
   }
   else
      menu_input->mouse.oldmiddle = false;

   if (menu_input->mouse.btn4)
   {
      BIT64_SET(*input, RETRO_DEVICE_ID_JOYPAD_L);
      if (!menu_input->mouse.oldbtn4)
      {
         menu_input->mouse.oldbtn4 = true;
         BIT64_SET(*trigger_input, RETRO_DEVICE_ID_JOYPAD_L);
      }
   }
   else
      menu_input->mouse.oldbtn4 = false;

   if (menu_input->mouse.btn5)
   {
      BIT64_SET(*input, RETRO_DEVICE_ID_JOYPAD_R);
      if (!menu_input->mouse.oldbtn5)
      {
         menu_input->mouse.oldbtn5 = true;
         BIT64_SET(*trigger_input, RETRO_DEVICE_ID_JOYPAD_R);
      }
   }
   else
      menu_input->mouse.oldbtn5 = false;

   if (menu_input->mouse.wheeldown)
      menu_navigation_increment(nav);

   if (menu_input->mouse.wheelup)
      menu_navigation_decrement(nav);

   return MENU_ACTION_NOOP;
}

static unsigned menu_input_pointer_tap_action(void)
{
   global_t *global           = global_get_ptr();
   menu_input_t *menu_input   = menu_input_get_ptr();
   menu_list_t *menu_list     = menu_list_get_ptr();
   menu_navigation_t *nav     = menu_navigation_get_ptr();
   size_t selected            = nav->selection_ptr;
   menu_display_t *disp       = menu_display_get_ptr();

   if (menu_input->keyboard.display)
   {
      if (menu_input->pointer.start_y < disp->header_height)
         input_keyboard_event(true, '\n', '\n', 0);
      return MENU_ACTION_NOOP;
   }

   if (menu_input->pointer.start_y < disp->header_height)
      return MENU_ACTION_CANCEL;
   else if (global->menu.block_push)
      return MENU_ACTION_NOOP;
   else if (menu_input->pointer.start_y > disp->frame_buf.height - disp->header_height)
      return MENU_ACTION_START;
   else if (menu_input->pointer.ptr > menu_list_get_size(menu_list)-1)
      return MENU_ACTION_NOOP;

   if (menu_input->pointer.ptr == selected)
   {
      if (menu_input_value_can_step(selected))
      {
         if (menu_input->pointer.x > disp->frame_buf.width / 2)
            return MENU_ACTION_RIGHT;
         else
            return MENU_ACTION_LEFT;
      }
      else
         return MENU_ACTION_OK;
   }
   else
      menu_navigation_set(nav, menu_input->pointer.ptr, false);

   return MENU_ACTION_NOOP;
}

static unsigned menu_input_pointer_action(void)
{
   unsigned ret               = MENU_ACTION_NOOP;
   menu_input_t *menu_input   = menu_input_get_ptr();
   menu_framebuf_t *frame_buf = menu_display_fb_get_ptr();

   if (!menu_input)
      return ret;

   if (menu_input->pointer.pressed)
   {
      if (!menu_input->pointer.oldpressed)
      {
         menu_input->pointer.start_x       = menu_input->pointer.x;
         menu_input->pointer.start_y       = menu_input->pointer.y;
         menu_input->pointer.old_x         = menu_input->pointer.x;
         menu_input->pointer.old_y         = menu_input->pointer.y;
         menu_input->pointer.oldpressed    = true;
      }
      else if (abs(menu_input->pointer.y - menu_input->pointer.start_y) > frame_buf->height / 20
            || abs(menu_input->pointer.x - menu_input->pointer.start_x) > frame_buf->width / 20)
      {
         menu_input->pointer.dragging = true;
         menu_input->pointer.dx       = menu_input->pointer.x - menu_input->pointer.old_x;
         menu_input->pointer.dy       = menu_input->pointer.y - menu_input->pointer.old_y;
         menu_input->pointer.old_x    = menu_input->pointer.x;
         menu_input->pointer.old_y    = menu_input->pointer.y;
      }
   }
   else
   {
      if (menu_input->pointer.oldpressed)
      {
         menu_input->pointer.oldpressed    = false;

         if (!menu_input->pointer.dragging)
            ret = menu_input_pointer_tap_action();

         menu_input->pointer.start_x       = 0;
         menu_input->pointer.start_y       = 0;
         menu_input->pointer.old_x         = 0;
         menu_input->pointer.old_y         = 0;
         menu_input->pointer.dx            = 0;
         menu_input->pointer.dy            = 0;
         menu_input->pointer.dragging      = false;
      }
   }

   return ret;
}

unsigned menu_input_frame(retro_input_t input, retro_input_t trigger_input)
{
   unsigned ret                            = MENU_ACTION_NOOP;
   static bool initial_held                = true;
   static bool restart_timer               = true;
   static retro_input_t repeat_input;
   static const retro_input_t repeat_mask  =
      (1ULL << RETRO_DEVICE_ID_JOYPAD_UP)
      | (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN)
      | (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT)
      | (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT)
      | (1ULL << RETRO_DEVICE_ID_JOYPAD_L)
      | (1ULL << RETRO_DEVICE_ID_JOYPAD_R)
      | (1ULL << RETRO_DEVICE_ID_JOYPAD_L2)
      | (1ULL << RETRO_DEVICE_ID_JOYPAD_R2);
   menu_navigation_t *nav      = menu_navigation_get_ptr();
   menu_handle_t *menu         = menu_driver_get_ptr();
   menu_display_t *disp        = menu_display_get_ptr();
   menu_input_t *menu_input    = menu_input_get_ptr();
   driver_t *driver            = driver_get_ptr();
   settings_t *settings        = config_get_ptr();

   if (!menu || !driver || !nav || !menu_input)
      return 0;

   if (settings->menu.mouse.enable)
      menu_input_mouse(&ret, &input, &trigger_input);

   /* Repeat held inputs from repeat_mask */
   if (input & repeat_mask)
   {
      if (initial_held)
         repeat_input = input & repeat_mask;
      else
         trigger_input &= ~repeat_mask;

      if (restart_timer)
      {
         restart_timer = false;
         menu_input->delay.timer = initial_held ? 15 : 1.5;
         menu_input->delay.count = 0;
      }

      if (menu_input->delay.count >= menu_input->delay.timer)
      {
         restart_timer = true;
         if (input & repeat_input)
            trigger_input = repeat_input;
      }

      menu_input->delay.count += disp->animation->delta_time / IDEAL_DT;
      initial_held = false;
   }
   else
   {
      restart_timer = true;
      initial_held = true;
   }

   if (menu_input->keyboard.display)
   {
      /* send return key to close keyboard input window */
      if (trigger_input & (1ULL << settings->menu_cancel_btn))
         input_keyboard_event(true, '\n', '\n', 0);

      trigger_input = 0;
   }

   if (trigger_input)
   {
      if (trigger_input & (1ULL << RETRO_DEVICE_ID_JOYPAD_UP))
         ret = MENU_ACTION_UP;
      else if (trigger_input & (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN))
         ret = MENU_ACTION_DOWN;
      else if (trigger_input & (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT))
         ret = MENU_ACTION_LEFT;
      else if (trigger_input & (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT))
         ret = MENU_ACTION_RIGHT;
      else if (trigger_input & (1ULL << RETRO_DEVICE_ID_JOYPAD_R))
         ret = MENU_ACTION_R;
      else if (trigger_input & (1ULL << RETRO_DEVICE_ID_JOYPAD_L))
         ret = MENU_ACTION_L;
      else if (trigger_input & (1ULL << RETRO_DEVICE_ID_JOYPAD_R2))
         ret = MENU_ACTION_R2;
      else if (trigger_input & (1ULL << RETRO_DEVICE_ID_JOYPAD_L2))
         ret = MENU_ACTION_L2;
      else if (trigger_input & (1ULL << settings->menu_cancel_btn))
         ret = MENU_ACTION_CANCEL;
      else if (trigger_input & (1ULL << settings->menu_ok_btn))
         ret = MENU_ACTION_OK;
      else if (trigger_input & (1ULL << settings->menu_default_btn))
         ret = MENU_ACTION_START;
      else if (trigger_input & (1ULL << settings->menu_info_btn))
         ret = MENU_ACTION_INFO;
      else if (trigger_input & (1ULL << RARCH_MENU_TOGGLE))
         ret = MENU_ACTION_TOGGLE;
   }

   if (settings->menu.pointer.enable)
      menu_input_pointer(&ret);

   if (trigger_input &&
         menu_ctx_driver_get_ptr()->perform_action &&
         menu_ctx_driver_get_ptr()->perform_action(menu->userdata, ret))
      return MENU_ACTION_NOOP;

   return ret;
}

unsigned* menu_input_desc_mapped_id(unsigned user,
      unsigned index_offset, const char *label)
{
   settings_t *settings = config_get_ptr();

   if (label[0] == 'T')       /* turbo */
      return &settings->input.turbo_remap_id[user];
   else if (label[0] == '-')  /* axis - */
      return &settings->input.custom_axis_ids[user]
            [index_offset - RARCH_FIRST_CUSTOM_BIND][0];
   else if (label[0] == '+')  /* axis + */
      return &settings->input.custom_axis_ids[user]
            [index_offset - RARCH_FIRST_CUSTOM_BIND][1];
   else  /* normal remap */
      return &settings->input.remap_ids[user][index_offset];
}
