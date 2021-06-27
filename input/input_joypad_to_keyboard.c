/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2021 - Neil Fore
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

#include "input_joypad_to_keyboard.h"

extern const struct retro_keybind *libretro_input_binds[];

/* Each bind is a linked list within @joykbd_bind_list */
static struct joykbd_bind *joykbd_binds[NUM_JOYKBD_BTNS];

static uint32_t joykbd_state[RETROK_LAST / 32 + 1];

bool joykbd_enabled;

struct joykbd_bind joykbd_bind_list[JOYKBD_LIST_LEN] = {
   {.rk = RETROK_UP,           .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_DOWN,         .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_RIGHT,        .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_LEFT,         .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_RETURN,       .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_ESCAPE,       .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_SPACE,        .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_BACKSPACE,    .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_TAB,          .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_LSHIFT,       .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_RSHIFT,       .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_LCTRL,        .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_RCTRL,        .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_LALT,         .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_RALT,         .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_a,            .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_b,            .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_c,            .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_d,            .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_e,            .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_f,            .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_g,            .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_h,            .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_i,            .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_j,            .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_k,            .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_l,            .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_m,            .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_n,            .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_o,            .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_p,            .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_q,            .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_r,            .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_s,            .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_t,            .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_u,            .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_v,            .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_w,            .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_x,            .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_y,            .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_z,            .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_0,            .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_1,            .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_2,            .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_3,            .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_4,            .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_5,            .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_6,            .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_7,            .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_8,            .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_9,            .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_BACKQUOTE,    .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_MINUS,        .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_EQUALS,       .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_LEFTBRACKET,  .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_RIGHTBRACKET, .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_BACKSLASH,    .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_SEMICOLON,    .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_QUOTE,        .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_COMMA,        .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_PERIOD,       .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_SLASH,        .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_INSERT,       .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_DELETE,       .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_HOME,         .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_END,          .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_PAGEUP,       .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_PAGEDOWN,     .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_NUMLOCK,      .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_CAPSLOCK,     .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_SCROLLOCK,    .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_PAUSE,        .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_KP0,          .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_KP1,          .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_KP2,          .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_KP3,          .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_KP4,          .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_KP5,          .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_KP6,          .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_KP7,          .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_KP8,          .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_KP9,          .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_KP_PERIOD,    .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_KP_DIVIDE,    .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_KP_MULTIPLY,  .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_KP_MINUS,     .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_KP_PLUS,      .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_KP_ENTER,     .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_KP_EQUALS,    .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_F1,           .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_F2,           .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_F3,           .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_F4,           .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_F5,           .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_F6,           .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_F7,           .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_F8,           .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_F9,           .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_F10,          .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_F11,          .btn = NO_BTN, .next = NULL},
   {.rk = RETROK_F12,          .btn = NO_BTN, .next = NULL}
};

void input_joykbd_update_enabled()
{
   settings_t *settings = config_get_ptr();
   int p;

   joykbd_enabled = false;

   for (p = 0; p < settings->input.max_users; p++)
   {
      if ((RETRO_DEVICE_MASK & settings->input.libretro_device[p])
             == RETRO_DEVICE_KEYBOARD)
      {
         joykbd_enabled = true;
         return;
      }
   }
}

void input_joykbd_init_binds()
{
   unsigned i;
   for (i = 0; i < JOYKBD_LIST_LEN; i++)
   {
      joykbd_bind_list[i].btn  = NO_BTN;
      joykbd_bind_list[i].next = NULL;
   }

   memset(joykbd_binds, 0, sizeof(joykbd_binds));
   BITARRAY32_CLEAR_ALL(joykbd_state);
}

void input_joykbd_add_bind(enum retro_key rk, uint8_t btn)
{
   unsigned i;

   if (btn >= NUM_JOYKBD_BTNS)
      return;

   /* find key */
   for (i = 0; i < JOYKBD_LIST_LEN && joykbd_bind_list[i].rk != rk; i++);
   if (i == JOYKBD_LIST_LEN)
      return;

   /* unbind if bound */
   if (joykbd_bind_list[i].btn < NUM_JOYKBD_BTNS)
      input_joykbd_remove_bind(rk, joykbd_bind_list[i].btn);

   /* update head or link to tail*/
   if (!joykbd_binds[btn])
      joykbd_binds[btn] = &joykbd_bind_list[i];
   else
   {
      struct joykbd_bind *bind = joykbd_binds[btn];

      while (bind->next)
         bind = bind->next;

      bind->next = &joykbd_bind_list[i];
   }

   /* set lookup button */
   joykbd_bind_list[i].btn = btn;
}

void input_joykbd_remove_bind(enum retro_key rk, uint8_t btn)
{
   struct joykbd_bind *bind = btn < NUM_JOYKBD_BTNS ?
                              joykbd_binds[btn] : NULL;
   struct joykbd_bind *prev = NULL;

   /* find key */
   while (bind && bind->rk != rk)
   {
      prev = bind;
      bind = bind->next;
   }

   if (!bind)
      return;

   /* remove lookup */
   bind->btn = NO_BTN;

   /* unlink */
   if (!prev)
      joykbd_binds[btn] = bind->next;
   else
      prev->next        = bind->next;
   bind->next = NULL;
}

static INLINE void input_joykbd_update_state(uint32_t btn_state)
{
   global_t *global = global_get_ptr();
   struct joykbd_bind *bind;
   uint32_t diff, i;
   bool down;

   static uint32_t old_btn_state;

   if (btn_state == old_btn_state)
      return;

   diff = btn_state ^ old_btn_state;
   old_btn_state = btn_state;

   for (i = 0; i < NUM_JOYKBD_BTNS; i++)
   {
      if ((diff & (1 << i)) == 0)
         continue;

      bind = joykbd_binds[i];
      down = (btn_state & (1 << i)) != 0;

      while (bind)
      {
         if (global->frontend_key_event)
            global->frontend_key_event(down, bind->rk, 0, 0);

         if (down)
            BITARRAY32_SET(joykbd_state, bind->rk);
         else
            BITARRAY32_CLEAR(joykbd_state, bind->rk);

         bind = bind->next;
      }
   }
}

/**
 * input_joykbd_poll:
 *
 * Sends keyboard events and updates @joykbd_state based on @joykbd_binds and
 * port 0 button state.
 **/
void input_joykbd_poll()
{
   uint32_t btn_state = driver_get_ptr()->overlay_state.buttons;
   uint8_t i;

   if (menu_driver_alive() || !joykbd_enabled)
      return;

   input_driver_keyboard_mapping_set_block(true);
   for (i = 0; i < NUM_JOYKBD_BTNS; i++)
   {
      if (joykbd_binds[i] == NULL)
         continue;

      if (input_driver_state(libretro_input_binds,
             0, RETRO_DEVICE_JOYPAD, 0, i))
         btn_state |= (1 << i);
   }
   input_driver_keyboard_mapping_set_block(false);

   input_joykbd_update_state(btn_state);
}

int16_t input_joykbd_state(enum retro_key rk)
{
   return BITARRAY32_GET(joykbd_state, rk) != 0;
}