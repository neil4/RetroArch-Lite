/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2015 - Jean-André Santoni
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

#ifndef _MENU_ANIMATION_H
#define _MENU_ANIMATION_H

#include <stdint.h>
#include <stdlib.h>
#include <boolean.h>
#include <libretro.h>

#ifndef IDEAL_DT
#define IDEAL_DT (1.0 / 60.0 * 1000000.0)
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef float (*easingFunc)(float, float, float, float);
typedef void  (*tween_cb) (void);

struct tween
{
   int    alive;
   float  duration;
   float  running_since;
   float  initial_value;
   float  target_value;
   float* subject;
   easingFunc easing;
   tween_cb cb;
};

typedef struct menu_animation
{
   struct tween *list;

   size_t capacity;
   size_t size;
   bool is_active;

   /* Delta timing */
   float delta_time;
   retro_time_t cur_time;
   retro_time_t old_time;

   struct
   {
      bool is_updated;
   } label;
} menu_animation_t;

enum menu_animation_easing_type
{
   /* Linear */
   EASING_LINEAR    = 0,
   /* Quad */
   EASING_IN_QUAD,
   EASING_OUT_QUAD,
   EASING_IN_OUT_QUAD,
   EASING_OUT_IN_QUAD,
   /* Cubic */
   EASING_IN_CUBIC,
   EASING_OUT_CUBIC,
   EASING_IN_OUT_CUBIC,
   EASING_OUT_IN_CUBIC,
   /* Quart */
   EASING_IN_QUART,
   EASING_OUT_QUART,
   EASING_IN_OUT_QUART,
   EASING_OUT_IN_QUART,
   /* Quint */
   EASING_IN_QUINT,
   EASING_OUT_QUINT,
   EASING_IN_OUT_QUINT,
   EASING_OUT_IN_QUINT,
   /* Sine */
   EASING_IN_SINE,
   EASING_OUT_SINE,
   EASING_IN_OUT_SINE,
   EASING_OUT_IN_SINE,
   /* Expo */
   EASING_IN_EXPO,
   EASING_OUT_EXPO,
   EASING_IN_OUT_EXPO,
   EASING_OUT_IN_EXPO,
   /* Circ */
   EASING_IN_CIRC,
   EASING_OUT_CIRC,
   EASING_IN_OUT_CIRC,
   EASING_OUT_IN_CIRC,
   /* Bounce */
   EASING_IN_BOUNCE,
   EASING_OUT_BOUNCE,
   EASING_IN_OUT_BOUNCE,
   EASING_OUT_IN_BOUNCE,
};

void menu_animation_free(
      menu_animation_t *animation);

void menu_animation_kill_by_subject(
      menu_animation_t *animation,
      size_t count,
      const void *subjects);

bool menu_animation_push(
      menu_animation_t *animation,
      float duration,
      float target_value, float* subject,
      enum menu_animation_easing_type easing_enum, tween_cb cb);

bool menu_animation_update(
      menu_animation_t *animation,
      float dt);

/**
 * menu_animation_ticker_line:
 * @buf                      : buffer to write new message line to.
 * @len                      : length of buffer @input.
 * @idx                      : Index. Will be used for ticker logic.
 * @str                      : Input string.
 * @selected                 : Is the item currently selected in the menu?
 * @return Font stride offset for smooth ticker text (-1.0, 1.0)
 *
 * Take the contents of @str and apply a ticker effect to it,
 * and write the results in @buf.
 **/
float menu_animation_ticker_line(char *buf, size_t len, uint64_t tick,
      const char *str, bool selected);

menu_animation_t *menu_animation_get_ptr(void);

void menu_animation_update_time(menu_animation_t *anim);

void menu_update_ticker_speed();

#ifdef __cplusplus
}
#endif

#endif
