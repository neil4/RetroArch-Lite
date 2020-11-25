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

#include "input_joypad.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "../general.h"
#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#define DPAD_MASK \
( (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_UP) \
| (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_DOWN) \
| (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_LEFT) \
| (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_RIGHT) )

/* Analog to Dpad
 * defaults: 8-way symmetry, 33% deadzone */
static float analog_dpad_high_slope  = 2.4142f;
static float analog_dpad_low_slope   = 0.4142f;
static float analog_dpad_deadzone_sq = 0.1111f;
bool analog_dpad_state_utd;  /* todo: a better way to flag updates */

void input_joypad_update_analog_dpad_params()
{
   settings_t *settings  = config_get_ptr();
   const float diag_sens = settings->input.analog_diagonal_sensitivity;
   float deadzone        = settings->input.analog_dpad_deadzone / 100.0f;
   float f               = 2.0f * diag_sens / (100.0f + diag_sens);

   float high_angle  /* 67.5 deg max */
      = (f * (0.375*M_PI) + (1.0f - f) * (0.25*M_PI));
   float low_angle   /* 22.5 deg min */
      = (f * (0.125*M_PI) + (1.0f - f) * (0.25*M_PI));

   analog_dpad_high_slope = tan(high_angle);
   analog_dpad_low_slope  = tan(low_angle);

   analog_dpad_deadzone_sq = deadzone * deadzone;
}

static inline uint64_t input_joypad_analog_eightway_state(int16_t x_axis,
                                                          int16_t y_axis)
{
   float x, y;
   float abs_slope;

   x = (float)x_axis / 0x8000;
   y = (float)-y_axis / 0x8000;

   if (x*x + y*y < analog_dpad_deadzone_sq)
      return 0;

   if (x == 0.0f)
     x = 0.0001f;
   abs_slope = fabs(y/x);

   if (x > 0.0f)
   {
      if (y > 0.0f)
      {  /* Q1 */
         if (abs_slope > analog_dpad_high_slope)
            return (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_UP);
         else if (abs_slope < analog_dpad_low_slope)
            return (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_RIGHT);
         else
            return ((UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_UP) |
                    (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_RIGHT));
      }
      else
      {  /* Q4 */
         if (abs_slope > analog_dpad_high_slope)
            return (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_DOWN);
         else if (abs_slope < analog_dpad_low_slope)
            return (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_RIGHT);
         else
            return ((UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_DOWN) |
                    (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_RIGHT));
      }
   }
   else
   {
      if (y > 0.0f)
      {  /* Q2 */
         if (abs_slope > analog_dpad_high_slope)
            return (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_UP);
         else if (abs_slope < analog_dpad_low_slope)
            return (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_LEFT);
         else
            return ((UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_UP) |
                    (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_LEFT));
      }
      else
      {  /* Q3 */
         if (abs_slope > analog_dpad_high_slope)
            return (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_DOWN);
         else if (abs_slope < analog_dpad_low_slope)
            return (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_LEFT);
         else
            return ((UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_DOWN) |
                    (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_LEFT));
      }
   }

   return UINT64_C(0);
}

/**
 * input_joypad_name:  
 * @drv                     : Input device driver handle.
 * @port                    : Joystick number.
 *
 * Gets name of the joystick (@port).
 *
 * Returns: name of joystick #port.
 **/
const char *input_joypad_name(const input_device_driver_t *drv,
      unsigned port)
{
   if (!drv)
      return NULL;
   return drv->name(port);
}

/**
 * input_joypad_set_rumble:
 * @drv                     : Input device driver handle.
 * @port                    : User number.
 * @effect                  : Rumble effect to set.
 * @strength                : Strength of rumble effect.
 *
 * Sets rumble effect @effect with strength @strength.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool input_joypad_set_rumble(const input_device_driver_t *drv,
      unsigned port, enum retro_rumble_effect effect, uint16_t strength)
{
   settings_t *settings = config_get_ptr();
   unsigned joy_idx     = settings->input.joypad_map[port];
   
   if (!settings->input.rumble_enable || !drv || !drv->set_rumble)
      return false;

   if (joy_idx >= MAX_USERS)
      return false;

   return drv->set_rumble(joy_idx, effect, strength);
}

/**
 * input_joypad_is_pressed:
 * @drv                     : Input device driver handle.
 * @port                    : User number.
 * @binds                   : Binds of user.
 * @key                     : Identifier of key.
 *
 * Checks if key (@key) was being pressed by user
 * with number @port with provided keybinds (@binds).
 *
 * Returns: true (1) if key was pressed, otherwise
 * false (0).
 **/
bool input_joypad_pressed(
      const input_device_driver_t *drv,
      unsigned port,
      const struct retro_keybind *binds,
      unsigned key)
{
   float scaled_axis;
   int16_t  axis;
   uint32_t joyaxis;
   uint64_t joykey;
   unsigned analog_idx;
   int16_t analog_x, analog_y;
   static int64_t analog_dpad_state;
   const struct retro_keybind *auto_binds = NULL;
   settings_t *settings = config_get_ptr();
   unsigned joy_idx = settings->input.joypad_map[port];

   if (joy_idx >= MAX_USERS)
      return false;
   if (!drv || !binds[key].valid)
      return false;

   /* Auto-binds are per joypad, not per user. */
   auto_binds = settings->input.autoconf_binds[joy_idx];

   joykey = binds[key].joykey;
   if (joykey == NO_BTN)
      joykey = auto_binds[key].joykey;

   if (drv->button(joy_idx, (uint16_t)joykey))
      return true;

   if (((UINT64_C(1) << key) & DPAD_MASK) && settings->input.analog_dpad_mode)
   {
      if (!analog_dpad_state_utd)
      {
         analog_idx = settings->input.analog_dpad_mode - 1;
         analog_x = input_joypad_analog(drv, port, analog_idx,
                          RETRO_DEVICE_ID_ANALOG_X, binds);
         analog_y = input_joypad_analog(drv, port, analog_idx,
                          RETRO_DEVICE_ID_ANALOG_Y, binds);

         analog_dpad_state = input_joypad_analog_eightway_state(analog_x,
                                                                analog_y);
         analog_dpad_state_utd = true;
      }

      return ((UINT64_C(1) << key) & analog_dpad_state);
   }

   joyaxis = binds[key].joyaxis;
   if (joyaxis == AXIS_NONE)
      joyaxis = auto_binds[key].joyaxis;

   axis        = drv->axis(joy_idx, joyaxis);
   scaled_axis = (float)abs(axis) / 0x8000;
   return scaled_axis > settings->input.axis_threshold;
}

/**
 * input_joypad_analog:
 * @drv                     : Input device driver handle.
 * @port                    : User number.
 * @idx                     : Analog key index.
 *                            E.g.: 
 *                            - RETRO_DEVICE_INDEX_ANALOG_LEFT
 *                            - RETRO_DEVICE_INDEX_ANALOG_RIGHT
 * @ident                   : Analog key identifier.
 *                            E.g.:
 *                            - RETRO_DEVICE_ID_ANALOG_X
 *                            - RETRO_DEVICE_ID_ANALOG_Y
 * @binds                   : Binds of user.
 *
 * Gets analog value of analog key identifiers @idx and @ident
 * from user with number @port with provided keybinds (@binds).
 *
 * Returns: analog value on success, otherwise 0.
 **/
int16_t input_joypad_analog(const input_device_driver_t *drv,
      unsigned port, unsigned idx, unsigned ident,
      const struct retro_keybind *binds)
{
   uint32_t axis_minus, axis_plus;
   uint64_t key_minus, key_plus;
   int16_t  pressed_minus, pressed_plus, res;
   unsigned ident_minus = 0, ident_plus = 0;
   int16_t digital_left = 0, digital_right = 0;
   const struct retro_keybind *auto_binds = NULL;
   const struct retro_keybind *bind_minus = NULL;
   const struct retro_keybind *bind_plus  = NULL;
   settings_t *settings = config_get_ptr();
   unsigned joy_idx = settings->input.joypad_map[port];

   if (!drv)
      return 0;

   if (joy_idx >= MAX_USERS)
      return 0;

   /* Auto-binds are per joypad, not per user. */
   auto_binds = settings->input.autoconf_binds[joy_idx];

   input_conv_analog_id_to_bind_id(idx, ident, &ident_minus, &ident_plus);

   bind_minus = &binds[ident_minus];
   bind_plus  = &binds[ident_plus];
   if (!bind_minus->valid || !bind_plus->valid)
      return 0;

   axis_minus = bind_minus->joyaxis;
   axis_plus  = bind_plus->joyaxis;
   if (axis_minus == AXIS_NONE)
      axis_minus = auto_binds[ident_minus].joyaxis;
   if (axis_plus == AXIS_NONE)
      axis_plus = auto_binds[ident_plus].joyaxis;

   pressed_minus = abs(drv->axis(joy_idx, axis_minus));
   pressed_plus  = abs(drv->axis(joy_idx, axis_plus));
   res           = pressed_plus - pressed_minus;

   if (res != 0)
      return res;

   key_minus = bind_minus->joykey;
   key_plus  = bind_plus->joykey;
   if (key_minus == NO_BTN)
      key_minus = auto_binds[ident_minus].joykey;
   if (key_plus == NO_BTN)
      key_plus = auto_binds[ident_plus].joykey;

   if (drv->button(joy_idx, (uint16_t)key_minus))
      digital_left  = -0x7fff;
   if (drv->button(joy_idx, (uint16_t)key_plus))
      digital_right = 0x7fff;
   return digital_right + digital_left;
}

/**
 * input_joypad_axis_raw:  
 * @drv                     : Input device driver handle.
 * @port                    : Joystick number.
 * @axis                    : Identifier of axis.
 *
 * Checks if axis (@axis) was being pressed by user   
 * with joystick number @port.
 *
 * Returns: true (1) if axis was pressed, otherwise
 * false (0).
 **/
int16_t input_joypad_axis_raw(const input_device_driver_t *drv,
      unsigned port, unsigned axis)
{
   if (!drv)
      return 0;
   return drv->axis(port, AXIS_POS(axis)) +
      drv->axis(port, AXIS_NEG(axis));
}

/**
 * input_joypad_button_raw:
 * @drv                     : Input device driver handle.
 * @port                    : Joystick number.
 * @button                  : Identifier of key.
 *
 * Checks if key (@button) was being pressed by user
 * with joystick number @port.
 *
 * Returns: true (1) if key was pressed, otherwise
 * false (0).
 **/
bool input_joypad_button_raw(const input_device_driver_t *drv,
      unsigned port, unsigned button)
{
   if (!drv)
      return false;
   return drv->button(port, button);
}

bool input_joypad_hat_raw(const input_device_driver_t *drv,
      unsigned port, unsigned hat_dir, unsigned hat)
{
   if (!drv)
      return false;
   return drv->button(port, HAT_MAP(hat, hat_dir));
}
