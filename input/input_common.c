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

#include "input_common.h"
#include "input_keymaps.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "../general.h"
#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "../gfx/video_viewport.h"

static const char *bind_user_prefix[MAX_USERS] = {
   "input_player1",
   "input_player2",
   "input_player3",
   "input_player4",
   "input_player5",
   "input_player6",
   "input_player7",
   "input_player8",
   "input_player9",
   "input_player10",
   "input_player11",
   "input_player12",
   "input_player13",
   "input_player14",
   "input_player15",
   "input_player16",
};

const struct input_bind_map input_config_bind_map[RARCH_BIND_LIST_END_NULL] = {
      DECLARE_BIND(b,         RETRO_DEVICE_ID_JOYPAD_B, "B button (down)"),
      DECLARE_BIND(y,         RETRO_DEVICE_ID_JOYPAD_Y, "Y button (left)"),
      DECLARE_BIND(select,    RETRO_DEVICE_ID_JOYPAD_SELECT, "Select button"),
      DECLARE_BIND(start,     RETRO_DEVICE_ID_JOYPAD_START, "Start button"),
      DECLARE_BIND(up,        RETRO_DEVICE_ID_JOYPAD_UP, "D-pad Up"),
      DECLARE_BIND(down,      RETRO_DEVICE_ID_JOYPAD_DOWN, "D-pad Down"),
      DECLARE_BIND(left,      RETRO_DEVICE_ID_JOYPAD_LEFT, "D-pad Left"),
      DECLARE_BIND(right,     RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-pad Right"),
      DECLARE_BIND(a,         RETRO_DEVICE_ID_JOYPAD_A, "A button (right)"),
      DECLARE_BIND(x,         RETRO_DEVICE_ID_JOYPAD_X, "X button (top)"),
      DECLARE_BIND(l,         RETRO_DEVICE_ID_JOYPAD_L, "L button (shoulder)"),
      DECLARE_BIND(r,         RETRO_DEVICE_ID_JOYPAD_R, "R button (shoulder)"),
      DECLARE_BIND(l2,        RETRO_DEVICE_ID_JOYPAD_L2, "L2 button (trigger)"),
      DECLARE_BIND(r2,        RETRO_DEVICE_ID_JOYPAD_R2, "R2 button (trigger)"),
      DECLARE_BIND(l3,        RETRO_DEVICE_ID_JOYPAD_L3, "L3 button (thumb)"),
      DECLARE_BIND(r3,        RETRO_DEVICE_ID_JOYPAD_R3, "R3 button (thumb)"),
      DECLARE_BIND(l_x_plus,  RARCH_ANALOG_LEFT_X_PLUS, "Left analog X+ (right)"),
      DECLARE_BIND(l_x_minus, RARCH_ANALOG_LEFT_X_MINUS, "Left analog X- (left)"),
      DECLARE_BIND(l_y_plus,  RARCH_ANALOG_LEFT_Y_PLUS, "Left analog Y+ (down)"),
      DECLARE_BIND(l_y_minus, RARCH_ANALOG_LEFT_Y_MINUS, "Left analog Y- (up)"),
      DECLARE_BIND(r_x_plus,  RARCH_ANALOG_RIGHT_X_PLUS, "Right analog X+ (right)"),
      DECLARE_BIND(r_x_minus, RARCH_ANALOG_RIGHT_X_MINUS, "Right analog X- (left)"),
      DECLARE_BIND(r_y_plus,  RARCH_ANALOG_RIGHT_Y_PLUS, "Right analog Y+ (down)"),
      DECLARE_BIND(r_y_minus, RARCH_ANALOG_RIGHT_Y_MINUS, "Right analog Y- (up)"),

      DECLARE_BIND(gun_trigger,        RARCH_LIGHTGUN_TRIGGER, "Lightgun trigger"),
      DECLARE_BIND(gun_start,          RARCH_LIGHTGUN_START, "Lightgun start"),
      DECLARE_BIND(gun_select,         RARCH_LIGHTGUN_SELECT, "Lightgun select"),
      DECLARE_BIND(gun_aux_a,          RARCH_LIGHTGUN_AUX_A, "Lightgun aux A"),
      DECLARE_BIND(gun_aux_b,          RARCH_LIGHTGUN_AUX_B, "Lightgun aux B"),
      DECLARE_BIND(gun_aux_c,          RARCH_LIGHTGUN_AUX_C, "Lightgun aux C"),
      DECLARE_BIND(gun_offscreen_shot, RARCH_LIGHTGUN_RELOAD, "Lightgun reload"),

      DECLARE_META_BIND(2, toggle_keyboard_focus, RARCH_TOGGLE_KEYBOARD_FOCUS, "Keyboard Focus toggle"),
      DECLARE_META_BIND(2, enable_hotkey,         RARCH_ENABLE_HOTKEY, "Hotkeys enable hold"),
      DECLARE_META_BIND(1, toggle_fast_forward,   RARCH_FAST_FORWARD_KEY, "Fast forward toggle"),
      DECLARE_META_BIND(2, hold_fast_forward,     RARCH_FAST_FORWARD_HOLD_KEY, "Fast forward hold"),
      DECLARE_META_BIND(1, load_state,            RARCH_LOAD_STATE_KEY, "Load state"),
      DECLARE_META_BIND(1, save_state,            RARCH_SAVE_STATE_KEY, "Save state"),
      DECLARE_META_BIND(2, toggle_fullscreen,     RARCH_FULLSCREEN_TOGGLE_KEY, "Fullscreen toggle"),
      DECLARE_META_BIND(2, exit_emulator,         RARCH_QUIT_KEY, "Quit RetroArch Lite"),
      DECLARE_META_BIND(2, state_slot_increase,   RARCH_STATE_SLOT_PLUS, "Savestate slot +"),
      DECLARE_META_BIND(2, state_slot_decrease,   RARCH_STATE_SLOT_MINUS, "Savestate slot -"),
      DECLARE_META_BIND(2, fps_toggle,            RARCH_SHOW_FPS_TOGGLE, "FPS toggle"),
      DECLARE_META_BIND(1, rewind,                RARCH_REWIND, "Rewind"),
      DECLARE_META_BIND(2, pause_toggle,          RARCH_PAUSE_TOGGLE, "Pause toggle"),
      DECLARE_META_BIND(2, frame_advance,         RARCH_FRAMEADVANCE, "Frame advance"),
      DECLARE_META_BIND(2, reset,                 RARCH_RESET, "Reset game"),
      DECLARE_META_BIND(2, shader_next,           RARCH_SHADER_NEXT, "Next shader"),
      DECLARE_META_BIND(2, shader_prev,           RARCH_SHADER_PREV, "Previous shader"),
      DECLARE_META_BIND(2, cheat_index_plus,      RARCH_CHEAT_INDEX_PLUS, "Cheat index +"),
      DECLARE_META_BIND(2, cheat_index_minus,     RARCH_CHEAT_INDEX_MINUS, "Cheat index -"),
      DECLARE_META_BIND(2, cheat_toggle,          RARCH_CHEAT_TOGGLE, "Cheat toggle"),
      DECLARE_META_BIND(2, screenshot,            RARCH_SCREENSHOT, "Take screenshot"),
      DECLARE_META_BIND(2, audio_mute,            RARCH_MUTE, "Audio mute toggle"),
      DECLARE_META_BIND(2, osk_toggle,            RARCH_OSK, "On-screen keyboard toggle"),
      DECLARE_META_BIND(2, netplay_flip_players,  RARCH_NETPLAY_FLIP, "Netplay flip users"),
      DECLARE_META_BIND(2, slowmotion,            RARCH_SLOWMOTION, "Slow motion"),
      DECLARE_META_BIND(2, show_advanced_toggle,  RARCH_ADVANCED_TOGGLE, "Obscure settings toggle"),
      DECLARE_META_BIND(0, overlay_next,          RARCH_OVERLAY_NEXT, "Overlay next"),
      DECLARE_META_BIND(2, disk_eject_toggle,     RARCH_DISK_EJECT_TOGGLE, "Disc eject toggle"),
      DECLARE_META_BIND(2, disk_next,             RARCH_DISK_NEXT, "Disc next"),
      DECLARE_META_BIND(2, disk_prev,             RARCH_DISK_NEXT, "Disc prev"),
      DECLARE_META_BIND(2, grab_mouse_toggle,     RARCH_GRAB_MOUSE_TOGGLE, "Grab mouse toggle"),
      DECLARE_META_BIND(1, menu_toggle,           RARCH_MENU_TOGGLE, "Menu toggle"),
};


/**
 * input_translate_coord_viewport:
 * @mouse_x                        : Pointer X coordinate.
 * @mouse_y                        : Pointer Y coordinate.
 * @res_x                          : Scaled  X coordinate.
 * @res_y                          : Scaled  Y coordinate.
 * @res_screen_x                   : Scaled screen X coordinate.
 * @res_screen_y                   : Scaled screen Y coordinate.
 *
 * Translates pointer [X,Y] coordinates into scaled screen
 * coordinates based on viewport info.
 *
 * Returns: true (1) if successful, false if video driver doesn't support
 * viewport info.
 **/
bool input_translate_coord_viewport(int mouse_x, int mouse_y,
      int16_t *res_x, int16_t *res_y, int16_t *res_screen_x,
      int16_t *res_screen_y)
{
   int scaled_screen_x, scaled_screen_y, scaled_x, scaled_y;
   struct video_viewport vp = {0};

   if (!video_driver_viewport_info(&vp))
      return false;

   if (res_screen_x && res_screen_y)
   {
      scaled_screen_x = (2 * mouse_x * 0x7fff) / (int)vp.full_width - 0x7fff;
      scaled_screen_y = (2 * mouse_y * 0x7fff) / (int)vp.full_height - 0x7fff;
      if (scaled_screen_x < -0x7fff || scaled_screen_x > 0x7fff)
         scaled_screen_x = -0x8000; /* OOB */
      if (scaled_screen_y < -0x7fff || scaled_screen_y > 0x7fff)
         scaled_screen_y = -0x8000; /* OOB */

      *res_screen_x = scaled_screen_x;
      *res_screen_y = scaled_screen_y;
   }
   
   mouse_x -= vp.x;
   mouse_y -= vp.y;

   if (res_x && res_y)
   {
      scaled_x = (2 * mouse_x * 0x7fff) / (int)vp.width - 0x7fff;
      scaled_y = (2 * mouse_y * 0x7fff) / (int)vp.height - 0x7fff;

      if (scaled_x < -0x7fff)     scaled_x = -0x7fff;
      else if (scaled_x > 0x7fff) scaled_x = 0x7fff;

      if (scaled_y < -0x7fff)     scaled_y = -0x7fff;
      else if (scaled_y > 0x7fff) scaled_y = 0x7fff;

      *res_x = scaled_x;
      *res_y = scaled_y;
   }

   return true;
}

void input_config_parse_key(config_file_t *conf,
      const char *prefix, const char *btn,
      struct retro_keybind *bind)
{
   char tmp[64] = {0};
   char key[64] = {0};
   snprintf(key, sizeof(key), "%s_%s", prefix, btn);

   if (config_get_array(conf, key, tmp, sizeof(tmp)))
      bind->key = input_translate_str_to_rk(tmp);
   }

const char *input_config_get_prefix(unsigned user, bool meta)
{
   if (user == 0)
      return meta ? "input" : bind_user_prefix[user];
   else if (user != 0 && !meta)
      return bind_user_prefix[user];
   /* Don't bother with meta bind for anyone else than first user. */
   return NULL;
}

static enum retro_key find_rk_bind(const char *str)
{
   size_t i;

   for (i = 0; input_config_key_map[i].str; i++)
   {
      if (strcasecmp(input_config_key_map[i].str, str) == 0)
         return input_config_key_map[i].key;
   }

   RARCH_WARN("Key name %s not found.\n", str);
   return RETROK_UNKNOWN;
}

/**
 * input_translate_str_to_rk:
 * @str                            : String to translate to key ID.
 *
 * Translates tring representation to key identifier.
 *
 * Returns: key identifier.
 **/
enum retro_key input_translate_str_to_rk(const char *str)
{
   if (strlen(str) == 1 && isalpha(*str))
      return (enum retro_key)(RETROK_a + (tolower(*str) - (int)'a'));
   return find_rk_bind(str);
}

/**
 * input_translate_str_to_bind_id:
 * @str                            : String to translate to bind ID.
 *
 * Translate string representation to bind ID.
 *
 * Returns: Bind ID value on success, otherwise RARCH_BIND_LIST_END on not found.
 **/
unsigned input_translate_str_to_bind_id(const char *str)
{
   unsigned i;

   for (i = 0; input_config_bind_map[i].valid; i++)
      if (!strcmp(str, input_config_bind_map[i].base))
         return i;
   return RARCH_BIND_LIST_END;
}


static void parse_hat(struct retro_keybind *bind, const char *str)
{
   uint16_t     hat;
   uint16_t hat_dir = 0;
   char        *dir = NULL;

   if (!bind || !str)
      return;

   if (!isdigit(*str))
      return;

   hat = strtoul(str, &dir, 0);

   if (!dir)
   {
      RARCH_WARN("Found invalid hat in config!\n");
      return;
   }

   if (strcasecmp(dir, "up") == 0)
      hat_dir = HAT_UP_MASK;
   else if (strcasecmp(dir, "down") == 0)
      hat_dir = HAT_DOWN_MASK;
   else if (strcasecmp(dir, "left") == 0)
      hat_dir = HAT_LEFT_MASK;
   else if (strcasecmp(dir, "right") == 0)
      hat_dir = HAT_RIGHT_MASK;

   if (hat_dir)
      bind->joykey = HAT_MAP(hat, hat_dir);
}

void input_config_parse_joy_button(config_file_t *conf, const char *prefix,
      const char *btn, struct retro_keybind *bind)
{
   char tmp[64]       = {0};
   char key[64]       = {0};
   char key_label[64] = {0};
   char *tmp_a        = NULL;

   snprintf(key, sizeof(key), "%s_%s_btn", prefix, btn);
   snprintf(key_label, sizeof(key_label), "%s_%s_btn_label", prefix, btn);

   if (config_get_array(conf, key, tmp, sizeof(tmp)))
   {
      btn = tmp;
      if (!strcmp(btn, EXPLICIT_NULL))
         bind->joykey = NO_BTN;
      else
      {
         if (*btn == 'h')
            parse_hat(bind, btn + 1);
         else
            bind->joykey = strtoull(tmp, NULL, 0);
      }
   }

   if (config_get_string(conf, key_label, &tmp_a))
      strlcpy(bind->joykey_label, tmp_a, sizeof(bind->joykey_label));
   if (tmp_a) free(tmp_a);
}

void input_config_parse_joy_axis(config_file_t *conf, const char *prefix,
      const char *axis, struct retro_keybind *bind)
{
   char       tmp[64] = {0};
   char       key[64] = {0};
   char key_label[64] = {0};
   char        *tmp_a = NULL;

   snprintf(key, sizeof(key), "%s_%s_axis", prefix, axis);
   snprintf(key_label, sizeof(key_label), "%s_%s_axis_label", prefix, axis);

   if (config_get_array(conf, key, tmp, sizeof(tmp)))
   {
      if (!strcmp(tmp, EXPLICIT_NULL))
         bind->joyaxis = AXIS_NONE;
      else if (strlen(tmp) >= 2 && (*tmp == '+' || *tmp == '-'))
      {
         int i_axis = strtol(tmp + 1, NULL, 0);
         if (*tmp == '+')
            bind->joyaxis = AXIS_POS(i_axis);
         else
            bind->joyaxis = AXIS_NEG(i_axis);
      }
   }

   if (config_get_string(conf, key_label, &tmp_a))
      strlcpy(bind->joyaxis_label, tmp_a, sizeof(bind->joyaxis_label));
   if (tmp_a) free(tmp_a);
}

#if !defined(IS_JOYCONFIG)
static void input_get_bind_string_joykey(char *buf,
      const struct retro_keybind *bind, size_t size)
{
   settings_t *settings = config_get_ptr();

   if (GET_HAT_DIR(bind->joykey))
   {
      const char *dir = NULL;

      switch (GET_HAT_DIR(bind->joykey))
      {
         case HAT_UP_MASK:
            dir = "up";
            break;
         case HAT_DOWN_MASK:
            dir = "down";
            break;
         case HAT_LEFT_MASK:
            dir = "left";
            break;
         case HAT_RIGHT_MASK:
            dir = "right";
            break;
         default:
            dir = "?";
            break;
      }

      if (bind->joykey_label[0] != '\0'
            && settings->input.autoconfig_descriptor_label_show)
         snprintf(buf, size, "(Hat: %s) ", bind->joykey_label);
      else
         snprintf(buf, size, "(Hat #%u %s) ",
               (unsigned)GET_HAT(bind->joykey), dir);
   }
   else
   {
      if (bind->joykey_label[0] != '\0'
            && settings->input.autoconfig_descriptor_label_show)
         snprintf(buf, size, "(Btn: %s) ", bind->joykey_label);
      else
         snprintf(buf, size, "(Btn: %u) ", (unsigned)bind->joykey);
   }
}

static void input_get_bind_string_joyaxis(char *buf,
      const struct retro_keybind *bind, size_t size)
{
   unsigned axis        = 0;
   char dir             = '\0';
   settings_t *settings = config_get_ptr();

   if (AXIS_NEG_GET(bind->joyaxis) != AXIS_DIR_NONE)
   {
      dir = '-';
      axis = AXIS_NEG_GET(bind->joyaxis);
   }
   else if (AXIS_POS_GET(bind->joyaxis) != AXIS_DIR_NONE)
   {
      dir = '+';
      axis = AXIS_POS_GET(bind->joyaxis);
   }
   if (bind->joyaxis_label[0] != '\0'
         && settings->input.autoconfig_descriptor_label_show)
      snprintf(buf, size, "(Axis: %s) ", bind->joyaxis_label);
   else
      snprintf(buf, size, "(Axis: %c%u) ", dir, axis);
}

void input_get_bind_string(char *buf, const struct retro_keybind *bind,
      const struct retro_keybind *auto_bind, size_t size)
{
   char key[64]    = {0};
   char keybuf[64] = {0};

   (void)key;
   (void)keybuf;

   *buf = '\0';
   if (bind->joykey != NO_BTN)
      input_get_bind_string_joykey(buf, bind, size);
   else if (bind->joyaxis != AXIS_NONE)
      input_get_bind_string_joyaxis(buf, bind, size);
   else if (auto_bind && auto_bind->joykey != NO_BTN)
      input_get_bind_string_joykey(buf, auto_bind, size);
   else if (auto_bind && auto_bind->joyaxis != AXIS_NONE)
      input_get_bind_string_joyaxis(buf, auto_bind, size);

#ifndef RARCH_CONSOLE
   input_keymaps_translate_rk_to_str(bind->key, key, sizeof(key));
   if (!strcmp(key, EXPLICIT_NULL))
      *key = '\0';

   snprintf(keybuf, sizeof(keybuf), "(Key: %s)", key);
   strlcat(buf, keybuf, size);
#endif
}
#endif

void input_set_keyboard_focus_auto(void)
{
   int p;
   settings_t *settings = config_get_ptr();
   global_t   *global   = global_get_ptr();
   bool want_kb_focus   = false;
   const struct retro_keybind *kb_focus_bind
         = &settings->input.binds[0][RARCH_TOGGLE_KEYBOARD_FOCUS];

   if (!settings->input.auto_keyboard_focus)
      return;

   /* To be safe, disable if hotkey isn't set
    * or if no controller info provided */
   if (kb_focus_bind->key == RETROK_UNKNOWN
         || global->system.num_ports == 0)
      goto finish;

   for (p = 0; p < settings->input.max_users; p++)
   {
      if ((RETRO_DEVICE_MASK & settings->input.libretro_device[p])
             == RETRO_DEVICE_KEYBOARD)
      {
         want_kb_focus = true;
         break;
      }
   }

finish:
   if (want_kb_focus != global->keyboard_focus)
      event_command(EVENT_CMD_KEYBOARD_FOCUS_TOGGLE);
}
